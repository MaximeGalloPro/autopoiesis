#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <cassert>

using namespace apo;

struct WaitDecider final : IDecider {
  Decision decide(const Perception&) override { return {}; }
};

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& ada(Simulation& simulation) { return simulation.agents_.front(); }
  static std::string execute(Simulation& simulation, Agent& agent,
                             const Decision& decision) {
    const Agent before = agent;
    const auto result = simulation.execute(agent, decision);
    simulation.update_behavior_after_action(agent, before, decision, result,
                                            result == "construit un abri" ||
                                                result == "ameliore un abri");
    return result;
  }
};
}

int main() {
  WaitDecider decider;
  Logger logger("/tmp/autopoiesis-shelter-tests");
  Simulation simulation(42, decider, logger);
  auto& world = SimulationTestAccess::world(simulation);
  auto& ada = SimulationTestAccess::ada(simulation);
  const Position site = ada.position;
  const Decision build{DecisionType::Action, "build_shelter", json::object(),
                       "Je construis mon foyer."};
  std::string error;

  world.add_materials(site, 3, 2);
  auto actions = available_actions(ada, world, simulation.agents());
  assert(std::find(actions.begin(), actions.end(), "build_shelter") != actions.end());
  assert(validate_decision(build, ada, world, simulation.agents(), error));
  assert(SimulationTestAccess::execute(simulation, ada, build) == "construit un abri");
  assert(world.wood(site) == 0);
  assert(world.fibers(site) == 0);
  assert(world.shelter_level(site) == 1);

  world.add_materials(site, 3, 2);
  assert(SimulationTestAccess::execute(simulation, ada, build) == "ameliore un abri");
  assert(world.wood(site) == 0);
  assert(world.fibers(site) == 0);
  assert(world.shelter_level(site) == 2);

  const int level_before_failure = world.shelter_level(site);
  world.add_materials(site, 2, 1);
  assert(!validate_decision(build, ada, world, simulation.agents(), error));
  assert(SimulationTestAccess::execute(simulation, ada, build) ==
         "materiaux insuffisants pour construire");
  assert(world.wood(site) == 2);
  assert(world.fibers(site) == 1);
  assert(world.shelter_level(site) == level_before_failure);

  ada.project.status = ProjectStatus::Blocked;
  ada.project.blocked_reason = "La capacite de construire un abri manque.";
  ada.project.missing_capability = "build_shelter";
  world.add_materials(site, 3, 2);
  assert(SimulationTestAccess::execute(simulation, ada, build) == "ameliore un abri");
  assert(ada.project.status == ProjectStatus::Active);
  assert(ada.project.blocked_reason.empty());
  assert(ada.project.missing_capability.empty());
}
