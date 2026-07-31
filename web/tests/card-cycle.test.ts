import { describe, expect, test } from "bun:test";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import {
  CardCycleCoordinator,
  JsonFileCardCycleStore,
  MemoryCardCycleStore,
  cardRequestCooldownMsFromEnvironment,
} from "../server/card-cycle";
import { CARD_BATCH_SIZE, type EvolutionCard } from "../src/card-cycle-protocol";

function cards(prefix: string): EvolutionCard[] {
  return Array.from({ length: CARD_BATCH_SIZE }, (_, index) => ({
    id: `${prefix}-${index + 1}`,
    title: `Carte ${index + 1}`,
    need: "Un besoin observé",
    obstacle: "Un obstacle concret",
    proposed_change: "Une évolution proposée",
    mechanism: "Mécanisme déterministe",
    acceptance_tests: ["Un test d'acceptation exécutable"],
  }));
}

describe("cycle de cartes", () => {
  test("génère exactement trois cartes et sérialise les appels existants", async () => {
    let activeCalls = 0;
    let maximumActiveCalls = 0;
    const cycle = new CardCycleCoordinator({ cooldownMs: 0 });

    const result = await cycle.requestNextBatch(async ({ call }) => {
      await Promise.all(
        ["bilan Ada", "demande Ada", "bilan Borin", "demande Borin", "bilan Cyra", "demande Cyra"].map(
          (operation) => call(operation, async () => {
            activeCalls += 1;
            maximumActiveCalls = Math.max(maximumActiveCalls, activeCalls);
            await new Promise((resolve) => setTimeout(resolve, 2));
            activeCalls -= 1;
          }),
        ),
      );
      return cards("lot-1");
    });

    expect(result.kind).toBe("generated");
    expect(result.kind === "generated" && result.batch.cards).toHaveLength(3);
    expect(maximumActiveCalls).toBe(1);
    expect(await cycle.snapshot()).toMatchObject({ call_count: 6, total_api_calls: 6 });
  });

  test("ne relance aucune demande avant la validation des trois cartes puis le délai configuré", async () => {
    let now = 1_000;
    let generations = 0;
    const cycle = new CardCycleCoordinator({ now: () => now, cooldownMs: 250 });
    const generate = async () => {
      generations += 1;
      return cards(`lot-${generations}`);
    };

    expect((await cycle.requestNextBatch(generate)).kind).toBe("generated");
    expect((await cycle.requestNextBatch(generate))).toMatchObject({
      kind: "blocked",
      reason: "pending_cards",
    });
    expect(generations).toBe(1);

    for (const card of cards("lot-1")) {
      expect(await cycle.recordCardDecision(card.id, "reject")).toBe(true);
    }
    expect((await cycle.requestNextBatch(generate))).toMatchObject({
      kind: "blocked",
      reason: "cooldown",
      retry_at_ms: 1_250,
    });

    now = 1_250;
    expect((await cycle.requestNextBatch(generate)).kind).toBe("generated");
    expect(generations).toBe(2);
  });

  test("refuse une décision hors protocole sans modifier une carte pending", async () => {
    const cycle = new CardCycleCoordinator({ cooldownMs: 0 });
    await cycle.requestNextBatch(async () => cards("lot-sûr"));

    expect(await cycle.recordCardDecision("lot-sûr-1", "ignore" as never)).toBe(false);
    expect((await cycle.snapshot()).current_batch?.cards[0].status).toBe("pending");
  });

  test("compte les appels de façon persistante, alerte au dixième et ne réessaie pas après un échec", async () => {
    const store = new MemoryCardCycleStore();
    const cycle = new CardCycleCoordinator({ cooldownMs: 0, store });
    let attempts = 0;
    const result = await cycle.requestNextBatch(async ({ call }) => {
      for (let index = 0; index < 10; index += 1) {
        await call(`appel-${index + 1}`, async () => {
          attempts += 1;
        });
      }
      return cards("lot-1");
    });

    expect(result.kind).toBe("generated");
    expect(attempts).toBe(10);
    expect(await cycle.snapshot()).toMatchObject({
      call_count: 10,
      call_alert: { call_count: 10, acknowledged: false },
    });
    expect(await cycle.requestNextBatch(async () => cards("lot-2"))).toMatchObject({
      kind: "blocked",
      reason: "call_alert",
    });
    expect(await cycle.acknowledgeCallAlert()).toBe(true);

    const restored = new CardCycleCoordinator({ cooldownMs: 0, store });
    expect((await restored.snapshot()).call_count).toBe(10);

    let failedCalls = 0;
    const failing = new CardCycleCoordinator({ cooldownMs: 0 });
    const failure = await failing.requestNextBatch(async ({ call }) => {
      await call("appel qui échoue", async () => {
        failedCalls += 1;
        throw new Error("réseau indisponible");
      });
      return cards("inaccessible");
    });
    expect(failure).toMatchObject({ kind: "failed", error: "réseau indisponible" });
    expect(failedCalls).toBe(1);
    expect((await failing.snapshot()).call_count).toBe(1);
  });

  test("refuse un contrat qui ne contient pas exactement trois cartes distinctes", async () => {
    const cycle = new CardCycleCoordinator({ cooldownMs: 0 });
    const invalid = await cycle.requestNextBatch(async () => cards("lot-1").slice(0, 2));
    expect(invalid).toMatchObject({ kind: "failed", error: "Le lot doit contenir exactement 3 cartes." });
    expect((await cycle.snapshot()).current_batch).toBeNull();

    const duplicated = await cycle.requestNextBatch(async () => {
      const result = cards("lot-2");
      result[2].id = result[1].id;
      return result;
    });
    expect(duplicated).toMatchObject({ kind: "failed", error: "Les identifiants de cartes doivent être distincts." });
  });

  test("lit un délai de refroidissement borné depuis la configuration serveur", () => {
    expect(cardRequestCooldownMsFromEnvironment({ CARD_REQUEST_COOLDOWN: "1250" })).toBe(1_250);
    expect(cardRequestCooldownMsFromEnvironment({ CARD_REQUEST_COOLDOWN_MS: "1250" })).toBe(1_250);
    expect(cardRequestCooldownMsFromEnvironment({ CARD_REQUEST_COOLDOWN_MS: "-1" })).toBe(0);
    expect(cardRequestCooldownMsFromEnvironment({ CARD_REQUEST_COOLDOWN_MS: "oops" })).toBe(0);
  });

  test("restaure le compteur et le lot ouvert depuis le fichier serveur atomique", async () => {
    const directory = await mkdtemp(join(tmpdir(), "autopoiesis-card-cycle-"));
    try {
      const path = join(directory, "card-cycle.json");
      const cycle = new CardCycleCoordinator({
        cooldownMs: 0,
        store: new JsonFileCardCycleStore(path),
      });
      expect((await cycle.requestNextBatch(async ({ call }) => {
        await call("appel existant", async () => undefined);
        return cards("lot-fichier");
      })).kind).toBe("generated");

      const restored = new CardCycleCoordinator({
        cooldownMs: 0,
        store: new JsonFileCardCycleStore(path),
      });
      expect(await restored.snapshot()).toMatchObject({
        call_count: 1,
        current_batch: { id: expect.stringContaining("cards-"), status: "awaiting_validation" },
      });
    } finally {
      await rm(directory, { recursive: true, force: true });
    }
  });
});
