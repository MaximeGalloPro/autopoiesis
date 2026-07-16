#include "autopoiesis/decision.hpp"
#include "autopoiesis/simulation.hpp"
#include <cassert>
#include <iostream>
using namespace apo;
struct Fake : IDecider { Decision d; Decision decide(const Perception&) override{return d;} };
int main(){World w;assert(!w.in_bounds({0,-1}));assert(!w.passable({0,0}));assert(!w.passable({5,2}));assert(w.passable({14,2}));Agent a{"a","A",{14,2}};std::vector<Agent> as{a};assert(w.eat_berries(a.position));assert(w.berries(a.position)==2);std::string e;assert(validate_decision({DecisionType::Action,"move",{{"direction","north"}}},a,w,as,e));assert(!validate_decision({DecisionType::Action,"hack",json::object()},a,w,as,e));assert(!validate_decision({DecisionType::Action,"move",{{"direction","up"}}},a,w,as,e));a.hunger=98;a.health=5;Fake f;f.d={DecisionType::Action,"wait",json::object(),""};Logger l("/tmp/autopoiesis-tests");Simulation s(42,f,l);s.cycle();LocalDecider local(*new std::mt19937(42));(void)local;Decision parsed;assert(parse_decision({{"type","action"},{"action","wait"},{"parameters",json::object()},{"reason","ok"}},parsed,e));assert(!parse_decision({{"type","action"},{"action","unknown"}},parsed,e));std::cout<<"all core tests passed\n";}
