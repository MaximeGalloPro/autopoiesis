import { describe, expect, test } from "bun:test";
import { readFileSync } from "node:fs";
import { renderToStaticMarkup } from "react-dom/server";
import {
  CAMPFIRE_CARD_COUNT,
  CampfireCallAlert,
  CampfireCardsOverlay,
  campfireCardsFromPrompt,
  campfireOverlayReducer,
} from "../src/components/CampfireCardsOverlay";
import {
  initialApiCallCounter,
  nextApiCallCounter,
} from "../src/lib/apiCallCounter";
import type { EvolutionRequest, ValidationPrompt } from "../src/protocol";

const card = (index: number): EvolutionRequest => ({
  request_id: `card-${index}`,
  agent_id: `agent-${index}`,
  agent_name: `Personnage ${index}`,
  title: `Proposition ${index}`,
  need: "Répondre à un besoin observé",
  obstacle: "Le mécanisme manque encore",
  proposed_change: "Ajouter une capacité locale validable",
  mechanism: "Une proposition sans effet direct",
  acceptance_tests: ["Le moteur valide chaque effet"],
  status: "pending",
});

const prompt = (requests: EvolutionRequest[]): ValidationPrompt => ({
  kind: "feature",
  stage: "choose",
  day: 4,
  simulation_cycle: 960,
  requests,
  allowed_commands: ["1", "2", "3"],
});
const styles = readFileSync(new URL("../src/styles.css", import.meta.url), "utf8");

describe("cartes au feu de camp", () => {
  test("n'expose le choix au feu que pour exactement trois cartes pending", () => {
    expect(campfireCardsFromPrompt(prompt([card(1), card(2), card(3)]))).toHaveLength(CAMPFIRE_CARD_COUNT);
    expect(campfireCardsFromPrompt(prompt([card(1), card(2)]))).toEqual([]);
    expect(campfireCardsFromPrompt({ ...prompt([card(1), card(2), card(3)]), kind: "devil" })).toEqual([]);
  });

  test("ouvre l'overlay au clic de l'alerte du feu et le referme dès le choix", () => {
    const open = campfireOverlayReducer({ open: false }, { type: "campfire_clicked", has_alert: true });
    expect(open).toEqual({ open: true });
    expect(campfireOverlayReducer(open, { type: "card_chosen" })).toEqual({ open: false });
    expect(campfireOverlayReducer({ open: false }, { type: "campfire_clicked", has_alert: false })).toEqual({ open: false });
  });

  test("place exactement trois cartes dans un overlay en bas du monde", () => {
    const html = renderToStaticMarkup(
      <CampfireCardsOverlay requests={[card(1), card(2), card(3)]} onChoose={() => undefined} />,
    );
    expect(html).toContain("campfire-cards-overlay");
    expect(html.match(/campfire-decision-card/g)).toHaveLength(CAMPFIRE_CARD_COUNT);
    expect(html).toContain("Choisir une proposition");

    const incomplete = renderToStaticMarkup(
      <CampfireCardsOverlay requests={[card(1), card(2)]} onChoose={() => undefined} />,
    );
    expect(incomplete).toBe("");
    expect(styles).toContain(".campfire-cards-overlay { position: fixed;");
    expect(styles).toContain("bottom: 92px");
  });

  test("fait confirmer le palier d’appels au feu sans lui donner de commande monde", () => {
    const html = renderToStaticMarkup(
      <CampfireCallAlert onAcknowledge={() => undefined} onClose={() => undefined} />,
    );
    expect(html).toContain("Alerte d’appels IA au feu de camp");
    expect(html).toContain("J’ai pris connaissance");
    expect(html).toContain("Aucune règle du monde n’est modifiée");
  });
});

describe("compteur d'appels de présentation", () => {
  test("compte chaque appel une seule fois et signale chaque dixième appel", () => {
    let counter = initialApiCallCounter();
    for (let index = 1; index <= 10; index += 1) {
      counter = nextApiCallCounter(counter, {
        kind: "evolution_request",
        agent_id: "ada",
        agent_name: "Ada",
        simulation_cycle: index * 240,
        call_number: 1,
        total_calls: 1,
        elapsed_ms: 0,
      });
    }
    const duplicate = nextApiCallCounter(counter, {
      kind: "evolution_request",
      agent_id: "ada",
      agent_name: "Ada",
      simulation_cycle: 2_400,
      call_number: 1,
      total_calls: 1,
      elapsed_ms: 800,
    });

    expect(counter.total).toBe(10);
    expect(counter.has_milestone_alert).toBe(true);
    expect(duplicate).toEqual(counter);
  });

  test("préfère le total persistant fourni par l'adaptateur", () => {
    const counter = nextApiCallCounter(initialApiCallCounter(), null, 20);
    expect(counter).toMatchObject({ total: 20, has_milestone_alert: true });
  });
});
