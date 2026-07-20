import { createApp } from "./app";
import { BackendProcessManager } from "./backend-process";
import { AiServicesManager } from "./ai-services";

const manager = new BackendProcessManager({ binaryArgs: process.argv.slice(2) });
const services = new AiServicesManager(manager);

if (import.meta.main) {
  const port = Number.parseInt(process.env.PORT ?? "3000", 10);
  void manager.start();
  services.startConfigured();
  const app = createApp(manager, { services }).listen({ hostname: "0.0.0.0", port });
  console.info(`Autopoiesis web écoute sur http://${app.server?.hostname}:${app.server?.port}`);

  const shutdown = () => {
    manager.stop();
    services.stop();
    void app.stop();
  };
  process.on("SIGINT", shutdown);
  process.on("SIGTERM", shutdown);
}
