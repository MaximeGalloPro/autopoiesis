#include "autopoiesis/simulation.hpp"

#include <cassert>
#include <cstdlib>
#include <random>

using namespace apo;

int main() {
  setenv("CYCLES_PER_DAY", "240", 1);
  setenv("REPORT_EVERY_DAYS", "100", 1);

  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Logger logger("/tmp/autopoiesis-survival-ai-tests");
  Simulation simulation(42, decider, logger);
  simulation.run(6, 0, 0);

  for (const auto& agent : simulation.agents()) {
    assert(agent.alive);
    assert(agent.health >= 80);
    assert(agent.hunger < 85);
    assert(agent.thirst < 85);
    assert(agent.map_memory.size() >= 100);
  }

  unsetenv("CYCLES_PER_DAY");
  unsetenv("REPORT_EVERY_DAYS");
}
