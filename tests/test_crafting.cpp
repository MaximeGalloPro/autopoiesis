#include "autopoiesis/simulation.hpp"
#include "autopoiesis/capability_registry.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace apo;

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& agent(Simulation& simulation) { return simulation.agents_.front(); }
  static Perception perceive(Simulation& simulation,Agent& agent) { return simulation.perceive(agent); }
  static std::string execute(Simulation& simulation,Agent& agent,const Decision& decision) {
    return simulation.execute(agent,decision);
  }
};
}

static Decision craft(std::string recipe) {
  return {DecisionType::Action,"craft_camp_item",{{"recipe",std::move(recipe)}},"test"};
}

int main() {
  const auto registry_path=std::filesystem::temp_directory_path()/"autopoiesis-capabilities-test.json";
  {
    std::ofstream output(registry_path);
    output << R"({"schema_version":1,"recipes":[{"id":"woven_mat","cost":{"wood":0,"branches":2,"iron_ore":0,"items":{}},"output":{"item":"woven_mat","quantity":1}}]})";
  }
  const auto custom=CapabilityRegistry::load(registry_path);
  assert(custom.recipes().size()==1&&custom.recipes().front().output=="woven_mat");
  assert(custom.recipe("woven_mat")!=nullptr);
  assert(custom.manifest()["recipes"].size()==1);
  std::filesystem::remove(registry_path);
  bool invalid_rejected=false;
  {
    std::ofstream output(registry_path);
    output << R"({"schema_version":1,"recipes":[{"id":"same","cost":{"items":{},"wood":0,"branches":0,"iron_ore":0},"output":{"item":"one","quantity":1}},{"id":"same","cost":{"items":{},"wood":0,"branches":0,"iron_ore":0},"output":{"item":"two","quantity":1}}]})";
  }
  try { static_cast<void>(CapabilityRegistry::load(registry_path)); }
  catch (const std::runtime_error&) { invalid_rejected=true; }
  std::filesystem::remove(registry_path);
  assert(invalid_rejected);

  const auto& recipes=crafting_recipes();
  assert(std::ranges::any_of(recipes,[](const CraftingRecipe& recipe){return recipe.key=="wooden_handle";}));
  assert(std::ranges::any_of(recipes,[](const CraftingRecipe& recipe){return recipe.key=="charcoal";}));
  assert(std::ranges::any_of(recipes,[](const CraftingRecipe& recipe){return recipe.key=="rope";}));

  Logger logger("/tmp/autopoiesis-crafting-tests");
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42,decider,logger);
  auto& world=SimulationTestAccess::world(simulation);
  auto& crafter=SimulationTestAccess::agent(simulation);
  const Position fire{13,2};
  assert(world.place_campfire(fire));
  assert(world.store_materials(fire,4,4));
  crafter.position={12,2};

  auto actions=available_actions(crafter,world,simulation.agents(),1,DayPhase::Day);
  assert(std::ranges::find(actions,"craft_camp_item")!=actions.end());
  const auto planned=decider.decide(SimulationTestAccess::perceive(simulation,crafter));
  assert(planned.action=="craft_camp_item");
  assert(planned.parameters["recipe"]=="wooden_handle");
  std::string error;
  assert(validate_decision(craft("wooden_handle"),crafter,world,simulation.agents(),error,
                           1,DayPhase::Day));
  assert(SimulationTestAccess::execute(simulation,crafter,craft("wooden_handle"))==
         "fabrique wooden_handle");
  assert(world.stored_wood(fire)==3);
  assert(world.stored_item(fire,CraftItem::WoodenHandle)==1);

  assert(SimulationTestAccess::execute(simulation,crafter,craft("charcoal"))==
         "fabrique charcoal");
  assert(world.stored_wood(fire)==1);
  assert(world.stored_item(fire,CraftItem::Charcoal)==1);
  assert(SimulationTestAccess::execute(simulation,crafter,craft("rope"))=="fabrique rope");
  assert(world.stored_branches(fire)==1);
  assert(world.stored_item(fire,CraftItem::Rope)==1);

  const auto before=world.checkpoint();
  assert(SimulationTestAccess::execute(simulation,crafter,craft("charcoal"))==
         "recette indisponible");
  assert(world.checkpoint()==before);

  World restored(7);
  restored.restore_checkpoint(world.checkpoint());
  assert(restored.stored_item(fire,CraftItem::WoodenHandle)==1);
  assert(restored.stored_item(fire,CraftItem::Charcoal)==1);
  assert(restored.stored_item(fire,CraftItem::Rope)==1);

  auto legacy_checkpoint=world.checkpoint();
  for(auto& cell:legacy_checkpoint["construction"])
    for(auto& item:cell["crafted_stockpile"])
      if(item["item"]=="wooden_handle")item["item"]=static_cast<int>(CraftItem::WoodenHandle);
  World legacy_restored(8);
  legacy_restored.restore_checkpoint(legacy_checkpoint);
  assert(legacy_restored.stored_item(fire,CraftItem::WoodenHandle)==1);
}
