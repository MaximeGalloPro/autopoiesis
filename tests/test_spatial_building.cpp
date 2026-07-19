#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <filesystem>
#include <unistd.h>

using namespace apo;

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& agent(Simulation& simulation) { return simulation.agents_.front(); }
  static Perception perceive(Simulation& simulation,Agent& agent) { return simulation.perceive(agent); }
  static std::string execute(Simulation& simulation,Agent& agent,const Decision& decision) {
    return simulation.execute(agent,decision);
  }
};
}

static Decision action(std::string name,json parameters=json::object()) {
  return {DecisionType::Action,std::move(name),std::move(parameters),"test"};
}

int main() {
  const auto root=std::filesystem::path("/tmp")/("autopoiesis-buildings-"+std::to_string(getpid()));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto checkpoint=root/"state.json";
  Logger logger(root.string());
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42,decider,logger,nullptr,checkpoint.string());
  auto& world=SimulationTestAccess::world(simulation);
  auto& builder=SimulationTestAccess::agent(simulation);
  const Position fire{13,2};
  assert(world.place_campfire(fire));
  assert(world.store_materials(fire,15,3,2));
  assert(world.craft(fire,"wooden_handle"));
  assert(world.craft(fire,"charcoal"));
  assert(world.craft(fire,"rope"));
  assert(world.craft(fire,"iron_ingot"));

  const std::array<std::pair<BuildingType,Position>,5> projects{{
      {BuildingType::Wall,{11,2}},{BuildingType::Door,{12,1}},
      {BuildingType::Bed,{13,0}},{BuildingType::Stockpile,{14,1}},
      {BuildingType::Workshop,{15,2}}}};
  const auto before_invalid=world.checkpoint();
  assert(!world.designate_building({5,5},fire,BuildingType::Wall));
  assert(world.checkpoint()==before_invalid);
  builder.position={12,2};
  builder.equipped_tool=Tool{CraftItem::Axe,20,20};
  const auto designation=decider.decide(SimulationTestAccess::perceive(simulation,builder));
  assert(designation.action=="designate_building");
  assert(designation.parameters["building"]=="wall");
  assert(SimulationTestAccess::execute(simulation,builder,designation)=="designe wall");
  for(std::size_t index=1;index<projects.size();++index){
    const auto&[type,site]=projects[index];
    assert(world.can_designate_building(site,fire,type));
    assert(world.designate_building(site,fire,type));
    const auto building=world.building(site);
    assert(building&&building->type==type&&!building->complete&&building->progress==0);
  }
  assert(world.stored_wood(fire)==2);
  assert(world.stored_item(fire,CraftItem::WoodenHandle)==0);
  assert(world.stored_item(fire,CraftItem::Rope)==0);
  assert(world.stored_item(fire,CraftItem::IronIngot)==0);

  int successful_work=0;
  for(const auto&[type,site]:projects){
    builder.position=world.neighbors(site).front();
    auto actions=available_actions(builder,world,simulation.agents());
    assert(std::ranges::find(actions,"work_on_building")!=actions.end());
    while(!world.building(site)->complete){
      assert(SimulationTestAccess::execute(simulation,builder,
          action("work_on_building",{{"x",site.x},{"y",site.y}})).starts_with("travaille sur "));
      ++successful_work;
    }
    assert(world.building(site)->type==type);
  }
  assert(builder.equipped_tool->durability==20-successful_work);
  assert(!world.passable(projects[0].second));
  assert(world.passable(projects[1].second));
  assert(world.rest_bonus(projects[2].second)==10);
  assert(world.has_completed_building(BuildingType::Stockpile));
  assert(world.workshop_bonus(fire)==2);
  assert(skill_experience(builder,Skill::Building)==successful_work);
  const auto snapshot=make_ui_snapshot(date_from_absolute_day(1),0,climate_for(date_from_absolute_day(1)),
                                       world,simulation.agents(),{});
  const auto wall_cell=std::ranges::find_if(snapshot.cells,[&](const UiCell& cell){
    return cell.position==projects[0].second;
  });
  assert(wall_cell!=snapshot.cells.end()&&wall_cell->building&&wall_cell->building->complete);

  const auto complete_state=world.checkpoint();
  assert(SimulationTestAccess::execute(simulation,builder,
      action("work_on_building",{{"x",projects[4].second.x},{"y",projects[4].second.y}}))==
      "chantier indisponible");
  assert(world.checkpoint()==complete_state);

  simulation.save_checkpoint();
  std::mt19937 restored_rng(7);
  LocalDecider restored_decider(restored_rng);
  Logger restored_logger(root.string());
  Simulation restored(7,restored_decider,restored_logger,nullptr,checkpoint.string());
  for(const auto&[type,site]:projects)
    assert(restored.world().building(site)&&restored.world().building(site)->type==type&&
           restored.world().building(site)->complete);
}
