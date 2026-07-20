import type { EngineCommand } from "../src/protocol";

function hasExactKeys(value: Record<string, unknown>, expected: string[]): boolean {
  const keys = Object.keys(value);
  return keys.length === expected.length && keys.every((key) => expected.includes(key));
}

function validRequestId(value: unknown): value is string {
  return typeof value === "string" && value.length >= 1 && value.length <= 256
    && /^[A-Za-z0-9._:-]+$/.test(value);
}

export function isEngineCommand(value: unknown): value is EngineCommand {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const command = value as Record<string, unknown>;
  switch (command.type) {
    case "control.pause":
      return hasExactKeys(command, ["type", "paused"]) && typeof command.paused === "boolean";
    case "control.speed":
      return hasExactKeys(command, ["type", "multiplier"])
        && [0.25, 0.5, 1, 2, 4].includes(command.multiplier as number);
    case "control.delay":
      return hasExactKeys(command, ["type", "milliseconds"])
        && Number.isInteger(command.milliseconds)
        && (command.milliseconds as number) >= 0
        && (command.milliseconds as number) <= 10_000;
    case "service.api":
      return hasExactKeys(command, ["type", "enabled"]) && typeof command.enabled === "boolean";
    case "validation.select":
      return hasExactKeys(command, ["type", "request_id"]) && validRequestId(command.request_id);
    case "validation.decision":
      return hasExactKeys(command, ["type", "request_id", "decision"])
        && validRequestId(command.request_id)
        && (command.decision === "approve" || command.decision === "reject");
    case "validation.back":
    case "validation.none":
    case "simulation.resume":
    case "simulation.stop":
      return hasExactKeys(command, ["type"]);
    default:
      return false;
  }
}
