import { Circle, Sparkles, TriangleAlert, Users } from "lucide-react";

type EventPresentation = {
  label: string;
  tone: "request" | "support" | "warning" | "neutral";
  Icon: typeof Circle;
};

function presentationFor(event: string): EventPresentation {
  const normalized = event
    .toLocaleLowerCase("fr-FR")
    .normalize("NFD")
    .replace(/\p{Diacritic}/gu, "");
  if (normalized.startsWith("demande a dieu a valider")) {
    return { label: "Nouvelle demande", tone: "request", Icon: Sparkles };
  }
  if (normalized.startsWith("insistance ia enregistree")) {
    return { label: "Soutien enregistré", tone: "support", Icon: Users };
  }
  if (normalized.startsWith("insistance ia ignoree")) {
    return { label: "Soutien non enregistré", tone: "warning", Icon: TriangleAlert };
  }
  if (normalized.startsWith("alerte") || normalized.includes("erreur") || normalized.includes("invalide")) {
    return { label: "Alerte du moteur", tone: "warning", Icon: TriangleAlert };
  }
  return { label: "Événement récent", tone: "neutral", Icon: Circle };
}

export function RecentEventCard({ event }: { event: string }) {
  const { label, tone, Icon } = presentationFor(event);
  return (
    <li className={`recent-event-card ${tone}`}>
      <Icon aria-hidden="true" />
      <div>
        <span>{label}</span>
        <p>{event}</p>
      </div>
    </li>
  );
}
