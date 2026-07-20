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
  static void progress_emotions(Simulation& simulation,Agent& agent) {
    simulation.update_emotions(agent);
  }
  static Perception perceive(Simulation& simulation,Agent& agent) { return simulation.perceive(agent); }
  static void set_day(Simulation& simulation,int day) {
    simulation.day_=day;simulation.date_=date_from_absolute_day(day);
  }
};
}

static Decision action(std::string name,json parameters=json::object()) {
  return {DecisionType::Action,std::move(name),std::move(parameters),"test"};
}

int main() {
  const auto root=std::filesystem::path("/tmp")/("autopoiesis-emotions-"+std::to_string(getpid()));
  std::filesystem::remove_all(root);std::filesystem::create_directories(root);
  const auto checkpoint=root/"state.json";
  Logger logger(root.string());std::mt19937 rng(42);LocalDecider decider(rng);
  Simulation simulation(42,decider,logger,nullptr,checkpoint.string());
  SimulationTestAccess::set_day(simulation,4);
  auto& agent=SimulationTestAccess::agent(simulation,0);

  agent.position={24,15};agent.attributes.strength=0;agent.attributes.toughness=0;
  assert(SimulationTestAccess::execute(simulation,agent,
      action("hunt_animal",{{"animal_id","boar-1"}}))=="chasse boar");
  assert(std::ranges::any_of(agent.emotions,[](const Emotion& emotion){
    return emotion.type==EmotionType::Fear&&emotion.cause=="blessure pendant la chasse au boar"&&
           emotion.effect=="éviter les chasses dangereuses";
  }));

  const int fear_before=emotion_intensity(agent,EmotionType::Fear);
  SimulationTestAccess::progress_emotions(simulation,agent);
  assert(emotion_intensity(agent,EmotionType::Fear)<fear_before);
  for(int day=0;day<8;++day)SimulationTestAccess::progress_emotions(simulation,agent);
  assert(emotion_intensity(agent,EmotionType::Fear)==0);

  add_emotion(agent,EmotionType::Stress,35,"échec répété de déplacement",
              "réduire la prise de risque",3,4);
  add_emotion(agent,EmotionType::Fear,70,"attaque récente d'un loup",
              "éviter les chasses dangereuses",3,4);
  const auto perception=SimulationTestAccess::perceive(simulation,agent);
  assert(perception.value["self"]["emotions"].front()["cause"]=="échec répété de déplacement");

  json fearful={{"world_width",40},{"world_height",24},
    {"time",{{"phase","day"},{"cycles_until_night",1000},{"cycles_per_day",2400}}},
    {"self",{{"id","fearful"},{"x",10},{"y",10},{"hunger",80},{"thirst",10},{"fatigue",10},
      {"boredom",0},{"personality",personality_json({50,50,50,50,50})},
      {"attributes",attributes_json({})},{"behavior",behavior_json({"test","survive",50,50,50,50,{}})},
      {"project",project_json({})},{"emotions",emotions_json(agent)}}},
    {"available_actions",json::array({"wait","move","hunt_animal"})},
    {"visible_agents",json::array()},{"animals",json::array({{{"id","wolf"},{"type","wolf"},{"danger",90},{"nutrition",20},{"adjacent",true}}})},
    {"known_map",json::array()},{"action_history",json::array()}};
  assert(decider.decide({fearful}).action!="hunt_animal");

  simulation.save_checkpoint();
  Logger restored_logger(root.string());std::mt19937 restored_rng(7);LocalDecider restored_decider(restored_rng);
  Simulation restored(7,restored_decider,restored_logger,nullptr,checkpoint.string());
  assert(restored.agents().front().emotions==agent.emotions);
}
