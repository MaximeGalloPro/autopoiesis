import { ArrowRight, Flame, ShieldCheck } from "lucide-react";
import type { EvolutionRequest } from "../../protocol";

export function EvolutionRequestCard({ request, devil, onSelect }: {
  request: EvolutionRequest;
  devil: boolean;
  onSelect: () => void;
}) {
  const isDevil = devil || request.source === "devil";
  const author = request.agent_name ?? "Personnification";

  return (
    <button className={`request-card${isDevil ? " devil" : ""}`} onClick={onSelect}>
      <span className="request-source">
        {isDevil ? <Flame size={14} /> : <ShieldCheck size={14} />}
        {isDevil ? "Contrainte du Diable" : `Nouvelle demande · ${author}`}
      </span>
      <h3>{request.title}</h3>
      <p>{request.need}</p>
      <dl className="request-facts">
        <div>
          <dt>Obstacle</dt>
          <dd>{request.obstacle}</dd>
        </div>
        {request.proposed_change && (
          <div>
            <dt>Changement proposé</dt>
            <dd>{request.proposed_change}</dd>
          </div>
        )}
      </dl>
      <span className="inspect-request">Examiner la proposition <ArrowRight aria-hidden="true" size={13} /></span>
    </button>
  );
}
