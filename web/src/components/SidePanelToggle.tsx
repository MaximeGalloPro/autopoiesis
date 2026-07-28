import { PanelRightClose, PanelRightOpen } from "lucide-react";

export function SidePanelToggle({
  expanded,
  onToggle,
  panelId,
}: {
  expanded: boolean;
  onToggle: () => void;
  panelId: string;
}) {
  const label = expanded ? "Replier le panneau d’observation" : "Déplier le panneau d’observation";

  return (
    <button
      className="panel-toggle"
      type="button"
      aria-label={label}
      aria-controls={panelId}
      aria-expanded={expanded}
      onClick={onToggle}
    >
      {expanded ? <PanelRightClose /> : <PanelRightOpen />}
      <span>{expanded ? "Réduire" : "Ouvrir"}</span>
    </button>
  );
}
