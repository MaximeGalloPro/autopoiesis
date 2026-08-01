import { Bot, CheckCircle2, Code2, LoaderCircle, ShieldCheck, TriangleAlert } from "lucide-react";
import type {
  AiActivity,
  EvolutionCompletion,
  EvolutionProgress,
  RecompileProgress,
  ValidationPrompt,
} from "../protocol";
import { formatDuration } from "../lib/format";

const evolutionLabels: Record<EvolutionProgress["stage"], string> = {
  queued: "File",
  preparing: "Préparation",
  implementing: "TDD",
  reporting: "Bilan",
  verifying: "Vérification",
  correcting: "Correction",
  activating: "Activation",
  complete: "Activée",
  failed: "Échec",
  timed_out: "Délai dépassé",
};

const validationLabels: Record<ValidationPrompt["stage"], string> = {
  empty: "Traitée",
  choose: "À choisir",
  confirm: "À confirmer",
  complete: "Traitée",
};

function Status({
  label,
  value,
  icon,
  elapsed,
  failed = false,
}: {
  label: string;
  value: string;
  icon: React.ReactNode;
  elapsed?: number;
  failed?: boolean;
}) {
  return (
    <section className={`progress-status${failed ? " failed" : ""}`}>
      <div className="progress-status-icon">{icon}</div>
      <div className="progress-status-copy"><span>{label}</span><strong>{value}</strong></div>
      {elapsed !== undefined && <time>{formatDuration(elapsed)}</time>}
    </section>
  );
}

export function ProgressDock({ activity, validation, evolution, recompilation, completion }: {
  activity: AiActivity | null;
  validation: ValidationPrompt | null;
  evolution: EvolutionProgress | null;
  recompilation: RecompileProgress | null;
  completion: EvolutionCompletion | null;
}) {
  const evolutionFailed = evolution?.stage === "failed" || evolution?.stage === "timed_out";
  const completionFailed = completion && !completion.successful;
  if (!activity && !validation && !evolution && !recompilation && !completion) return null;

  return (
    <div className="progress-dock" aria-live="polite">
      {activity && <Status
        label="Génération"
        value={`${activity.call_number} / ${activity.total_calls}`}
        icon={<Bot />}
        elapsed={activity.elapsed_ms}
      />}
      {validation && <Status
        label="Validation"
        value={validationLabels[validation.stage]}
        icon={<ShieldCheck />}
      />}
      {evolution && <Status
        label={evolutionFailed ? "Échec" : "Évolution"}
        value={evolutionLabels[evolution.stage]}
        icon={evolutionFailed ? <TriangleAlert /> : <Code2 />}
        elapsed={evolution.elapsed_seconds * 1_000}
        failed={evolutionFailed}
      />}
      {recompilation && <Status
        label={recompilation.stage === "failed" ? "Échec" : "Compilation"}
        value={recompilation.stage === "compiling" ? "En cours" : recompilation.stage === "ready" ? "Prête" : "Échec"}
        icon={recompilation.stage === "compiling" ? <LoaderCircle className="spin" /> : recompilation.stage === "ready" ? <CheckCircle2 /> : <TriangleAlert />}
        elapsed={recompilation.elapsed_ms}
        failed={recompilation.stage === "failed"}
      />}
      {completionFailed && !evolutionFailed && recompilation?.stage !== "failed" && <Status
        label="Échec"
        value={completion.stage === "timed_out" ? "Délai dépassé" : "Non activée"}
        icon={<TriangleAlert />}
        elapsed={completion.elapsed_seconds * 1_000}
        failed
      />}
    </div>
  );
}
