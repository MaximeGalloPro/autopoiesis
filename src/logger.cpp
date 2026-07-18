#include "autopoiesis/logger.hpp"
#include "autopoiesis/feature_request.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>

namespace apo {
Logger::Logger(const std::string& directory) : directory_(directory) {
  const char* configured_run=std::getenv("AUTOPOIESIS_RUN_ID");
  const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  request_prefix_=(configured_run&&*configured_run?configured_run:std::string("run"))+"-"+std::to_string(now);
  std::error_code ec; std::filesystem::create_directories(directory,ec); readable_.open(directory+"/simulation.log",std::ios::app); structured_.open(directory+"/events.jsonl",std::ios::app);
}
void Logger::message(const std::string& line) { if(readable_) readable_<<line<<'\n'; recent_.push_back(line); if(recent_.size()>5) recent_.erase(recent_.begin()); }
void Logger::feature_request(int simulation_cycle,int day,const Agent& agent,const Decision& decision) {
  std::string id=request_prefix_+"-day-"+std::to_string(day)+"-cycle-"+std::to_string(simulation_cycle)+"-"+agent.id+"-"+std::to_string(++request_counter_);
  json request={{"id",id},{"status","pending"},{"day",day},{"simulation_cycle",simulation_cycle},{"agent_id",agent.id},{"agent_name",agent.name},{"need",decision.need},{"obstacle",decision.obstacle},{"desired_result",decision.desired_result}};
  std::ofstream out(directory_+"/feature_requests.jsonl",std::ios::app); if(out) out<<request.dump()<<'\n';
  message("Demande humaine "+id+" — "+decision.desired_result);
}
void Logger::ai_report(int simulation_cycle,int day,const Agent& agent,const json& report) {
  json event={{"day",day},{"simulation_cycle",simulation_cycle},{"type","ai_period_report"},{"agent_id",agent.id},{"report",report}};
  std::ofstream out(directory_+"/ai_reports.jsonl",std::ios::app); if(out) out<<event.dump()<<'\n';
  message("Bilan IA de "+agent.name+" : "+report.value("day_summary","indisponible"));
}
void Logger::ai_feature_request(int simulation_cycle,int day,const Agent& agent,const json& report,const json& request) {
  std::string error;
  if(!validate_feature_request(request,error)) { message("Demande IA rejetee : "+error); return; }
  json pending={{"id",request_prefix_+"-day-"+std::to_string(day)+"-cycle-"+std::to_string(simulation_cycle)+"-"+agent.id+"-ai-"+std::to_string(++request_counter_)},{"status","pending"},{"source","ai_period_report"},{"day",day},{"simulation_cycle",simulation_cycle},{"agent_id",agent.id},{"agent_name",agent.name},{"report",report},{"title",request.value("title","")},{"need",request.value("need","")},{"obstacle",request.value("obstacle","")},{"proposed_change",request.value("proposed_change","")},{"mechanism",request.value("mechanism",json::object())},{"acceptance_tests",request.value("acceptance_tests",json::array())}};
  std::ofstream requests(directory_+"/feature_requests.jsonl",std::ios::app); if(requests) requests<<pending.dump()<<'\n';
  message("Demande à Dieu à valider "+pending["id"].get<std::string>()+" : "+pending["title"].get<std::string>());
}
void Logger::event(int simulation_cycle,int day,const Agent& before,const Decision& d,const std::string& result,const Agent& after) { json j={{"day",day},{"simulation_cycle",simulation_cycle},{"type","agent_action"},{"agent_id",before.id},{"state_before",{{"health",before.health},{"hunger",before.hunger},{"thirst",before.thirst},{"fatigue",before.fatigue},{"x",before.position.x},{"y",before.position.y}}},{"decision",decision_json(d)},{"result",result},{"state_after",{{"health",after.health},{"hunger",after.hunger},{"thirst",after.thirst},{"fatigue",after.fatigue},{"x",after.position.x},{"y",after.position.y},{"alive",after.alive}}}}; if(structured_) structured_<<j.dump()<<'\n'; std::string detail=d.reason; if(d.type==DecisionType::Blocked) detail=d.need+" : "+d.obstacle; message("Jour "+std::to_string(day)+" / cycle elementaire "+std::to_string(simulation_cycle)+" — "+before.name+" "+result+(detail.empty()?"":" ["+detail+"]")); }
}
