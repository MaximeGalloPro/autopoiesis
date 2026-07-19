import { describe, expect, test } from "bun:test";
import { renderToStaticMarkup } from "react-dom/server";
import { ValidationOverlay } from "../src/components/ValidationOverlay";
import type { ValidationPrompt } from "../src/protocol";

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
  test("rappelle qu’une seule proposition est traitée et offre aucune/arrêt", () => {
    const prompt: ValidationPrompt = {
      kind: "feature",
      stage: "choose",
      day: 3,
      simulation_cycle: 7200,
      requests: [request],
      allowed_commands: ["1", "n", "q"],
    };
    const html = renderToStaticMarkup(<ValidationOverlay prompt={prompt} sendCommand={async () => true} />);
    expect(html).toContain("Les autres resteront pending");
    expect(html).toContain("Aucune évolution");
    expect(html).toContain("Arrêter le run");
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
    const html = renderToStaticMarkup(<ValidationOverlay prompt={prompt} sendCommand={async () => true} />);
    expect(html).toContain("Fondement réel");
    expect(html).toContain("Pression future");
    expect(html).not.toContain("Aucune évolution");
  });
});
