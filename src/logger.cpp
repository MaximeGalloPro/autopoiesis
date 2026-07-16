#include "autopoiesis/logger.hpp"
#include "autopoiesis/feature_request.hpp"
#include <filesystem>
namespace apo {
Logger::Logger(const std::string& directory) : directory_(directory) { std::error_code ec; std::filesystem::create_directories(directory,ec); readable_.open(directory+"/simulation.log",std::ios::app); structured_.open(directory+"/events.jsonl",std::ios::app); }
void Logger::message(const std::string& line) { if(readable_) readable_<<line<<'\n'; recent_.push_back(line); if(recent_.size()>5) recent_.erase(recent_.begin()); }
void Logger::feature_request(int cycle,const Agent& agent,const Decision& decision) {
  std::string id="cycle-"+std::to_string(cycle)+"-"+agent.id+"-"+std::to_string(++request_counter_);
  json request={{"id",id},{"status","pending"},{"cycle",cycle},{"agent_id",agent.id},{"agent_name",agent.name},{"need",decision.need},{"obstacle",decision.obstacle},{"desired_result",decision.desired_result}};
  std::ofstream out(directory_+"/feature_requests.jsonl",std::ios::app); if(out) out<<request.dump()<<'\n';
  message("Demande humaine "+id+" — "+decision.desired_result);
}
void Logger::ai_report(int cycle,const Agent& agent,const json& report) {
  json event={{"cycle",cycle},{"type","ai_cycle_report"},{"agent_id",agent.id},{"report",report}};
  std::ofstream out(directory_+"/ai_reports.jsonl",std::ios::app); if(out) out<<event.dump()<<'\n';
  message("Bilan IA de "+agent.name+" : "+report.value("day_summary","indisponible"));
  if(!report.contains("feature_request")) return;
  const auto& request=report["feature_request"];
  if(request.is_object() && request.value("requested",false)) {
    std::string error;
    if(!validate_feature_request(request,error)) {
      message("Demande IA rejetee : "+error);
      return;
    }
    json pending={{"id","cycle-"+std::to_string(cycle)+"-"+agent.id+"-ai-"+std::to_string(++request_counter_)},{"status","pending"},{"source","ai_cycle_report"},{"cycle",cycle},{"agent_id",agent.id},{"agent_name",agent.name},{"title",request["title"]},{"need",request["need"]},{"obstacle",request["obstacle"]},{"proposed_change",request["proposed_change"]},{"mechanism",request["mechanism"]},{"acceptance_tests",request["acceptance_tests"]}};
    std::ofstream requests(directory_+"/feature_requests.jsonl",std::ios::app); if(requests) requests<<pending.dump()<<'\n';
    message("Demande à Dieu à valider "+pending["id"].get<std::string>()+" : "+pending["title"].get<std::string>());
  }
}
void Logger::event(int cycle,const Agent& before,const Decision& d,const std::string& result,const Agent& after) { json j={{"cycle",cycle},{"type","agent_action"},{"agent_id",before.id},{"state_before",{{"health",before.health},{"hunger",before.hunger},{"fatigue",before.fatigue},{"x",before.position.x},{"y",before.position.y}}},{"decision",decision_json(d)},{"result",result},{"state_after",{{"health",after.health},{"hunger",after.hunger},{"fatigue",after.fatigue},{"x",after.position.x},{"y",after.position.y},{"alive",after.alive}}}}; if(structured_) structured_<<j.dump()<<'\n'; message("Cycle "+std::to_string(cycle)+" — "+before.name+" "+result); }
}
