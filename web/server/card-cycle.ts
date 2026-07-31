import {
  CARD_BATCH_SIZE,
  type CardBatch,
  type CardCycleSnapshot,
  type CardCycleState,
  type CardDecision,
  type CardGenerationContext,
  type CardGenerationResult,
  type ExistingCardGenerator,
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

function boundedCooldown(value: number | undefined): number {
  if (!Number.isFinite(value) || value === undefined) return 0;
  return Math.max(0, Math.min(MAX_COOLDOWN_MS, Math.floor(value)));
}

function errorMessage(error: unknown): string {
  return error instanceof Error && error.message ? error.message : "La génération des cartes a échoué.";
}

function generatedBatch(id: string, createdAt: number, cards: readonly import("../src/card-cycle-protocol").EvolutionCard[]): CardBatch {
  return {
    id,
    status: "awaiting_validation",
    created_at_ms: createdAt,
    cards: cards.map((card) => ({
      ...card,
      acceptance_tests: [...card.acceptance_tests],
      status: "pending",
    })),
  };
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
    if (this.generationActive) return { kind: "blocked", reason: "generation_active" };
    if (this.state.call_alert && !this.state.call_alert.acknowledged) {
      return { kind: "blocked", reason: "call_alert" };
    }
    if (this.state.current_batch?.status === "awaiting_validation") {
      return { kind: "blocked", reason: "pending_cards" };
    }
    const now = this.now();
    if (now < this.state.cooldown_until_ms) {
      return { kind: "blocked", reason: "cooldown", retry_at_ms: this.state.cooldown_until_ms };
    }

    this.generationActive = true;
    try {
      const batchId = this.nextBatchId();
      const context: CardGenerationContext = {
        batch_id: batchId,
        call: (label, operation) => this.runExistingCall(label, operation),
      };
      const cards = await generator(context);
      const contractError = cardContractError(cards);
      if (contractError) return { kind: "failed", error: contractError };

      this.state.current_batch = generatedBatch(batchId, this.now(), cards);
      await this.persist();
      return { kind: "generated", batch: cloneCardBatch(this.state.current_batch) };
    } catch (error) {
      // Une erreur libère la garde ; elle ne relance jamais le générateur.
      return { kind: "failed", error: errorMessage(error) };
    } finally {
      this.generationActive = false;
    }
  }

  /** Enregistre une décision humaine sans l'interpréter ni modifier le monde. */
  async recordCardDecision(cardId: string, decision: CardDecision): Promise<boolean> {
    await this.loaded;
    if (decision !== "approve" && decision !== "reject") return false;
    const batch = this.state.current_batch;
    if (!batch || batch.status !== "awaiting_validation") return false;
    const card = batch.cards.find((candidate) => candidate.id === cardId);
    if (!card || card.status !== "pending") return false;

    card.status = decision === "approve" ? "approved" : "rejected";
    card.decided_at_ms = this.now();
    if (batch.cards.every((candidate) => candidate.status !== "pending")) {
      batch.status = "validated";
      batch.validated_at_ms = this.now();
      this.state.cooldown_until_ms = batch.validated_at_ms + this.cooldownMs;
    }
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
    const phase = this.generationActive ? "generating"
      : snapshot.call_alert && !snapshot.call_alert.acknowledged ? "call_alert"
      : snapshot.current_batch?.status === "awaiting_validation" ? "awaiting_validation"
      : this.now() < snapshot.cooldown_until_ms ? "cooldown"
      : "ready";
    return {
      ...snapshot,
      total_api_calls: snapshot.call_count,
      phase,
      active_call_count: this.activeCallCount,
    };
  }

  private async load(): Promise<void> {
    if (!this.store) return;
    const stored = await this.store.read();
    if (stored === null) return;
    const parsed = parseCardCycleState(stored);
    if (!parsed) throw new Error("État persistant du cycle de cartes invalide.");
    this.state = parsed;
  }

  private async persist(): Promise<void> {
    if (this.store) await this.store.write(cloneCardCycleState(this.state));
  }

  private async runExistingCall<T>(label: string, operation: () => Promise<T>): Promise<T> {
    // Une chaîne de promesses sérialise aussi un générateur qui lancerait
    // accidentellement plusieurs appels via Promise.all().
    const previous = this.callTail;
    let release: (() => void) | undefined;
    this.callTail = new Promise<void>((resolve) => {
      release = resolve;
    });
    await previous;
    this.activeCallCount += 1;
    this.state.call_count += 1;
    if (this.state.call_count % 10 === 0) {
      this.state.call_alert = { call_count: this.state.call_count, acknowledged: false };
    }
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
