#include "autopoiesis/renderer.hpp"

namespace apo {
void render(int day, int simulation_cycle, const World& world,
            const std::vector<Agent>& agents, const Logger& logger) {
  std::cout << "\033[2J\033[H\nAUTOPOIESIS — Jour " << day
            << " | Cycle elementaire " << simulation_cycle << "\n\n"
            << world.ascii(agents) << '\n';
  for (const auto& agent : agents) {
    std::cout << agent.name << "  santé " << agent.health << " | faim "
              << agent.hunger << " | fatigue " << agent.fatigue
              << (agent.alive ? "" : " | mort") << '\n';
  }
  std::cout << "\nDerniers événements :\n";
  for (const auto& line : logger.recent()) std::cout << "- " << line << '\n';
  std::cout.flush();
}
}
