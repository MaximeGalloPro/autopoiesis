#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <unistd.h>

using namespace apo;

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& agent(Simulation& simulation) { return simulation.agents_.front(); }
  static Perception perceive(Simulation& simulation,Agent& agent) { return simulation.perceive(agent); }
  static std::string execute(Simulation& simulation,Agent& agent,const Decision& decision) {
    return simulation.execute(agent,decision);
  }
};
}

static Decision action(std::string name,json parameters=json::object()) {
  return {DecisionType::Action,std::move(name),std::move(parameters),"test"};
}

int main() {
  const auto root=std::filesystem::path("/tmp")/("autopoiesis-tools-"+std::to_string(getpid()));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto checkpoint=root/"state.json";
  Logger logger(root.string());
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42,decider,logger,nullptr,checkpoint.string());
  auto& world=SimulationTestAccess::world(simulation);
  auto& agent=SimulationTestAccess::agent(simulation);

  const Position tree{6,1};
  agent.position=tree;
  auto actions=available_actions(agent,world,simulation.agents());
  assert(std::ranges::find(actions,"harvest_wood")==actions.end());
  assert(SimulationTestAccess::execute(simulation,agent,action("harvest_wood"))=="hache requise");
  assert(world.living_trees(tree)==1);

  Position ore{};
  for(int y=0;y<World::height;++y)for(int x=0;x<World::width;++x)
    if(world.iron_ore({x,y})>=2)ore={x,y};
  agent.position=ore;
  actions=available_actions(agent,world,simulation.agents());
  assert(std::ranges::find(actions,"collect_iron_ore")!=actions.end());
  assert(SimulationTestAccess::execute(simulation,agent,action("collect_iron_ore"))==
         "ramasse du minerai de fer");
  assert(SimulationTestAccess::execute(simulation,agent,action("collect_iron_ore"))==
         "ramasse du minerai de fer");
  assert(agent.iron_ore_inventory==2);

  const Position fire{13,2};
  assert(world.place_campfire(fire));
  const auto before_failed_equip=world.checkpoint();
  assert(!world.take_stored_item(fire,CraftItem::Axe));
  assert(world.checkpoint()==before_failed_equip);
  assert(world.store_materials(fire,5,0,0));
  agent.position={12,2};
  assert(SimulationTestAccess::execute(simulation,agent,action("deposit_materials"))==
         "depose des materiaux au camp");
  assert(world.stored_iron_ore(fire)==2);
  const auto planned_handle=decider.decide(SimulationTestAccess::perceive(simulation,agent));
  assert(planned_handle.action=="craft_camp_item");
  assert(planned_handle.parameters["recipe"]=="wooden_handle");
  assert(SimulationTestAccess::execute(simulation,agent,planned_handle)=="fabrique wooden_handle");
  assert(SimulationTestAccess::execute(simulation,agent,action("craft_camp_item",{{"recipe","charcoal"}}))=="fabrique charcoal");
  assert(SimulationTestAccess::execute(simulation,agent,action("craft_camp_item",{{"recipe","iron_ingot"}}))=="fabrique iron_ingot");
  assert(SimulationTestAccess::execute(simulation,agent,action("craft_camp_item",{{"recipe","axe"}}))=="fabrique axe");
  assert(world.stored_item(fire,CraftItem::Axe)==1);

  actions=available_actions(agent,world,simulation.agents());
  assert(std::ranges::find(actions,"equip_axe")!=actions.end());
  const auto equip=decider.decide(SimulationTestAccess::perceive(simulation,agent));
  assert(equip.action=="equip_axe");
  assert(SimulationTestAccess::execute(simulation,agent,equip)=="equipe une hache");
  assert(agent.equipped_tool&&agent.equipped_tool->type==CraftItem::Axe);
  assert(world.stored_item(fire,CraftItem::Axe)==0);

  agent.position=tree;
  assert(SimulationTestAccess::execute(simulation,agent,action("harvest_wood"))=="preleve du bois");
  assert(world.living_trees(tree)==0);
  assert(agent.equipped_tool->durability==19);

  agent.equipped_tool->durability=0;
  actions=available_actions(agent,world,simulation.agents());
  assert(std::ranges::find(actions,"harvest_wood")==actions.end());
  agent.position={12,2};
  const auto deposit_wood=decider.decide(SimulationTestAccess::perceive(simulation,agent));
  assert(deposit_wood.action=="deposit_materials");
  assert(SimulationTestAccess::execute(simulation,agent,deposit_wood)==
         "depose des materiaux au camp");
  const auto repair=decider.decide(SimulationTestAccess::perceive(simulation,agent));
  assert(repair.action=="repair_axe");
  assert(SimulationTestAccess::execute(simulation,agent,repair)=="repare sa hache");
  assert(agent.equipped_tool->durability==10);

  simulation.save_checkpoint();
  std::mt19937 restored_rng(7);
  LocalDecider restored_decider(restored_rng);
  Logger restored_logger(root.string());
  Simulation restored(7,restored_decider,restored_logger,nullptr,checkpoint.string());
  assert(restored.agents().front().equipped_tool->durability==10);
  assert(restored.world().stored_iron_ore(fire)==0);
}
