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
  const auto registry_root=std::filesystem::temp_directory_path()/"autopoiesis-capabilities-test";
  const auto registry_path=registry_root/"core/recipes.json";
  std::filesystem::create_directories(registry_path.parent_path());
  {
    std::ofstream output(registry_root/"core/actions.json");
    output << R"({"schema_version":1,"actions":[{"id":"craft_camp_item","operation":"craft_recipe"}]})";
  }
  {
    std::ofstream output(registry_root/"features.json");
    output << R"({"schema_version":1,"features":[
      {"id":"core_survival","version":1,"title":"Survie","default_active":true,"dependencies":[],"cards":[
        {"id":"craft_camp_item","type":"action","title":"Fabriquer","action":"craft_camp_item"}]},
      {"id":"camp_cooking","version":1,"title":"Cuisine","default_active":false,"dependencies":["core_survival"],"cards":[]}
    ]})";
  }
  {
    std::ofstream output(registry_path);
    output << R"({"schema_version":1,"recipes":[
      {"id":"wooden_handle","cost":{"wood":1,"branches":0,"iron_ore":0,"items":{}},"output":{"item":"wooden_handle","quantity":1}},
      {"id":"charcoal","cost":{"wood":2,"branches":0,"iron_ore":0,"items":{}},"output":{"item":"charcoal","quantity":1}},
      {"id":"rope","cost":{"wood":0,"branches":3,"iron_ore":0,"items":{}},"output":{"item":"rope","quantity":1}},
      {"id":"iron_ingot","cost":{"wood":0,"branches":0,"iron_ore":2,"items":{"charcoal":1}},"output":{"item":"iron_ingot","quantity":1}},
      {"id":"axe","cost":{"wood":0,"branches":0,"iron_ore":0,"items":{"wooden_handle":1,"iron_ingot":1}},"output":{"item":"axe","quantity":1}},
      {"id":"woven_mat","cost":{"wood":0,"branches":2,"iron_ore":0,"items":{}},"output":{"item":"woven_mat","quantity":1}}
    ]})";
  }
  setenv("AUTOPOIESIS_CAPABILITY_ROOT",registry_root.c_str(),1);
  const auto custom=CapabilityRegistry::load(registry_path);
  const auto features=FeatureRegistry::load(registry_root/"features.json");
  assert(features.feature("core_survival",1)!=nullptr);
  assert(features.card("craft_camp_item")!=nullptr);
  assert(features.default_activations().size()==1);
  assert(features.manifest()["features"].size()==2);
  assert(ActionRegistry::load(registry_root/"core/actions.json").action("craft_camp_item") != nullptr);
  assert(custom.recipes().size()==6&&custom.recipe("woven_mat")!=nullptr);
  assert(custom.recipes().back().output=="woven_mat");
  assert(custom.manifest()["recipes"].size()==6);
  const auto invalid_path=registry_root/"invalid.json";
  bool invalid_rejected=false;
  {
    std::ofstream output(invalid_path);
    output << R"({"schema_version":1,"recipes":[{"id":"same","cost":{"items":{},"wood":0,"branches":0,"iron_ore":0},"output":{"item":"one","quantity":1}},{"id":"same","cost":{"items":{},"wood":0,"branches":0,"iron_ore":0},"output":{"item":"two","quantity":1}}]})";
  }
  try { static_cast<void>(CapabilityRegistry::load(invalid_path)); }
  catch (const std::runtime_error&) { invalid_rejected=true; }
  std::filesystem::remove(invalid_path);
  assert(invalid_rejected);

  const auto& recipes=crafting_recipes();
  assert(std::ranges::any_of(recipes,[](const CraftingRecipe& recipe){return recipe.key=="wooden_handle";}));
  assert(std::ranges::any_of(recipes,[](const CraftingRecipe& recipe){return recipe.key=="charcoal";}));
  assert(std::ranges::any_of(recipes,[](const CraftingRecipe& recipe){return recipe.key=="rope";}));
  assert(std::ranges::any_of(recipes,[](const CraftingRecipe& recipe){return recipe.key=="woven_mat";}));
  std::string error;

  Logger logger("/tmp/autopoiesis-crafting-tests");
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  auto startup_profile=features.default_profile();
  assert(features.activate_for_startup(startup_profile,"camp_cooking"));
  Simulation simulation(42,decider,logger,nullptr,(registry_root/"checkpoint.json").string(),
                        startup_profile);
  assert(simulation.feature_active("core_survival",1));
  assert(simulation.feature_active("camp_cooking",1));
  auto& world=SimulationTestAccess::world(simulation);
  auto& crafter=SimulationTestAccess::agent(simulation);
  const Position fire{13,2};
  assert(world.place_campfire(fire));
  assert(world.store_materials(fire,4,8));
  crafter.position={12,2};

  auto actions=available_actions(crafter,world,simulation.agents(),1,DayPhase::Day);
  assert(std::ranges::find(actions,"craft_camp_item")!=actions.end());
  const auto planned=decider.decide(SimulationTestAccess::perceive(simulation,crafter));
  assert(planned.action=="craft_camp_item");
  const auto craftable=world.craftable_recipes(fire);
  assert(planned.parameters.contains("recipe")&&planned.parameters["recipe"].is_string());
  assert(std::ranges::find(craftable,planned.parameters["recipe"].get<std::string>())!=craftable.end());
  assert(validate_decision(craft("woven_mat"),crafter,world,simulation.agents(),error,
                           1,DayPhase::Day));
  assert(SimulationTestAccess::execute(simulation,crafter,craft("woven_mat"))==
         "fabrique woven_mat");
  assert(world.stored_branches(fire)==6);
  assert(world.stored_item(fire,"woven_mat")==1);
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
  assert(world.stored_branches(fire)==3);
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
  assert(restored.stored_item(fire,"woven_mat")==1);

  auto legacy_checkpoint=world.checkpoint();
  for(auto& cell:legacy_checkpoint["construction"])
    for(auto& item:cell["crafted_stockpile"])
      if(item["item"]=="wooden_handle")item["item"]=static_cast<int>(CraftItem::WoodenHandle);
  World legacy_restored(8);
  legacy_restored.restore_checkpoint(legacy_checkpoint);
  assert(legacy_restored.stored_item(fire,CraftItem::WoodenHandle)==1);
  simulation.save_checkpoint();
  std::mt19937 restored_rng(7);
  LocalDecider restored_decider(restored_rng);
  Logger restored_logger("/tmp/autopoiesis-crafting-restored");
  Simulation restored_simulation(7,restored_decider,restored_logger,nullptr,registry_root/"checkpoint.json");
  assert(restored_simulation.restored_checkpoint());
  assert(restored_simulation.feature_active("core_survival",1));
  assert(restored_simulation.feature_active("camp_cooking",1));

  json legacy_profile_checkpoint;
  {
    std::ifstream input(registry_root/"checkpoint.json");
    input>>legacy_profile_checkpoint;
  }
  assert(legacy_profile_checkpoint.at("group").contains("world_profile"));
  assert(!legacy_profile_checkpoint.contains("active_features"));
  // Feature packages predate the persistent Group model: this is the former
  // top-level representation used by existing saves.
  legacy_profile_checkpoint.erase("group");
  legacy_profile_checkpoint["active_features"]={
      {{"id","core_survival"},{"version",1}},
      {{"id","camp_cooking"},{"version",1}}};
  const auto legacy_profile_path=registry_root/"legacy-profile-checkpoint.json";
  {
    std::ofstream output(legacy_profile_path);
    output<<legacy_profile_checkpoint;
  }
  std::mt19937 legacy_profile_rng(9);
  LocalDecider legacy_profile_decider(legacy_profile_rng);
  Logger legacy_profile_logger("/tmp/autopoiesis-crafting-legacy-profile");
  Simulation legacy_profile_simulation(9,legacy_profile_decider,legacy_profile_logger,nullptr,
                                       legacy_profile_path.string());
  assert(legacy_profile_simulation.restored_checkpoint());
  assert(legacy_profile_simulation.feature_active("core_survival",1));
  assert(legacy_profile_simulation.feature_active("camp_cooking",1));
  unsetenv("AUTOPOIESIS_CAPABILITY_ROOT");
  std::filesystem::remove_all(registry_root);
}
