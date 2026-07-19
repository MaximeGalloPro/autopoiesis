import { describe, expect, test } from "bun:test";
import { parseBackendEvent, toricDistance } from "../src/protocol";
import { worldSnapshot } from "./fixtures";

describe("protocole du moteur", () => {
  test("n’accepte que les lignes préfixées contenant un instantané canonique", () => {
    const snapshot = worldSnapshot();
    expect(parseBackendEvent(`AUTOPOIESIS_EVENT ${JSON.stringify({ type: "state", payload: snapshot })}`))
      .toEqual({ type: "state", payload: snapshot });
    expect(parseBackendEvent(JSON.stringify({ type: "state", payload: snapshot }))).toBeNull();
    expect(parseBackendEvent("AUTOPOIESIS_EVENT pas-du-json")).toBeNull();
    expect(parseBackendEvent(`AUTOPOIESIS_EVENT ${JSON.stringify({ type: "state", payload: { ...snapshot, width: 41 } })}`)).toBeNull();
    expect(parseBackendEvent(`AUTOPOIESIS_EVENT ${JSON.stringify({ type: "unknown", payload: snapshot })}`)).toBeNull();
  });

  test("mesure la distance sur le tore 40 × 24", () => {
    expect(toricDistance({ x: 0, y: 0 }, { x: 39, y: 0 })).toBe(1);
    expect(toricDistance({ x: 4, y: 0 }, { x: 4, y: 23 })).toBe(1);
    expect(toricDistance({ x: 2, y: 2 }, { x: 7, y: 8 })).toBe(11);
  });
});
