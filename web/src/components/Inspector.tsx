import { Activity, Brain, Heart, PackageOpen, PawPrint, Sparkles, Users } from "lucide-react";
import { useState } from "react";
import type { AgentState, AnimalState, WorldSnapshot } from "../protocol";
import { actionLabels, animalLabels, attributeLabels, clampPercent } from "../lib/format";
import type { EntitySelection } from "./WorldScene";

function Meter({ label, value, inverse = false }: { label: string; value: number; inverse?: boolean }) {
  const display = clampPercent(value);
  const critical = inverse ? display >= 70 : display <= 30;
  return (
    <div className="meter-row">
      <div className="meter-label"><span>{label}</span><strong>{display}</strong></div>
      <div className="meter-track" aria-label={`${label} : ${display} sur 100`} role="meter" aria-valuemin={0} aria-valuemax={100} aria-valuenow={display}>
        <span className={critical ? "critical" : ""} style={{ width: `${display}%` }} />
      </div>
    </div>
  );
}

function VitalView({ agent }: { agent: AgentState }) {
  const progress = agent.project.target > 0 ? clampPercent((agent.project.progress / agent.project.target) * 100) : 0;
  return (
    <div className="panel-stack">
      <section className="detail-section vital-grid" aria-label="Besoins vitaux">
        <Meter label="Santé" value={agent.health} />
        <Meter label="Faim" value={agent.hunger} inverse />
        <Meter label="Soif" value={agent.thirst} inverse />
        <Meter label="Fatigue" value={agent.fatigue} inverse />
      </section>
      <section className="detail-section mood-card">
        <span className="eyebrow">Humeur</span>
        <strong>{agent.mood}</strong>
        <span>Monotonie {agent.boredom}/100</span>
      </section>
      <section className="detail-section">
        <div className="section-title"><Sparkles size={15} /> Aspiration</div>
        <p className="aspiration">« {agent.behavior.aspiration || "Non formulée"} »</p>
        <div className="project-head">
          <div><span className="eyebrow">Projet durable</span><strong>{agent.project.title || "En émergence"}</strong></div>
          <span className={`status-pill ${agent.project.status}`}>{agent.project.status}</span>
        </div>
        <div className="project-progress"><span style={{ width: `${progress}%` }} /></div>
        <div className="project-caption"><span>Étape {agent.project.step}</span><span>{agent.project.progress} / {agent.project.target}</span></div>
        {agent.project.blocked_reason && <p className="blocked-reason">{agent.project.blocked_reason}</p>}
        {agent.project.missing_capability && <p className="missing-capability">Capacité attendue · {agent.project.missing_capability}</p>}
      </section>
      <section className="detail-section inventory">
        <div className="section-title"><PackageOpen size={15} /> Inventaire</div>
        <span>Bois <strong>{agent.wood_inventory}</strong></span>
        <span>Branches <strong>{agent.branch_inventory}</strong></span>
        <span>Provision <strong>{agent.carried_food ? `${agent.carried_food.type} · ${agent.carried_food.nutrition}` : "Aucune"}</strong></span>
      </section>
    </div>
  );
}

function ProfileView({ agent }: { agent: AgentState }) {
  return (
    <div className="panel-stack">
      <section className="detail-section">
        <div className="section-title"><Brain size={15} /> Attributs</div>
        <div className="attribute-grid">
          {(Object.entries(agent.attributes) as [keyof AgentState["attributes"], number][]).map(([key, value]) => (
            <div key={key}><span>{attributeLabels[key]}</span><strong>{value}</strong></div>
          ))}
        </div>
      </section>
      <section className="detail-section">
        <div className="section-title">Tempérament · {agent.behavior.archetype || "singulier"}</div>
        <div className="trait-list">
          {Object.entries(agent.personality).map(([key, value]) => (
            <div key={key}><span>{key}</span><i><b style={{ width: `${clampPercent(value)}%` }} /></i><strong>{value}</strong></div>
          ))}
        </div>
      </section>
      <section className="detail-section">
        <div className="section-title"><Activity size={15} /> Actions disponibles</div>
        <div className="chip-list">
          {agent.available_actions.map((action) => <span key={action}>{actionLabels[action] ?? action}</span>)}
        </div>
      </section>
    </div>
  );
}

function SocialView({ agent, snapshot }: { agent: AgentState; snapshot: WorldSnapshot }) {
  const relationships = Object.entries(agent.relationships);
  return (
    <div className="panel-stack">
      <section className="detail-section">
        <div className="section-title"><Users size={15} /> Relations</div>
        {relationships.length === 0 ? <p className="muted">Aucune relation consolidée.</p> : (
          <div className="relationship-list">
            {relationships.map(([id, relation]) => (
              <article key={id}>
                <strong>{snapshot.agents.find((candidate) => candidate.id === id)?.name ?? id}</strong>
                <span>Confiance {relation.trust}</span><span>Affinité {relation.affinity}</span>
                <small>{relation.interactions} interaction{relation.interactions > 1 ? "s" : ""}</small>
              </article>
            ))}
          </div>
        )}
      </section>
      <section className="detail-section">
        <div className="section-title"><Brain size={15} /> Mémoires récentes</div>
        {agent.memories.length === 0 ? <p className="muted">La mémoire narrative est silencieuse.</p> : (
          <ol className="memory-list">{agent.memories.map((memory, index) => <li key={`${index}-${memory}`}>{memory}</li>)}</ol>
        )}
      </section>
    </div>
  );
}

function AnimalView({ animal }: { animal: AnimalState }) {
  return (
    <div className="panel-stack">
      <section className="animal-portrait" aria-hidden="true"><PawPrint size={42} /></section>
      <section className="detail-section vital-grid">
        <Meter label="Danger" value={animal.danger} inverse />
        <Meter label="Nutrition" value={animal.nutrition} />
      </section>
      <section className="detail-section key-values">
        <span>Identifiant <strong>{animal.id}</strong></span>
        <span>Position <strong>{animal.position.x} · {animal.position.y}</strong></span>
        <span>État <strong>{animal.alive ? "Vivant" : "Sans vie"}</strong></span>
      </section>
    </div>
  );
}

export function Inspector({ snapshot, selected, onSelect }: {
  snapshot: WorldSnapshot;
  selected: EntitySelection | null;
  onSelect: (selection: EntitySelection) => void;
}) {
  const [tab, setTab] = useState<"vitals" | "profile" | "social" | "events">("vitals");
  const agent = selected?.kind === "agent" ? snapshot.agents.find((candidate) => candidate.id === selected.id) : undefined;
  const animal = selected?.kind === "animal" ? snapshot.animals.find((candidate) => candidate.id === selected.id) : undefined;
  const fallback = snapshot.agents[0];
  const inspected = agent ?? (!animal ? fallback : undefined);

  return (
    <aside className="inspector">
      <div className="entity-switcher" aria-label="Entités observables">
        {snapshot.agents.map((candidate) => (
          <button
            key={candidate.id}
            className={inspected?.id === candidate.id ? "active" : ""}
            onClick={() => onSelect({ kind: "agent", id: candidate.id })}
            title={`Observer ${candidate.name}`}
          >{candidate.name.slice(0, 1)}</button>
        ))}
        {snapshot.animals.filter((candidate) => candidate.alive).slice(0, 6).map((candidate) => (
          <button
            key={candidate.id}
            className={animal?.id === candidate.id ? "active animal" : "animal"}
            onClick={() => onSelect({ kind: "animal", id: candidate.id })}
            title={`Observer ${animalLabels[candidate.type] ?? candidate.type}`}
          ><PawPrint size={13} /></button>
        ))}
      </div>
      <header className="inspector-header">
        <div>
          <span className="eyebrow">{animal ? "Faune observée" : inspected?.behavior.archetype || "Personnage"}</span>
          <h2>{animal ? animalLabels[animal.type] ?? animal.type : inspected?.name ?? "Aucun personnage"}</h2>
        </div>
        <span className="coordinate">{(animal ?? inspected)?.position.x ?? "–"} · {(animal ?? inspected)?.position.y ?? "–"}</span>
      </header>
      {!animal && inspected && (
        <nav className="inspector-tabs" aria-label="Détails du personnage">
          <button className={tab === "vitals" ? "active" : ""} onClick={() => setTab("vitals")}><Heart size={14} /> Vie</button>
          <button className={tab === "profile" ? "active" : ""} onClick={() => setTab("profile")}><Brain size={14} /> Profil</button>
          <button className={tab === "social" ? "active" : ""} onClick={() => setTab("social")}><Users size={14} /> Liens</button>
          <button className={tab === "events" ? "active" : ""} onClick={() => setTab("events")}><Activity size={14} /> Fil</button>
        </nav>
      )}
      <div className="inspector-content">
        {animal && <AnimalView animal={animal} />}
        {inspected && tab === "vitals" && <VitalView agent={inspected} />}
        {inspected && tab === "profile" && <ProfileView agent={inspected} />}
        {inspected && tab === "social" && <SocialView agent={inspected} snapshot={snapshot} />}
        {inspected && tab === "events" && (
          <section className="detail-section">
            <div className="section-title"><Activity size={15} /> Événements récents</div>
            <ol className="event-list">
              {snapshot.recent_events.slice().reverse().map((event, index) => <li key={`${index}-${event}`}>{event}</li>)}
            </ol>
          </section>
        )}
      </div>
    </aside>
  );
}
