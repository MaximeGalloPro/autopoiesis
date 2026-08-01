#include "autopoiesis/capability_registry.hpp"

#include <cstdlib>
#include <fstream>
#include <regex>
#include <set>
#include <stdexcept>

namespace apo {
namespace {
bool positive_identifier(const json& value) {
  return value.is_string() && std::regex_match(value.get<std::string>(),
                                                std::regex(R"([a-z][a-z0-9_]{0,63})"));
}

int non_negative(const json& value, const char* field, const std::string& recipe) {
  if (!value.is_number_integer() || value.get<int>() < 0 || value.get<int>() > 1000000)
    throw std::runtime_error("invalid recipe " + recipe + " field " + field);
  return value.get<int>();
}

std::string feature_kind_name(FeatureKind kind) {
  switch (kind) {
    case FeatureKind::PeopleMechanism:
      return "people_mechanism";
    case FeatureKind::IndividualSkill:
      return "individual_skill";
  }
  return "unknown";
}

FeatureKind feature_kind_from_json(const json& value, int schema_version,
                                   const std::string& feature) {
  if (schema_version == 1 && value.is_null()) return FeatureKind::PeopleMechanism;
  if (!value.is_string()) throw std::runtime_error("invalid feature kind " + feature);
  if (value.get<std::string>() == "people_mechanism") return FeatureKind::PeopleMechanism;
  if (value.get<std::string>() == "individual_skill") return FeatureKind::IndividualSkill;
  throw std::runtime_error("invalid feature kind " + feature);
}

void canonicalize(WorldProfile& profile) {
  std::sort(profile.active_features.begin(), profile.active_features.end(),
            [](const ActiveFeature& left, const ActiveFeature& right) {
              return left.key != right.key ? left.key < right.key : left.version < right.version;
            });
}

CapabilityRegistry parse(json document) {
  if (!document.is_object() || document.value("schema_version", 0) != 1 ||
      !document.contains("recipes") || !document["recipes"].is_array())
    throw std::runtime_error("invalid capability recipe document");
  std::vector<CraftingRecipe> recipes;
  for (const auto& value : document["recipes"]) {
    if (!value.is_object() || !positive_identifier(value.value("id", json{})) ||
        !value.contains("cost") || !value["cost"].is_object() ||
        !value.contains("output") || !value["output"].is_object())
      throw std::runtime_error("invalid capability recipe entry");
    const auto key = value["id"].get<std::string>();
    if (std::any_of(recipes.begin(), recipes.end(), [&](const auto& candidate) { return candidate.key == key; }))
      throw std::runtime_error("duplicate capability recipe " + key);
    const auto& cost = value["cost"];
    const auto& output = value["output"];
    if (!positive_identifier(output.value("item", json{})))
      throw std::runtime_error("invalid output item for recipe " + key);
    CraftingRecipe recipe{key,
                          non_negative(cost.value("wood", 0), "wood", key),
                          non_negative(cost.value("branches", 0), "branches", key),
                          non_negative(cost.value("iron_ore", 0), "iron_ore", key),
                          {}, output["item"].get<std::string>(),
                          non_negative(output.value("quantity", 0), "quantity", key)};
    if (recipe.output_count <= 0) throw std::runtime_error("empty output for recipe " + key);
    const auto items = cost.value("items", json::object());
    if (!items.is_object()) throw std::runtime_error("invalid item costs for recipe " + key);
    for (const auto& [item, amount] : items.items()) {
      if (!std::regex_match(item, std::regex(R"([a-z][a-z0-9_]{0,63})")))
        throw std::runtime_error("invalid item cost for recipe " + key);
      const int quantity = non_negative(amount, "items", key);
      if (quantity <= 0) throw std::runtime_error("empty item cost for recipe " + key);
      recipe.items.emplace_back(item, quantity);
    }
    recipes.push_back(std::move(recipe));
  }
  if (recipes.empty()) throw std::runtime_error("capability recipe document is empty");
  return CapabilityRegistry(std::move(recipes));
}

std::filesystem::path default_path() {
  if (const char* configured = std::getenv("AUTOPOIESIS_CAPABILITY_ROOT"); configured && *configured)
    return std::filesystem::path(configured) / "core/recipes.json";
#ifdef AUTOPOIESIS_SOURCE_ROOT
  return std::filesystem::path(AUTOPOIESIS_SOURCE_ROOT) / "capabilities/core/recipes.json";
#else
  return std::filesystem::path("capabilities/core/recipes.json");
#endif
}
std::filesystem::path action_path() {
  if (const char* configured = std::getenv("AUTOPOIESIS_CAPABILITY_ROOT"); configured && *configured)
    return std::filesystem::path(configured) / "core/actions.json";
#ifdef AUTOPOIESIS_SOURCE_ROOT
  return std::filesystem::path(AUTOPOIESIS_SOURCE_ROOT) / "capabilities/core/actions.json";
#else
  return std::filesystem::path("capabilities/core/actions.json");
#endif
}

std::filesystem::path feature_path() {
  if (const char* configured = std::getenv("AUTOPOIESIS_CAPABILITY_ROOT"); configured && *configured)
    return std::filesystem::path(configured) / "features.json";
#ifdef AUTOPOIESIS_SOURCE_ROOT
  return std::filesystem::path(AUTOPOIESIS_SOURCE_ROOT) / "capabilities/features.json";
#else
  return std::filesystem::path("capabilities/features.json");
#endif
}
}

CapabilityRegistry CapabilityRegistry::load(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open capability registry: " + path.string());
  json document;
  input >> document;
  return parse(std::move(document));
}

const CapabilityRegistry& CapabilityRegistry::defaults() {
  static const CapabilityRegistry registry = CapabilityRegistry::load(default_path());
  return registry;
}

const CraftingRecipe* CapabilityRegistry::recipe(const std::string& key) const {
  const auto found = std::find_if(recipes_.begin(), recipes_.end(),
                                  [&](const auto& candidate) { return candidate.key == key; });
  return found == recipes_.end() ? nullptr : &*found;
}

json CapabilityRegistry::manifest() const {
  json result = { {"schema_version", 1}, {"recipes", json::array()} };
  for (const auto& recipe : recipes_) {
    json items = json::object();
    for (const auto& [item, amount] : recipe.items) items[item] = amount;
    result["recipes"].push_back({
        {"id", recipe.key},
        {"cost", {{"wood", recipe.wood}, {"branches", recipe.branches},
                   {"iron_ore", recipe.iron_ore}, {"items", std::move(items)}}},
        {"output", {{"item", recipe.output}, {"quantity", recipe.output_count}}}});
  }
  return result;
}

const std::vector<CraftingRecipe>& crafting_recipes() {
  return CapabilityRegistry::defaults().recipes();
}

ActionRegistry ActionRegistry::load(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open capability action registry: " + path.string());
  json document; input >> document;
  if (!document.is_object() || document.value("schema_version", 0) != 1 ||
      !document.contains("actions") || !document["actions"].is_array() || document["actions"].empty())
    throw std::runtime_error("invalid capability action document");
  std::vector<ActionDefinition> actions;
  for (const auto& value : document["actions"]) {
    if (!value.is_object() || !positive_identifier(value.value("id", json{})) ||
        !value.value("operation", json{}).is_string())
      throw std::runtime_error("invalid capability action entry");
    const auto id = value["id"].get<std::string>();
    const auto operation = value["operation"].get<std::string>();
    if (operation != "craft_recipe" && operation != "upgrade_camp_chest")
      throw std::runtime_error("unknown capability action operation " + operation);
    if (std::any_of(actions.begin(), actions.end(), [&](const auto& candidate) { return candidate.id == id; }))
      throw std::runtime_error("duplicate capability action " + id);
    actions.push_back({id, operation});
  }
  return ActionRegistry(std::move(actions));
}

const ActionRegistry& ActionRegistry::defaults() {
  static const ActionRegistry registry = ActionRegistry::load(action_path());
  return registry;
}

const ActionDefinition* ActionRegistry::action(const std::string& id) const {
  const auto found = std::find_if(actions_.begin(), actions_.end(),
                                  [&](const auto& candidate) { return candidate.id == id; });
  return found == actions_.end() ? nullptr : &*found;
}

FeatureRegistry FeatureRegistry::load(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open feature registry: " + path.string());
  json document;
  input >> document;
  if (!document.is_object()) throw std::runtime_error("invalid feature registry document");
  const auto schema_version = document.value("schema_version", 0);
  if ((schema_version != 1 && schema_version != 2) ||
      !document.contains("features") || !document["features"].is_array() ||
      document["features"].empty())
    throw std::runtime_error("invalid feature registry document");

  std::vector<FeatureDefinition> features;
  std::set<std::string> card_ids;
  for (const auto& value : document["features"]) {
    if (!value.is_object() || !positive_identifier(value.value("id", json{})) ||
        !value.value("version", json{}).is_number_integer() ||
        value.value("version", 0) <= 0 || value.value("version", 0) > 1000000 ||
        !value.value("title", json{}).is_string())
      throw std::runtime_error("invalid feature definition");
    const auto key = value["id"].get<std::string>();
    const auto kind = feature_kind_from_json(
        value.contains("kind") ? value.at("kind") : json(nullptr), schema_version, key);
    FeatureDefinition feature{key, value["version"].get<int>(), value["title"].get<std::string>(),
                              value.value("default_active", false), kind, {}, {}};
    if (!std::all_of(features.begin(), features.end(), [&](const auto& candidate) {
          return candidate.key != feature.key || candidate.version != feature.version;
        }))
      throw std::runtime_error("duplicate feature " + feature.key);
    const auto dependencies = value.value("dependencies", json::array());
    if (!dependencies.is_array()) throw std::runtime_error("invalid feature dependencies " + feature.key);
    for (const auto& dependency : dependencies) {
      if (!positive_identifier(dependency)) throw std::runtime_error("invalid feature dependency " + feature.key);
      feature.dependencies.push_back(dependency.get<std::string>());
    }
    const auto cards = value.value("cards", json::array());
    if (!cards.is_array()) throw std::runtime_error("invalid feature cards " + feature.key);
    if (feature.kind == FeatureKind::IndividualSkill &&
        (feature.default_active || !feature.dependencies.empty() || !cards.empty()))
      throw std::runtime_error("individual skill feature cannot be globally activated " + feature.key);
    for (const auto& card : cards) {
      if (!card.is_object() || !positive_identifier(card.value("id", json{})) ||
          !card.value("type", json{}).is_string() || !card.value("title", json{}).is_string())
        throw std::runtime_error("invalid feature card in " + feature.key);
      const auto id = card["id"].get<std::string>();
      const auto type = card["type"].get<std::string>();
      if (type != "action" && type != "feature_unlock")
        throw std::runtime_error("unknown feature card type " + type);
      if (!card_ids.insert(id).second) throw std::runtime_error("duplicate feature card " + id);
      FeatureCard definition{id, type, card["title"].get<std::string>(), {}, {}};
      if (type == "action") {
        if (!positive_identifier(card.value("action", json{})))
          throw std::runtime_error("feature action card requires an action " + id);
        definition.action = card["action"].get<std::string>();
      } else {
        if (!positive_identifier(card.value("target_feature", json{})))
          throw std::runtime_error("feature unlock card requires a target feature " + id);
        definition.target_feature = card["target_feature"].get<std::string>();
      }
      feature.cards.push_back(std::move(definition));
    }
    features.push_back(std::move(feature));
  }
  for (const auto& feature : features) {
    for (const auto& dependency : feature.dependencies)
      if (!std::any_of(features.begin(), features.end(), [&](const auto& candidate) {
            return candidate.key == dependency;
          }))
        throw std::runtime_error("unknown feature dependency " + dependency);
    for (const auto& card : feature.cards)
      if (card.type == "feature_unlock" &&
          !std::any_of(features.begin(), features.end(), [&](const auto& candidate) {
            return candidate.key == card.target_feature;
          }))
        throw std::runtime_error("unknown feature card target " + card.target_feature);
  }
  FeatureRegistry registry(std::move(features));
  if (!registry.valid_profile(registry.default_profile()))
    throw std::runtime_error("invalid default feature profile");
  return registry;
}

const FeatureRegistry& FeatureRegistry::defaults() {
  static const FeatureRegistry registry = FeatureRegistry::load(feature_path());
  return registry;
}

const FeatureDefinition* FeatureRegistry::feature(const std::string& key, int version) const {
  const FeatureDefinition* selected = nullptr;
  for (const auto& candidate : features_) {
    if (candidate.key != key || (version != 0 && candidate.version != version)) continue;
    if (!selected || candidate.version > selected->version) selected = &candidate;
  }
  return selected;
}

const FeatureCard* FeatureRegistry::card(const std::string& key) const {
  for (const auto& feature : features_)
    for (const auto& card : feature.cards)
      if (card.id == key) return &card;
  return nullptr;
}

std::vector<ActiveFeature> FeatureRegistry::default_activations() const {
  std::vector<ActiveFeature> result;
  for (const auto& feature : features_)
    if (feature.default_active && feature.kind == FeatureKind::PeopleMechanism)
      result.push_back({feature.key, feature.version});
  std::sort(result.begin(), result.end(), [](const ActiveFeature& left, const ActiveFeature& right) {
    return left.key != right.key ? left.key < right.key : left.version < right.version;
  });
  return result;
}

WorldProfile FeatureRegistry::default_profile() const {
  WorldProfile profile{1, default_activations()};
  canonicalize(profile);
  return profile;
}

bool FeatureRegistry::valid_profile(const WorldProfile& profile) const {
  if (profile.schema_version != 1) return false;
  std::set<std::string> active_keys;
  for (const auto& activation : profile.active_features) {
    const auto* definition = feature(activation.key, activation.version);
    if (!definition || definition->kind != FeatureKind::PeopleMechanism ||
        !active_keys.insert(activation.key).second)
      return false;
  }
  for (const auto& activation : profile.active_features) {
    const auto* definition = feature(activation.key, activation.version);
    if (!definition || !std::all_of(definition->dependencies.begin(), definition->dependencies.end(),
                                    [&](const std::string& dependency) {
                                      return active_keys.contains(dependency);
                                    }))
      return false;
  }
  return true;
}

bool FeatureRegistry::profile_active(const WorldProfile& profile, const std::string& key,
                                     int version) const {
  return std::any_of(profile.active_features.begin(), profile.active_features.end(),
                     [&](const ActiveFeature& activation) {
                       return activation.key == key && (version == 0 || activation.version == version);
                     });
}

bool FeatureRegistry::activate_for_startup(WorldProfile& profile, const std::string& key,
                                           int version) const {
  const auto* definition = feature(key, version);
  if (!definition || definition->kind != FeatureKind::PeopleMechanism) return false;
  WorldProfile candidate = profile;
  if (profile_active(candidate, definition->key)) return false;
  candidate.active_features.push_back({definition->key, definition->version});
  canonicalize(candidate);
  if (!valid_profile(candidate)) return false;
  profile = std::move(candidate);
  return true;
}

bool FeatureRegistry::deactivate_for_startup(WorldProfile& profile, const std::string& key,
                                             int version) const {
  WorldProfile candidate = profile;
  const auto found = std::find_if(candidate.active_features.begin(), candidate.active_features.end(),
                                  [&](const ActiveFeature& activation) {
                                    return activation.key == key &&
                                           (version == 0 || activation.version == version);
                                  });
  if (found == candidate.active_features.end()) return false;
  candidate.active_features.erase(found);
  canonicalize(candidate);
  if (!valid_profile(candidate)) return false;
  profile = std::move(candidate);
  return true;
}

json FeatureRegistry::profile_json(const WorldProfile& profile) const {
  if (!valid_profile(profile)) throw std::runtime_error("invalid world profile");
  WorldProfile canonical = profile;
  canonicalize(canonical);
  json active = json::array();
  for (const auto& activation : canonical.active_features)
    active.push_back({{"id", activation.key}, {"version", activation.version}});
  return {{"schema_version", canonical.schema_version}, {"active_features", std::move(active)}};
}

WorldProfile FeatureRegistry::profile_from_json(const json& state) const {
  if (!state.is_object() || state.value("schema_version", 0) != 1 ||
      !state.contains("active_features") || !state.at("active_features").is_array())
    throw std::runtime_error("checkpoint world profile is invalid");
  for (const auto& [key, value] : state.items()) {
    (void)value;
    if (key != "schema_version" && key != "active_features")
      throw std::runtime_error("checkpoint world profile field is unknown");
  }
  WorldProfile profile;
  for (const auto& value : state.at("active_features")) {
    if (!value.is_object() || !positive_identifier(value.value("id", json{})) ||
        !value.value("version", json{}).is_number_integer() ||
        value.value("version", 0) <= 0 || value.value("version", 0) > 1000000)
      throw std::runtime_error("checkpoint active feature is invalid");
    for (const auto& [key, field] : value.items()) {
      (void)field;
      if (key != "id" && key != "version")
        throw std::runtime_error("checkpoint active feature field is unknown");
    }
    profile.active_features.push_back({value.at("id").get<std::string>(),
                                       value.at("version").get<int>()});
  }
  canonicalize(profile);
  if (!valid_profile(profile)) throw std::runtime_error("checkpoint world profile is invalid");
  return profile;
}

bool FeatureRegistry::valid_activation(const ActiveFeature& activation,
                                       const std::vector<ActiveFeature>& active) const {
  WorldProfile candidate{1, active};
  candidate.active_features.push_back(activation);
  return valid_profile(candidate);
}

json FeatureRegistry::manifest() const {
  json result = {{"schema_version", 2}, {"features", json::array()}};
  for (const auto& feature : features_) {
    json cards = json::array();
    for (const auto& card : feature.cards) {
      json value = {{"id", card.id}, {"type", card.type}, {"title", card.title}};
      if (card.type == "action") value["action"] = card.action;
      else value["target_feature"] = card.target_feature;
      cards.push_back(std::move(value));
    }
    result["features"].push_back({{"id", feature.key}, {"version", feature.version},
                                    {"title", feature.title},
                                    {"default_active", feature.default_active},
                                    {"kind", feature_kind_name(feature.kind)},
                                    {"dependencies", feature.dependencies}, {"cards", std::move(cards)}});
  }
  return result;
}
}
