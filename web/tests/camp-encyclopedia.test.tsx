import { describe, expect, test } from "bun:test";
import { renderToStaticMarkup } from "react-dom/server";
import { Inspector } from "../src/components/Inspector";
import { campEncyclopediaFromSnapshot } from "../src/lib/campEncyclopedia";
import type { AgentState } from "../src/protocol";
import { worldSnapshot } from "./fixtures";

function resident(overrides: Partial<AgentState> = {}): AgentState {
  return {
    id: "ada",
    name: "Ada",
    position: { x: 12, y: 8 },
    health: 90,
    hunger: 20,
    thirst: 20,
    fatigue: 10,
    alive: true,
    sleeping_days: 0,
    boredom: 12,
    mood: "Curieuse",
    personality: { curiosity: 70, prudence: 40, sociability: 60, patience: 50, empathy: 60 },
    attributes: { strength: 50, agility: 50, endurance: 50, toughness: 50, recuperation: 50, disease_resistance: 50, focus: 50, willpower: 50, memory: 50, spatial_sense: 50 },
    behavior: { archetype: "éclaireuse", aspiration: "Apprendre", construction_drive: 40, provision_drive: 40, exploration_drive: 70, social_drive: 60, preferred_foods: [] },
    project: { key: "explore", title: "Explorer", status: "active", step: 1, progress: 1, target: 4, blocked_reason: "", missing_capability: "", started_day: 1, last_progress_cycle: 1 },
    memories: [],
    relationships: {},
    available_actions: [],
    wood_inventory: 0,
    branch_inventory: 0,
    carried_food: null,
    ...overrides,
  };
}

describe("encyclopédie du foyer", () => {
  test("dérive les possibilités du snapshot sans écrire d’état de monde", () => {
    const snapshot = worldSnapshot({
      agents: [
        resident({ available_actions: ["craft_camp_item", "teach_skill", "share_map"] }),
        resident({ id: "noe", name: "Noé", available_actions: ["craft_camp_item"] }),
      ],
    });

    expect(campEncyclopediaFromSnapshot(snapshot)).toEqual({
      recipes: [{ action: "craft_camp_item", residents: ["Ada", "Noé"] }],
      learnableSkills: [{ action: "teach_skill", residents: ["Ada"] }],
      mapSharing: { available: true, residents: ["Ada"] },
    });
    expect(snapshot.agents[0]?.available_actions).toEqual(["craft_camp_item", "teach_skill", "share_map"]);
  });

  test("affiche au foyer les recettes, les leçons et le partage de carte signalés par le moteur", () => {
    const snapshot = worldSnapshot({
      agents: [resident({ available_actions: ["craft_camp_item", "teach_skill", "share_map"] })],
      cells: [{
        position: { x: 12, y: 8 }, terrain: "ground", food: 0, wood: 0, fibers: 0,
        shelter_level: 0, branches: 0, campfire: true, stored_food: 2,
      }],
    });
    const html = renderToStaticMarkup(
      <Inspector snapshot={snapshot} selected={null} onSelect={() => undefined} view="camp" />,
    );

    expect(html).toContain("Encyclopédie du foyer");
    expect(html).toContain("Recettes disponibles");
    expect(html).toContain("Fabrication au foyer");
    expect(html).toContain("Compétences à apprendre");
    expect(html).toContain("Leçon au foyer");
    expect(html).toContain("Partage de carte");
    expect(html).toContain("Ada peut partager sa carte connue.");
  });

  test("laisse les rubriques indisponibles informatives sans inventer de capacité", () => {
    const snapshot = worldSnapshot({ agents: [resident()] });
    const html = renderToStaticMarkup(
      <Inspector snapshot={snapshot} selected={null} onSelect={() => undefined} view="camp" />,
    );

    expect(html).toContain("Aucune recette n’est disponible dans cet instantané.");
    expect(html).toContain("Aucune leçon n’est disponible dans cet instantané.");
    expect(html).toContain("Le moteur ne signale aucun partage de carte disponible.");
  });
});
