import { describe, expect, test } from "bun:test";
import { advancePosition, interpolatePosition } from "../src/lib/smoothPosition";

describe("interpolation des déplacements visuels", () => {
  test("ne téléporte pas un personnage vers sa nouvelle cible", () => {
    const next = interpolatePosition({ x: 0, y: 0, z: 0 }, { x: 1, y: 0, z: 0 }, 1 / 60);

    expect(next.x).toBeGreaterThan(0);
    expect(next.x).toBeLessThan(1);
  });

  test("converge vers la cible sans la dépasser", () => {
    let current = { x: 0, y: 0, z: 0 };
    for (let frame = 0; frame < 120; frame += 1) {
      current = interpolatePosition(current, { x: 4, y: 0, z: -2 }, 1 / 60);
    }

    expect(current.x).toBeCloseTo(4, 3);
    expect(current.z).toBeCloseTo(-2, 3);
  });

  test("ignore un delta négatif et limite les gros sauts d'horloge", () => {
    expect(interpolatePosition({ x: 2, y: 0, z: 0 }, { x: 4, y: 0, z: 0 }, -1).x).toBe(2);
    const afterPause = interpolatePosition({ x: 0, y: 0, z: 0 }, { x: 10, y: 0, z: 0 }, 5);
    expect(afterPause.x).toBeLessThan(10);
  });

  test("avance à vitesse constante et s'arrête exactement sur la cible", () => {
    const first = advancePosition({ x: 0, y: 0, z: 0 }, { x: 1, y: 0, z: 0 }, 1 / 60);
    expect(first.x).toBeCloseTo(3.2 / 60, 5);
    expect(advancePosition({ x: 0.98, y: 0, z: 0 }, { x: 1, y: 0, z: 0 }, 1 / 60)).toEqual({ x: 1, y: 0, z: 0 });
  });
});
