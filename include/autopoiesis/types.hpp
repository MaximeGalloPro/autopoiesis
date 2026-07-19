#pragma once
#include <algorithm>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace apo {
using json = nlohmann::json;
struct Position { int x{}; int y{}; friend bool operator==(const Position&, const Position&) = default; };
enum class Terrain { Ground, Wall, Water, Tree, Bush };
enum class FoodType { Berries, Roots, Mushrooms, Fish, Venison };
enum class AnimalType { Rabbit, Deer, Boar, Wolf, Fish };
enum class DecisionType { Action, Blocked };
struct Personality { int curiosity{}; int prudence{}; int sociability{}; int patience{}; int empathy{}; };
struct Attributes {
  int strength{50};
  int agility{50};
  int endurance{50};
  int toughness{50};
  int recuperation{50};
  int disease_resistance{50};
  int focus{50};
  int willpower{50};
  int memory{50};
  int spatial_sense{50};
};
enum class ProjectStatus { Candidate, Active, Blocked, Completed, Abandoned };
struct BehaviorProfile {
  std::string archetype;
  std::string aspiration;
  int construction_drive{50};
  int provision_drive{50};
  int exploration_drive{50};
  int social_drive{50};
  std::vector<FoodType> preferred_foods;
};
struct Project {
  std::string key;
  std::string title;
  ProjectStatus status{ProjectStatus::Candidate};
  int step{};
  int progress{};
  int target{};
  std::string blocked_reason;
  std::string missing_capability;
  int started_day{};
  int last_progress_cycle{};
};
struct Relationship { int trust{}; int affinity{}; int interactions{}; };
struct ShelterConstruction { Position position; int progress{}; };
struct FoodResource { FoodType type; Position position; int amount; int nutrition; int capacity{}; };
struct FoodItem {
  FoodType type{FoodType::Berries};
  int nutrition{};
  friend bool operator==(const FoodItem&,const FoodItem&)=default;
};
struct Animal { std::string id; AnimalType type; Position position; bool alive{true}; int danger{}; int nutrition{}; };
struct Agent {
  std::string id, name; Position position; int health{100}, hunger{30}, fatigue{20};
  Personality personality; Attributes attributes; int thirst{20}; std::deque<std::string> memories; bool alive{true}; int sleeping_days{0}; int critical_hunger_days{0}; int critical_thirst_days{0};
  void remember(const std::string& s) { memories.push_back(s); const auto maximum=static_cast<std::size_t>(5+attributes.memory/10); while (memories.size() > maximum) memories.pop_front(); }
  std::map<std::pair<int,int>, Terrain> map_memory;
  std::map<std::pair<int,int>, int> map_visit_counts;
  BehaviorProfile behavior;
  Project project;
  int boredom{};
  std::map<std::string, Relationship> relationships;
  std::set<std::string> observed_animals;
  std::set<std::pair<int,int>> known_campfires;
  int wood_inventory{};
  int branch_inventory{};
  std::optional<FoodItem> carried_food;
  std::optional<ShelterConstruction> shelter_construction;
  void remember_map(Position p, Terrain terrain) { map_memory[{p.x,p.y}] = terrain; }
};
struct Decision {
  DecisionType type{DecisionType::Action}; std::string action{"wait"}; json parameters = json::object();
  std::string reason, need, obstacle, desired_result;
};
struct Perception { json value; };
inline int clamp_stat(int value) { return std::clamp(value, 0, 100); }
inline json personality_json(const Personality& p) { return {{"curiosity",p.curiosity},{"prudence",p.prudence},{"sociability",p.sociability},{"patience",p.patience},{"empathy",p.empathy}}; }
inline json attributes_json(const Attributes& a) { return {{"strength",a.strength},{"agility",a.agility},{"endurance",a.endurance},{"toughness",a.toughness},{"recuperation",a.recuperation},{"disease_resistance",a.disease_resistance},{"focus",a.focus},{"willpower",a.willpower},{"memory",a.memory},{"spatial_sense",a.spatial_sense}}; }
inline std::string food_type_name(FoodType type);
inline std::string project_status_name(ProjectStatus status) { switch(status){case ProjectStatus::Candidate:return "candidate";case ProjectStatus::Active:return "active";case ProjectStatus::Blocked:return "blocked";case ProjectStatus::Completed:return "completed";case ProjectStatus::Abandoned:return "abandoned";} return "unknown"; }
inline json behavior_json(const BehaviorProfile& behavior) { json foods=json::array();for(const auto food:behavior.preferred_foods)foods.push_back(food_type_name(food));return {{"archetype",behavior.archetype},{"aspiration",behavior.aspiration},{"construction_drive",behavior.construction_drive},{"provision_drive",behavior.provision_drive},{"exploration_drive",behavior.exploration_drive},{"social_drive",behavior.social_drive},{"preferred_foods",foods}}; }
inline json project_json(const Project& project) { return {{"key",project.key},{"title",project.title},{"status",project_status_name(project.status)},{"step",project.step},{"progress",project.progress},{"target",project.target},{"blocked_reason",project.blocked_reason},{"missing_capability",project.missing_capability},{"started_day",project.started_day},{"last_progress_cycle",project.last_progress_cycle}}; }
inline json relationships_json(const std::map<std::string,Relationship>& relationships) { json value=json::object();for(const auto&[id,relationship]:relationships)value[id]={{"trust",relationship.trust},{"affinity",relationship.affinity},{"interactions",relationship.interactions}};return value; }
inline std::string food_type_name(FoodType type) { switch(type){case FoodType::Berries:return "berries";case FoodType::Roots:return "roots";case FoodType::Mushrooms:return "mushrooms";case FoodType::Fish:return "fish";case FoodType::Venison:return "venison";} return "unknown"; }
inline std::string animal_type_name(AnimalType type) { switch(type){case AnimalType::Rabbit:return "rabbit";case AnimalType::Deer:return "deer";case AnimalType::Boar:return "boar";case AnimalType::Wolf:return "wolf";case AnimalType::Fish:return "fish";} return "unknown"; }
}
