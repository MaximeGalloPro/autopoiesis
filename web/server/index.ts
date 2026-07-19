import { createApp } from "./app";
import { BackendProcessManager } from "./backend-process";

const manager = new BackendProcessManager();

if (import.meta.main) {
  const port = Number.parseInt(process.env.PORT ?? "3000", 10);
  void manager.start();
  const app = createApp(manager).listen({ hostname: "0.0.0.0", port });
  console.info(`Autopoiesis web écoute sur http://${app.server?.hostname}:${app.server?.port}`);

  const shutdown = () => {
    manager.stop();
    void app.stop();
  };
  process.on("SIGINT", shutdown);
  process.on("SIGTERM", shutdown);
}
