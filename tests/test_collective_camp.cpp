#include "autopoiesis/simulation.hpp"

#include <cassert>

using namespace apo;

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& agent(Simulation& simulation,std::size_t index) {
    return simulation.agents_.at(index);
  }
  static Perception perceive(Simulation& simulation,Agent& agent) {
    return simulation.perceive(agent);
  }
};
}

int main() {
  Logger logger("/tmp/autopoiesis-collective-camp-tests");
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42,decider,logger);
  auto& world=SimulationTestAccess::world(simulation);
  auto& ada=SimulationTestAccess::agent(simulation,0);
  auto& borin=SimulationTestAccess::agent(simulation,1);
  const Position fire{8,5};
  assert(world.place_campfire(fire));
  assert(world.primary_campfire()==fire);
  assert(!world.place_campfire({20,20}));
  auto legacy_world=world.checkpoint();
  legacy_world.erase("primary_campfire");
  World migrated(7);
  migrated.restore_checkpoint(legacy_world);
  assert(migrated.primary_campfire()==fire);
  ada.position={7,5};
  borin.position={9,5};

  SimulationTestAccess::perceive(simulation,ada);
  SimulationTestAccess::perceive(simulation,borin);
  assert(ada.home_camp==fire);
  assert(borin.home_camp==fire);
  assert(ada.camp_rest_position.has_value());
  assert(borin.camp_rest_position.has_value());
  assert(ada.camp_rest_position!=borin.camp_rest_position);
  assert(world.adjacent(*ada.camp_rest_position,fire));
  assert(world.adjacent(*borin.camp_rest_position,fire));

  auto& cyra=SimulationTestAccess::agent(simulation,2);
  cyra.position={30,20};
  SimulationTestAccess::perceive(simulation,cyra);
  assert(cyra.home_camp==fire);

  auto perception=SimulationTestAccess::perceive(simulation,ada);
  const auto ada_view=perception.value["self"];
  assert(ada_view["home_camp"]==json({{"x",8},{"y",5}}));
  assert(!ada_view["camp_rest_position"].is_null());
  perception.value["time"]={{"phase","night"},{"cycles_per_day",2400},{"cycles_until_night",0}};
  const auto return_to_place=decider.decide(perception);
  assert(return_to_place.action=="move");
  assert(return_to_place.reason=="Je rejoins le feu de camp connu pour la nuit.");
}
