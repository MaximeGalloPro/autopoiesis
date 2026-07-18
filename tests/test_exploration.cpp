#include "autopoiesis/simulation.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>

using namespace apo;

static Perception exploration_perception(const json& known_map) {
  return Perception{json{
      {"self", {{"x", 5}, {"y", 5}, {"hunger", 30}, {"fatigue", 20}}},
      {"cells", json::array()},
      {"known_map", known_map},
      {"available_actions", json::array({"move", "wait"})}}};
}

struct FixedMove final : IDecider {
  std::string direction;
  explicit FixedMove(std::string value) : direction(std::move(value)) {}
  Decision decide(const Perception&) override {
    return {DecisionType::Action, "move", {{"direction", direction}}, "test"};
  }
};

int main() {
  std::mt19937 rng(42);
  LocalDecider decider(rng);

  json known = json::array({
      {{"x", 5}, {"y", 4}, {"status", "blocked"}, {"visit_count", 0}},
      {{"x", 6}, {"y", 5}, {"status", "traversable"}, {"visit_count", 4}},
      {{"x", 5}, {"y", 6}, {"status", "traversable"}, {"visit_count", 2}},
      {{"x", 4}, {"y", 5}, {"status", "traversable"}, {"visit_count", 2}}});
  Decision selected = decider.decide(exploration_perception(known));
  assert(selected.action == "move");
  assert(selected.parameters["direction"] == "south");

  json surrounded = json::array({
      {{"x", 5}, {"y", 4}, {"status", "blocked"}},
      {{"x", 6}, {"y", 5}, {"status", "out_of_bounds"}},
      {{"x", 5}, {"y", 6}, {"status", "blocked"}},
      {{"x", 4}, {"y", 5}, {"status", "blocked"}}});
  selected = decider.decide(exploration_perception(surrounded));
  assert(selected.action != "move");

  const auto open = exploration_perception(json::array());
  const Decision first = decider.decide(open);
  for (int i = 0; i < 10; ++i) {
    const Decision repeated = decider.decide(open);
    assert(repeated.action == first.action);
    assert(repeated.parameters == first.parameters);
  }

  setenv("CYCLES_PER_DAY", "1", 1);
  Logger logger("/tmp/autopoiesis-exploration-tests");
  FixedMove successful{"east"};
  Simulation success_simulation(42, successful, logger);
  success_simulation.run_day();
  const auto& moved_ada = success_simulation.agents().front();
  assert((moved_ada.position == Position{4, 2}));
  assert(moved_ada.map_visit_counts.at({4, 2}) == 1);

  FixedMove blocked{"east"};
  Simulation blocked_simulation(42, blocked, logger);
  blocked_simulation.run_day();
  blocked_simulation.run_day();
  const auto& stopped_ada = blocked_simulation.agents().front();
  assert((stopped_ada.position == Position{4, 2}));
  assert(stopped_ada.map_memory.at({5, 2}) == Terrain::Water);
  unsetenv("CYCLES_PER_DAY");

  std::cout << "exploration tests passed\n";
}
