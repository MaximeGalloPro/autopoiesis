#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <cassert>
#include <set>

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
  assert((world.wrap({-1, 7}) == Position{39, 7}));
  assert((world.wrap({40, 7}) == Position{0, 7}));
  assert((world.wrap({4, -1}) == Position{4, 23}));
  assert(world.toroidal_distance({0, 5}, {39, 5}) == 1);
  assert(world.terrain({-1, 7}) == world.terrain({39, 7}));

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
}
