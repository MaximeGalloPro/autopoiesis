#pragma once

#include "types.hpp"

#include <filesystem>
#include <utility>

namespace apo {

struct CraftingRecipe {
  std::string key;
  int wood{};
  int branches{};
  int iron_ore{};
  std::vector<std::pair<std::string, int>> items;
  std::string output;
  int output_count{1};
};

struct ActionDefinition {
  std::string id;
  std::string operation;
};

class ActionRegistry {
 public:
  static ActionRegistry load(const std::filesystem::path& path);
  static const ActionRegistry& defaults();
  explicit ActionRegistry(std::vector<ActionDefinition> actions) : actions_(std::move(actions)) {}
  const ActionDefinition* action(const std::string& id) const;

 private:
  std::vector<ActionDefinition> actions_;
};

class CapabilityRegistry {
 public:
  static CapabilityRegistry load(const std::filesystem::path& path);
  static const CapabilityRegistry& defaults();

  explicit CapabilityRegistry(std::vector<CraftingRecipe> recipes) : recipes_(std::move(recipes)) {}

  const std::vector<CraftingRecipe>& recipes() const { return recipes_; }
  const CraftingRecipe* recipe(const std::string& key) const;
  json manifest() const;

 private:
  std::vector<CraftingRecipe> recipes_;
};

const std::vector<CraftingRecipe>& crafting_recipes();

}
