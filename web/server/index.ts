import { resolve } from "node:path";
import { createApp } from "./app";
import { BackendProcessManager } from "./backend-process";
import { AiServicesManager } from "./ai-services";
import {
  CardCycleCoordinator,
  JsonFileCardCycleStore,
  cardRequestCooldownMsFromEnvironment,
} from "./card-cycle";
import { BackendCardCycleBridge } from "./card-cycle-runtime";

const manager = new BackendProcessManager({ binaryArgs: process.argv.slice(2) });
const services = new AiServicesManager(manager);
const dataDirectory = process.env.AUTOPOIESIS_DATA_DIR ?? resolve(manager.projectRoot, "data");
const cardCycle = new CardCycleCoordinator({
  cooldownMs: cardRequestCooldownMsFromEnvironment(process.env),
  store: new JsonFileCardCycleStore(resolve(dataDirectory, "card-cycle-state.json")),
});
const cardCycleBridge = new BackendCardCycleBridge(manager, cardCycle);

if (import.meta.main) {
  const port = Number.parseInt(process.env.PORT ?? "3000", 10);
  void manager.start();
  services.startConfigured();
  const app = createApp(manager, { services, cardCycle, cardCycleBridge }).listen({ hostname: "0.0.0.0", port });
  console.info(`Autopoiesis web écoute sur http://${app.server?.hostname}:${app.server?.port}`);

  const shutdown = () => {
    manager.stop();
    cardCycleBridge.stop();
    services.stop();
    void app.stop();
  };
  process.on("SIGINT", shutdown);
  process.on("SIGTERM", shutdown);
}
