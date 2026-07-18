#include "autopoiesis/simulation.hpp"
#include <cassert>
#include <random>

using namespace apo;

static Perception perception(int hunger, int fatigue,
                             const std::vector<std::string>& actions) {
  return Perception{json{
      {"self", {{"x", 5}, {"y", 5}, {"hunger", hunger}, {"fatigue", fatigue}}},
      {"available_actions", actions},
      {"cells", json::array()},
      {"known_map", json::array()}}};
}

int main() {
  std::mt19937 rng(42);
  LocalDecider decider(rng);

  const Decision tired = decider.decide(
      perception(39, 45, {"move", "wait", "rest"}));
  assert(tired.action == "rest");

  const Decision hungry = decider.decide(
      perception(40, 44, {"move", "wait", "eat_berries"}));
  assert(hungry.action == "eat_berries");

  const Decision hungry_and_tired = decider.decide(
      perception(40, 45, {"move", "rest", "eat_berries"}));
  assert(hungry_and_tired.action == "eat_berries");

  const Decision below_thresholds = decider.decide(
      perception(39, 44, {"move", "rest", "eat_berries"}));
  assert(below_thresholds.action == "move");
  assert(below_thresholds.parameters["direction"] == "north");

  const Perception identical =
      perception(40, 45, {"move", "rest", "eat_berries"});
  const Decision first = decider.decide(identical);
  const Decision second = decider.decide(identical);
  assert(first.action == second.action);
  assert(first.parameters == second.parameters);
}
