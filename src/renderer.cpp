#include "autopoiesis/renderer.hpp"

namespace apo {
void render(const CalendarDate& date, int simulation_cycle, const ClimateState& climate, const World& world,
            const std::vector<Agent>& agents, const Logger& logger) {
  std::cout << "\033[2J\033[H\nAUTOPOIESIS — Année " << date.year
            << " | Mois " << date.month << " | Jour " << date.day_of_month
            << " | " << season_name(date.season) << " | " << climate.temperature_c
            << " °C, " << climate.condition << "\nJour absolu " << date.absolute_day
            << " | Cycle elementaire " << simulation_cycle << "\n\n"
            << world.ascii(agents) << '\n';
  for (const auto& agent : agents) {
    std::cout << agent.name << "  santé " << agent.health << " | faim "
              << agent.hunger << " | soif " << agent.thirst << " | fatigue " << agent.fatigue
              << (agent.alive ? "" : " | mort") << '\n';
  }
  std::cout << "\nDerniers événements :\n";
  for (const auto& line : logger.recent()) std::cout << "- " << line << '\n';
  std::cout.flush();
}
}
