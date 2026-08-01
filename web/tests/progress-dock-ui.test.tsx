import { describe, expect, test } from "bun:test";
import { renderToStaticMarkup } from "react-dom/server";
import { ProgressDock } from "../src/components/ProgressDock";
import type { EvolutionCompletion, ValidationPrompt } from "../src/protocol";

describe("cartes de progression", () => {
  test("résume génération, validation, compilation et échec sans bloquer la carte", () => {
    const validation: ValidationPrompt = {
      kind: "feature",
      stage: "confirm",
      day: 3,
      simulation_cycle: 7200,
      requests: [],
      allowed_commands: ["a", "r", "q"],
    };
    const completion: EvolutionCompletion = {
      stage: "failed",
      request_id: "request-7",
      message: "La vérification a échoué.",
      detail: "Détail interne à ne pas afficher dans le dock.",
      elapsed_seconds: 12,
      successful: false,
      allowed_commands: ["o", "q"],
    };
    const html = renderToStaticMarkup(
      <ProgressDock
        activity={{ kind: "evolution_request", agent_id: "ada", agent_name: "Ada", call_number: 2, total_calls: 6, elapsed_ms: 1_200 }}
        validation={validation}
        evolution={null}
        recompilation={{ stage: "compiling", elapsed_ms: 800, detail: "" }}
        completion={completion}
      />,
    );

    expect(html).toContain("Génération");
    expect(html).toContain("Validation");
    expect(html).toContain("Compilation");
    expect(html).toContain("Échec");
    expect(html).not.toContain("Workflow de Dieu");
    expect(html).not.toContain("Dernier retour");
    expect(html).not.toContain("Détail interne");
  });

  test("n’affiche aucune carte lorsque le moteur n’a rien signalé", () => {
    const html = renderToStaticMarkup(
      <ProgressDock activity={null} validation={null} evolution={null} recompilation={null} completion={null} />,
    );
    expect(html).toBe("");
  });
});
