#include "autopoiesis/openai_client.hpp"
#include <curl/curl.h>
#include <cstdlib>

namespace apo {
OpenAIClient::OpenAIClient(std::string key,std::string model,std::string base,ApiCallBudget budget)
  : key_(std::move(key)),model_(std::move(model)),base_url_(std::move(base)),budget_(std::move(budget)) { while(!base_url_.empty()&&base_url_.back()=='/') base_url_.pop_back(); }

static size_t write_body(char* p,size_t s,size_t n,void* u){static_cast<std::string*>(u)->append(p,s*n);return s*n;}

static json post_response(ApiCallBudget& budget,const std::string& key,const std::string& model,const std::string& url,const std::string& instructions,const json& input,const json& schema){
  if(!budget.try_acquire()) return nullptr;
  json body={{"model",model},{"instructions",instructions},{"input",input.dump()},{"text",{{"format",{{"type","json_schema"},{"name","autopoiesis_output"},{"strict",true},{"schema",schema}}}}}};
  CURL* curl=curl_easy_init(); if(!curl) return nullptr;
  std::string response; auto payload=body.dump();
  struct curl_slist* headers=nullptr;
  headers=curl_slist_append(headers,"Content-Type: application/json");
  headers=curl_slist_append(headers,("Authorization: Bearer "+key).c_str());
  curl_easy_setopt(curl,CURLOPT_URL,(url+"/responses").c_str());
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,payload.c_str());
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_body);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&response);
  curl_easy_setopt(curl,CURLOPT_TIMEOUT,20L);
  auto rc=curl_easy_perform(curl); long status=0; curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
  curl_slist_free_all(headers); curl_easy_cleanup(curl);
  if(rc!=CURLE_OK||status<200||status>=300) return nullptr;
  try{
    auto result=json::parse(response); std::string text=result.value("output_text","");
    if(text.empty()&&result.contains("output")) for(auto&item:result["output"]) for(auto&content:item.value("content",json::array())) if(content.value("type","")=="output_text") text=content.value("text","");
    return json::parse(text);
  }catch(...){return nullptr;}
}

static json report_schema(){
  return {{"type","object"},{"additionalProperties",false},{"properties",{
    {"character_voice",{{"type","string"},{"description","Texte uniquement en francais."}}},
    {"day_summary",{{"type","string"},{"description","Resume uniquement en francais."}}},
    {"state_assessment",{{"type","string"},{"description","Evaluation uniquement en francais."}}},
    {"ask_god",{{"type","boolean"}}}
  }},{"required",{"character_voice","day_summary","state_assessment","ask_god"}}};
}

static json french_string(){
  return {{"type","string"},{"description","Texte uniquement en francais."}};
}

static json french_string_array(){
  return {{"type","array"},{"items",french_string()}};
}

static json feature_request_schema(){
  return {{"type","object"},{"additionalProperties",false},{"properties",{
    {"requested",{{"type","boolean"}}},{"title",french_string()},{"need",french_string()},{"obstacle",french_string()},{"proposed_change",french_string()},
    {"mechanism",{{"type","object"},{"additionalProperties",false},{"properties",{
      {"name",french_string()},{"summary",french_string()},{"resources",french_string_array()},{"actions",french_string_array()},{"preconditions",french_string_array()},{"deterministic_effects",french_string_array()}
    }},{"required",{"name","summary","resources","actions","preconditions","deterministic_effects"}}}},
    {"acceptance_tests",french_string_array()}
  }},{"required",{"requested","title","need","obstacle","proposed_change","mechanism","acceptance_tests"}}};
}

std::string period_report_instructions(){
  return "Personnifie ce personnage et redige le bilan de la periode terminee. "
         "Reponds exclusivement en francais dans tous les champs textuels, sans titre ni expression en anglais. "
         "Explique la periode depuis son point de vue et evalue son etat. Inclus uniquement la voix du personnage, "
         "le resume de la periode, l'evaluation de son etat et l'indication qu'une demande d'evolution doit suivre ou non. "
         "Ne propose et ne pretend aucun changement de code ou du monde dans cet appel.";
}

std::string evolution_request_instructions(){
  return "A partir du bilan termine et de l'historique d'actions verifie, formule exactement une demande d'evolution "
         "soumise a validation humaine. Redige exclusivement en francais tous les champs textuels, y compris title, "
         "need, obstacle, proposed_change, les champs de mechanism et chaque element de acceptance_tests. "
         "Decris un seul mecanisme deterministe incremental avec ses ressources, actions, preconditions, effets "
         "deterministes et tests d'acceptation executables. N'implemente et n'active rien : cette sortie est seulement "
         "une proposition pending destinee au Validator et a Dieu.";
}

Decision OpenAIClient::decide(const Perception& p){
  const auto schema=json::parse(R"({"type":"object","additionalProperties":false,"properties":{"type":{"type":"string","enum":["action","blocked"]},"action":{"type":["string","null"]},"parameters":{"type":["object","null"]},"reason":{"type":["string","null"]},"need":{"type":["string","null"]},"obstacle":{"type":["string","null"]},"desired_result":{"type":["string","null"]}},"required":["type","action","parameters","reason","need","obstacle","desired_result"]})");
  auto result=post_response(budget_,key_,model_,base_url_,"Choose one currently available action or report blocked. Personality influences risk, exploration, cooperation, and persistence. Never invent actions; reason is only for logs.",p.value,schema);
  Decision d; std::string error; if(!result.is_null()&&parse_decision(result,d,error)) return d;
  Decision fallback; fallback.reason="API unavailable or call limit reached"; return fallback;
}

json OpenAIClient::report_period(int simulation_cycle,int day,const Agent& agent,const std::vector<std::string>& history){
  json context={{"output_language","fr-FR"},{"day",day},{"simulation_cycle",simulation_cycle},{"character",{{"id",agent.id},{"name",agent.name},{"position",{{"x",agent.position.x},{"y",agent.position.y}}},{"health",agent.health},{"hunger",agent.hunger},{"fatigue",agent.fatigue},{"alive",agent.alive},{"personality",personality_json(agent.personality)},{"memories",agent.memories},{"known_map_cells",agent.map_memory.size()},{"recent_actions",history}}}};
  return post_response(budget_,key_,model_,base_url_,period_report_instructions(),context,report_schema());
}

json OpenAIClient::request_evolution(int simulation_cycle,int day,const Agent& agent,const std::vector<std::string>& history,const json& report){
  json context={{"output_language","fr-FR"},{"day",day},{"simulation_cycle",simulation_cycle},{"character",{{"id",agent.id},{"name",agent.name},{"position",{{"x",agent.position.x},{"y",agent.position.y}}},{"health",agent.health},{"hunger",agent.hunger},{"fatigue",agent.fatigue},{"alive",agent.alive},{"personality",personality_json(agent.personality)},{"memories",agent.memories},{"known_map_cells",agent.map_memory.size()},{"recent_actions",history}}},{"report",report}};
  return post_response(budget_,key_,model_,base_url_,evolution_request_instructions(),context,feature_request_schema());
}
}
