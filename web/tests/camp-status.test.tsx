import { describe, expect, test } from "bun:test";
import { renderToStaticMarkup } from "react-dom/server";
import { Inspector } from "../src/components/Inspector";
import type { AgentState } from "../src/protocol";
import { campStatusFromSnapshot } from "../src/lib/campStatus";
import { worldSnapshot } from "./fixtures";

function resident(overrides: Partial<AgentState> = {}): AgentState {
  return {
    id: "ada", name: "Ada", position: { x: 12, y: 8 }, health: 90, hunger: 20, thirst: 20,
    fatigue: 10, alive: true, sleeping_days: 0, boredom: 12, mood: "Curieuse",
    personality: { curiosity: 70, prudence: 40, sociability: 60, patience: 50, empathy: 60 },
    attributes: { strength: 50, agility: 50, endurance: 50, toughness: 50, recuperation: 50, disease_resistance: 50, focus: 50, willpower: 50, memory: 50, spatial_sense: 50 },
    behavior: { archetype: "éclaireuse", aspiration: "Apprendre", construction_drive: 40, provision_drive: 40, exploration_drive: 70, social_drive: 60, preferred_foods: [] },
    project: { key: "explore", title: "Explorer", status: "active", step: 1, progress: 1, target: 4, blocked_reason: "", missing_capability: "", started_day: 1, last_progress_cycle: 1 },
    memories: [], relationships: {}, available_actions: [], wood_inventory: 0, branch_inventory: 0,
    carried_food: null, ...overrides,
  };
}

describe("statut observé du foyer", () => {
  test("dérive le stade, le coffre et les cohortes sans écrire dans l’instantané", () => {
    const snapshot = worldSnapshot({
      camp_stage: "Foyer établi",
      agents: [
        resident({ age_days: 25, generation: 0 }),
        resident({ id: "noe", name: "Noé", age_days: 6, generation: 1 }),
        resident({ id: "lia", name: "Lia" }),
      ],
      cells: [{
        position: { x: 12, y: 8 }, terrain: "ground", food: 0, wood: 0, fibers: 0,
        shelter_level: 1, branches: 0, campfire: true, stored_food: 4,
        camp_chest: { position: { x: 13, y: 8 }, level: 1, occupation: 12, capacity: 48 },
      }],
    });

    expect(campStatusFromSnapshot(snapshot)).toMatchObject({
      stage: "Foyer établi",
      chest: { occupation: 12, capacity: 48 },
      population: {
        living: 3,
        extinct: false,
        ages: [{ label: "Enfants", count: 1 }, { label: "Adultes", count: 1 }, { label: "Âge non transmis", count: 1 }],
        generations: [{ label: "Génération 0", count: 1 }, { label: "Génération 1", count: 1 }, { label: "Génération non transmise", count: 1 }],
      },
    });
    expect(snapshot.agents).toHaveLength(3);
  });

  test("signale l’extinction et n’offre aucun redémarrage simulé", () => {
    const snapshot = worldSnapshot({ agents: [resident({ alive: false, age_days: 100, generation: 0 })] });
    const html = renderToStaticMarkup(<Inspector snapshot={snapshot} selected={null} onSelect={() => undefined} view="camp" />);

    expect(html).toContain("Extinction constatée");
    expect(html).toContain("Nouvelle civilisation");
    expect(html).toContain("disabled");
    expect(html).toContain("Aucune commande validée");
  });
});
