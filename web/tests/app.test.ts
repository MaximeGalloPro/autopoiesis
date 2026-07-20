import { describe, expect, test } from "bun:test";
import type { BackendEvent, EngineCommand, PublicState } from "../src/protocol";
import { BROWSER_TRANSPORT_PREFIX } from "../src/transport";
import { createApp, createEventRelay } from "../server/app";
import type { BackendProcessManager } from "../server/backend-process";
import type { AiServiceController, AiServicesState } from "../server/ai-services";
import { worldSnapshot } from "./fixtures";

class FakeManager {
  commands: EngineCommand[] = [];
  current: PublicState = {
    state: worldSnapshot(),
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
  subscribe(_subscriber: (event: BackendEvent) => void) { return () => undefined; }
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
  test("protège toute la surface web par Basic Auth", async () => {
    const manager = new FakeManager();
    const app = createApp(manager as unknown as BackendProcessManager, {
      serveStatic: false,
      basicAuth: { username: "test-user", password: "test-password" },
    });
    const healthUrl = `http://localhost${BROWSER_TRANSPORT_PREFIX}/health`;

    const anonymous = await app.handle(new Request(healthUrl));
    expect(anonymous.status).toBe(401);
    expect(anonymous.headers.get("www-authenticate")).toBe('Basic realm="Autopoiesis", charset="UTF-8"');

    const rejected = await app.handle(new Request(healthUrl, {
      headers: { authorization: `Basic ${btoa("test-user:incorrect")}` },
    }));
    expect(rejected.status).toBe(401);

    const accepted = await app.handle(new Request(healthUrl, {
      headers: { authorization: `Basic ${btoa("test-user:test-password")}` },
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
    const app = createApp(manager as unknown as BackendProcessManager, { serveStatic: false, basicAuth: false });
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
    const app = createApp(manager as unknown as BackendProcessManager, { serveStatic: false, services, basicAuth: false });
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
    const app = createApp(manager as unknown as BackendProcessManager, { serveStatic: false, basicAuth: false });
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
      basicAuth: false,
    });
    const response = await app.handle(new Request(`http://localhost${BROWSER_TRANSPORT_PREFIX}/commands`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ type: "validation.decision", request_id: "request-1", decision: "approve" }),
    }));
    expect(response.status).toBe(403);
    expect(manager.commands).toEqual([]);
  });
});
