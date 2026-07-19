import { describe, expect, test } from "bun:test";
import { BackendProcessManager, type ManagedProcess } from "../server/backend-process";
import { worldSnapshot } from "./fixtures";

describe("processus backend", () => {
  test("écrit une commande JSON par ligne sans l’appliquer localement", async () => {
    const writes: string[] = [];
    let killedWith: number | NodeJS.Signals | undefined;
    const child: ManagedProcess = {
      pid: 123,
      stdin: {
        write(data) { writes.push(String(data)); return String(data).length; },
        flush() { return 0; },
        end() {},
      },
      stdout: new ReadableStream({ start(controller) { controller.close(); } }),
      stderr: new ReadableStream({ start(controller) { controller.close(); } }),
      exited: new Promise(() => undefined),
      kill(signal) { killedWith = signal; },
    };
    const manager = new BackendProcessManager({ autoRestart: false, spawn: () => child });
    expect(await manager.start()).toBe(true);
    expect(manager.send({ type: "control.speed", multiplier: 2 })).toBe(true);
    manager.acceptStdout(`AUTOPOIESIS_EVENT ${JSON.stringify({
      version: 1,
      type: "validation_prompt",
      payload: {
        kind: "feature",
        stage: "choose",
        day: 3,
        simulation_cycle: 720,
        requests: [{ id: "request-1", title: "Un abri", need: "Rester au sec" }],
        selected_index: 0,
        allowed_commands: ["1", "n", "q"],
      },
    })}`);
    expect(manager.send({ type: "validation.select", request_id: "request-1" })).toBe(true);
    manager.acceptStdout(`AUTOPOIESIS_EVENT ${JSON.stringify({
      version: 1,
      type: "validation_prompt",
      payload: {
        kind: "feature",
        stage: "confirm",
        day: 3,
        simulation_cycle: 720,
        requests: [{ id: "request-1", title: "Un abri", need: "Rester au sec" }],
        selected_index: 1,
        allowed_commands: ["a", "r", "b", "d", "q"],
      },
    })}`);
    expect(manager.send({ type: "validation.decision", request_id: "request-1", decision: "approve" })).toBe(true);
    expect(writes).toEqual([
      "{\"version\":1,\"command\":\"set_speed\",\"speed\":2}\n",
      "{\"version\":1,\"command\":\"validation\",\"text\":\"1\"}\n",
      "{\"version\":1,\"command\":\"validation\",\"text\":\"a\"}\n",
    ]);
    expect(manager.snapshot().state).toBeNull();
    manager.stop();
    expect(killedWith).toBe("SIGTERM");
  });

  test("diffuse et mémorise le dernier état autoritaire", () => {
    const manager = new BackendProcessManager({ autoRestart: false });
    const observed: string[] = [];
    manager.subscribe((event) => observed.push(event.type));
    const snapshot = worldSnapshot({ simulation_cycle: 42 });
    manager.acceptStdout(`AUTOPOIESIS_EVENT ${JSON.stringify({ version: 1, type: "snapshot", payload: snapshot })}`);
    manager.acceptStdout("journal humain ignoré");
    expect(manager.snapshot().state?.simulation_cycle).toBe(42);
    expect(observed).toEqual(["state"]);
  });

  test("ne relance pas une simulation terminée normalement", async () => {
    let finish!: (code: number) => void;
    let spawnCount = 0;
    const exited = new Promise<number>((resolve) => { finish = resolve; });
    const manager = new BackendProcessManager({
      autoRestart: true,
      restartDelayMs: 1,
      spawn: () => {
        spawnCount += 1;
        return {
          pid: 456,
          stdin: { write: () => 0, flush: () => 0, end() {} },
          stdout: new ReadableStream({ start(controller) { controller.close(); } }),
          stderr: new ReadableStream({ start(controller) { controller.close(); } }),
          exited,
          kill() {},
        };
      },
    });
    await manager.start();
    finish(0);
    await Bun.sleep(5);
    expect(spawnCount).toBe(1);
    expect(manager.snapshot().engine).toMatchObject({ status: "stopped", restarts: 0, last_error: null });
  });
});
