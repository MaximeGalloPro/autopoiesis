import {
  CalendarDays,
  CloudRain,
  Bot,
  Flame,
  Gauge,
  HelpCircle,
  Leaf,
  Maximize2,
  Minimize2,
  Pause,
  PanelRightClose,
  PanelRightOpen,
  Play,
  Radio,
  RotateCcw,
  Settings2,
  Sparkles,
  Sun,
  TimerReset,
  TriangleAlert,
  Users,
} from "lucide-react";
import { lazy, Suspense, useEffect, useState } from "react";
import { Inspector } from "./components/Inspector";
import { ObservatoryNavigation, type ObservatoryView } from "./components/ObservatoryNavigation";
import { ProgressDock } from "./components/ProgressDock";
import {
  EvolutionCompletionOverlay,
  EvolutionCompletionReminder,
  ValidationOverlay,
  ValidationReminder,
} from "./components/ValidationOverlay";
import type { EntitySelection } from "./components/WorldScene";
import { useSimulation } from "./hooks/useSimulation";
import { seasonLabels } from "./lib/format";
import { SIMULATION_SPEEDS, type AiServicesState, type EngineCommand, type SimulationSpeed } from "./protocol";

const WorldScene = lazy(() => import("./components/WorldScene").then((module) => ({
  default: module.WorldScene,
})));

const connectionLabels = {
  connecting: "Connexion…",
  connected: "Flux direct",
  reconnecting: "Reconnexion…",
  error: "Connexion interrompue",
};

function ShortcutHelp({ onClose }: { onClose: () => void }) {
  return (
    <div className="shortcut-popover" role="dialog" aria-label="Raccourcis clavier">
      <header><strong>Raccourcis</strong><button onClick={onClose} aria-label="Fermer l’aide">×</button></header>
      <dl>
        <div><dt><kbd>Espace</kbd></dt><dd>Pause / reprise</dd></div>
        <div><dt><kbd>1</kbd>…<kbd>5</kbd></dt><dd>Vitesses 0,25× à 4×</dd></div>
        <div><dt><kbd>[</kbd> <kbd>]</kbd></dt><dd>Ralentir / accélérer</dd></div>
        <div><dt><kbd>F</kbd></dt><dd>Mode observation plein écran</dd></div>
        <div><dt><kbd>?</kbd></dt><dd>Afficher cette aide</dd></div>
      </dl>
      <p>La souris fait pivoter le monde. La molette zoome et le clic droit déplace la caméra.</p>
    </div>
  );
}

function ServiceControls({
  services,
  onToggle,
  onClose,
}: {
  services: AiServicesState | null;
  onToggle: (service: "api" | "codex", enabled: boolean) => void;
  onClose: () => void;
}) {
  const entries = services ? [
    {
      key: "api" as const,
      label: "API OpenAI",
      icon: <Sparkles />,
      state: services.api,
      allowed: services.api.available,
    },
    {
      key: "codex" as const,
      label: "Codex · Validator et Dieu",
      icon: <Bot />,
      state: services.codex,
      allowed: services.codex.available && services.codex.verifier_available,
    },
  ] : [];
  return (
    <section className="service-popover" role="dialog" aria-label="Services IA">
      <header><div><Settings2 /><strong>Services IA</strong></div><button onClick={onClose} aria-label="Fermer">×</button></header>
      {!services && <p>Lecture des capacités du serveur…</p>}
      {entries.map((entry) => (
        <div className="service-row" key={entry.key}>
          <div className="service-icon">{entry.icon}</div>
          <div className="service-copy">
            <strong>{entry.label}</strong>
            <small>{entry.state.detail}</small>
          </div>
          <button
            type="button"
            role="switch"
            aria-checked={entry.state.enabled}
            disabled={!entry.allowed}
            className={entry.state.enabled ? "service-switch active" : "service-switch"}
            onClick={() => onToggle(entry.key, !entry.state.enabled)}
          ><span /></button>
        </div>
      ))}
      <p>Les identifiants restent exclusivement sur le serveur. Une évolution exige toujours la validation humaine et le build Docker.</p>
    </section>
  );
}

function EmptyInspector({ error }: { error: string | null }) {
  return (
    <aside className="inspector empty-inspector">
      <div className="empty-mark"><Leaf /></div>
      <span className="eyebrow">Observatoire en attente</span>
      <h2>Le monde n’a pas encore parlé.</h2>
      <p>Le serveur attend le premier événement d’état autoritaire émis par le moteur C++.</p>
      {error && <div className="engine-error"><TriangleAlert size={16} />{error}</div>}
      <div className="contract-note"><Radio size={15} /><span>Canal attendu<br /><code>AUTOPOIESIS_EVENT &#123;…&#125;</code></span></div>
    </aside>
  );
}

export default function App() {
  const { data, services, connection, commandError, dismissCommandError, sendCommand, setService } = useSimulation();
  const snapshot = data.state;
  const [selected, setSelected] = useState<EntitySelection | null>(null);
  const [delayDraft, setDelayDraft] = useState(500);
  const [showShortcuts, setShowShortcuts] = useState(false);
  const [showServices, setShowServices] = useState(false);
  const [openGuardKey, setOpenGuardKey] = useState<string | null>(null);
  const [activeView, setActiveView] = useState<ObservatoryView>("characters");
  // La carte est le plan principal. Les informations secondaires ne prennent
  // de la place qu'après une demande explicite via la navigation ou une entité.
  const [panelCollapsed, setPanelCollapsed] = useState(true);
  const [observationMode, setObservationMode] = useState(false);

  useEffect(() => {
    if (snapshot) setDelayDraft(snapshot.delay_ms);
  }, [snapshot?.delay_ms]);

  useEffect(() => {
    if (!snapshot || selected) return;
    const firstAgent = snapshot.agents[0];
    const firstAnimal = snapshot.animals.find((animal) => animal.alive);
    if (firstAgent) setSelected({ kind: "agent", id: firstAgent.id });
    else if (firstAnimal) setSelected({ kind: "animal", id: firstAnimal.id });
  }, [snapshot, selected]);

  const dispatch = (command: EngineCommand) => void sendCommand(command);
  const controlDisabled = !snapshot || data.engine.status !== "running";

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      const target = event.target as HTMLElement | null;
      if (target?.matches("input, textarea, select, button, [contenteditable=true]")) return;
      if (event.key === "?") {
        setShowShortcuts((visible) => !visible);
        return;
      }
      if (event.key.toLowerCase() === "f") {
        setObservationMode((active) => !active);
        return;
      }
      if (event.key === "Escape" && observationMode) {
        setObservationMode(false);
        return;
      }
      if (!snapshot || data.engine.status !== "running") return;
      if (event.code === "Space") {
        event.preventDefault();
        dispatch({ type: "control.pause", paused: !snapshot.paused });
      }
      const numericIndex = Number.parseInt(event.key, 10) - 1;
      if (numericIndex >= 0 && numericIndex < SIMULATION_SPEEDS.length) {
        dispatch({ type: "control.speed", multiplier: SIMULATION_SPEEDS[numericIndex] });
      }
      if (event.key === "[" || event.key === "]") {
        const current = SIMULATION_SPEEDS.indexOf(snapshot.speed);
        const offset = event.key === "[" ? -1 : 1;
        const next = SIMULATION_SPEEDS[Math.max(0, Math.min(SIMULATION_SPEEDS.length - 1, current + offset))];
        dispatch({ type: "control.speed", multiplier: next });
      }
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [snapshot, data.engine.status, observationMode, sendCommand]);

  const dayProgress = snapshot ? Math.max(0, Math.min(100, (snapshot.cycle_in_day / snapshot.cycles_per_day) * 100)) : 0;
  const validationGuardKey = data.validation
    ? `validation:${data.validation.kind}:${data.validation.simulation_cycle}`
    : null;
  const completionGuardKey = data.evolution_completion
    ? `completion:${data.evolution_completion.request_id}:${data.evolution_completion.stage}`
    : null;
  const selectEntity = (selection: EntitySelection) => {
    setSelected(selection);
    setActiveView("characters");
    setPanelCollapsed(false);
  };
  const selectView = (view: ObservatoryView) => {
    setActiveView(view);
    setPanelCollapsed(false);
  };

  return (
    <div className={`app-shell ${snapshot?.phase === "night" && !data.awaiting_dawn ? "night" : "day"}${observationMode ? " observation-mode" : ""}`}>
      <header className="topbar">
        <div className="brand-block">
          <div className="brand-mark" aria-hidden="true"><span /><span /><span /></div>
          <div><strong>Autopoiesis</strong><span>Observatoire du vivant</span></div>
        </div>
        <ObservatoryNavigation activeView={activeView} onChange={selectView} />
        <div className="world-glance" aria-label="État du monde">
          <span><CalendarDays aria-hidden="true" />{snapshot ? `Jour ${snapshot.date.absolute_day}` : "Connexion"}</span>
          <span><Sun aria-hidden="true" />{data.awaiting_dawn ? "Aube" : snapshot?.phase === "night" ? "Nuit" : "Jour"}</span>
          <span><CloudRain aria-hidden="true" />{snapshot ? `${snapshot.climate.temperature_c} °C · ${snapshot.climate.condition}` : "—"}</span>
        </div>
        <div className="connection-tools">
          <span className={`connection-pill ${connection}`}><i />{connectionLabels[connection]}</span>
          <button
            className="observation-button"
            onClick={() => setObservationMode((active) => !active)}
            aria-label={observationMode ? "Quitter le mode observation plein écran" : "Activer le mode observation plein écran"}
            aria-pressed={observationMode}
            title="Mode observation (F)"
          >{observationMode ? <Minimize2 /> : <Maximize2 />}<span>{observationMode ? "Quitter" : "Observer"}</span></button>
          <button className="icon-button" onClick={() => setShowServices((visible) => !visible)} aria-label="Configurer les services IA" title="Services IA"><Settings2 /></button>
          <button className="icon-button" onClick={() => setShowShortcuts((visible) => !visible)} aria-label="Afficher les raccourcis" title="Raccourcis (?)"><HelpCircle /></button>
        </div>
      </header>

      <main className="workspace">
        <section className="world-panel">
          <div className="world-overlay top-left">
            <span className="eyebrow">Monde torique · 40 × 24</span>
            <strong>{snapshot ? `Jour ${snapshot.date.absolute_day} · ${seasonLabels[snapshot.date.season]}` : "Connexion au moteur"}</strong>
            <span>{data.awaiting_dawn ? "Aube prochaine" : `Cycle ${snapshot?.simulation_cycle.toLocaleString("fr-FR") ?? "—"}`}</span>
          </div>
          <div className="world-overlay top-right phase-orb" aria-label={`Progression de la journée ${Math.round(dayProgress)} %`}>
            <div style={{ "--progress": `${dayProgress * 3.6}deg` } as React.CSSProperties}><span>{data.awaiting_dawn ? "☀" : snapshot?.phase === "night" ? "☾" : "☀"}</span></div>
            <p><strong>{data.awaiting_dawn ? "Aube" : `${Math.round(dayProgress)}%`}</strong><small>{data.awaiting_dawn ? "prochaine" : "de la journée"}</small></p>
          </div>
          <Suspense fallback={<div className="world-canvas" aria-label="Chargement de la scène tridimensionnelle" />}>
            <WorldScene snapshot={snapshot} awaitingDawn={data.awaiting_dawn} selected={selected} onSelect={selectEntity} />
          </Suspense>
          <div className="world-legend" aria-label="Légende du monde">
            <span><i className="food" />Nourriture</span><span><i className="wood" />Bois</span><span><i className="fiber" />Fibres</span><span><i className="shelter" />Abri</span><span><i className="fire" /><Flame />Feu</span><span><i className="stock" />Réserve commune</span>
          </div>
          {!snapshot && (
            <div className="world-waiting">
              <RotateCcw className={connection === "reconnecting" ? "spin" : ""} />
              <strong>{data.engine.status === "unavailable" ? "Backend C++ indisponible" : "Initialisation du monde"}</strong>
              <span>{data.engine.last_error ?? "En attente du premier instantané…"}</span>
            </div>
          )}
        </section>

        <section className={`side-panel${panelCollapsed ? " collapsed" : ""}`} aria-label="Panneau d’observation">
          <button
            className="panel-toggle"
            type="button"
            aria-label={panelCollapsed ? "Déplier le panneau d’observation" : "Replier le panneau d’observation"}
            aria-controls="observatory-inspector"
            aria-expanded={!panelCollapsed}
            onClick={() => setPanelCollapsed((collapsed) => !collapsed)}
          >{panelCollapsed ? <PanelRightOpen /> : <PanelRightClose />}<span>{panelCollapsed ? "Ouvrir" : "Réduire"}</span></button>
          <div id="observatory-inspector" className="side-panel-content" hidden={panelCollapsed}>
            {snapshot
              ? <Inspector snapshot={snapshot} selected={selected} onSelect={selectEntity} view={activeView} />
              : <EmptyInspector error={data.engine.last_error} />}
          </div>
        </section>
      </main>

      <footer className="control-deck">
        <div className="pause-control">
          <button
            className={snapshot?.paused ? "paused" : ""}
            disabled={controlDisabled}
            aria-label={snapshot?.paused ? "Reprendre la simulation" : "Mettre la simulation en pause"}
            aria-pressed={snapshot?.paused ?? false}
            aria-keyshortcuts="Space"
            onClick={() => snapshot && dispatch({ type: "control.pause", paused: !snapshot.paused })}
          >{snapshot?.paused ? <Play /> : <Pause />}<span><small>Simulation</small><strong>{snapshot?.paused ? "Reprendre" : "En cours"}</strong></span></button>
        </div>
        <div className="speed-controls">
          <span><Gauge />Vitesse</span>
          <div role="group" aria-label="Vitesse de simulation">
            {SIMULATION_SPEEDS.map((speed, index) => (
              <button
                key={speed}
                disabled={controlDisabled}
                className={snapshot?.speed === speed ? "active" : ""}
                aria-pressed={snapshot?.speed === speed}
                aria-keyshortcuts={`${index + 1}`}
                onClick={() => dispatch({ type: "control.speed", multiplier: speed as SimulationSpeed })}
              >{String(speed).replace(".", ",")}×</button>
            ))}
          </div>
        </div>
        <label className="delay-control">
          <span><TimerReset /><span><small>Délai entre journées</small><strong>{delayDraft.toLocaleString("fr-FR")} ms</strong></span></span>
          <input
            type="range"
            min={0}
            max={10_000}
            step={100}
            value={delayDraft}
            disabled={controlDisabled}
            onChange={(event) => setDelayDraft(Number(event.target.value))}
            onPointerUp={() => dispatch({ type: "control.delay", milliseconds: delayDraft })}
            onKeyUp={() => dispatch({ type: "control.delay", milliseconds: delayDraft })}
          />
        </label>
        <div className="population"><Users /><span><small>Population</small><strong>{snapshot?.agents.filter((agent) => agent.alive).length ?? 0} humains · {snapshot?.animals.filter((animal) => animal.alive).length ?? 0} animaux</strong></span></div>
      </footer>

      <ProgressDock
        activity={data.activity}
        evolution={data.evolution}
        recompilation={data.recompilation}
        recentEvents={snapshot?.recent_events ?? []}
      />
      {data.validation && validationGuardKey && (openGuardKey === validationGuardKey
        ? <ValidationOverlay prompt={data.validation} sendCommand={sendCommand} onMinimize={() => setOpenGuardKey(null)} />
        : <ValidationReminder prompt={data.validation} sendCommand={sendCommand} onOpen={() => setOpenGuardKey(validationGuardKey)} />)}
      {data.evolution_completion && completionGuardKey && (openGuardKey === completionGuardKey
        ? <EvolutionCompletionOverlay completion={data.evolution_completion} sendCommand={sendCommand} onMinimize={() => setOpenGuardKey(null)} />
        : <EvolutionCompletionReminder completion={data.evolution_completion} sendCommand={sendCommand} onOpen={() => setOpenGuardKey(completionGuardKey)} />)}
      {showShortcuts && <ShortcutHelp onClose={() => setShowShortcuts(false)} />}
      {showServices && <ServiceControls services={services} onToggle={(service, enabled) => void setService(service, enabled)} onClose={() => setShowServices(false)} />}
      {commandError && <div className="toast error" role="alert"><TriangleAlert />{commandError}<button onClick={dismissCommandError} aria-label="Fermer">×</button></div>}
    </div>
  );
}
