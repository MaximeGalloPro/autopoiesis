import { describe, expect, test } from "bun:test";
import { readFileSync } from "node:fs";

const sceneSource = readFileSync(new URL("../src/components/WorldScene.tsx", import.meta.url), "utf8");

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
});
