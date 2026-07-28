import { describe, expect, test } from "bun:test";
import { readFileSync } from "node:fs";
import { renderToStaticMarkup } from "react-dom/server";
import { Inspector } from "../src/components/Inspector";
import { ObservatoryNavigation, observatoryViews } from "../src/components/ObservatoryNavigation";
import { worldSnapshot } from "./fixtures";

const snapshot = worldSnapshot({
  climate: { temperature_c: 8, rainfall_mm: 7, condition: "Pluie fine" },
  cells: [{
    position: { x: 12, y: 8 },
    terrain: "ground",
    food: 0,
    wood: 0,
    fibers: 0,
    shelter_level: 1,
    branches: 0,
    campfire: true,
    stored_food: 4,
  }],
  recent_events: ["Ada a rejoint le foyer."],
});

describe("coquille de l’observatoire", () => {
  test("réserve le viewport à la carte et transforme le panneau en tiroir", () => {
    const styles = readFileSync(new URL("../src/styles.css", import.meta.url), "utf8");

    expect(styles).toContain("height: 100dvh !important");
    expect(styles).toContain("overflow: hidden !important");
    expect(styles).toContain(".app-shell:not(.observation-mode) .side-panel");
    expect(styles).toContain("position: absolute !important");
    expect(styles).toContain("transform: translateX(100%) !important");
  });

  test("propose les cinq lectures du monde avec une section courante explicite", () => {
    const html = renderToStaticMarkup(
      <ObservatoryNavigation activeView="maps" onChange={() => undefined} />,
    );

    expect(observatoryViews).toEqual(["characters", "camp", "history", "maps", "world"]);
    expect(html).toContain('aria-label="Sections de l’observatoire"');
    expect(html).toContain("Personnages");
    expect(html).toContain("Foyer");
    expect(html).toContain("Histoire");
    expect(html).toContain("Cartes");
    expect(html).toContain("Monde");
    expect(html).toContain('aria-current="page"');
  });

  test("présente les lectures Foyer, Histoire, Cartes et Monde sans modifier l’instantané", () => {
    const renderView = (view: "camp" | "history" | "maps" | "world") => renderToStaticMarkup(
      <Inspector snapshot={snapshot} selected={null} onSelect={() => undefined} view={view} />,
    );

    expect(renderView("camp")).toContain("Réserve commune");
    expect(renderView("camp")).toContain("4 rations");
    expect(renderView("history")).toContain("Ada a rejoint le foyer.");
    expect(renderView("maps")).toContain("Un monde sans bord.");
    expect(renderView("world")).toContain("Pluie fine · 8 °C");
  });
});
