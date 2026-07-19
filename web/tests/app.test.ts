import { describe, expect, test } from "bun:test";
import type { BackendEvent, EngineCommand, PublicState } from "../src/protocol";
import { createApp } from "../server/app";
import type { BackendProcessManager } from "../server/backend-process";
import { worldSnapshot } from "./fixtures";

class FakeManager {
  commands: EngineCommand[] = [];
  current: PublicState = {
    state: worldSnapshot(),
    activity: null,
    validation: null,
    evolution: null,
    recompilation: null,
    engine: { status: "running", pid: 321, restarts: 0, last_error: null },
    latest_event: null,
  };
  snapshot() { return this.current; }
  send(command: EngineCommand) { this.commands.push(command); return true; }
  subscribe(_subscriber: (event: BackendEvent) => void) { return () => undefined; }
}

describe("BFF Elysia", () => {
  test("expose santé et projection d’état", async () => {
    const manager = new FakeManager();
    const app = createApp(manager as unknown as BackendProcessManager, { serveStatic: false });
    const health = await app.handle(new Request("http://localhost/api/health"));
    expect(health.status).toBe(200);
    expect(await health.json()).toMatchObject({ status: "ok", has_state: true });
    const state = await app.handle(new Request("http://localhost/api/state"));
    expect(state.status).toBe(200);
    expect((await state.json()).state.simulation_cycle).toBe(7200);
  });

  test("valide strictement la forme et les bornes des commandes", async () => {
    const manager = new FakeManager();
    const app = createApp(manager as unknown as BackendProcessManager, { serveStatic: false });
    const send = (body: unknown) => app.handle(new Request("http://localhost/api/commands", {
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
});
