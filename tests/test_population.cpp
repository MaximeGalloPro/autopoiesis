#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <unistd.h>

using namespace apo;

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& agent(Simulation& simulation,std::size_t index) { return simulation.agents_.at(index); }
  static void update_population(Simulation& simulation) { simulation.update_population(); }
  static void set_day(Simulation& simulation,int day) { simulation.day_=day;simulation.date_=date_from_absolute_day(day); }
};
}

static void stock_food(World& world,Position camp,int amount) {
  for(int index=0;index<amount;++index)
    assert(world.store_food(camp,FoodItem{FoodType::Roots,25,false,0,5}));
}

int main() {
  const auto root=std::filesystem::path("/tmp")/("autopoiesis-population-"+std::to_string(getpid()));
  std::filesystem::remove_all(root);std::filesystem::create_directories(root);
  const auto checkpoint=root/"state.json";
  Logger logger(root.string());std::mt19937 rng(42);LocalDecider decider(rng);
  Simulation simulation(42,decider,logger,nullptr,checkpoint.string());
  auto& world=SimulationTestAccess::world(simulation);
  const Position camp{13,2};assert(world.place_campfire(camp));
  assert(world.create_shelter({14,2}));stock_food(world,camp,40);
  for(std::size_t index=0;index<simulation.agents().size();++index){
    auto& agent=SimulationTestAccess::agent(simulation,index);agent.position={14,2};agent.age_days=25*360;
  }

  const auto initial=simulation.agents().size();
  SimulationTestAccess::set_day(simulation,60);SimulationTestAccess::update_population(simulation);
  assert(simulation.agents().size()==initial+1);
  const auto& newcomer=simulation.agents().back();
  assert(newcomer.origin=="arrival"&&newcomer.arrival_day==60&&newcomer.age_days>=18*360);

  SimulationTestAccess::set_day(simulation,90);SimulationTestAccess::update_population(simulation);
  assert(simulation.agents().size()==initial+2);
  const auto& child=simulation.agents().back();
  assert(child.origin=="birth"&&child.age_days==0&&child.parent_ids.size()==2);
  const auto child_actions=available_actions(child,world,simulation.agents(),90,DayPhase::Day);
  assert(std::ranges::find(child_actions,"hunt_animal")==child_actions.end());
  assert(std::ranges::find(child_actions,"confront")==child_actions.end());
  const auto child_parents=child.parent_ids;

  auto& departing=SimulationTestAccess::agent(simulation,1);
  departing.critical_hunger_days=4;departing.relationships.clear();
  SimulationTestAccess::set_day(simulation,120);SimulationTestAccess::update_population(simulation);
  assert(!departing.alive&&departing.departure_day==120&&!departing.departure_reason.empty());

  auto& elder=SimulationTestAccess::agent(simulation,0);elder.age_days=80*360-1;
  SimulationTestAccess::set_day(simulation,121);SimulationTestAccess::update_population(simulation);
  assert(!elder.alive&&elder.death_cause=="vieillesse");

  simulation.save_checkpoint();
  Logger restored_logger(root.string());std::mt19937 restored_rng(7);LocalDecider restored_decider(restored_rng);
  Simulation restored(7,restored_decider,restored_logger,nullptr,checkpoint.string());
  assert(restored.agents().size()==simulation.agents().size());
  assert(std::ranges::any_of(restored.agents(),[&](const Agent& agent){
    return agent.origin=="birth"&&agent.parent_ids==child_parents;
  }));
}
