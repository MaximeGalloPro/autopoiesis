import { createHash, timingSafeEqual } from "node:crypto";
import { extname, resolve, sep } from "node:path";
import { Elysia } from "elysia";
import type { BackendEvent, EngineCommand } from "../src/protocol";
import { BROWSER_TRANSPORT_PREFIX } from "../src/transport";
import { BackendProcessManager } from "./backend-process";
import type { AiServiceController, AiServiceName } from "./ai-services";
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

type BasicAuth = {
  username: string;
  password: string;
};

function sameCredential(candidate: string, expected: string): boolean {
  const candidateDigest = createHash("sha256").update(candidate).digest();
  const expectedDigest = createHash("sha256").update(expected).digest();
  return timingSafeEqual(candidateDigest, expectedDigest);
}

function hasValidBasicAuth(request: Request, credentials: BasicAuth): boolean {
  const authorization = request.headers.get("authorization");
  if (!authorization?.startsWith("Basic ")) return false;
  try {
    const decoded = Buffer.from(authorization.slice(6), "base64").toString("utf8");
    const separator = decoded.indexOf(":");
    if (separator < 0) return false;
    return sameCredential(decoded.slice(0, separator), credentials.username)
      && sameCredential(decoded.slice(separator + 1), credentials.password);
  } catch {
    return false;
  }
}

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

function mountServiceRoutes(
  app: Elysia,
  prefix: string,
  services: AiServiceController,
  safePreview: boolean,
): void {
  app
    .get(`${prefix}/services`, () => services.snapshot())
    .post(`${prefix}/services`, ({ body, set }) => {
      if (!body || typeof body !== "object" || Array.isArray(body)) {
        set.status = 422;
        return { accepted: false, error: "Configuration de service invalide." };
      }
      const candidate = body as Record<string, unknown>;
      const service = candidate.service;
      if (Object.keys(candidate).length !== 2 || (service !== "api" && service !== "codex")
        || typeof candidate.enabled !== "boolean") {
        set.status = 422;
        return { accepted: false, error: "Service ou état inconnu." };
      }
      if (safePreview && service === "codex" && candidate.enabled) {
        set.status = 403;
        return { accepted: false, error: "Codex est désactivé par la politique de preview sûre." };
      }
      const result = services.setEnabled(service as AiServiceName, candidate.enabled);
      if (!result.accepted) set.status = 409;
      return { ...result, services: services.snapshot() };
    });
}

export function createApp(
  manager: BackendProcessManager,
  options: {
    serveStatic?: boolean;
    distDirectory?: string;
    safePreview?: boolean;
    snapshotIntervalMs?: number;
    basicAuth?: BasicAuth | false;
    services?: AiServiceController;
  } = {},
) {
  const sockets = new Map<string, () => void>();
  const serveStatic = options.serveStatic ?? true;
  const distDirectory = options.distDirectory ?? resolve(import.meta.dir, "../dist");
  const safePreview = options.safePreview ?? process.env.AUTOPOIESIS_SAFE_PREVIEW === "1";
  const configuredInterval = Number.parseInt(process.env.AUTOPOIESIS_WEB_SNAPSHOT_MS ?? "100", 10);
  const snapshotIntervalMs = options.snapshotIntervalMs
    ?? (Number.isFinite(configuredInterval) ? Math.max(16, Math.min(1_000, configuredInterval)) : 100);
  const configuredPassword = process.env.BASIC_AUTH_PASSWORD;
  const basicAuth = options.basicAuth === undefined
    ? configuredPassword
      ? {
          username: process.env.BASIC_AUTH_USERNAME || "autopoiesis",
          password: configuredPassword,
        }
      : false
    : options.basicAuth;

  const app = new Elysia();
  if (basicAuth) {
    app.onRequest(({ request }) => {
      if (hasValidBasicAuth(request, basicAuth)) return;
      return new Response("Authentification requise.", {
        status: 401,
        headers: {
          "cache-control": "no-store",
          "www-authenticate": 'Basic realm="Autopoiesis", charset="UTF-8"',
        },
      });
    });
  }
  mountTransportRoutes(app, BROWSER_TRANSPORT_PREFIX, manager, safePreview);
  mountTransportRoutes(app, "/api", manager, safePreview);
  if (options.services) {
    mountServiceRoutes(app, BROWSER_TRANSPORT_PREFIX, options.services, safePreview);
    mountServiceRoutes(app, "/api", options.services, safePreview);
  }

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
