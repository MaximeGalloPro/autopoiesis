#pragma once

#include "capability_registry.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace apo {
inline constexpr std::string_view primary_group_id{"groupe-principal"};

class CampKnowledge {
 public:
  CampKnowledge() {
    for (const auto skill : all_skills()) skills_[skill] = {};
    for (const auto& recipe : crafting_recipes()) recipes_.insert(recipe.key);
  }

  void observe_skills(const std::vector<Agent>& agents) {
    for (const auto& agent : agents) {
      if (!agent.alive) continue;
      for (const auto skill : all_skills()) {
        const SkillProgress observed{skill_experience(agent, skill), skill_level(agent, skill)};
        auto& remembered = skills_[skill];
        if (observed.level > remembered.level ||
            (observed.level == remembered.level && observed.experience > remembered.experience))
          remembered = observed;
      }
    }
  }

  void observe_cartography(const std::vector<Agent>& agents,int width,int height) {
    if(width<=0||height<=0)return;
    for(const auto& agent:agents) {
      if(!agent.alive)continue;
      for(const auto& [position,terrain]:agent.map_memory) {
        if(position.first<0||position.first>=width||position.second<0||position.second>=height)
          continue;
        cartography_.emplace(position,terrain);
      }
    }
  }

  std::optional<std::pair<Position,Terrain>> next_map_lesson(const Agent& learner) const {
    for(const auto& [coordinates,terrain]:cartography_)
      if(!learner.map_memory.contains(coordinates))
        return std::pair{Position{coordinates.first,coordinates.second},terrain};
    return std::nullopt;
  }

  SkillProgress skill_knowledge(Skill skill) const {
    const auto found=skills_.find(skill);
    return found==skills_.end()?SkillProgress{}:found->second;
  }

  json view() const {
    json recipes = json::array();
    for (const auto& recipe : crafting_recipes()) {
      if (!recipes_.contains(recipe.key)) continue;
      json items = json::object();
      for (const auto& [item, amount] : recipe.items) items[item] = amount;
      recipes.push_back({
          {"id", recipe.key},
          {"cost", {{"wood", recipe.wood}, {"branches", recipe.branches},
                    {"iron_ore", recipe.iron_ore}, {"items", std::move(items)}}},
          {"output", {{"item", recipe.output}, {"quantity", recipe.output_count}}}});
    }
    json skills = json::object();
    for (const auto skill : all_skills()) {
      const auto found = skills_.find(skill);
      const SkillProgress progress = found == skills_.end() ? SkillProgress{} : found->second;
      skills[skill_name(skill)] = {{"experience", progress.experience}, {"level", progress.level}};
    }
    json available_skills=json::array();
    for(const auto skill:all_skills())available_skills.push_back(skill_name(skill));
    json cells=json::array();
    for(const auto& [position,terrain]:cartography_)
      cells.push_back({{"x",position.first},{"y",position.second},
                       {"terrain",static_cast<int>(terrain)}});
    return {{"recipes", std::move(recipes)}, {"skills", std::move(skills)},
            {"learning", {{"available_skills",std::move(available_skills)}}},
            {"cartography", {{"known_cells",cartography_.size()},{"cells",std::move(cells)}}}};
  }

  void restore(const json& state) {
    if (!state.is_object() || !state.contains("recipes") || !state.at("recipes").is_array() ||
        !state.contains("skills") || !state.at("skills").is_object())
      throw std::runtime_error("checkpoint camp knowledge is invalid");

    std::set<std::string> restored_recipes;
    for (const auto& value : state.at("recipes")) {
      if (!value.is_string()) throw std::runtime_error("checkpoint camp recipe is invalid");
      const auto key = value.get<std::string>();
      if (!CapabilityRegistry::defaults().recipe(key) || !restored_recipes.insert(key).second)
        throw std::runtime_error("checkpoint camp recipe is unknown");
    }

    std::map<Skill, SkillProgress> restored_skills;
    const auto& saved_skills = state.at("skills");
    for (const auto skill : all_skills()) {
      const auto key = skill_name(skill);
      if (!saved_skills.contains(key) || !saved_skills.at(key).is_object())
        throw std::runtime_error("checkpoint camp skill is missing");
      const auto& saved = saved_skills.at(key);
      if (!saved.contains("experience") || !saved.at("experience").is_number_integer() ||
          !saved.contains("level") || !saved.at("level").is_number_integer())
        throw std::runtime_error("checkpoint camp skill is invalid");
      const int experience = saved.at("experience").get<int>();
      const int level = saved.at("level").get<int>();
      if (experience < 0 || experience > 50 || level != std::min(10, experience / 5))
        throw std::runtime_error("checkpoint camp skill is invalid");
      restored_skills[skill] = {experience, level};
    }
    for (const auto& [key, value] : saved_skills.items()) {
      (void)value;
      if (!skill_from_name(key)) throw std::runtime_error("checkpoint camp skill is unknown");
    }

    std::map<std::pair<int,int>,Terrain> restored_cartography;
    if(state.contains("cartography")) {
      const auto& cartography=state.at("cartography");
      if(!cartography.is_object()||!cartography.contains("known_cells")||
         !cartography.at("known_cells").is_number_unsigned()||!cartography.contains("cells")||
         !cartography.at("cells").is_array())
        throw std::runtime_error("checkpoint camp cartography is invalid");
      for(const auto& cell:cartography.at("cells")) {
        if(!cell.is_object()||!cell.value("x",json{}).is_number_integer()||
           !cell.value("y",json{}).is_number_integer()||!cell.value("terrain",json{}).is_number_integer())
          throw std::runtime_error("checkpoint camp map cell is invalid");
        const int x=cell.at("x").get<int>(),y=cell.at("y").get<int>(),
                  terrain=cell.at("terrain").get<int>();
        if(x<0||x>=40||y<0||y>=24||terrain<static_cast<int>(Terrain::Ground)||
           terrain>static_cast<int>(Terrain::Bush)||
           !restored_cartography.emplace(std::pair{x,y},static_cast<Terrain>(terrain)).second)
          throw std::runtime_error("checkpoint camp map cell is invalid");
      }
      if(cartography.at("known_cells").get<std::size_t>()!=restored_cartography.size())
        throw std::runtime_error("checkpoint camp cartography count is invalid");
    }

    recipes_ = std::move(restored_recipes);
    skills_ = std::move(restored_skills);
    cartography_ = std::move(restored_cartography);
  }

 private:
  std::set<std::string> recipes_;
  std::map<Skill, SkillProgress> skills_;
  std::map<std::pair<int,int>,Terrain> cartography_;
};

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
  explicit Group(std::string id = std::string{primary_group_id})
      : Group(std::move(id), FeatureRegistry::defaults().default_profile(),
              FeatureRegistry::defaults()) {}

  Group(std::string id, const WorldProfile& startup_profile,
        const FeatureRegistry& registry)
      : id_(std::move(id)) {
    if (id_.empty()) throw std::invalid_argument("group id must not be empty");
    if (!registry.valid_profile(startup_profile))
      throw std::invalid_argument("startup world profile is invalid");
    world_profile_ = startup_profile;
  }

  const std::string& id() const { return id_; }
  const GroupBase& primary_base() const { return primary_base_; }
  const WorldProfile& world_profile() const { return world_profile_; }
  const std::vector<ActiveFeature>& active_features() const {
    return world_profile_.active_features;
  }
  bool feature_active(const std::string& key, int version = 0) const {
    return std::any_of(world_profile_.active_features.begin(), world_profile_.active_features.end(),
                       [&](const ActiveFeature& feature) {
                         return feature.key == key && (version == 0 || feature.version == version);
                       });
  }
  const CampKnowledge& camp_knowledge() const { return camp_knowledge_; }
  json camp_knowledge_json() const { return camp_knowledge_.view(); }
  void observe_camp_skills(const std::vector<Agent>& agents) { camp_knowledge_.observe_skills(agents); }
  void observe_camp_cartography(const std::vector<Agent>& agents,int width,int height) {
    camp_knowledge_.observe_cartography(agents,width,height);
  }
  std::optional<std::pair<Position,Terrain>> next_camp_map_lesson(const Agent& learner) const {
    return camp_knowledge_.next_map_lesson(learner);
  }
  SkillProgress camp_skill_knowledge(Skill skill) const {
    return camp_knowledge_.skill_knowledge(skill);
  }

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

  json checkpoint(const FeatureRegistry& registry = FeatureRegistry::defaults()) const {
    json levels = json::object();
    const auto& catalog = base_infrastructure_catalog();
    for (std::size_t index = 0; index < catalog.size(); ++index)
      levels[std::string{catalog[index].key}] = infrastructure_levels_[index];
    const auto knowledge = camp_knowledge_.view();
    json recipes = json::array();
    for (const auto& recipe : knowledge.at("recipes")) recipes.push_back(recipe.at("id"));
    return {{"id", id_},
            {"primary_base", {{"kind", "campfire"}, {"infrastructure_levels", std::move(levels)}}},
            {"camp_knowledge", {{"recipes", std::move(recipes)}, {"skills", knowledge.at("skills")},
                                {"cartography", knowledge.at("cartography")}}},
            {"world_profile", registry.profile_json(world_profile_)}};
  }

  void restore_checkpoint(const json& state,
                          const FeatureRegistry& registry = FeatureRegistry::defaults(),
                          const std::optional<WorldProfile>& legacy_profile = std::nullopt) {
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

    CampKnowledge restored_knowledge;
    if (state.contains("camp_knowledge")) restored_knowledge.restore(state.at("camp_knowledge"));

    // Checkpoints written before world profiles leave the startup profile in
    // place. Simulation migrates its former top-level activation list first.
    WorldProfile restored_profile = world_profile_;
    if (state.contains("world_profile"))
      restored_profile = registry.profile_from_json(state.at("world_profile"));
    else if (legacy_profile)
      restored_profile = *legacy_profile;

    id_ = id;
    primary_base_ = GroupBase{BaseKind::Campfire};
    infrastructure_levels_ = restored_levels;
    camp_knowledge_ = std::move(restored_knowledge);
    world_profile_ = std::move(restored_profile);
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
  CampKnowledge camp_knowledge_;
  WorldProfile world_profile_;
};
}  // namespace apo
