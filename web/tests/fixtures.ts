import type { WorldSnapshot } from "../src/protocol";

export function worldSnapshot(overrides: Partial<WorldSnapshot> = {}): WorldSnapshot {
  return {
    date: { absolute_day: 3, day_of_month: 3, month: 1, year: 1, season: "spring" },
    simulation_cycle: 7200,
    climate: { temperature_c: 16, rainfall_mm: 2, condition: "Clair" },
    phase: "day",
    cycle_in_day: 1,
    cycles_per_day: 2400,
    width: 40,
    height: 24,
    cells: [],
    agents: [],
    animals: [],
    recent_events: [],
    paused: false,
    speed: 1,
    delay_ms: 500,
    ...overrides,
  };
}
