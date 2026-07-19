import { ArrowLeft, Check, Flame, OctagonX, PauseCircle, ShieldCheck, Square, X } from "lucide-react";
import type { EngineCommand, EvolutionRequest, ValidationPrompt } from "../protocol";
import { stringifyMechanism } from "../lib/format";

function RequestCard({ request, devil, onSelect }: {
  request: EvolutionRequest;
  devil: boolean;
  onSelect: () => void;
}) {
  return (
    <button className={`request-card${devil ? " devil" : ""}`} onClick={onSelect}>
      <span className="request-source">{devil ? <><Flame size={14} /> Contrainte du Diable</> : request.agent_name ?? "Personnification"}</span>
      <h3>{request.title}</h3>
      <p>{request.need}</p>
      <div className="request-obstacle"><span>Obstacle</span>{request.obstacle}</div>
      <span className="inspect-request">Examiner la proposition <span aria-hidden="true">→</span></span>
    </button>
  );
}

export function ValidationOverlay({ prompt, sendCommand }: {
  prompt: ValidationPrompt;
  sendCommand: (command: EngineCommand) => Promise<boolean>;
}) {
  const selected = prompt.requests.find((request) => request.request_id === prompt.selected_request_id);
  const isDevil = prompt.kind === "devil";

  return (
    <div className="modal-layer" role="dialog" aria-modal="true" aria-labelledby="validation-title">
      <section className={`validation-modal${isDevil ? " devil" : ""}`}>
        <header className="validation-heading">
          <div className="validation-icon">{isDevil ? <Flame /> : <ShieldCheck />}</div>
          <div>
            <span className="eyebrow">Garde humaine · jour {prompt.day} · cycle {prompt.simulation_cycle}</span>
            <h2 id="validation-title">
              {prompt.stage === "choose" ? (isDevil ? "Une pression réelle se présente" : "Le monde demande à évoluer") : "Confirmer votre décision"}
            </h2>
            <p>{isDevil
              ? "Cette contrainte locale n’entrera dans le monde qu’après votre décision explicite."
              : "Une seule demande peut être traitée dans cette fenêtre. Les autres resteront pending."}</p>
          </div>
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
                <RequestCard
                  key={request.request_id}
                  request={request}
                  devil={isDevil || request.source === "devil"}
                  onSelect={() => void sendCommand({ type: "validation.select", request_id: request.request_id })}
                />
              ))}
            </div>
            <div className="modal-actions split">
              {!isDevil && (
                <button className="secondary-button" onClick={() => void sendCommand({ type: "validation.none" })}>
                  <X size={17} /> Aucune évolution
                </button>
              )}
              <button className="danger-ghost" onClick={() => void sendCommand({ type: "simulation.stop" })}>
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
              <p>Le moteur reste en pause jusqu’à votre choix.</p>
              <button className="approve-button" onClick={() => void sendCommand({ type: "validation.decision", request_id: selected.request_id, decision: "approve" })}>
                <Check size={19} /> Approuver
                <small>Autoriser le workflow de Dieu</small>
              </button>
              <button className="reject-button" onClick={() => void sendCommand({ type: "validation.decision", request_id: selected.request_id, decision: "reject" })}>
                <OctagonX size={19} /> Refuser
                <small>Conserver le moteur actuel</small>
              </button>
              {!isDevil && (
                <button className="secondary-button" onClick={() => void sendCommand({ type: "validation.back" })}>
                  <ArrowLeft size={17} /> Revenir aux cartes
                </button>
              )}
              <button className="danger-ghost" onClick={() => void sendCommand({ type: "simulation.stop" })}>
                <Square size={15} /> Arrêter
              </button>
            </aside>
          </div>
        )}

        {prompt.stage === "complete" && (
          <div className="completion-choice">
            <PauseCircle size={42} />
            <h3>La fenêtre est traitée</h3>
            <p>Le moteur attend toujours une confirmation explicite avant de reprendre.</p>
            <div className="modal-actions">
              <button className="approve-button" onClick={() => void sendCommand({ type: "simulation.resume" })}><Check size={18} /> Reprendre</button>
              <button className="danger-ghost" onClick={() => void sendCommand({ type: "simulation.stop" })}><Square size={15} /> Arrêter</button>
            </div>
          </div>
        )}
      </section>
    </div>
  );
}
