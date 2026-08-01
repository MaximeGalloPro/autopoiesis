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

// A people mechanism is selected once for the whole world. An individual skill
// is deliberately not selectable here: its progress remains on each Agent.
enum class FeatureKind { PeopleMechanism, IndividualSkill };

struct FeatureDefinition {
  std::string key;
  int version{1};
  std::string title;
  bool default_active{};
  FeatureKind kind{FeatureKind::PeopleMechanism};
  std::vector<std::string> dependencies;
  std::vector<FeatureCard> cards;
};

struct ActiveFeature {
  std::string key;
  int version{1};
  friend bool operator==(const ActiveFeature&, const ActiveFeature&) = default;
};

// This is a declarative, version-pinned startup configuration. It never
// contains code or an action to execute and is immutable once a world starts.
struct WorldProfile {
  int schema_version{1};
  std::vector<ActiveFeature> active_features;
  friend bool operator==(const WorldProfile&, const WorldProfile&) = default;
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

  WorldProfile default_profile() const;
  bool valid_profile(const WorldProfile& profile) const;
  bool profile_active(const WorldProfile& profile, const std::string& key,
                      int version = 0) const;
  bool activate_for_startup(WorldProfile& profile, const std::string& key,
                            int version = 0) const;
  bool deactivate_for_startup(WorldProfile& profile, const std::string& key,
                              int version = 0) const;
  json profile_json(const WorldProfile& profile) const;
  WorldProfile profile_from_json(const json& state) const;

  // Kept for callers migrating from the former append-only activation list.
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
