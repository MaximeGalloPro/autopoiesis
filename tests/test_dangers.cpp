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
  static void update_dangers(Simulation& simulation) { simulation.update_dangers(); }
  static void set_day_climate(Simulation& simulation,int day,ClimateState climate) {
    simulation.day_=day;simulation.date_=date_from_absolute_day(day);simulation.climate_=std::move(climate);
  }
  static const std::vector<DangerEvent>& dangers(const Simulation& simulation) { return simulation.dangers_; }
  static Perception perceive(Simulation& simulation,Agent& agent) { return simulation.perceive(agent); }
};
}

static bool has_danger(const Simulation& simulation,DangerType type) {
  return std::ranges::any_of(SimulationTestAccess::dangers(simulation),[&](const DangerEvent& danger){return danger.type==type;});
}

int main() {
  const auto root=std::filesystem::path("/tmp")/("autopoiesis-dangers-"+std::to_string(getpid()));
  std::filesystem::remove_all(root);std::filesystem::create_directories(root);
  const auto checkpoint=root/"state.json";
  Logger logger(root.string());std::mt19937 rng(42);LocalDecider decider(rng);
  Simulation simulation(42,decider,logger,nullptr,checkpoint.string());
  auto& world=SimulationTestAccess::world(simulation);auto& ada=SimulationTestAccess::agent(simulation,0);
  const Position camp{13,2};assert(world.place_campfire(camp));ada.position={14,2};ada.fatigue=85;
  ada.equipped_tool=Tool{};
  const auto wolf=std::ranges::find_if(world.animals(),[](const Animal& animal){return animal.type==AnimalType::Wolf;});
  assert(wolf!=world.animals().end());
  ada.position=world.neighbors(wolf->position).front();

  const int health_before=ada.health;
  SimulationTestAccess::set_day_climate(simulation,15,{32,0,"sec"});
  SimulationTestAccess::update_dangers(simulation);
  assert(has_danger(simulation,DangerType::Predator));
  assert(has_danger(simulation,DangerType::Wildfire));
  assert(has_danger(simulation,DangerType::Accident));
  assert(has_danger(simulation,DangerType::Shortage));
  assert(ada.health==health_before);
  assert(std::ranges::all_of(SimulationTestAccess::dangers(simulation),[](const DangerEvent& danger){return danger.warning_day==15;}));
  assert(SimulationTestAccess::perceive(simulation,ada).value["dangers"].size()>=4);

  SimulationTestAccess::set_day_climate(simulation,16,{25,0,"clair"});
  SimulationTestAccess::update_dangers(simulation);
  assert(ada.health<health_before);
  assert(emotion_intensity(ada,EmotionType::Fear)>0);

  const int fatigue_before=ada.fatigue;
  SimulationTestAccess::set_day_climate(simulation,17,{18,12,"tempête"});
  SimulationTestAccess::update_dangers(simulation);
  assert(has_danger(simulation,DangerType::Storm));
  assert(ada.fatigue==fatigue_before);
  SimulationTestAccess::set_day_climate(simulation,18,{18,0,"clair"});
  SimulationTestAccess::update_dangers(simulation);
  assert(ada.fatigue>fatigue_before);

  simulation.save_checkpoint();
  Logger restored_logger(root.string());std::mt19937 restored_rng(7);LocalDecider restored_decider(restored_rng);
  Simulation restored(7,restored_decider,restored_logger,nullptr,checkpoint.string());
  assert(SimulationTestAccess::dangers(restored)==SimulationTestAccess::dangers(simulation));
}
