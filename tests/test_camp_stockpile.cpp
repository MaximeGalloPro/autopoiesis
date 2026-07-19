#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <cassert>

using namespace apo;

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& agent(Simulation& simulation, std::size_t index) {
    return simulation.agents_.at(index);
  }
  static std::string execute(Simulation& simulation, Agent& agent,
                             const Decision& decision) {
    return simulation.execute(agent, decision);
  }
};
}

static Decision action(std::string name) {
  return {DecisionType::Action, std::move(name), json::object(), "test"};
}

static Perception camp_perception(Position self,int hunger,json carried,
                                  const std::vector<std::string>& actions) {
  return Perception{json{
      {"world_width",40},{"world_height",24},
      {"time",{{"phase","day"},{"cycles_per_day",2400},{"cycles_until_night",900}}},
      {"self",{{"id","provider-test"},{"x",self.x},{"y",self.y},
               {"hunger",hunger},{"thirst",20},{"fatigue",20},
               {"carried_food",std::move(carried)},
               {"personality",{{"curiosity",50}}},
               {"attributes",{{"focus",60},{"willpower",60},{"endurance",60},{"spatial_sense",70}}},
               {"behavior",{{"provision_drive",95},{"exploration_drive",40}}},
               {"project",{{"key","secure_food"},{"status","active"}}}}},
      {"available_actions",actions},{"cells",json::array()},
      {"known_map",json::array({
          {{"x",10},{"y",5},{"status","traversable"},{"food",1},{"campfire",false}},
          {{"x",11},{"y",5},{"status","traversable"},{"campfire",false}},
          {{"x",12},{"y",5},{"status","traversable"},{"campfire",true},{"stored_food",1}}})}}};
}

int main() {
  Logger logger("/tmp/autopoiesis-camp-stockpile-tests");
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42, decider, logger);
  auto& world=SimulationTestAccess::world(simulation);
  auto& collector=SimulationTestAccess::agent(simulation,0);
  auto& diner=SimulationTestAccess::agent(simulation,1);

  const Position food_position{14,2};
  const Position fire_position{13,2};
  assert(world.place_campfire(fire_position));
  collector.known_campfires.insert({fire_position.x,fire_position.y});
  diner.known_campfires.insert({fire_position.x,fire_position.y});

  collector.position=food_position;
  auto actions=available_actions(collector,world,simulation.agents());
  assert(std::ranges::find(actions,"collect_food")!=actions.end());
  assert(SimulationTestAccess::execute(simulation,collector,action("collect_food"))==
         "ramasse de la nourriture");
  assert(collector.carried_food.has_value());
  assert(world.food(food_position)==7);

  collector.position={12,2};
  actions=available_actions(collector,world,simulation.agents());
  assert(std::ranges::find(actions,"deposit_food")!=actions.end());
  assert(SimulationTestAccess::execute(simulation,collector,action("deposit_food"))==
         "depose de la nourriture au camp");
  assert(!collector.carried_food.has_value());
  assert(world.stored_food(fire_position)==1);

  diner.position={13,1};
  diner.hunger=70;
  actions=available_actions(diner,world,simulation.agents());
  assert(std::ranges::find(actions,"eat_camp_food")!=actions.end());
  assert(SimulationTestAccess::execute(simulation,diner,action("eat_camp_food"))==
         "mange une reserve du camp");
  assert(diner.hunger<70);
  assert(world.stored_food(fire_position)==0);

  const auto checkpoint=world.checkpoint();
  World restored(7);
  restored.restore_checkpoint(checkpoint);
  assert(restored.stored_food(fire_position)==0);

  const json ration={{"type","berries"},{"nutrition",35}};
  const auto return_to_camp=decider.decide(camp_perception(
      {10,5},20,ration,{"observe","wait","move","eat_carried_food"}));
  assert(return_to_camp.action=="move");
  assert(return_to_camp.parameters["direction"]=="east");
  assert(return_to_camp.reason=="Je rapporte ma nourriture au feu de camp.");

  const auto deposit=decider.decide(camp_perception(
      {11,5},20,ration,{"observe","wait","move","deposit_food","eat_carried_food"}));
  assert(deposit.action=="deposit_food");

  const auto collect=decider.decide(camp_perception(
      {10,5},20,nullptr,{"observe","wait","move","collect_food"}));
  assert(collect.action=="collect_food");

  const auto shared_meal=decider.decide(camp_perception(
      {11,5},70,nullptr,{"observe","wait","move","eat_camp_food"}));
  assert(shared_meal.action=="eat_camp_food");
}
