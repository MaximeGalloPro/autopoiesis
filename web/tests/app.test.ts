import { describe, expect, test } from "bun:test";
import type { BackendEvent, EngineCommand, PublicState } from "../src/protocol";
import { BROWSER_TRANSPORT_PREFIX } from "../src/transport";
import { createApp, createEventRelay } from "../server/app";
import type { BackendProcessManager } from "../server/backend-process";
import type { AiServiceController, AiServicesState } from "../server/ai-services";
import { CardCycleCoordinator } from "../server/card-cycle";
import { BackendCardCycleBridge } from "../server/card-cycle-runtime";
import { worldSnapshot } from "./fixtures";

class FakeManager {
  commands: EngineCommand[] = [];
  private subscribers = new Set<(event: BackendEvent) => void>();
  current: PublicState = {
    state: worldSnapshot(),
    card_cycle: null,
    awaiting_dawn: false,
    activity: null,
    validation: null,
    evolution: null,
    evolution_completion: null,
    recompilation: null,
    engine: { status: "running", pid: 321, restarts: 0, last_error: null },
    latest_event: null,
  };
  snapshot() { return this.current; }
  send(command: EngineCommand) { this.commands.push(command); return true; }
  subscribe(subscriber: (event: BackendEvent) => void) {
    this.subscribers.add(subscriber);
    return () => this.subscribers.delete(subscriber);
  }
  emit(event: BackendEvent) {
    if (event.type === "validation") this.current = { ...this.current, validation: event.payload };
    for (const subscriber of this.subscribers) subscriber(event);
  }
  setCardCycleSnapshot(card_cycle: NonNullable<PublicState["card_cycle"]>) {
    this.current = { ...this.current, card_cycle };
  }
}

class FakeServices implements AiServiceController {
  current: AiServicesState = {
    api: { available: true, enabled: false, detail: "API prête." },
    codex: { available: true, authenticated: true, verifier_available: true, enabled: false, detail: "Codex prêt." },
  };
  snapshot() { return structuredClone(this.current); }
  setEnabled(service: "api" | "codex", enabled: boolean) {
    this.current[service].enabled = enabled;
    return { accepted: true };
  }
  startConfigured() {}
  stop() {}
}

describe("BFF Elysia", () => {
  test("protège toute la surface web par une session à mot de passe seul", async () => {
    const manager = new FakeManager();
    const app = createApp(manager as unknown as BackendProcessManager, {
      serveStatic: false,
      passwordAuth: { password: "test-password" },
    });
    const healthUrl = `http://localhost${BROWSER_TRANSPORT_PREFIX}/health`;

    const anonymous = await app.handle(new Request(healthUrl));
    expect(anonymous.status).toBe(401);
    expect(anonymous.headers.get("www-authenticate")).toBeNull();

    const rejected = await app.handle(new Request(healthUrl, {
      headers: { authorization: `Basic ${btoa(":incorrect")}` },
    }));
    expect(rejected.status).toBe(401);

    const login = await app.handle(new Request(`http://localhost/__auth/login`, {
      method: "POST",
      headers: { "content-type": "application/x-www-form-urlencoded" },
      body: "password=test-password",
    }));
    expect(login.status).toBe(303);
    const session = login.headers.get("set-cookie");
    expect(session).toContain("autopoiesis_session=");

    const accepted = await app.handle(new Request(healthUrl, {
      headers: { cookie: session?.split(";")[0] ?? "" },
    }));
    expect(accepted.status).toBe(200);
  });

  test("coalesce les instantanés sans retarder une garde", async () => {
    const events: BackendEvent[] = [];
    const relay = createEventRelay((event) => events.push(event), 5);
    relay.accept({ type: "state", payload: worldSnapshot({ simulation_cycle: 1 }) });
    relay.accept({ type: "state", payload: worldSnapshot({ simulation_cycle: 2 }) });
    expect(events).toEqual([]);
    await Bun.sleep(8);
    expect(events).toHaveLength(1);
    expect(events[0]?.type === "state" && events[0].payload.simulation_cycle).toBe(2);

    relay.accept({ type: "state", payload: worldSnapshot({ simulation_cycle: 3 }) });
    relay.accept({ type: "validation", payload: null });
    expect(events.slice(-2).map((event) => event.type)).toEqual(["state", "validation"]);
    relay.close();
  });

  test("expose le transport navigateur et les alias API", async () => {
    const manager = new FakeManager();
    const app = createApp(manager as unknown as BackendProcessManager, { serveStatic: false, passwordAuth: false });
    for (const prefix of [BROWSER_TRANSPORT_PREFIX, "/api"]) {
      const health = await app.handle(new Request(`http://localhost${prefix}/health`));
      expect(health.status).toBe(200);
      expect(await health.json()).toMatchObject({ status: "ok", has_state: true });
      const state = await app.handle(new Request(`http://localhost${prefix}/state`));
      expect(state.status).toBe(200);
      expect((await state.json()).state.simulation_cycle).toBe(7200);
    }
  });

  test("expose des commandes bornées pour les services IA", async () => {
    const manager = new FakeManager();
    const services = new FakeServices();
    const app = createApp(manager as unknown as BackendProcessManager, { serveStatic: false, services, passwordAuth: false });
    const endpoint = `http://localhost${BROWSER_TRANSPORT_PREFIX}/services`;
    const initial = await app.handle(new Request(endpoint));
    expect(initial.status).toBe(200);
    expect((await initial.json()).api.enabled).toBe(false);

    const enabled = await app.handle(new Request(endpoint, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ service: "api", enabled: true }),
    }));
    expect(enabled.status).toBe(200);
    expect((await enabled.json()).services.api.enabled).toBe(true);

    const invalid = await app.handle(new Request(endpoint, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ service: "shell", enabled: true }),
    }));
    expect(invalid.status).toBe(422);
  });

  test("valide strictement la forme et les bornes des commandes", async () => {
    const manager = new FakeManager();
    const app = createApp(manager as unknown as BackendProcessManager, { serveStatic: false, passwordAuth: false });
    const send = (body: unknown) => app.handle(new Request(`http://localhost${BROWSER_TRANSPORT_PREFIX}/commands`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify(body),
    }));

    expect((await send({ type: "control.speed", multiplier: 2 })).status).toBe(202);
    expect((await send({ type: "control.speed", multiplier: 3 })).status).toBe(422);
    expect((await send({ type: "control.delay", milliseconds: 10_001 })).status).toBe(422);
    expect((await send({ type: "simulation.resume", injected: true })).status).toBe(422);
    expect((await send({ type: "validation.decision", request_id: "../secret", decision: "approve" })).status).toBe(422);
    expect(manager.commands).toEqual([{ type: "control.speed", multiplier: 2 }]);
  });

  test("interdit l’approbation réelle dans une preview publique", async () => {
    const manager = new FakeManager();
    const app = createApp(manager as unknown as BackendProcessManager, {
      serveStatic: false,
      safePreview: true,
      passwordAuth: false,
    });
    const response = await app.handle(new Request(`http://localhost${BROWSER_TRANSPORT_PREFIX}/commands`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ type: "validation.decision", request_id: "request-1", decision: "approve" }),
    }));
    expect(response.status).toBe(403);
    expect(manager.commands).toEqual([]);
  });

  test("expose les commandes strictes du cycle de cartes sans les confondre avec le moteur", async () => {
    const manager = new FakeManager();
    const cycle = new CardCycleCoordinator({ cooldownMs: 0 });
    const generated = await cycle.requestNextBatch(async (context) => {
      for (let call = 1; call <= 10; call += 1) {
        await context.call(`appel-${call}`, async () => undefined);
      }
      return ["a", "b", "c"].map((id) => ({
        id,
        title: `Carte ${id}`,
        need: "Besoin",
        obstacle: "Obstacle",
        proposed_change: "Changement",
        mechanism: "Mécanisme",
        acceptance_tests: ["Test"],
      }));
    });
    expect(generated.kind).toBe("generated");
    const app = createApp(manager as unknown as BackendProcessManager, {
      serveStatic: false,
      passwordAuth: false,
      cardCycle: cycle,
    });
    const endpoint = `http://localhost${BROWSER_TRANSPORT_PREFIX}/card-cycle`;
    const send = (body: unknown) => app.handle(new Request(`${endpoint}/commands`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify(body),
    }));

    const snapshot = await app.handle(new Request(endpoint));
    expect(snapshot.status).toBe(200);
    expect(await snapshot.json()).toMatchObject({ total_api_calls: 10, phase: "call_alert" });
    expect((await send({ type: "acknowledge_call_alert", extra: true })).status).toBe(422);
    expect((await send({ type: "acknowledge_call_alert" })).status).toBe(202);
    expect((await send({ type: "card_decision", card_id: "a", decision: "approve" })).status).toBe(202);
    expect((await send({ type: "card_decision", card_id: "a", decision: "approve" })).status).toBe(409);
    expect(manager.commands).toEqual([]);
  });

  test("n’autorise la reprise après échec du cycle que par une commande humaine stricte", async () => {
    const manager = new FakeManager();
    const cycle = new CardCycleCoordinator({ cooldownMs: 0 });
    await cycle.requestNextBatch(async () => {
      throw new Error("fournisseur indisponible");
    });
    const app = createApp(manager as unknown as BackendProcessManager, {
      serveStatic: false,
      passwordAuth: false,
      cardCycle: cycle,
    });
    const endpoint = `http://localhost${BROWSER_TRANSPORT_PREFIX}/card-cycle/commands`;
    const send = (body: unknown) => app.handle(new Request(endpoint, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify(body),
    }));

    expect((await send({ type: "acknowledge_failure", extra: true })).status).toBe(422);
    expect((await send({ type: "acknowledge_failure" })).status).toBe(202);
    expect(await cycle.snapshot()).toMatchObject({ stage: "ready", failure: null });
    expect(manager.commands).toEqual([]);
  });

  test("raccorde les trois cartes du moteur aux routes réelles et persiste leur décision", async () => {
    const manager = new FakeManager();
    const cycle = new CardCycleCoordinator({ cooldownMs: 100 });
    const bridge = new BackendCardCycleBridge(manager as unknown as BackendProcessManager, cycle);
    const app = createApp(manager as unknown as BackendProcessManager, {
      serveStatic: false,
      passwordAuth: false,
      cardCycle: cycle,
      cardCycleBridge: bridge,
    });
    for (let call = 1; call <= 10; call += 1) {
      manager.emit({
        type: "activity",
        payload: {
          kind: "period_report",
          agent_id: "ada",
          agent_name: "Ada",
          simulation_cycle: call,
          call_number: 1,
          total_calls: 1,
          elapsed_ms: 0,
        },
      });
    }
    await bridge.flush();
    const cycleEndpoint = `http://localhost${BROWSER_TRANSPORT_PREFIX}/card-cycle`;
    const acknowledged = await app.handle(new Request(`${cycleEndpoint}/commands`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ type: "acknowledge_call_alert" }),
    }));
    expect(acknowledged.status).toBe(202);
    expect(manager.snapshot().card_cycle?.call_alert).toEqual({ call_count: 10, acknowledged: true });
    const cards = ["a", "b", "c"].map((id) => ({
      request_id: id,
      title: `Carte ${id}`,
      need: "Besoin observé",
      obstacle: "Obstacle concret",
      proposed_change: "Changement proposé",
      mechanism: "Mécanisme déterministe",
      acceptance_tests: ["Test exécutable"],
      status: "pending" as const,
    }));

    manager.emit({
      type: "validation",
      payload: {
        kind: "feature",
        stage: "choose",
        day: 3,
        simulation_cycle: 7200,
        requests: cards,
        allowed_commands: ["1", "2", "3", "n", "q"],
      },
    });
    await bridge.flush();

    const commandEndpoint = `http://localhost${BROWSER_TRANSPORT_PREFIX}/commands`;
    const select = await app.handle(new Request(commandEndpoint, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ type: "validation.select", request_id: "b" }),
    }));
    expect(select.status).toBe(202);
    manager.emit({
      type: "validation",
      payload: {
        kind: "feature",
        stage: "confirm",
        day: 3,
        simulation_cycle: 7200,
        requests: cards,
        selected_request_id: "b",
        allowed_commands: ["a", "r", "b", "q"],
      },
    });
    await bridge.flush();
    const reject = await app.handle(new Request(commandEndpoint, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ type: "validation.decision", request_id: "b", decision: "reject" }),
    }));
    expect(reject.status).toBe(202);
    await bridge.flush();

    const snapshot = await app.handle(new Request(cycleEndpoint));
    expect(await snapshot.json()).toMatchObject({
      phase: "cooldown",
      current_batch: {
        status: "validated",
        selected_card_id: "b",
        cards: [
          { id: "a", status: "pending" },
          { id: "b", status: "rejected" },
          { id: "c", status: "pending" },
        ],
      },
    });
    expect(manager.commands).toEqual([
      { type: "validation.select", request_id: "b" },
      { type: "validation.decision", request_id: "b", decision: "reject" },
    ]);
    bridge.stop();
  });

  test("observe l’activation sans jamais envoyer une commande de pause au monde", async () => {
    const manager = new FakeManager();
    const cycle = new CardCycleCoordinator({ cooldownMs: 100 });
    const bridge = new BackendCardCycleBridge(manager as unknown as BackendProcessManager, cycle);
    const app = createApp(manager as unknown as BackendProcessManager, {
      serveStatic: false,
      passwordAuth: false,
      cardCycle: cycle,
      cardCycleBridge: bridge,
    });
    const cards = ["a", "b", "c"].map((id) => ({
      request_id: id,
      title: `Carte ${id}`,
      need: "Besoin observé",
      obstacle: "Obstacle concret",
      proposed_change: "Changement proposé",
      mechanism: "Mécanisme déterministe",
      acceptance_tests: ["Test exécutable"],
      status: "pending" as const,
    }));
    manager.emit({
      type: "validation",
      payload: {
        kind: "feature",
        stage: "choose",
        day: 3,
        simulation_cycle: 7200,
        requests: cards,
        allowed_commands: ["1", "2", "3", "n", "q"],
      },
    });
    await bridge.flush();

    const endpoint = `http://localhost${BROWSER_TRANSPORT_PREFIX}/commands`;
    for (const body of [
      { type: "validation.select", request_id: "a" },
      { type: "validation.decision", request_id: "a", decision: "approve" },
    ]) {
      expect((await app.handle(new Request(endpoint, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(body),
      }))).status).toBe(202);
    }
    await bridge.flush();
    expect(await cycle.snapshot()).toMatchObject({ stage: "validating", phase: "validating" });

    manager.emit({
      type: "evolution_progress",
      payload: {
        stage: "activating",
        request_id: "a",
        message: "Activation en cours",
        detail: "Commit prêt",
        elapsed_seconds: 1,
        successful: false,
      },
    });
    await bridge.flush();
    expect(await cycle.snapshot()).toMatchObject({ stage: "activating", phase: "activating" });

    manager.emit({
      type: "evolution_progress",
      payload: {
        stage: "complete",
        request_id: "a",
        message: "Activation terminée",
        detail: "Version active",
        elapsed_seconds: 2,
        successful: true,
      },
    });
    await bridge.flush();
    const activated = await cycle.snapshot();
    expect(activated).toMatchObject({
      stage: "ready",
      phase: "cooldown",
      current_batch: { status: "activated" },
    });
    expect(activated.current_batch?.cards[0]).toMatchObject({ id: "a", status: "activated" });
    expect(manager.commands).toEqual([
      { type: "validation.select", request_id: "a" },
      { type: "validation.decision", request_id: "a", decision: "approve" },
    ]);
    bridge.stop();
  });
});
