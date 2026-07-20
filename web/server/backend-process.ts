import { existsSync } from "node:fs";
import { resolve } from "node:path";
import type {
  AiActivity,
  BackendEvent,
  EngineCommand,
  EngineInfo,
  EvolutionProgress,
  EvolutionCompletion,
  PublicState,
  RecompileProgress,
  RuntimeStatus,
  ValidationPrompt,
  WorldSnapshot,
} from "../src/protocol";
import { DEFAULT_RUNTIME_STATUS, parseBackendEvent } from "../src/protocol";

interface ProcessInput {
  write(data: string | Uint8Array): number;
  flush(): number | Promise<number>;
  end(): void;
}

export interface ManagedProcess {
  pid: number;
  stdin: ProcessInput;
  stdout: ReadableStream<Uint8Array>;
  stderr: ReadableStream<Uint8Array>;
  exited: Promise<number>;
  kill(signal?: number | NodeJS.Signals): void;
}

export interface BackendProcessOptions {
  projectRoot?: string;
  binaryPath?: string;
  binaryArgs?: string[];
  autoRestart?: boolean;
  restartDelayMs?: number;
  maxRestarts?: number;
  spawn?: (binaryPath: string, projectRoot: string, binaryArgs: string[]) => ManagedProcess;
}

type Subscriber = (event: BackendEvent) => void;

const initialEngine = (): EngineInfo => ({
  status: "stopped",
  pid: null,
  restarts: 0,
  last_error: null,
});

async function consumeLines(
  stream: ReadableStream<Uint8Array>,
  onLine: (line: string) => void,
): Promise<void> {
  const reader = stream.getReader();
  const decoder = new TextDecoder();
  let buffered = "";
  try {
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      buffered += decoder.decode(value, { stream: true });
      const lines = buffered.split(/\r?\n/);
      buffered = lines.pop() ?? "";
      for (const line of lines) onLine(line);
    }
    buffered += decoder.decode();
    if (buffered) onLine(buffered);
  } finally {
    reader.releaseLock();
  }
}

function bunSpawn(binaryPath: string, projectRoot: string, binaryArgs: string[]): ManagedProcess {
  const command = [binaryPath, ...binaryArgs];
  if (process.env.USE_API === "0" && !binaryArgs.includes("--no-api")) command.push("--no-api");
  return Bun.spawn({
    cmd: command,
    cwd: projectRoot,
    env: process.env,
    stdin: "pipe",
    stdout: "pipe",
    stderr: "pipe",
  }) as unknown as ManagedProcess;
}

export class BackendProcessManager {
  readonly projectRoot: string;
  readonly binaryPath: string;
  private readonly binaryArgs: string[];
  private readonly autoRestart: boolean;
  private readonly restartDelayMs: number;
  private readonly maxRestarts: number;
  private readonly spawnProcess: (
    binaryPath: string,
    projectRoot: string,
    binaryArgs: string[],
  ) => ManagedProcess;
  private child: ManagedProcess | null = null;
  private subscribers = new Set<Subscriber>();
  private stopping = false;
  private restartTimer: ReturnType<typeof setTimeout> | null = null;
  private state: WorldSnapshot | null = null;
  private activity: AiActivity | null = null;
  private validation: ValidationPrompt | null = null;
  private evolution: EvolutionProgress | null = null;
  private evolutionCompletion: EvolutionCompletion | null = null;
  private recompilation: RecompileProgress | null = null;
  private runtime: RuntimeStatus = { ...DEFAULT_RUNTIME_STATUS };
  private latestEvent: BackendEvent | null = null;
  private engine: EngineInfo = initialEngine();

  constructor(options: BackendProcessOptions = {}) {
    this.projectRoot = options.projectRoot ?? resolve(import.meta.dir, "../..");
    this.binaryPath = options.binaryPath
      ?? process.env.AUTOPOIESIS_BACKEND_PATH
      ?? resolve(this.projectRoot, "build/autopoiesis_backend");
    this.binaryArgs = [...(options.binaryArgs ?? [])];
    this.autoRestart = options.autoRestart ?? process.env.AUTOPOIESIS_BACKEND_RESTART !== "0";
    this.restartDelayMs = options.restartDelayMs ?? 1_500;
    this.maxRestarts = options.maxRestarts ?? 5;
    this.spawnProcess = options.spawn ?? bunSpawn;
  }

  async start(): Promise<boolean> {
    if (this.child) return true;
    this.stopping = false;
    if (!existsSync(this.binaryPath) && this.spawnProcess === bunSpawn) {
      this.updateEngine("unavailable", `Binaire introuvable : ${this.binaryPath}`);
      return false;
    }

    this.updateEngine(this.engine.restarts > 0 ? "restarting" : "starting", null);
    try {
      const child = this.spawnProcess(this.binaryPath, this.projectRoot, this.binaryArgs);
      this.child = child;
      this.engine = { ...this.engine, status: "running", pid: child.pid, last_error: null };
      this.publish({ type: "engine", payload: this.engine });
      void consumeLines(child.stdout, (line) => this.acceptStdout(line));
      void consumeLines(child.stderr, (line) => this.acceptStderr(line));
      void child.exited.then((code) => this.handleExit(code));
      return true;
    } catch (error) {
      this.child = null;
      this.updateEngine("unavailable", error instanceof Error ? error.message : String(error));
      return false;
    }
  }

  stop(): void {
    this.stopping = true;
    if (this.restartTimer) clearTimeout(this.restartTimer);
    this.restartTimer = null;
    this.child?.kill("SIGTERM");
    this.child = null;
    this.updateEngine("stopped", null);
  }

  send(command: EngineCommand): boolean {
    if (!this.child || this.engine.status !== "running") return false;
    const wireCommand = this.toWireCommand(command);
    if (!wireCommand) return false;
    try {
      this.child.stdin.write(`${JSON.stringify(wireCommand)}\n`);
      void this.child.stdin.flush();
      return true;
    } catch (error) {
      this.updateEngine("unavailable", error instanceof Error ? error.message : String(error));
      return false;
    }
  }

  subscribe(subscriber: Subscriber): () => void {
    this.subscribers.add(subscriber);
    return () => this.subscribers.delete(subscriber);
  }

  snapshot(): PublicState {
    return {
      state: this.state,
      activity: this.activity,
      validation: this.validation,
      evolution: this.evolution,
      evolution_completion: this.evolutionCompletion,
      recompilation: this.recompilation,
      engine: { ...this.engine },
      latest_event: this.latestEvent,
    };
  }

  acceptStdout(line: string): void {
    const event = parseBackendEvent(line, this.runtime);
    if (!event) return;
    switch (event.type) {
      case "runtime":
        this.runtime = event.payload;
        if (this.state) this.state = {
          ...this.state,
          paused: event.payload.paused,
          speed: event.payload.speed,
          delay_ms: event.payload.delay_ms,
        };
        break;
      case "state":
        this.state = event.payload;
        this.activity = null;
        this.validation = null;
        this.evolution = null;
        this.evolutionCompletion = null;
        this.recompilation = null;
        break;
      case "activity": this.activity = event.payload; break;
      case "validation":
        this.validation = event.payload;
        this.activity = null;
        break;
      case "evolution_progress":
        this.evolution = event.payload;
        this.validation = null;
        break;
      case "evolution_completion":
        this.evolutionCompletion = event.payload;
        this.evolution = event.payload;
        this.validation = null;
        break;
      case "recompilation":
        this.recompilation = event.payload;
        this.evolutionCompletion = null;
        break;
    }
    this.publish(event);
  }

  private toWireCommand(command: EngineCommand): Record<string, unknown> | null {
    switch (command.type) {
      case "control.pause":
        return { version: 1, command: command.paused ? "pause" : "resume" };
      case "control.speed":
        return { version: 1, command: "set_speed", speed: command.multiplier };
      case "control.delay":
        return { version: 1, command: "set_delay_ms", delay_ms: command.milliseconds };
      case "service.api":
        return { version: 1, command: "set_api_enabled", enabled: command.enabled };
      case "simulation.stop":
        return { version: 1, command: "stop" };
      case "validation.select": {
        if (!this.validation || this.validation.stage !== "choose") return null;
        const index = this.validation.requests.findIndex((request) => request.request_id === command.request_id);
        const text = String(index + 1);
        return index >= 0 && this.validation.allowed_commands.includes(text)
          ? { version: 1, command: "validation", text } : null;
      }
      case "validation.decision": {
        if (!this.validation || this.validation.stage !== "confirm"
          || this.validation.selected_request_id !== command.request_id) return null;
        const text = command.decision === "approve" ? "a" : "r";
        return this.validation.allowed_commands.includes(text)
          ? { version: 1, command: "validation", text } : null;
      }
      case "validation.back":
        return this.validation?.allowed_commands.includes("b")
          ? { version: 1, command: "validation", text: "b" } : null;
      case "validation.none":
        return this.validation?.allowed_commands.includes("n")
          ? { version: 1, command: "validation", text: "n" } : null;
      case "simulation.resume": {
        const allowed = this.validation?.allowed_commands.includes("o")
          || this.evolutionCompletion?.allowed_commands.includes("o");
        return allowed ? { version: 1, command: "validation", text: "o" } : null;
      }
    }
    return null;
  }

  private acceptStderr(line: string): void {
    const sanitized = line.trim().slice(-500);
    if (!sanitized) return;
    const isError = /fatal|error|erreur|failed|échec|exception/i.test(sanitized);
    // Les sorties narratives restent côté serveur : le WebSocket n'est pas un
    // endpoint de logs bruts. Seul un diagnostic borné utile à l'interface est
    // projeté lorsqu'il s'agit réellement d'une erreur.
    if (!isError) return;
    this.engine = { ...this.engine, last_error: sanitized };
    this.publish({
      type: "notice",
      payload: { level: "error", message: sanitized, at: new Date().toISOString() },
    });
  }

  private handleExit(code: number): void {
    this.child = null;
    if (this.stopping) {
      this.updateEngine("stopped", null);
      return;
    }
    const message = `Le moteur s’est arrêté avec le code ${code}.`;
    if (code === 0) {
      this.updateEngine("stopped", null);
      return;
    }
    if (!this.autoRestart) {
      this.updateEngine("stopped", message);
      return;
    }
    if (this.engine.restarts >= this.maxRestarts) {
      this.updateEngine("unavailable", `${message} Limite de ${this.maxRestarts} relances atteinte.`);
      return;
    }
    this.engine = {
      status: "restarting",
      pid: null,
      restarts: this.engine.restarts + 1,
      last_error: message,
    };
    this.publish({ type: "engine", payload: this.engine });
    const backoff = Math.min(30_000, this.restartDelayMs * 2 ** Math.max(0, this.engine.restarts - 1));
    this.restartTimer = setTimeout(() => {
      this.restartTimer = null;
      void this.start();
    }, backoff);
  }

  private updateEngine(status: EngineInfo["status"], error: string | null): void {
    this.engine = { ...this.engine, status, pid: null, last_error: error };
    this.publish({ type: "engine", payload: this.engine });
  }

  private publish(event: BackendEvent): void {
    this.latestEvent = event;
    for (const subscriber of this.subscribers) subscriber(event);
  }
}
