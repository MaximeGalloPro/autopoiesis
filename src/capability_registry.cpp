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
    if (operation != "craft_recipe") throw std::runtime_error("unknown capability action operation " + operation);
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
  if (!document.is_object() || document.value("schema_version", 0) != 1 ||
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
    FeatureDefinition feature{value["id"].get<std::string>(), value["version"].get<int>(),
                              value["title"].get<std::string>(),
                              value.value("default_active", false), {}, {}};
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
  return FeatureRegistry(std::move(features));
}

const FeatureRegistry& FeatureRegistry::defaults() {
  static const FeatureRegistry registry = FeatureRegistry::load(feature_path());
  return registry;
}

const FeatureDefinition* FeatureRegistry::feature(const std::string& key, int version) const {
  const auto found = std::find_if(features_.begin(), features_.end(), [&](const auto& candidate) {
    return candidate.key == key && (version == 0 || candidate.version == version);
  });
  return found == features_.end() ? nullptr : &*found;
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
    if (feature.default_active) result.push_back({feature.key, feature.version});
  return result;
}

bool FeatureRegistry::valid_activation(const ActiveFeature& activation,
                                       const std::vector<ActiveFeature>& active) const {
  const auto* definition = feature(activation.key, activation.version);
  if (!definition) return false;
  if (std::any_of(active.begin(), active.end(), [&](const auto& candidate) {
        return candidate.key == activation.key;
      })) return false;
  return std::all_of(definition->dependencies.begin(), definition->dependencies.end(),
                     [&](const auto& dependency) {
                       return std::any_of(active.begin(), active.end(), [&](const auto& candidate) {
                         return candidate.key == dependency;
                       });
                     });
}

json FeatureRegistry::manifest() const {
  json result = {{"schema_version", 1}, {"features", json::array()}};
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
                                    {"dependencies", feature.dependencies}, {"cards", std::move(cards)}});
  }
  return result;
}
}
