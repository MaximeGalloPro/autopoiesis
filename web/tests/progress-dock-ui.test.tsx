import { describe, expect, test } from "bun:test";
import { renderToStaticMarkup } from "react-dom/server";
import { ProgressDock } from "../src/components/ProgressDock";

describe("cartes de progression", () => {
  test("ne rend plus le fil du monde dans le dock flottant", () => {
    const request = "Demande à Dieu à valider req-7 : Construire un abri";
    const support = "Insistance IA enregistree pour req-7 par Ada";
    const html = renderToStaticMarkup(
      <ProgressDock activity={null} evolution={null} recompilation={null} recentEvents={[request, support]} />,
    );

    expect(html).toBe("");
  });

  test("n’affiche aucune carte lorsque le moteur n’a rien signalé", () => {
    const html = renderToStaticMarkup(
      <ProgressDock activity={null} evolution={null} recompilation={null} recentEvents={[]} />,
    );
    expect(html).toBe("");
  });
});
