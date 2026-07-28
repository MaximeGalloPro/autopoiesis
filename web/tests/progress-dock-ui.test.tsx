import { describe, expect, test } from "bun:test";
import { renderToStaticMarkup } from "react-dom/server";
import { ProgressDock } from "../src/components/ProgressDock";

describe("cartes de progression", () => {
  test("rend les demandes et soutiens signalés par le moteur sans compléter leur contenu", () => {
    const request = "Demande à Dieu à valider req-7 : Construire un abri";
    const support = "Insistance IA enregistree pour req-7 par Ada";
    const html = renderToStaticMarkup(
      <ProgressDock activity={null} evolution={null} recompilation={null} recentEvents={[request, support]} />,
    );

    expect(html).toContain("Événements récents");
    expect(html).toContain("Nouvelle demande");
    expect(html).toContain("Soutien enregistré");
    expect(html).toContain(request);
    expect(html).toContain(support);
  });

  test("n’affiche aucune carte lorsque le moteur n’a rien signalé", () => {
    const html = renderToStaticMarkup(
      <ProgressDock activity={null} evolution={null} recompilation={null} recentEvents={[]} />,
    );
    expect(html).toBe("");
  });
});
