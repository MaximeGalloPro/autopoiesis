#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
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

struct PopulationReporter final : ICycleReporter {
  int reports{};
  int requests{};

  json report_period(int, int, const Agent&, const std::vector<std::string>&) override {
    ++reports;
    return nullptr;
  }

  json request_evolution(int, int, const Agent&, const std::vector<std::string>&,
                         const json&) override {
    ++requests;
    return nullptr;
  }
};

int main() {
  const auto root=std::filesystem::path("/tmp")/("autopoiesis-population-"+std::to_string(getpid()));
  std::filesystem::remove_all(root);std::filesystem::create_directories(root);
  const auto checkpoint=root/"state.json";
  Logger logger(root.string());std::mt19937 rng(42);LocalDecider decider(rng);
  Simulation simulation(42,decider,logger,nullptr,checkpoint.string());
  auto& world=SimulationTestAccess::world(simulation);
  const Position camp{13,2};assert(world.place_campfire(camp));
  assert(world.create_shelter({14,2}));stock_food(world,camp,40);
  for(const auto& agent:simulation.agents())assert(agent.family_id=="foyer-principal");
  for(std::size_t index=0;index<simulation.agents().size();++index){
    auto& agent=SimulationTestAccess::agent(simulation,index);agent.position={14,2};agent.age_days=25;
  }

  const auto initial=simulation.agents().size();
  SimulationTestAccess::set_day(simulation,60);SimulationTestAccess::update_population(simulation);
  assert(simulation.agents().size()==initial+1);
  const auto& newcomer=simulation.agents().back();
  assert(newcomer.origin=="arrival"&&newcomer.arrival_day==60&&newcomer.age_days>=18);
  assert(newcomer.family_id=="foyer-principal");
  assert(newcomer.name=="Daphné des Aulnes 4");
  assert(newcomer.generation==0);
  const auto newcomer_name=newcomer.name;

  SimulationTestAccess::agent(simulation,1).family_id="famille-externe";
  SimulationTestAccess::agent(simulation,0).generation=2;
  SimulationTestAccess::agent(simulation,2).generation=1;
  SimulationTestAccess::set_day(simulation,90);SimulationTestAccess::update_population(simulation);
  assert(simulation.agents().size()==initial+2);
  const auto& child=simulation.agents().back();
  assert(child.origin=="birth"&&child.age_days==0&&child.parent_ids.size()==2);
  assert(child.family_id=="foyer-principal");
  assert((child.parent_ids==std::vector<std::string>{"a1","a3"}));
  assert(child.name=="Éloi des Aulnes 5");
  assert(child.name!=newcomer_name);
  assert(child.generation==3);
  const auto child_actions=available_actions(child,world,simulation.agents(),90,DayPhase::Day);
  assert(std::ranges::find(child_actions,"hunt_animal")==child_actions.end());
  assert(std::ranges::find(child_actions,"confront")==child_actions.end());
  const auto child_parents=child.parent_ids;

  auto& departing=SimulationTestAccess::agent(simulation,1);
  departing.critical_hunger_days=4;departing.relationships.clear();
  SimulationTestAccess::set_day(simulation,120);SimulationTestAccess::update_population(simulation);
  assert(!departing.alive&&departing.departure_day==120&&!departing.departure_reason.empty());

  auto& elder=SimulationTestAccess::agent(simulation,0);elder.age_days=98;
  SimulationTestAccess::set_day(simulation,121);SimulationTestAccess::update_population(simulation);
  assert(elder.alive&&elder.age_days==99);
  SimulationTestAccess::set_day(simulation,122);SimulationTestAccess::update_population(simulation);
  assert(!elder.alive&&elder.death_cause=="vieillesse");

  simulation.save_checkpoint();
  Logger restored_logger(root.string());std::mt19937 restored_rng(7);LocalDecider restored_decider(restored_rng);
  Simulation restored(7,restored_decider,restored_logger,nullptr,checkpoint.string());
  assert(restored.agents().size()==simulation.agents().size());
  assert(std::ranges::any_of(restored.agents(),[&](const Agent& agent){
    return agent.origin=="birth"&&agent.parent_ids==child_parents&&
        agent.name=="Éloi des Aulnes 5"&&agent.generation==3;
  }));

  setenv("CYCLES_PER_DAY","1",1);
  setenv("REPORT_EVERY_DAYS","1",1);
  PopulationReporter reporter;
  Logger final_day_logger((root/"final-day").string());
  std::mt19937 final_day_rng(99);LocalDecider final_day_decider(final_day_rng);
  Simulation final_day(99,final_day_decider,final_day_logger,&reporter,(root/"final-day-state.json").string());
  for(std::size_t index=0;index<final_day.agents().size();++index)
    SimulationTestAccess::agent(final_day,index).age_days=99;

  final_day.run(3,0,0);
  assert(final_day.simulation_cycle()==1);
  assert(std::ranges::none_of(final_day.agents(),[](const Agent& agent){return agent.alive;}));
  assert(reporter.reports==0&&reporter.requests==0);
  assert(std::filesystem::exists(root/"final-day-state.json"));

  unsetenv("CYCLES_PER_DAY");
  unsetenv("REPORT_EVERY_DAYS");
}
