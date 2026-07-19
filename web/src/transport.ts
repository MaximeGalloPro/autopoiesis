/**
 * Nuagent réserve `/api/*` à son port API dédié. L'observatoire reste un
 * processus Elysia unique : ses appels navigateur utilisent donc un préfixe
 * servi par le port web, tandis que le serveur conserve `/api/*` comme alias
 * pour les lancements locaux et Docker.
 */
export const BROWSER_TRANSPORT_PREFIX = "/bridge";

export const browserTransportUrl = (resource: "state" | "commands" | "health") =>
  `${BROWSER_TRANSPORT_PREFIX}/${resource}`;
