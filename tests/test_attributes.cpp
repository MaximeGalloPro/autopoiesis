#include "autopoiesis/simulation.hpp"

#include <cassert>
#include <random>

using namespace apo;

struct WaitDecider final : IDecider {
  Decision decide(const Perception&) override { return {}; }
};

namespace apo {
struct SimulationTestAccess {
  static std::string execute(Simulation& simulation, Agent& agent,
                             const Decision& decision) {
    return simulation.execute(agent, decision);
  }
  static void advance_needs(Simulation& simulation, Agent& agent, int index) {
    simulation.advance_action_needs(agent, index);
  }
  static void update_needs(Simulation& simulation, Agent& agent) {
    simulation.update_needs(agent);
  }
};
}

static Simulation make_simulation(WaitDecider& decider, Logger& logger) {
  return Simulation(42, decider, logger);
}

int main() {
  WaitDecider decider;
  Logger logger("/tmp/autopoiesis-attribute-tests");

  Simulation rest_simulation = make_simulation(decider, logger);
  Agent slow_recovery{"slow", "Slow", {3, 2}};
  Agent fast_recovery = slow_recovery;
  slow_recovery.fatigue = fast_recovery.fatigue = 80;
  slow_recovery.attributes.recuperation = 0;
  fast_recovery.attributes.recuperation = 100;
  Decision rest{DecisionType::Action, "rest", json::object(), "test"};
  SimulationTestAccess::execute(rest_simulation, slow_recovery, rest);
  SimulationTestAccess::execute(rest_simulation, fast_recovery, rest);
  assert(fast_recovery.fatigue < slow_recovery.fatigue);

  Simulation agility_simulation = make_simulation(decider, logger);
  Agent slow{"slow", "Slow", {3, 2}};
  Agent agile = slow;
  slow.attributes.agility = 0;
  agile.attributes.agility = 100;
  Decision move{DecisionType::Action, "move", {{"direction", "north"}}, "test"};
  SimulationTestAccess::execute(agility_simulation, slow, move);
  SimulationTestAccess::execute(agility_simulation, agile, move);
  assert(slow.fatigue > agile.fatigue);

  Agent winded{"low", "Low endurance", {3, 2}};
  Agent enduring = winded;
  winded.attributes.endurance = 0;
  enduring.attributes.endurance = 100;
  SimulationTestAccess::advance_needs(agility_simulation, winded, 12);
  SimulationTestAccess::advance_needs(agility_simulation, enduring, 12);
  assert(winded.fatigue > enduring.fatigue);

  Simulation weak_hunt = make_simulation(decider, logger);
  Simulation strong_hunt = make_simulation(decider, logger);
  Agent weak{"weak", "Weak", {34, 20}};
  Agent strong = weak;
  weak.attributes.strength = 0;
  weak.attributes.toughness = 50;
  strong.attributes.strength = 100;
  strong.attributes.toughness = 50;
  Decision hunt{DecisionType::Action, "hunt_animal", {{"animal_id", "wolf-1"}}, "test"};
  SimulationTestAccess::execute(weak_hunt, weak, hunt);
  SimulationTestAccess::execute(strong_hunt, strong, hunt);
  assert(weak.health < strong.health);

  Simulation frail_hunt = make_simulation(decider, logger);
  Simulation tough_hunt = make_simulation(decider, logger);
  Agent frail{"frail", "Frail", {34, 20}};
  Agent tough = frail;
  frail.attributes.strength = tough.attributes.strength = 50;
  frail.attributes.toughness = 0;
  tough.attributes.toughness = 100;
  SimulationTestAccess::execute(frail_hunt, frail, hunt);
  SimulationTestAccess::execute(tough_hunt, tough, hunt);
  assert(frail.health < tough.health);

  Simulation mushroom_low = make_simulation(decider, logger);
  Simulation mushroom_high = make_simulation(decider, logger);
  Agent sensitive{"sensitive", "Sensitive", {22, 10}};
  Agent resistant = sensitive;
  sensitive.attributes.disease_resistance = 0;
  resistant.attributes.disease_resistance = 100;
  Decision eat{DecisionType::Action, "eat_food", json::object(), "test"};
  SimulationTestAccess::execute(mushroom_low, sensitive, eat);
  SimulationTestAccess::execute(mushroom_high, resistant, eat);
  assert(sensitive.health < resistant.health);

  Agent low_will{"low", "Low will", {3, 2}};
  Agent high_will = low_will;
  low_will.thirst = high_will.thirst = 100;
  low_will.critical_thirst_days = high_will.critical_thirst_days = 1;
  low_will.attributes.willpower = 0;
  high_will.attributes.willpower = 100;
  SimulationTestAccess::update_needs(agility_simulation, low_will);
  SimulationTestAccess::update_needs(agility_simulation, high_will);
  assert(low_will.health < high_will.health);

  Agent forgetful{"forgetful", "Forgetful", {3, 2}};
  Agent mindful = forgetful;
  forgetful.attributes.memory = 0;
  mindful.attributes.memory = 100;
  for (int index = 0; index < 12; ++index) {
    forgetful.remember(std::to_string(index));
    mindful.remember(std::to_string(index));
  }
  assert(forgetful.memories.size() < mindful.memories.size());

  auto exploration = [](int spatial_sense) {
    return Perception{json{
        {"self", {{"x", 5}, {"y", 5}, {"hunger", 0}, {"thirst", 0}, {"fatigue", 0},
                  {"attributes", {{"spatial_sense", spatial_sense}}}}},
        {"available_actions", json::array({"move", "wait"})},
        {"cells", json::array({
            {{"x", 5}, {"y", 4}, {"terrain", static_cast<int>(Terrain::Ground)}},
            {{"x", 6}, {"y", 5}, {"terrain", static_cast<int>(Terrain::Ground)}}})},
        {"known_map", json::array({
            {{"x", 5}, {"y", 4}, {"status", "traversable"}, {"visit_count", 0}}})}}};
  };
  std::mt19937 rng(42);
  LocalDecider local(rng);
  assert(local.decide(exploration(0)).parameters["direction"] == "north");
  assert(local.decide(exploration(100)).parameters["direction"] == "east");
}
