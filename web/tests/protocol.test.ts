import { describe, expect, test } from "bun:test";
import { parseBackendEvent, toricDistance } from "../src/protocol";
import { worldSnapshot } from "./fixtures";

describe("protocole du moteur", () => {
  test("n’accepte que les lignes préfixées contenant un instantané canonique", () => {
    const snapshot = worldSnapshot();
    expect(parseBackendEvent(`AUTOPOIESIS_EVENT ${JSON.stringify({ version: 1, type: "snapshot", payload: snapshot })}`))
      .toEqual({ type: "state", payload: snapshot });
    expect(parseBackendEvent(JSON.stringify({ version: 1, type: "snapshot", payload: snapshot }))).toBeNull();
    expect(parseBackendEvent("AUTOPOIESIS_EVENT pas-du-json")).toBeNull();
    expect(parseBackendEvent(`AUTOPOIESIS_EVENT ${JSON.stringify({ version: 1, type: "snapshot", payload: { ...snapshot, width: 41 } })}`)).toBeNull();
    expect(parseBackendEvent(`AUTOPOIESIS_EVENT ${JSON.stringify({ version: 1, type: "unknown", payload: snapshot })}`)).toBeNull();
  });

  test("normalise exactement le fil C++ sans inventer d’état local", () => {
    const runtime = {
      state: "paused" as const,
      paused: true,
      speed: 4 as const,
      delay_ms: 900,
      message: "command_accepted",
    };
    expect(parseBackendEvent(`AUTOPOIESIS_EVENT ${JSON.stringify({ version: 1, type: "status", payload: runtime })}`))
      .toEqual({ type: "runtime", payload: runtime });

    const snapshot = worldSnapshot();
    const rawSnapshot = {
      ...snapshot,
      date: { ...snapshot.date, season: "printemps" },
      agents: [{ state: { id: "ada", name: "Ada" }, mood: "curieuse", available_actions: ["explore"] }],
      paused: undefined,
      speed: undefined,
      delay_ms: undefined,
    };
    const event = parseBackendEvent(
      `AUTOPOIESIS_EVENT ${JSON.stringify({ version: 1, type: "snapshot", payload: rawSnapshot })}`,
      runtime,
    );
    expect(event?.type).toBe("state");
    if (event?.type !== "state") throw new Error("instantané C++ non normalisé");
    expect(event.payload).toMatchObject({
      date: { season: "spring" },
      paused: true,
      speed: 4,
      delay_ms: 900,
      agents: [{ id: "ada", name: "Ada", mood: "curieuse", available_actions: ["explore"] }],
    });
  });

  test("mesure la distance sur le tore 40 × 24", () => {
    expect(toricDistance({ x: 0, y: 0 }, { x: 39, y: 0 })).toBe(1);
    expect(toricDistance({ x: 4, y: 0 }, { x: 4, y: 23 })).toBe(1);
    expect(toricDistance({ x: 2, y: 2 }, { x: 7, y: 8 })).toBe(11);
  });
});
