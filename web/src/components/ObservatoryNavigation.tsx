import { BookOpenText, Flame, Globe2, Map, Users } from "lucide-react";

export const observatoryViews = ["characters", "camp", "history", "maps", "world"] as const;

export type ObservatoryView = (typeof observatoryViews)[number];

const navigationItems: Array<{
  id: ObservatoryView;
  label: string;
  Icon: typeof Users;
}> = [
  { id: "characters", label: "Personnages", Icon: Users },
  { id: "camp", label: "Foyer", Icon: Flame },
  { id: "history", label: "Histoire", Icon: BookOpenText },
  { id: "maps", label: "Cartes", Icon: Map },
  { id: "world", label: "Monde", Icon: Globe2 },
];

export function observatoryViewLabel(view: ObservatoryView) {
  return navigationItems.find((item) => item.id === view)?.label ?? "Observatoire";
}

export function ObservatoryNavigation({
  activeView,
  onChange,
}: {
  activeView: ObservatoryView;
  onChange: (view: ObservatoryView) => void;
}) {
  return (
    <nav className="observatory-navigation" aria-label="Sections de l’observatoire">
      {navigationItems.map(({ id, label, Icon }) => (
        <button
          key={id}
          type="button"
          className={activeView === id ? "active" : ""}
          aria-current={activeView === id ? "page" : undefined}
          onClick={() => onChange(id)}
        >
          <Icon aria-hidden="true" />
          <span>{label}</span>
        </button>
      ))}
    </nav>
  );
}
