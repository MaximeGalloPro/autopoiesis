#include "autopoiesis/simulation.hpp"
#include <cassert>
#include <random>

using namespace apo;

int main() {
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Perception perception{json{
      {"self", {{"x", 3}, {"y", 2}, {"hunger", 65}, {"fatigue", 20}}},
      {"available_actions", json::array({"observe", "wait", "move", "sleep"})},
      {"cells", json::array({
          {{"x", 3}, {"y", 2}, {"terrain", 0}, {"rabbit", false}},
          {{"x", 4}, {"y", 2}, {"terrain", 0}, {"rabbit", false}},
          {{"x", 5}, {"y", 2}, {"terrain", static_cast<int>(Terrain::Bush)}, {"rabbit", false}},
      })},
  }};

  const Decision decision = decider.decide(perception);
  assert(decision.action == "move");
  assert(decision.parameters["direction"] == "east");
  assert(decision.reason == "I seek food");

  Perception beside_water{json{
      {"self", {{"x", 4}, {"y", 2}, {"hunger", 20}, {"thirst", 70}, {"fatigue", 20}}},
      {"available_actions", json::array({"move", "wait", "drink"})},
      {"cells", json::array()},
      {"known_map", json::array()}}};
  assert(decider.decide(beside_water).action == "drink");

  auto water_route = [](int thirst, int hunger, const std::string& id = "") {
    return Perception{json{
        {"world_width", 40}, {"world_height", 24},
        {"self", {{"id", id}, {"x", 0}, {"y", 5}, {"hunger", hunger},
                  {"thirst", thirst}, {"fatigue", 20},
                  {"attributes", {{"focus", 100}, {"willpower", 50},
                                  {"endurance", 50}, {"spatial_sense", 80}}}}},
        {"available_actions", json::array({"move", "wait"})},
        {"cells", json::array()},
        {"known_map", json::array({
            {{"x", 0}, {"y", 5}, {"terrain", static_cast<int>(Terrain::Ground)}, {"status", "traversable"}},
            {{"x", 39}, {"y", 5}, {"terrain", static_cast<int>(Terrain::Ground)}, {"status", "traversable"}},
            {{"x", 38}, {"y", 5}, {"terrain", static_cast<int>(Terrain::Water)}, {"status", "blocked"}}})}}};
  };

  const Decision wrapped_route = decider.decide(water_route(70, 20));
  assert(wrapped_route.action == "move");
  assert(wrapped_route.parameters["direction"] == "west");

  const Decision persistent_first = decider.decide(water_route(50, 55, "persistent"));
  const Decision persistent_second = decider.decide(water_route(40, 60, "persistent"));
  assert(persistent_first.parameters["direction"] == "west");
  assert(persistent_second.parameters["direction"] == "west");
}
