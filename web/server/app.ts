import { createHash, randomBytes, timingSafeEqual } from "node:crypto";
import { extname, resolve, sep } from "node:path";
import { Elysia } from "elysia";
import { isCardCycleCommand, type BackendEvent, type EngineCommand } from "../src/protocol";
import { BROWSER_TRANSPORT_PREFIX } from "../src/transport";
import { BackendProcessManager } from "./backend-process";
import type { AiServiceController, AiServiceName } from "./ai-services";
import { CardCycleCoordinator } from "./card-cycle";
import type { BackendCardCycleBridge } from "./card-cycle-runtime";
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

type PasswordAuth = {
  password: string;
};

const AUTH_LOGIN_PATH = "/__auth/login";
const AUTH_LOGOUT_PATH = "/__auth/logout";
const AUTH_COOKIE = "autopoiesis_session";
const AUTH_MAX_AGE_SECONDS = 60 * 60 * 24 * 30;

function sameCredential(candidate: string, expected: string): boolean {
  const candidateDigest = createHash("sha256").update(candidate).digest();
  const expectedDigest = createHash("sha256").update(expected).digest();
  return timingSafeEqual(candidateDigest, expectedDigest);
}

function cookieValue(request: Request, name: string): string | null {
  const cookieHeader = request.headers.get("cookie") ?? "";
  for (const part of cookieHeader.split(";")) {
    const separator = part.indexOf("=");
    if (separator < 0) continue;
    const key = part.slice(0, separator).trim();
    if (key === name) return part.slice(separator + 1).trim();
  }
  return null;
}

function hasValidSession(request: Request, sessions: Set<string>): boolean {
  const token = cookieValue(request, AUTH_COOKIE);
  return token !== null && sessions.has(token);
}

function loginPage(error = ""): Response {
  const message = error ? `<p class="error">${error}</p>` : "";
  return new Response(`<!doctype html>
<html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Autopoiesis · Accès</title>
<style>body{margin:0;min-height:100vh;display:grid;place-items:center;background:#08120f;color:#f1ead8;font:16px system-ui,sans-serif}main{width:min(360px,calc(100% - 40px));padding:28px;border:1px solid #30483c;border-radius:14px;background:#101f19;box-sizing:border-box}h1{margin:0 0 8px;font:28px Georgia,serif}p{color:#afc1b3}label{display:block;margin:20px 0 7px;font-size:13px;color:#dcb35c}input{width:100%;box-sizing:border-box;padding:12px;border:1px solid #536b5d;border-radius:8px;background:#07110d;color:#fff;font-size:16px}button{width:100%;margin-top:16px;padding:12px;border:0;border-radius:8px;background:#dcb35c;color:#101810;font-weight:700;font-size:16px}.error{color:#f1a39a}</style></head>
<body><main><h1>Autopoiesis</h1><p>Entrez le mot de passe pour accéder à l’observatoire.</p>${message}<form method="post" action="${AUTH_LOGIN_PATH}"><label for="password">Mot de passe</label><input id="password" name="password" type="password" autocomplete="current-password" autofocus required><button type="submit">Accéder</button></form></main></body></html>`, {
    status: error ? 401 : 200,
    headers: { "content-type": "text/html; charset=utf-8", "cache-control": "no-store" },
  });
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
  onAcceptedCommand?: (command: EngineCommand) => Promise<void>,
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
    .post(`${prefix}/commands`, async ({ body, set }) => {
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
      await onAcceptedCommand?.(body);
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

/**
 * Ces routes ne transmettent aucune commande au monde : elles consignent la
 * décision humaine qui libère (ou non) le prochain lot de propositions.
 */
function mountCardCycleRoutes(
  app: Elysia,
  prefix: string,
  cycle: CardCycleCoordinator,
  safePreview: boolean,
  onStateChange?: () => Promise<void>,
): void {
  app
    .get(`${prefix}/card-cycle`, () => cycle.snapshot())
    .post(`${prefix}/card-cycle/commands`, async ({ body, set }) => {
      if (!isCardCycleCommand(body)) {
        set.status = 422;
        return { accepted: false, error: "Commande de cycle de cartes inconnue ou mal formée." };
      }
      if (safePreview && body.type === "card_decision" && body.decision === "approve") {
        set.status = 403;
        return { accepted: false, error: "L’approbation réelle est désactivée dans cette preview publique." };
      }
      const accepted = body.type === "acknowledge_call_alert"
        ? await cycle.acknowledgeCallAlert()
        : body.type === "acknowledge_failure"
          ? await cycle.acknowledgeFailure()
        : await cycle.recordCardDecision(body.card_id, body.decision);
      if (!accepted) {
        set.status = 409;
        return { accepted: false, error: "Commande incompatible avec l’état courant du cycle de cartes." };
      }
      await onStateChange?.();
      set.status = 202;
      return { accepted: true, card_cycle: await cycle.snapshot() };
    });
}

export function createApp(
  manager: BackendProcessManager,
  options: {
    serveStatic?: boolean;
    distDirectory?: string;
    safePreview?: boolean;
    snapshotIntervalMs?: number;
    passwordAuth?: PasswordAuth | false;
    services?: AiServiceController;
    cardCycle?: CardCycleCoordinator;
    cardCycleBridge?: BackendCardCycleBridge;
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
  const passwordAuth = options.passwordAuth === undefined
    ? configuredPassword
      ? {
          password: configuredPassword,
        }
      : false
    : options.passwordAuth;
  const sessions = new Set<string>();

  const app = new Elysia();
  if (passwordAuth) {
    app.post(AUTH_LOGIN_PATH, async ({ request, set }) => {
      const form = await request.formData().catch(() => null);
      const password = form?.get("password");
      if (typeof password !== "string" || !sameCredential(password, passwordAuth.password)) {
        set.status = 401;
        return loginPage("Mot de passe incorrect.");
      }
      const token = randomBytes(32).toString("hex");
      sessions.add(token);
      set.status = 303;
      set.headers.location = "/";
      set.headers["set-cookie"] = `${AUTH_COOKIE}=${token}; Path=/; HttpOnly; SameSite=Lax; Max-Age=${AUTH_MAX_AGE_SECONDS}`;
      return "";
    });
    app.get(AUTH_LOGOUT_PATH, ({ request, set }) => {
      const token = cookieValue(request, AUTH_COOKIE);
      if (token) sessions.delete(token);
      set.status = 303;
      set.headers.location = AUTH_LOGIN_PATH;
      set.headers["set-cookie"] = `${AUTH_COOKIE}=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0`;
      return "";
    });
    app.onRequest(({ request }) => {
      const path = new URL(request.url).pathname;
      if (path === AUTH_LOGIN_PATH || path === AUTH_LOGOUT_PATH) return;
      if (hasValidSession(request, sessions)) return;
      if (request.method === "GET" && path === "/") return loginPage();
      return new Response("Authentification requise.", {
        status: 401,
        headers: { "cache-control": "no-store" },
      });
    });
  }
  const recordCardCommand = options.cardCycleBridge
    ? (command: EngineCommand) => options.cardCycleBridge!.recordAcceptedCommand(command)
    : undefined;
  mountTransportRoutes(app, BROWSER_TRANSPORT_PREFIX, manager, safePreview, recordCardCommand);
  mountTransportRoutes(app, "/api", manager, safePreview, recordCardCommand);
  if (options.cardCycle) {
    const publishCardCycle = options.cardCycleBridge
      ? () => options.cardCycleBridge!.refresh()
      : undefined;
    mountCardCycleRoutes(app, BROWSER_TRANSPORT_PREFIX, options.cardCycle, safePreview, publishCardCycle);
    mountCardCycleRoutes(app, "/api", options.cardCycle, safePreview, publishCardCycle);
  }
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
