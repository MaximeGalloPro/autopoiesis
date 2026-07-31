#include "autopoiesis/group.hpp"
#include "autopoiesis/simulation.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace apo;

int main() {
  Group group;
  assert(group.id() == primary_group_id);
  assert(group.primary_base().kind == BaseKind::Campfire);

  const auto& catalog = base_infrastructure_catalog();
  assert(catalog.size() == 2);
  assert(catalog[0].type == BaseInfrastructure::Stockpile);
  assert(catalog[0].key == "stockpile");
  assert(catalog[0].building_type == BuildingType::Stockpile);
  assert(catalog[0].cost.wood == 2);
  assert(catalog[0].cost.branches == 0);
  assert(catalog[0].prerequisite == BasePrerequisite::Campfire);
  assert(catalog[1].type == BaseInfrastructure::Workshop);
  assert(catalog[1].key == "workshop");
  assert(catalog[1].building_type == BuildingType::Workshop);
  assert(catalog[1].cost.wood == 3);
  assert((catalog[1].cost.items ==
          std::vector<std::pair<CraftItem, int>>{{CraftItem::IronIngot, 1}}));
  assert(catalog[1].prerequisite == BasePrerequisite::Stockpile);
  assert(base_infrastructure(BuildingType::Wall) == nullptr);

  assert(group.infrastructure_level(BaseInfrastructure::Stockpile) == 0);
  assert(group.infrastructure_level(BaseInfrastructure::Workshop) == 0);
  assert(group.can_evolve(BaseInfrastructure::Stockpile));
  assert(!group.can_evolve(BaseInfrastructure::Workshop));
  assert(group.evolve(BaseInfrastructure::Stockpile));
  assert(group.infrastructure_level(BaseInfrastructure::Stockpile) == 1);
  assert(!group.evolve(BaseInfrastructure::Stockpile));
  assert(group.can_evolve(BaseInfrastructure::Workshop));

  Group named_group("groupe-secondaire");
  Group restored;
  restored.restore_checkpoint(named_group.checkpoint());
  assert(restored.id() == "groupe-secondaire");
  restored.restore_checkpoint(group.checkpoint());
  assert(restored.id() == primary_group_id);
  assert(restored.primary_base().kind == BaseKind::Campfire);
  assert(restored.infrastructure_level(BaseInfrastructure::Stockpile) == 1);
  assert(restored.infrastructure_level(BaseInfrastructure::Workshop) == 0);

  const auto root = std::filesystem::path("/tmp") /
                    ("autopoiesis-group-base-" + std::to_string(getpid()));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto checkpoint = root / "state.json";
  Logger logger(root.string());
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42, decider, logger, nullptr, checkpoint.string());
  assert(simulation.group().id() == primary_group_id);
  assert(simulation.group().primary_base().kind == BaseKind::Campfire);
  simulation.save_checkpoint();

  std::mt19937 restored_rng(7);
  LocalDecider restored_decider(restored_rng);
  Logger restored_logger(root.string());
  Simulation resumed(7, restored_decider, restored_logger, nullptr, checkpoint.string());
  assert(resumed.restored_checkpoint());
  assert(resumed.group().id() == primary_group_id);
  assert(resumed.group().primary_base().kind == BaseKind::Campfire);

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
  std::mt19937 legacy_rng(99);
  LocalDecider legacy_decider(legacy_rng);
  Logger legacy_logger(root.string());
  Simulation legacy(99, legacy_decider, legacy_logger, nullptr, legacy_checkpoint.string());
  assert(legacy.restored_checkpoint());
  assert(legacy.group().id() == primary_group_id);
  assert(legacy.group().primary_base().kind == BaseKind::Campfire);

  std::filesystem::remove_all(root);
}
