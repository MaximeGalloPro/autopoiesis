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
enum class DecisionType { Action, Blocked };
struct Personality { int curiosity{}; int prudence{}; int sociability{}; int patience{}; int empathy{}; };
struct Agent {
  std::string id, name; Position position; int health{100}, hunger{30}, fatigue{20};
  Personality personality; std::deque<std::string> memories; bool alive{true}; int sleeping_days{0}; int critical_hunger_days{0};
  void remember(const std::string& s) { memories.push_back(s); while (memories.size() > 10) memories.pop_front(); }
  std::map<std::pair<int,int>, Terrain> map_memory;
  void remember_map(Position p, Terrain terrain) { map_memory[{p.x,p.y}] = terrain; }
};
struct Decision {
  DecisionType type{DecisionType::Action}; std::string action{"wait"}; json parameters = json::object();
  std::string reason, need, obstacle, desired_result;
};
struct Perception { json value; };
inline int clamp_stat(int value) { return std::clamp(value, 0, 100); }
inline json personality_json(const Personality& p) { return {{"curiosity",p.curiosity},{"prudence",p.prudence},{"sociability",p.sociability},{"patience",p.patience},{"empathy",p.empathy}}; }
}
