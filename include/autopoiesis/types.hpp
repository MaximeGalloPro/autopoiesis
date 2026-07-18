#pragma once
#include <algorithm>
#include <deque>
#include <map>
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
struct FoodResource { FoodType type; Position position; int amount; int nutrition; };
struct Animal { std::string id; AnimalType type; Position position; bool alive{true}; int danger{}; int nutrition{}; };
struct Agent {
  std::string id, name; Position position; int health{100}, hunger{30}, fatigue{20};
  Personality personality; Attributes attributes; int thirst{20}; std::deque<std::string> memories; bool alive{true}; int sleeping_days{0}; int critical_hunger_days{0}; int critical_thirst_days{0};
  void remember(const std::string& s) { memories.push_back(s); const auto maximum=static_cast<std::size_t>(5+attributes.memory/10); while (memories.size() > maximum) memories.pop_front(); }
  std::map<std::pair<int,int>, Terrain> map_memory;
  std::map<std::pair<int,int>, int> map_visit_counts;
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
inline std::string food_type_name(FoodType type) { switch(type){case FoodType::Berries:return "berries";case FoodType::Roots:return "roots";case FoodType::Mushrooms:return "mushrooms";case FoodType::Fish:return "fish";case FoodType::Venison:return "venison";} return "unknown"; }
inline std::string animal_type_name(AnimalType type) { switch(type){case AnimalType::Rabbit:return "rabbit";case AnimalType::Deer:return "deer";case AnimalType::Boar:return "boar";case AnimalType::Wolf:return "wolf";case AnimalType::Fish:return "fish";} return "unknown"; }
}
