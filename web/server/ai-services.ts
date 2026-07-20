import { existsSync } from "node:fs";
import { resolve } from "node:path";
import type { EngineCommand } from "../src/protocol";

export type AiServiceName = "api" | "codex";

export interface AiServicesState {
  api: { available: boolean; enabled: boolean; detail: string };
  codex: {
    available: boolean;
    authenticated: boolean;
    verifier_available: boolean;
    enabled: boolean;
    detail: string;
  };
}

interface SidecarProcess {
  pid: number;
  exited: Promise<number>;
  kill(signal?: number | NodeJS.Signals): void;
}

export interface AiServiceBackend {
  send(command: EngineCommand): boolean;
}

export interface AiServiceController {
  snapshot(): AiServicesState;
  setEnabled(service: AiServiceName, enabled: boolean): { accepted: boolean; error?: string };
  startConfigured(): void;
  stop(): void;
}

export interface AiServicesOptions {
  projectRoot?: string;
  environment?: NodeJS.ProcessEnv;
  codexPath?: string | null;
  dockerAvailable?: boolean;
  codexAuthenticated?: boolean;
  spawn?: (script: string, projectRoot: string, environment: NodeJS.ProcessEnv) => SidecarProcess;
}

function defaultSpawn(script: string, projectRoot: string, environment: NodeJS.ProcessEnv): SidecarProcess {
  return Bun.spawn({
    cmd: [script],
    cwd: projectRoot,
    env: environment,
    stdin: "ignore",
    stdout: "ignore",
    stderr: "ignore",
  }) as unknown as SidecarProcess;
}

function findExecutable(environment: NodeJS.ProcessEnv, configured: string | undefined, name: string): string | null {
  if (configured && existsSync(configured)) return configured;
  const path = environment.PATH ?? "";
  for (const directory of path.split(":")) {
    const candidate = resolve(directory, name);
    if (existsSync(candidate)) return candidate;
  }
  return null;
}

function loginAvailable(codexPath: string | null, environment: NodeJS.ProcessEnv): boolean {
  if (!codexPath) return false;
  const result = Bun.spawnSync({
    cmd: [codexPath, "login", "status"],
    env: environment,
    stdout: "ignore",
    stderr: "ignore",
  });
  return result.exitCode === 0;
}

export class AiServicesManager implements AiServiceController {
  private readonly backend: AiServiceBackend;
  private readonly projectRoot: string;
  private readonly environment: NodeJS.ProcessEnv;
  private readonly script: string;
  private readonly spawnSidecar: NonNullable<AiServicesOptions["spawn"]>;
  private readonly initialCodexEnabled: boolean;
  private state: AiServicesState;
  private sidecar: SidecarProcess | null = null;
  private stopping = false;

  constructor(backend: AiServiceBackend, options: AiServicesOptions = {}) {
    this.backend = backend;
    this.projectRoot = options.projectRoot ?? resolve(import.meta.dir, "../..");
    this.environment = options.environment ?? process.env;
    this.script = resolve(this.projectRoot, "scripts/run-evolution-daemon-loop.sh");
    this.spawnSidecar = options.spawn ?? defaultSpawn;
    this.initialCodexEnabled = this.environment.EVOLUTION_AUTOSTART === "1";
    const codexPath = options.codexPath === undefined
      ? findExecutable(this.environment, this.environment.CODEX_BIN, "codex")
      : options.codexPath;
    const authenticated = options.codexAuthenticated ?? loginAvailable(codexPath, this.environment);
    const dockerAvailable = options.dockerAvailable
      ?? Boolean(findExecutable(this.environment, undefined, "docker"));
    const apiAvailable = Boolean(this.environment.LLM_API_KEY && this.environment.OPENAI_MODEL);
    const codexAvailable = Boolean(codexPath && authenticated && existsSync(this.script));
    this.state = {
      api: {
        available: apiAvailable,
        enabled: apiAvailable && this.environment.USE_API !== "0",
        detail: apiAvailable ? "Clé et modèle configurés côté serveur." : "Clé ou modèle OpenAI absent.",
      },
      codex: {
        available: codexAvailable,
        authenticated,
        verifier_available: dockerAvailable,
        enabled: false,
        detail: !codexPath ? "CLI Codex absent."
          : !authenticated ? "CLI Codex non authentifié."
          : !dockerAvailable ? "Codex est prêt, mais Docker manque pour la vérification obligatoire."
          : "Codex et le vérificateur Docker sont prêts.",
      },
    };
  }

  snapshot(): AiServicesState {
    return {
      api: { ...this.state.api },
      codex: { ...this.state.codex },
    };
  }

  setEnabled(service: AiServiceName, enabled: boolean): { accepted: boolean; error?: string } {
    if (service === "api") {
      if (enabled && !this.state.api.available) return { accepted: false, error: this.state.api.detail };
      if (!this.backend.send({ type: "service.api", enabled })) {
        return { accepted: false, error: "Le moteur C++ n’est pas disponible." };
      }
      this.state.api.enabled = enabled;
      return { accepted: true };
    }
    if (!enabled) {
      this.stopping = true;
      this.sidecar?.kill("SIGTERM");
      this.sidecar = null;
      this.state.codex.enabled = false;
      return { accepted: true };
    }
    if (!this.state.codex.available || !this.state.codex.verifier_available) {
      return { accepted: false, error: this.state.codex.detail };
    }
    if (this.sidecar) return { accepted: true };
    this.stopping = false;
    try {
      const child = this.spawnSidecar(this.script, this.projectRoot, this.environment);
      this.sidecar = child;
      this.state.codex.enabled = true;
      void child.exited.then((code) => {
        if (this.sidecar !== child) return;
        this.sidecar = null;
        this.state.codex.enabled = false;
        if (!this.stopping) this.state.codex.detail = `Le daemon Codex s’est arrêté (code ${code}).`;
      });
      return { accepted: true };
    } catch (error) {
      this.state.codex.enabled = false;
      return { accepted: false, error: error instanceof Error ? error.message : String(error) };
    }
  }

  startConfigured(): void {
    if (this.initialCodexEnabled) this.setEnabled("codex", true);
  }

  stop(): void {
    this.setEnabled("codex", false);
  }
}
