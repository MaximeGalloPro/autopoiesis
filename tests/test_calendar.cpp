#include "autopoiesis/calendar.hpp"
#include "autopoiesis/simulation.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>

using namespace apo;

struct WaitDecider final : IDecider {
  Decision decide(const Perception&) override {
    return {DecisionType::Action, "wait", json::object(), "test"};
  }
};

namespace apo {
struct SimulationTestAccess {
  static void apply_climate(Simulation& simulation, Agent& agent,
                            const CalendarDate& date,
                            const ClimateState& climate) {
    simulation.apply_climate_effects(agent, date, climate);
  }
  static World& world(Simulation& simulation) { return simulation.world_; }
};
}

int main() {
  assert(daylight_cycles(2400) == 1800);
  assert(night_cycles(2400) == 600);
  assert(day_phase_for(1, 2400) == DayPhase::Day);
  assert(day_phase_for(1800, 2400) == DayPhase::Day);
  assert(day_phase_for(1801, 2400) == DayPhase::Night);
  assert(day_phase_for(2400, 2400) == DayPhase::Night);
  assert((date_from_absolute_day(1) == CalendarDate{1, 1, 1, 1, Season::Spring}));
  assert((date_from_absolute_day(30) == CalendarDate{30, 30, 1, 1, Season::Spring}));
  assert((date_from_absolute_day(31) == CalendarDate{31, 1, 2, 1, Season::Spring}));
  assert((date_from_absolute_day(91) == CalendarDate{91, 1, 4, 1, Season::Summer}));
  assert((date_from_absolute_day(181) == CalendarDate{181, 1, 7, 1, Season::Autumn}));
  assert((date_from_absolute_day(271) == CalendarDate{271, 1, 10, 1, Season::Winter}));
  assert((date_from_absolute_day(360) == CalendarDate{360, 30, 12, 1, Season::Winter}));
  assert((date_from_absolute_day(361) == CalendarDate{361, 1, 1, 2, Season::Spring}));

  const auto summer = climate_for(date_from_absolute_day(94));
  assert(summer == climate_for(date_from_absolute_day(94)));
  assert(summer.temperature_c >= 25);
  assert(!summer.condition.empty());

  World world(42);
  const Position berries{14, 2};
  assert(world.berries(berries) == 8);
  assert(world.eat_berries(berries));
  world.apply_climate(date_from_absolute_day(4), climate_for(date_from_absolute_day(4)));
  assert(world.berries(berries) == 8);
  world.apply_climate(date_from_absolute_day(280), climate_for(date_from_absolute_day(280)));
  assert(world.berries(berries) == 7);

  setenv("CYCLES_PER_DAY", "1", 1);
  setenv("REPORT_EVERY_DAYS", "1000", 1);
  const std::filesystem::path directory = "/tmp/autopoiesis-calendar-tests";
  std::filesystem::remove_all(directory);
  WaitDecider decider;
  Logger logger(directory.string());
  Simulation simulation(42, decider, logger);

  Agent exposed{"exposed", "Exposé", {3, 2}};
  exposed.hunger = 20;
  exposed.fatigue = 20;
  const auto winter_date = date_from_absolute_day(280);
  const auto winter = climate_for(winter_date);
  SimulationTestAccess::apply_climate(simulation, exposed, winter_date, winter);
  assert(exposed.hunger > 20);
  assert(exposed.fatigue >= 24);

  Agent protected_agent{"protected", "Protégé", {4, 2}};
  protected_agent.hunger = 20;
  protected_agent.fatigue = 20;
  assert(SimulationTestAccess::world(simulation).create_shelter(protected_agent.position));
  SimulationTestAccess::apply_climate(simulation, protected_agent, winter_date, winter);
  assert(protected_agent.fatigue < exposed.fatigue);

  Agent hot{"hot", "Chaud", {5, 2}};
  hot.thirst = 20;
  SimulationTestAccess::apply_climate(simulation, hot, date_from_absolute_day(94), summer);
  assert(hot.thirst > 20);

  simulation.run(31, 0, 0);
  assert(simulation.date().year == 1);
  assert(simulation.date().month == 2);
  assert(simulation.date().day_of_month == 1);

  unsetenv("CYCLES_PER_DAY");
  unsetenv("REPORT_EVERY_DAYS");
}
