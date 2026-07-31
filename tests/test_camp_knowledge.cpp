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
  static Agent& agent(Simulation& simulation, std::size_t index) {
    return simulation.agents_.at(index);
  }
  static Perception perceive(Simulation& simulation, Agent& agent) {
    return simulation.perceive(agent);
  }
  static std::string execute(Simulation& simulation, Agent& agent, const Decision& decision) {
    return simulation.execute(agent, decision);
  }
  static void set_day(Simulation& simulation, int day) {
    simulation.day_ = day;
    simulation.date_ = date_from_absolute_day(day);
  }
};
}

namespace {
bool includes_recipe(const json& recipes, const std::string& key) {
  return std::any_of(recipes.begin(), recipes.end(), [&](const json& recipe) {
    return recipe.value("id", "") == key;
  });
}

Decision share_with(const Agent& target) {
  return {DecisionType::Action, "share_map_knowledge", {{"target_id", target.id}}, "test"};
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("autopoiesis-camp-knowledge-" + std::to_string(getpid()));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto checkpoint = root / "state.json";

  Logger logger(root.string());
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42, decider, logger, nullptr, checkpoint.string());
  SimulationTestAccess::set_day(simulation, 1);
  auto& world = SimulationTestAccess::world(simulation);
  auto& group = SimulationTestAccess::group(simulation);
  auto& ada = SimulationTestAccess::agent(simulation, 0);
  auto& borin = SimulationTestAccess::agent(simulation, 1);
  const Position fire{13, 2};
  assert(world.place_campfire(fire));
  ada.position = {12, 2};
  borin.position = {14, 2};

  add_skill_experience(ada, Skill::Woodcutting, 19);
  const auto perception = SimulationTestAccess::perceive(simulation, ada);
  SimulationTestAccess::perceive(simulation, borin);
  assert(ada.home_camp == fire);
  assert(borin.home_camp == fire);
  assert(ada.family_id == primary_family_id);
  assert(borin.family_id == primary_family_id);
  assert(group.primary_base().kind == BaseKind::Campfire);

  const auto camp_knowledge = perception.value.at("camp_knowledge");
  assert(includes_recipe(camp_knowledge.at("recipes"), "axe"));
  assert(camp_knowledge.at("skills").at("woodcutting").at("level") == 3);
  const auto shared_perception = SimulationTestAccess::perceive(simulation, ada).value;
  assert(!shared_perception.at("map_sharing_opportunities").empty());
  assert(std::ranges::find(shared_perception.at("available_actions").begin(),
                           shared_perception.at("available_actions").end(),
                           "share_map_knowledge") !=
         shared_perception.at("available_actions").end());

  ada.map_memory.clear();
  borin.map_memory.clear();
  ada.map_memory[{20, 20}] = Terrain::Tree;
  ada.map_memory[{21, 20}] = Terrain::Ground;
  borin.map_memory[{20, 20}] = Terrain::Tree;
  assert(!borin.map_memory.contains({21, 20}));
  assert(!borin.map_memory.contains({22, 20}));

  const auto actions = available_actions(ada, world, simulation.agents(), 1, DayPhase::Day);
  assert(std::ranges::find(actions, "share_map_knowledge") != actions.end());
  const auto sharing = map_sharing_opportunities(ada, world, simulation.agents());
  assert(sharing.size() == 1);
  assert(sharing.at(0).at("target_id") == borin.id);
  assert(sharing.at(0).at("cell_count") == 1);
  const auto share = share_with(borin);
  std::string validation_error;
  assert(validate_decision(share, ada, world, simulation.agents(), validation_error, 1,
                           DayPhase::Day));
  assert(SimulationTestAccess::execute(simulation, ada, share) ==
         "transmet 1 case cartographique a " + borin.name);
  assert(borin.map_memory.at({20, 20}) == Terrain::Tree);
  assert(borin.map_memory.at({21, 20}) == Terrain::Ground);
  assert(!borin.map_memory.contains({22, 20}));
  assert(borin.map_memory.size() <= static_cast<std::size_t>(World::width * World::height));

  ada.map_memory[{World::width, 0}] = Terrain::Ground;
  const auto target_before = borin.map_memory;
  assert(!validate_decision(share, ada, world, simulation.agents(), validation_error, 1,
                            DayPhase::Day));
  assert(SimulationTestAccess::execute(simulation, ada, share) ==
         "transmission cartographique indisponible");
  assert(borin.map_memory == target_before);
  ada.map_memory.erase({World::width, 0});

  simulation.save_checkpoint();
  std::mt19937 restored_rng(7);
  LocalDecider restored_decider(restored_rng);
  Logger restored_logger(root.string());
  Simulation restored(7, restored_decider, restored_logger, nullptr, checkpoint.string());
  assert(restored.restored_checkpoint());
  assert(restored.group().primary_base().kind == BaseKind::Campfire);
  assert(restored.agents().at(0).family_id == primary_family_id);
  assert(restored.agents().at(1).family_id == primary_family_id);
  assert(restored.agents().at(1).map_memory.at({21, 20}) == Terrain::Ground);
  assert(restored.group().camp_knowledge_json() == group.camp_knowledge_json());

  json legacy_state;
  {
    std::ifstream input(checkpoint);
    input >> legacy_state;
  }
  legacy_state.at("group").erase("camp_knowledge");
  const auto legacy_checkpoint = root / "legacy-state.json";
  {
    std::ofstream output(legacy_checkpoint);
    output << legacy_state;
  }
  std::mt19937 legacy_rng(11);
  LocalDecider legacy_decider(legacy_rng);
  Logger legacy_logger(root.string());
  Simulation legacy(11, legacy_decider, legacy_logger, nullptr, legacy_checkpoint.string());
  assert(legacy.restored_checkpoint());
  assert(legacy.group().primary_base().kind == BaseKind::Campfire);
  assert(includes_recipe(legacy.group().camp_knowledge_json().at("recipes"), "axe"));

  std::filesystem::remove_all(root);
}
