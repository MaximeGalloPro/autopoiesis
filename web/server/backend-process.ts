import { existsSync } from "node:fs";
import { resolve } from "node:path";
import type {
  AiActivity,
  BackendEvent,
  EngineCommand,
  EngineInfo,
  EvolutionProgress,
  PublicState,
  RecompileProgress,
  ValidationPrompt,
  WorldSnapshot,
} from "../src/protocol";
import { parseBackendEvent } from "../src/protocol";

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
  autoRestart?: boolean;
  restartDelayMs?: number;
  maxRestarts?: number;
  spawn?: (binaryPath: string, projectRoot: string) => ManagedProcess;
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

function bunSpawn(binaryPath: string, projectRoot: string): ManagedProcess {
  return Bun.spawn({
    cmd: [binaryPath],
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
  private readonly autoRestart: boolean;
  private readonly restartDelayMs: number;
  private readonly maxRestarts: number;
  private readonly spawnProcess: (binaryPath: string, projectRoot: string) => ManagedProcess;
  private child: ManagedProcess | null = null;
  private subscribers = new Set<Subscriber>();
  private stopping = false;
  private restartTimer: ReturnType<typeof setTimeout> | null = null;
  private state: WorldSnapshot | null = null;
  private activity: AiActivity | null = null;
  private validation: ValidationPrompt | null = null;
  private evolution: EvolutionProgress | null = null;
  private recompilation: RecompileProgress | null = null;
  private latestEvent: BackendEvent | null = null;
  private engine: EngineInfo = initialEngine();

  constructor(options: BackendProcessOptions = {}) {
    this.projectRoot = options.projectRoot ?? resolve(import.meta.dir, "../..");
    this.binaryPath = options.binaryPath
      ?? process.env.AUTOPOIESIS_BACKEND_PATH
      ?? resolve(this.projectRoot, "build/autopoiesis_backend");
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
      const child = this.spawnProcess(this.binaryPath, this.projectRoot);
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
    try {
      this.child.stdin.write(`${JSON.stringify(command)}\n`);
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
      recompilation: this.recompilation,
      engine: { ...this.engine },
      latest_event: this.latestEvent,
    };
  }

  acceptStdout(line: string): void {
    const event = parseBackendEvent(line);
    if (!event) return;
    switch (event.type) {
      case "state": this.state = event.payload; break;
      case "activity": this.activity = event.payload; break;
      case "validation": this.validation = event.payload; break;
      case "evolution_progress": this.evolution = event.payload; break;
      case "recompilation": this.recompilation = event.payload; break;
    }
    this.publish(event);
  }

  private acceptStderr(line: string): void {
    const sanitized = line.trim().slice(-500);
    if (!sanitized) return;
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
