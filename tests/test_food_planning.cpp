#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <cassert>

using namespace apo;

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& agent(Simulation& simulation) { return simulation.agents_.front(); }
  static Perception perceive(Simulation& simulation,Agent& agent) {
    return simulation.perceive(agent);
  }
  static std::string execute(Simulation& simulation,Agent& agent,const Decision& decision) {
    return simulation.execute(agent,decision);
  }
};
}

static Decision action(std::string name) {
  return {DecisionType::Action,std::move(name),json::object(),"test"};
}

int main() {
  World world(42);
  const Position fire{13,2};
  assert(world.place_campfire(fire));
  assert(world.store_food(fire,FoodItem{FoodType::Berries,35,false,0,3}));
  assert(world.store_food(fire,FoodItem{FoodType::Fish,45,false,0,2}));
  assert(world.raw_stored_food(fire)==2);
  assert(world.cook_stored_food(fire));
  assert(world.raw_stored_food(fire)==1);
  assert(world.cooked_stored_food(fire)==1);

  FoodItem preferred;
  assert(world.take_stored_food(fire,&preferred,{FoodType::Berries}));
  assert(preferred.type==FoodType::Berries);
  assert(!preferred.cooked);

  const auto checkpoint=world.checkpoint();
  World restored(7);
  restored.restore_checkpoint(checkpoint);
  assert(restored.cooked_stored_food(fire)==1);
  assert(restored.age_stored_food()==0);
  assert(restored.stored_food(fire)==1);
  assert(restored.age_stored_food()==0);
  assert(restored.stored_food(fire)==1);
  assert(restored.age_stored_food()==0);
  assert(restored.stored_food(fire)==1);
  assert(restored.age_stored_food()==0);
  assert(restored.stored_food(fire)==1);
  assert(restored.age_stored_food()==1);
  assert(restored.stored_food(fire)==0);

  Logger logger("/tmp/autopoiesis-food-planning-tests");
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42,decider,logger);
  auto& simulation_world=SimulationTestAccess::world(simulation);
  auto& agent=SimulationTestAccess::agent(simulation);
  const Position simulation_fire{13,2};
  assert(simulation_world.place_campfire(simulation_fire));
  assert(simulation_world.store_food(simulation_fire,FoodItem{FoodType::Berries,35}));
  agent.position={12,2};
  agent.hunger=55;
  auto actions=available_actions(agent,simulation_world,simulation.agents());
  assert(std::ranges::find(actions,"cook_camp_food")!=actions.end());
  const auto cook=decider.decide(SimulationTestAccess::perceive(simulation,agent));
  assert(cook.action=="cook_camp_food");
  assert(SimulationTestAccess::execute(simulation,agent,cook)==
         "cuit une ration au camp");
  assert(simulation_world.cooked_stored_food(simulation_fire)==1);
  const auto meal=decider.decide(SimulationTestAccess::perceive(simulation,agent));
  assert(meal.action=="eat_camp_food");
  assert(SimulationTestAccess::execute(simulation,agent,meal)=="mange une reserve du camp");
  assert(agent.hunger<55);
}
