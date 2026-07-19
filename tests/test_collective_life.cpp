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
  static Perception perceive(Simulation& simulation,Agent& agent) { return simulation.perceive(agent); }
  static std::string execute(Simulation& simulation,Agent& agent,const Decision& decision) {
    return simulation.execute(agent,decision);
  }
  static void set_time(Simulation& simulation,int day,int cycle_in_day) {
    simulation.day_=day;
    simulation.date_=date_from_absolute_day(day);
    simulation.cycle_in_day_=cycle_in_day;
  }
};
}

static Decision action(std::string name) {
  return {DecisionType::Action,std::move(name),json::object(),"test"};
}

int main() {
  const auto root=std::filesystem::path("/tmp")/
      ("autopoiesis-collective-life-"+std::to_string(getpid()));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto checkpoint=root/"state.json";
  Logger logger(root.string());
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42,decider,logger,nullptr,checkpoint.string());
  auto& world=SimulationTestAccess::world(simulation);
  auto& ada=SimulationTestAccess::agent(simulation,0);
  auto& borin=SimulationTestAccess::agent(simulation,1);
  auto& cyra=SimulationTestAccess::agent(simulation,2);
  const Position fire{13,2};
  assert(world.place_campfire(fire));
  ada.position={12,2};
  borin.position={13,1};
  cyra.position={14,2};
  SimulationTestAccess::set_time(simulation,4,2000);
  (void)SimulationTestAccess::perceive(simulation,ada);
  (void)SimulationTestAccess::perceive(simulation,borin);
  assert(!ada.community_role.empty());
  assert(!borin.community_role.empty());
  assert(ada.community_role!=borin.community_role);

  assert(world.store_food(fire,FoodItem{FoodType::Berries,35,true,0,6}));
  assert(world.store_food(fire,FoodItem{FoodType::Fish,45,true,0,5}));
  ada.hunger=70;
  borin.hunger=65;
  auto actions=available_actions(ada,world,simulation.agents(),4,DayPhase::Night);
  assert(std::ranges::find(actions,"share_camp_meal")!=actions.end());
  assert(std::ranges::find(actions,"hold_vigil")!=actions.end());
  auto day_actions=available_actions(ada,world,simulation.agents(),4,DayPhase::Day);
  assert(std::ranges::find(day_actions,"hold_vigil")==day_actions.end());
  std::string error;
  assert(!validate_decision(action("hold_vigil"),ada,world,simulation.agents(),error,
                            4,DayPhase::Day));
  const auto meal_decision=decider.decide(SimulationTestAccess::perceive(simulation,ada));
  assert(meal_decision.action=="share_camp_meal");
  assert(SimulationTestAccess::execute(simulation,ada,meal_decision)==
         "partage un repas avec Borin");
  assert(ada.hunger<70&&borin.hunger<65);
  assert(world.stored_food(fire)==0);
  assert(ada.last_shared_meal_day==4&&borin.last_shared_meal_day==4);

  const int trust_before=ada.relationships[borin.id].trust;
  const auto vigil_decision=decider.decide(SimulationTestAccess::perceive(simulation,ada));
  assert(vigil_decision.action=="hold_vigil");
  assert(SimulationTestAccess::execute(simulation,ada,vigil_decision)==
         "veille avec Borin");
  assert(ada.relationships[borin.id].trust>trust_before);
  assert(ada.last_vigil_day==4&&borin.last_vigil_day==4);
  actions=available_actions(ada,world,simulation.agents(),4,DayPhase::Night);
  assert(std::ranges::find(actions,"hold_vigil")==actions.end());

  ada.celebration_pending=true;
  ada.boredom=50;
  assert(SimulationTestAccess::execute(simulation,ada,action("celebrate"))==
         "celebre avec le groupe");
  assert(!ada.celebration_pending);
  assert(ada.boredom<50);

  cyra.alive=false;
  assert(SimulationTestAccess::execute(simulation,ada,action("mourn"))==
         "honore la memoire de Cyra");
  assert(ada.mourned_agents.contains(cyra.id));

  simulation.save_checkpoint();
  std::mt19937 resumed_rng(7);
  LocalDecider resumed_decider(resumed_rng);
  Logger resumed_logger(root.string());
  Simulation resumed(7,resumed_decider,resumed_logger,nullptr,checkpoint.string());
  const auto& restored=resumed.agents().front();
  assert(restored.community_role==ada.community_role);
  assert(restored.last_shared_meal_day==4);
  assert(restored.last_vigil_day==4);
  assert(restored.mourned_agents.contains(cyra.id));
}
