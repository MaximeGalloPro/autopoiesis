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
  static std::string execute(Simulation& simulation,Agent& agent,const Decision& decision) { return simulation.execute(agent,decision); }
  static void set_day(Simulation& simulation,int day) { simulation.day_=day;simulation.date_=date_from_absolute_day(day); }
};
}

static Decision action(std::string name,const std::string& target) {
  return {DecisionType::Action,std::move(name),{{"target_id",target}},"test"};
}

int main() {
  const auto root=std::filesystem::path("/tmp")/("autopoiesis-relations-"+std::to_string(getpid()));
  std::filesystem::remove_all(root);std::filesystem::create_directories(root);
  const auto checkpoint=root/"state.json";
  Logger logger(root.string());std::mt19937 rng(42);LocalDecider decider(rng);
  Simulation simulation(42,decider,logger,nullptr,checkpoint.string());
  SimulationTestAccess::set_day(simulation,5);
  auto& ada=SimulationTestAccess::agent(simulation,0);
  auto& borin=SimulationTestAccess::agent(simulation,1);
  ada.position={10,10};borin.position={11,10};borin.fatigue=85;
  ada.observed_animals.insert("wolf");

  auto available=available_actions(ada,simulation.world(),simulation.agents(),5,DayPhase::Day);
  assert(std::ranges::find(available,"warn_danger")!=available.end());
  assert(std::ranges::find(available,"help_companion")!=available.end());
  assert(std::ranges::find(available,"accompany")!=available.end());
  std::string error;
  assert(validate_decision(action("warn_danger",borin.id),ada,simulation.world(),simulation.agents(),error,5,DayPhase::Day));
  assert(SimulationTestAccess::execute(simulation,ada,action("warn_danger",borin.id))=="avertit Borin du danger");
  assert(borin.observed_animals.contains("wolf"));
  assert(ada.relationships[borin.id].trust>0);

  const int fatigue_before=borin.fatigue;
  assert(SimulationTestAccess::execute(simulation,ada,action("help_companion",borin.id))=="aide Borin à récupérer");
  assert(borin.fatigue<fatigue_before);

  assert(SimulationTestAccess::execute(simulation,ada,action("accompany",borin.id))=="accompagne Borin");
  assert(ada.companion_id==borin.id&&ada.companion_until_day==6);

  ada.relationships[borin.id].affinity=10;
  assert(SimulationTestAccess::execute(simulation,ada,action("confront",borin.id))=="se dispute avec Borin");
  assert(ada.relationships[borin.id].conflict);
  assert(emotion_intensity(ada,EmotionType::Anger)>0);
  available=available_actions(ada,simulation.world(),simulation.agents(),5,DayPhase::Day);
  assert(std::ranges::find(available,"reconcile")!=available.end());
  assert(SimulationTestAccess::execute(simulation,ada,action("reconcile",borin.id))=="se réconcilie avec Borin");
  assert(!ada.relationships[borin.id].conflict);
  assert(ada.relationships[borin.id].affinity>0);

  simulation.save_checkpoint();
  Logger restored_logger(root.string());std::mt19937 restored_rng(7);LocalDecider restored_decider(restored_rng);
  Simulation restored(7,restored_decider,restored_logger,nullptr,checkpoint.string());
  const auto& restored_ada=restored.agents().front();
  assert(restored_ada.relationships==ada.relationships);
  assert(restored_ada.companion_id==ada.companion_id);
  assert(restored_ada.companion_until_day==ada.companion_until_day);
}
