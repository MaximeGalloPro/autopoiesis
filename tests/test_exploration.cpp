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

static json repeated_moves() {
  json history = json::array();
  for (int index = 0; index < 20; ++index) {
    const Position position = std::vector<Position>{{5, 5}, {5, 4}, {6, 4}, {6, 5}}[index % 4];
    history.push_back({{"action", "move"}, {"outcome", "success"},
                       {"x", position.x}, {"y", position.y}});
  }
  return history;
}

static json straight_moves(int count, const std::string& direction = "north") {
  json history = json::array();
  for (int index = 0; index < count; ++index)
    history.push_back({{"action", "move"}, {"outcome", "success"},
                       {"parameters", {{"direction", direction}}},
                       {"x", 5}, {"y", 5 - index}});
  return history;
}

static Perception loop_perception(int hunger, const json& known_map,
                                  const json& history = repeated_moves()) {
  json cells = json::array();
  for (const Position position : std::vector<Position>{{5, 4}, {6, 5}, {5, 6}, {4, 5}})
    cells.push_back({{"x", position.x}, {"y", position.y},
                     {"terrain", static_cast<int>(Terrain::Ground)}});
  return Perception{json{
      {"self", {{"x", 5}, {"y", 5}, {"hunger", hunger}, {"fatigue", 20}}},
      {"cells", cells},
      {"known_map", known_map},
      {"action_history", history},
      {"available_actions", json::array({"move", "wait"})}}};
}

static Perception mapped_region(Position self, const std::string& agent_id) {
  json known = json::array();
  for (int y = 2; y <= 8; ++y)
    for (int x = 2; x <= 8; ++x)
      known.push_back({{"x", x}, {"y", y}, {"status", "traversable"},
                       {"terrain", static_cast<int>(Terrain::Ground)},
                       {"visit_count", 0}, {"food", 0}});
  return Perception{json{
      {"world_width", 40}, {"world_height", 24},
      {"self", {{"id", agent_id}, {"x", self.x}, {"y", self.y},
                {"hunger", 20}, {"thirst", 20}, {"fatigue", 20},
                {"personality", {{"curiosity", 70}}},
                {"attributes", {{"focus", 60}, {"willpower", 50},
                                {"endurance", 50}, {"spatial_sense", 70}}}}},
      {"cells", json::array()}, {"known_map", known},
      {"available_actions", json::array({"observe", "move", "wait"})}}};
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


  json food_map = json::array({
      {{"x", 5}, {"y", 5}, {"status", "traversable"}, {"food", 0}},
      {{"x", 5}, {"y", 4}, {"status", "traversable"}, {"food", 0}},
      {{"x", 5}, {"y", 3}, {"status", "traversable"}, {"food", 2}},
      {{"x", 6}, {"y", 5}, {"status", "traversable"}, {"food", 0}},
      {{"x", 7}, {"y", 5}, {"status", "traversable"}, {"food", 2}}});
  const Decision food_exit = decider.decide(loop_perception(70, food_map));
  assert(food_exit.action == "move");
  assert(food_exit.parameters["direction"] == "north");
  assert(food_exit.reason == "Repetition detected: seek known food");

  json unknown_exit_map = json::array({
      {{"x", 4}, {"y", 5}, {"status", "blocked"}}});
  const Decision unknown_exit = decider.decide(loop_perception(70, unknown_exit_map));
  assert(unknown_exit.action == "move");
  assert(unknown_exit.parameters["direction"] == "south");
  assert(unknown_exit.reason == "Repetition detected: explore deterministic exit");

  const Perception below_threshold = loop_perception(69, food_map);
  Perception without_history = below_threshold;
  without_history.value.erase("action_history");
  const Decision normal_with_loop = decider.decide(below_threshold);
  const Decision normal_without_loop = decider.decide(without_history);
  assert(normal_with_loop.action == normal_without_loop.action);
  assert(normal_with_loop.parameters == normal_without_loop.parameters);
  assert(normal_with_loop.reason == normal_without_loop.reason);

  const Decision deterministic_first = decider.decide(loop_perception(70, food_map));
  const Decision deterministic_second = decider.decide(loop_perception(70, food_map));
  assert(deterministic_first.parameters == deterministic_second.parameters);
  assert(deterministic_first.reason == deterministic_second.reason);

  Perception long_walk = exploration_perception(json::array());
  long_walk.value["available_actions"] = json::array({"observe", "move", "wait"});
  long_walk.value["action_history"] = straight_moves(12);
  LocalDecider rhythm_decider(rng);
  const Decision observation = rhythm_decider.decide(long_walk);
  assert(observation.action == "observe");
  assert(observation.reason == "Je fais une pause pour observer et réévaluer mon trajet.");

  json equal_neighbors = json::array({
      {{"x", 5}, {"y", 4}, {"status", "traversable"}, {"visit_count", 0}},
      {{"x", 6}, {"y", 5}, {"status", "traversable"}, {"visit_count", 0}},
      {{"x", 5}, {"y", 6}, {"status", "traversable"}, {"visit_count", 0}},
      {{"x", 4}, {"y", 5}, {"status", "traversable"}, {"visit_count", 0}}});
  Perception straight_line = exploration_perception(equal_neighbors);
  straight_line.value["action_history"] = straight_moves(4);
  LocalDecider turning_decider(rng);
  const Decision turn = turning_decider.decide(straight_line);
  assert(turn.action == "move");
  assert(turn.parameters["direction"] != "north");

  LocalDecider frontier_decider(rng);
  const Decision first_route_step = frontier_decider.decide(mapped_region({5, 5}, "frontier"));
  assert(first_route_step.action == "move");
  assert(first_route_step.reason.starts_with("Je suis mon itinéraire d'exploration vers "));
  Position next{5, 5};
  const std::string first_direction = first_route_step.parameters["direction"];
  if (first_direction == "north") --next.y;
  if (first_direction == "south") ++next.y;
  if (first_direction == "east") ++next.x;
  if (first_direction == "west") --next.x;
  const Decision second_route_step = frontier_decider.decide(mapped_region(next, "frontier"));
  assert(second_route_step.action == "move");
  assert(second_route_step.reason == first_route_step.reason);

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
