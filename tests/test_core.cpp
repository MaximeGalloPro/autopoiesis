#include "autopoiesis/decision.hpp"
#include "autopoiesis/feature_request.hpp"
#include "autopoiesis/simulation.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
using namespace apo;
struct Fake : IDecider { Decision d; Decision decide(const Perception&) override{return d;} };
struct CountingReporter : ICycleReporter {
  int calls{0};
  std::vector<int> cycles;
  json report_cycle(int cycle, const Agent&, const std::vector<std::string>& history) override {
    ++calls;
    cycles.push_back(cycle);
    assert(!history.empty());
    return {{"character_voice","ok"},{"day_summary","ok"},{"state_assessment","ok"},{"ask_god",false},{"feature_request",{{"requested",false},{"title",""},{"need",""},{"obstacle",""},{"proposed_change",""},{"mechanism",{{"name",""},{"summary",""},{"resources",json::array()},{"actions",json::array()},{"preconditions",json::array()},{"deterministic_effects",json::array()}}},{"acceptance_tests",json::array()}}}};
  }
};
int main(){World w;assert(!w.in_bounds({0,-1}));assert(!w.passable({0,0}));assert(!w.passable({5,2}));assert(w.passable({14,2}));Agent a{"a","A",{14,2}};std::vector<Agent> as{a};assert(w.eat_berries(a.position));assert(w.berries(a.position)==7);std::string e;assert(validate_decision({DecisionType::Action,"move",{{"direction","north"}}},a,w,as,e));assert(!validate_decision({DecisionType::Action,"hack",json::object()},a,w,as,e));assert(!validate_decision({DecisionType::Action,"move",{{"direction","up"}}},a,w,as,e));a.hunger=98;a.health=5;Fake f;f.d={DecisionType::Action,"wait",json::object(),""};Logger l("/tmp/autopoiesis-tests");Simulation s(42,f,l);s.cycle();CountingReporter reporter;Simulation reported(42,f,l,&reporter);reported.run(2,0,0);assert(reporter.calls==6);assert(reporter.cycles[0]==1);assert(reporter.cycles[3]==2);std::mt19937 local_rng(42);LocalDecider local(local_rng);(void)local;Decision parsed;assert(parse_decision({{"type","action"},{"action","wait"},{"parameters",json::object()},{"reason","ok"}},parsed,e));assert(parse_decision({{"type","action"},{"action","unknown"},{"parameters",json::object()}},parsed,e));assert(!validate_decision(parsed,a,w,as,e));
  setenv("REPORT_EVERY_CYCLES","3",1);
  CountingReporter every_three;
  Simulation configured(42,f,l,&every_three);
  configured.run(3,0,0);
  assert(every_three.calls==3);
  assert(every_three.cycles[0]==3);
  unsetenv("REPORT_EVERY_CYCLES");
  json request={{"requested",true},{"title","Chasser le lapin"},{"need","Manger"},{"obstacle","Les baies ne suffisent pas"},{"proposed_change","Ajouter une chasse locale"},{"mechanism",{{"name","chasse_du_lapin"},{"summary","Un personnage peut chasser un lapin voisin"},{"resources",json::array({"lapin"})},{"actions",json::array({"hunt_rabbit"})},{"preconditions",json::array({"lapin adjacent"})},{"deterministic_effects",json::array({"le lapin est retire et la faim diminue"})}}},{"acceptance_tests",json::array({"Une chasse valide diminue la faim","Une chasse sans lapin est refusee"})}};
  assert(validate_feature_request(request,e));
  request["mechanism"]["preconditions"]=json::array();
  assert(!validate_feature_request(request,e));
  assert(!e.empty());
  World hunt_world;
  Agent hunter{"hunter","Hunter",{15,3}};
  std::vector<Agent> hunters{hunter};
  auto hunt_actions=available_actions(hunter,hunt_world,hunters);
  assert(std::find(hunt_actions.begin(),hunt_actions.end(),"hunt_rabbit")!=hunt_actions.end());
  Decision hunt{DecisionType::Action,"hunt_rabbit",json::object(),"I need food"};
  assert(validate_decision(hunt,hunter,hunt_world,hunters,e));
  Agent distant{"distant","Distant",{10,10}};
  std::vector<Agent> distants{distant};
  assert(!validate_decision(hunt,distant,hunt_world,distants,e));
  assert(hunt_world.hunt_rabbit(hunter.position));
  assert(!hunt_world.rabbit_alive());
  assert(!validate_decision(hunt,hunter,hunt_world,hunters,e));
  std::cout<<"all core tests passed\n";}
