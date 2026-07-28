import {
  ArrowLeft,
  Check,
  ChevronUp,
  Flame,
  Minimize2,
  OctagonX,
  PauseCircle,
  Play,
  ShieldCheck,
  Square,
  X,
} from "lucide-react";
import type { EngineCommand, EvolutionCompletion, EvolutionRequest, ValidationPrompt } from "../protocol";
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

export function ValidationReminder({ prompt, sendCommand, onOpen }: {
  prompt: ValidationPrompt;
  sendCommand: (command: EngineCommand) => Promise<boolean>;
  onOpen: () => void;
}) {
  const isDevil = prompt.kind === "devil";
  const canResume = (prompt.stage === "empty" || prompt.stage === "complete")
    && prompt.allowed_commands.includes("o");
  const title = prompt.stage === "choose"
    ? (isDevil ? "Contrainte à examiner" : "Évolution à examiner")
    : prompt.stage === "confirm"
      ? "Décision à confirmer"
      : "Reprise à confirmer";

  return (
    <section className={`guard-reminder${isDevil ? " devil" : ""}`} role="status" aria-label="Décision humaine en attente">
      <div className="guard-reminder-icon">{isDevil ? <Flame /> : <ShieldCheck />}</div>
      <div className="guard-reminder-copy">
        <span>Simulation en attente</span>
        <strong>{title}</strong>
        <small>Le monde reste entièrement consultable.</small>
      </div>
      <div className="guard-reminder-actions">
        {canResume && (
          <button className="guard-resume" onClick={() => void sendCommand({ type: "simulation.resume" })}>
            <Play /> Reprendre
          </button>
        )}
        <button className="guard-open" onClick={onOpen} aria-haspopup="dialog">
          <ChevronUp /> {canResume ? "Détails" : "Examiner"}
        </button>
      </div>
    </section>
  );
}

export function ValidationOverlay({ prompt, sendCommand, onMinimize }: {
  prompt: ValidationPrompt;
  sendCommand: (command: EngineCommand) => Promise<boolean>;
  onMinimize: () => void;
}) {
  const selected = prompt.requests.find((request) => request.request_id === prompt.selected_request_id);
  const isDevil = prompt.kind === "devil";

  return (
    <div className="guard-layer">
      <section className={`validation-modal${isDevil ? " devil" : ""}`} role="dialog" aria-labelledby="validation-title">
        <header className="validation-heading">
          <div className="validation-icon">{isDevil ? <Flame /> : <ShieldCheck />}</div>
          <div className="validation-heading-copy">
            <span className="eyebrow">Garde humaine · jour {prompt.day} · cycle {prompt.simulation_cycle}</span>
            <h2 id="validation-title">
              {prompt.stage === "choose" ? (isDevil ? "Une pression réelle se présente" : "Le monde demande à évoluer") : "Confirmer votre décision"}
            </h2>
            <p>{isDevil
              ? "Cette contrainte locale n’entrera dans le monde qu’après votre décision explicite."
              : "Une seule demande peut être traitée dans cette fenêtre. Les autres resteront pending."}</p>
          </div>
          <button className="guard-minimize" onClick={onMinimize} aria-label="Réduire la fenêtre de décision">
            <Minimize2 /><span>Observer le monde</span>
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

        {(prompt.stage === "complete" || prompt.stage === "empty") && (
          <div className="completion-choice">
            <PauseCircle size={42} />
            <h3>{prompt.stage === "empty" ? "Aucune évolution disponible" : "La fenêtre est traitée"}</h3>
            <p>{prompt.stage === "empty"
              ? "La simulation reste sous garde humaine et peut reprendre sans modifier le monde."
              : "Le moteur attend toujours une confirmation explicite avant de reprendre."}</p>
            <div className="modal-actions">
              {prompt.allowed_commands.includes("o") && (
                <button className="approve-button" onClick={() => void sendCommand({ type: "simulation.resume" })}><Check size={18} /> Reprendre</button>
              )}
              <button className="danger-ghost" onClick={() => void sendCommand({ type: "simulation.stop" })}><Square size={15} /> Arrêter</button>
            </div>
          </div>
        )}
      </section>
    </div>
  );
}

export function EvolutionCompletionReminder({ completion, sendCommand, onOpen }: {
  completion: EvolutionCompletion;
  sendCommand: (command: EngineCommand) => Promise<boolean>;
  onOpen: () => void;
}) {
  const successful = completion.stage === "complete" && completion.successful;
  const timedOut = completion.stage === "timed_out";
  const canResume = (successful || timedOut) && completion.allowed_commands.includes("o");
  const title = successful
    ? "Nouvelle évolution prête"
    : timedOut
      ? "Suivi de l’évolution expiré"
      : "Évolution non activée";
  return (
    <section className="guard-reminder" role="status" aria-label="Confirmation de transfert en attente">
      <div className="guard-reminder-icon">{successful ? <Check /> : <OctagonX />}</div>
      <div className="guard-reminder-copy">
        <span>Simulation en attente</span>
        <strong>{title}</strong>
        <small>{timedOut ? "La partie reste sur la version actuelle." : "Le monde reste entièrement consultable."}</small>
      </div>
      <div className="guard-reminder-actions">
        {canResume && (
          <button className="guard-resume" onClick={() => void sendCommand({ type: "simulation.resume" })}>
            <Play /> {successful ? "Recompiler et reprendre" : "Reprendre la partie"}
          </button>
        )}
        <button className="guard-open" onClick={onOpen} aria-haspopup="dialog"><ChevronUp /> Détails</button>
      </div>
    </section>
  );
}

export function EvolutionCompletionOverlay({ completion, sendCommand, onMinimize }: {
  completion: EvolutionCompletion;
  sendCommand: (command: EngineCommand) => Promise<boolean>;
  onMinimize: () => void;
}) {
  const successful = completion.stage === "complete" && completion.successful;
  const timedOut = completion.stage === "timed_out";
  const title = successful
    ? "La nouvelle évolution est prête"
    : timedOut
      ? "L’évolution n’a pas été activée"
      : "L’évolution a échoué";
  const message = successful
    ? completion.message
    : timedOut
      ? "Le suivi a expiré avant la fin du traitement."
      : completion.message;
  const detail = successful
    ? completion.detail || "Une nouvelle version a été vérifiée et attend votre confirmation."
    : timedOut
      ? "Aucun changement n’est actif. Vous pouvez reprendre la partie avec la version actuelle."
      : completion.detail || "La partie reste sur la version actuelle. Aucun changement n’est actif.";
  return (
    <div className="guard-layer">
      <section className="validation-modal" role="dialog" aria-labelledby="evolution-completion-title">
        <header className="validation-heading">
          <div className="validation-icon">{successful ? <Check /> : <OctagonX />}</div>
          <div className="validation-heading-copy">
            <span className="eyebrow">Garde humaine · résultat de l’évolution</span>
            <h2 id="evolution-completion-title">{title}</h2>
            <p>{message}</p>
          </div>
          <button className="guard-minimize" onClick={onMinimize} aria-label="Réduire la fenêtre de décision">
            <Minimize2 /><span>Observer le monde</span>
          </button>
        </header>
        <div className="completion-choice">
          <PauseCircle size={42} />
          <h3>{successful ? "Que voulez-vous faire ?" : "La partie peut-elle reprendre ?"}</h3>
          <p>{detail}</p>
          <div className="modal-actions">
            {successful && completion.allowed_commands.includes("o") && (
              <button className="approve-button" onClick={() => void sendCommand({ type: "simulation.resume" })}>
                <Check size={18} /> Recompiler et reprendre
              </button>
            )}
            {timedOut && completion.allowed_commands.includes("o") && (
              <button className="approve-button" onClick={() => void sendCommand({ type: "simulation.resume" })}>
                <Play size={18} /> Reprendre la partie
              </button>
            )}
            <button className="danger-ghost" onClick={() => void sendCommand({ type: "simulation.stop" })}>
              <Square size={15} /> Arrêter la partie
            </button>
          </div>
        </div>
      </section>
    </div>
  );
}
