import { Bot, CheckCircle2, Circle, Code2, LoaderCircle, TriangleAlert } from "lucide-react";
import type { AiActivity, EvolutionProgress, RecompileProgress } from "../protocol";
import { formatDuration } from "../lib/format";

const stages = [
  ["queued", "File"],
  ["preparing", "Préparation"],
  ["implementing", "TDD"],
  ["reporting", "Compte rendu"],
  ["verifying", "Vérification"],
  ["correcting", "Correction"],
  ["activating", "Activation"],
  ["complete", "Activée"],
] as const;

export function ProgressDock({ activity, evolution, recompilation }: {
  activity: AiActivity | null;
  evolution: EvolutionProgress | null;
  recompilation: RecompileProgress | null;
}) {
  if (!activity && !evolution && !recompilation) return null;
  const stageIndex = evolution ? stages.findIndex(([key]) => key === evolution.stage) : -1;
  const failed = evolution?.stage === "failed" || evolution?.stage === "timed_out";
  return (
    <div className="progress-dock" aria-live="polite">
      {activity && (
        <section className="activity-progress">
          <div className="progress-icon"><Bot size={19} /><span /></div>
          <div className="progress-copy">
            <span className="eyebrow">Appel IA {activity.call_number} / {activity.total_calls}</span>
            <strong>{activity.kind === "period_report" ? "Bilan de période" : "Demande d’évolution"} · {activity.agent_name}</strong>
            <div className="thin-progress"><span style={{ width: `${(activity.call_number / Math.max(1, activity.total_calls)) * 100}%` }} /></div>
          </div>
          <time>{formatDuration(activity.elapsed_ms)}</time>
        </section>
      )}
      {evolution && (
        <section className={`god-progress${failed ? " failed" : ""}`}>
          <header>
            <div><Code2 size={18} /><span><small>Workflow de Dieu</small><strong>{evolution.message || "Évolution en cours"}</strong></span></div>
            <time>{formatDuration(evolution.elapsed_seconds * 1_000)}</time>
          </header>
          <ol>
            {stages.map(([key, label], index) => {
              const active = index === stageIndex;
              const done = stageIndex > index || evolution.stage === "complete";
              return <li key={key} className={active ? "active" : done ? "done" : ""}>{done ? <CheckCircle2 /> : active ? <LoaderCircle className="spin" /> : <Circle />}<span>{label}</span></li>;
            })}
          </ol>
          {evolution.detail && <p>{failed && <TriangleAlert size={15} />}{evolution.detail}</p>}
        </section>
      )}
      {recompilation && (
        <section className={`recompile-progress ${recompilation.stage}`}>
          {recompilation.stage === "compiling" ? <LoaderCircle className="spin" /> : recompilation.stage === "ready" ? <CheckCircle2 /> : <TriangleAlert />}
          <div><span className="eyebrow">Transfert de version</span><strong>{recompilation.stage === "compiling" ? "Recompilation du moteur" : recompilation.stage === "ready" ? "Nouvelle version prête" : "Échec de recompilation"}</strong><p>{recompilation.detail}</p></div>
          <time>{formatDuration(recompilation.elapsed_ms)}</time>
        </section>
      )}
    </div>
  );
}
