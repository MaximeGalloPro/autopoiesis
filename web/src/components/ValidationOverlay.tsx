import {
  ArrowLeft,
  Check,
  Flame,
  OctagonX,
  ShieldCheck,
  Square,
  X,
} from "lucide-react";
import type { EngineCommand, EvolutionCompletion, ValidationPrompt } from "../protocol";
import { stringifyMechanism } from "../lib/format";
import { EvolutionRequestCard } from "./cards/EvolutionRequestCard";

/** Une résolution humaine ou un résultat terminal reprend sans second bouton. */
export function automaticContinuationKey(
  prompt: ValidationPrompt | null,
  completion: EvolutionCompletion | null,
): string | null {
  if (
    prompt
    && (prompt.stage === "empty" || prompt.stage === "complete")
    && prompt.allowed_commands.includes("o")
  ) return `validation:${prompt.kind}:${prompt.simulation_cycle}:${prompt.stage}`;

  if (
    completion
    && ["complete", "failed", "timed_out"].includes(completion.stage)
    && completion.allowed_commands.includes("o")
  ) return `completion:${completion.request_id}:${completion.stage}`;

  return null;
}

export function ValidationOverlay({ prompt, sendCommand, onClose }: {
  prompt: ValidationPrompt;
  sendCommand: (command: EngineCommand) => Promise<boolean>;
  onClose: () => void;
}) {
  const selected = prompt.requests.find((request) => request.request_id === prompt.selected_request_id);
  const isDevil = prompt.kind === "devil";
  const closeAfter = (command: EngineCommand) => {
    onClose();
    void sendCommand(command);
  };

  if (prompt.stage === "empty" || prompt.stage === "complete") return null;

  return (
    <section className={`campfire-validation-overlay${isDevil ? " devil" : ""}`} role="dialog" aria-labelledby="validation-title">
      <header className="validation-heading">
          <div className="validation-icon">{isDevil ? <Flame /> : <ShieldCheck />}</div>
          <div className="validation-heading-copy">
            <span className="eyebrow">Garde humaine · jour {prompt.day} · cycle {prompt.simulation_cycle}</span>
            <h2 id="validation-title">
              {prompt.stage === "choose" ? (isDevil ? "Contrainte à examiner" : "Évolution à examiner") : "Confirmer la décision"}
            </h2>
          </div>
          <button className="campfire-cards-close" onClick={onClose} aria-label="Fermer la validation">
            <X /><span>Fermer</span>
          </button>
      </header>

      {prompt.stage === "choose" && (
          <>
            {isDevil && (prompt.real_world_basis || prompt.future_pressure) && (
              <div className="devil-context">
                {prompt.real_world_basis && <p><span>Fondement réel</span>{prompt.real_world_basis}</p>}
                {prompt.future_pressure && <p><span>Pression future</span>{prompt.future_pressure}</p>}
              </div>
            )}
            <div className="request-grid">
              {prompt.requests.map((request) => (
                <EvolutionRequestCard
                  key={request.request_id}
                  request={request}
                  devil={isDevil || request.source === "devil"}
                  onSelect={() => void sendCommand({ type: "validation.select", request_id: request.request_id })}
                />
              ))}
            </div>
            <div className="modal-actions split">
              {!isDevil && (
                <button className="secondary-button" onClick={() => closeAfter({ type: "validation.none" })}>
                  <X size={17} /> Aucune évolution
                </button>
              )}
              <button className="danger-ghost" onClick={() => closeAfter({ type: "simulation.stop" })}>
                <Square size={15} /> Arrêter le run
              </button>
            </div>
          </>
      )}

      {prompt.stage === "confirm" && selected && (
          <div className="confirmation-layout">
            <article className="confirmation-card">
              <span className="request-source">{isDevil ? "Contrainte déterministe" : selected.agent_name ?? "Demande d’évolution"}</span>
              <h3>{selected.title}</h3>
              <dl>
                <div><dt>Besoin</dt><dd>{selected.need}</dd></div>
                <div><dt>Obstacle</dt><dd>{selected.obstacle}</dd></div>
                <div><dt>Changement proposé</dt><dd>{selected.proposed_change}</dd></div>
                <div><dt>Mécanisme</dt><dd>{stringifyMechanism(selected.mechanism)}</dd></div>
              </dl>
              {selected.acceptance_tests.length > 0 && (
                <div className="acceptance-tests"><span>Critères d’acceptation</span><ul>{selected.acceptance_tests.map((test) => <li key={test}>{test}</li>)}</ul></div>
              )}
              {isDevil && prompt.detail && <p className="devil-detail">{prompt.detail}</p>}
            </article>
            <aside className="decision-panel">
              <button className="approve-button" onClick={() => closeAfter({ type: "validation.decision", request_id: selected.request_id, decision: "approve" })}>
                <Check size={19} /> Approuver
              </button>
              <button className="reject-button" onClick={() => closeAfter({ type: "validation.decision", request_id: selected.request_id, decision: "reject" })}>
                <OctagonX size={19} /> Refuser
              </button>
              {!isDevil && (
                <button className="secondary-button" onClick={() => closeAfter({ type: "validation.back" })}>
                  <ArrowLeft size={17} /> Revenir aux cartes
                </button>
              )}
              <button className="danger-ghost" onClick={() => closeAfter({ type: "simulation.stop" })}>
                <Square size={15} /> Arrêter
              </button>
            </aside>
          </div>
      )}
    </section>
  );
}
