#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <cassert>

using namespace apo;

struct WaitDecider final : IDecider {
  Decision decide(const Perception&) override { return {}; }
};

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& ada(Simulation& simulation) { return simulation.agents_.front(); }
  static std::string execute(Simulation& simulation, Agent& agent,
                             const Decision& decision) {
    const Agent before = agent;
    const auto result = simulation.execute(agent, decision);
    simulation.update_behavior_after_action(
        agent, before, decision, result,
        result == "preleve du bois" || result == "assemble l'abri" ||
            result == "construit un abri");
    return result;
  }
};
}

int main() {
  WaitDecider decider;
  Logger logger("/tmp/autopoiesis-shelter-tests");
  Simulation simulation(42, decider, logger);
  auto& world = SimulationTestAccess::world(simulation);
  auto& ada = SimulationTestAccess::ada(simulation);
  const Decision harvest{DecisionType::Action, "harvest_wood", json::object(),
                         "Je prélève du bois."};
  const Decision assemble{DecisionType::Action, "assemble_shelter", json::object(),
                          "J'assemble mon abri."};
  std::string error;

  // Un prélèvement transforme exactement un arbre vivant en une unité portée.
  ada.position = {6, 1};
  assert(world.living_trees(ada.position) == 1);
  assert(validate_decision(harvest, ada, world, simulation.agents(), error));
  assert(SimulationTestAccess::execute(simulation, ada, harvest) ==
         "preleve du bois");
  assert(world.living_trees(ada.position) == 0);
  assert(ada.wood_inventory == 1);

  // Sans arbre, tout l'état pertinent reste inchangé.
  const auto empty_cell = ada.position;
  const int wood_before_failed_harvest = ada.wood_inventory;
  const int project_progress_before_failed_harvest = ada.project.progress;
  assert(!validate_decision(harvest, ada, world, simulation.agents(), error));
  assert(SimulationTestAccess::execute(simulation, ada, harvest) ==
         "aucun arbre vivant a prelever");
  assert(ada.wood_inventory == wood_before_failed_harvest);
  assert(world.living_trees(empty_cell) == 0);
  assert(ada.project.progress == project_progress_before_failed_harvest);

  // Trois assemblages au même emplacement consomment chacun une unité.
  const Position site = ada.position;
  ada.project.status = ProjectStatus::Blocked;
  ada.project.blocked_reason = "La capacite de construire un abri manque.";
  ada.project.missing_capability = "build_shelter";
  ada.wood_inventory = 3;
  for (int expected = 1; expected <= 2; ++expected) {
    assert(validate_decision(assemble, ada, world, simulation.agents(), error));
    assert(SimulationTestAccess::execute(simulation, ada, assemble) ==
           "assemble l'abri");
    assert(ada.wood_inventory == 3 - expected);
    assert(ada.shelter_construction.has_value());
    assert(ada.shelter_construction->position == site);
    assert(ada.shelter_construction->progress == expected);
    assert(world.shelter_level(site) == 0);
  }
  assert(SimulationTestAccess::execute(simulation, ada, assemble) ==
         "construit un abri");
  assert(ada.wood_inventory == 0);
  assert(!ada.shelter_construction.has_value());
  assert(world.shelter_level(site) == 1);
  assert(ada.project.status == ProjectStatus::Active);
  assert(ada.project.blocked_reason.empty());
  assert(ada.project.missing_capability.empty());

  // Sans bois, sur un abri, et loin du chantier : aucun effet.
  const int completed_project_progress = ada.project.progress;
  assert(SimulationTestAccess::execute(simulation, ada, assemble) ==
         "assemblage impossible");
  assert(ada.wood_inventory == 0);
  assert(world.shelter_level(site) == 1);
  assert(ada.project.progress == completed_project_progress);

  ada.wood_inventory = 1;
  assert(!validate_decision(assemble, ada, world, simulation.agents(), error));
  assert(SimulationTestAccess::execute(simulation, ada, assemble) ==
         "assemblage impossible");
  assert(ada.wood_inventory == 1);
  assert(world.shelter_level(site) == 1);
  assert(ada.project.progress == completed_project_progress);

  ada.position = {7, 1};
  ada.wood_inventory = 2;
  assert(SimulationTestAccess::execute(simulation, ada, assemble) ==
         "assemble l'abri");
  assert(ada.shelter_construction->progress == 1);
  const int wood_before_wrong_site = ada.wood_inventory;
  const int progress_before_wrong_site = ada.shelter_construction->progress;
  ada.position = {8, 1};
  assert(!validate_decision(assemble, ada, world, simulation.agents(), error));
  assert(SimulationTestAccess::execute(simulation, ada, assemble) ==
         "assemblage impossible");
  assert(ada.wood_inventory == wood_before_wrong_site);
  assert(ada.shelter_construction->progress == progress_before_wrong_site);
  assert(world.shelter_level(ada.position) == 0);
  assert(ada.project.progress == completed_project_progress);

  // Le mécanisme antérieur de construction avec matériaux au sol reste couvert.
  Logger legacy_logger("/tmp/autopoiesis-legacy-shelter-tests");
  Simulation legacy_simulation(42, decider, legacy_logger);
  auto& legacy_world = SimulationTestAccess::world(legacy_simulation);
  auto& legacy_ada = SimulationTestAccess::ada(legacy_simulation);
  const Position legacy_site = legacy_ada.position;
  const Decision build{DecisionType::Action, "build_shelter", json::object(),
                       "Je construis mon foyer."};
  legacy_world.add_materials(legacy_site, 3, 2);
  const auto actions =
      available_actions(legacy_ada, legacy_world, legacy_simulation.agents());
  assert(std::find(actions.begin(), actions.end(), "build_shelter") !=
         actions.end());
  assert(validate_decision(build, legacy_ada, legacy_world,
                           legacy_simulation.agents(), error));
  assert(SimulationTestAccess::execute(legacy_simulation, legacy_ada, build) ==
         "construit un abri");
  assert(legacy_world.wood(legacy_site) == 0);
  assert(legacy_world.fibers(legacy_site) == 0);
  assert(legacy_world.shelter_level(legacy_site) == 1);
}
