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
}
