#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <cassert>

using namespace apo;

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& first_agent(Simulation& simulation) { return simulation.agents_.front(); }
  static std::string execute(Simulation& simulation, Agent& agent, const Decision& decision) {
    return simulation.execute(agent, decision);
  }
};
}

static Perception night_perception(Position self, int branch_inventory,
                                   const std::vector<std::string>& actions) {
  return Perception{json{
      {"world_width", 40}, {"world_height", 24},
      {"time", {{"phase", "night"}, {"cycle_in_day", 2000},
                {"cycles_until_night", 0}}},
      {"self", {{"id", "camper"}, {"x", self.x}, {"y", self.y},
                {"hunger", 20}, {"thirst", 20}, {"fatigue", 35},
                {"branch_inventory", branch_inventory},
                {"personality", {{"curiosity", 50}}},
                {"attributes", {{"focus", 60}, {"willpower", 50},
                                {"endurance", 50}, {"spatial_sense", 60}}}}},
      {"available_actions", actions}, {"cells", json::array()},
      {"known_map", json::array({
          {{"x", 5}, {"y", 5}, {"status", "traversable"}, {"campfire", false}},
          {{"x", 6}, {"y", 5}, {"status", "traversable"}, {"campfire", false}},
          {{"x", 7}, {"y", 5}, {"status", "traversable"}, {"campfire", true}}})}}};
}

int main() {
  World world(42);
  Position branch_position{};
  bool found_branches=false;
  for(int y=0;y<World::height&&!found_branches;++y)
    for(int x=0;x<World::width&&!found_branches;++x)
      if(world.branches({x,y})>0){branch_position={x,y};found_branches=true;}
  assert(found_branches);
  assert(std::ranges::any_of(world.neighbors(branch_position),[&](Position neighbor){
    return world.terrain(neighbor)==Terrain::Tree;
  }));
  const int branches_before=world.branches(branch_position);
  assert(world.take_branch(branch_position));
  assert(world.branches(branch_position)==branches_before-1);
  assert(world.place_campfire(branch_position));
  assert(world.campfire(branch_position));

  auto legacy_checkpoint=world.checkpoint();
  for(auto& cell:legacy_checkpoint["construction"]){
    cell.erase("loose_branches");
    cell.erase("campfire");
  }
  World migrated_world(7);
  migrated_world.restore_checkpoint(legacy_checkpoint);
  assert(std::ranges::any_of(migrated_world.neighbors(branch_position),[&](Position neighbor){
    return migrated_world.terrain(neighbor)==Terrain::Tree;
  }));
  bool migrated_branches=false;
  for(int y=0;y<World::height&&!migrated_branches;++y)
    for(int x=0;x<World::width&&!migrated_branches;++x)
      migrated_branches=migrated_world.branches({x,y})>0;
  assert(migrated_branches);

  Logger logger("/tmp/autopoiesis-campfire-tests");
  std::mt19937 wait_rng(42);
  LocalDecider wait_decider(wait_rng);
  Simulation simulation(42,wait_decider,logger);
  auto& simulation_world=SimulationTestAccess::world(simulation);
  auto& agent=SimulationTestAccess::first_agent(simulation);
  Position available_branch{};
  for(int y=0;y<World::height;++y)for(int x=0;x<World::width;++x)
    if(simulation_world.branches({x,y})>0)available_branch={x,y};
  agent.position=available_branch;
  const auto actions=available_actions(agent,simulation_world,simulation.agents());
  assert(std::ranges::find(actions,"collect_branch")!=actions.end());
  assert(SimulationTestAccess::execute(simulation,agent,
      {DecisionType::Action,"collect_branch",json::object(),"Je ramasse une branche."})==
      "ramasse une branche");
  agent.branch_inventory=3;
  assert(SimulationTestAccess::execute(simulation,agent,
      {DecisionType::Action,"build_campfire",json::object(),"Je prépare le camp."})==
      "allume un feu de camp");
  assert(agent.branch_inventory==0);

  std::mt19937 rng(42);
  LocalDecider decider(rng);
  const auto approach=decider.decide(night_perception({5,5},0,{"observe","move","wait"}));
  assert(approach.action=="move");
  assert(approach.parameters["direction"]=="east");
  assert(approach.reason=="Je rejoins le feu de camp connu pour la nuit.");
  const auto settle=decider.decide(night_perception(
      {6,5},0,{"observe","move","wait","rest_by_campfire"}));
  assert(settle.action=="rest_by_campfire");
}
