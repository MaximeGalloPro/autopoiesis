#include "autopoiesis/simulation.hpp"
#include <algorithm>
#include <cassert>

using namespace apo;

struct FakeDecider final : IDecider {
  Decision decide(const Perception&) override { return {}; }
};

namespace apo {
struct SimulationTestAccess {
  static std::string execute(Simulation& simulation, Agent& agent,
                             const Decision& decision) {
    return simulation.execute(agent, decision);
  }
};
}

static bool has_rest(const Agent& agent, const World& world,
                     const std::vector<Agent>& agents) {
  const auto actions = available_actions(agent, world, agents);
  return std::find(actions.begin(), actions.end(), "rest") != actions.end();
}

int main() {
  FakeDecider decider;
  Logger logger("/tmp/autopoiesis-rest-tests");
  Simulation simulation(42, decider, logger);
  World world(42);
  Decision rest{DecisionType::Action, "rest", json::object(), "I need rest"};
  std::string error;

  Agent rested{"a", "Ada", {3, 2}, 100, 60, 65};
  rested.remember_map({3, 2}, world.terrain({3, 2}));
  rested.map_visit_counts[{3, 2}] = 4;
  const Agent before = rested;
  std::vector<Agent> occupants{rested};
  assert(has_rest(rested, world, occupants));
  assert(validate_decision(rest, rested, world, occupants, error));
  assert(SimulationTestAccess::execute(simulation, rested, rest) == "rested");
  assert(rested.fatigue == 45);
  assert(rested.position == before.position);
  assert(rested.health == before.health);
  assert(rested.hunger == before.hunger);
  assert(rested.map_memory == before.map_memory);
  assert(rested.map_visit_counts == before.map_visit_counts);

  Agent low{"a", "Ada", {3, 2}, 100, 60, 20};
  occupants = {low};
  assert(validate_decision(rest, low, world, occupants, error));
  assert(SimulationTestAccess::execute(simulation, low, rest) == "rested");
  assert(low.fatigue == 0);

  Agent fresh{"a", "Ada", {3, 2}, 100, 60, 0};
  occupants = {fresh};
  const Agent fresh_before = fresh;
  assert(!has_rest(fresh, world, occupants));
  assert(!validate_decision(rest, fresh, world, occupants, error));
  assert(SimulationTestAccess::execute(simulation, fresh, rest) == "attend");
  assert(fresh.fatigue == fresh_before.fatigue);

  Agent dead{"a", "Ada", {3, 2}, 100, 60, 65};
  dead.alive = false;
  occupants = {dead};
  const Agent dead_before = dead;
  assert(!has_rest(dead, world, occupants));
  assert(!validate_decision(rest, dead, world, occupants, error));
  assert(SimulationTestAccess::execute(simulation, dead, rest) == "attend");
  assert(dead.fatigue == dead_before.fatigue);

  Agent first{"a", "Ada", {3, 2}, 100, 60, 65};
  Agent second = first;
  const auto first_result = SimulationTestAccess::execute(simulation, first, rest);
  const auto second_result = SimulationTestAccess::execute(simulation, second, rest);
  assert(first_result == second_result);
  assert(first.fatigue == second.fatigue);
  assert(first.position == second.position);
  assert(first.map_memory == second.map_memory);
}
