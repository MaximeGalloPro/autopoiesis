import { describe, expect, test } from "bun:test";
import { BackendProcessManager, type ManagedProcess } from "../server/backend-process";
import { CardCycleCoordinator } from "../server/card-cycle";
import { BackendCardCycleBridge } from "../server/card-cycle-runtime";
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
    expect(manager.send({ type: "service.api", enabled: false })).toBe(true);
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
      "{\"version\":1,\"command\":\"set_api_enabled\",\"enabled\":false}\n",
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

  test("ne crée un nouveau monde qu’après une extinction attestée et un arrêt propre", async () => {
    let finishFirst!: (code: number) => void;
    const firstExit = new Promise<number>((resolve) => { finishFirst = resolve; });
    const launchedArguments: string[][] = [];
    const writes: string[] = [];
    let launch = 0;
    const manager = new BackendProcessManager({
      autoRestart: true,
      spawn: (_binaryPath, _projectRoot, binaryArgs) => {
        launchedArguments.push(binaryArgs);
        launch += 1;
        return {
          pid: launch,
          stdin: {
            write(data) { writes.push(String(data)); return String(data).length; },
            flush() { return 0; },
            end() {},
          },
          stdout: new ReadableStream({ start(controller) { controller.close(); } }),
          stderr: new ReadableStream({ start(controller) { controller.close(); } }),
          exited: launch === 1 ? firstExit : new Promise(() => undefined),
          kill() {},
        };
      },
    });

    await manager.start();
    expect(manager.requestNewWorldRestart()).toBe(false);
    expect(writes).toEqual([]);

    manager.acceptStdout(`AUTOPOIESIS_EVENT ${JSON.stringify({
      version: 1,
      type: "snapshot",
      payload: worldSnapshot({
        agents: [{
          state: { id: "ada", name: "Ada", alive: false },
          mood: "Sans vie",
          available_actions: [],
        }] as unknown as ReturnType<typeof worldSnapshot>["agents"],
        civilization: { status: "extinct", extinction_day: 124, restart_contract: "--new-world" },
      }),
    })}`);
    expect(manager.requestNewWorldRestart()).toBe(true);
    expect(writes).toEqual(["{\"version\":1,\"command\":\"stop\"}\n"]);

    finishFirst(0);
    await Bun.sleep(1);
    expect(launchedArguments).toEqual([[], ["--new-world"]]);
    expect(manager.snapshot().engine.status).toBe("running");
  });

  test("n’ajoute jamais --new-world à une relance automatique", async () => {
    let finish!: (code: number) => void;
    const exited = new Promise<number>((resolve) => { finish = resolve; });
    const launchedArguments: string[][] = [];
    let launch = 0;
    const manager = new BackendProcessManager({
      binaryArgs: ["--new-world"],
      autoRestart: true,
      restartDelayMs: 1,
      spawn: (_binaryPath, _projectRoot, binaryArgs) => {
        launchedArguments.push(binaryArgs);
        launch += 1;
        return {
          pid: launch,
          stdin: { write: () => 0, flush: () => 0, end() {} },
          stdout: new ReadableStream({ start(controller) { controller.close(); } }),
          stderr: new ReadableStream({ start(controller) { controller.close(); } }),
          exited: launch === 1 ? exited : new Promise(() => undefined),
          kill() {},
        };
      },
    });

    await manager.start();
    finish(1);
    await Bun.sleep(5);
    expect(launchedArguments).toEqual([["--new-world"], []]);
  });

  test("projette le cycle persistant sans reboucler sa propre publication", async () => {
    const manager = new BackendProcessManager({ autoRestart: false });
    const bridge = new BackendCardCycleBridge(manager, new CardCycleCoordinator());
    await bridge.flush();
    expect(manager.snapshot().card_cycle).toMatchObject({ phase: "ready", total_api_calls: 0 });
    bridge.stop();
  });
});
