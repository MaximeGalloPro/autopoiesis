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
enum class CraftItem { WoodenHandle, Charcoal, Rope, IronIngot, Axe };
enum class BuildingType { Wall, Door, Bed, Stockpile, Workshop };
struct BuildingCost { int wood{}; int branches{}; int iron_ore{}; std::vector<std::pair<CraftItem,int>> items; int work{}; };
struct Building {
  BuildingType type{BuildingType::Wall};
  int progress{};
  int required_work{1};
  bool complete{};
  friend bool operator==(const Building&,const Building&)=default;
};
inline std::string building_type_name(BuildingType type) { switch(type){case BuildingType::Wall:return "wall";case BuildingType::Door:return "door";case BuildingType::Bed:return "bed";case BuildingType::Stockpile:return "stockpile";case BuildingType::Workshop:return "workshop";}return "unknown"; }
inline std::optional<BuildingType> building_type_from_name(const std::string& name) { for(const auto type:{BuildingType::Wall,BuildingType::Door,BuildingType::Bed,BuildingType::Stockpile,BuildingType::Workshop})if(building_type_name(type)==name)return type;return std::nullopt; }
inline BuildingCost building_cost(BuildingType type) { switch(type){case BuildingType::Wall:return {2,0,0,{},3};case BuildingType::Door:return {1,0,0,{{CraftItem::WoodenHandle,1}},2};case BuildingType::Bed:return {2,0,0,{{CraftItem::Rope,1}},3};case BuildingType::Stockpile:return {2,0,0,{},3};case BuildingType::Workshop:return {3,0,0,{{CraftItem::IronIngot,1}},4};}return {}; }
enum class Skill { Woodcutting, Mining, Crafting, Building, Foraging, Hunting, Cooking, Social };
struct SkillProgress {
  int experience{};
  int level{};
  friend bool operator==(const SkillProgress&,const SkillProgress&)=default;
};
struct CraftingRecipe {
  std::string key;
  int wood{};
  int branches{};
  int iron_ore{};
  std::vector<std::pair<CraftItem,int>> items;
  CraftItem output{CraftItem::WoodenHandle};
  int output_count{1};
};
inline const std::vector<CraftingRecipe>& crafting_recipes() {
  static const std::vector<CraftingRecipe> recipes{
      {"wooden_handle",1,0,0,{},CraftItem::WoodenHandle,1},
      {"charcoal",2,0,0,{},CraftItem::Charcoal,1},
      {"rope",0,3,0,{},CraftItem::Rope,1},
      {"iron_ingot",0,0,2,{{CraftItem::Charcoal,1}},CraftItem::IronIngot,1},
      {"axe",0,0,0,{{CraftItem::WoodenHandle,1},{CraftItem::IronIngot,1}},CraftItem::Axe,1}};
  return recipes;
}
inline std::string craft_item_name(CraftItem item) { switch(item){case CraftItem::WoodenHandle:return "wooden_handle";case CraftItem::Charcoal:return "charcoal";case CraftItem::Rope:return "rope";case CraftItem::IronIngot:return "iron_ingot";case CraftItem::Axe:return "axe";}return "unknown"; }
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
struct FoodResource { FoodType type; Position position; int amount; int nutrition; int capacity{}; int depleted_days{}; };
struct FoodItem {
  FoodType type{FoodType::Berries};
  int nutrition{};
  bool cooked{};
  int age_days{};
  int shelf_life_days{3};
  friend bool operator==(const FoodItem&,const FoodItem&)=default;
};
struct Tool {
  CraftItem type{CraftItem::Axe};
  int durability{20};
  int maximum_durability{20};
  friend bool operator==(const Tool&,const Tool&)=default;
};
struct Animal { std::string id; AnimalType type; Position position; bool alive{true}; int danger{}; int nutrition{}; };
struct EcologyState {
  int day{};
  int births{};
  int predations{};
  int regrown_food{};
  int depleted_patches{};
  int total_births{};
  int total_predations{};
  friend bool operator==(const EcologyState&,const EcologyState&)=default;
};
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
  std::optional<Position> home_camp;
  std::optional<Position> camp_rest_position;
  int wood_inventory{};
  int branch_inventory{};
  int iron_ore_inventory{};
  std::optional<FoodItem> carried_food;
  std::optional<Tool> equipped_tool;
  std::optional<ShelterConstruction> shelter_construction;
  std::string community_role;
  int last_shared_meal_day{};
  int last_vigil_day{};
  bool celebration_pending{};
  std::set<std::string> mourned_agents;
  std::map<Skill,SkillProgress> skills;
  int last_taught_day{};
  int last_lesson_day{};
  void remember_map(Position p, Terrain terrain) { map_memory[{p.x,p.y}] = terrain; }
};
inline std::string skill_name(Skill skill) { switch(skill){case Skill::Woodcutting:return "woodcutting";case Skill::Mining:return "mining";case Skill::Crafting:return "crafting";case Skill::Building:return "building";case Skill::Foraging:return "foraging";case Skill::Hunting:return "hunting";case Skill::Cooking:return "cooking";case Skill::Social:return "social";}return "unknown"; }
inline std::optional<Skill> skill_from_name(const std::string& name) { for(const auto skill:{Skill::Woodcutting,Skill::Mining,Skill::Crafting,Skill::Building,Skill::Foraging,Skill::Hunting,Skill::Cooking,Skill::Social})if(skill_name(skill)==name)return skill;return std::nullopt; }
inline int skill_experience(const Agent& agent,Skill skill) { const auto found=agent.skills.find(skill);return found==agent.skills.end()?0:found->second.experience; }
inline int skill_level(const Agent& agent,Skill skill) { const auto found=agent.skills.find(skill);return found==agent.skills.end()?0:found->second.level; }
inline void add_skill_experience(Agent& agent,Skill skill,int amount=1) { if(amount<=0)return;auto& progress=agent.skills[skill];progress.experience=std::min(50,progress.experience+amount);progress.level=std::min(10,progress.experience/5); }
inline std::vector<Skill> top_skills(const Agent& agent,std::size_t maximum) { std::vector<Skill> result;for(const auto&[skill,progress]:agent.skills)if(progress.experience>0)result.push_back(skill);std::sort(result.begin(),result.end(),[&](Skill left,Skill right){const auto left_level=skill_level(agent,left),right_level=skill_level(agent,right);if(left_level!=right_level)return left_level>right_level;const auto left_xp=skill_experience(agent,left),right_xp=skill_experience(agent,right);return left_xp!=right_xp?left_xp>right_xp:left<right;});if(result.size()>maximum)result.resize(maximum);return result; }
inline std::optional<Skill> teachable_skill(const Agent& mentor,const Agent& learner) { std::optional<Skill> selected;for(const auto skill:top_skills(mentor,8))if(skill_level(mentor,skill)>=skill_level(learner,skill)+2&&(!selected||skill_level(mentor,skill)>skill_level(mentor,*selected)))selected=skill;return selected; }
inline json skills_json(const Agent& agent) { json result=json::object();for(const auto&[skill,progress]:agent.skills)result[skill_name(skill)]={{"experience",progress.experience},{"level",progress.level}};return result; }
inline int inventory_capacity(const Agent& agent) { return std::clamp(4+agent.attributes.strength/25,4,10); }
inline int inventory_load(const Agent& agent) { return agent.wood_inventory+agent.branch_inventory+agent.iron_ore_inventory+(agent.carried_food?1:0); }
inline bool inventory_full(const Agent& agent) { return inventory_load(agent)>=inventory_capacity(agent); }
struct Decision {
  DecisionType type{DecisionType::Action}; std::string action{"wait"}; json parameters = json::object();
  std::string reason, need, obstacle, desired_result;
};
struct Perception { json value; };
inline int clamp_stat(int value) { return std::clamp(value, 0, 100); }
inline json personality_json(const Personality& p) { return {{"curiosity",p.curiosity},{"prudence",p.prudence},{"sociability",p.sociability},{"patience",p.patience},{"empathy",p.empathy}}; }
inline json attributes_json(const Attributes& a) { return {{"strength",a.strength},{"agility",a.agility},{"endurance",a.endurance},{"toughness",a.toughness},{"recuperation",a.recuperation},{"disease_resistance",a.disease_resistance},{"focus",a.focus},{"willpower",a.willpower},{"memory",a.memory},{"spatial_sense",a.spatial_sense}}; }
inline std::string food_type_name(FoodType type);
inline int food_shelf_life(FoodType type) { switch(type){case FoodType::Berries:return 3;case FoodType::Roots:return 5;case FoodType::Mushrooms:return 3;case FoodType::Fish:return 2;case FoodType::Venison:return 2;}return 3; }
inline std::string project_status_name(ProjectStatus status) { switch(status){case ProjectStatus::Candidate:return "candidate";case ProjectStatus::Active:return "active";case ProjectStatus::Blocked:return "blocked";case ProjectStatus::Completed:return "completed";case ProjectStatus::Abandoned:return "abandoned";} return "unknown"; }
inline json behavior_json(const BehaviorProfile& behavior) { json foods=json::array();for(const auto food:behavior.preferred_foods)foods.push_back(food_type_name(food));return {{"archetype",behavior.archetype},{"aspiration",behavior.aspiration},{"construction_drive",behavior.construction_drive},{"provision_drive",behavior.provision_drive},{"exploration_drive",behavior.exploration_drive},{"social_drive",behavior.social_drive},{"preferred_foods",foods}}; }
inline json project_json(const Project& project) { return {{"key",project.key},{"title",project.title},{"status",project_status_name(project.status)},{"step",project.step},{"progress",project.progress},{"target",project.target},{"blocked_reason",project.blocked_reason},{"missing_capability",project.missing_capability},{"started_day",project.started_day},{"last_progress_cycle",project.last_progress_cycle}}; }
inline json relationships_json(const std::map<std::string,Relationship>& relationships) { json value=json::object();for(const auto&[id,relationship]:relationships)value[id]={{"trust",relationship.trust},{"affinity",relationship.affinity},{"interactions",relationship.interactions}};return value; }
inline std::string food_type_name(FoodType type) { switch(type){case FoodType::Berries:return "berries";case FoodType::Roots:return "roots";case FoodType::Mushrooms:return "mushrooms";case FoodType::Fish:return "fish";case FoodType::Venison:return "venison";} return "unknown"; }
inline std::string animal_type_name(AnimalType type) { switch(type){case AnimalType::Rabbit:return "rabbit";case AnimalType::Deer:return "deer";case AnimalType::Boar:return "boar";case AnimalType::Wolf:return "wolf";case AnimalType::Fish:return "fish";} return "unknown"; }
}
