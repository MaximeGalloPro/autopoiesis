#include "autopoiesis/openai_client.hpp"
#include "autopoiesis/feature_request.hpp"
#include <curl/curl.h>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <regex>
#include <sstream>

namespace apo {
OpenAIClient::OpenAIClient(std::string key,std::string model,std::string base,ApiCallBudget budget,DiagnosticSink diagnostic_sink)
  : key_(std::move(key)),model_(std::move(model)),base_url_(std::move(base)),budget_(std::move(budget)),diagnostic_sink_(std::move(diagnostic_sink)) { while(!base_url_.empty()&&base_url_.back()=='/') base_url_.pop_back(); }

static size_t write_body(char* p,size_t s,size_t n,void* u){static_cast<std::string*>(u)->append(p,s*n);return s*n;}

static long positive_timeout_from_env(){
  const char* value=std::getenv("LLM_API_TIMEOUT_SECONDS");
  if(!value||!*value)return 120L;
  try{return std::max(1L,std::stol(value));}catch(...){return 120L;}
}

static std::string elapsed_text(long duration_ms){
  std::ostringstream output;output<<std::fixed<<std::setprecision(2)<<duration_ms/1000.0<<" s";return output.str();
}

static std::string redact_secrets(std::string value){
  value=std::regex_replace(value,std::regex(R"(sk-[A-Za-z0-9_-]{8,})"),"[SECRET MASQUE]");
  value=std::regex_replace(value,std::regex(R"(Bearer[[:space:]]+[^[:space:]]+)"),"Bearer [SECRET MASQUE]");
  if(value.size()>400)value=value.substr(0,400)+"...";
  return value;
}

std::string api_http_diagnostic(long status,const std::string& response_body,long duration_ms){
  std::ostringstream output;output<<"HTTP "<<status<<" en "<<elapsed_text(duration_ms);
  try{
    const auto response=json::parse(response_body);
    const auto error=response.value("error",json::object());
    const auto type=error.value("type","");
    const auto code=error.value("code","");
    const auto message=redact_secrets(error.value("message",""));
    if(!type.empty())output<<" | type="<<type;
    if(!code.empty())output<<" | code="<<code;
    if(!message.empty())output<<" | message="<<message;
  }catch(const json::exception&){
    output<<" | corps d'erreur illisible";
  }
  return output.str();
}

namespace {
struct PostResult { json value{nullptr}; std::string error; };

PostResult post_response(ApiCallBudget& budget,const std::string& key,const std::string& model,const std::string& url,const std::string& instructions,const json& input,const json& schema){
  if(!budget.try_acquire()) return {nullptr,"budget d'appels épuisé ou verrou de budget indisponible"};
  const auto started=std::chrono::steady_clock::now();
  const auto duration_ms=[&](){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();};
  json body={{"model",model},{"instructions",instructions},{"input",input.dump()},{"text",{{"format",{{"type","json_schema"},{"name","autopoiesis_output"},{"strict",true},{"schema",schema}}}}}};
  CURL* curl=curl_easy_init(); if(!curl) return {nullptr,"initialisation cURL impossible"};
  std::string response; auto payload=body.dump();
  char curl_error[CURL_ERROR_SIZE]{};
  struct curl_slist* headers=nullptr;
  headers=curl_slist_append(headers,"Content-Type: application/json");
  headers=curl_slist_append(headers,("Authorization: Bearer "+key).c_str());
  curl_easy_setopt(curl,CURLOPT_URL,(url+"/responses").c_str());
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,payload.c_str());
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_body);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&response);
  curl_easy_setopt(curl,CURLOPT_ERRORBUFFER,curl_error);
  curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,10L);
  curl_easy_setopt(curl,CURLOPT_TIMEOUT,positive_timeout_from_env());
  curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
  auto rc=curl_easy_perform(curl); long status=0; curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
  curl_slist_free_all(headers); curl_easy_cleanup(curl);
  const auto elapsed=duration_ms();
  if(rc!=CURLE_OK){
    const std::string detail=*curl_error?curl_error:curl_easy_strerror(rc);
    return {nullptr,"cURL "+std::to_string(static_cast<int>(rc))+" en "+elapsed_text(elapsed)+" | "+redact_secrets(detail)};
  }
  if(status<200||status>=300) return {nullptr,api_http_diagnostic(status,response,elapsed)};
  try{
    auto result=json::parse(response); std::string text=result.value("output_text","");
    if(text.empty()&&result.contains("output")) for(auto&item:result["output"]) for(auto&content:item.value("content",json::array())) if(content.value("type","")=="output_text") text=content.value("text","");
    if(text.empty())return {nullptr,"HTTP "+std::to_string(status)+" en "+elapsed_text(elapsed)+" | réponse sans output_text | status="+result.value("status","inconnu")};
    try{return {json::parse(text),{}};}catch(const json::exception& error){return {nullptr,"HTTP "+std::to_string(status)+" en "+elapsed_text(elapsed)+" | sortie structurée invalide | "+redact_secrets(error.what())};}
  }catch(const json::exception& error){return {nullptr,"HTTP "+std::to_string(status)+" en "+elapsed_text(elapsed)+" | réponse Responses invalide | "+redact_secrets(error.what())};}
}
}

void OpenAIClient::record_result(const std::string& operation,const std::string& agent_name,const std::string& error){
  last_error_=error;
  if(!error.empty()&&diagnostic_sink_)diagnostic_sink_("API "+operation+" de "+agent_name+" indisponible : "+error);
}

static json report_schema(){
  return {{"type","object"},{"additionalProperties",false},{"properties",{
    {"character_voice",{{"type","string"},{"description","Texte uniquement en francais."}}},
    {"day_summary",{{"type","string"},{"description","Resume uniquement en francais."}}},
    {"state_assessment",{{"type","string"},{"description","Evaluation uniquement en francais."}}},
    {"memory_summary",{{"type","string"},{"maxLength",180},{"description","Une phrase factuelle tres courte en francais a conserver comme bilan."}}},
    {"memory_feeling",{{"type","string"},{"maxLength",180},{"description","Une phrase subjective tres courte en francais a conserver comme ressenti."}}},
    {"ask_god",{{"type","boolean"}}}
  }},{"required",{"character_voice","day_summary","state_assessment","memory_summary","memory_feeling","ask_god"}}};
}

static json french_string(){
  return {{"type","string"},{"description","Texte uniquement en francais."}};
}

static json french_string_array(){
  return {{"type","array"},{"items",french_string()}};
}

static json feature_request_schema(){
  return {{"type","object"},{"additionalProperties",false},{"properties",{
    {"requested",{{"type","boolean"}}},{"evolution_key",{{"type","string"},{"description","Cle technique stable en snake_case pour dedupliquer le mecanisme."}}},{"domain",{{"type","string"},{"enum",{"survie","construction","production","exploration","social","connaissance"}}}},{"title",french_string()},{"need",french_string()},{"obstacle",french_string()},{"proposed_change",french_string()},
    {"mechanism",{{"type","object"},{"additionalProperties",false},{"properties",{
      {"name",french_string()},{"summary",french_string()},{"resources",french_string_array()},{"actions",french_string_array()},{"preconditions",french_string_array()},{"deterministic_effects",french_string_array()}
    }},{"required",{"name","summary","resources","actions","preconditions","deterministic_effects"}}}},
    {"acceptance_tests",french_string_array()}
  }},{"required",{"requested","evolution_key","domain","title","need","obstacle","proposed_change","mechanism","acceptance_tests"}}};
}

std::string period_report_instructions(){
  return "Personnifie ce personnage et redige le bilan de la periode terminee. "
         "Reponds exclusivement en francais dans tous les champs textuels, sans titre ni expression en anglais. "
         "Explique la periode depuis son point de vue et evalue son etat. Inclus uniquement la voix du personnage, "
         "le resume de la periode, l'evaluation de son etat et l'indication qu'une demande d'evolution doit suivre ou non. "
         "Relie explicitement ses actions a son aspiration et a son projet durable. Mentionne les progres, la monotonie, "
         "les relations et surtout toute capacite manquante qui bloque ce projet. "
         "Utilise les souvenirs des periodes precedentes pour conserver une continuite sans les recopier. "
         "Produis aussi memory_summary et memory_feeling : exactement une phrase tres courte chacun, 180 caracteres maximum, "
         "la premiere factuelle pour le bilan persistant et la seconde subjective pour le ressenti persistant. "
         "Ne propose et ne pretend aucun changement de code ou du monde dans cet appel.";
}

std::string evolution_request_instructions(){
  return "A partir du bilan termine et de l'historique d'actions verifie, formule exactement une demande d'evolution "
         "soumise a validation humaine. Redige exclusivement en francais tous les champs textuels, y compris title, "
         "need, obstacle, proposed_change, les champs de mechanism et chaque element de acceptance_tests. "
         "Priorise la capacite manquante qui bloque le projet durable ou l'aspiration du personnage. Utilise evolution_key "
         "comme cle technique stable et choisis un domain. La proposition doit etre distincte des cles deja proposees "
         "dans cette fenetre. Examine d'abord active_world_mechanisms et evolution_history. Ne redemande jamais un mecanisme "
         "deja actif, pending, approved ou activated, meme sous un autre titre ou une nouvelle cle evolution_key. Si une "
         "capacite active ne suffit pas, demande uniquement l'integration manquante precise et prouvee par l'historique, "
         "sans recreer cette capacite. Ne propose pas une simple optimisation de navigation, anti-boucle, repos ou seuil numerique, "
         "sauf si le contexte prouve qu'elle bloque directement la survie ou le projet. "
         "Decris un seul mecanisme deterministe incremental avec ses ressources, actions, preconditions, effets "
         "deterministes et tests d'acceptation executables. N'implemente et n'active rien : cette sortie est seulement "
         "une proposition pending destinee au Validator et a Dieu.";
}

static json character_context(const Agent& agent,const std::vector<std::string>& history){
  return {{"id",agent.id},{"name",agent.name},{"position",{{"x",agent.position.x},{"y",agent.position.y}}},
          {"health",agent.health},{"hunger",agent.hunger},{"thirst",agent.thirst},{"fatigue",agent.fatigue},
          {"boredom",agent.boredom},{"alive",agent.alive},{"personality",personality_json(agent.personality)},
          {"attributes",attributes_json(agent.attributes)},{"behavior",behavior_json(agent.behavior)},
          {"project",project_json(agent.project)},{"relationships",relationships_json(agent.relationships)},
          {"observed_animals",agent.observed_animals},{"memories",agent.memories},
          {"known_map_cells",agent.map_memory.size()},{"recent_actions",history}};
}

Decision OpenAIClient::decide(const Perception& p){
  const auto schema=json::parse(R"({"type":"object","additionalProperties":false,"properties":{"type":{"type":"string","enum":["action","blocked"]},"action":{"type":["string","null"]},"parameters":{"type":["object","null"]},"reason":{"type":["string","null"]},"need":{"type":["string","null"]},"obstacle":{"type":["string","null"]},"desired_result":{"type":["string","null"]}},"required":["type","action","parameters","reason","need","obstacle","desired_result"]})");
  auto result=post_response(budget_,key_,model_,base_url_,"Choose one currently available action or report blocked. Personality influences risk, exploration, cooperation, and persistence. Never invent actions; reason is only for logs.",p.value,schema);
  record_result("décision",p.value.value("self",json::object()).value("name","inconnu"),result.error);
  Decision d; std::string error; if(!result.value.is_null()&&parse_decision(result.value,d,error)) return d;
  Decision fallback; fallback.reason="API unavailable or call limit reached"; return fallback;
}

json OpenAIClient::report_period(int simulation_cycle,int day,const Agent& agent,const std::vector<std::string>& history){
  const auto date=date_from_absolute_day(day);
  return report_period(simulation_cycle,day,agent,history,{date,climate_for(date),json::array()});
}

json OpenAIClient::report_period(int simulation_cycle,int day,const Agent& agent,const std::vector<std::string>& history,const PeriodContext& period){
  json context={{"output_language","fr-FR"},{"day",day},{"simulation_cycle",simulation_cycle},
                {"calendar",calendar_json(period.date)},{"climate",climate_json(period.climate)},
                {"previous_period_memories",period.previous_memories},{"character",character_context(agent,history)}};
  auto result=post_response(budget_,key_,model_,base_url_,period_report_instructions(),context,report_schema());
  record_result("bilan",agent.name,result.error);
  return result.value;
}

json OpenAIClient::request_evolution(int simulation_cycle,int day,const Agent& agent,const std::vector<std::string>& history,const json& report){
  return request_evolution(simulation_cycle,day,agent,history,report,
                           {active_world_mechanisms(),json::array(),{}});
}

json OpenAIClient::request_evolution(int simulation_cycle,int day,const Agent& agent,const std::vector<std::string>& history,const json& report,const EvolutionContext& context){
  if(proposal_window_cycle_!=simulation_cycle){proposal_window_cycle_=simulation_cycle;proposals_in_window_.clear();}
  json input={{"output_language","fr-FR"},{"day",day},{"simulation_cycle",simulation_cycle},
              {"character",character_context(agent,history)},{"report",report},
              {"currently_available_actions",context.currently_available_actions},
              {"active_world_mechanisms",context.active_world_mechanisms},
              {"evolution_history",context.evolution_history},
              {"proposals_already_made",proposals_in_window_}};
  auto result=post_response(budget_,key_,model_,base_url_,evolution_request_instructions(),input,feature_request_schema());
  record_result("demande d'évolution",agent.name,result.error);
  if(result.value.is_object()&&result.value.value("requested",false))
    proposals_in_window_.push_back({{"evolution_key",result.value.value("evolution_key","")},{"title",result.value.value("title","")}});
  return result.value;
}
}
