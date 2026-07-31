/**
 * Contrat entre l'orchestrateur serveur et la présentation des cartes.
 *
 * Les cartes restent des propositions : ce protocole ne porte aucune commande
 * du monde et ne constitue jamais une autorisation d'activation.
 */
export const CARD_BATCH_SIZE = 3 as const;
export const CARD_CYCLE_PROTOCOL_VERSION = 1 as const;

export type CardDecision = "approve" | "reject";
export type CardStatus = "pending" | "approved" | "rejected";
export type CardBatchStatus = "awaiting_validation" | "validated";
export type CardCyclePhase = "ready" | "generating" | "awaiting_validation" | "cooldown" | "call_alert";

/** Commandes humaines bornées, traitées par l'orchestrateur et non par l'IA. */
export type CardCycleCommand =
  | { type: "acknowledge_call_alert" }
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
  cards: PendingEvolutionCard[];
}

export interface ApiCallAlert {
  call_count: number;
  acknowledged: boolean;
}

/** État sérialisable : il doit survivre au redémarrage du processus serveur. */
export interface CardCycleState {
  protocol_version: typeof CARD_CYCLE_PROTOCOL_VERSION;
  call_count: number;
  current_batch: CardBatch | null;
  cooldown_until_ms: number;
  call_alert: ApiCallAlert | null;
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
    reason: "generation_active" | "pending_cards" | "cooldown" | "call_alert";
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
  return value === "pending" || value === "approved" || value === "rejected";
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

function isCardBatch(value: unknown): value is CardBatch {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const batch = value as Record<string, unknown>;
  if (!(isNonEmptyString(batch.id)
    && (batch.status === "awaiting_validation" || batch.status === "validated")
    && Number.isFinite(batch.created_at_ms)
    && (batch.validated_at_ms === undefined || Number.isFinite(batch.validated_at_ms))
    && Array.isArray(batch.cards)
    && batch.cards.length === CARD_BATCH_SIZE
    && batch.cards.every(isPendingCard))) return false;
  const cards = batch.cards as PendingEvolutionCard[];
  const ids = new Set(cards.map((card) => card.id));
  if (ids.size !== CARD_BATCH_SIZE) return false;
  if (batch.status === "validated") {
    return Number.isFinite(batch.validated_at_ms) && cards.every((card) => card.status !== "pending");
  }
  if (batch.validated_at_ms !== undefined) return false;
  return cards.some((card) => card.status === "pending");
}

/** Rejette les états non fiables plutôt que de réouvrir une génération. */
export function parseCardCycleState(value: unknown): CardCycleState | null {
  if (!value || typeof value !== "object" || Array.isArray(value)) return null;
  const state = value as Record<string, unknown>;
  if (state.protocol_version !== CARD_CYCLE_PROTOCOL_VERSION
    || !Number.isSafeInteger(state.call_count) || Number(state.call_count) < 0
    || !Number.isFinite(state.cooldown_until_ms)
    || !(state.current_batch === null || isCardBatch(state.current_batch))) return null;
  if (state.call_alert !== null) {
    if (!state.call_alert || typeof state.call_alert !== "object" || Array.isArray(state.call_alert)) return null;
    const alert = state.call_alert as Record<string, unknown>;
    if (!Number.isSafeInteger(alert.call_count) || Number(alert.call_count) <= 0
      || Number(alert.call_count) > Number(state.call_count) || Number(alert.call_count) % 10 !== 0
      || typeof alert.acknowledged !== "boolean") return null;
  }
  return cloneCardCycleState(state as unknown as CardCycleState);
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
