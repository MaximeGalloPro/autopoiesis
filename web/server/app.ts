import { extname, resolve, sep } from "node:path";
import { Elysia } from "elysia";
import type { BackendEvent, EngineCommand } from "../src/protocol";
import { BROWSER_TRANSPORT_PREFIX } from "../src/transport";
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

export function createEventRelay(
  send: (event: BackendEvent) => void,
  snapshotIntervalMs = 100,
): { accept: (event: BackendEvent) => void; close: () => void } {
  let timer: ReturnType<typeof setTimeout> | null = null;
  let pendingState: Extract<BackendEvent, { type: "state" }> | null = null;
  let closed = false;

  const flush = () => {
    timer = null;
    const event = pendingState;
    pendingState = null;
    if (!closed && event) send(event);
  };

  return {
    accept(event) {
      if (closed) return;
      if (event.type === "state") {
        pendingState = event;
        if (timer === null) timer = setTimeout(flush, snapshotIntervalMs);
        return;
      }
      // Une garde ou un statut important ne reste jamais derrière un rendu
      // périmé : le dernier monde est envoyé d'abord, puis l'événement.
      if (timer !== null) clearTimeout(timer);
      flush();
      send(event);
    },
    close() {
      closed = true;
      pendingState = null;
      if (timer !== null) clearTimeout(timer);
      timer = null;
    },
  };
}

function mountTransportRoutes(
  app: Elysia,
  prefix: string,
  manager: BackendProcessManager,
  safePreview: boolean,
): void {
  app
    .get(`${prefix}/health`, () => {
      const snapshot = manager.snapshot();
      return {
        status: snapshot.engine.status === "running" ? "ok" : "degraded",
        engine: snapshot.engine,
        has_state: snapshot.state !== null,
      };
    })
    .get(`${prefix}/state`, ({ set }) => {
      const snapshot = manager.snapshot();
      if (!snapshot.state) set.status = 503;
      return snapshot;
    })
    .post(`${prefix}/commands`, ({ body, set }) => {
      if (!isEngineCommand(body)) {
        set.status = 422;
        return { accepted: false, error: "Commande inconnue, mal paramétrée ou hors limites." };
      }
      if (safePreview && body.type === "validation.decision" && body.decision === "approve") {
        set.status = 403;
        return { accepted: false, error: "L’approbation réelle est désactivée dans cette preview publique." };
      }
      if (!manager.send(body)) {
        set.status = 503;
        return { accepted: false, error: "Le moteur n’est pas disponible." };
      }
      set.status = 202;
      return { accepted: true };
    });
}

export function createApp(
  manager: BackendProcessManager,
  options: {
    serveStatic?: boolean;
    distDirectory?: string;
    safePreview?: boolean;
    snapshotIntervalMs?: number;
  } = {},
) {
  const sockets = new Map<string, () => void>();
  const serveStatic = options.serveStatic ?? true;
  const distDirectory = options.distDirectory ?? resolve(import.meta.dir, "../dist");
  const safePreview = options.safePreview ?? process.env.AUTOPOIESIS_SAFE_PREVIEW === "1";
  const configuredInterval = Number.parseInt(process.env.AUTOPOIESIS_WEB_SNAPSHOT_MS ?? "100", 10);
  const snapshotIntervalMs = options.snapshotIntervalMs
    ?? (Number.isFinite(configuredInterval) ? Math.max(16, Math.min(1_000, configuredInterval)) : 100);

  const app = new Elysia();
  mountTransportRoutes(app, BROWSER_TRANSPORT_PREFIX, manager, safePreview);
  mountTransportRoutes(app, "/api", manager, safePreview);

  return app
    .ws("/ws", {
      open(ws) {
        ws.send(JSON.stringify({ type: "snapshot", payload: manager.snapshot() }));
        const relay = createEventRelay((event) => {
          ws.send(JSON.stringify({ type: "event", payload: event }));
        }, snapshotIntervalMs);
        const unsubscribe = manager.subscribe((event) => {
          relay.accept(event);
        });
        sockets.set(ws.id, () => {
          relay.close();
          unsubscribe();
        });
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
      if (path.startsWith("/api/") || path.startsWith(`${BROWSER_TRANSPORT_PREFIX}/`)) {
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
