#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <cassert>
#include <set>
#include <vector>

using namespace apo;

struct WaitOnly final : IDecider {
  Decision decide(const Perception&) override {
    return {DecisionType::Action, "wait", json::object(), "test"};
  }
};

namespace apo {
struct SimulationTestAccess {
  static std::string execute(Simulation& simulation, Agent& agent,
                             const Decision& decision) {
    return simulation.execute(agent, decision);
  }
};
}

int main() {
  World world(42);
  assert(World::width == 40);
  assert(World::height == 24);
  assert(!world.step({0, 7}, "west"));
  assert(!world.step({39, 7}, "east"));
  assert(!world.step({4, 0}, "north"));
  assert((world.step({0, 7}, "east") == Position{1, 7}));
  assert((world.neighbors({0, 0}) == std::vector<Position>{{1, 0}, {0, 1}}));
  assert(world.distance({0, 5}, {39, 5}) == 39);
  assert(!world.adjacent({0, 5}, {39, 5}));
  assert(world.terrain({-1, 7}) == Terrain::Wall);
  assert(!world.passable({-1, 7}));
  const int berries_before = world.berries({14, 2});
  assert(!world.eat_berries({14 - World::width, 2}));
  assert(world.berries({14, 2}) == berries_before);

  std::set<FoodType> foods;
  for (const auto& resource : world.food_resources()) foods.insert(resource.type);
  assert(foods.contains(FoodType::Berries));
  assert(foods.contains(FoodType::Roots));
  assert(foods.contains(FoodType::Mushrooms));
  assert(foods.contains(FoodType::Fish));
  assert(foods.contains(FoodType::Venison));

  std::set<AnimalType> animals;
  for (const auto& animal : world.animals()) animals.insert(animal.type);
  assert(animals.contains(AnimalType::Rabbit));
  assert(animals.contains(AnimalType::Deer));
  assert(animals.contains(AnimalType::Boar));
  assert(animals.contains(AnimalType::Wolf));
  assert(animals.contains(AnimalType::Fish));

  Agent attributed{"a", "Ada", {3, 2}};
  const auto attributes = attributes_json(attributed.attributes);
  assert(attributes.size() == 10);
  assert(attributes.contains("strength"));
  assert(attributes.contains("spatial_sense"));

  WaitOnly wait;
  Logger logger("/tmp/autopoiesis-world-complexity-tests");
  Simulation simulation(42, wait, logger);
  Agent thirsty{"a", "Ada", {4, 2}};
  thirsty.thirst = 70;
  const int before_thirst = thirsty.thirst;
  Decision drink{DecisionType::Action, "drink", json::object(), "test"};
  assert(SimulationTestAccess::execute(simulation, thirsty, drink) == "boit de l'eau");
  assert(thirsty.thirst < before_thirst);

  Agent edge{"edge", "Bord", {0, 0}};
  Decision leave_world{DecisionType::Action, "move", {{"direction", "west"}}, "test"};
  assert(SimulationTestAccess::execute(simulation, edge, leave_world) == "deplacement bloque");
  assert((edge.position == Position{0, 0}));
  assert(edge.map_memory.empty());
}
