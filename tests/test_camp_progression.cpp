#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace apo;

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Group& group(Simulation& simulation) { return simulation.group_; }
  static Agent& agent(Simulation& simulation) { return simulation.agents_.front(); }
  static Perception perceive(Simulation& simulation, Agent& agent) {
    return simulation.perceive(agent);
  }
  static std::string execute(Simulation& simulation, Agent& agent, const Decision& decision) {
    return simulation.execute(agent, decision);
  }
};
}  // namespace apo

namespace {
Decision action(std::string name, json parameters = json::object()) {
  return {DecisionType::Action, std::move(name), std::move(parameters), "test"};
}

Position neighboring_worksite(const World& world, Position camp) {
  for (const auto candidate : world.neighbors(camp))
    if (world.passable(candidate)) return candidate;
  assert(false && "a campfire must have a passable neighboring worksite");
  return {};
}
}  // namespace

int main() {
  const auto root = std::filesystem::path("/tmp") /
                    ("autopoiesis-camp-progression-" + std::to_string(getpid()));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto checkpoint = root / "state.json";

  Logger logger(root.string());
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42, decider, logger, nullptr, checkpoint.string());
  auto& world = SimulationTestAccess::world(simulation);
  auto& group = SimulationTestAccess::group(simulation);
  auto& builder = SimulationTestAccess::agent(simulation);
  const Position camp{13, 2};
  assert(world.place_campfire(camp));

  // The initial chest is deliberately small. Its only expansion is a validated
  // camp action, advertised through the active capability cards.
  assert(world.camp_chest_level(camp) == 1);
  const int initial_capacity = world.camp_chest_capacity(camp);
  assert(world.store_materials(camp, World::camp_chest_upgrade_wood_cost,
                               World::camp_chest_upgrade_branch_cost));
  builder.position = neighboring_worksite(world, camp);
  auto actions = available_actions(builder, world, simulation.agents());
  assert(std::ranges::find(actions, "upgrade_camp_chest") != actions.end());
  const auto perception = SimulationTestAccess::perceive(simulation, builder);
  const auto cards = perception.value.at("active_cards");
  assert(std::any_of(cards.begin(), cards.end(), [](const json& card) {
    return card.value("action", "") == "upgrade_camp_chest";
  }));
  std::string error;
  const auto upgrade = decider.decide(perception);
  assert(upgrade.action == "upgrade_camp_chest");
  assert(validate_decision(upgrade, builder, world, simulation.agents(), error));
  assert(SimulationTestAccess::execute(simulation, builder, upgrade) ==
         "agrandit le coffre du camp");
  assert(world.camp_chest_level(camp) == 2);
  assert(world.camp_chest_capacity(camp) > initial_capacity);

  // A complete storage building is a prerequisite for the workshop; both are
  // committed to the persistent collective camp progression.
  assert(world.store_materials(camp, 12, 3, 2));
  assert(world.craft(camp, "wooden_handle"));
  assert(world.craft(camp, "charcoal"));
  assert(world.craft(camp, "rope"));
  assert(world.craft(camp, "iron_ingot"));
  const Position storage_site{14, 1};
  const Position workshop_site{15, 2};
  assert(!world.can_designate_building(workshop_site, camp, BuildingType::Workshop));
  assert(world.can_designate_building(storage_site, camp, BuildingType::Stockpile));
  assert(world.designate_building(storage_site, camp, BuildingType::Stockpile));
  builder.position = world.neighbors(storage_site).front();
  builder.equipped_tool = Tool{CraftItem::Axe, 20, 20};
  while (!world.building(storage_site)->complete) {
    assert(SimulationTestAccess::execute(
               simulation, builder,
               action("work_on_building", {{"x", storage_site.x}, {"y", storage_site.y}}))
               .starts_with("travaille sur stockpile"));
  }
  assert(group.infrastructure_level(BaseInfrastructure::Stockpile) == 1);
  assert(world.can_designate_building(workshop_site, camp, BuildingType::Workshop));
  assert(world.designate_building(workshop_site, camp, BuildingType::Workshop));
  builder.position = world.neighbors(workshop_site).front();
  while (!world.building(workshop_site)->complete) {
    assert(SimulationTestAccess::execute(
               simulation, builder,
               action("work_on_building", {{"x", workshop_site.x}, {"y", workshop_site.y}}))
               .starts_with("travaille sur workshop"));
  }
  assert(group.infrastructure_level(BaseInfrastructure::Workshop) == 1);

  // A full chest is a bounded logistical state, not an invitation to wander.
  // The local decider follows its remembered path home and then waits when no
  // consuming, crafting, or further upgrade action is currently available.
  assert(world.store_materials(camp, world.camp_chest_capacity(camp) -
                                        world.camp_chest_occupation(camp),
                               0, 0));
  const Position camp_rest = neighboring_worksite(world, camp);
  Position camp_approach{};
  bool has_camp_approach = false;
  for (const auto candidate : world.neighbors(camp_rest)) {
    if (candidate != camp && world.passable(candidate)) {
      camp_approach = candidate;
      has_camp_approach = true;
      break;
    }
  }
  assert(has_camp_approach);
  builder.position = camp_approach;
  builder.home_camp = camp;
  builder.camp_rest_position = camp_rest;
  builder.known_campfires.insert({camp.x, camp.y});
  for (const Position position : {camp_approach, camp_rest, camp})
    builder.remember_map(position, world.terrain(position));
  std::mt19937 full_chest_rng(77);
  LocalDecider full_chest_decider(full_chest_rng);
  const auto return_to_full_camp =
      full_chest_decider.decide(SimulationTestAccess::perceive(simulation, builder));
  assert(return_to_full_camp.action == "move");
  assert(validate_decision(return_to_full_camp, builder, world, simulation.agents(), error));
  assert(SimulationTestAccess::execute(simulation, builder, return_to_full_camp) ==
         "se deplace");
  assert(builder.position == camp_rest);
  const auto settle_full_camp =
      full_chest_decider.decide(SimulationTestAccess::perceive(simulation, builder));
  assert(settle_full_camp.action == "wait");
  assert(validate_decision(settle_full_camp, builder, world, simulation.agents(), error));

  simulation.save_checkpoint();
  std::mt19937 restored_rng(7);
  LocalDecider restored_decider(restored_rng);
  Logger restored_logger(root.string());
  Simulation restored(7, restored_decider, restored_logger, nullptr, checkpoint.string());
  assert(restored.world().camp_chest_level(camp) == 2);
  assert(restored.group().infrastructure_level(BaseInfrastructure::Stockpile) == 1);
  assert(restored.group().infrastructure_level(BaseInfrastructure::Workshop) == 1);

  json legacy_state;
  {
    std::ifstream input(checkpoint);
    input >> legacy_state;
  }
  legacy_state.erase("group");
  const auto legacy_checkpoint = root / "legacy-state.json";
  {
    std::ofstream output(legacy_checkpoint);
    output << legacy_state;
  }
  std::mt19937 legacy_rng(9);
  LocalDecider legacy_decider(legacy_rng);
  Logger legacy_logger(root.string());
  Simulation migrated(9, legacy_decider, legacy_logger, nullptr, legacy_checkpoint.string());
  assert(migrated.restored_checkpoint());
  assert(migrated.world().camp_chest_level(camp) == 2);
  assert(migrated.group().infrastructure_level(BaseInfrastructure::Stockpile) == 1);
  assert(migrated.group().infrastructure_level(BaseInfrastructure::Workshop) == 1);

  std::filesystem::remove_all(root);
}
