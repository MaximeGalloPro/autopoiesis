import { extname, resolve, sep } from "node:path";
import { Elysia } from "elysia";
import type { EngineCommand } from "../src/protocol";
import { BackendProcessManager } from "./backend-process";
import { isEngineCommand } from "./command-schema";

const contentTypes: Record<string, string> = {
  ".css": "text/css; charset=utf-8",
  ".html": "text/html; charset=utf-8",
  ".ico": "image/x-icon",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".map": "application/json; charset=utf-8",
  ".png": "image/png",
  ".svg": "image/svg+xml",
  ".webp": "image/webp",
};

export function createApp(
  manager: BackendProcessManager,
  options: { serveStatic?: boolean; distDirectory?: string } = {},
) {
  const sockets = new Map<string, () => void>();
  const serveStatic = options.serveStatic ?? true;
  const distDirectory = options.distDirectory ?? resolve(import.meta.dir, "../dist");

  return new Elysia()
    .get("/api/health", () => {
      const snapshot = manager.snapshot();
      return {
        status: snapshot.engine.status === "running" ? "ok" : "degraded",
        engine: snapshot.engine,
        has_state: snapshot.state !== null,
      };
    })
    .get("/api/state", ({ set }) => {
      const snapshot = manager.snapshot();
      if (!snapshot.state) set.status = 503;
      return snapshot;
    })
    .post("/api/commands", ({ body, set }) => {
      if (!isEngineCommand(body)) {
        set.status = 422;
        return { accepted: false, error: "Commande inconnue, mal paramétrée ou hors limites." };
      }
      if (!manager.send(body)) {
        set.status = 503;
        return { accepted: false, error: "Le moteur n’est pas disponible." };
      }
      set.status = 202;
      return { accepted: true };
    })
    .ws("/ws", {
      open(ws) {
        ws.send(JSON.stringify({ type: "snapshot", payload: manager.snapshot() }));
        const unsubscribe = manager.subscribe((event) => {
          ws.send(JSON.stringify({ type: "event", payload: event }));
        });
        sockets.set(ws.id, unsubscribe);
      },
      close(ws) {
        sockets.get(ws.id)?.();
        sockets.delete(ws.id);
      },
      message() {
        // Le WebSocket est volontairement descendant. Les commandes passent par
        // le point HTTP strictement validé ci-dessus.
      },
    })
    .get("*", async ({ path, set }) => {
      if (path.startsWith("/api/")) {
        set.status = 404;
        return { error: "Point d’API inconnu." };
      }
      if (!serveStatic) {
        set.status = 404;
        return { error: "Not found" };
      }
      const requested = path === "/" ? "index.html" : path.replace(/^\/+/, "");
      const candidate = resolve(distDirectory, requested);
      const safePrefix = `${resolve(distDirectory)}${sep}`;
      let file = candidate.startsWith(safePrefix) ? Bun.file(candidate) : Bun.file("");
      if (!(await file.exists()) || path.endsWith("/")) file = Bun.file(resolve(distDirectory, "index.html"));
      if (!(await file.exists())) {
        set.status = 404;
        return "Application web non compilée. Exécutez bun run build dans web/.";
      }
      set.headers["cache-control"] = extname(file.name ?? "") === ".html"
        ? "no-cache"
        : "public, max-age=31536000, immutable";
      set.headers["content-type"] = contentTypes[extname(file.name ?? "")] ?? "application/octet-stream";
      return file;
    });
}
