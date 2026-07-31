import type {
  BackendEvent,
  CardDecision,
  EngineCommand,
  EvolutionCard,
  EvolutionRequest,
  ValidationPrompt,
} from "../src/protocol";
import { BackendProcessManager } from "./backend-process";
import { CardCycleCoordinator } from "./card-cycle";

function activityKey(event: Extract<BackendEvent, { type: "activity" }>): string | null {
  const activity = event.payload;
  if (!activity) return null;
  return [
    activity.simulation_cycle ?? "stream",
    activity.kind,
    activity.agent_id,
    activity.call_number,
  ].join(":");
}

function mechanismSummary(request: EvolutionRequest): string {
  if (typeof request.mechanism === "string" && request.mechanism.trim()) return request.mechanism.trim();
  if (request.mechanism && typeof request.mechanism === "object") {
    const candidate = request.mechanism as Record<string, unknown>;
    for (const key of ["summary", "name"]) {
      if (typeof candidate[key] === "string" && candidate[key].trim()) return candidate[key].trim();
    }
  }
  return "Mécanisme déterministe proposé";
}

function cardFromEngineRequest(request: EvolutionRequest): EvolutionCard | null {
  const acceptanceTests = request.acceptance_tests.map((test) => test.trim()).filter(Boolean);
  if (!request.request_id.trim() || !request.title.trim() || !request.need.trim()
    || !request.obstacle.trim() || !request.proposed_change.trim() || acceptanceTests.length === 0) return null;
  return {
    id: request.request_id,
    title: request.title,
    need: request.need,
    obstacle: request.obstacle,
    proposed_change: request.proposed_change,
    mechanism: mechanismSummary(request),
    acceptance_tests: acceptanceTests,
  };
}

function featureBatch(prompt: ValidationPrompt): EvolutionCard[] | null {
  if (prompt.kind !== "feature" || prompt.stage !== "choose" || prompt.requests.length !== 3) return null;
  const cards = prompt.requests.map(cardFromEngineRequest);
  return cards.some((card) => card === null) ? null : cards as EvolutionCard[];
}

/**
 * Pont sans état de monde entre le transport C++ et la garde persistante des
 * cartes. Il observe le flux et enregistre les commandes déjà validées par les
 * deux barrières : route Elysia stricte, puis validateur C++.
 */
export class BackendCardCycleBridge {
  private tail: Promise<void> = Promise.resolve();
  private readonly unsubscribe: () => void;

  constructor(
    private readonly manager: BackendProcessManager,
    private readonly cycle: CardCycleCoordinator,
  ) {
    this.unsubscribe = manager.subscribe((event) => {
      // Cet événement est précisément la projection produite par ce pont ; le
      // réabsorber créerait une boucle de publications sans toucher au moteur.
      if (event.type === "card_cycle") return;
      void this.enqueue(async () => this.observe(event));
    });
    void this.enqueue(async () => this.publishSnapshot());
  }

  stop(): void {
    this.unsubscribe();
  }

  /** Point de synchronisation déterministe pour les tests et l'arrêt serveur. */
  async flush(): Promise<void> {
    await this.tail;
  }

  /** Publie la conséquence d'une route de garde sans attendre un événement C++. */
  async refresh(): Promise<void> {
    await this.enqueue(async () => this.publishSnapshot());
  }

  async recordAcceptedCommand(command: EngineCommand): Promise<void> {
    await this.enqueue(async () => {
      switch (command.type) {
        case "validation.select":
          await this.cycle.recordEngineSelection(command.request_id);
          break;
        case "validation.decision":
          await this.cycle.completeEngineDecision(command.request_id, command.decision as CardDecision);
          break;
        case "validation.back":
          await this.cycle.clearEngineSelection();
          break;
        case "validation.none":
          await this.cycle.completeEngineWithoutSelection();
          break;
        default:
          break;
      }
      await this.publishSnapshot();
    });
  }

  private enqueue(task: () => Promise<void>): Promise<void> {
    this.tail = this.tail.then(task, task).catch(() => {
      // Une corruption de l'état est renvoyée par les routes au prochain
      // accès. Le pont n'écrit ni commande de repli ni état de monde.
    });
    return this.tail;
  }

  private async observe(event: BackendEvent): Promise<void> {
    if (event.type === "activity") {
      const key = activityKey(event);
      if (key) await this.cycle.recordObservedCall(key);
    }
    if (event.type === "validation" && event.payload) {
      const cards = featureBatch(event.payload);
      if (cards) await this.cycle.ingestEngineBatch(`engine-${event.payload.simulation_cycle}`, cards);
      if (event.payload.kind === "feature" && event.payload.stage === "confirm"
        && event.payload.selected_request_id) {
        await this.cycle.recordEngineSelection(event.payload.selected_request_id);
      }
    }
    await this.publishSnapshot();
  }

  private async publishSnapshot(): Promise<void> {
    this.manager.setCardCycleSnapshot(await this.cycle.snapshot());
  }
}
