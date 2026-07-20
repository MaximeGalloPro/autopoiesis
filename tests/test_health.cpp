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
  static std::string execute(Simulation& simulation,Agent& agent,const Decision& decision) {
    return simulation.execute(agent,decision);
  }
  static void progress_health(Simulation& simulation,Agent& agent) {
    simulation.update_health_conditions(agent);
  }
  static void set_day(Simulation& simulation,int day) {
    simulation.day_=day;simulation.date_=date_from_absolute_day(day);
  }
};
}

static Decision action(std::string name,json parameters=json::object()) {
  return {DecisionType::Action,std::move(name),std::move(parameters),"test"};
}

int main() {
  const auto root=std::filesystem::path("/tmp")/("autopoiesis-health-"+std::to_string(getpid()));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto checkpoint=root/"state.json";
  Logger logger(root.string());
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42,decider,logger,nullptr,checkpoint.string());
  SimulationTestAccess::set_day(simulation,4);
  auto& world=SimulationTestAccess::world(simulation);
  auto& patient=SimulationTestAccess::agent(simulation,0);
  auto& healer=SimulationTestAccess::agent(simulation,1);

  patient.position={24,15};
  patient.attributes.strength=0;patient.attributes.toughness=0;
  assert(SimulationTestAccess::execute(simulation,patient,
      action("hunt_animal",{{"animal_id","boar-1"}}))=="chasse boar");
  assert(patient.conditions.size()==1);
  assert(patient.conditions.front().type==HealthConditionType::Injury);
  assert(patient.conditions.front().cause=="chasse boar");

  patient.conditions.front().days=2;
  patient.conditions.front().severity=30;
  const int health_before=patient.health;
  SimulationTestAccess::progress_health(simulation,patient);
  assert(patient.health<health_before);
  assert(std::ranges::any_of(patient.conditions,[](const HealthCondition& condition){
    return condition.type==HealthConditionType::Infection;
  }));

  const Position fire{13,2};
  assert(world.place_campfire(fire));
  patient.position={14,2};healer.position={13,1};
  auto actions=available_actions(healer,world,simulation.agents(),4,DayPhase::Day);
  assert(std::ranges::find(actions,"treat_condition")!=actions.end());
  const auto injury_id=patient.conditions.front().id;
  const int severity_before=patient.conditions.front().severity;
  const auto treatment=action("treat_condition",{{"target_id",patient.id},{"condition_id",injury_id}});
  std::string error;
  assert(validate_decision(treatment,healer,world,simulation.agents(),error,4,DayPhase::Day));
  assert(!validate_decision(action("treat_condition",{{"target_id",patient.id},{"condition_id",injury_id}}),
                            patient,world,simulation.agents(),error,4,DayPhase::Day));
  assert(SimulationTestAccess::execute(simulation,healer,treatment)=="soigne "+patient.name);
  assert(patient.conditions.front().treated);
  assert(patient.conditions.front().severity<severity_before);
  assert(healer.relationships[patient.id].trust>0);

  assert(world.create_shelter(patient.position));
  actions=available_actions(patient,world,simulation.agents(),4,DayPhase::Day);
  assert(std::ranges::find(actions,"convalesce")!=actions.end());
  const int treated_severity=patient.conditions.front().severity;
  assert(SimulationTestAccess::execute(simulation,patient,action("convalesce"))=="recupere en convalescence");
  assert(patient.conditions.front().severity<treated_severity);
  assert(patient.last_convalescence_day==4);
  assert(SimulationTestAccess::execute(simulation,patient,action("convalesce"))=="convalescence indisponible");

  Agent sick=patient;
  sick.id="sick";sick.conditions.clear();sick.next_condition_id=1;
  sick.attributes.disease_resistance=0;sick.position={22,10};
  assert(SimulationTestAccess::execute(simulation,sick,action("eat_food")).starts_with("mange "));
  assert(std::ranges::any_of(sick.conditions,[](const HealthCondition& condition){
    return condition.type==HealthConditionType::Disease;
  }));

  sick.conditions.clear();sick.carried_food=FoodItem{FoodType::Mushrooms,18,false,2};
  assert(SimulationTestAccess::execute(simulation,sick,action("eat_carried_food"))=="mange sa nourriture transportee");
  assert(std::ranges::any_of(sick.conditions,[](const HealthCondition& condition){
    return condition.type==HealthConditionType::Disease;
  }));

  simulation.save_checkpoint();
  std::mt19937 restored_rng(7);
  LocalDecider restored_decider(restored_rng);
  Logger restored_logger(root.string());
  Simulation restored(7,restored_decider,restored_logger,nullptr,checkpoint.string());
  assert(restored.agents().front().conditions==patient.conditions);
  assert(restored.agents().front().last_convalescence_day==4);
}
