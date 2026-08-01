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
import {
  CARD_BATCH_SIZE,
  initialCardCycleState,
  parseCardCycleState,
  type EvolutionCard,
} from "../src/card-cycle-protocol";

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

  test("ne relance aucune demande avant la décision puis le délai configuré", async () => {
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

    expect(await cycle.recordCardDecision("lot-1-1", "reject")).toBe(true);
    expect((await cycle.requestNextBatch(generate))).toMatchObject({
      kind: "blocked",
      reason: "cooldown",
      retry_at_ms: 1_250,
    });

    now = 1_250;
    expect((await cycle.requestNextBatch(generate)).kind).toBe("generated");
    expect(generations).toBe(2);
  });

  test("persiste les étapes génération, offre, choix, validation et activation sans appeler pendant un lot incomplet", async () => {
    let now = 1_000;
    let releaseGeneration: (() => void) | undefined;
    const cycle = new CardCycleCoordinator({ now: () => now, cooldownMs: 250 });
    const generating = cycle.requestNextBatch(async () => {
      await new Promise<void>((resolve) => { releaseGeneration = resolve; });
      return cards("lot-étapes");
    });

    await Promise.resolve();
    expect(await cycle.snapshot()).toMatchObject({ stage: "generating", phase: "generating" });
    expect(await cycle.requestNextBatch(async () => cards("interdit-génération"))).toMatchObject({
      kind: "blocked",
      reason: "generation_active",
    });

    releaseGeneration?.();
    expect(await generating).toMatchObject({ kind: "generated" });
    expect(await cycle.snapshot()).toMatchObject({ stage: "offered", phase: "offered" });
    expect(await cycle.requestNextBatch(async () => cards("interdit-offre"))).toMatchObject({
      kind: "blocked",
      reason: "pending_cards",
    });

    expect(await cycle.recordEngineSelection("lot-étapes-2")).toBe(true);
    expect(await cycle.snapshot()).toMatchObject({ stage: "choosing", phase: "choosing" });
    expect(await cycle.recordEngineDecision("lot-étapes-2", "approve")).toBe(true);
    expect(await cycle.snapshot()).toMatchObject({ stage: "validating", phase: "validating" });
    expect(await cycle.requestNextBatch(async () => cards("interdit-validation"))).toMatchObject({
      kind: "blocked",
      reason: "pending_cards",
    });

    expect(await cycle.beginActivation("lot-étapes-2")).toBe(true);
    expect(await cycle.snapshot()).toMatchObject({ stage: "activating", phase: "activating" });
    expect(await cycle.requestNextBatch(async () => cards("interdit-activation"))).toMatchObject({
      kind: "blocked",
      reason: "pending_cards",
    });

    expect(await cycle.completeActivation("lot-étapes-2", true)).toBe(true);
    const activated = await cycle.snapshot();
    expect(activated).toMatchObject({
      stage: "ready",
      phase: "cooldown",
      current_batch: { status: "activated" },
    });
    expect(activated.current_batch?.cards[1]).toMatchObject({ id: "lot-étapes-2", status: "activated" });
    expect((await cycle.requestNextBatch(async () => cards("interdit-cooldown"))).kind).toBe("blocked");

    now = 1_250;
    expect((await cycle.requestNextBatch(async () => cards("après-activation"))).kind).toBe("generated");
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
    const failingStore = new MemoryCardCycleStore();
    const failing = new CardCycleCoordinator({ cooldownMs: 0, store: failingStore });
    const failure = await failing.requestNextBatch(async ({ call }) => {
      await call("appel qui échoue", async () => {
        failedCalls += 1;
        throw new Error("réseau indisponible");
      });
      return cards("inaccessible");
    });
    expect(failure).toMatchObject({ kind: "failed", error: "réseau indisponible" });
    expect(failedCalls).toBe(1);
    expect(await failing.snapshot()).toMatchObject({
      call_count: 1,
      stage: "failed",
      phase: "failed",
      failure: { phase: "generation", message: "réseau indisponible" },
    });
    expect(await failing.requestNextBatch(async () => {
      failedCalls += 100;
      return cards("retry-interdit");
    })).toMatchObject({ kind: "blocked", reason: "failed" });
    expect(failedCalls).toBe(1);

    const restoredFailure = new CardCycleCoordinator({ cooldownMs: 0, store: failingStore });
    expect(await restoredFailure.snapshot()).toMatchObject({ stage: "failed", call_count: 1 });
    expect(await restoredFailure.acknowledgeFailure()).toBe(true);
    expect((await restoredFailure.requestNextBatch(async () => cards("retry-explicite"))).kind).toBe("generated");
  });

  test("conserve un échec d’activation après redémarrage et exige son acquittement explicite", async () => {
    const store = new MemoryCardCycleStore();
    const cycle = new CardCycleCoordinator({ cooldownMs: 0, store });
    await cycle.requestNextBatch(async () => cards("lot-échec"));
    await cycle.recordEngineSelection("lot-échec-1");
    await cycle.recordEngineDecision("lot-échec-1", "approve");
    await cycle.beginActivation("lot-échec-1");
    expect(await cycle.completeActivation("lot-échec-1", false, "activation indisponible")).toBe(true);

    const restored = new CardCycleCoordinator({ cooldownMs: 0, store });
    expect(await restored.snapshot()).toMatchObject({
      stage: "failed",
      current_batch: { status: "failed" },
      failure: { phase: "activation", message: "activation indisponible" },
    });
    expect((await restored.requestNextBatch(async () => cards("retry-interdit"))).kind).toBe("blocked");
    expect(await restored.acknowledgeFailure()).toBe(true);
    expect((await restored.requestNextBatch(async () => cards("nouveau-lot"))).kind).toBe("generated");
  });

  test("transforme une génération interrompue au redémarrage en échec acquittable sans replay", async () => {
    const store = new MemoryCardCycleStore();
    await store.write({
      ...initialCardCycleState(),
      stage: "generating",
      generation_batch_id: "lot-interrompu",
    });
    const restored = new CardCycleCoordinator({ cooldownMs: 0, store });

    expect(await restored.snapshot()).toMatchObject({
      stage: "failed",
      phase: "failed",
      generation_batch_id: null,
      failure: { phase: "generation", batch_id: "lot-interrompu" },
    });
    expect((await restored.requestNextBatch(async () => cards("pas-de-replay")))).toMatchObject({
      kind: "blocked",
      reason: "failed",
    });
  });

  test("migre un lot ouvert v1 vers l’offre v2 sans rouvrir d’appel", () => {
    const migrated = parseCardCycleState({
      protocol_version: 1,
      call_count: 7,
      current_batch: {
        id: "lot-v1",
        status: "awaiting_validation",
        created_at_ms: 1_000,
        cards: cards("v1").map((card) => ({ ...card, status: "pending" })),
      },
      cooldown_until_ms: 0,
      call_alert: null,
      recent_call_keys: [],
    });
    expect(migrated).toMatchObject({
      protocol_version: 2,
      call_count: 7,
      stage: "offered",
      current_batch: { id: "lot-v1", status: "offered" },
    });
  });

  test("sérialise les persistances concurrentes afin que le compteur ne régresse pas", async () => {
    const store = new MemoryCardCycleStore();
    const cycle = new CardCycleCoordinator({ store });
    const counted = await Promise.all(
      Array.from({ length: 20 }, (_, index) => cycle.recordObservedCall(`appel-concurrent-${index}`)),
    );
    expect(counted.every(Boolean)).toBe(true);
    expect(await cycle.snapshot()).toMatchObject({
      call_count: 20,
      call_alert: { call_count: 20, acknowledged: false },
    });
    const restored = new CardCycleCoordinator({ store });
    expect(await restored.snapshot()).toMatchObject({ call_count: 20 });
  });

  test("refuse un contrat qui ne contient pas exactement trois cartes distinctes", async () => {
    const cycle = new CardCycleCoordinator({ cooldownMs: 0 });
    const invalid = await cycle.requestNextBatch(async () => cards("lot-1").slice(0, 2));
    expect(invalid).toMatchObject({ kind: "failed", error: "Le lot doit contenir exactement 3 cartes." });
    expect((await cycle.snapshot()).current_batch).toBeNull();

    const duplicateCycle = new CardCycleCoordinator({ cooldownMs: 0 });
    const duplicated = await duplicateCycle.requestNextBatch(async () => {
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
        stage: "offered",
        current_batch: { id: expect.stringContaining("cards-"), status: "offered" },
      });
    } finally {
      await rm(directory, { recursive: true, force: true });
    }
  });

  test("persiste un lot observé du moteur et n’ouvre le suivant qu’après sa décision puis le cooldown", async () => {
    let now = 10_000;
    const store = new MemoryCardCycleStore();
    const cycle = new CardCycleCoordinator({ now: () => now, cooldownMs: 500, store });

    expect(await cycle.recordObservedCall("7200:period_report:ada:1")).toBe(true);
    expect(await cycle.recordObservedCall("7200:period_report:ada:1")).toBe(false);
    expect(await cycle.ingestEngineBatch("engine-7200", cards("moteur"))).toMatchObject({ kind: "generated" });
    expect(await cycle.recordEngineSelection("moteur-2")).toBe(true);
    expect(await cycle.completeEngineDecision("moteur-2", "reject")).toBe(true);

    const resolved = await cycle.snapshot();
    expect(resolved).toMatchObject({
      call_count: 1,
      current_batch: {
        id: "engine-7200",
        status: "validated",
        selected_card_id: "moteur-2",
        cards: [
          { id: "moteur-1", status: "pending" },
          { id: "moteur-2", status: "rejected" },
          { id: "moteur-3", status: "pending" },
        ],
      },
      phase: "cooldown",
    });

    expect((await cycle.requestNextBatch(async () => cards("trop-tôt"))).kind).toBe("blocked");
    now = 10_500;
    expect((await cycle.requestNextBatch(async () => cards("après-cooldown"))).kind).toBe("generated");

    const restored = new CardCycleCoordinator({ now: () => now, cooldownMs: 500, store });
    expect(await restored.snapshot()).toMatchObject({ call_count: 1, current_batch: { id: expect.any(String) } });
  });

  test("reprend un checkpoint en pleine activation sans doubler l’appel ni l’activation", async () => {
    let now = 20_000;
    const store = new MemoryCardCycleStore();
    const cycle = new CardCycleCoordinator({ now: () => now, cooldownMs: 500, store });

    expect(await cycle.recordObservedCall("7200:period_report:a1:1")).toBe(true);
    expect(await cycle.ingestEngineBatch("engine-7200", cards("milieu-cycle"))).toMatchObject({
      kind: "generated",
    });
    expect(await cycle.recordEngineSelection("milieu-cycle-1")).toBe(true);
    expect(await cycle.recordEngineDecision("milieu-cycle-1", "approve")).toBe(true);
    expect(await cycle.beginActivation("milieu-cycle-1")).toBe(true);
    expect(await cycle.snapshot()).toMatchObject({
      stage: "activating",
      call_count: 1,
      current_batch: { id: "engine-7200", status: "activating" },
    });

    now = 20_100;
    const restored = new CardCycleCoordinator({ now: () => now, cooldownMs: 500, store });
    expect(await restored.snapshot()).toMatchObject({
      stage: "activating",
      call_count: 1,
      current_batch: { id: "engine-7200", status: "activating" },
    });
    expect(await restored.recordObservedCall("7200:period_report:a1:1")).toBe(false);
    expect(await restored.completeActivation("milieu-cycle-1", true)).toBe(true);
    expect(await restored.completeActivation("milieu-cycle-1", true)).toBe(false);

    const finished = await restored.snapshot();
    expect(finished).toMatchObject({
      stage: "ready",
      call_count: 1,
      phase: "cooldown",
      current_batch: { id: "engine-7200", status: "activated" },
    });
    expect(finished.current_batch?.cards.filter((card) => card.status === "activated"))
      .toHaveLength(1);
  });
});
