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

struct FeatureCard {
  std::string id;
  std::string type;
  std::string title;
  std::string action;
  std::string target_feature;
};

struct FeatureDefinition {
  std::string key;
  int version{1};
  std::string title;
  bool default_active{};
  std::vector<std::string> dependencies;
  std::vector<FeatureCard> cards;
};

struct ActiveFeature {
  std::string key;
  int version{1};
};

class FeatureRegistry {
 public:
  static FeatureRegistry load(const std::filesystem::path& path);
  static const FeatureRegistry& defaults();

  explicit FeatureRegistry(std::vector<FeatureDefinition> features)
      : features_(std::move(features)) {}
  const FeatureDefinition* feature(const std::string& key, int version = 0) const;
  const FeatureCard* card(const std::string& key) const;
  std::vector<ActiveFeature> default_activations() const;
  bool valid_activation(const ActiveFeature& activation,
                        const std::vector<ActiveFeature>& active) const;
  json manifest() const;

 private:
  std::vector<FeatureDefinition> features_;
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
