#include "autopoiesis/capability_registry.hpp"

#include <cstdlib>
#include <fstream>
#include <regex>
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
}
