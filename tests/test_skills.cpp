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
  static void set_day(Simulation& simulation,int day) {
    simulation.day_=day;
    simulation.date_=date_from_absolute_day(day);
  }
};
}

static Decision action(std::string name,json parameters=json::object()) {
  return {DecisionType::Action,std::move(name),std::move(parameters),"test"};
}

int main() {
  const auto root=std::filesystem::path("/tmp")/("autopoiesis-skills-"+std::to_string(getpid()));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto checkpoint=root/"state.json";
  Logger logger(root.string());
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42,decider,logger,nullptr,checkpoint.string());
  SimulationTestAccess::set_day(simulation,1);
  auto& world=SimulationTestAccess::world(simulation);
  auto& mentor=SimulationTestAccess::agent(simulation,0);
  auto& learner=SimulationTestAccess::agent(simulation,1);

  assert(skill_level(mentor,Skill::Woodcutting)==0);
  mentor.equipped_tool=Tool{};
  mentor.position={6,1};
  assert(SimulationTestAccess::execute(simulation,mentor,action("harvest_wood"))=="preleve du bois");
  assert(skill_experience(mentor,Skill::Woodcutting)==1);
  assert(SimulationTestAccess::execute(simulation,mentor,action("harvest_wood"))==
         "aucun arbre vivant a prelever");
  assert(skill_experience(mentor,Skill::Woodcutting)==1);

  add_skill_experience(mentor,Skill::Woodcutting,19);
  assert(skill_level(mentor,Skill::Woodcutting)==4);
  const auto specialities=top_skills(mentor,3);
  assert(!specialities.empty()&&specialities.front()==Skill::Woodcutting);

  const Position fire{13,2};
  assert(world.place_campfire(fire));
  assert(world.store_materials(fire,2,0,0));
  mentor.position={12,2};
  learner.position={14,2};
  mentor.equipped_tool->durability=0;
  assert(SimulationTestAccess::execute(simulation,mentor,action("repair_axe"))=="repare sa hache");
  assert(mentor.equipped_tool->durability==14);

  auto actions=available_actions(mentor,world,simulation.agents(),1,DayPhase::Day);
  assert(std::ranges::find(actions,"teach_skill")!=actions.end());
  const auto lesson=action("teach_skill",{{"target_id",learner.id},{"skill","woodcutting"}});
  std::string validation_error;
  assert(validate_decision(lesson,mentor,world,simulation.agents(),validation_error,1,DayPhase::Day));
  assert(!validate_decision(action("teach_skill",{{"target_id",learner.id},{"skill","alchemy"}}),
                            mentor,world,simulation.agents(),validation_error,1,DayPhase::Day));
  assert(SimulationTestAccess::execute(simulation,mentor,lesson)=="transmet woodcutting a "+learner.name);
  assert(skill_experience(learner,Skill::Woodcutting)==1);
  assert(mentor.last_taught_day==1&&learner.last_lesson_day==1);
  assert(SimulationTestAccess::execute(simulation,mentor,lesson)=="lecon indisponible");
  assert(skill_experience(learner,Skill::Woodcutting)==1);

  simulation.save_checkpoint();
  std::mt19937 restored_rng(7);
  LocalDecider restored_decider(restored_rng);
  Logger restored_logger(root.string());
  Simulation restored(7,restored_decider,restored_logger,nullptr,checkpoint.string());
  assert(skill_level(restored.agents().at(0),Skill::Woodcutting)==4);
  assert(skill_experience(restored.agents().at(1),Skill::Woodcutting)==1);
  assert(restored.agents().at(0).last_taught_day==1);
  assert(restored.agents().at(1).last_lesson_day==1);
}
