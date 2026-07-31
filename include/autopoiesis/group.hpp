#pragma once

#include "types.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace apo {
inline constexpr std::string_view primary_group_id{"groupe-principal"};

enum class BaseKind { Campfire };
enum class BaseInfrastructure { Stockpile, Workshop };
enum class BasePrerequisite { Campfire, Stockpile };

struct GroupBase {
  BaseKind kind{BaseKind::Campfire};
};

struct BaseInfrastructureDefinition {
  BaseInfrastructure type{BaseInfrastructure::Stockpile};
  std::string_view key;
  BuildingType building_type{BuildingType::Stockpile};
  BuildingCost cost;
  BasePrerequisite prerequisite{BasePrerequisite::Campfire};
  int maximum_level{1};
};

inline const std::array<BaseInfrastructureDefinition, 2>& base_infrastructure_catalog() {
  static const std::array<BaseInfrastructureDefinition, 2> catalog{{
      {BaseInfrastructure::Stockpile, "stockpile", BuildingType::Stockpile,
       building_cost(BuildingType::Stockpile), BasePrerequisite::Campfire, 1},
      {BaseInfrastructure::Workshop, "workshop", BuildingType::Workshop,
       building_cost(BuildingType::Workshop), BasePrerequisite::Stockpile, 1},
  }};
  return catalog;
}

inline const BaseInfrastructureDefinition* base_infrastructure(BaseInfrastructure type) {
  const auto& catalog = base_infrastructure_catalog();
  for (const auto& definition : catalog)
    if (definition.type == type) return &definition;
  return nullptr;
}

inline const BaseInfrastructureDefinition* base_infrastructure(BuildingType building_type) {
  const auto& catalog = base_infrastructure_catalog();
  for (const auto& definition : catalog)
    if (definition.building_type == building_type) return &definition;
  return nullptr;
}

class Group {
 public:
  explicit Group(std::string id = std::string{primary_group_id}) : id_(std::move(id)) {
    if (id_.empty()) throw std::invalid_argument("group id must not be empty");
  }

  const std::string& id() const { return id_; }
  const GroupBase& primary_base() const { return primary_base_; }

  int infrastructure_level(BaseInfrastructure type) const {
    const auto* definition = base_infrastructure(type);
    return definition ? infrastructure_levels_[catalog_index(*definition)] : 0;
  }

  bool can_evolve(BaseInfrastructure type) const {
    const auto* definition = base_infrastructure(type);
    return definition && infrastructure_level(type) < definition->maximum_level &&
           prerequisite_satisfied(definition->prerequisite);
  }

  // The caller is responsible for validating and paying the declared world cost first.
  bool evolve(BaseInfrastructure type) {
    if (!can_evolve(type)) return false;
    ++infrastructure_levels_[catalog_index(*base_infrastructure(type))];
    return true;
  }

  json checkpoint() const {
    json levels = json::object();
    const auto& catalog = base_infrastructure_catalog();
    for (std::size_t index = 0; index < catalog.size(); ++index)
      levels[std::string{catalog[index].key}] = infrastructure_levels_[index];
    return {{"id", id_},
            {"primary_base", {{"kind", "campfire"}, {"infrastructure_levels", std::move(levels)}}}};
  }

  void restore_checkpoint(const json& state) {
    const auto id = state.at("id").get<std::string>();
    if (id.empty()) throw std::runtime_error("checkpoint group id is empty");
    const auto& base = state.at("primary_base");
    if (base.at("kind").get<std::string>() != "campfire")
      throw std::runtime_error("checkpoint group base kind is invalid");
    const auto& levels = base.at("infrastructure_levels");
    if (!levels.is_object()) throw std::runtime_error("checkpoint group infrastructure levels are invalid");

    std::array<int, 2> restored_levels{};
    const auto& catalog = base_infrastructure_catalog();
    for (std::size_t index = 0; index < catalog.size(); ++index) {
      const auto key = std::string{catalog[index].key};
      if (!levels.contains(key) || !levels.at(key).is_number_integer())
        throw std::runtime_error("checkpoint group infrastructure level is missing");
      const auto level = levels.at(key).get<int>();
      if (level < 0 || level > catalog[index].maximum_level)
        throw std::runtime_error("checkpoint group infrastructure level is invalid");
      restored_levels[index] = level;
    }
    for (const auto& [key, value] : levels.items()) {
      (void)value;
      bool known = false;
      for (const auto& definition : catalog)
        known = known || key == definition.key;
      if (!known) throw std::runtime_error("checkpoint group infrastructure is unknown");
    }
    if (restored_levels[catalog_index(*base_infrastructure(BaseInfrastructure::Workshop))] > 0 &&
        restored_levels[catalog_index(*base_infrastructure(BaseInfrastructure::Stockpile))] == 0)
      throw std::runtime_error("checkpoint group infrastructure prerequisite is invalid");

    id_ = id;
    primary_base_ = GroupBase{BaseKind::Campfire};
    infrastructure_levels_ = restored_levels;
  }

 private:
  static std::size_t catalog_index(const BaseInfrastructureDefinition& definition) {
    const auto& catalog = base_infrastructure_catalog();
    for (std::size_t index = 0; index < catalog.size(); ++index)
      if (catalog[index].type == definition.type) return index;
    throw std::logic_error("base infrastructure is absent from the catalog");
  }

  bool prerequisite_satisfied(BasePrerequisite prerequisite) const {
    switch (prerequisite) {
      case BasePrerequisite::Campfire:
        return primary_base_.kind == BaseKind::Campfire;
      case BasePrerequisite::Stockpile:
        return infrastructure_level(BaseInfrastructure::Stockpile) > 0;
    }
    return false;
  }

  std::string id_;
  GroupBase primary_base_;
  std::array<int, 2> infrastructure_levels_{};
};
}  // namespace apo
