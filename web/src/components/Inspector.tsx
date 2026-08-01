import { Activity, BookOpenText, Brain, CalendarDays, CloudRain, Flame, Heart, Map, PackageOpen, PawPrint, Sparkles, Users } from "lucide-react";
import { useState } from "react";
import type { AgentState, AnimalState, WorldSnapshot } from "../protocol";
import { campEncyclopediaFromSnapshot } from "../lib/campEncyclopedia";
import { campStatusFromSnapshot } from "../lib/campStatus";
import { actionLabels, animalLabels, attributeLabels, clampPercent } from "../lib/format";
import { observatoryViewLabel, type ObservatoryView } from "./ObservatoryNavigation";
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

function CampView({ snapshot }: { snapshot: WorldSnapshot }) {
  const campfires = snapshot.cells.filter((cell) => cell.campfire);
  const storedFood = snapshot.cells.reduce((total, cell) => total + cell.stored_food, 0);
  const residents = snapshot.agents.filter((agent) => agent.alive);
  const focalFire = campfires[0];
  const encyclopedia = campEncyclopediaFromSnapshot(snapshot);
  const campStatus = campStatusFromSnapshot(snapshot);
  const residentNames = (names: string[]) => names.join(", ");

  return (
    <div className="panel-stack section-view">
      <section className="inspector-intro camp-intro">
        <span className="section-kicker"><Flame size={14} /> Vie collective</span>
        <h3>{campStatus.population.extinct ? "Le foyer ne compte plus aucun vivant." : campfires.length > 0 ? "Le foyer rassemble le vivant." : "Aucun foyer n’est encore allumé."}</h3>
        <p>{campStatus.population.extinct
          ? "Le monde reste consultable : l’interface ne relance jamais une simulation sans commande du moteur."
          : campfires.length > 0
          ? "Les stocks et abris ci-dessous sont les éléments actuellement observables par le moteur."
          : "La première flamme persistante deviendra le point de ralliement de la communauté."}</p>
      </section>
      <section className="detail-section camp-summary" aria-label="État du foyer">
        <span><small>Stade</small><strong>{campStatus.stage}</strong></span>
        <span><small>Coffre</small><strong>{campStatus.chest ? `${campStatus.chest.occupation} / ${campStatus.chest.capacity}` : "Absent"}</strong></span>
        <span><small>Population</small><strong>{campStatus.population.living} vivant{campStatus.population.living > 1 ? "s" : ""}</strong></span>
        <span><small>État</small><strong className={campStatus.population.extinct ? "status-extinct" : "status-living"}>{campStatus.population.extinct ? "Extinction" : "Vivante"}</strong></span>
      </section>
      <section className="detail-section key-values">
        <div className="section-title"><Flame size={15} /> Point de ralliement</div>
        {focalFire ? <>
          <span>Position <strong>{focalFire.position.x} · {focalFire.position.y}</strong></span>
          <span>Provisions observées <strong>{focalFire.stored_food}</strong></span>
          <span>Réserve commune <strong>{storedFood} rations</strong></span>
          <span>Coffre <strong>{campStatus.chest ? `Niveau ${campStatus.chest.level} · ${campStatus.chest.occupation} / ${campStatus.chest.capacity}` : "Non construit"}</strong></span>
          <span>Abri local <strong>{focalFire.shelter_level > 0 ? `Niveau ${focalFire.shelter_level}` : "À construire"}</strong></span>
        </> : <p className="muted">Les habitants cherchent encore à constituer un premier feu de camp.</p>}
      </section>
      <section className="detail-section camp-population" aria-label="Population par âge et génération">
        <div className="section-title"><Users size={15} /> Population</div>
        <div className="camp-cohorts">
          <div><small>Âge</small>{campStatus.population.ages.map((cohort) => <span key={cohort.label}>{cohort.label}<strong>{cohort.count}</strong></span>)}</div>
          <div><small>Génération</small>{campStatus.population.generations.length === 0
            ? <p className="muted">Aucune population vivante.</p>
            : campStatus.population.generations.map((cohort) => <span key={cohort.label}>{cohort.label}<strong>{cohort.count}</strong></span>)}</div>
        </div>
      </section>
      <section className="detail-section resident-list">
        <div className="section-title"><Users size={15} /> Autour du foyer</div>
        {residents.length === 0 ? <p className="muted">Aucun personnage vivant à observer.</p> : residents.map((resident) => (
          <div key={resident.id}><span>{resident.name}</span><small>{resident.behavior.archetype || "habitant"}{resident.age_days === undefined ? "" : ` · ${resident.age_days} j`}{resident.generation === undefined ? "" : ` · gén. ${resident.generation}`}</small></div>
        ))}
      </section>
      {campStatus.population.extinct && <section className="detail-section extinction-state" aria-label="État d’extinction">
        <div className="section-title"><Users size={15} /> Extinction constatée</div>
        <p>La simulation est terminée ; son état reste seulement observable.</p>
        <button type="button" disabled aria-describedby="new-civilization-unavailable">Nouvelle civilisation</button>
        <small id="new-civilization-unavailable">{campStatus.newCivilization.reason}</small>
      </section>}
      <section className="detail-section camp-encyclopedia" aria-label="Encyclopédie du foyer">
        <div className="section-title"><BookOpenText size={15} /> Encyclopédie du foyer</div>
        <div className="camp-encyclopedia-group">
          <h4>Recettes disponibles</h4>
          {encyclopedia.recipes.length === 0 ? <p className="muted">Aucune recette n’est disponible dans cet instantané.</p> : encyclopedia.recipes.map((entry) => (
            <article key={entry.action}>
              <strong>Fabrication au foyer</strong>
              <span>{residentNames(entry.residents)} peut fabriquer selon une recette validée.</span>
            </article>
          ))}
        </div>
        <div className="camp-encyclopedia-group">
          <h4>Compétences à apprendre</h4>
          {encyclopedia.learnableSkills.length === 0 ? <p className="muted">Aucune leçon n’est disponible dans cet instantané.</p> : encyclopedia.learnableSkills.map((entry) => (
            <article key={entry.action}>
              <strong>Leçon au foyer</strong>
              <span>{residentNames(entry.residents)} peut transmettre une compétence validée.</span>
            </article>
          ))}
        </div>
        <div className="camp-encyclopedia-group">
          <h4>Partage de carte</h4>
          {encyclopedia.mapSharing.available
            ? <p>{residentNames(encyclopedia.mapSharing.residents)} peut partager sa carte connue.</p>
            : <p className="muted">Le moteur ne signale aucun partage de carte disponible.</p>}
        </div>
      </section>
    </div>
  );
}

function HistoryView({ snapshot }: { snapshot: WorldSnapshot }) {
  const events = snapshot.recent_events.slice().reverse();
  return (
    <div className="panel-stack section-view">
      <section className="inspector-intro">
        <span className="section-kicker"><Activity size={14} /> Chronique vivante</span>
        <h3>Histoire</h3>
      </section>
      <section className="detail-section history-feed">
        {events.length === 0 ? <p className="muted">Aucun événement n’a encore été rapporté.</p> : (
          <ol className="event-list">{events.map((event, index) => <li key={`${index}-${event}`}>{event}</li>)}</ol>
        )}
      </section>
    </div>
  );
}

function MapsView({ snapshot, selected }: { snapshot: WorldSnapshot; selected: EntitySelection | null }) {
  const selectedEntity = selected?.kind === "agent"
    ? snapshot.agents.find((agent) => agent.id === selected.id)
    : snapshot.animals.find((animal) => animal.id === selected?.id);
  return (
    <div className="panel-stack section-view">
      <section className="inspector-intro map-intro">
        <span className="section-kicker"><Map size={14} /> Carte d’observation</span>
        <h3>Un monde sans bord.</h3>
        <p>La carte est bornée : ses lisières arrêtent les déplacements.</p>
      </section>
      <section className="detail-section map-card">
        <div className="map-grid" aria-hidden="true"><span /><span /><span /><span /><span /><span /><span /><span /><span /></div>
        <div className="map-key" aria-label="Repères de carte">
          <span><i className="food" />Nourriture</span>
          <span><i className="wood" />Bois</span>
          <span><i className="shelter" />Abri</span>
          <span><i className="fire" />Foyer</span>
        </div>
      </section>
      <section className="detail-section key-values">
        <div className="section-title"><Map size={15} /> Repère actuel</div>
        <span>Étendue <strong>{snapshot.width} × {snapshot.height}</strong></span>
        <span>Entité suivie <strong>{selectedEntity ? ("name" in selectedEntity ? selectedEntity.name : animalLabels[selectedEntity.type]) : "Aucune"}</strong></span>
        <span>Position <strong>{selectedEntity ? `${selectedEntity.position.x} · ${selectedEntity.position.y}` : "—"}</strong></span>
      </section>
    </div>
  );
}

function WorldView({ snapshot }: { snapshot: WorldSnapshot }) {
  const livingAgents = snapshot.agents.filter((agent) => agent.alive).length;
  const livingAnimals = snapshot.animals.filter((animal) => animal.alive).length;
  return (
    <div className="panel-stack section-view">
      <section className="inspector-intro world-intro">
        <span className="section-kicker"><CloudRain size={14} /> Lecture du monde</span>
        <h3>{snapshot.climate.condition} · {snapshot.climate.temperature_c} °C</h3>
        <p>Le rythme du monde reste défini par le moteur ; la vitesse graphique n’altère ni ses cycles ni ses validations.</p>
      </section>
      <section className="detail-section world-grid">
        <span><CalendarDays size={15} /><small>Calendrier</small><strong>An {snapshot.date.year} · mois {snapshot.date.month}</strong></span>
        <span><Activity size={15} /><small>Cycle</small><strong>{snapshot.simulation_cycle.toLocaleString("fr-FR")}</strong></span>
        <span><CloudRain size={15} /><small>Pluie</small><strong>{snapshot.climate.rainfall_mm} mm</strong></span>
        <span><Users size={15} /><small>Population</small><strong>{livingAgents} humains · {livingAnimals} animaux</strong></span>
      </section>
    </div>
  );
}

export function Inspector({ snapshot, selected, onSelect, view }: {
  snapshot: WorldSnapshot;
  selected: EntitySelection | null;
  onSelect: (selection: EntitySelection) => void;
  view: ObservatoryView;
}) {
  const [tab, setTab] = useState<"vitals" | "profile" | "social" | "events">("vitals");
  const agent = selected?.kind === "agent" ? snapshot.agents.find((candidate) => candidate.id === selected.id) : undefined;
  const animal = selected?.kind === "animal" ? snapshot.animals.find((candidate) => candidate.id === selected.id) : undefined;
  const fallback = snapshot.agents[0];
  const inspected = agent ?? (!animal ? fallback : undefined);

  return (
    <aside className="inspector" aria-label={`${observatoryViewLabel(view)} de l’observatoire`}>
      {view === "characters" && <>
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
            <ol className="event-list">
              {snapshot.recent_events.slice().reverse().map((event, index) => <li key={`${index}-${event}`}>{event}</li>)}
            </ol>
          </section>
        )}
      </div>
      </>}
      {view === "camp" && <div className="inspector-content"><CampView snapshot={snapshot} /></div>}
      {view === "history" && <div className="inspector-content"><HistoryView snapshot={snapshot} /></div>}
      {view === "maps" && <div className="inspector-content"><MapsView snapshot={snapshot} selected={selected} /></div>}
      {view === "world" && <div className="inspector-content"><WorldView snapshot={snapshot} /></div>}
    </aside>
  );
}
