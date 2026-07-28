import { describe, expect, test } from "bun:test";
import { renderToStaticMarkup } from "react-dom/server";
import { EvolutionCompletionOverlay, EvolutionCompletionReminder, ValidationOverlay, ValidationReminder } from "../src/components/ValidationOverlay";
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
  test("rappelle qu’une seule proposition est traitée et offre aucune/arrêt", () => {
    const prompt: ValidationPrompt = {
      kind: "feature",
      stage: "choose",
      day: 3,
      simulation_cycle: 7200,
      requests: [request],
      allowed_commands: ["1", "n", "q"],
    };
    const html = renderToStaticMarkup(<ValidationOverlay prompt={prompt} sendCommand={async () => true} onMinimize={() => undefined} />);
    expect(html).toContain("Les autres resteront pending");
    expect(html).toContain("Aucune évolution");
    expect(html).toContain("Arrêter le run");
    expect(html).toContain("Nouvelle demande · Ada");
    expect(html).toContain("Changement proposé");
    expect(html).toContain("Réduire la fenêtre de décision");
    expect(html).not.toContain("aria-modal=\"true\"");
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
    const html = renderToStaticMarkup(<ValidationOverlay prompt={prompt} sendCommand={async () => true} onMinimize={() => undefined} />);
    expect(html).toContain("Fondement réel");
    expect(html).toContain("Pression future");
    expect(html).not.toContain("Aucune évolution");
  });

  test("propose une reprise compacte sans masquer le monde", () => {
    const prompt: ValidationPrompt = {
      kind: "feature",
      stage: "empty",
      day: 3,
      simulation_cycle: 7200,
      requests: [],
      allowed_commands: ["o", "q"],
    };
    const html = renderToStaticMarkup(
      <ValidationReminder prompt={prompt} sendCommand={async () => true} onOpen={() => undefined} />,
    );
    expect(html).toContain("Simulation en attente");
    expect(html).toContain("Le monde reste entièrement consultable");
    expect(html).toContain("Reprendre");
    expect(html).toContain("Détails");
    expect(html).not.toContain("role=\"dialog\"");
  });

  test("explique clairement un délai dépassé sans présenter une évolution comme active", () => {
    const completion: EvolutionCompletion = {
      stage: "timed_out",
      request_id: "request-1",
      message: "Le délai de suivi est dépassé",
      detail: "Dieu peut encore démarrer dans le daemon.",
      elapsed_seconds: 900,
      successful: false,
      allowed_commands: ["o", "q"],
    };
    const html = renderToStaticMarkup(
      <EvolutionCompletionOverlay completion={completion} sendCommand={async () => true} onMinimize={() => undefined} />,
    );
    expect(html).toContain("L’évolution n’a pas été activée");
    expect(html).toContain("Aucun changement n’est actif");
    expect(html).toContain("Reprendre la partie");
    expect(html).toContain("Arrêter la partie");
    expect(html).not.toContain("Recompiler et reprendre");
    expect(html).not.toContain("Voulez-vous passer à l’étape suivante");
  });

  test("réserve la recompilation au transfert réellement réussi", () => {
    const completion: EvolutionCompletion = {
      stage: "complete",
      request_id: "request-1",
      message: "La nouvelle évolution est active",
      detail: "Une version vérifiée est prête.",
      elapsed_seconds: 12,
      successful: true,
      allowed_commands: ["o", "q"],
    };
    const html = renderToStaticMarkup(
      <EvolutionCompletionReminder completion={completion} sendCommand={async () => true} onOpen={() => undefined} />,
    );
    expect(html).toContain("Recompiler et reprendre");
    expect(html).toContain("Nouvelle évolution prête");
  });
});
