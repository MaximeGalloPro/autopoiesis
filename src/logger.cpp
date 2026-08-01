#include "autopoiesis/logger.hpp"
#include "autopoiesis/feature_request.hpp"
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <set>

namespace apo {
Logger::Logger(const std::string& directory) : directory_(directory) {
  const char* configured_run=std::getenv("AUTOPOIESIS_RUN_ID");
  const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  request_prefix_=(configured_run&&*configured_run?configured_run:std::string("run"))+"-"+std::to_string(now);
  std::error_code ec; std::filesystem::create_directories(directory,ec); readable_.open(directory+"/simulation.log",std::ios::app); structured_.open(directory+"/events.jsonl",std::ios::app);
}
void Logger::message(const std::string& line) { std::lock_guard lock(mutex_); if(readable_) readable_<<line<<'\n'; recent_.push_back(line); if(recent_.size()>5) recent_.erase(recent_.begin()); }
std::vector<std::string> Logger::recent() const { std::lock_guard lock(mutex_); return recent_; }
void Logger::action_started(int simulation_cycle, int day, const Agent& agent,
                            const Decision& decision) {
  std::lock_guard lock(mutex_);
  std::string reason=decision.reason;
  if(decision.type==DecisionType::Blocked) reason=decision.need+" : "+decision.obstacle;
  if(reason.empty()) reason="action "+(decision.type==DecisionType::Blocked?"bloquée":decision.action);
  message("Jour "+std::to_string(day)+" / cycle élémentaire "+std::to_string(simulation_cycle)+
          " — "+agent.name+" : "+reason);
}
void Logger::feature_request(int simulation_cycle,int day,const Agent& agent,const Decision& decision) {
  std::lock_guard lock(mutex_);
  std::string id=request_prefix_+"-day-"+std::to_string(day)+"-cycle-"+std::to_string(simulation_cycle)+"-"+agent.id+"-"+std::to_string(++request_counter_);
  json request={{"id",id},{"status","pending"},{"day",day},{"simulation_cycle",simulation_cycle},{"agent_id",agent.id},{"agent_name",agent.name},{"need",decision.need},{"obstacle",decision.obstacle},{"desired_result",decision.desired_result}};
  std::ofstream out(directory_+"/feature_requests.jsonl",std::ios::app); if(out) out<<request.dump()<<'\n';
  message("Demande humaine "+id+" — "+decision.desired_result);
}
void Logger::ai_report(int simulation_cycle,int day,const Agent& agent,const json& report,const CalendarDate& date,const ClimateState& climate) {
  std::lock_guard lock(mutex_);
  json event={{"day",day},{"simulation_cycle",simulation_cycle},{"type","ai_period_report"},{"agent_id",agent.id},{"calendar",calendar_json(date)},{"climate",climate_json(climate)},{"report",report}};
  std::ofstream out(directory_+"/ai_reports.jsonl",std::ios::app); if(out) out<<event.dump()<<'\n';
  message("Bilan IA de "+agent.name+" : "+report.value("day_summary","indisponible"));
}
namespace {
std::string compact_memory_sentence(const std::string& value) {
  std::string compact;
  bool space=false;
  for(unsigned char character:value){
    if(std::isspace(character)){space=!compact.empty();continue;}
    if(space){compact.push_back(' ');space=false;}
    compact.push_back(static_cast<char>(character));
  }
  if(compact.size()<=180)return compact;
  std::size_t end=180;
  while(end>0&&(static_cast<unsigned char>(compact[end])&0xC0)==0x80)--end;
  compact.resize(end);
  return compact;
}
}
json Logger::period_memories(const std::string& agent_id,std::size_t maximum) const {
  std::lock_guard lock(mutex_);
  json memories=json::array();
  if(maximum==0)return memories;
  std::ifstream input(directory_+"/ai_reports.jsonl");
  std::string line;
  while(std::getline(input,line)){
    try{
      const auto event=json::parse(line);
      if(event.value("agent_id","")!=agent_id||!event.value("report",json::object()).is_object())continue;
      const auto& report=event["report"];
      const auto summary=compact_memory_sentence(report.value("memory_summary",report.value("day_summary","")));
      const auto feeling=compact_memory_sentence(report.value("memory_feeling",report.value("state_assessment","")));
      if(summary.empty()&&feeling.empty())continue;
      const auto fallback="jour absolu "+std::to_string(event.value("day",0));
      const auto label=event.value("calendar",json::object()).value("label",fallback);
      memories.push_back({{"jour_absolu",event.value("day",0)},{"date",label},{"bilan",summary},{"ressenti",feeling}});
      while(memories.size()>maximum)memories.erase(memories.begin());
    }catch(const json::exception&){}
  }
  return memories;
}
json Logger::evolution_memory(std::size_t maximum) const {
  std::lock_guard lock(mutex_);
  if(maximum==0)return json::array();
  const auto bounded=[](std::string value,std::size_t limit){
    if(value.size()<=limit)return value;
    std::size_t end=limit;
    while(end>0&&(static_cast<unsigned char>(value[end])&0xC0)==0x80)--end;
    value.resize(end);return value;
  };
  std::map<std::string,std::string> decisions;
  const auto read_decisions=[&](const std::string& filename,const std::string& status){
    std::ifstream input(directory_+"/"+filename);std::string line;
    while(std::getline(input,line))try{const auto item=json::parse(line);const auto id=item.value("id","");if(!id.empty())decisions[id]=status;}catch(const json::exception&){}
  };
  read_decisions("approved_feature_requests.jsonl","approved");
  read_decisions("rejected_feature_requests.jsonl","rejected");

  std::map<std::string,std::size_t> endorsement_counts;
  std::ifstream endorsements(directory_+"/feature_endorsements.jsonl");
  std::string endorsement_line;
  while(std::getline(endorsements,endorsement_line))try{
    const auto item=json::parse(endorsement_line);
    const auto parent=item.value("parent_request_id","");
    if(!parent.empty())++endorsement_counts[parent];
  }catch(const json::exception&){}

  std::vector<json> evolutions;std::map<std::string,std::size_t> indexes;
  std::ifstream input(directory_+"/feature_requests.jsonl");std::string line;
  while(std::getline(input,line))try{
    const auto request=json::parse(line);const auto id=request.value("id","");
    if(id.empty())continue;
    std::string status=decisions.contains(id)?decisions[id]:request.value("status","pending");
    std::ifstream activation(directory_+"/evolution_runs/"+id+"/activation.json");
    if(activation){try{json value;activation>>value;if(value.value("status","")=="activated")status="activated";}catch(const json::exception&){}}
    if(status=="rejected")continue;
    const auto summary=bounded(request.value("mechanism",json::object()).value("summary",""),280);
    json compact={{"id",id},{"status",status},{"evolution_key",request.value("evolution_key","")},
                  {"title",bounded(request.value("title",""),180)},{"mechanism_summary",summary},
                  {"support_count",endorsement_counts[id]}};
    if(indexes.contains(id))evolutions[indexes[id]]=std::move(compact);
    else{indexes[id]=evolutions.size();evolutions.push_back(std::move(compact));}
  }catch(const json::exception&){}

  json result=json::array();
  const auto append_status=[&](bool priority){
    for(auto it=evolutions.rbegin();it!=evolutions.rend()&&result.size()<maximum;++it){
      const auto status=it->value("status","pending");
      const bool important=status=="approved"||status=="activated";
      if(important==priority)result.push_back(*it);
    }
  };
  append_status(true);append_status(false);
  return result;
}
void Logger::ai_feature_request(int simulation_cycle,int day,const Agent& agent,const json& report,const json& request) {
  std::lock_guard lock(mutex_);
  std::string error;
  if(!validate_feature_request(request,error)) { message("Demande IA rejetee : "+error); return; }
  if(evolution_window_cycle_!=simulation_cycle){evolution_window_cycle_=simulation_cycle;evolution_keys_.clear();}
  const auto evolution_key=request.value("evolution_key","");
  const auto request_mode=request.value("request_mode","propose");
  if(request_mode=="insist"){
    const auto parent_id=request.value("parent_request_id","");
    if(parent_id.empty()){message("Insistance IA ignoree : identifiant de demande parent absent.");return;}
    std::string line;
    std::set<std::string> closed_requests;
    for(const auto& filename: {"approved_feature_requests.jsonl","rejected_feature_requests.jsonl"}){
      std::ifstream decisions_input(directory_+"/"+filename);
      while(std::getline(decisions_input,line))try{
        const auto decision=json::parse(line); const auto id=decision.value("id","");
        if(!id.empty())closed_requests.insert(id);
      }catch(const json::exception&){}
    }
    bool open=false;
    std::ifstream requests_input(directory_+"/feature_requests.jsonl");
    while(std::getline(requests_input,line))try{
      const auto existing=json::parse(line);
      if(existing.value("id","")==parent_id&&existing.value("status","pending")=="pending"&&
         existing.value("evolution_key","")==evolution_key&&!closed_requests.contains(parent_id)){open=true;break;}
    }catch(const json::exception&){}
    if(!open){message("Insistance IA ignoree : la demande n'est plus ouverte ("+parent_id+").");return;}

    std::ifstream endorsements_input(directory_+"/feature_endorsements.jsonl");
    while(std::getline(endorsements_input,line))try{
      const auto existing=json::parse(line);
      if(existing.value("parent_request_id","")==parent_id&&existing.value("agent_id","")==agent.id&&
         existing.value("simulation_cycle",-1)==simulation_cycle){
        message("Insistance IA ignoree : personnage deja inscrit pour cette fenetre ("+agent.id+").");return;
      }
    }catch(const json::exception&){}
    json endorsement={{"parent_request_id",parent_id},{"evolution_key",evolution_key},
      {"agent_id",agent.id},{"agent_name",agent.name},{"day",day},{"simulation_cycle",simulation_cycle},
      {"reason",request.value("insistence_reason",request.value("need",""))},
      {"evidence",request.value("evidence",json::array())}};
    std::ofstream output(directory_+"/feature_endorsements.jsonl",std::ios::app);
    if(output)output<<endorsement.dump()<<'\n';
    message("Insistance IA enregistree pour "+parent_id+" par "+agent.name);
    return;
  }
  if(!evolution_keys_.insert(evolution_key).second){message("Demande IA ignoree : mecanisme deja propose dans cette fenetre ("+evolution_key+").");return;}
  std::ifstream existing_requests(directory_+"/feature_requests.jsonl");
  std::string existing_line;
  while(std::getline(existing_requests,existing_line))try{
    const auto existing=json::parse(existing_line);
    if(existing.value("status","pending")=="pending"&&existing.value("evolution_key","")==evolution_key){
      message("Demande IA ignoree : une demande ouverte existe deja pour "+evolution_key+".");return;
    }
  }catch(const json::exception&){}
  json pending={{"id",request_prefix_+"-day-"+std::to_string(day)+"-cycle-"+std::to_string(simulation_cycle)+"-"+agent.id+"-ai-"+std::to_string(++request_counter_)},{"status","pending"},{"source","ai_period_report"},{"day",day},{"simulation_cycle",simulation_cycle},{"agent_id",agent.id},{"agent_name",agent.name},{"report",report},{"evolution_key",evolution_key},{"domain",request.value("domain","")},{"title",request.value("title","")},{"need",request.value("need","")},{"obstacle",request.value("obstacle","")},{"proposed_change",request.value("proposed_change","")},{"mechanism",request.value("mechanism",json::object())},{"acceptance_tests",request.value("acceptance_tests",json::array())}};
  std::ofstream requests(directory_+"/feature_requests.jsonl",std::ios::app); if(requests) requests<<pending.dump()<<'\n';
  message("Demande à Dieu à valider "+pending["id"].get<std::string>()+" : "+pending["title"].get<std::string>());
}
std::set<std::string> Logger::known_evolution_keys() const {
  std::lock_guard lock(mutex_);
  std::set<std::string> keys;
  std::ifstream input(directory_+"/feature_requests.jsonl");
  std::string line;
  while(std::getline(input,line)){
    try{const auto request=json::parse(line);const auto key=request.value("evolution_key","");if(!key.empty())keys.insert(key);}catch(const json::exception&){}
  }
  return keys;
}
std::string Logger::devil_constraint(int simulation_cycle,int day,const json& request) {
  std::lock_guard lock(mutex_);
  std::string error;
  if(!validate_feature_request(request,error)){message("Contrainte du Diable rejetee : "+error);return {};}
  const auto evolution_key=request.value("evolution_key","");
  if(known_evolution_keys().contains(evolution_key)){message("Contrainte du Diable deja connue : "+evolution_key);return {};}
  const auto id=request_prefix_+"-day-"+std::to_string(day)+"-cycle-"+std::to_string(simulation_cycle)+"-devil-"+std::to_string(++request_counter_);
  json pending=request;
  pending["id"]=id;pending["status"]="pending";pending["source"]="devil";
  pending["day"]=day;pending["simulation_cycle"]=simulation_cycle;
  pending["agent_id"]="devil";pending["agent_name"]="Le Diable";
  std::ofstream output(directory_+"/feature_requests.jsonl",std::ios::app);
  if(!output)return {};
  output<<pending.dump()<<'\n';
  message("Le Diable propose "+id+" : "+pending.value("title","contrainte sans titre"));
  return id;
}
void Logger::event(int simulation_cycle,int day,const Agent& before,const Decision& d,
                   const std::string& result, bool succeeded) {
  std::lock_guard lock(mutex_);
  std::string reason=d.reason;
  if(d.type==DecisionType::Blocked) reason=d.need+" : "+d.obstacle;
  json j={{"day",day},{"simulation_cycle",simulation_cycle},{"type","agent_action"},
         {"agent_id",before.id},{"action",d.type==DecisionType::Blocked?"blocked":d.action},
         {"reason",reason},{"outcome",succeeded?"success":"failure"},{"result",result}};
  if(structured_) structured_<<j.dump()<<'\n';
  message("Jour "+std::to_string(day)+" / cycle élémentaire "+std::to_string(simulation_cycle)+
          " — "+before.name+" : "+(succeeded?"réussite":"échec")+
          (result.empty()?"":" — "+result));
}
}
