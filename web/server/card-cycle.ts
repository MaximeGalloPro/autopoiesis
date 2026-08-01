import {
  CARD_BATCH_SIZE,
  CARD_CYCLE_PROTOCOL_VERSION,
  type CardBatch,
  type CardCycleFailure,
  type CardCycleSnapshot,
  type CardCycleState,
  type CardDecision,
  type CardGenerationContext,
  type CardGenerationResult,
  type ExistingCardGenerator,
  type EvolutionCard,
  cardContractError,
  cloneCardBatch,
  cloneCardCycleState,
  initialCardCycleState,
  parseCardCycleState,
} from "../src/card-cycle-protocol";
import { randomUUID } from "node:crypto";
import { mkdir, readFile, rename, writeFile } from "node:fs/promises";
import { dirname } from "node:path";

export interface CardCycleStore {
  read(): Promise<unknown | null>;
  write(state: CardCycleState): Promise<void>;
}

/** Stockage de test et fallback explicite pour les processus sans persistance. */
export class MemoryCardCycleStore implements CardCycleStore {
  private value: CardCycleState | null = null;

  async read(): Promise<unknown | null> {
    return this.value ? cloneCardCycleState(this.value) : null;
  }

  async write(state: CardCycleState): Promise<void> {
    this.value = cloneCardCycleState(state);
  }
}

/**
 * Persistance atomique du garde-fou côté serveur. Le fichier ne contient ni
 * prompt, ni réponse IA brute, ni secret : seulement le contrat de cartes.
 */
export class JsonFileCardCycleStore implements CardCycleStore {
  constructor(private readonly path: string) {}

  async read(): Promise<unknown | null> {
    try {
      return JSON.parse(await readFile(this.path, "utf8")) as unknown;
    } catch (error) {
      if (error && typeof error === "object" && "code" in error && error.code === "ENOENT") return null;
      throw error;
    }
  }

  async write(state: CardCycleState): Promise<void> {
    await mkdir(dirname(this.path), { recursive: true });
    const temporary = `${this.path}.${randomUUID()}.tmp`;
    await writeFile(temporary, `${JSON.stringify(state)}\n`, { encoding: "utf8", mode: 0o600 });
    await rename(temporary, this.path);
  }
}

export interface CardCycleOptions {
  cooldownMs?: number;
  now?: () => number;
  store?: CardCycleStore;
  batchId?: () => string;
}

const MAX_COOLDOWN_MS = 86_400_000;
const MAX_REMEMBERED_CALL_KEYS = 96;
const MAX_FAILURE_MESSAGE_LENGTH = 1_024;

function boundedCooldown(value: number | undefined): number {
  if (!Number.isFinite(value) || value === undefined) return 0;
  return Math.max(0, Math.min(MAX_COOLDOWN_MS, Math.floor(value)));
}

function errorMessage(error: unknown): string {
  const message = error instanceof Error && error.message ? error.message : "La génération des cartes a échoué.";
  return message.trim().slice(0, MAX_FAILURE_MESSAGE_LENGTH) || "La génération des cartes a échoué.";
}

function generatedBatch(id: string, createdAt: number, cards: readonly EvolutionCard[]): CardBatch {
  return {
    id,
    status: "offered",
    created_at_ms: createdAt,
    cards: cards.map((card) => ({
      ...card,
      acceptance_tests: [...card.acceptance_tests],
      status: "pending",
    })),
  };
}

function activeBatchBlocksGeneration(batch: CardBatch | null): boolean {
  if (!batch) return false;
  return batch.status === "offered" || batch.status === "choosing" || batch.status === "validating"
    || batch.status === "activating" || batch.status === "failed";
}

/**
 * Orchestrateur sans état du monde : il encapsule les appels existants mais ne
 * connaît ni règles de simulation, ni décision à appliquer au moteur.
 */
export class CardCycleCoordinator {
  private readonly cooldownMs: number;
  private readonly now: () => number;
  private readonly store?: CardCycleStore;
  private readonly nextBatchId: () => string;
  private state = initialCardCycleState();
  private readonly loaded: Promise<void>;
  private generationActive = false;
  private activeCallCount = 0;
  private callTail: Promise<void> = Promise.resolve();
  private persistenceTail: Promise<void> = Promise.resolve();

  constructor(options: CardCycleOptions = {}) {
    this.cooldownMs = boundedCooldown(options.cooldownMs);
    this.now = options.now ?? Date.now;
    this.store = options.store;
    let sequence = 0;
    this.nextBatchId = options.batchId ?? (() => `cards-${this.now()}-${++sequence}`);
    this.loaded = this.load();
  }

  /** À appeler par le tick du serveur : aucune planification ni retry n'est caché ici. */
  async tick(generator: ExistingCardGenerator): Promise<CardGenerationResult> {
    return this.requestNextBatch(generator);
  }

  async requestNextBatch(generator: ExistingCardGenerator): Promise<CardGenerationResult> {
    await this.loaded;
    const blocked = this.generationBlock();
    if (blocked) return blocked;

    const batchId = this.nextBatchId();
    this.generationActive = true;
    this.state.stage = "generating";
    this.state.generation_batch_id = batchId;
    this.state.failure = null;
    try {
      // Le verrou est durable avant le premier appel : un redémarrage ne peut
      // pas rejouer aveuglément une requête dont la réponse est inconnue.
      await this.persist();
      const context: CardGenerationContext = {
        batch_id: batchId,
        call: (label, operation) => this.runExistingCall(label, operation),
      };
      const cards = await generator(context);
      const contractError = cardContractError(cards);
      if (contractError) return this.failGeneration(batchId, contractError);

      this.state.current_batch = generatedBatch(batchId, this.now(), cards);
      this.state.stage = "offered";
      this.state.generation_batch_id = null;
      await this.persist();
      return { kind: "generated", batch: cloneCardBatch(this.state.current_batch) };
    } catch (error) {
      return this.failGeneration(batchId, errorMessage(error));
    } finally {
      this.generationActive = false;
    }
  }

  /**
   * Raccorde un lot déjà produit par le moteur C++. Cette observation ne crée
   * aucun appel et ne transmet aucune décision au monde. Les mêmes gardes
   * empêchent qu'un flux hors délai remplace un lot encore actif.
   */
  async ingestEngineBatch(batchId: string, cards: readonly EvolutionCard[]): Promise<CardGenerationResult> {
    await this.loaded;
    if (!batchId.trim()) return { kind: "failed", error: "Identifiant de lot moteur invalide." };
    const current = this.state.current_batch;
    if (current) {
      if (current.id === batchId && activeBatchBlocksGeneration(current)) {
        return { kind: "generated", batch: cloneCardBatch(current) };
      }
      if (activeBatchBlocksGeneration(current)) return { kind: "blocked", reason: "pending_cards" };
    }
    if (this.state.stage === "failed") return { kind: "blocked", reason: "failed" };
    const contractError = cardContractError(cards);
    if (contractError) return this.failGeneration(batchId, contractError);

    this.state.current_batch = generatedBatch(batchId, this.now(), cards);
    this.state.stage = "offered";
    this.state.generation_batch_id = null;
    this.state.failure = null;
    await this.persist();
    return { kind: "generated", batch: cloneCardBatch(this.state.current_batch) };
  }

  /** Déduplique les activités moteur afin que le compteur survive à un restart. */
  async recordObservedCall(key: string): Promise<boolean> {
    await this.loaded;
    if (!key.trim() || key.length > 256 || this.state.recent_call_keys.includes(key)) return false;
    this.state.recent_call_keys = [...this.state.recent_call_keys, key].slice(-MAX_REMEMBERED_CALL_KEYS);
    this.incrementCallCount();
    await this.persist();
    return true;
  }

  /** Conserve le choix affiché, sans le confondre avec une décision du monde. */
  async recordEngineSelection(cardId: string): Promise<boolean> {
    await this.loaded;
    const batch = this.state.current_batch;
    const card = batch?.cards.find((candidate) => candidate.id === cardId);
    if (!batch || !card) return false;
    if (batch.status === "choosing" && batch.selected_card_id === cardId) return true;
    if (batch.status !== "offered" || card.status !== "pending") return false;
    batch.status = "choosing";
    batch.selected_card_id = cardId;
    this.state.stage = "choosing";
    await this.persist();
    return true;
  }

  async clearEngineSelection(): Promise<boolean> {
    await this.loaded;
    const batch = this.state.current_batch;
    if (!batch || batch.status !== "choosing" || batch.selected_card_id === undefined) return false;
    batch.status = "offered";
    delete batch.selected_card_id;
    this.state.stage = "offered";
    await this.persist();
    return true;
  }

  /**
   * Le serveur note une décision déjà transmise au validateur C++, mais ne
   * touche jamais au monde. Une approbation demeure bloquée en validation
   * jusqu'au signal d'activation observé sur le flux du moteur.
   */
  async recordEngineDecision(cardId: string, decision: CardDecision): Promise<boolean> {
    await this.loaded;
    if (decision !== "approve" && decision !== "reject") return false;
    const batch = this.state.current_batch;
    const card = batch?.cards.find((candidate) => candidate.id === cardId);
    if (!batch || batch.status !== "choosing" || !card || card.status !== "pending"
      || batch.selected_card_id !== cardId) return false;
    card.status = decision === "approve" ? "approved" : "rejected";
    card.decided_at_ms = this.now();
    batch.status = "validating";
    batch.resolution = "engine_decision";
    batch.validated_at_ms = this.now();
    this.state.stage = "validating";
    await this.persist();
    return true;
  }

  /** Termine une validation qui n'a pas d'activation, notamment un refus. */
  async completeValidationWithoutActivation(): Promise<boolean> {
    await this.loaded;
    const batch = this.state.current_batch;
    const selected = batch?.cards.find((candidate) => candidate.id === batch.selected_card_id);
    if (!batch || batch.status !== "validating" || batch.resolution !== "engine_decision"
      || !selected || selected.status !== "rejected") return false;
    batch.status = "validated";
    this.enterCooldown();
    await this.persist();
    return true;
  }

  /** Compatibilité avec le pont existant : un refus clôt immédiatement sa validation. */
  async completeEngineDecision(cardId: string, decision: CardDecision): Promise<boolean> {
    const recorded = await this.recordEngineDecision(cardId, decision);
    if (!recorded || decision === "approve") return recorded;
    return this.completeValidationWithoutActivation();
  }

  /** Une fenêtre explicitement ignorée est traitée sans changer les demandes pending. */
  async completeEngineWithoutSelection(): Promise<boolean> {
    await this.loaded;
    const batch = this.state.current_batch;
    if (!batch || batch.status !== "offered" || batch.selected_card_id !== undefined) return false;
    batch.status = "validated";
    batch.resolution = "engine_none";
    batch.validated_at_ms = this.now();
    this.enterCooldown();
    await this.persist();
    return true;
  }

  /** Passe d'une proposition validée au suivi de son activation externe. */
  async beginActivation(cardId: string): Promise<boolean> {
    await this.loaded;
    const batch = this.state.current_batch;
    const selected = batch?.cards.find((candidate) => candidate.id === cardId);
    if (!batch || !selected || batch.selected_card_id !== cardId || selected.status !== "approved") return false;
    if (batch.status === "activating") return true;
    if (batch.status !== "validating" || batch.resolution !== "engine_decision") return false;
    batch.status = "activating";
    batch.activation_started_at_ms = this.now();
    this.state.stage = "activating";
    await this.persist();
    return true;
  }

  /**
   * Termine l'observation de l'activation. Cette transition ne démarre ni ne
   * paie aucune activation : elle ne fait que mémoriser le verdict émis ailleurs.
   */
  async completeActivation(cardId: string, successful: boolean, message?: string): Promise<boolean> {
    await this.loaded;
    const batch = this.state.current_batch;
    const selected = batch?.cards.find((candidate) => candidate.id === cardId);
    if (!batch || !selected || batch.selected_card_id !== cardId || selected.status !== "approved") return false;
    if (batch.status === "validating") {
      batch.status = "activating";
      batch.activation_started_at_ms = this.now();
    }
    if (batch.status !== "activating") return false;
    if (successful) {
      selected.status = "activated";
      batch.status = "activated";
      batch.activated_at_ms = this.now();
      this.state.failure = null;
      this.enterCooldown();
    } else {
      batch.status = "failed";
      batch.failed_at_ms = this.now();
      this.state.stage = "failed";
      this.state.generation_batch_id = null;
      this.state.failure = this.failure("activation", message ?? "L’activation de la carte a échoué.", batch.id);
    }
    await this.persist();
    return true;
  }

  /**
   * Endpoint de compatibilité pour une décision purement serveur. Il n'envoie
   * aucune commande au moteur ; une approbation reste donc en validation.
   */
  async recordCardDecision(cardId: string, decision: CardDecision): Promise<boolean> {
    await this.loaded;
    const batch = this.state.current_batch;
    if (!batch) return false;
    if (batch.status === "offered") {
      const selected = await this.recordEngineSelection(cardId);
      if (!selected) return false;
    }
    return this.completeEngineDecision(cardId, decision);
  }

  /** L'acquittement humain d'une erreur est le seul moyen de libérer un retry. */
  async acknowledgeFailure(): Promise<boolean> {
    await this.loaded;
    if (this.state.stage !== "failed" || !this.state.failure) return false;
    const batch = this.state.current_batch;
    if (batch?.status === "failed") {
      batch.status = "validated";
      batch.resolution = "failure_acknowledged";
      batch.validated_at_ms ??= this.now();
    }
    this.state.failure = null;
    this.state.generation_batch_id = null;
    this.enterCooldown();
    await this.persist();
    return true;
  }

  async acknowledgeCallAlert(): Promise<boolean> {
    await this.loaded;
    if (!this.state.call_alert || this.state.call_alert.acknowledged) return false;
    this.state.call_alert.acknowledged = true;
    await this.persist();
    return true;
  }

  async snapshot(): Promise<CardCycleSnapshot> {
    await this.loaded;
    const snapshot = cloneCardCycleState(this.state);
    const phase = snapshot.stage === "failed" ? "failed"
      : snapshot.call_alert && !snapshot.call_alert.acknowledged ? "call_alert"
      : snapshot.stage === "ready" && this.now() < snapshot.cooldown_until_ms ? "cooldown"
      : snapshot.stage;
    return {
      ...snapshot,
      total_api_calls: snapshot.call_count,
      phase,
      active_call_count: this.activeCallCount,
    };
  }

  private generationBlock(): Extract<CardGenerationResult, { kind: "blocked" }> | null {
    if (this.generationActive || this.state.stage === "generating") {
      return { kind: "blocked", reason: "generation_active" };
    }
    if (this.state.stage === "failed") return { kind: "blocked", reason: "failed" };
    if (this.state.call_alert && !this.state.call_alert.acknowledged) {
      return { kind: "blocked", reason: "call_alert" };
    }
    if (activeBatchBlocksGeneration(this.state.current_batch)) return { kind: "blocked", reason: "pending_cards" };
    if (this.now() < this.state.cooldown_until_ms) {
      return { kind: "blocked", reason: "cooldown", retry_at_ms: this.state.cooldown_until_ms };
    }
    return null;
  }

  private enterCooldown(): void {
    this.state.stage = "ready";
    this.state.generation_batch_id = null;
    this.state.cooldown_until_ms = this.now() + this.cooldownMs;
  }

  private async failGeneration(batchId: string, message: string): Promise<CardGenerationResult> {
    this.state.stage = "failed";
    this.state.generation_batch_id = null;
    this.state.failure = this.failure("generation", message, batchId);
    await this.persist();
    return { kind: "failed", error: this.state.failure.message };
  }

  private failure(phase: CardCycleFailure["phase"], message: string, batchId?: string): CardCycleFailure {
    return {
      phase,
      message: message.trim().slice(0, MAX_FAILURE_MESSAGE_LENGTH) || "Le cycle de cartes a échoué.",
      occurred_at_ms: this.now(),
      ...(batchId ? { batch_id: batchId } : {}),
    };
  }

  private incrementCallCount(): void {
    this.state.call_count += 1;
    if (this.state.call_count % 10 === 0) {
      this.state.call_alert = { call_count: this.state.call_count, acknowledged: false };
    }
  }

  private async load(): Promise<void> {
    if (!this.store) return;
    const stored = await this.store.read();
    if (stored === null) return;
    const parsed = parseCardCycleState(stored);
    if (!parsed) throw new Error("État persistant du cycle de cartes invalide.");
    this.state = parsed;
    // Un appel ne survit pas au processus. Le marquer en échec évite le replay
    // automatique d'une génération possiblement reçue par le fournisseur.
    if (this.state.stage === "generating") {
      const batchId = this.state.generation_batch_id ?? undefined;
      this.state.stage = "failed";
      this.state.generation_batch_id = null;
      this.state.failure = this.failure("generation", "La génération a été interrompue par un redémarrage.", batchId);
      await this.persist();
      return;
    }
    if (stored && typeof stored === "object" && !Array.isArray(stored)
      && (stored as Record<string, unknown>).protocol_version !== CARD_CYCLE_PROTOCOL_VERSION) {
      await this.persist();
    }
  }

  /**
   * Les écritures sont ordonnées et chaque snapshot est cloné au moment de
   * l'écriture. Deux appels observés simultanément ne peuvent donc pas faire
   * régresser le compteur persistant.
   */
  private async persist(): Promise<void> {
    if (!this.store) return;
    const previous = this.persistenceTail;
    let release: (() => void) | undefined;
    this.persistenceTail = new Promise<void>((resolve) => { release = resolve; });
    await previous;
    try {
      await this.store.write(cloneCardCycleState(this.state));
    } finally {
      release?.();
    }
  }

  private async runExistingCall<T>(label: string, operation: () => Promise<T>): Promise<T> {
    // Une chaîne de promesses sérialise aussi un générateur qui lancerait
    // accidentellement plusieurs appels via Promise.all().
    const previous = this.callTail;
    let release: (() => void) | undefined;
    this.callTail = new Promise<void>((resolve) => { release = resolve; });
    await previous;
    this.activeCallCount += 1;
    this.incrementCallCount();
    try {
      await this.persist();
      return await operation();
    } finally {
      this.activeCallCount -= 1;
      release?.();
      // Le libellé est volontairement reçu pour permettre au serveur appelant
      // de tracer l'opération sans enregistrer de payload ou de secret.
      void label;
    }
  }
}

/** Variable serveur exprimée en millisecondes, bornée à une journée. */
export function cardRequestCooldownMsFromEnvironment(environment: Record<string, string | undefined>): number {
  const raw = environment.CARD_REQUEST_COOLDOWN ?? environment.CARD_REQUEST_COOLDOWN_MS;
  if (!raw || !/^\d+$/.test(raw)) return 0;
  return boundedCooldown(Number(raw));
}

export { CARD_BATCH_SIZE };
