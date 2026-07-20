import { describe, expect, test } from "bun:test";
import { AiServicesManager } from "../server/ai-services";
import type { EngineCommand } from "../src/protocol";

describe("supervision des services IA", () => {
  test("active l’API dans le moteur et Codex seulement avec son vérificateur", () => {
    const commands: EngineCommand[] = [];
    let killed = false;
    let spawned = 0;
    const manager = new AiServicesManager(
      { send(command) { commands.push(command); return true; } },
      {
        environment: {
          PATH: process.env.PATH,
          LLM_API_KEY: "test-key",
          OPENAI_MODEL: "test-model",
          USE_API: "0",
        },
        codexPath: "/bin/true",
        codexAuthenticated: true,
        dockerAvailable: true,
        spawn: () => {
          spawned += 1;
          return { pid: 42, exited: new Promise(() => undefined), kill() { killed = true; } };
        },
      },
    );

    expect(manager.snapshot().api).toMatchObject({ available: true, enabled: false });
    expect(manager.setEnabled("api", true)).toEqual({ accepted: true });
    expect(commands).toEqual([{ type: "service.api", enabled: true }]);
    expect(manager.setEnabled("codex", true)).toEqual({ accepted: true });
    expect(manager.snapshot().codex.enabled).toBe(true);
    expect(spawned).toBe(1);
    manager.stop();
    expect(killed).toBe(true);
  });

  test("refuse le workflow complet quand Docker est absent", () => {
    const manager = new AiServicesManager(
      { send() { return true; } },
      {
        environment: { PATH: process.env.PATH },
        codexPath: "/bin/true",
        codexAuthenticated: true,
        dockerAvailable: false,
      },
    );
    expect(manager.setEnabled("codex", true).accepted).toBe(false);
    expect(manager.snapshot().codex.detail).toContain("Docker");
  });
});
