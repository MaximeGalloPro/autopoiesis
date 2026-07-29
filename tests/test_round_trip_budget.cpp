#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <queue>

using namespace apo;

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& agent(Simulation& simulation) { return simulation.agents_.front(); }
};
}

namespace {
using Coordinates = std::pair<int, int>;

struct Route {
  std::vector<std::string> directions;
};

Route route_between(const World& world, Position start, Position target) {
  if (start == target) return {};
  const std::vector<std::string> directions{"north", "east", "south", "west"};
  std::queue<Position> pending;
  std::map<Coordinates, Coordinates> parent;
  std::map<Coordinates, std::string> parent_direction;
  pending.push(start);
  parent[{start.x, start.y}] = {start.x, start.y};

  while (!pending.empty()) {
    const auto current = pending.front();
    pending.pop();
    for (const auto& direction : directions) {
      const auto next = world.step(current, direction);
      if (!next || !world.passable(*next) || parent.contains({next->x, next->y})) continue;
      parent[{next->x, next->y}] = {current.x, current.y};
      parent_direction[{next->x, next->y}] = direction;
      if (*next == target) {
        std::vector<std::string> result;
        Coordinates cursor{next->x, next->y};
        const Coordinates origin{start.x, start.y};
        while (cursor != origin) {
          result.push_back(parent_direction.at(cursor));
          cursor = parent.at(cursor);
        }
        std::ranges::reverse(result);
        return {std::move(result)};
      }
      pending.push(*next);
    }
  }
  return {};
}

Position position_after(const World& world, Position position, const Route& route) {
  for (const auto& direction : route.directions) position = *world.step(position, direction);
  return position;
}

struct RoundTripDecider final : IDecider {
  World* world{};
  Position target{};
  Position camp_deposit{};

  RoundTripDecider(Position destination, Position deposit)
      : target(destination), camp_deposit(deposit) {}

  Decision decide(const Perception& perception) override {
    const auto& self = perception.value.at("self");
    if (self.at("id") != "a1")
      return {DecisionType::Action, "wait", json::object(), "laisser le monde vivre"};

    const Position current{self.at("x"), self.at("y")};
    const int branches = self.value("branch_inventory", 0);
    if (branches == 0) {
      if (current == target)
        return {DecisionType::Action, "collect_branch", json::object(), "récolter une ressource éloignée"};
      const auto route = route_between(*world, current, target);
      assert(!route.directions.empty());
      return {DecisionType::Action, "move", {{"direction", route.directions.front()}},
              "rejoindre la ressource puis revenir au foyer"};
    }

    if (current == camp_deposit)
      return {DecisionType::Action, "deposit_materials", json::object(), "rapporter la ressource au foyer"};
    const auto route = route_between(*world, current, camp_deposit);
    assert(!route.directions.empty());
    return {DecisionType::Action, "move", {{"direction", route.directions.front()}},
            "retourner au foyer pour déposer la ressource"};
  }
};
}

int main() {
  setenv("CYCLES_PER_DAY", "2400", 1);
  Logger logger("/tmp/autopoiesis-round-trip-budget-tests");
  struct WaitDecider final : IDecider {
    Decision decide(const Perception&) override {
      return {DecisionType::Action, "wait", json::object(), "test"};
    }
  } wait_decider;
  Simulation placeholder(42, wait_decider, logger);
  auto& world = SimulationTestAccess::world(placeholder);
  const auto camp = SimulationTestAccess::agent(placeholder).position;
  assert(world.place_campfire(camp));

  Position target = camp;
  Route outbound;
  for (int y = 0; y < World::height; ++y) {
    for (int x = 0; x < World::width; ++x) {
      const Position candidate{x, y};
      if (world.branches(candidate) <= 0) continue;
      const auto route = route_between(world, camp, candidate);
      if (route.directions.size() > outbound.directions.size()) {
        target = candidate;
        outbound = route;
      }
    }
  }
  assert(target != camp);
  assert(!outbound.directions.empty());

  Position deposit = camp;
  for (const auto candidate : world.neighbors(camp)) {
    if (world.passable(candidate)) {
      deposit = candidate;
      break;
    }
  }
  assert(deposit != camp);
  assert(!route_between(world, target, deposit).directions.empty());

  RoundTripDecider decider{target, deposit};
  Simulation simulation(42, decider, logger);
  auto& simulation_world = SimulationTestAccess::world(simulation);
  auto& agent = SimulationTestAccess::agent(simulation);
  assert(simulation_world.place_campfire(agent.position));
  decider.world = &simulation_world;
  const auto actual_target = target;
  const auto actual_outbound = route_between(simulation_world, agent.position, actual_target);
  assert(!actual_outbound.directions.empty());
  decider.target = actual_target;
  decider.camp_deposit = deposit;

  simulation.run(1, 0, 0);

  const auto return_route = route_between(simulation_world, actual_target, deposit);
  const int measured_route_ticks = static_cast<int>(actual_outbound.directions.size() + return_route.directions.size() + 9);
  assert(measured_route_ticks <= 2400);
  assert(simulation_world.stored_branches(camp) == 1);
  assert(agent.branch_inventory == 0);
  assert(simulation.simulation_cycle() == 2400);
  std::cout << "round trip: " << actual_outbound.directions.size() << " ticks aller, "
            << return_route.directions.size() << " ticks retour, " << measured_route_ticks
            << " ticks d'action\n";

  unsetenv("CYCLES_PER_DAY");
}
