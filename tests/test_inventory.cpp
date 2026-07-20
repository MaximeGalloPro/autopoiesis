#include "autopoiesis/simulation.hpp"
#include "autopoiesis/feature_request.hpp"

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

int main() {
  Agent inventory;
  inventory.attributes.strength=0;
  assert(inventory_capacity(inventory)==4);
  inventory.attributes.strength=50;
  assert(inventory_capacity(inventory)==6);
  inventory.attributes.strength=100;
  assert(inventory_capacity(inventory)==8);
  inventory.attributes.strength=1000;
  assert(inventory_capacity(inventory)==10);
  inventory.attributes.strength=50;
  inventory.wood_inventory=2;
  inventory.branch_inventory=3;
  inventory.carried_food=FoodItem{FoodType::Berries,35};
  assert(inventory_load(inventory)==6);
  assert(inventory_full(inventory));

  Logger logger("/tmp/autopoiesis-inventory-tests");
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42,decider,logger);
  auto& agent=SimulationTestAccess::agent(simulation);
  auto& world=SimulationTestAccess::world(simulation);
  Position branches{};
  for(int y=0;y<World::height;++y)for(int x=0;x<World::width;++x)
    if(world.branches({x,y})>0)branches={x,y};
  agent.position=branches;
  agent.wood_inventory=inventory_capacity(agent);
  const auto perception=SimulationTestAccess::perceive(simulation,agent);
  assert(perception.value["self"]["inventory_load"]==inventory_capacity(agent));
  assert(perception.value["self"]["inventory_capacity"]==inventory_capacity(agent));
  const int branches_before=world.branches(branches);
  const auto actions=available_actions(agent,world,simulation.agents());
  assert(std::ranges::find(actions,"collect_branch")==actions.end());
  assert(SimulationTestAccess::execute(simulation,agent,
      {DecisionType::Action,"collect_branch",json::object(),"test"})=="inventaire plein");
  assert(world.branches(branches)==branches_before);

  const auto mechanisms=active_world_mechanisms();
  assert(std::ranges::any_of(mechanisms,[](const json& mechanism){
    return mechanism.value("key","")=="bounded_inventory";
  }));
}
