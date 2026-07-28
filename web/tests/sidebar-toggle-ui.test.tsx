import { describe, expect, test } from "bun:test";
import { renderToStaticMarkup } from "react-dom/server";
import { SidePanelToggle } from "../src/components/SidePanelToggle";

describe("bouton du panneau latéral", () => {
  test("annonce explicitement les états réduit et déplié sans retirer le contenu du flux", () => {
    const expanded = renderToStaticMarkup(
      <SidePanelToggle expanded onToggle={() => undefined} panelId="observatory-inspector" />,
    );
    const collapsed = renderToStaticMarkup(
      <SidePanelToggle expanded={false} onToggle={() => undefined} panelId="observatory-inspector" />,
    );

    expect(expanded).toContain('aria-controls="observatory-inspector"');
    expect(expanded).toContain('aria-expanded="true"');
    expect(expanded).toContain("Replier le panneau d’observation");
    expect(collapsed).toContain('aria-expanded="false"');
    expect(collapsed).toContain("Déplier le panneau d’observation");
    expect(collapsed).toContain("Ouvrir");
  });
});
