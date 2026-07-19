#include "autopoiesis/decision.hpp"
#include "autopoiesis/feature_request.hpp"
#include "autopoiesis/simulation.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>

using namespace apo;

struct Fake final : IDecider {
  Decision decision;
  Decision decide(const Perception&) override { return decision; }
};

struct CountingReporter final : ICycleReporter {
  int calls{0};
  std::vector<int> simulation_cycles;
  std::vector<std::string> histories;

  json report_period(int simulation_cycle, int, const Agent&,
                     const std::vector<std::string>& history) override {
    ++calls;
    simulation_cycles.push_back(simulation_cycle);
    assert(!history.empty());
    histories.push_back(history.front());
    return {{"character_voice", "ok"},
            {"day_summary", "ok"},
            {"state_assessment", "ok"},
            {"ask_god", false}};
  }
};

int main() {
  World world;
  assert(!world.in_bounds({0, -1}));
  assert(world.passable({0, 0}));
  assert(world.passable({-1, 0}) == world.passable({World::width - 1, 0}));
  assert(!world.passable({5, 2}));
  assert(world.passable({14, 2}));

  Agent agent{"a", "A", {14, 2}};
  std::vector<Agent> agents{agent};
  assert(world.eat_berries(agent.position));
  assert(world.berries(agent.position) == 7);
  std::string error;
  assert(validate_decision({DecisionType::Action, "move", {{"direction", "north"}}},
                           agent, world, agents, error));
  assert(!validate_decision({DecisionType::Action, "hack", json::object()},
                            agent, world, agents, error));
  assert(!validate_decision({DecisionType::Action, "move", {{"direction", "up"}}},
                            agent, world, agents, error));

  agent.hunger = 98;
  agent.health = 5;
  Fake fake;
  fake.decision = {DecisionType::Action, "wait", json::object(), "I need rest",
                   "rest", "", "be rested"};
  Logger logger("/tmp/autopoiesis-tests");
  Simulation single_day(42, fake, logger);
  single_day.run(1, 0, 0);

  setenv("REPORT_EVERY_DAYS", "1", 1);
  CountingReporter reporter;
  Simulation reported(42, fake, logger, &reporter);
  reported.run(2, 0, 0);
  assert(reporter.calls == 6);
  assert(reporter.simulation_cycles[0] == 2400);
  assert(reporter.simulation_cycles[3] == 4800);
  assert(reporter.histories[0].find("reason=I need rest") != std::string::npos);
  assert(reporter.histories[0].find("outcome=success") != std::string::npos);
  unsetenv("REPORT_EVERY_DAYS");

  setenv("REPORT_EVERY_DAYS", "3", 1);
  CountingReporter every_three;
  Simulation configured(42, fake, logger, &every_three);
  configured.run(3, 0, 0);
  assert(every_three.calls == 3);
  assert(every_three.simulation_cycles[0] == 7200);
  unsetenv("REPORT_EVERY_DAYS");

  std::mt19937 local_rng(42);
  LocalDecider local(local_rng);
  (void)local;
  Decision parsed;
  assert(parse_decision({{"type", "action"}, {"action", "wait"},
                         {"parameters", json::object()}, {"reason", "ok"}},
                        parsed, error));
  assert(parse_decision({{"type", "action"}, {"action", "unknown"},
                         {"parameters", json::object()}}, parsed, error));
  assert(!validate_decision(parsed, agent, world, agents, error));

  json request = {
      {"requested", true},
      {"evolution_key", "chasse_du_lapin"},
      {"domain", "survie"},
      {"title", "Chasser le lapin"},
      {"need", "Manger"},
      {"obstacle", "Les baies ne suffisent pas"},
      {"proposed_change", "Ajouter une chasse locale"},
      {"mechanism", {{"name", "chasse_du_lapin"},
                      {"summary", "Un personnage peut chasser un lapin voisin"},
                      {"resources", json::array({"lapin"})},
                      {"actions", json::array({"hunt_rabbit"})},
                      {"preconditions", json::array({"lapin adjacent"})},
                      {"deterministic_effects", json::array({"le lapin est retire et la faim diminue"})}}},
      {"acceptance_tests", json::array({"Une chasse valide diminue la faim",
                                        "Une chasse sans lapin est refusee"})}};
  assert(validate_feature_request(request, error));
  request["mechanism"]["preconditions"] = json::array();
  assert(!validate_feature_request(request, error));
  assert(!error.empty());

  World hunt_world;
  Agent hunter{"hunter", "Hunter", {15, 3}};
  std::vector<Agent> hunters{hunter};
  auto hunt_actions = available_actions(hunter, hunt_world, hunters);
  assert(std::find(hunt_actions.begin(), hunt_actions.end(), "hunt_rabbit") !=
         hunt_actions.end());
  Decision hunt{DecisionType::Action, "hunt_rabbit", json::object(), "I need food"};
  assert(validate_decision(hunt, hunter, hunt_world, hunters, error));
  Agent distant{"distant", "Distant", {10, 10}};
  std::vector<Agent> distants{distant};
  assert(!validate_decision(hunt, distant, hunt_world, distants, error));
  assert(hunt_world.hunt_rabbit(hunter.position));
  assert(!hunt_world.rabbit_alive());
  assert(!validate_decision(hunt, hunter, hunt_world, hunters, error));
  std::cout << "all core tests passed\n";
}
