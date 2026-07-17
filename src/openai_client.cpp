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
    {"character_voice",{{"type","string"}}},{"day_summary",{{"type","string"}}},{"state_assessment",{{"type","string"}}},{"ask_god",{{"type","boolean"}}}
  }},{"required",{"character_voice","day_summary","state_assessment","ask_god"}}};
}

static json feature_request_schema(){
  return {{"type","object"},{"additionalProperties",false},{"properties",{
    {"requested",{{"type","boolean"}}},{"title",{{"type","string"}}},{"need",{{"type","string"}}},{"obstacle",{{"type","string"}}},{"proposed_change",{{"type","string"}}},
    {"mechanism",{{"type","object"},{"additionalProperties",false},{"properties",{
      {"name",{{"type","string"}}},{"summary",{{"type","string"}}},{"resources",{{"type","array"},{"items",{{"type","string"}}}}},{"actions",{{"type","array"},{"items",{{"type","string"}}}}},{"preconditions",{{"type","array"},{"items",{{"type","string"}}}}},{"deterministic_effects",{{"type","array"},{"items",{{"type","string"}}}}}
    }},{"required",{"name","summary","resources","actions","preconditions","deterministic_effects"}}}},
    {"acceptance_tests",{{"type","array"},{"items",{{"type","string"}}}}}
  }},{"required",{"requested","title","need","obstacle","proposed_change","mechanism","acceptance_tests"}}};
}

Decision OpenAIClient::decide(const Perception& p){
  const auto schema=json::parse(R"({"type":"object","additionalProperties":false,"properties":{"type":{"type":"string","enum":["action","blocked"]},"action":{"type":["string","null"]},"parameters":{"type":["object","null"]},"reason":{"type":["string","null"]},"need":{"type":["string","null"]},"obstacle":{"type":["string","null"]},"desired_result":{"type":["string","null"]}},"required":["type","action","parameters","reason","need","obstacle","desired_result"]})");
  auto result=post_response(budget_,key_,model_,base_url_,"Choose one currently available action or report blocked. Personality influences risk, exploration, cooperation, and persistence. Never invent actions; reason is only for logs.",p.value,schema);
  Decision d; std::string error; if(!result.is_null()&&parse_decision(result,d,error)) return d;
  Decision fallback; fallback.reason="API unavailable or call limit reached"; return fallback;
}

json OpenAIClient::report_period(int simulation_cycle,int day,const Agent& agent,const std::vector<std::string>& history){
  json context={{"day",day},{"simulation_cycle",simulation_cycle},{"character",{{"id",agent.id},{"name",agent.name},{"position",{{"x",agent.position.x},{"y",agent.position.y}}},{"health",agent.health},{"hunger",agent.hunger},{"fatigue",agent.fatigue},{"alive",agent.alive},{"personality",personality_json(agent.personality)},{"memories",agent.memories},{"known_map_cells",agent.map_memory.size()},{"recent_actions",history}}}};
  std::string instructions="Personify this character and write a report for the completed simulation period. Explain the period from its perspective and assess its state. Include only the character voice, day summary, state assessment, and whether an evolution request should follow. Do not propose or claim any code or world change in this call.";
  return post_response(budget_,key_,model_,base_url_,instructions,context,report_schema());
}

json OpenAIClient::request_evolution(int simulation_cycle,int day,const Agent& agent,const std::vector<std::string>& history,const json& report){
  json context={{"day",day},{"simulation_cycle",simulation_cycle},{"character",{{"id",agent.id},{"name",agent.name},{"position",{{"x",agent.position.x},{"y",agent.position.y}}},{"health",agent.health},{"hunger",agent.hunger},{"fatigue",agent.fatigue},{"alive",agent.alive},{"personality",personality_json(agent.personality)},{"memories",agent.memories},{"known_map_cells",agent.map_memory.size()},{"recent_actions",history}}},{"report",report}};
  std::string instructions="Using the completed character report and its verified action history, formulate exactly one human-reviewed evolution request. Describe one incremental deterministic mechanism with resources, actions, preconditions, deterministic effects, and executable acceptance tests. Never implement or activate anything; this output is only a pending proposal for the Validator and God.";
  return post_response(budget_,key_,model_,base_url_,instructions,context,feature_request_schema());
}
}
