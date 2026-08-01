/**
 * Contrat entre l'orchestrateur serveur et la présentation des cartes.
 *
 * Les cartes restent des propositions : ce protocole ne porte aucune commande
 * du monde et ne constitue jamais une autorisation d'activation.
 */
export const CARD_BATCH_SIZE = 3 as const;
export const CARD_CYCLE_PROTOCOL_VERSION = 2 as const;
const LEGACY_CARD_CYCLE_PROTOCOL_VERSION = 1 as const;

export type CardDecision = "approve" | "reject";
export type CardStatus = "pending" | "approved" | "rejected" | "activated";
export type CardBatchStatus =
  | "offered"
  | "choosing"
  | "validating"
  | "activating"
  | "validated"
  | "activated"
  | "failed";
export type CardBatchResolution =
  | "all_cards"
  | "engine_decision"
  | "engine_none"
  | "failure_acknowledged";
/** Étapes persistantes, distinctes de l'alerte de quota et du cooldown. */
export type CardCycleStage =
  | "ready"
  | "generating"
  | "offered"
  | "choosing"
  | "validating"
  | "activating"
  | "failed";
/** Phase de présentation : le cooldown et l'alerte sont des gardes transverses. */
export type CardCyclePhase = CardCycleStage | "cooldown" | "call_alert";

/** Commandes humaines bornées, traitées par l'orchestrateur et non par l'IA. */
export type CardCycleCommand =
  | { type: "acknowledge_call_alert" }
  | { type: "acknowledge_failure" }
  | { type: "card_decision"; card_id: string; decision: CardDecision };

export interface EvolutionCard {
  /** Identifiant produit localement par le protocole, jamais par l'IA seule. */
  id: string;
  title: string;
  need: string;
  obstacle: string;
  proposed_change: string;
  mechanism: string;
  acceptance_tests: string[];
}

export interface PendingEvolutionCard extends EvolutionCard {
  status: CardStatus;
  decided_at_ms?: number;
}

export interface CardBatch {
  id: string;
  status: CardBatchStatus;
  created_at_ms: number;
  validated_at_ms?: number;
  activation_started_at_ms?: number;
  activated_at_ms?: number;
  failed_at_ms?: number;
  /** Sélection locale persistée, revalidée ensuite par le moteur C++. */
  selected_card_id?: string;
  /** Une résolution est toujours explicite avant de libérer le lot. */
  resolution?: CardBatchResolution;
  cards: PendingEvolutionCard[];
}

export interface ApiCallAlert {
  call_count: number;
  acknowledged: boolean;
}

/** Aucun payload IA brut : seulement un diagnostic borné et persistant. */
export interface CardCycleFailure {
  phase: "generation" | "activation";
  message: string;
  occurred_at_ms: number;
  batch_id?: string;
}

/** État sérialisable : il doit survivre au redémarrage du processus serveur. */
export interface CardCycleState {
  protocol_version: typeof CARD_CYCLE_PROTOCOL_VERSION;
  call_count: number;
  current_batch: CardBatch | null;
  cooldown_until_ms: number;
  call_alert: ApiCallAlert | null;
  /** Clés bornées des activités moteur déjà comptées après une reconnexion. */
  recent_call_keys: string[];
  /** Étape métier autoritaire du serveur, jamais un état du monde. */
  stage: CardCycleStage;
  /** Identifiant réservé avant le tout premier appel du générateur. */
  generation_batch_id: string | null;
  /** Une erreur bloque toute nouvelle génération jusqu'à un acquittement humain. */
  failure: CardCycleFailure | null;
}

export interface CardCycleSnapshot extends CardCycleState {
  /** Nom public du compteur persistant consommé par l'observatoire. */
  total_api_calls: number;
  phase: CardCyclePhase;
  active_call_count: number;
}

export interface CardGenerationContext {
  batch_id: string;
  /**
   * Passe un appel existant dans le garde-fou du serveur. Les appels sont
   * exécutés un à un et comptés avant leur démarrage, sans retry implicite.
   */
  call<T>(label: string, operation: () => Promise<T>): Promise<T>;
}

export type ExistingCardGenerator = (
  context: CardGenerationContext,
) => Promise<readonly EvolutionCard[]>;

export type CardGenerationResult =
  | { kind: "generated"; batch: CardBatch }
  | {
    kind: "blocked";
    reason: "generation_active" | "pending_cards" | "cooldown" | "call_alert" | "failed";
    retry_at_ms?: number;
  }
  | { kind: "failed"; error: string };

export function initialCardCycleState(): CardCycleState {
  return {
    protocol_version: CARD_CYCLE_PROTOCOL_VERSION,
    call_count: 0,
    current_batch: null,
    cooldown_until_ms: 0,
    call_alert: null,
    recent_call_keys: [],
    stage: "ready",
    generation_batch_id: null,
    failure: null,
  };
}

function hasExactKeys(value: Record<string, unknown>, expected: readonly string[]): boolean {
  const keys = Object.keys(value);
  return keys.length === expected.length && keys.every((key) => expected.includes(key));
}

/** Refuse les champs inattendus avant d'atteindre l'état persistant. */
export function isCardCycleCommand(value: unknown): value is CardCycleCommand {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const command = value as Record<string, unknown>;
  switch (command.type) {
    case "acknowledge_call_alert":
    case "acknowledge_failure":
      return hasExactKeys(command, ["type"]);
    case "card_decision":
      return hasExactKeys(command, ["type", "card_id", "decision"])
        && isNonEmptyString(command.card_id)
        && command.card_id.length <= 256
        && (command.decision === "approve" || command.decision === "reject");
    default:
      return false;
  }
}

function isNonEmptyString(value: unknown): value is string {
  return typeof value === "string" && value.trim().length > 0;
}

function isCardStatus(value: unknown): value is CardStatus {
  return value === "pending" || value === "approved" || value === "rejected" || value === "activated";
}

function isCardBatchStatus(value: unknown): value is CardBatchStatus {
  return value === "offered" || value === "choosing" || value === "validating"
    || value === "activating" || value === "validated" || value === "activated" || value === "failed";
}

function isCardCycleStage(value: unknown): value is CardCycleStage {
  return value === "ready" || value === "generating" || value === "offered"
    || value === "choosing" || value === "validating" || value === "activating" || value === "failed";
}

function isCardBatchResolution(value: unknown): value is CardBatchResolution {
  return value === "all_cards" || value === "engine_decision" || value === "engine_none"
    || value === "failure_acknowledged";
}

function isEvolutionCard(value: unknown): value is EvolutionCard {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const card = value as Record<string, unknown>;
  return isNonEmptyString(card.id)
    && isNonEmptyString(card.title)
    && isNonEmptyString(card.need)
    && isNonEmptyString(card.obstacle)
    && isNonEmptyString(card.proposed_change)
    && isNonEmptyString(card.mechanism)
    && Array.isArray(card.acceptance_tests)
    && card.acceptance_tests.length > 0
    && card.acceptance_tests.every(isNonEmptyString);
}

function isPendingCard(value: unknown): value is PendingEvolutionCard {
  if (!isEvolutionCard(value)) return false;
  const card = value as unknown as Record<string, unknown>;
  return isCardStatus(card.status)
    && (card.decided_at_ms === undefined || Number.isFinite(card.decided_at_ms));
}

function finiteOptional(value: unknown): boolean {
  return value === undefined || Number.isFinite(value);
}

function selectedCard(batch: Record<string, unknown>, cards: PendingEvolutionCard[]): PendingEvolutionCard | null {
  const selectedCardId = batch.selected_card_id;
  if (!isNonEmptyString(selectedCardId)) return null;
  return cards.find((card) => card.id === selectedCardId) ?? null;
}

function isCardBatch(value: unknown): value is CardBatch {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const batch = value as Record<string, unknown>;
  if (!(isNonEmptyString(batch.id)
    && isCardBatchStatus(batch.status)
    && Number.isFinite(batch.created_at_ms)
    && finiteOptional(batch.validated_at_ms)
    && finiteOptional(batch.activation_started_at_ms)
    && finiteOptional(batch.activated_at_ms)
    && finiteOptional(batch.failed_at_ms)
    && Array.isArray(batch.cards)
    && batch.cards.length === CARD_BATCH_SIZE
    && batch.cards.every(isPendingCard))) return false;
  const cards = batch.cards as PendingEvolutionCard[];
  if (new Set(cards.map((card) => card.id)).size !== CARD_BATCH_SIZE) return false;
  const selected = selectedCard(batch, cards);
  if (batch.selected_card_id !== undefined && !selected) return false;
  const resolution = batch.resolution;
  if (resolution !== undefined && !isCardBatchResolution(resolution)) return false;

  switch (batch.status) {
    case "offered":
      return !selected && resolution === undefined && cards.every((card) => card.status === "pending")
        && batch.validated_at_ms === undefined;
    case "choosing":
      return !!selected && resolution === undefined && cards.every((card) => card.status === "pending")
        && batch.validated_at_ms === undefined;
    case "validating":
      return !!selected && resolution === "engine_decision" && Number.isFinite(batch.validated_at_ms)
        && selected.status !== "pending" && batch.activation_started_at_ms === undefined;
    case "activating":
      return !!selected && resolution === "engine_decision" && selected.status === "approved"
        && Number.isFinite(batch.validated_at_ms) && Number.isFinite(batch.activation_started_at_ms);
    case "validated":
      if (!Number.isFinite(batch.validated_at_ms)) return false;
      if (resolution === "engine_none") return !selected && cards.every((card) => card.status === "pending");
      if (resolution === "engine_decision") return !!selected && selected.status === "rejected";
      if (resolution === "all_cards") return cards.every((card) => card.status !== "pending");
      return resolution === "failure_acknowledged" && !!selected && selected.status === "approved";
    case "activated":
      return !!selected && resolution === "engine_decision" && selected.status === "activated"
        && Number.isFinite(batch.validated_at_ms) && Number.isFinite(batch.activation_started_at_ms)
        && Number.isFinite(batch.activated_at_ms);
    case "failed":
      return !!selected && resolution === "engine_decision" && selected.status === "approved"
        && Number.isFinite(batch.validated_at_ms) && Number.isFinite(batch.failed_at_ms);
  }
}

function isFailure(value: unknown): value is CardCycleFailure {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const failure = value as Record<string, unknown>;
  return (failure.phase === "generation" || failure.phase === "activation")
    && isNonEmptyString(failure.message) && failure.message.length <= 1_024
    && Number.isFinite(failure.occurred_at_ms)
    && (failure.batch_id === undefined || (isNonEmptyString(failure.batch_id) && failure.batch_id.length <= 256));
}

function parseAlert(value: unknown, callCount: number): ApiCallAlert | null | undefined {
  if (value === null) return null;
  if (!value || typeof value !== "object" || Array.isArray(value)) return undefined;
  const alert = value as Record<string, unknown>;
  if (!Number.isSafeInteger(alert.call_count) || Number(alert.call_count) <= 0
    || Number(alert.call_count) > callCount || Number(alert.call_count) % 10 !== 0
    || typeof alert.acknowledged !== "boolean") return undefined;
  return { call_count: Number(alert.call_count), acknowledged: alert.acknowledged };
}

function isRecentCallKeys(value: unknown): value is string[] {
  return Array.isArray(value) && value.length <= 96
    && value.every((key) => isNonEmptyString(key) && key.length <= 256);
}

function stateMatchesStage(state: CardCycleState): boolean {
  const hasTerminalBatch = state.current_batch === null || state.current_batch.status === "validated"
    || state.current_batch.status === "activated";
  if (state.stage === "ready") {
    return hasTerminalBatch && state.generation_batch_id === null && state.failure === null;
  }
  if (state.stage === "generating") {
    return hasTerminalBatch && state.failure === null && isNonEmptyString(state.generation_batch_id);
  }
  if (state.stage === "failed") {
    if (!state.failure || state.generation_batch_id !== null) return false;
    if (state.failure.phase === "generation") return hasTerminalBatch;
    return state.current_batch?.status === "failed" && state.current_batch.id === state.failure.batch_id;
  }
  if (!state.current_batch || state.generation_batch_id !== null || state.failure !== null) return false;
  return state.current_batch.status === state.stage;
}

function parseCurrentState(value: Record<string, unknown>): CardCycleState | null {
  if (value.protocol_version !== CARD_CYCLE_PROTOCOL_VERSION
    || !Number.isSafeInteger(value.call_count) || Number(value.call_count) < 0
    || !Number.isFinite(value.cooldown_until_ms)
    || !(value.current_batch === null || isCardBatch(value.current_batch))
    || !isCardCycleStage(value.stage)
    || !(value.generation_batch_id === null || (isNonEmptyString(value.generation_batch_id) && value.generation_batch_id.length <= 256))
    || !(value.failure === null || isFailure(value.failure))
    || !isRecentCallKeys(value.recent_call_keys)) return null;
  const callCount = Number(value.call_count);
  const callAlert = parseAlert(value.call_alert, callCount);
  if (callAlert === undefined) return null;
  const state: CardCycleState = {
    protocol_version: CARD_CYCLE_PROTOCOL_VERSION,
    call_count: callCount,
    current_batch: value.current_batch === null ? null : cloneCardBatch(value.current_batch),
    cooldown_until_ms: Number(value.cooldown_until_ms),
    call_alert: callAlert,
    recent_call_keys: [...value.recent_call_keys],
    stage: value.stage,
    generation_batch_id: value.generation_batch_id,
    failure: value.failure ? { ...value.failure } : null,
  };
  return stateMatchesStage(state) ? state : null;
}

type LegacyBatch = {
  id: string;
  status: "awaiting_validation" | "validated";
  created_at_ms: number;
  validated_at_ms?: number;
  selected_card_id?: string;
  resolution?: "all_cards" | "engine_decision" | "engine_none";
  cards: PendingEvolutionCard[];
};

function isLegacyBatch(value: unknown): value is LegacyBatch {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const batch = value as Record<string, unknown>;
  if (!(isNonEmptyString(batch.id)
    && (batch.status === "awaiting_validation" || batch.status === "validated")
    && Number.isFinite(batch.created_at_ms)
    && finiteOptional(batch.validated_at_ms)
    && Array.isArray(batch.cards) && batch.cards.length === CARD_BATCH_SIZE
    && batch.cards.every(isPendingCard))) return false;
  const cards = batch.cards as PendingEvolutionCard[];
  if (new Set(cards.map((card) => card.id)).size !== CARD_BATCH_SIZE) return false;
  const selected = selectedCard(batch, cards);
  if (batch.selected_card_id !== undefined && !selected) return false;
  const resolution = batch.resolution;
  if (resolution !== undefined && resolution !== "all_cards"
    && resolution !== "engine_decision" && resolution !== "engine_none") return false;
  if (batch.status === "awaiting_validation") {
    return batch.validated_at_ms === undefined && resolution === undefined && cards.some((card) => card.status === "pending");
  }
  if (!Number.isFinite(batch.validated_at_ms)) return false;
  if (resolution === "engine_decision") return !!selected && selected.status !== "pending";
  if (resolution === "engine_none") return !selected && cards.every((card) => card.status === "pending");
  return cards.every((card) => card.status !== "pending");
}

/** Migration conservatrice : un ancien choix approuvé reste bloqué en validation. */
function migrateLegacyState(value: Record<string, unknown>): CardCycleState | null {
  if (value.protocol_version !== LEGACY_CARD_CYCLE_PROTOCOL_VERSION
    || !Number.isSafeInteger(value.call_count) || Number(value.call_count) < 0
    || !Number.isFinite(value.cooldown_until_ms)
    || !(value.current_batch === null || isLegacyBatch(value.current_batch))) return null;
  const callCount = Number(value.call_count);
  const callAlert = parseAlert(value.call_alert, callCount);
  const recentCallKeys = value.recent_call_keys === undefined ? [] : value.recent_call_keys;
  if (callAlert === undefined || !isRecentCallKeys(recentCallKeys)) return null;
  const legacy = value.current_batch;
  if (legacy === null) {
    return {
      ...initialCardCycleState(),
      call_count: callCount,
      cooldown_until_ms: Number(value.cooldown_until_ms),
      call_alert: callAlert,
      recent_call_keys: [...recentCallKeys],
    };
  }
  const cards = legacy.cards.map((card) => ({ ...card, acceptance_tests: [...card.acceptance_tests] }));
  const base = {
    id: legacy.id,
    created_at_ms: legacy.created_at_ms,
    cards,
    ...(legacy.selected_card_id === undefined ? {} : { selected_card_id: legacy.selected_card_id }),
    ...(legacy.validated_at_ms === undefined ? {} : { validated_at_ms: legacy.validated_at_ms }),
  };
  let batch: CardBatch;
  let stage: CardCycleStage;
  if (legacy.status === "awaiting_validation") {
    const choosing = legacy.selected_card_id !== undefined;
    batch = { ...base, status: choosing ? "choosing" : "offered" };
    stage = choosing ? "choosing" : "offered";
  } else if (legacy.resolution === "engine_decision") {
    const selected = cards.find((card) => card.id === legacy.selected_card_id);
    if (!selected) return null;
    if (selected.status === "approved") {
      batch = { ...base, status: "validating", resolution: "engine_decision" };
      stage = "validating";
    } else {
      batch = { ...base, status: "validated", resolution: "engine_decision" };
      stage = "ready";
    }
  } else if (legacy.resolution === "engine_none") {
    batch = { ...base, status: "validated", resolution: "engine_none" };
    stage = "ready";
  } else {
    batch = { ...base, status: "validated", resolution: "all_cards" };
    stage = cards.some((card) => card.status === "approved") ? "validating" : "ready";
    if (stage === "validating") {
      const approved = cards.find((card) => card.status === "approved");
      if (!approved) return null;
      batch.selected_card_id = approved.id;
      batch.status = "validating";
      batch.resolution = "engine_decision";
    }
  }
  return {
    protocol_version: CARD_CYCLE_PROTOCOL_VERSION,
    call_count: callCount,
    current_batch: batch,
    cooldown_until_ms: Number(value.cooldown_until_ms),
    call_alert: callAlert,
    recent_call_keys: [...recentCallKeys],
    stage,
    generation_batch_id: null,
    failure: null,
  };
}

/** Rejette les états non fiables plutôt que de réouvrir une génération. */
export function parseCardCycleState(value: unknown): CardCycleState | null {
  if (!value || typeof value !== "object" || Array.isArray(value)) return null;
  const state = value as Record<string, unknown>;
  const parsed = state.protocol_version === CARD_CYCLE_PROTOCOL_VERSION
    ? parseCurrentState(state)
    : migrateLegacyState(state);
  return parsed ? cloneCardCycleState(parsed) : null;
}

export function cloneCardBatch(batch: CardBatch): CardBatch {
  return {
    ...batch,
    cards: batch.cards.map((card) => ({
      ...card,
      acceptance_tests: [...card.acceptance_tests],
    })),
  };
}

export function cloneCardCycleState(state: CardCycleState): CardCycleState {
  return {
    ...state,
    current_batch: state.current_batch ? cloneCardBatch(state.current_batch) : null,
    call_alert: state.call_alert ? { ...state.call_alert } : null,
    recent_call_keys: [...state.recent_call_keys],
    failure: state.failure ? { ...state.failure } : null,
  };
}

/** Le contrat de sortie IA doit être complet avant toute proposition pending. */
export function cardContractError(cards: readonly EvolutionCard[]): string | null {
  if (!Array.isArray(cards)) return "Le lot de cartes est invalide.";
  if (cards.length !== CARD_BATCH_SIZE) return `Le lot doit contenir exactement ${CARD_BATCH_SIZE} cartes.`;
  const ids = new Set<string>();
  for (const card of cards) {
    if (!isEvolutionCard(card)) return "Une carte ne respecte pas le contrat de proposition.";
    if (ids.has(card.id)) return "Les identifiants de cartes doivent être distincts.";
    ids.add(card.id);
  }
  return null;
}
