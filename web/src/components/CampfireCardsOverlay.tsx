import { Flame } from "lucide-react";
import type { EvolutionRequest, ValidationPrompt } from "../protocol";
import { EvolutionRequestCard } from "./cards/EvolutionRequestCard";

export const CAMPFIRE_CARD_COUNT = 3 as const;

export type CampfireOverlayAction =
  | { type: "campfire_clicked"; has_alert: boolean }
  | { type: "card_chosen" }
  | { type: "cards_closed" };

export interface CampfireOverlayState {
  open: boolean;
}

/**
 * La présentation ne retient jamais une proposition partielle. La sélection
 * reste une commande de validation, transmise seulement par l'adaptateur web.
 */
export function campfireCardsFromPrompt(prompt: ValidationPrompt | null): EvolutionRequest[] {
  if (!prompt || prompt.kind !== "feature" || prompt.stage !== "choose") return [];
  if (prompt.requests.length !== CAMPFIRE_CARD_COUNT) return [];
  return prompt.requests.every((request) => request.status === "pending") ? prompt.requests : [];
}

/** Toute décision humaine ouverte est signalée au foyer, sans imposer d'écran. */
export function campfireNeedsAttention(prompt: ValidationPrompt | null): boolean {
  return prompt?.stage === "choose" || prompt?.stage === "confirm";
}

export function campfireOverlayReducer(
  state: CampfireOverlayState,
  action: CampfireOverlayAction,
): CampfireOverlayState {
  switch (action.type) {
    case "campfire_clicked": return action.has_alert ? { open: true } : state;
    case "card_chosen":
    case "cards_closed": return { open: false };
  }
}

export function CampfireCardsOverlay({
  requests,
  onChoose,
  hasCallMilestone = false,
  onAcknowledgeCallMilestone,
  onClose,
}: {
  requests: readonly EvolutionRequest[];
  onChoose: (requestId: string) => void;
  hasCallMilestone?: boolean;
  onAcknowledgeCallMilestone?: () => void;
  onClose?: () => void;
}) {
  if (requests.length !== CAMPFIRE_CARD_COUNT) return null;
  return (
    <section className="campfire-cards-overlay" role="dialog" aria-label="Choisir une proposition au feu de camp">
      <header className="campfire-cards-heading">
        <div><Flame aria-hidden="true" /><span><small>Feu de camp</small><strong>Choisir une proposition</strong></span></div>
        {onClose && <button type="button" className="campfire-cards-close" onClick={onClose}>Fermer</button>}
      </header>
      {hasCallMilestone && (
        <div className="campfire-call-alert" role="status">
          <span>Palier de dix appels atteint : une confirmation humaine est attendue.</span>
          {onAcknowledgeCallMilestone && <button type="button" onClick={onAcknowledgeCallMilestone}>J’ai pris connaissance</button>}
        </div>
      )}
      <div className="campfire-cards-grid">
        {requests.map((request) => (
          <div className="campfire-decision-card" key={request.request_id}>
            <EvolutionRequestCard request={request} devil={false} onSelect={() => onChoose(request.request_id)} />
          </div>
        ))}
      </div>
    </section>
  );
}

/** Alerte de garde au foyer, sans carte ni commande adressée au moteur. */
export function CampfireCallAlert({ onAcknowledge, onClose }: {
  onAcknowledge: () => void;
  onClose: () => void;
}) {
  return (
    <section className="campfire-cards-overlay campfire-call-only" role="dialog" aria-label="Alerte d’appels IA au feu de camp">
      <header className="campfire-cards-heading">
        <div><Flame aria-hidden="true" /><span><small>Feu de camp</small><strong>Palier d’appels atteint</strong></span></div>
        <button type="button" className="campfire-cards-close" onClick={onClose}>Fermer</button>
      </header>
      <p>Aucune règle du monde n’est modifiée. Confirmez simplement avoir pris connaissance du palier avant la prochaine demande de cartes.</p>
      <div className="campfire-call-actions"><button type="button" onClick={onAcknowledge}>J’ai pris connaissance</button></div>
    </section>
  );
}
