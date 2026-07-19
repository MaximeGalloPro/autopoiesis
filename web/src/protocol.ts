export const WORLD_WIDTH = 40 as const;
export const WORLD_HEIGHT = 24 as const;
export const SIMULATION_SPEEDS = [0.25, 0.5, 1, 2, 4] as const;

export type SimulationSpeed = (typeof SIMULATION_SPEEDS)[number];
export type ConnectionState = "connecting" | "connected" | "reconnecting" | "error";
export type EngineStatus = "starting" | "running" | "restarting" | "stopped" | "unavailable";
export type Terrain = "ground" | "wall" | "water" | "tree" | "bush";
export type Season = "spring" | "summer" | "autumn" | "winter";
export type DayPhase = "day" | "night";
export type ProjectStatus = "candidate" | "active" | "blocked" | "completed" | "abandoned";
export type AnimalType = "rabbit" | "deer" | "boar" | "wolf" | "fish";

export interface Position {
  x: number;
  y: number;
}

export interface CalendarDate {
  absolute_day: number;
  day_of_month: number;
  month: number;
  year: number;
  season: Season;
}

export interface ClimateState {
  temperature_c: number;
  rainfall_mm: number;
  condition: string;
}

export interface WorldCell {
  position: Position;
  terrain: Terrain;
  food: number;
  wood: number;
  fibers: number;
  shelter_level: number;
  branches: number;
  campfire: boolean;
  stored_food: number;
}

export interface Personality {
  curiosity: number;
  prudence: number;
  sociability: number;
  patience: number;
  empathy: number;
}

export interface Attributes {
  strength: number;
  agility: number;
  endurance: number;
  toughness: number;
  recuperation: number;
  disease_resistance: number;
  focus: number;
  willpower: number;
  memory: number;
  spatial_sense: number;
}

export interface BehaviorProfile {
  archetype: string;
  aspiration: string;
  construction_drive: number;
  provision_drive: number;
  exploration_drive: number;
  social_drive: number;
  preferred_foods: string[];
}

export interface Project {
  key: string;
  title: string;
  status: ProjectStatus;
  step: number;
  progress: number;
  target: number;
  blocked_reason: string;
  missing_capability: string;
  started_day: number;
  last_progress_cycle: number;
}

export interface Relationship {
  trust: number;
  affinity: number;
  interactions: number;
}

export interface CarriedFood {
  type: string;
  nutrition: number;
}

export interface AgentState {
  id: string;
  name: string;
  position: Position;
  health: number;
  hunger: number;
  thirst: number;
  fatigue: number;
  alive: boolean;
  sleeping_days: number;
  boredom: number;
  mood: string;
  personality: Personality;
  attributes: Attributes;
  behavior: BehaviorProfile;
  project: Project;
  memories: string[];
  relationships: Record<string, Relationship>;
  available_actions: string[];
  wood_inventory: number;
  branch_inventory: number;
  carried_food?: CarriedFood | null;
}

export interface AnimalState {
  id: string;
  type: AnimalType;
  position: Position;
  alive: boolean;
  danger: number;
  nutrition: number;
}

export interface WorldSnapshot {
  date: CalendarDate;
  simulation_cycle: number;
  climate: ClimateState;
  phase: DayPhase;
  cycle_in_day: number;
  cycles_per_day: number;
  width: 40;
  height: 24;
  cells: WorldCell[];
  agents: AgentState[];
  animals: AnimalState[];
  recent_events: string[];
  paused: boolean;
  speed: SimulationSpeed;
  delay_ms: number;
}

export type ActivityKind = "period_report" | "evolution_request";
export interface AiActivity {
  kind: ActivityKind;
  agent_id: string;
  agent_name: string;
  call_number: number;
  total_calls: number;
  elapsed_ms: number;
}

export interface EvolutionRequest {
  request_id: string;
  agent_id?: string;
  agent_name?: string;
  title: string;
  need: string;
  obstacle: string;
  proposed_change: string;
  mechanism: string | Record<string, unknown>;
  acceptance_tests: string[];
  status: "pending" | "approved" | "rejected" | "activated";
  source?: "agent" | "devil";
}

export interface ValidationPrompt {
  kind: "feature" | "devil";
  stage: "empty" | "choose" | "confirm" | "complete";
  day: number;
  simulation_cycle: number;
  requests: EvolutionRequest[];
  selected_request_id?: string;
  allowed_commands: string[];
  real_world_basis?: string;
  future_pressure?: string;
  detail?: string;
}

export type EvolutionStage =
  | "queued"
  | "preparing"
  | "implementing"
  | "reporting"
  | "verifying"
  | "correcting"
  | "activating"
  | "complete"
  | "failed"
  | "timed_out";

export interface EvolutionProgress {
  stage: EvolutionStage;
  request_id: string;
  message: string;
  detail: string;
  elapsed_seconds: number;
  successful: boolean;
}

export interface RecompileProgress {
  stage: "compiling" | "ready" | "failed";
  elapsed_ms: number;
  detail: string;
}

export interface EvolutionCompletion extends EvolutionProgress {
  allowed_commands: string[];
}

export interface RuntimeStatus {
  state: "running" | "paused" | "stopping" | "completed";
  paused: boolean;
  speed: SimulationSpeed;
  delay_ms: number;
  message: string;
  accepted?: boolean;
  command?: string;
}

export interface EngineInfo {
  status: EngineStatus;
  pid: number | null;
  restarts: number;
  last_error: string | null;
}

export interface PublicState {
  state: WorldSnapshot | null;
  activity: AiActivity | null;
  validation: ValidationPrompt | null;
  evolution: EvolutionProgress | null;
  evolution_completion: EvolutionCompletion | null;
  recompilation: RecompileProgress | null;
  engine: EngineInfo;
  latest_event: BackendEvent | null;
}

export type BackendEvent =
  | { type: "state"; payload: WorldSnapshot }
  | { type: "runtime"; payload: RuntimeStatus }
  | { type: "activity"; payload: AiActivity | null }
  | { type: "validation"; payload: ValidationPrompt | null }
  | { type: "evolution_progress"; payload: EvolutionProgress | null }
  | { type: "evolution_completion"; payload: EvolutionCompletion | null }
  | { type: "recompilation"; payload: RecompileProgress | null }
  | { type: "notice"; payload: { level: "info" | "warning" | "error"; message: string; at?: string } }
  | { type: "engine"; payload: EngineInfo };

export type ClientMessage =
  | { type: "snapshot"; payload: PublicState }
  | { type: "event"; payload: BackendEvent };

export type EngineCommand =
  | { type: "control.pause"; paused: boolean }
  | { type: "control.speed"; multiplier: SimulationSpeed }
  | { type: "control.delay"; milliseconds: number }
  | { type: "validation.select"; request_id: string }
  | { type: "validation.decision"; request_id: string; decision: "approve" | "reject" }
  | { type: "validation.back" }
  | { type: "validation.none" }
  | { type: "simulation.resume" }
  | { type: "simulation.stop" };

export const DEFAULT_RUNTIME_STATUS: RuntimeStatus = {
  state: "running",
  paused: false,
  speed: 1,
  delay_ms: 500,
  message: "initializing",
};

export function isWorldSnapshot(value: unknown): value is WorldSnapshot {
  if (!value || typeof value !== "object") return false;
  const candidate = value as Partial<WorldSnapshot>;
  return candidate.width === WORLD_WIDTH && candidate.height === WORLD_HEIGHT
    && Array.isArray(candidate.cells) && Array.isArray(candidate.agents)
    && Array.isArray(candidate.animals) && Array.isArray(candidate.recent_events)
    && typeof candidate.simulation_cycle === "number"
    && typeof candidate.paused === "boolean"
    && SIMULATION_SPEEDS.includes(candidate.speed as SimulationSpeed)
    && Number.isInteger(candidate.delay_ms) && (candidate.delay_ms ?? -1) >= 0 && (candidate.delay_ms ?? 10_001) <= 10_000;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return !!value && typeof value === "object" && !Array.isArray(value);
}

function normalizedSeason(value: unknown): Season | null {
  const seasons: Record<string, Season> = {
    spring: "spring",
    printemps: "spring",
    summer: "summer",
    "été": "summer",
    autumn: "autumn",
    automne: "autumn",
    winter: "winter",
    hiver: "winter",
  };
  return typeof value === "string" ? seasons[value] ?? null : null;
}

function normalizeSnapshot(value: unknown, runtime: RuntimeStatus): WorldSnapshot | null {
  if (!isRecord(value) || value.width !== WORLD_WIDTH || value.height !== WORLD_HEIGHT
    || !Array.isArray(value.cells) || !Array.isArray(value.agents)
    || !Array.isArray(value.animals) || !Array.isArray(value.recent_events)
    || !isRecord(value.date) || !isRecord(value.climate)) return null;
  const season = normalizedSeason(value.date.season);
  if (!season) return null;

  const agents: AgentState[] = [];
  for (const entry of value.agents) {
    if (!isRecord(entry) || !isRecord(entry.state) || typeof entry.mood !== "string"
      || !Array.isArray(entry.available_actions)
      || !entry.available_actions.every((action) => typeof action === "string")) return null;
    agents.push({
      ...(entry.state as unknown as Omit<AgentState, "mood" | "available_actions">),
      mood: entry.mood,
      available_actions: entry.available_actions,
    });
  }

  const snapshot = {
    ...value,
    date: { ...value.date, season },
    agents,
    paused: runtime.paused,
    speed: runtime.speed,
    delay_ms: runtime.delay_ms,
  } as unknown as WorldSnapshot;
  return isWorldSnapshot(snapshot) ? snapshot : null;
}

function normalizeRequest(value: unknown): EvolutionRequest | null {
  if (!isRecord(value)) return null;
  const requestId = typeof value.request_id === "string" ? value.request_id
    : typeof value.id === "string" ? value.id : null;
  if (!requestId || typeof value.title !== "string" || typeof value.need !== "string") return null;
  const rawStatus = String(value.status ?? "pending");
  const status = ["pending", "approved", "rejected", "activated"].includes(rawStatus)
    ? rawStatus as EvolutionRequest["status"] : "pending";
  const source = value.source === "devil" ? "devil"
    : value.source === "ai_period_report" ? "agent" : undefined;
  return {
    request_id: requestId,
    agent_id: typeof value.agent_id === "string" ? value.agent_id : undefined,
    agent_name: typeof value.agent_name === "string" ? value.agent_name : undefined,
    title: value.title,
    need: value.need,
    obstacle: typeof value.obstacle === "string" ? value.obstacle : "",
    proposed_change: typeof value.proposed_change === "string" ? value.proposed_change : "",
    mechanism: typeof value.mechanism === "string" || isRecord(value.mechanism)
      ? value.mechanism : {},
    acceptance_tests: Array.isArray(value.acceptance_tests)
      ? value.acceptance_tests.filter((test): test is string => typeof test === "string") : [],
    status,
    source,
  };
}

function isActivity(value: unknown): value is AiActivity | null {
  if (value === null) return true;
  if (!isRecord(value)) return false;
  return (value.kind === "period_report" || value.kind === "evolution_request")
    && typeof value.agent_id === "string" && typeof value.agent_name === "string"
    && Number.isInteger(value.call_number) && Number.isInteger(value.total_calls)
    && typeof value.elapsed_ms === "number";
}

function isValidation(value: unknown): value is ValidationPrompt | null {
  if (value === null) return true;
  if (!isRecord(value)) return false;
  return (value.kind === "feature" || value.kind === "devil")
    && (value.stage === "empty" || value.stage === "choose" || value.stage === "confirm" || value.stage === "complete")
    && Number.isInteger(value.day) && Number.isInteger(value.simulation_cycle)
    && Array.isArray(value.requests) && Array.isArray(value.allowed_commands);
}

function normalizeValidation(value: unknown): ValidationPrompt | null {
  if (!isRecord(value) || (value.kind !== "feature" && value.kind !== "devil")
    || !["empty", "choose", "confirm", "complete"].includes(String(value.stage))
    || !Number.isInteger(value.day) || !Number.isInteger(value.simulation_cycle)
    || !Array.isArray(value.requests) || !Array.isArray(value.allowed_commands)) return null;
  const requests = value.requests.map(normalizeRequest);
  if (requests.some((request) => request === null)) return null;
  const normalizedRequests = requests as EvolutionRequest[];
  const selectedIndex = Number.isInteger(value.selected_index) ? Number(value.selected_index) : 0;
  const firstRawRequest = value.requests[0];
  const firstRequest = isRecord(firstRawRequest) ? firstRawRequest : undefined;
  const prompt: ValidationPrompt = {
    kind: value.kind,
    stage: value.stage as ValidationPrompt["stage"],
    day: Number(value.day),
    simulation_cycle: Number(value.simulation_cycle),
    requests: normalizedRequests,
    selected_request_id: selectedIndex > 0 ? normalizedRequests[selectedIndex - 1]?.request_id : undefined,
    allowed_commands: value.allowed_commands.filter((command): command is string => typeof command === "string"),
    real_world_basis: typeof value.real_world_basis === "string" ? value.real_world_basis
      : typeof firstRequest?.real_world_basis === "string" ? firstRequest.real_world_basis : undefined,
    future_pressure: typeof value.future_pressure === "string" ? value.future_pressure
      : typeof firstRequest?.future_pressure === "string" ? firstRequest.future_pressure : undefined,
    detail: typeof value.detail === "string" ? value.detail : undefined,
  };
  return isValidation(prompt) ? prompt : null;
}

function isEvolution(value: unknown): value is EvolutionProgress | null {
  if (value === null) return true;
  if (!isRecord(value)) return false;
  return ["queued", "preparing", "implementing", "reporting", "verifying", "correcting", "activating", "complete", "failed", "timed_out"].includes(String(value.stage))
    && typeof value.request_id === "string" && typeof value.message === "string"
    && typeof value.detail === "string" && typeof value.elapsed_seconds === "number"
    && typeof value.successful === "boolean";
}

function isRecompilation(value: unknown): value is RecompileProgress | null {
  if (value === null) return true;
  if (!isRecord(value)) return false;
  return ["compiling", "ready", "failed"].includes(String(value.stage))
    && typeof value.elapsed_ms === "number" && typeof value.detail === "string";
}

function isRuntimeStatus(value: unknown): value is RuntimeStatus {
  if (!isRecord(value)) return false;
  return ["running", "paused", "stopping", "completed"].includes(String(value.state))
    && typeof value.paused === "boolean"
    && SIMULATION_SPEEDS.includes(value.speed as SimulationSpeed)
    && Number.isInteger(value.delay_ms) && Number(value.delay_ms) >= 0 && Number(value.delay_ms) <= 10_000
    && typeof value.message === "string";
}

function isEvolutionCompletion(value: unknown): value is EvolutionCompletion {
  return isEvolution(value) && value !== null && isRecord(value)
    && Array.isArray(value.allowed_commands)
    && value.allowed_commands.every((command) => typeof command === "string");
}

function isNotice(value: unknown): value is Extract<BackendEvent, { type: "notice" }>["payload"] {
  return isRecord(value) && ["info", "warning", "error"].includes(String(value.level))
    && typeof value.message === "string";
}

export function parseBackendEvent(
  line: string,
  runtime: RuntimeStatus = DEFAULT_RUNTIME_STATUS,
): BackendEvent | null {
  const prefix = "AUTOPOIESIS_EVENT ";
  if (!line.startsWith(prefix)) return null;
  try {
    const parsed = JSON.parse(line.slice(prefix.length)) as { version?: unknown; type?: unknown; payload?: unknown };
    if (!parsed || typeof parsed !== "object" || parsed.version !== 1
      || typeof parsed.type !== "string" || !("payload" in parsed)) {
      return null;
    }
    switch (parsed.type) {
      case "status": return isRuntimeStatus(parsed.payload)
        ? { type: "runtime", payload: parsed.payload } : null;
      case "snapshot": {
        const snapshot = normalizeSnapshot(parsed.payload, runtime);
        return snapshot ? { type: "state", payload: snapshot } : null;
      }
      case "activity": return isActivity(parsed.payload)
        ? { type: "activity", payload: parsed.payload } : null;
      case "validation_prompt": {
        const prompt = normalizeValidation(parsed.payload);
        return prompt ? { type: "validation", payload: prompt } : null;
      }
      case "evolution_progress": return isEvolution(parsed.payload)
        ? { type: "evolution_progress", payload: parsed.payload } : null;
      case "evolution_completion": return isEvolutionCompletion(parsed.payload)
        ? { type: "evolution_completion", payload: parsed.payload } : null;
      case "recompilation": return isRecompilation(parsed.payload)
        ? { type: "recompilation", payload: parsed.payload } : null;
      case "notice": return isNotice(parsed.payload)
        ? { type: "notice", payload: parsed.payload } : null;
      default: return null;
    }
  } catch {
    return null;
  }
}

export function toricDistance(a: Position, b: Position): number {
  const dx = Math.min(Math.abs(a.x - b.x), WORLD_WIDTH - Math.abs(a.x - b.x));
  const dy = Math.min(Math.abs(a.y - b.y), WORLD_HEIGHT - Math.abs(a.y - b.y));
  return dx + dy;
}
