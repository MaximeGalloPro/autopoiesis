#include "autopoiesis/simulation.hpp"

#include <array>
#include <cassert>
#include <optional>
#include <utility>

using namespace apo;

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& agent(Simulation& simulation) { return simulation.agents_.front(); }
  static Perception perceive(Simulation& simulation, Agent& agent) {
    return simulation.perceive(agent);
  }
  static std::string execute(Simulation& simulation, Agent& agent,
                             const Decision& decision) {
    return simulation.execute(agent, decision);
  }
};
}

static Perception camp_route_perception(bool project_blocked, Position self,
                                        bool at_camp = false,
                                        bool camp_rest_available = true,
                                        bool local_action_available = false) {
  json actions = at_camp && camp_rest_available
      ? json::array({"observe", "move", "wait", "rest_by_campfire"})
      : json::array({"observe", "move", "wait"});
  if (local_action_available) actions.push_back("collect_branch");
  return Perception{json{
      {"world_width", 40}, {"world_height", 24},
      {"self", {{"id", project_blocked ? "home-fallback" : ""}, {"x", self.x}, {"y", self.y},
                {"hunger", 20}, {"thirst", 20}, {"fatigue", 20},
                {"home_camp", {{"x", 5}, {"y", 2}}},
                {"camp_rest_position", {{"x", 4}, {"y", 2}}},
                {"project", {{"key", "secure_food"},
                             {"status", project_blocked ? "blocked" : "active"},
                             {"missing_capability", project_blocked ? "food_storage" : ""}}},
                {"attributes", {{"focus", 60}, {"willpower", 50},
                                {"endurance", 50}, {"spatial_sense", 60}}}}},
      {"available_actions", std::move(actions)},
      {"cells", json::array()},
      {"known_map", json::array({
          {{"x", 2}, {"y", 2}, {"status", "traversable"}, {"visit_count", 0}},
          {{"x", 3}, {"y", 2}, {"status", "traversable"}, {"visit_count", 0}},
          {{"x", 4}, {"y", 2}, {"status", "traversable"}, {"visit_count", 0}},
          {{"x", 5}, {"y", 2}, {"status", "traversable"}, {"campfire", true},
           {"visit_count", 0}}})}}};
}

static std::optional<std::array<Position, 3>> camp_route(World& world) {
  for (int y = 1; y < World::height - 1; ++y) {
    for (int x = 1; x < World::width - 1; ++x) {
      const Position camp{x, y};
      if (!world.passable(camp)) continue;
      for (const Position rest : world.neighbors(camp)) {
        if (!world.passable(rest)) continue;
        for (const Position start : world.neighbors(rest)) {
          if (start != camp && world.passable(start)) return {{camp, rest, start}};
        }
      }
    }
  }
  return std::nullopt;
}

int main() {
  std::mt19937 rng(42);
  LocalDecider decider(rng);

  const Decision blocked_return = decider.decide(camp_route_perception(true, {2, 2}));
  assert(blocked_return.action == "move");
  assert(blocked_return.parameters == json({{"direction", "east"}}));
  assert(blocked_return.reason == "Je rejoins le foyer connu pour sortir de l'impasse.");

  const Decision blocked_return_over_local_action =
      decider.decide(camp_route_perception(true, {2, 2}, false, true, true));
  assert(blocked_return_over_local_action.action == "move");
  assert(blocked_return_over_local_action.parameters == json({{"direction", "east"}}));

  const Decision blocked_settle = decider.decide(camp_route_perception(true, {4, 2}, true));
  assert(blocked_settle.action == "rest_by_campfire");
  assert(blocked_settle.reason == "Je me repose près du feu du foyer après une impasse.");

  const Decision blocked_observe = decider.decide(camp_route_perception(true, {4, 2}, true, false));
  assert(blocked_observe.action == "observe");
  assert(blocked_observe.reason == "J'observe le foyer pour réévaluer la suite.");

  const Decision idle_return = decider.decide(camp_route_perception(false, {2, 2}));
  assert(idle_return.action == "move");
  assert(idle_return.parameters == json({{"direction", "east"}}));
  assert(idle_return.reason == "Je rejoins le foyer connu pour sortir de l'impasse.");

  Logger logger("/tmp/autopoiesis-home-fallback-tests");
  Simulation simulation(42, decider, logger);
  auto& world = SimulationTestAccess::world(simulation);
  auto& agent = SimulationTestAccess::agent(simulation);
  const auto route = camp_route(world);
  assert(route.has_value());
  const auto [camp, rest, start] = *route;
  assert(world.place_campfire(camp));

  agent.position = start;
  agent.home_camp = camp;
  agent.camp_rest_position = rest;
  agent.known_campfires.insert({camp.x, camp.y});
  agent.project.status = ProjectStatus::Blocked;
  agent.project.missing_capability = "food_storage";
  agent.fatigue = 40;

  std::string error;
  const Decision return_home = decider.decide(SimulationTestAccess::perceive(simulation, agent));
  assert(return_home.action == "move");
  assert(validate_decision(return_home, agent, world, simulation.agents(), error));
  assert(SimulationTestAccess::execute(simulation, agent, return_home) == "se deplace");
  assert(agent.position == rest);

  const Decision settle = decider.decide(SimulationTestAccess::perceive(simulation, agent));
  assert(settle.action == "rest_by_campfire");
  assert(validate_decision(settle, agent, world, simulation.agents(), error));
  const int fatigue_before = agent.fatigue;
  assert(SimulationTestAccess::execute(simulation, agent, settle) == "reste pres du feu de camp");
  assert(agent.fatigue == fatigue_before - 1);
}
