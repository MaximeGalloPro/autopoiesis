#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <cassert>

using namespace apo;

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& agent(Simulation& simulation) { return simulation.agents_.front(); }
  static std::string execute(Simulation& simulation,Agent& agent,const Decision& decision) {
    return simulation.execute(agent,decision);
  }
};
}

static Decision action(std::string name) {
  return {DecisionType::Action,std::move(name),json::object(),"test"};
}

static Perception transport_perception(Position self,int wood,int branches,
                                       const std::vector<std::string>& actions) {
  return Perception{json{
      {"world_width",40},{"world_height",24},
      {"time",{{"phase","day"},{"cycles_per_day",2400},{"cycles_until_night",900}}},
      {"self",{{"id","carrier"},{"x",self.x},{"y",self.y},{"hunger",10},{"thirst",10},
               {"fatigue",10},{"wood_inventory",wood},{"branch_inventory",branches},
               {"carried_food",nullptr},{"home_camp",{{"x",12},{"y",5}}},
               {"camp_rest_position",{{"x",11},{"y",5}}},{"personality",{{"curiosity",50}}},
               {"attributes",{{"focus",60},{"willpower",60},{"endurance",60},{"spatial_sense",70}}},
               {"behavior",{{"provision_drive",70},{"exploration_drive",40}}},
               {"project",{{"key","secure_food"},{"status","active"}}}}},
      {"available_actions",actions},{"cells",json::array()},
      {"known_map",json::array({
          {{"x",10},{"y",5},{"status","traversable"},{"branches",0},{"campfire",false}},
          {{"x",11},{"y",5},{"status","traversable"},{"branches",1},{"campfire",false}},
          {{"x",12},{"y",5},{"status","traversable"},{"branches",0},{"campfire",true},
           {"stored_wood",0},{"stored_branches",0}}})}}};
}

int main() {
  Logger logger("/tmp/autopoiesis-resource-transport-tests");
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42,decider,logger);
  auto& world=SimulationTestAccess::world(simulation);
  auto& carrier=SimulationTestAccess::agent(simulation);
  const Position fire{13,2};
  assert(world.place_campfire(fire));

  carrier.position={12,2};
  carrier.wood_inventory=2;
  carrier.branch_inventory=3;
  auto actions=available_actions(carrier,world,simulation.agents());
  assert(std::ranges::find(actions,"deposit_materials")!=actions.end());
  assert(SimulationTestAccess::execute(simulation,carrier,action("deposit_materials"))==
         "depose des materiaux au camp");
  assert(carrier.wood_inventory==0);
  assert(carrier.branch_inventory==0);
  assert(world.stored_wood(fire)==2);
  assert(world.stored_branches(fire)==3);

  const auto checkpoint=world.checkpoint();
  World restored(7);
  restored.restore_checkpoint(checkpoint);
  assert(restored.stored_wood(fire)==2);
  assert(restored.stored_branches(fire)==3);

  const auto snapshot=make_ui_snapshot(date_from_absolute_day(1),0,
      climate_for(date_from_absolute_day(1)),world,simulation.agents(),{});
  const auto fire_cell=std::ranges::find_if(snapshot.cells,[&](const UiCell& cell){
    return cell.position==fire;
  });
  assert(fire_cell!=snapshot.cells.end());
  assert(fire_cell->stored_wood==2);
  assert(fire_cell->stored_branches==3);

  const auto seek_resource=decider.decide(transport_perception(
      {10,5},0,0,{"observe","wait","move"}));
  assert(seek_resource.action=="move");
  assert(seek_resource.parameters["direction"]=="east");
  assert(seek_resource.reason=="Je rejoins des branches connues pour le foyer.");

  const auto route=decider.decide(transport_perception(
      {10,5},2,0,{"observe","wait","move"}));
  assert(route.action=="move");
  assert(route.parameters["direction"]=="east");
  assert(route.reason=="Je rapporte mes matériaux au foyer.");

  const auto deposit=decider.decide(transport_perception(
      {11,5},0,2,{"observe","wait","move","deposit_materials"}));
  assert(deposit.action=="deposit_materials");
}
