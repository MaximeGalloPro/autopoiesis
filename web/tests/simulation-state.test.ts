import { describe, expect, test } from "bun:test";
import { applyEvent } from "../src/hooks/useSimulation";
import type { PublicState } from "../src/protocol";
import { worldSnapshot } from "./fixtures";

const staleState = (): PublicState => ({
  state: worldSnapshot({ simulation_cycle: 719 }),
  activity: {
    kind: "evolution_request",
    agent_id: "ada",
    agent_name: "Ada",
    call_number: 6,
    total_calls: 6,
    elapsed_ms: 1200,
  },
  validation: {
    kind: "feature",
    stage: "empty",
    day: 3,
    simulation_cycle: 720,
    requests: [],
    allowed_commands: ["o", "q"],
  },
  evolution: {
    stage: "complete",
    request_id: "request-1",
    message: "Activée",
    detail: "ok",
    elapsed_seconds: 12,
    successful: true,
  },
  evolution_completion: {
    stage: "complete",
    request_id: "request-1",
    message: "Activée",
    detail: "ok",
    elapsed_seconds: 12,
    successful: true,
    allowed_commands: ["o", "q"],
  },
  recompilation: { stage: "ready", elapsed_ms: 800, detail: "ok" },
  engine: { status: "running", pid: 42, restarts: 0, last_error: null },
  latest_event: null,
});

describe("projection temps réel", () => {
  test("un nouvel instantané referme les états transitoires de la fenêtre précédente", () => {
    const next = worldSnapshot({ simulation_cycle: 721 });
    const projected = applyEvent(staleState(), { type: "state", payload: next });
    expect(projected).toMatchObject({
      state: { simulation_cycle: 721 },
      activity: null,
      validation: null,
      evolution: null,
      evolution_completion: null,
      recompilation: null,
    });
  });

  test("une garde remplace la progression IA sans modifier l’instantané", () => {
    const current = staleState();
    const prompt = current.validation!;
    const projected = applyEvent(current, { type: "validation", payload: prompt });
    expect(projected.state).toBe(current.state);
    expect(projected.validation).toBe(prompt);
    expect(projected.activity).toBeNull();
  });
});
