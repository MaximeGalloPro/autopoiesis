import { describe, expect, test } from "bun:test";
import { readFileSync } from "node:fs";

const sceneSource = readFileSync(new URL("../src/components/WorldScene.tsx", import.meta.url), "utf8");
const entitiesSource = readFileSync(new URL("../src/components/entities/WorldEntities.tsx", import.meta.url), "utf8");

describe("scène du monde", () => {
  test("présente un sol continu sans repère de grille ni bandeau torique", () => {
    expect(sceneSource).toContain('<planeGeometry args={[40, 24]} />');
    expect(sceneSource).not.toContain("Grid");
    expect(sceneSource).not.toContain("torus-hint");
    expect(sceneSource).not.toContain("continuité torique");
  });

  test("déplace la carte au glisser gauche en vue du dessus", () => {
    expect(sceneSource).toContain("enablePan");
    expect(sceneSource).toContain("screenSpacePanning");
    expect(sceneSource).toContain("LEFT: MOUSE.PAN");
  });

  test("rend le feu de camp cliquable seulement lorsqu'une alerte est présente", () => {
    expect(sceneSource).toContain("campfireAlert");
    expect(sceneSource).toContain("onCampfireClick");
    expect(entitiesSource).toContain("hasAlert");
    expect(entitiesSource).toContain("onCampfireClick");
  });

  test("affiche une carte compacte avec le nom et l'action de chaque personnage", () => {
    expect(entitiesSource).toContain("AgentInfoCard");
    expect(entitiesSource).toContain("actionWordFor");
    expect(entitiesSource).toContain("agent-info-card");
    expect(entitiesSource).toContain('<div className="agent-info-card__name">{agent.name}</div>');
    expect(entitiesSource).toContain('<div className="agent-info-card__action">{actionWordFor(agent)}</div>');
  });
});
