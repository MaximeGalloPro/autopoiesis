import { describe, expect, test } from "bun:test";
import { readFileSync } from "node:fs";
import { renderToStaticMarkup } from "react-dom/server";
import { automaticContinuationKey, ValidationOverlay } from "../src/components/ValidationOverlay";
import type { EvolutionCompletion, ValidationPrompt } from "../src/protocol";

const request = {
  request_id: "request-1",
  agent_id: "ada",
  agent_name: "Ada",
  title: "Partager les provisions",
  need: "Réduire la faim commune",
  obstacle: "Les aliments restent dispersés",
  proposed_change: "Créer une réserve",
  mechanism: "Transfert local validé vers le feu",
  acceptance_tests: ["Aucun aliment ne peut être dupliqué"],
  status: "pending" as const,
};

describe("interface de validation", () => {
  test("ne conserve aucune couche de garde qui capte la carte entière", () => {
    const styles = readFileSync(new URL("../src/styles.css", import.meta.url), "utf8");

    expect(styles).toContain(".campfire-validation-overlay { position: fixed;");
    expect(styles).not.toContain(".guard-layer");
    expect(styles).not.toContain(".validation-modal");
  });

  test("garde la confirmation dans un panneau refermable, sans garde plein écran ni reprise", () => {
    const prompt: ValidationPrompt = {
      kind: "feature",
      stage: "confirm",
      day: 3,
      simulation_cycle: 7200,
      requests: [request],
      selected_request_id: request.request_id,
      allowed_commands: ["a", "r", "b", "q"],
    };
    const html = renderToStaticMarkup(<ValidationOverlay prompt={prompt} sendCommand={async () => true} onClose={() => undefined} />);
    expect(html).toContain("campfire-validation-overlay");
    expect(html).toContain("Fermer la validation");
    expect(html).toContain("Approuver");
    expect(html).toContain("Refuser");
    expect(html).toContain("Arrêter");
    expect(html).toContain("Changement proposé");
    expect(html).not.toContain("guard-layer");
    expect(html).not.toContain("Reprendre");
    expect(html).not.toContain("Le moteur reste en pause");
  });

  test("présente le fondement et la pression du Diable sans option aucune", () => {
    const prompt: ValidationPrompt = {
      kind: "devil",
      stage: "choose",
      day: 3,
      simulation_cycle: 7200,
      requests: [{ ...request, source: "devil" as const }],
      allowed_commands: ["a", "r", "d", "q"],
      real_world_basis: "Le froid réduit les rendements.",
      future_pressure: "Préparer une isolation testable.",
    };
    const html = renderToStaticMarkup(<ValidationOverlay prompt={prompt} sendCommand={async () => true} onClose={() => undefined} />);
    expect(html).toContain("Fondement réel");
    expect(html).toContain("Pression future");
    expect(html).not.toContain("Aucune évolution");
  });

  test("reprend automatiquement une fenêtre résolue mais jamais une garde non résolue", () => {
    const complete: ValidationPrompt = {
      kind: "feature",
      stage: "complete",
      day: 3,
      simulation_cycle: 7200,
      requests: [],
      allowed_commands: ["o", "q"],
    };
    const waiting = { ...complete, stage: "confirm" as const, selected_request_id: request.request_id, requests: [request], allowed_commands: ["a", "r", "q"] };

    expect(automaticContinuationKey(complete, null)).toBe("validation:feature:7200:complete");
    expect(automaticContinuationKey(waiting, null)).toBeNull();
  });

  test("reprend aussi après une évolution terminale, sans offrir un bouton de reprise", () => {
    const completion: EvolutionCompletion = {
      stage: "timed_out",
      request_id: "request-1",
      message: "Le délai de suivi est dépassé",
      detail: "Dieu peut encore démarrer dans le daemon.",
      elapsed_seconds: 900,
      successful: false,
      allowed_commands: ["o", "q"],
    };
    expect(automaticContinuationKey(null, completion)).toBe("completion:request-1:timed_out");
    expect(automaticContinuationKey(null, { ...completion, allowed_commands: ["q"] })).toBeNull();
  });
});
