#include "autopoiesis/decision.hpp"
#include <sstream>

namespace apo {
static bool adjacent(Position a, Position b) { return std::abs(a.x-b.x)+std::abs(a.y-b.y)==1; }
std::vector<std::string> available_actions(const Agent& a, const World& w, const std::vector<Agent>& agents) {
  std::vector<std::string> r{"observe","wait","move","sleep"}; if (w.berries(a.position)>0) r.push_back("eat_berries");
  for (const auto& other:agents) if (other.alive && other.id!=a.id && adjacent(a.position,other.position)) r.push_back("talk"); return r;
}
bool validate_decision(const Decision& d,const Agent& a,const World& w,const std::vector<Agent>& agents,std::string& e) {
  if (d.type==DecisionType::Blocked) return !d.need.empty() && !d.obstacle.empty() && !d.desired_result.empty() || (e="blocked fields missing",false);
  static const std::set<std::string> names{"observe","move","wait","talk","sleep","eat_berries"}; if (!names.contains(d.action)) {e="unknown action";return false;}
  auto avail=available_actions(a,w,agents); if (std::find(avail.begin(),avail.end(),d.action)==avail.end()) {e="action unavailable";return false;}
  if (d.action=="move") { if (!d.parameters.is_object() || !d.parameters.contains("direction") || !d.parameters["direction"].is_string()) {e="direction required";return false;} auto dir=d.parameters["direction"].get<std::string>(); if (!std::set<std::string>{"north","south","east","west"}.contains(dir)){e="invalid direction";return false;} }
  if (d.action=="talk") { if (!d.parameters.is_object() || !d.parameters.contains("target_agent_id") || !d.parameters["target_agent_id"].is_string()) {e="target_agent_id required";return false;} bool ok=false; for(const auto&o:agents) if(o.id==d.parameters["target_agent_id"] && o.alive && adjacent(a.position,o.position)) ok=true; if(!ok){e="talk target not adjacent";return false;} }
  return true;
}
bool parse_decision(const json& v, Decision& d, std::string& e) {
  try { if(!v.is_object() || !v.contains("type") || !v["type"].is_string()) throw std::runtime_error("type missing"); auto type=v["type"].get<std::string>(); if(type=="blocked"){d.type=DecisionType::Blocked;d.need=v.at("need").get<std::string>();d.obstacle=v.at("obstacle").get<std::string>();d.desired_result=v.at("desired_result").get<std::string>();return true;} if(type!="action") throw std::runtime_error("invalid type"); d.type=DecisionType::Action; d.action=v.at("action").get<std::string>(); d.parameters=v.value("parameters",json::object()); d.reason=v.value("reason",""); return true; } catch(const std::exception& ex){e=ex.what();return false;}
}
json decision_json(const Decision& d) { if(d.type==DecisionType::Blocked) return {{"type","blocked"},{"need",d.need},{"obstacle",d.obstacle},{"desired_result",d.desired_result}}; return {{"type","action"},{"action",d.action},{"parameters",d.parameters},{"reason",d.reason}}; }
}
