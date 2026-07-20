#include "autopoiesis/simulation.hpp"
#include "autopoiesis/feature_request.hpp"
#include "autopoiesis/renderer.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace apo {
namespace {
struct ActivityResult {
  json payload;
  bool interface_open{true};
};

template<typename Work>
ActivityResult run_with_activity(IUserInterface* interface, UiActivity activity, Work work) {
  if(!interface)return {work(),true};

  std::atomic_bool finished{false};
  json payload=nullptr;
  std::exception_ptr failure;
  std::thread worker([&]{
    try{payload=work();}catch(...){failure=std::current_exception();}
    finished.store(true,std::memory_order_release);
  });
  const auto started=std::chrono::steady_clock::now();
  bool interface_open=true;
  do{
    activity.elapsed_ms=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now()-started).count();
    if(interface_open)interface_open=interface->present_activity(activity);
    if(!finished.load(std::memory_order_acquire))
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }while(!finished.load(std::memory_order_acquire));
  worker.join();
  if(failure)std::rethrow_exception(failure);
  return {std::move(payload),interface_open};
}

std::string rng_checkpoint(const std::mt19937& rng) {
  std::ostringstream output;output<<rng;return output.str();
}

void restore_rng(std::mt19937& rng,const std::string& state) {
  std::istringstream input(state);input>>rng;
  if(!input)throw std::runtime_error("checkpoint simulation RNG is invalid");
}

json agent_checkpoint(const Agent& agent) {
  json memories=json::array();for(const auto& memory:agent.memories)memories.push_back(memory);
  json map_memory=json::array();
  for(const auto&[position,terrain]:agent.map_memory)map_memory.push_back({
      {"x",position.first},{"y",position.second},{"terrain",static_cast<int>(terrain)}});
  json visits=json::array();
  for(const auto&[position,count]:agent.map_visit_counts)visits.push_back({
      {"x",position.first},{"y",position.second},{"count",count}});
  json foods=json::array();
  for(const auto food:agent.behavior.preferred_foods)foods.push_back(static_cast<int>(food));
  json observed=json::array();for(const auto& animal:agent.observed_animals)observed.push_back(animal);
  json mourned=json::array();for(const auto& id:agent.mourned_agents)mourned.push_back(id);
  json conditions=json::array();for(const auto& condition:agent.conditions)conditions.push_back({
      {"id",condition.id},{"type",static_cast<int>(condition.type)},{"severity",condition.severity},
      {"days",condition.days},{"treated",condition.treated},{"cause",condition.cause}});
  json emotions=json::array();for(const auto& emotion:agent.emotions)emotions.push_back({
      {"id",emotion.id},{"type",static_cast<int>(emotion.type)},{"intensity",emotion.intensity},
      {"cause",emotion.cause},{"effect",emotion.effect},{"memory",emotion.memory},
      {"created_day",emotion.created_day},{"remaining_days",emotion.remaining_days}});
  json campfires=json::array();for(const auto& position:agent.known_campfires)campfires.push_back({{"x",position.first},{"y",position.second}});
  json home=nullptr;if(agent.home_camp)home={{"x",agent.home_camp->x},{"y",agent.home_camp->y}};
  json rest_position=nullptr;if(agent.camp_rest_position)rest_position={{"x",agent.camp_rest_position->x},{"y",agent.camp_rest_position->y}};
  json carried=nullptr;if(agent.carried_food)carried={{"type",static_cast<int>(agent.carried_food->type)},{"nutrition",agent.carried_food->nutrition},{"cooked",agent.carried_food->cooked},{"age_days",agent.carried_food->age_days},{"shelf_life_days",agent.carried_food->shelf_life_days}};
  json tool=nullptr;if(agent.equipped_tool)tool={{"type",static_cast<int>(agent.equipped_tool->type)},
      {"durability",agent.equipped_tool->durability},
      {"maximum_durability",agent.equipped_tool->maximum_durability}};
  json shelter=nullptr;
  if(agent.shelter_construction)shelter={{"x",agent.shelter_construction->position.x},
      {"y",agent.shelter_construction->position.y},{"progress",agent.shelter_construction->progress}};
  return {{"id",agent.id},{"name",agent.name},{"x",agent.position.x},{"y",agent.position.y},
          {"health",agent.health},{"hunger",agent.hunger},{"fatigue",agent.fatigue},
          {"thirst",agent.thirst},{"personality",personality_json(agent.personality)},
          {"attributes",attributes_json(agent.attributes)},{"memories",std::move(memories)},
          {"alive",agent.alive},{"sleeping_days",agent.sleeping_days},
          {"critical_hunger_days",agent.critical_hunger_days},
          {"critical_thirst_days",agent.critical_thirst_days},
          {"map_memory",std::move(map_memory)},{"map_visit_counts",std::move(visits)},
          {"behavior",{{"archetype",agent.behavior.archetype},{"aspiration",agent.behavior.aspiration},
                       {"construction_drive",agent.behavior.construction_drive},
                       {"provision_drive",agent.behavior.provision_drive},
                       {"exploration_drive",agent.behavior.exploration_drive},
                       {"social_drive",agent.behavior.social_drive},{"preferred_foods",std::move(foods)}}},
          {"project",{{"key",agent.project.key},{"title",agent.project.title},
                      {"status",static_cast<int>(agent.project.status)},{"step",agent.project.step},
                      {"progress",agent.project.progress},{"target",agent.project.target},
                      {"blocked_reason",agent.project.blocked_reason},
                      {"missing_capability",agent.project.missing_capability},
                      {"started_day",agent.project.started_day},
                      {"last_progress_cycle",agent.project.last_progress_cycle}}},
          {"boredom",agent.boredom},{"relationships",relationships_json(agent.relationships)},
          {"observed_animals",std::move(observed)},{"known_campfires",std::move(campfires)},
          {"home_camp",std::move(home)},{"camp_rest_position",std::move(rest_position)},
          {"wood_inventory",agent.wood_inventory},
          {"branch_inventory",agent.branch_inventory},
          {"iron_ore_inventory",agent.iron_ore_inventory},
          {"carried_food",std::move(carried)},
          {"equipped_tool",std::move(tool)},
          {"shelter_construction",std::move(shelter)},
          {"community_role",agent.community_role},
          {"last_shared_meal_day",agent.last_shared_meal_day},
          {"last_vigil_day",agent.last_vigil_day},
          {"celebration_pending",agent.celebration_pending},
          {"mourned_agents",std::move(mourned)},
          {"skills",skills_json(agent)},
          {"last_taught_day",agent.last_taught_day},
          {"last_lesson_day",agent.last_lesson_day},
          {"conditions",std::move(conditions)},
          {"next_condition_id",agent.next_condition_id},
          {"last_convalescence_day",agent.last_convalescence_day},
          {"emotions",std::move(emotions)},{"next_emotion_id",agent.next_emotion_id},
          {"companion_id",agent.companion_id},{"companion_until_day",agent.companion_until_day},
          {"last_help_day",agent.last_help_day},{"last_warning_day",agent.last_warning_day},
          {"age_days",agent.age_days},{"origin",agent.origin},{"arrival_day",agent.arrival_day},
          {"parent_ids",agent.parent_ids},{"departure_day",agent.departure_day},
          {"departure_reason",agent.departure_reason},{"death_cause",agent.death_cause}};
}

Agent restore_agent(const json& state) {
  Agent agent;
  agent.id=state.at("id").get<std::string>();agent.name=state.at("name").get<std::string>();
  agent.position={state.at("x").get<int>(),state.at("y").get<int>()};
  agent.health=state.at("health").get<int>();agent.hunger=state.at("hunger").get<int>();
  agent.fatigue=state.at("fatigue").get<int>();agent.thirst=state.at("thirst").get<int>();
  const auto& personality=state.at("personality");
  agent.personality={personality.at("curiosity").get<int>(),personality.at("prudence").get<int>(),
      personality.at("sociability").get<int>(),personality.at("patience").get<int>(),
      personality.at("empathy").get<int>()};
  const auto& attributes=state.at("attributes");
  agent.attributes={attributes.at("strength").get<int>(),attributes.at("agility").get<int>(),
      attributes.at("endurance").get<int>(),attributes.at("toughness").get<int>(),
      attributes.at("recuperation").get<int>(),attributes.at("disease_resistance").get<int>(),
      attributes.at("focus").get<int>(),attributes.at("willpower").get<int>(),
      attributes.at("memory").get<int>(),attributes.at("spatial_sense").get<int>()};
  for(const auto& memory:state.at("memories"))agent.memories.push_back(memory.get<std::string>());
  agent.alive=state.at("alive").get<bool>();agent.sleeping_days=state.at("sleeping_days").get<int>();
  agent.critical_hunger_days=state.at("critical_hunger_days").get<int>();
  agent.critical_thirst_days=state.at("critical_thirst_days").get<int>();
  for(const auto& value:state.at("map_memory"))agent.map_memory[
      {value.at("x").get<int>(),value.at("y").get<int>()}]=static_cast<Terrain>(value.at("terrain").get<int>());
  for(const auto& value:state.at("map_visit_counts"))agent.map_visit_counts[
      {value.at("x").get<int>(),value.at("y").get<int>()}]=value.at("count").get<int>();
  const auto& behavior=state.at("behavior");
  agent.behavior.archetype=behavior.at("archetype").get<std::string>();
  agent.behavior.aspiration=behavior.at("aspiration").get<std::string>();
  agent.behavior.construction_drive=behavior.at("construction_drive").get<int>();
  agent.behavior.provision_drive=behavior.at("provision_drive").get<int>();
  agent.behavior.exploration_drive=behavior.at("exploration_drive").get<int>();
  agent.behavior.social_drive=behavior.at("social_drive").get<int>();
  for(const auto& food:behavior.at("preferred_foods"))
    agent.behavior.preferred_foods.push_back(static_cast<FoodType>(food.get<int>()));
  const auto& project=state.at("project");
  agent.project={project.at("key").get<std::string>(),project.at("title").get<std::string>(),
      static_cast<ProjectStatus>(project.at("status").get<int>()),project.at("step").get<int>(),
      project.at("progress").get<int>(),project.at("target").get<int>(),
      project.at("blocked_reason").get<std::string>(),project.at("missing_capability").get<std::string>(),
      project.at("started_day").get<int>(),project.at("last_progress_cycle").get<int>()};
  agent.boredom=state.at("boredom").get<int>();
  for(const auto&[id,value]:state.at("relationships").items())agent.relationships[id]={
      value.at("trust").get<int>(),value.at("affinity").get<int>(),value.at("interactions").get<int>(),
      value.value("conflict",false)};
  for(const auto& animal:state.at("observed_animals"))agent.observed_animals.insert(animal.get<std::string>());
  for(const auto& fire:state.value("known_campfires",json::array()))
    agent.known_campfires.insert({fire.at("x").get<int>(),fire.at("y").get<int>()});
  const auto home=state.value("home_camp",json(nullptr));
  if(!home.is_null())agent.home_camp=Position{home.at("x").get<int>(),home.at("y").get<int>()};
  const auto rest_position=state.value("camp_rest_position",json(nullptr));
  if(!rest_position.is_null())agent.camp_rest_position=Position{
      rest_position.at("x").get<int>(),rest_position.at("y").get<int>()};
  agent.wood_inventory=state.at("wood_inventory").get<int>();
  agent.branch_inventory=state.value("branch_inventory",0);
  agent.iron_ore_inventory=state.value("iron_ore_inventory",0);
  const auto carried=state.value("carried_food",json(nullptr));
  if(!carried.is_null())agent.carried_food=FoodItem{
      static_cast<FoodType>(carried.at("type").get<int>()),carried.at("nutrition").get<int>(),
      carried.value("cooked",false),carried.value("age_days",0),
      carried.value("shelf_life_days",food_shelf_life(static_cast<FoodType>(carried.at("type").get<int>())))};
  const auto tool=state.value("equipped_tool",json(nullptr));
  if(!tool.is_null())agent.equipped_tool=Tool{static_cast<CraftItem>(tool.at("type").get<int>()),
      tool.at("durability").get<int>(),tool.value("maximum_durability",20)};
  if(!state.at("shelter_construction").is_null()){
    const auto& shelter=state.at("shelter_construction");
    agent.shelter_construction=ShelterConstruction{
        {shelter.at("x").get<int>(),shelter.at("y").get<int>()},shelter.at("progress").get<int>()};
  }
  agent.community_role=state.value("community_role","");
  agent.last_shared_meal_day=state.value("last_shared_meal_day",0);
  agent.last_vigil_day=state.value("last_vigil_day",0);
  agent.celebration_pending=state.value("celebration_pending",false);
  for(const auto& id:state.value("mourned_agents",json::array()))
    agent.mourned_agents.insert(id.get<std::string>());
  const auto saved_skills=state.value("skills",json::object());
  for(const auto&[name,value]:saved_skills.items())if(const auto skill=skill_from_name(name))
    agent.skills[*skill]={value.value("experience",0),value.value("level",0)};
  agent.last_taught_day=state.value("last_taught_day",0);
  agent.last_lesson_day=state.value("last_lesson_day",0);
  for(const auto& condition:state.value("conditions",json::array()))agent.conditions.push_back({
      condition.at("id").get<std::string>(),static_cast<HealthConditionType>(condition.at("type").get<int>()),
      condition.value("severity",1),condition.value("days",0),condition.value("treated",false),
      condition.value("cause","")});
  agent.next_condition_id=state.value("next_condition_id",1);
  agent.last_convalescence_day=state.value("last_convalescence_day",0);
  for(const auto& emotion:state.value("emotions",json::array()))agent.emotions.push_back({
      emotion.at("id").get<std::string>(),static_cast<EmotionType>(emotion.at("type").get<int>()),
      emotion.value("intensity",1),emotion.value("cause",""),emotion.value("effect",""),
      emotion.value("memory",emotion.value("cause","")),emotion.value("created_day",0),
      emotion.value("remaining_days",1)});
  agent.next_emotion_id=state.value("next_emotion_id",1);
  agent.companion_id=state.value("companion_id","");
  agent.companion_until_day=state.value("companion_until_day",0);
  agent.last_help_day=state.value("last_help_day",0);
  agent.last_warning_day=state.value("last_warning_day",0);
  agent.age_days=state.value("age_days",25*360);
  agent.origin=state.value("origin","founder");agent.arrival_day=state.value("arrival_day",1);
  agent.parent_ids=state.value("parent_ids",std::vector<std::string>{});
  agent.departure_day=state.value("departure_day",0);
  agent.departure_reason=state.value("departure_reason","");agent.death_cause=state.value("death_cause","");
  return agent;
}

std::string collective_role(const BehaviorProfile& behavior) {
  const std::array<std::pair<int,const char*>,4> roles{{
      {behavior.construction_drive,"bâtisseur"},{behavior.provision_drive,"intendant"},
      {behavior.exploration_drive,"éclaireur"},{behavior.social_drive,"médiateur"}}};
  return std::max_element(roles.begin(),roles.end(),[](const auto& left,const auto& right){
    return left.first<right.first;
  })->second;
}
}

static int positive_int_from_env(const char* name, int fallback) {
  const char* configured=std::getenv(name);
  if(!configured) return fallback;
  try {
    int value=std::stoi(configured);
    return value>0 ? value : fallback;
  } catch(...) {
    return fallback;
  }
}

Decision LocalDecider::decide(const Perception& p) {
  const auto& me=p.value["self"];
  auto acts=p.value["available_actions"].get<std::vector<std::string>>();
  const auto has_action=[&](const std::string& action){return std::find(acts.begin(),acts.end(),action)!=acts.end();};
  const int hunger=me.value("hunger",0),thirst=me.value("thirst",0),fatigue=me.value("fatigue",0);
  const auto time=p.value.value("time",json::object());
  const bool is_night=time.value("phase","")=="night";
  const int cycles_until_night=time.value("cycles_until_night",std::numeric_limits<int>::max());
  const int cycles_per_day=time.value("cycles_per_day",2400);
  const int camp_preparation_cycles=std::max(1,daylight_cycles(cycles_per_day)/6);
  const bool camp_time=is_night||cycles_until_night<=camp_preparation_cycles;
  const auto reserved_rest=me.value("camp_rest_position",json(nullptr));
  const bool at_reserved_rest=reserved_rest.is_null()||
      (reserved_rest.value("x",-1)==me.value("x",0)&&reserved_rest.value("y",-1)==me.value("y",0));
  const auto attributes=me.value("attributes",json::object());
  const auto personality=me.value("personality",json::object());
  const auto behavior=me.value("behavior",json::object());
  const auto project=me.value("project",json::object());
  const int boredom=me.value("boredom",0);
  const auto emotions=me.value("emotions",json::array());
  int fear=0,anger=0;for(const auto& emotion:emotions){if(emotion.value("type","")=="fear")fear=std::max(fear,emotion.value("intensity",0));if(emotion.value("type","")=="anger")anger=std::max(anger,emotion.value("intensity",0));}
  const int focus=attributes.value("focus",50),willpower=attributes.value("willpower",50);
  const int width=p.value.value("world_width",World::width),height=p.value.value("world_height",World::height);
  const auto wrap=[&](Position position){return Position{((position.x%width)+width)%width,((position.y%height)+height)%height};};
  const auto step=[&](Position position,const std::string& direction){if(direction=="north")--position.y;if(direction=="south")++position.y;if(direction=="east")++position.x;if(direction=="west")--position.x;return wrap(position);};
  static const std::vector<std::string> directions{"north","east","south","west"};
  const Position self{me.value("x",0),me.value("y",0)};

  std::map<std::string,double> utilities{
      {"hydrate",thirst>=35?thirst*(1.45-willpower/1000.0):0.0},
      {"eat",hunger>=40?hunger*(1.30-willpower/1200.0):0.0},
      {"rest",fatigue>=45?fatigue*(1.15-attributes.value("endurance",50)/1000.0):0.0},
      {"explore",30.0+personality.value("curiosity",50)/5.0}};
  const bool blocked_shelter_project=
      project.value("status","")=="blocked"&&
      project.value("key","")=="build_shelter"&&
      project.value("missing_capability","")=="build_shelter";
  if(project.value("status","")=="active"||blocked_shelter_project){
    int drive=behavior.value("exploration_drive",50);
    if(project.value("key","")=="build_shelter")drive=behavior.value("construction_drive",50);
    if(project.value("key","")=="secure_food")drive=behavior.value("provision_drive",50);
    utilities["project"]=35.0+drive/4.0;
  }
  const auto visible_agents=p.value.value("visible_agents",json::array());
  const bool adjacent_agent=std::any_of(visible_agents.begin(),visible_agents.end(),[](const json& agent){return agent.value("adjacent",false);});
  if(adjacent_agent&&has_action("talk")&&boredom>=25)
    utilities["social"]=20.0+behavior.value("social_drive",50)/5.0+boredom*0.8;
  std::string best_goal="explore";
  for(const auto&[goal,score]:utilities)if(score>utilities[best_goal])best_goal=goal;
  const std::string agent_id=me.value("id","");
  GoalState* goal_state=nullptr;
  if(!agent_id.empty()){
    auto& state=goals_[agent_id];
    const double hysteresis=5.0+focus/5.0;
    if(state.remaining>0&&utilities[state.name]>0&&utilities[state.name]+hysteresis>=utilities[best_goal]){
      best_goal=state.name;--state.remaining;
    }else{
      if(state.name!=best_goal)state.exploration_target.reset();
      state.name=best_goal;
      state.remaining=2+focus/20;
    }
    goal_state=&state;
  }

  if(best_goal=="hydrate"&&has_action("drink"))
    return {DecisionType::Action,"drink",json::object(),"Je bois avant de poursuivre.","water","","be hydrated"};
  if(best_goal=="eat"&&has_action("eat_food"))
    return {DecisionType::Action,"eat_food",json::object(),"Je mange la ressource disponible.","food","","be fed"};
  if(best_goal=="eat"&&has_action("eat_berries"))
    return {DecisionType::Action,"eat_berries",json::object(),"I need food","food","","be fed"};
  if(best_goal=="eat"&&has_action("eat_carried_food"))
    return {DecisionType::Action,"eat_carried_food",json::object(),"Je mange la nourriture que je transporte.","food","","be fed"};
  if(best_goal=="eat"&&has_action("share_camp_meal"))
    return {DecisionType::Action,"share_camp_meal",json::object(),"Je propose un repas commun.","relation","","partager le repas"};
  if(best_goal=="eat"&&hunger<75&&has_action("cook_camp_food"))
    return {DecisionType::Action,"cook_camp_food",json::object(),"Je prépare une ration avant le repas.","food","","préparer un repas"};
  if(best_goal=="eat"&&has_action("eat_camp_food"))
    return {DecisionType::Action,"eat_camp_food",json::object(),"Je prends un repas dans la réserve commune.","food","","be fed"};
  const bool vital_emergency=hunger>=75||thirst>=75;
  if(!vital_emergency&&has_action("treat_condition")){
    const auto care=p.value.value("care_opportunities",json::array());
    if(!care.empty())return {DecisionType::Action,"treat_condition",
        {{"target_id",care.front().value("target_id","")},{"condition_id",care.front().value("condition_id","")}},
        "Je soigne l'affection la plus grave du foyer.","santé","","stabiliser un compagnon"};
  }
  for(const auto& other:visible_agents)if(other.value("adjacent",false)){
    const auto relationship=other.value("relationship",json::object());
    const auto target=other.value("id","");
    if(has_action("reconcile")&&relationship.value("conflict",false)&&personality.value("empathy",50)>=40)
      return {DecisionType::Action,"reconcile",{{"target_id",target}},"Je cherche à réparer notre conflit.","relation","","rétablir la confiance"};
    if(has_action("help_companion")&&other.value("fatigue",0)>=70)
      return {DecisionType::Action,"help_companion",{{"target_id",target}},"J'aide un compagnon épuisé.","entraide","","soutenir le groupe"};
    if(has_action("warn_danger")&&!me.value("observed_animals",json::array()).empty())
      return {DecisionType::Action,"warn_danger",{{"target_id",target}},"Je partage le danger observé.","sécurité","","avertir le groupe"};
    if(has_action("confront")&&anger>=30)
      return {DecisionType::Action,"confront",{{"target_id",target}},"Ma colère déborde en désaccord.","relation","","exprimer le conflit"};
  }
  const auto companion_id=me.value("companion_id","");
  const int companion_until_day=me.value("companion_until_day",0);
  if(!vital_emergency&&!companion_id.empty()&&companion_until_day>=time.value("day",0))for(const auto& other:visible_agents)
    if(other.value("id","")==companion_id&&!other.value("adjacent",false)){
      int dx=other.value("x",self.x)-self.x,dy=other.value("y",self.y)-self.y;
      if(std::abs(dx)>width/2)dx=dx>0?dx-width:dx+width;
      if(std::abs(dy)>height/2)dy=dy>0?dy-height:dy+height;
      const std::string direction=std::abs(dx)>=std::abs(dy)?(dx>0?"east":"west"):(dy>0?"south":"north");
      return {DecisionType::Action,"move",{{"direction",direction}},"Je reste auprès de mon compagnon.","relation","","accompagner"};
    }
  if(!vital_emergency&&has_action("convalesce"))
    return {DecisionType::Action,"convalesce",json::object(),
            "Je consacre cette journée à ma guérison.","santé","","récupérer"};
  const bool carrying_food=!me.value("carried_food",json(nullptr)).is_null();
  const bool carrying_materials=me.value("wood_inventory",0)>0||me.value("branch_inventory",0)>0||
                                me.value("iron_ore_inventory",0)>0;
  if(carrying_food&&has_action("deposit_food"))
    return {DecisionType::Action,"deposit_food",json::object(),
            "Je dépose ma nourriture dans la réserve commune.","réserve","","approvisionner le camp"};
  if(carrying_materials&&has_action("deposit_materials"))
    return {DecisionType::Action,"deposit_materials",json::object(),
            "Je dépose mes matériaux dans la réserve commune.","réserve","","approvisionner le camp"};
  if(has_action("equip_axe"))
    return {DecisionType::Action,"equip_axe",json::object(),"J'équipe la hache disponible.","outil","","travailler le bois"};
  if(has_action("repair_axe"))
    return {DecisionType::Action,"repair_axe",json::object(),"Je répare ma hache avant de repartir.","outil","","entretenir mon outil"};
  const bool has_equipped_tool=!me.value("equipped_tool",json(nullptr)).is_null();
  if(has_action("work_on_building")){
    for(const auto& building:p.value.value("buildings",json::array()))if(!building.value("complete",false)&&building.value("adjacent",false))
      return {DecisionType::Action,"work_on_building",{{"x",building.value("x",0)},{"y",building.value("y",0)}},
              "Je poursuis le chantier désigné.","construction","","achever le bâtiment"};
  }
  if(has_equipped_tool&&has_action("designate_building")){
    const auto designations=p.value.value("building_designations",json::array());
    if(!designations.empty())return {DecisionType::Action,"designate_building",
        {{"building",designations.front().value("building","")},{"x",designations.front().value("x",0)},{"y",designations.front().value("y",0)}},
        "Je réserve les matériaux et désigne le prochain chantier.","construction","","organiser le foyer"};
  }
  if(has_action("craft_camp_item")){
    const auto recipes=p.value.value("craftable_recipes",json::array());
    const auto inventory=p.value.value("camp_inventory",json::object());
    const auto has_recipe=[&](const std::string& key){return std::find(recipes.begin(),recipes.end(),key)!=recipes.end();};
    std::string selected;
    if(inventory.value("axes",0)==0&&!has_equipped_tool){
      if(inventory.value("wooden_handles",0)==0&&has_recipe("wooden_handle"))selected="wooden_handle";
      else if(inventory.value("charcoal",0)==0&&has_recipe("charcoal"))selected="charcoal";
      else if(inventory.value("iron_ingots",0)==0&&has_recipe("iron_ingot"))selected="iron_ingot";
      else if(has_recipe("axe"))selected="axe";
    }
    if(selected.empty()&&inventory.value("ropes",0)==0&&has_recipe("rope"))selected="rope";
    if(!selected.empty())return {DecisionType::Action,"craft_camp_item",{{"recipe",selected}},
        "Je transforme les matériaux selon une recette connue.","fabrication","","produire un objet utile"};
  }
  if(camp_time&&!vital_emergency&&has_action("celebrate"))
    return {DecisionType::Action,"celebrate",json::object(),"Je rassemble le groupe pour célébrer.","relation","","marquer notre réussite"};
  if(camp_time&&!vital_emergency&&has_action("mourn"))
    return {DecisionType::Action,"mourn",json::object(),"Je prends le temps d'honorer notre mort.","relation","","faire mémoire"};
  if(is_night&&!vital_emergency&&has_action("hold_vigil"))
    return {DecisionType::Action,"hold_vigil",json::object(),"Je veille avec un compagnon près du feu.","relation","","partager la veillée"};
  if(camp_time&&!vital_emergency&&at_reserved_rest&&has_action("rest_by_campfire"))
    return {DecisionType::Action,"rest_by_campfire",json::object(),
            "Je passe la nuit près du feu de camp.","repos nocturne","","rester au camp"};
  if(camp_time&&!vital_emergency&&has_action("build_campfire"))
    return {DecisionType::Action,"build_campfire",json::object(),
            "J'allume un feu avant la nuit.","repos nocturne","","établir un camp"};
  if(!vital_emergency&&me.value("branch_inventory",0)<3&&has_action("collect_branch"))
    return {DecisionType::Action,"collect_branch",json::object(),
            "Je ramasse une branche pour préparer un feu.","camp","","réunir du combustible"};
  if(!vital_emergency&&has_action("collect_iron_ore"))
    return {DecisionType::Action,"collect_iron_ore",json::object(),
            "Je ramasse du minerai pour fabriquer un outil.","fabrication","","produire une hache"};
  if(best_goal=="project"&&project.value("key","")=="build_shelter"){
    if(has_action("assemble_shelter"))
      return {DecisionType::Action,"assemble_shelter",json::object(),"J'assemble mon abri.","projet","","construire un abri"};
    if(has_action("build_shelter"))
      return {DecisionType::Action,"build_shelter",json::object(),"Je construis mon abri.","projet","","construire un abri"};
    if(has_action("harvest_wood"))
      return {DecisionType::Action,"harvest_wood",json::object(),"Je prélève du bois pour mon abri.","projet","","construire un abri"};
  }
  if(best_goal=="social"){
    const auto lessons=p.value.value("teachable_lessons",json::array());
    if(has_action("teach_skill")&&!lessons.empty())return {DecisionType::Action,"teach_skill",
        {{"target_id",lessons.front().value("target_id","")},{"skill",lessons.front().value("skill","")}},
        "Je transmets un savoir utile au foyer.","relation","","partager une compétence"};
    for(const auto& other:p.value.value("visible_agents",json::array()))if(other.value("adjacent",false))
      return {DecisionType::Action,"talk",{{"target_agent_id",other.value("id","")},{"message","Partageons ce que nous avons appris."}},"Je renforce notre coopération.","relation","","mieux coopérer"};
  }
  if((best_goal=="eat"||best_goal=="project")&&has_action("hunt_animal")&&p.value.contains("animals")){
    const json* selected=nullptr;
    for(const auto& animal:p.value["animals"]){
      if(!animal.value("adjacent",false))continue;
      if(fear>=30&&animal.value("danger",0)>std::max(10,70-fear))continue;
      if(!selected||animal.value("danger",100)<selected->value("danger",100)||(animal.value("danger",100)==selected->value("danger",100)&&animal.value("id","")<selected->value("id","")))selected=&animal;
    }
    if(selected)return {DecisionType::Action,"hunt_animal",{{"animal_id",selected->value("id","")}},best_goal=="project"?"Je chasse pour constituer une réserve.":"Je chasse la proie la moins risquee.","food","","be fed"};
  }
  if(best_goal=="eat"&&hunger>=75&&has_action("hunt_rabbit")) return {DecisionType::Action,"hunt_rabbit",json::object(),"I need food","food","","be fed"};

  const auto history=p.value.value("action_history",json::array());
  int movement_streak=0;
  if(history.is_array()){
    for(auto it=history.rbegin();it!=history.rend();++it){
      if(it->value("action","")!="move"||it->value("outcome","")!="success")break;
      ++movement_streak;
    }
  }
  if(movement_streak>=12&&has_action("observe"))
    return {DecisionType::Action,"observe",json::object(),
            "Je fais une pause pour observer et réévaluer mon trajet.",
            "orientation","","choisir la suite de mon trajet"};

  std::set<std::pair<int,int>> repeated_positions;
  bool repetition=history.is_array()&&history.size()>=20;
  if(repetition){
    for(auto it=history.end()-20;it!=history.end();++it){
      if(it->value("action","")!="move"||it->value("outcome","")!="success") { repetition=false; break; }
      repeated_positions.insert({it->value("x",0),it->value("y",0)});
    }
    repetition=repetition&&repeated_positions.size()<=4;
  }
  if(repetition&&best_goal=="eat"&&hunger>=70){
    const auto& known=p.value["known_map"];
    Position self{me.value("x",0),me.value("y",0)};
    std::map<std::pair<int,int>,const json*> traversable;
    for(const auto& cell:known) if(cell.value("status","")=="traversable")
      traversable[{cell.value("x",0),cell.value("y",0)}]=&cell;
    std::queue<Position> pending;
    std::map<std::pair<int,int>,std::pair<int,std::string>> reached;
    pending.push(self); reached[{self.x,self.y}]={0,""};
    while(!pending.empty()){
      Position current=pending.front(); pending.pop();
      const auto current_info=reached.at({current.x,current.y});
      for(const auto& direction:directions){
        Position next=step(current,direction);
        if(reached.contains({next.x,next.y})||!traversable.contains({next.x,next.y})) continue;
        reached[{next.x,next.y}]={current_info.first+1,current_info.first==0?direction:current_info.second};
        pending.push(next);
      }
    }
    int best_distance=std::numeric_limits<int>::max();
    std::pair<int,int> best_coordinates{std::numeric_limits<int>::max(),std::numeric_limits<int>::max()};
    std::string food_direction;
    for(const auto&[coordinates,cell]:traversable){
      if((*cell).value("food",0)<=0||!reached.contains(coordinates)) continue;
      const auto& route=reached.at(coordinates);
      if(route.first>0&&(route.first<best_distance||(route.first==best_distance&&coordinates<best_coordinates))){
        best_distance=route.first; best_coordinates=coordinates; food_direction=route.second;
      }
    }
    if(!food_direction.empty()) return {DecisionType::Action,"move",{{"direction",food_direction}},"Repetition detected: seek known food","food","","be fed"};

    std::vector<std::pair<std::string,Position>> unknown;
    for(const auto& direction:directions){
      Position next=step(self,direction);
      bool known_cell=false;
      for(const auto& cell:known) if(cell.value("x",-1)==next.x&&cell.value("y",-1)==next.y){known_cell=true;break;}
      bool accessible=false;
      for(const auto& cell:p.value["cells"]) if(cell.value("x",-1)==next.x&&cell.value("y",-1)==next.y){
        const int terrain=cell.value("terrain",-1);
        accessible=terrain==static_cast<int>(Terrain::Ground)||terrain==static_cast<int>(Terrain::Bush);
        break;
      }
      if(!known_cell&&accessible) unknown.push_back({direction,next});
    }
    const bool has_fresh=std::any_of(unknown.begin(),unknown.end(),[&](const auto& candidate){return !repeated_positions.contains({candidate.second.x,candidate.second.y});});
    for(const auto&[direction,target]:unknown) if(!has_fresh||!repeated_positions.contains({target.x,target.y}))
      return {DecisionType::Action,"move",{{"direction",direction}},"Repetition detected: explore deterministic exit","exploration","","leave movement loop"};
  }
  if(best_goal=="rest"&&fatigue>=75) return {DecisionType::Action,"sleep",json::object(),"I need rest","rest","","be rested"};
  if(best_goal=="rest"&&has_action("rest")) return {DecisionType::Action,"rest",json::object(),"I need rest","rest","","be rested"};

  const auto known=p.value.value("known_map",json::array());
  std::map<std::pair<int,int>,json> known_cells;
  std::set<std::pair<int,int>> traversable;
  std::set<std::pair<int,int>> food_targets;
  std::set<std::pair<int,int>> water_cells;
  std::set<std::pair<int,int>> branch_targets;
  std::set<std::pair<int,int>> iron_targets;
  std::set<std::pair<int,int>> campfire_cells;
  for(const auto& cell:known){
    const std::pair<int,int> coordinates{cell.value("x",0),cell.value("y",0)};
    known_cells[coordinates]=cell;
    if(cell.value("status","")=="traversable")traversable.insert(coordinates);
    if(cell.value("food",0)>0)food_targets.insert(coordinates);
    if(cell.value("terrain",-1)==static_cast<int>(Terrain::Water))water_cells.insert(coordinates);
    if(cell.value("branches",0)>0)branch_targets.insert(coordinates);
    if(cell.value("iron_ore",0)>0)iron_targets.insert(coordinates);
    if(cell.value("campfire",false))campfire_cells.insert(coordinates);
  }
  for(const auto& cell:p.value.value("cells",json::array())){
    const std::pair<int,int> coordinates{cell.value("x",0),cell.value("y",0)};
    const int terrain=cell.value("terrain",-1);
    const auto building=cell.value("building",json(nullptr));
    const bool blocked_wall=building.is_object()&&building.value("complete",false)&&building.value("type","")=="wall";
    if(!blocked_wall&&(terrain==static_cast<int>(Terrain::Ground)||terrain==static_cast<int>(Terrain::Bush)))traversable.insert(coordinates);
    if(cell.value("food",0)>0||terrain==static_cast<int>(Terrain::Bush))food_targets.insert(coordinates);
    if(terrain==static_cast<int>(Terrain::Water))water_cells.insert(coordinates);
    if(cell.value("branches",0)>0)branch_targets.insert(coordinates);
    if(cell.value("iron_ore",0)>0)iron_targets.insert(coordinates);
    if(cell.value("campfire",false))campfire_cells.insert(coordinates);
  }
  const auto home_camp=me.value("home_camp",json(nullptr));
  if(!home_camp.is_null()){
    const std::pair<int,int> home{home_camp.value("x",0),home_camp.value("y",0)};
    if(campfire_cells.contains(home))campfire_cells={home};
  }
  traversable.insert({self.x,self.y});
  auto route_to=[&](const std::set<std::pair<int,int>>& targets){
    std::queue<Position> pending;std::map<std::pair<int,int>,std::string> first_steps;
    pending.push(self);first_steps[{self.x,self.y}]="";
    while(!pending.empty()){
      const auto current=pending.front();pending.pop();
      if(current!=self&&targets.contains({current.x,current.y}))return first_steps[{current.x,current.y}];
      for(const auto& direction:directions){const auto next=step(current,direction);const std::pair<int,int> key{next.x,next.y};if(first_steps.contains(key)||!traversable.contains(key))continue;first_steps[key]=first_steps[{current.x,current.y}].empty()?direction:first_steps[{current.x,current.y}];pending.push(next);}
    }
    return std::string{};
  };
  if(!me.value("equipped_tool",json(nullptr)).is_null()){
    std::set<std::pair<int,int>> work_positions;
    for(const auto& building:p.value.value("buildings",json::array()))if(!building.value("complete",false)){
      Position site{building.value("x",0),building.value("y",0)};
      for(const auto& direction:directions){const auto candidate=step(site,direction);if(traversable.contains({candidate.x,candidate.y}))work_positions.insert({candidate.x,candidate.y});}
    }
    const auto direction=route_to(work_positions);
    if(!direction.empty())return {DecisionType::Action,"move",{{"direction",direction}},
        "Je rejoins le chantier désigné.","construction","","achever le bâtiment"};
  }
  if(carrying_food){
    std::set<std::pair<int,int>> camp_positions;
    for(const auto& coordinates:campfire_cells){
      Position fire{coordinates.first,coordinates.second};
      for(const auto& direction:directions){const auto candidate=step(fire,direction);if(traversable.contains({candidate.x,candidate.y}))camp_positions.insert({candidate.x,candidate.y});}
    }
    const auto camp_direction=route_to(camp_positions);
    if(!camp_direction.empty())return {DecisionType::Action,"move",{{"direction",camp_direction}},
        "Je rapporte ma nourriture au feu de camp.","réserve","","approvisionner le camp"};
  }
  if(carrying_materials){
    std::set<std::pair<int,int>> camp_positions;
    for(const auto& coordinates:campfire_cells){
      Position fire{coordinates.first,coordinates.second};
      for(const auto& direction:directions){const auto candidate=step(fire,direction);if(traversable.contains({candidate.x,candidate.y}))camp_positions.insert({candidate.x,candidate.y});}
    }
    const auto camp_direction=route_to(camp_positions);
    if(!camp_direction.empty())return {DecisionType::Action,"move",{{"direction",camp_direction}},
        "Je rapporte mes matériaux au foyer.","réserve","","approvisionner le camp"};
  }
  if(!carrying_food&&!carrying_materials&&!vital_emergency&&!campfire_cells.empty()){
    const auto camp_inventory=p.value.value("camp_inventory",json::object());
    if(camp_inventory.value("iron_ore",0)<2){
      const auto iron_direction=route_to(iron_targets);
      if(!iron_direction.empty())return {DecisionType::Action,"move",{{"direction",iron_direction}},
          "Je rejoins du minerai connu pour fabriquer une hache.","fabrication","","produire une hache"};
    }
    const auto branch_direction=route_to(branch_targets);
    if(!branch_direction.empty())return {DecisionType::Action,"move",{{"direction",branch_direction}},
        "Je rejoins des branches connues pour le foyer.","réserve","","approvisionner le camp"};
  }
  if(!carrying_food&&!vital_emergency&&!campfire_cells.empty()&&has_action("collect_food"))
    return {DecisionType::Action,"collect_food",json::object(),
            "Je collecte cette nourriture pour la réserve commune.","réserve","","approvisionner le camp"};
  if(camp_time&&!vital_emergency){
    std::set<std::pair<int,int>> camp_positions;
    if(!reserved_rest.is_null()){
      const std::pair<int,int> rest{reserved_rest.value("x",0),reserved_rest.value("y",0)};
      if(traversable.contains(rest))camp_positions.insert(rest);
    }else for(const auto& coordinates:campfire_cells){
      Position fire{coordinates.first,coordinates.second};
      for(const auto& direction:directions){const auto candidate=step(fire,direction);if(traversable.contains({candidate.x,candidate.y}))camp_positions.insert({candidate.x,candidate.y});}
    }
    const auto camp_direction=route_to(camp_positions);
    if(!camp_direction.empty())return {DecisionType::Action,"move",{{"direction",camp_direction}},
        "Je rejoins le feu de camp connu pour la nuit.","repos nocturne","","rester au camp"};
    if(me.value("branch_inventory",0)<3){
      const auto branch_direction=route_to(branch_targets);
      if(!branch_direction.empty())return {DecisionType::Action,"move",{{"direction",branch_direction}},
          "Je rejoins des branches connues pour préparer le camp.","camp","","réunir du combustible"};
    }
  }
  if(best_goal=="hydrate"){
    std::set<std::pair<int,int>> drink_targets;
    for(const auto& coordinates:water_cells){Position water{coordinates.first,coordinates.second};for(const auto& direction:directions){const auto candidate=step(water,direction);if(traversable.contains({candidate.x,candidate.y}))drink_targets.insert({candidate.x,candidate.y});}}
    const auto direction=route_to(drink_targets);
    if(!direction.empty())return {DecisionType::Action,"move",{{"direction",direction}},"Je rejoins une source d'eau connue.","water","","be hydrated"};
  }
  if(best_goal=="eat"){
    const auto direction=route_to(food_targets);
    if(!direction.empty())return {DecisionType::Action,"move",{{"direction",direction}},"I seek food","food","","be fed"};
  }
  if(best_goal=="project"&&project.value("key","")=="secure_food"){
    std::set<std::pair<int,int>> animal_targets;
    for(const auto& animal:p.value.value("animals",json::array()))
      animal_targets.insert({animal.value("x",0),animal.value("y",0)});
    const auto direction=route_to(animal_targets);
    if(!direction.empty())return {DecisionType::Action,"move",{{"direction",direction}},"Je piste une proie pour les réserves.","projet","","sécuriser la nourriture"};
  }

  if(goal_state){
    const auto known_position=[&](Position position){
      return known_cells.contains({position.x,position.y});
    };
    std::vector<Position> frontiers;
    for(const auto& coordinates:traversable){
      Position candidate{coordinates.first,coordinates.second};
      if(candidate==self)continue;
      const bool borders_unknown=std::any_of(directions.begin(),directions.end(),[&](const std::string& direction){
        return !known_position(step(candidate,direction));
      });
      if(borders_unknown)frontiers.push_back(candidate);
    }

    auto route_to_position=[&](Position target){
      return route_to({{target.x,target.y}});
    };
    if(goal_state->exploration_target){
      const auto target=*goal_state->exploration_target;
      if(target==self||!traversable.contains({target.x,target.y})||route_to_position(target).empty())
        goal_state->exploration_target.reset();
    }
    if(!goal_state->exploration_target){
      std::uint32_t best_rank=0;
      bool found=false;
      for(const auto& candidate:frontiers){
        if(route_to_position(candidate).empty())continue;
        std::uint32_t rank=2166136261u;
        for(const unsigned char character:agent_id){rank^=character;rank*=16777619u;}
        rank^=static_cast<std::uint32_t>(candidate.x+1);rank*=16777619u;
        rank^=static_cast<std::uint32_t>(candidate.y+1);rank*=16777619u;
        if(!found||rank>best_rank){best_rank=rank;goal_state->exploration_target=candidate;found=true;}
      }
    }
    if(goal_state->exploration_target){
      const auto target=*goal_state->exploration_target;
      const auto direction=route_to_position(target);
      if(!direction.empty()){
        const std::string destination="("+std::to_string(target.x)+", "+std::to_string(target.y)+")";
        return {DecisionType::Action,"move",{{"direction",direction}},
                "Je suis mon itinéraire d'exploration vers "+destination+".",
                "exploration","","atteindre une zone non cartographiée"};
      }
    }
  }

  int lowest_visits=std::numeric_limits<int>::max();
  std::string selected;
  const int spatial_sense=attributes.value("spatial_sense",50);
  std::string repeated_direction;
  int direction_streak=0;
  if(history.is_array()){
    for(auto it=history.rbegin();it!=history.rend();++it){
      if(it->value("action","")!="move"||it->value("outcome","")!="success")break;
      const auto parameters=it->value("parameters",json::object());
      const auto direction=parameters.value("direction","");
      if(direction.empty())break;
      if(repeated_direction.empty())repeated_direction=direction;
      if(direction!=repeated_direction)break;
      ++direction_streak;
    }
  }
  for(const auto& direction:directions){
    Position adjacent=step(self,direction);
    int visits=0;
    bool excluded=false;
    bool known_direction=false;
    for(const auto& cell:known){
      if(cell.value("x",-1)!=adjacent.x||cell.value("y",-1)!=adjacent.y) continue;
      known_direction=true;
      const std::string status=cell.value("status","unknown");
      excluded=status=="blocked"||status=="out_of_bounds";
      visits=cell.value("visit_count",0);
      break;
    }
    const int straight_line_penalty=direction==repeated_direction?direction_streak*10:0;
    const int weighted_visits=visits*(1+spatial_sense/25)+
        (known_direction?spatial_sense/10:0)+straight_line_penalty;
    if(!excluded&&weighted_visits<lowest_visits){lowest_visits=weighted_visits;selected=direction;}
  }
  if(selected.empty()) return {DecisionType::Action,"wait",json::object(),"No eligible exploration direction","exploration","","discover the world"};
  std::string reason=best_goal=="hydrate"?"J'explore pour trouver de l'eau.":best_goal=="eat"?"J'explore pour trouver de la nourriture.":"J'explore le monde.";
  if(best_goal=="project"&&project.value("key","")=="build_shelter")reason="Je repère des ressources et un lieu pour notre futur abri.";
  if(best_goal=="project"&&project.value("key","")=="secure_food")reason="Je cherche une proie pour constituer une réserve.";
  if(best_goal=="project"&&project.value("key","")=="map_region")reason="Je cartographie méthodiquement cette région.";
  return {DecisionType::Action,"move",{{"direction",selected}},reason,"exploration","","discover the world"};
}

json LocalDecider::checkpoint() const {
  json goals=json::array();
  for(const auto&[agent,state]:goals_){
    json target=nullptr;
    if(state.exploration_target)target={{"x",state.exploration_target->x},{"y",state.exploration_target->y}};
    goals.push_back({{"agent_id",agent},{"name",state.name},{"remaining",state.remaining},
                     {"exploration_target",std::move(target)}});
  }
  return {{"type","local"},{"goals",std::move(goals)}};
}

void LocalDecider::restore_checkpoint(const json& state) {
  goals_.clear();
  for(const auto& goal:state.value("goals",json::array())){
    GoalState restored{goal.at("name").get<std::string>(),goal.at("remaining").get<int>(),std::nullopt};
    const auto target=goal.value("exploration_target",json(nullptr));
    if(target.is_object())restored.exploration_target=Position{target.at("x").get<int>(),target.at("y").get<int>()};
    goals_[goal.at("agent_id").get<std::string>()]=std::move(restored);
  }
}

static bool action_succeeded(const Decision& decision, const std::string& result) {
  if (decision.type == DecisionType::Blocked) return false;
  if (decision.action == "move") return result != "deplacement bloque";
  if (decision.action == "drink") return result == "boit de l'eau";
  if (decision.action == "eat_food") return result.starts_with("mange ");
  if (decision.action == "eat_berries") return result == "mange des baies";
  if (decision.action == "hunt_animal") return result.starts_with("chasse ");
  if (decision.action == "hunt_rabbit") return result == "chasse le lapin";
  if (decision.action == "build_shelter") return result == "construit un abri" || result == "ameliore un abri";
  if (decision.action == "harvest_wood") return result == "preleve du bois";
  if (decision.action == "assemble_shelter") return result == "assemble l'abri" || result == "construit un abri";
  if (decision.action == "collect_branch") return result == "ramasse une branche";
  if (decision.action == "collect_iron_ore") return result == "ramasse du minerai de fer";
  if (decision.action == "build_campfire") return result == "allume un feu de camp";
  if (decision.action == "rest_by_campfire") return result == "reste pres du feu de camp";
  if (decision.action == "collect_food") return result == "ramasse de la nourriture";
  if (decision.action == "deposit_food") return result == "depose de la nourriture au camp";
  if (decision.action == "deposit_materials") return result == "depose des materiaux au camp";
  if (decision.action == "eat_carried_food") return result == "mange sa nourriture transportee";
  if (decision.action == "eat_camp_food") return result == "mange une reserve du camp";
  if (decision.action == "cook_camp_food") return result == "cuit une ration au camp";
  if (decision.action == "craft_camp_item") return result.starts_with("fabrique ");
  if (decision.action == "equip_axe") return result == "equipe une hache";
  if (decision.action == "repair_axe") return result == "repare sa hache";
  if (decision.action == "share_camp_meal") return result.starts_with("partage un repas avec ");
  if (decision.action == "hold_vigil") return result.starts_with("veille avec ");
  if (decision.action == "celebrate") return result == "celebre avec le groupe";
  if (decision.action == "mourn") return result.starts_with("honore la memoire de ");
  if (decision.action == "warn_danger") return result.starts_with("avertit ");
  if (decision.action == "help_companion") return result.starts_with("aide ");
  if (decision.action == "accompany") return result.starts_with("accompagne ");
  if (decision.action == "confront") return result.starts_with("se dispute avec ");
  if (decision.action == "reconcile") return result.starts_with("se réconcilie avec ");
  if (decision.action == "convalesce") return result == "recupere en convalescence";
  if (decision.action == "treat_condition") return result.starts_with("soigne ");
  return true;
}

static Agent initial_agent(std::string id,std::string name,Position position,int hunger,int fatigue,
                           Personality personality,Attributes attributes,int thirst,
                           BehaviorProfile behavior,Project project){
  Agent agent;
  agent.id=std::move(id);agent.name=std::move(name);agent.position=position;
  agent.hunger=hunger;agent.fatigue=fatigue;agent.personality=personality;
  agent.attributes=attributes;agent.thirst=thirst;agent.behavior=std::move(behavior);
  agent.project=std::move(project);
  return agent;
}

Simulation::Simulation(unsigned seed,IDecider& d,Logger& l,ICycleReporter* reporter,
                       std::string checkpoint_path):world_(seed),decider_(d),logger_(l),reporter_(reporter),rng_(seed),devil_(seed,positive_int_from_env("DEVIL_CHANCE_ONE_IN",10)),cycles_per_day_(positive_int_from_env("CYCLES_PER_DAY",2400)),report_every_days_(positive_int_from_env("REPORT_EVERY_DAYS",3)),checkpoint_path_(std::move(checkpoint_path)){
  agents_={initial_agent("a1","Ada",{3,2},45,20,{90,20,30,30,40},{55,65,50,45,50,45,70,60,75,80},25,
                         {"builder","Créer un foyer sûr et organisé",95,45,55,70,{FoodType::Berries,FoodType::Mushrooms}},
                         {"build_shelter","Préparer un abri durable",ProjectStatus::Active,0,0,2,"","",1,0}),
           initial_agent("a2","Borin",{10,5},55,15,{25,85,70,90,40},{75,45,70,75,55,60,50,80,45,50},30,
                         {"provider","Assurer des réserves et protéger le groupe",30,95,45,60,{FoodType::Venison,FoodType::Fish}},
                         {"secure_food","Constituer une réserve de nourriture",ProjectStatus::Active,0,0,1,"","",1,0}),
           initial_agent("a3","Cyra",{16,7},35,60,{40,45,95,55,90},{45,80,60,50,65,55,75,55,70,85},20,
                         {"explorer","Comprendre et cartographier le monde vivant",25,35,100,55,{FoodType::Roots,FoodType::Mushrooms}},
                         {"map_region","Cartographier une région inconnue",ProjectStatus::Active,0,0,180,"","",1,0})};
  load_checkpoint();
}

void Simulation::load_checkpoint() {
  if(checkpoint_path_.empty()||!std::filesystem::exists(checkpoint_path_))return;
  std::ifstream input(checkpoint_path_);
  json state;input>>state;
  if(state.at("version").get<int>()!=1)throw std::runtime_error("unsupported simulation checkpoint version");
  world_.restore_checkpoint(state.at("world"));
  std::vector<Agent> restored_agents;
  for(const auto& agent:state.at("agents"))restored_agents.push_back(restore_agent(agent));
  if(restored_agents.empty())throw std::runtime_error("simulation checkpoint has no agents");
  agents_=std::move(restored_agents);
  day_=state.at("day").get<int>();simulation_cycle_=state.at("simulation_cycle").get<int>();
  date_=date_from_absolute_day(std::max(1,day_));climate_=climate_for(date_);
  action_history_=state.value("action_history",decltype(action_history_){});
  planning_history_=state.value("planning_history",decltype(planning_history_){});
  next_agent_id_=state.value("next_agent_id",static_cast<int>(agents_.size())+1);
  restore_rng(rng_,state.at("rng").get<std::string>());
  devil_.restore_rng_checkpoint(state.at("devil_rng").get<std::string>());
  decider_.restore_checkpoint(state.value("decider",json::object()));
  restored_checkpoint_=true;
  logger_.message("Checkpoint restauré : jour "+std::to_string(day_)+
                  ", cycle élémentaire "+std::to_string(simulation_cycle_)+".");
}

void Simulation::save_checkpoint() const {
  if(checkpoint_path_.empty())return;
  json agents=json::array();for(const auto& agent:agents_)agents.push_back(agent_checkpoint(agent));
  const json state={{"version",1},{"day",day_},{"simulation_cycle",simulation_cycle_},
      {"world",world_.checkpoint()},{"agents",std::move(agents)},
      {"action_history",action_history_},{"planning_history",planning_history_},
      {"rng",rng_checkpoint(rng_)},{"devil_rng",devil_.rng_checkpoint()},
      {"decider",decider_.checkpoint()},{"next_agent_id",next_agent_id_}};
  const std::filesystem::path target(checkpoint_path_);
  if(!target.parent_path().empty())std::filesystem::create_directories(target.parent_path());
  const auto temporary=target.string()+".tmp";
  {
    std::ofstream output(temporary,std::ios::trunc);
    if(!output)throw std::runtime_error("cannot write simulation checkpoint");
    output<<state.dump(2)<<'\n';
    output.flush();
    if(!output)throw std::runtime_error("cannot flush simulation checkpoint");
  }
  std::filesystem::rename(temporary,target);
}

Perception Simulation::perceive(Agent& a) {
  json cells=json::array();
  std::set<std::pair<int,int>> perceived;
  for(int dy=-3;dy<=3;++dy) for(int dx=-3;dx<=3;++dx)if(std::abs(dx)+std::abs(dy)<=3){Position p=world_.wrap({a.position.x+dx,a.position.y+dy});if(!perceived.insert({p.x,p.y}).second)continue;auto terrain=world_.terrain(p);a.remember_map(p,terrain);if(world_.campfire(p))a.known_campfires.insert({p.x,p.y});json animals=json::array();for(const auto& animal:world_.animals())if(animal.alive&&animal.position==p){const auto type=animal_type_name(animal.type);a.observed_animals.insert(type);animals.push_back({{"id",animal.id},{"type",type},{"danger",animal.danger},{"nutrition",animal.nutrition}});}json building=nullptr;if(const auto structure=world_.building(p))building={{"type",building_type_name(structure->type)},{"progress",structure->progress},{"required_work",structure->required_work},{"complete",structure->complete}};cells.push_back({{"x",p.x},{"y",p.y},{"terrain",static_cast<int>(terrain)},{"food",world_.food(p)},{"branches",world_.branches(p)},{"iron_ore",world_.iron_ore(p)},{"campfire",world_.campfire(p)},{"building",building},{"stored_food",world_.stored_food(p)},{"stored_wood",world_.stored_wood(p)},{"stored_branches",world_.stored_branches(p)},{"stored_iron_ore",world_.stored_iron_ore(p)},{"crafted_items",world_.stored_crafted_items(p)},{"water",terrain==Terrain::Water},{"rabbit",world_.rabbit_alive()&&p==world_.rabbit()},{"animals",animals}});}
  if(const auto primary=world_.primary_campfire()){
    a.known_campfires.insert({primary->x,primary->y});
    if(a.home_camp!=primary){a.home_camp=primary;a.camp_rest_position.reset();}
  }
  if(!a.home_camp&&!a.known_campfires.empty()){
    std::vector<Position> candidates;for(const auto& coordinates:a.known_campfires)candidates.push_back({coordinates.first,coordinates.second});
    std::sort(candidates.begin(),candidates.end(),[&](Position left,Position right){const int left_distance=world_.toroidal_distance(a.position,left),right_distance=world_.toroidal_distance(a.position,right);return left_distance!=right_distance?left_distance<right_distance:std::pair{left.x,left.y}<std::pair{right.x,right.y};});
    a.home_camp=candidates.front();
  }
  if(a.home_camp&&!a.camp_rest_position){
    std::set<std::pair<int,int>> reserved;
    for(const auto& other:agents_)if(other.id!=a.id&&other.home_camp==a.home_camp&&other.camp_rest_position)
      reserved.insert({other.camp_rest_position->x,other.camp_rest_position->y});
    for(const auto candidate:world_.neighbors(*a.home_camp))if(world_.passable(candidate)&&!reserved.contains({candidate.x,candidate.y})){
      a.camp_rest_position=candidate;break;
    }
  }
  json known=json::array();for(const auto&[position,terrain]:a.map_memory){Position p{position.first,position.second};bool traversable=world_.passable(p);const bool known_fire=a.known_campfires.contains(position);json building=nullptr;if(const auto structure=world_.building(p))building={{"type",building_type_name(structure->type)},{"complete",structure->complete}};known.push_back({{"x",position.first},{"y",position.second},{"terrain",static_cast<int>(terrain)},{"status",traversable?"traversable":"blocked"},{"visit_count",a.map_visit_counts[position]},{"food",world_.food(p)},{"branches",world_.branches(p)},{"iron_ore",world_.iron_ore(p)},{"campfire",known_fire},{"building",building},{"stored_food",known_fire?world_.stored_food(p):0},{"stored_wood",known_fire?world_.stored_wood(p):0},{"stored_branches",known_fire?world_.stored_branches(p):0},{"stored_iron_ore",known_fire?world_.stored_iron_ore(p):0},{"crafted_items",known_fire?world_.stored_crafted_items(p):0}});}
  json visible=json::array();for(const auto&o:agents_)if(o.alive&&o.id!=a.id&&world_.toroidal_distance(o.position,a.position)<=3)visible.push_back({{"id",o.id},{"name",o.name},{"x",o.position.x},{"y",o.position.y},{"fatigue",o.fatigue},{"adjacent",world_.adjacent(a.position,o.position)},{"skills",skills_json(o)},{"conditions",health_conditions_json(o)},{"relationship",relationships_json(a.relationships).value(o.id,json::object())}});
  json animals=json::array();for(const auto& animal:world_.animals())if(animal.alive&&world_.toroidal_distance(animal.position,a.position)<=3)animals.push_back({{"id",animal.id},{"type",animal_type_name(animal.type)},{"x",animal.position.x},{"y",animal.position.y},{"danger",animal.danger},{"nutrition",animal.nutrition},{"adjacent",world_.adjacent(a.position,animal.position)}});
  json mem=json::array();for(const auto&s:a.memories)mem.push_back(s);
  const auto phase=day_phase_for(cycle_in_day_,cycles_per_day_);
  if(a.home_camp&&a.community_role.empty()){
    a.community_role=collective_role(a.behavior);
    a.remember("J'assume le rôle de "+a.community_role+" au foyer.");
  }
  const auto actions=available_actions(a,world_,agents_,day_,phase);
  const bool shelter_action_available=std::any_of(actions.begin(),actions.end(),[](const std::string& action){
    return action=="harvest_wood"||action=="assemble_shelter"||action=="build_shelter";
  });
  if(a.project.status==ProjectStatus::Blocked&&a.project.key=="build_shelter"&&
     a.project.missing_capability=="build_shelter"&&shelter_action_available){
    a.project.status=ProjectStatus::Active;
    a.project.blocked_reason.clear();
    a.project.missing_capability.clear();
    a.remember("La capacite de construction de l'abri est disponible.");
  }
  json construction=nullptr;if(a.shelter_construction)construction={{"x",a.shelter_construction->position.x},{"y",a.shelter_construction->position.y},{"progress",a.shelter_construction->progress}};
  json carried=nullptr;if(a.carried_food)carried={{"type",food_type_name(a.carried_food->type)},{"nutrition",a.carried_food->nutrition},{"cooked",a.carried_food->cooked},{"age_days",a.carried_food->age_days},{"shelf_life_days",a.carried_food->shelf_life_days}};
  json tool=nullptr;if(a.equipped_tool)tool={{"type",craft_item_name(a.equipped_tool->type)},
      {"durability",a.equipped_tool->durability},
      {"maximum_durability",a.equipped_tool->maximum_durability}};
  json home=nullptr;if(a.home_camp)home={{"x",a.home_camp->x},{"y",a.home_camp->y}};
  json rest_position=nullptr;if(a.camp_rest_position)rest_position={{"x",a.camp_rest_position->x},{"y",a.camp_rest_position->y}};
  json craftable=json::array();if(const auto fire=world_.nearby_campfire(a.position))for(const auto& recipe:world_.craftable_recipes(*fire))craftable.push_back(recipe);
  json camp_inventory=json::object();if(const auto fire=world_.nearby_campfire(a.position))camp_inventory={{"wood",world_.stored_wood(*fire)},{"branches",world_.stored_branches(*fire)},{"iron_ore",world_.stored_iron_ore(*fire)},{"wooden_handles",world_.stored_item(*fire,CraftItem::WoodenHandle)},{"charcoal",world_.stored_item(*fire,CraftItem::Charcoal)},{"ropes",world_.stored_item(*fire,CraftItem::Rope)},{"iron_ingots",world_.stored_item(*fire,CraftItem::IronIngot)},{"axes",world_.stored_item(*fire,CraftItem::Axe)}};
  json lessons=json::array();if(const auto fire=world_.nearby_campfire(a.position))for(const auto& learner:agents_)if(learner.alive&&learner.id!=a.id&&world_.nearby_campfire(learner.position)==fire)if(const auto skill=teachable_skill(a,learner))lessons.push_back({{"target_id",learner.id},{"target_name",learner.name},{"skill",skill_name(*skill)}});
  json care=json::array();if(const auto fire=world_.nearby_campfire(a.position))for(const auto& patient:agents_)if(patient.alive&&patient.id!=a.id&&world_.nearby_campfire(patient.position)==fire){const HealthCondition* selected=nullptr;for(const auto& condition:patient.conditions)if(!condition.treated&&(!selected||condition.severity>selected->severity))selected=&condition;if(selected)care.push_back({{"target_id",patient.id},{"target_name",patient.name},{"condition_id",selected->id},{"type",health_condition_name(selected->type)},{"severity",selected->severity}});}
  json buildings=json::array();for(const auto&[site,building]:world_.buildings())if(a.map_memory.contains({site.x,site.y})||world_.toroidal_distance(a.position,site)<=3)buildings.push_back({{"x",site.x},{"y",site.y},{"type",building_type_name(building.type)},{"progress",building.progress},{"required_work",building.required_work},{"complete",building.complete},{"adjacent",world_.adjacent(a.position,site)}});
  json designations=json::array();if(const auto fire=world_.nearby_campfire(a.position)){
    const std::array<std::pair<BuildingType,Position>,5> preferred{{
      {BuildingType::Wall,{-2,0}},{BuildingType::Door,{-1,-1}},{BuildingType::Bed,{0,-2}},
      {BuildingType::Stockpile,{1,-1}},{BuildingType::Workshop,{2,0}}}};
    for(const auto&[type,offset]:preferred)if(!world_.has_completed_building(type)){
      Position site=world_.wrap({fire->x+offset.x,fire->y+offset.y});
      if(world_.can_designate_building(site,*fire,type)){designations.push_back({{"building",building_type_name(type)},{"x",site.x},{"y",site.y}});break;}
    }
  }
  const auto ecology=world_.ecology();
  return Perception{json{{"world_width",World::width},{"world_height",World::height},{"calendar",calendar_json(date_)},{"climate",climate_json(climate_)},{"ecology",{{"day",ecology.day},{"births_today",ecology.births},{"predations_today",ecology.predations},{"regrown_food_today",ecology.regrown_food},{"depleted_patches",ecology.depleted_patches},{"total_births",ecology.total_births},{"total_predations",ecology.total_predations}}},{"time",{{"day",day_},{"phase",day_phase_name(phase)},{"cycle_in_day",cycle_in_day_},{"cycles_per_day",cycles_per_day_},{"cycles_until_night",std::max(0,daylight_cycles(cycles_per_day_)-cycle_in_day_+1)}}},{"self",{{"id",a.id},{"name",a.name},{"x",a.position.x},{"y",a.position.y},{"health",a.health},{"hunger",a.hunger},{"thirst",a.thirst},{"fatigue",a.fatigue},{"boredom",a.boredom},{"wood_inventory",a.wood_inventory},{"branch_inventory",a.branch_inventory},{"iron_ore_inventory",a.iron_ore_inventory},{"carried_food",carried},{"equipped_tool",tool},{"inventory_load",inventory_load(a)},{"inventory_capacity",inventory_capacity(a)},{"home_camp",home},{"camp_rest_position",rest_position},{"shelter_construction",construction},{"community_role",a.community_role},{"skills",skills_json(a)},{"conditions",health_conditions_json(a)},{"emotions",emotions_json(a)},{"companion_id",a.companion_id},{"companion_until_day",a.companion_until_day},{"personality",personality_json(a.personality)},{"attributes",attributes_json(a.attributes)},{"behavior",behavior_json(a.behavior)},{"project",project_json(a.project)},{"relationships",relationships_json(a.relationships)},{"observed_animals",a.observed_animals}}},{"cells",cells},{"known_map",known},{"action_history",planning_history_[a.id]},{"visible_agents",visible},{"teachable_lessons",lessons},{"care_opportunities",care},{"buildings",buildings},{"building_designations",designations},{"animals",animals},{"memories",mem},{"available_actions",actions},{"craftable_recipes",craftable},{"camp_inventory",camp_inventory}}};
}

void Simulation::advance_action_needs(Agent& a,int action_index){if(action_index%80==0)a.hunger=clamp_stat(a.hunger+1);const int fatigue_interval=12+std::max(0,a.attributes.endurance-50)/10;if(action_index%fatigue_interval==0)a.fatigue=clamp_stat(a.fatigue+1);const int thirst_interval=60+std::max(0,a.attributes.endurance-40);if(action_index%thirst_interval==0)a.thirst=clamp_stat(a.thirst+1);}
void Simulation::update_needs(Agent& a){if(a.hunger>=90){++a.critical_hunger_days;if(a.critical_hunger_days>3)a.health=clamp_stat(a.health-std::max(1,7-a.attributes.willpower/20));}else a.critical_hunger_days=0;if(a.thirst>=90){++a.critical_thirst_days;if(a.critical_thirst_days>1)a.health=clamp_stat(a.health-std::max(2,10-a.attributes.willpower/15));}else a.critical_thirst_days=0;if(a.health==0){a.alive=false;if(a.death_cause.empty())a.death_cause="besoins vitaux non satisfaits";}}

void Simulation::update_health_conditions(Agent& agent) {
  if(!agent.alive)return;
  bool infection_risk=false;
  for(auto& condition:agent.conditions){
    ++condition.days;
    if(condition.treated)condition.severity=std::max(0,condition.severity-3);
    else if(condition.type==HealthConditionType::Injury)condition.severity=std::min(100,condition.severity+2);
    else if(condition.type==HealthConditionType::Disease)condition.severity=std::min(100,condition.severity+1);
    else condition.severity=std::min(100,condition.severity+3);
    if(condition.type==HealthConditionType::Injury&&!condition.treated&&condition.days>=3&&condition.severity>=20)
      infection_risk=true;
    if(condition.severity>0)agent.health=clamp_stat(agent.health-std::max(1,condition.severity/25));
  }
  const bool infected=std::any_of(agent.conditions.begin(),agent.conditions.end(),[](const HealthCondition& condition){
    return condition.type==HealthConditionType::Infection;
  });
  if(infection_risk&&!infected){add_health_condition(agent,HealthConditionType::Infection,20,"blessure non soignée");agent.remember("Ma blessure s'est infectée.");}
  std::erase_if(agent.conditions,[](const HealthCondition& condition){return condition.severity<=0;});
  if(agent.health==0){agent.alive=false;if(agent.death_cause.empty())agent.death_cause="complications de santé";}
}

void Simulation::update_emotions(Agent& agent) {
  if(!agent.alive)return;
  const int decay=std::clamp(6+agent.attributes.willpower/20,6,11);
  for(auto& emotion:agent.emotions){emotion.intensity=std::max(0,emotion.intensity-decay);--emotion.remaining_days;}
  std::erase_if(agent.emotions,[](const Emotion& emotion){return emotion.intensity<=0||emotion.remaining_days<=0;});
}

void Simulation::update_population() {
  for(auto& agent:agents_)if(agent.alive){
    ++agent.age_days;
    if(agent.age_days>=80*360){agent.alive=false;agent.death_cause="vieillesse";agent.remember("Ma vie s'achève après un grand âge.");logger_.message(agent.name+" meurt de vieillesse.");}
  }
  if(day_%30!=0)return;
  for(auto& agent:agents_)if(agent.alive){
    int belonging=0;for(const auto&[_,relationship]:agent.relationships)belonging+=relationship.trust+relationship.affinity;
    if((agent.critical_hunger_days>=4||agent.critical_thirst_days>=3)&&belonging<10){
      agent.alive=false;agent.departure_day=day_;agent.departure_reason="détresse vitale sans soutien";
      agent.remember("J'ai quitté le foyer faute de ressources et de soutien.");logger_.message(agent.name+" quitte le foyer.");
    }
  }
  const auto camp=world_.primary_campfire();
  if(!camp)return;
  const auto sheltered=[&]{if(world_.shelter_level(*camp)>0)return true;for(const auto neighbor:world_.neighbors(*camp))if(world_.shelter_level(neighbor)>0)return true;return world_.has_completed_building(BuildingType::Bed);}();
  int residents=static_cast<int>(std::count_if(agents_.begin(),agents_.end(),[](const Agent& agent){return agent.alive;}));
  auto consume_reserves=[&](int amount){for(int index=0;index<amount;++index)if(!world_.take_stored_food(*camp))return false;return true;};
  auto make_resident=[&](std::string origin,int age,std::vector<std::string> parents){
    const int serial=next_agent_id_++;Agent newcomer;
    newcomer.id="a"+std::to_string(serial);newcomer.name=origin=="birth"?"Enfant "+std::to_string(serial):"Voyageur "+std::to_string(serial);
    newcomer.position=world_.neighbors(*camp).front();newcomer.age_days=age;newcomer.origin=std::move(origin);
    newcomer.arrival_day=day_;newcomer.parent_ids=std::move(parents);newcomer.hunger=25;newcomer.thirst=20;newcomer.fatigue=20;
    newcomer.personality={50,55,55,55,60};newcomer.attributes={45,50,50,45,55,55,50,55,50,50};
    newcomer.behavior={"resident","Contribuer à la continuité du foyer",50,60,45,65,{FoodType::Roots,FoodType::Berries}};
    newcomer.project={"join_camp","Trouver sa place dans le foyer",ProjectStatus::Active,0,0,3,"","",day_,simulation_cycle_};
    newcomer.home_camp=*camp;newcomer.known_campfires.insert({camp->x,camp->y});agents_.push_back(std::move(newcomer));
  };
  if(sheltered&&day_%60==0&&residents<8&&world_.stored_food(*camp)>=residents*4+4&&consume_reserves(4)){
    make_resident("arrival",20*360+((day_/60)%20)*360,{});++residents;logger_.message("Un nouveau voyageur rejoint le foyer.");
  }
  std::vector<std::string> adults;for(const auto& agent:agents_)if(agent.alive&&is_adult(agent))adults.push_back(agent.id);
  if(sheltered&&day_%90==0&&residents<10&&adults.size()>=2&&world_.stored_food(*camp)>=residents*6+6&&consume_reserves(6)){
    make_resident("birth",0,{adults[0],adults[1]});logger_.message("Une naissance agrandit le foyer.");
  }
}

void Simulation::apply_climate_effects(Agent& agent,const CalendarDate& date,const ClimateState& climate) {
  if(!agent.alive)return;
  const bool sheltered=world_.shelter_level(agent.position)>0;
  if(date.season==Season::Summer&&climate.temperature_c>=29)agent.thirst=clamp_stat(agent.thirst+3);
  if(date.season==Season::Winter&&climate.temperature_c<=2){
    agent.hunger=clamp_stat(agent.hunger+2);
    agent.fatigue=clamp_stat(agent.fatigue+(sheltered?1:4));
  }
  if(climate.rainfall_mm>=8&&!sheltered)agent.fatigue=clamp_stat(agent.fatigue+1);
}

std::string Simulation::execute(Agent&a,const Decision&d){
  if(d.type==DecisionType::Blocked){a.remember("I reported a blockage: "+d.obstacle);return "signale un blocage : "+d.obstacle;}
  if(d.action=="wait"||d.action=="observe"){
    a.remember(d.action=="wait"?"J'ai attendu.":"J'ai observe mon environnement.");
    return d.action=="observe"?"observe son environnement":"attend";
  }
  if(d.action=="rest"){if(!a.alive||a.fatigue<=0||!world_.passable(a.position))return "attend";a.fatigue=clamp_stat(a.fatigue-(15+a.attributes.recuperation/10+world_.rest_bonus(a.position)));return "rested";}
  if(d.action=="sleep"){a.sleeping_days=2;a.fatigue=clamp_stat(a.fatigue-(15+a.attributes.recuperation/10));a.remember("Je commence a dormir.");return "commence a dormir";}
  if(d.action=="drink"){if(!world_.drinkable(a.position))return "ne trouve pas d'eau";a.thirst=clamp_stat(a.thirst-(40+a.attributes.recuperation/10));return "boit de l'eau";}
  if(d.action=="eat_food"){FoodType type{};int nutrition=0;if(world_.consume_food(a.position,&type,&nutrition)){a.hunger=clamp_stat(a.hunger-nutrition);if(type==FoodType::Mushrooms){const int impact=std::max(0,5-a.attributes.disease_resistance/20);a.health=clamp_stat(a.health-impact);if(impact>0&&std::none_of(a.conditions.begin(),a.conditions.end(),[](const HealthCondition& condition){return condition.type==HealthConditionType::Disease;}))add_health_condition(a,HealthConditionType::Disease,10+impact*2,"champignons crus");}return "mange "+food_type_name(type);}return "ne peut pas manger ici";}
  if(d.action=="eat_berries"){if(world_.eat_berries(a.position)){a.hunger=clamp_stat(a.hunger-35);return "mange des baies";}return "ne peut pas manger ici";}
  if(d.action=="hunt_rabbit"){if(world_.hunt_rabbit(a.position)){a.hunger=clamp_stat(a.hunger-35);add_skill_experience(a,Skill::Hunting);a.remember("J'ai chasse le lapin.");return "chasse le lapin";}return "ne peut pas chasser ici";}
  if(d.action=="hunt_animal"){Animal hunted;if(world_.hunt_animal(a.position,d.parameters.value("animal_id",""),&hunted)){const int injury=std::max(0,hunted.danger-(a.attributes.strength+a.attributes.toughness)/2)/5;a.health=clamp_stat(a.health-injury);a.hunger=clamp_stat(a.hunger-hunted.nutrition);if(injury>0){add_health_condition(a,HealthConditionType::Injury,std::max(5,injury*3),"chasse "+animal_type_name(hunted.type));add_emotion(a,EmotionType::Fear,std::min(80,25+injury*2),"blessure pendant la chasse au "+animal_type_name(hunted.type),"éviter les chasses dangereuses",4,day_);}add_skill_experience(a,Skill::Hunting);a.remember("J'ai chasse "+animal_type_name(hunted.type)+".");return "chasse "+animal_type_name(hunted.type);}return "ne peut pas chasser ici";}
  if(d.action=="build_shelter"){const bool existing=world_.shelter_level(a.position)>0;if(!a.alive||!world_.build_shelter(a.position))return "materiaux insuffisants pour construire";add_skill_experience(a,Skill::Building);a.remember(existing?"J'ai ameliore l'abri.":"J'ai construit un abri.");return existing?"ameliore un abri":"construit un abri";}
  if(d.action=="harvest_wood"){if(!a.equipped_tool||a.equipped_tool->type!=CraftItem::Axe||a.equipped_tool->durability<=0)return "hache requise";if(inventory_full(a))return "inventaire plein";if(!a.alive||!world_.harvest_tree(a.position))return "aucun arbre vivant a prelever";--a.equipped_tool->durability;++a.wood_inventory;add_skill_experience(a,Skill::Woodcutting);a.remember("J'ai abattu un arbre avec ma hache.");return "preleve du bois";}
  if(d.action=="collect_branch"){if(inventory_full(a))return "inventaire plein";if(!a.alive||!world_.take_branch(a.position))return "aucune branche a ramasser";++a.branch_inventory;add_skill_experience(a,Skill::Foraging);a.remember("J'ai ramasse une branche pour le camp.");return "ramasse une branche";}
  if(d.action=="collect_iron_ore"){if(inventory_full(a))return "inventaire plein";if(!a.alive||!world_.take_iron_ore(a.position))return "aucun minerai de fer";++a.iron_ore_inventory;add_skill_experience(a,Skill::Mining);a.remember("J'ai ramassé du minerai de fer.");return "ramasse du minerai de fer";}
  if(d.action=="build_campfire"){if(!a.alive||a.branch_inventory<3||!world_.place_campfire(a.position))return "ne peut pas allumer de feu";a.branch_inventory-=3;a.known_campfires.insert({a.position.x,a.position.y});a.remember("J'ai allume un feu de camp.");return "allume un feu de camp";}
  if(d.action=="rest_by_campfire"){if(!a.alive||!world_.adjacent_campfire(a.position))return "aucun feu a proximite";a.fatigue=clamp_stat(a.fatigue-1);return "reste pres du feu de camp";}
  if(d.action=="collect_food"){if(inventory_full(a))return "inventaire plein";FoodItem food;if(!a.alive||a.carried_food||!world_.consume_food(a.position,&food.type,&food.nutrition))return "aucune nourriture a ramasser";food.shelf_life_days=food_shelf_life(food.type);a.carried_food=food;add_skill_experience(a,Skill::Foraging);a.remember("J'ai pris une nourriture pour la réserve commune.");return "ramasse de la nourriture";}
  if(d.action=="deposit_food"){const auto fire=world_.nearby_campfire(a.position);if(!a.alive||!a.carried_food||!fire||!world_.store_food(*fire,*a.carried_food))return "ne peut pas deposer de nourriture";a.carried_food.reset();a.remember("J'ai approvisionne la réserve commune.");return "depose de la nourriture au camp";}
  if(d.action=="deposit_materials"){const auto fire=world_.nearby_campfire(a.position);if(!a.alive||!fire||!world_.store_materials(*fire,a.wood_inventory,a.branch_inventory,a.iron_ore_inventory))return "ne peut pas deposer de materiaux";a.wood_inventory=0;a.branch_inventory=0;a.iron_ore_inventory=0;a.remember("J'ai rapporté des matériaux au foyer.");return "depose des materiaux au camp";}
  if(d.action=="eat_carried_food"){if(!a.alive||!a.carried_food)return "aucune nourriture transportee";const auto food=*a.carried_food;a.carried_food.reset();a.hunger=clamp_stat(a.hunger-food.nutrition);if(food.type==FoodType::Mushrooms&&!food.cooked){const int impact=std::max(0,5-a.attributes.disease_resistance/20);a.health=clamp_stat(a.health-impact);if(impact>0&&std::none_of(a.conditions.begin(),a.conditions.end(),[](const HealthCondition& condition){return condition.type==HealthConditionType::Disease;}))add_health_condition(a,HealthConditionType::Disease,10+impact*2,"champignons crus");}return "mange sa nourriture transportee";}
  if(d.action=="eat_camp_food"){const auto fire=world_.nearby_campfire(a.position);FoodItem food;if(!a.alive||!fire||!world_.take_stored_food(*fire,&food,a.behavior.preferred_foods))return "aucune reserve au camp";a.hunger=clamp_stat(a.hunger-food.nutrition);if(food.type==FoodType::Mushrooms&&!food.cooked){const int impact=std::max(0,5-a.attributes.disease_resistance/20);a.health=clamp_stat(a.health-impact);if(impact>0&&std::none_of(a.conditions.begin(),a.conditions.end(),[](const HealthCondition& condition){return condition.type==HealthConditionType::Disease;}))add_health_condition(a,HealthConditionType::Disease,10+impact*2,"champignons crus");}a.remember("J'ai mangé une ration "+food_type_name(food.type)+(food.cooked?" cuite.":"."));return "mange une reserve du camp";}
  if(d.action=="cook_camp_food"){const auto fire=world_.nearby_campfire(a.position);if(!a.alive||!fire||!world_.cook_stored_food(*fire))return "aucune ration crue au camp";add_skill_experience(a,Skill::Cooking);a.remember("J'ai cuit une ration pour le groupe.");return "cuit une ration au camp";}
  if(d.action=="craft_camp_item"){const auto fire=world_.nearby_campfire(a.position);const auto recipe=d.parameters.value("recipe","");if(!a.alive||!fire||!world_.craft(*fire,recipe))return "recette indisponible";add_skill_experience(a,Skill::Crafting);a.remember("J'ai fabriqué "+recipe+" au foyer.");return "fabrique "+recipe;}
  if(d.action=="equip_axe"){const auto fire=world_.nearby_campfire(a.position);if(!a.alive||a.equipped_tool||!fire||!world_.take_stored_item(*fire,CraftItem::Axe))return "aucune hache disponible";a.equipped_tool=Tool{};a.remember("J'ai équipé une hache.");return "equipe une hache";}
  if(d.action=="repair_axe"){const auto fire=world_.nearby_campfire(a.position);if(!a.alive||!a.equipped_tool||a.equipped_tool->type!=CraftItem::Axe||a.equipped_tool->durability>=a.equipped_tool->maximum_durability||!fire||!world_.consume_stored_wood(*fire,1))return "reparation impossible";a.equipped_tool->durability=std::min(a.equipped_tool->maximum_durability,a.equipped_tool->durability+10+skill_level(a,Skill::Woodcutting)+world_.workshop_bonus(*fire));add_skill_experience(a,Skill::Crafting);a.remember("J'ai réparé ma hache avec du bois du foyer.");return "repare sa hache";}
  if(d.action=="designate_building"){
    const auto fire=world_.nearby_campfire(a.position);
    const auto type=building_type_from_name(d.parameters.value("building",""));
    const Position site{d.parameters.value("x",-1),d.parameters.value("y",-1)};
    if(!a.alive||!fire||!type||!world_.designate_building(site,*fire,*type))return "designation impossible";
    a.remember("J'ai désigné un chantier "+building_type_name(*type)+".");
    return "designe "+building_type_name(*type);
  }
  if(d.action=="work_on_building"){
    const Position site{d.parameters.value("x",-1),d.parameters.value("y",-1)};
    const auto before=world_.building(site);
    if(!a.alive||!a.equipped_tool||a.equipped_tool->type!=CraftItem::Axe||a.equipped_tool->durability<=0||
       !before||before->complete||!world_.adjacent(a.position,site)||
       !world_.work_on_building(site,1+skill_level(a,Skill::Building)/3))return "chantier indisponible";
    --a.equipped_tool->durability;add_skill_experience(a,Skill::Building);
    const auto after=world_.building(site);
    a.remember(after->complete?"J'ai achevé le chantier "+building_type_name(after->type)+".":
                               "J'ai avancé le chantier "+building_type_name(after->type)+".");
    return "travaille sur "+building_type_name(after->type)+(after->complete?" et termine":"");
  }
  if(d.action=="convalesce"){
    if(!a.alive||a.conditions.empty()||a.last_convalescence_day==day_||
       (world_.shelter_level(a.position)==0&&world_.rest_bonus(a.position)==0&&!world_.adjacent_campfire(a.position)))return "convalescence indisponible";
    const int recovery=6+a.attributes.recuperation/10+world_.rest_bonus(a.position);
    for(auto& condition:a.conditions)condition.severity=std::max(0,condition.severity-recovery);
    std::erase_if(a.conditions,[](const HealthCondition& condition){return condition.severity<=0;});
    a.fatigue=clamp_stat(a.fatigue-10);a.last_convalescence_day=day_;
    a.remember("Je me suis reposé pour récupérer de mes affections.");
    return "recupere en convalescence";
  }
  if(d.action=="treat_condition"){
    const auto fire=world_.nearby_campfire(a.position);Agent* patient=nullptr;HealthCondition* condition=nullptr;
    if(fire)for(auto& candidate:agents_)if(candidate.alive&&candidate.id!=a.id&&candidate.id==d.parameters.value("target_id","")&&
       world_.nearby_campfire(candidate.position)==fire)for(auto& candidate_condition:candidate.conditions)if(
         candidate_condition.id==d.parameters.value("condition_id","")&&!candidate_condition.treated){patient=&candidate;condition=&candidate_condition;}
    if(!a.alive||!patient||!condition)return "soin indisponible";
    condition->treated=true;condition->severity=std::max(1,condition->severity-(8+a.personality.empathy/10));
    auto& relation=a.relationships[patient->id];++relation.interactions;relation.trust=clamp_stat(relation.trust+4);
    auto& reciprocal=patient->relationships[a.id];++reciprocal.interactions;reciprocal.trust=clamp_stat(reciprocal.trust+3);
    add_skill_experience(a,Skill::Social);a.remember("J'ai soigné "+patient->name+".");patient->remember(a.name+" a soigné mon affection.");
    return "soigne "+patient->name;
  }
  if(d.action=="teach_skill"){
    const auto fire=world_.nearby_campfire(a.position);
    const auto skill=skill_from_name(d.parameters.value("skill",""));
    Agent* learner=nullptr;
    if(skill&&fire&&a.last_taught_day!=day_)for(auto& candidate:agents_)if(
        candidate.id==d.parameters.value("target_id","")&&candidate.alive&&
        candidate.last_lesson_day!=day_&&world_.nearby_campfire(candidate.position)==fire&&
        skill_level(a,*skill)>=skill_level(candidate,*skill)+2)learner=&candidate;
    if(!learner)return "lecon indisponible";
    add_skill_experience(*learner,*skill);add_skill_experience(a,Skill::Social);
    a.last_taught_day=day_;learner->last_lesson_day=day_;
    a.remember("J'ai transmis "+skill_name(*skill)+" à "+learner->name+".");
    learner->remember(a.name+" m'a enseigné "+skill_name(*skill)+".");
    auto& relation=a.relationships[learner->id];++relation.interactions;
    relation.trust=clamp_stat(relation.trust+2);
    return "transmet "+skill_name(*skill)+" a "+learner->name;
  }
  if(d.action=="share_camp_meal"){
    const auto fire=world_.nearby_campfire(a.position);
    Agent* companion=nullptr;
    if(fire&&a.last_shared_meal_day!=day_)for(auto& other:agents_)if(
        other.alive&&other.id!=a.id&&other.last_shared_meal_day!=day_&&
        world_.nearby_campfire(other.position)==fire&&(!companion||other.id<companion->id))companion=&other;
    if(!a.alive||!fire||!companion||world_.stored_food(*fire)<2)return "repas commun impossible";
    FoodItem first,second;
    if(!world_.take_stored_food(*fire,&first,a.behavior.preferred_foods))return "repas commun impossible";
    if(!world_.take_stored_food(*fire,&second,companion->behavior.preferred_foods)){
      world_.store_food(*fire,first);return "repas commun impossible";
    }
    a.hunger=clamp_stat(a.hunger-first.nutrition);
    companion->hunger=clamp_stat(companion->hunger-second.nutrition);
    a.last_shared_meal_day=day_;companion->last_shared_meal_day=day_;
    auto& outgoing=a.relationships[companion->id];++outgoing.interactions;
    outgoing.trust=clamp_stat(outgoing.trust+2);outgoing.affinity=clamp_stat(outgoing.affinity+3);
    auto& incoming=companion->relationships[a.id];++incoming.interactions;
    incoming.trust=clamp_stat(incoming.trust+2);incoming.affinity=clamp_stat(incoming.affinity+3);
    a.remember("J'ai partagé un repas avec "+companion->name+".");
    companion->remember("J'ai partagé un repas avec "+a.name+".");
    return "partage un repas avec "+companion->name;
  }
  if(d.action=="hold_vigil"){
    const auto fire=world_.nearby_campfire(a.position);
    Agent* companion=nullptr;
    if(fire&&day_phase_for(cycle_in_day_,cycles_per_day_)==DayPhase::Night&&a.last_vigil_day!=day_)
      for(auto& other:agents_)if(other.alive&&other.id!=a.id&&other.last_vigil_day!=day_&&
          world_.nearby_campfire(other.position)==fire&&(!companion||other.id<companion->id))companion=&other;
    if(!a.alive||!companion)return "veille impossible";
    a.last_vigil_day=day_;companion->last_vigil_day=day_;
    a.fatigue=clamp_stat(a.fatigue+1);companion->fatigue=clamp_stat(companion->fatigue+1);
    a.boredom=clamp_stat(a.boredom-12);companion->boredom=clamp_stat(companion->boredom-12);
    auto& outgoing=a.relationships[companion->id];++outgoing.interactions;
    outgoing.trust=clamp_stat(outgoing.trust+3);outgoing.affinity=clamp_stat(outgoing.affinity+2);
    auto& incoming=companion->relationships[a.id];++incoming.interactions;
    incoming.trust=clamp_stat(incoming.trust+3);incoming.affinity=clamp_stat(incoming.affinity+2);
    a.remember("J'ai veillé près du feu avec "+companion->name+".");
    companion->remember("J'ai veillé près du feu avec "+a.name+".");
    return "veille avec "+companion->name;
  }
  if(d.action=="celebrate"){
    const auto fire=world_.nearby_campfire(a.position);
    if(!a.alive||!fire||!a.celebration_pending)return "celebration impossible";
    const int participants=static_cast<int>(std::count_if(agents_.begin(),agents_.end(),[&](const Agent& other){
      return other.alive&&world_.nearby_campfire(other.position)==fire;
    }));
    if(participants<2)return "celebration impossible";
    for(auto& other:agents_)if(other.alive&&world_.nearby_campfire(other.position)==fire){
      other.boredom=clamp_stat(other.boredom-15);
      other.remember("Le groupe a célébré la réussite de "+a.name+".");
      add_emotion(other,EmotionType::Joy,40,"célébration de la réussite de "+a.name,
                  "renforcer les liens et l'engagement",4,day_);
      if(other.id!=a.id){auto& relation=a.relationships[other.id];++relation.interactions;relation.affinity=clamp_stat(relation.affinity+3);}
    }
    a.celebration_pending=false;
    return "celebre avec le groupe";
  }
  if(d.action=="mourn"){
    const auto fire=world_.nearby_campfire(a.position);
    const Agent* deceased=nullptr;
    bool has_companion=false;
    if(fire)for(const auto& other:agents_){
      if(other.alive&&other.id!=a.id&&world_.nearby_campfire(other.position)==fire)has_companion=true;
      if(!other.alive&&other.id!=a.id&&!a.mourned_agents.contains(other.id)&&
         (!deceased||other.id<deceased->id))deceased=&other;
    }
    if(!a.alive||!fire||!has_companion||!deceased)return "deuil impossible";
    a.mourned_agents.insert(deceased->id);a.boredom=clamp_stat(a.boredom-5);
    a.remember("J'ai honoré la mémoire de "+deceased->name+" avec le groupe.");
    add_emotion(a,EmotionType::Sadness,45,"mort de "+deceased->name,
                "chercher le soutien du groupe",6,day_);
    return "honore la memoire de "+deceased->name;
  }
  if(d.action=="assemble_shelter"){
    if(!a.alive||a.wood_inventory<1||!world_.passable(a.position)||world_.shelter_level(a.position)>0||(a.shelter_construction&&a.shelter_construction->position!=a.position))return "assemblage impossible";
    if(!a.shelter_construction)a.shelter_construction=ShelterConstruction{a.position,0};
    --a.wood_inventory;++a.shelter_construction->progress;add_skill_experience(a,Skill::Building);
    if(a.shelter_construction->progress<3){a.remember("J'ai ajoute une unite de bois a l'abri.");return "assemble l'abri";}
    if(!world_.create_shelter(a.position)){--a.shelter_construction->progress;++a.wood_inventory;return "assemblage impossible";}
    a.shelter_construction.reset();a.remember("J'ai construit un abri en bois.");return "construit un abri";
  }
  if(d.action=="move"){auto dir=d.parameters["direction"].get<std::string>();Position p=world_.step(a.position,dir);if(world_.passable(p)){a.position=p;a.fatigue=clamp_stat(a.fatigue+std::max(0,(50-a.attributes.agility)/20));a.remember_map(p,world_.terrain(p));++a.map_visit_counts[{p.x,p.y}];a.remember("Je me suis deplace vers "+dir+".");return "se deplace vers "+dir;}a.remember_map(p,world_.terrain(p));a.remember("Mon deplacement vers "+dir+" a ete bloque par un obstacle.");return "deplacement bloque";}
  if(d.action=="warn_danger"||d.action=="help_companion"||d.action=="accompany"||d.action=="confront"||d.action=="reconcile"){
    Agent* target=nullptr;for(auto& other:agents_)if(other.alive&&other.id!=a.id&&other.id==d.parameters.value("target_id","")&&world_.adjacent(a.position,other.position))target=&other;
    if(!target)return "interaction relationnelle indisponible";
    auto& outgoing=a.relationships[target->id];auto& incoming=target->relationships[a.id];
    if(d.action=="warn_danger"){
      if(a.observed_animals.empty()||a.last_warning_day==day_)return "avertissement indisponible";
      target->observed_animals.insert(a.observed_animals.begin(),a.observed_animals.end());a.last_warning_day=day_;
      ++outgoing.interactions;++incoming.interactions;outgoing.trust=clamp_stat(outgoing.trust+3);incoming.trust=clamp_stat(incoming.trust+2);
      a.remember("J'ai averti "+target->name+" des animaux dangereux observés.");target->remember(a.name+" m'a averti d'un danger.");
      return "avertit "+target->name+" du danger";
    }
    if(d.action=="help_companion"){
      if(target->fatigue<70||a.last_help_day==day_)return "aide indisponible";
      target->fatigue=clamp_stat(target->fatigue-(8+a.personality.empathy/20));a.fatigue=clamp_stat(a.fatigue+3);a.last_help_day=day_;
      ++outgoing.interactions;++incoming.interactions;outgoing.affinity=clamp_stat(outgoing.affinity+3);incoming.trust=clamp_stat(incoming.trust+4);
      a.remember("J'ai aidé "+target->name+" à récupérer.");target->remember(a.name+" m'a aidé à récupérer.");
      return "aide "+target->name+" à récupérer";
    }
    if(d.action=="accompany"){
      a.companion_id=target->id;a.companion_until_day=day_+1;
      ++outgoing.interactions;++incoming.interactions;outgoing.affinity=clamp_stat(outgoing.affinity+2);incoming.trust=clamp_stat(incoming.trust+2);
      a.remember("J'accompagne "+target->name+" jusqu'à demain.");return "accompagne "+target->name;
    }
    if(d.action=="confront"){
      if(outgoing.conflict||outgoing.affinity>15)return "conflit indisponible";
      outgoing.conflict=true;incoming.conflict=true;++outgoing.interactions;++incoming.interactions;
      outgoing.trust=clamp_stat(outgoing.trust-5);incoming.trust=clamp_stat(incoming.trust-5);
      outgoing.affinity=clamp_stat(outgoing.affinity-4);incoming.affinity=clamp_stat(incoming.affinity-4);
      add_emotion(a,EmotionType::Anger,40,"désaccord avec "+target->name,"éviter la coopération",4,day_);
      add_emotion(*target,EmotionType::Anger,35,"dispute avec "+a.name,"éviter la coopération",4,day_);
      a.remember("Je me suis disputé avec "+target->name+".");target->remember("Je me suis disputé avec "+a.name+".");
      return "se dispute avec "+target->name;
    }
    if(!outgoing.conflict)return "réconciliation indisponible";
    outgoing.conflict=false;incoming.conflict=false;++outgoing.interactions;++incoming.interactions;
    outgoing.trust=clamp_stat(outgoing.trust+5);incoming.trust=clamp_stat(incoming.trust+5);
    outgoing.affinity=clamp_stat(outgoing.affinity+5);incoming.affinity=clamp_stat(incoming.affinity+5);
    add_emotion(a,EmotionType::Hope,30,"réconciliation avec "+target->name,"reprendre la coopération",4,day_);
    add_emotion(*target,EmotionType::Hope,30,"réconciliation avec "+a.name,"reprendre la coopération",4,day_);
    a.remember("Je me suis réconcilié avec "+target->name+".");target->remember("Je me suis réconcilié avec "+a.name+".");
    return "se réconcilie avec "+target->name;
  }
  if(d.action=="talk"){auto id=d.parameters["target_agent_id"].get<std::string>();for(auto&o:agents_)if(o.id==id){std::string msg=d.parameters.value("message","Bonjour.");a.remember("J'ai parle a "+o.name+" : "+msg);o.remember(a.name+" m'a parle : "+msg);auto& outgoing=a.relationships[o.id];++outgoing.interactions;outgoing.trust=clamp_stat(outgoing.trust+1+std::max(0,a.personality.empathy-50)/25);outgoing.affinity=clamp_stat(outgoing.affinity+1);auto& incoming=o.relationships[a.id];++incoming.interactions;incoming.trust=clamp_stat(incoming.trust+1+std::max(0,o.personality.empathy-50)/25);incoming.affinity=clamp_stat(incoming.affinity+1);return "parle a "+o.name;}}
  return "attend";
}

void Simulation::update_behavior_after_action(Agent& agent,const Agent& before,const Decision& decision,
                                              const std::string& result,bool succeeded){
  int boredom_delta=0;
  if(decision.type==DecisionType::Blocked||decision.action=="wait"||!succeeded)boredom_delta=3;
  else if(decision.action=="move"){
    const auto previous=before.map_visit_counts.find({agent.position.x,agent.position.y});
    const int previous_visits=previous==before.map_visit_counts.end()?0:previous->second;
    boredom_delta=previous_visits==0?-2:1+std::max(0,previous_visits-2)/2;
  }else if(decision.action=="talk"||decision.action=="hunt_animal"||decision.action=="hunt_rabbit"||
           decision.action=="share_camp_meal"||decision.action=="hold_vigil"||
           decision.action=="celebrate"||decision.action=="mourn")boredom_delta=-12;
  else if(decision.action=="eat_food"||decision.action=="eat_berries"||decision.action=="eat_carried_food"||decision.action=="eat_camp_food"||decision.action=="drink")boredom_delta=-5;
  agent.boredom=clamp_stat(agent.boredom+boredom_delta);
  if(!succeeded)add_emotion(agent,EmotionType::Stress,25,"échec de l'action "+decision.action,
                            "réduire la prise de risque",3,day_);
  if(!succeeded&&agent.boredom>=70&&agent.personality.patience<50)
    add_emotion(agent,EmotionType::Anger,35,"échecs répétés de "+decision.action,
                "exprimer un conflit avec un proche",3,day_);
  else if(decision.action=="hunt_animal"||decision.action=="hunt_rabbit"||decision.action=="build_shelter"||
          decision.action=="celebrate")add_emotion(agent,EmotionType::Joy,25,"réussite de l'action "+decision.action,
                                                    "soutenir l'engagement",3,day_);

  if(succeeded&&(decision.action=="build_shelter"||(decision.action=="assemble_shelter"&&result=="construit un abri"))&&agent.project.status==ProjectStatus::Blocked&&agent.project.missing_capability=="build_shelter"){
    agent.project.status=ProjectStatus::Active;
    agent.project.blocked_reason.clear();
    agent.project.missing_capability.clear();
    agent.remember("La construction de l'abri permet a mon projet de reprendre.");
    return;
  }

  if(agent.project.status!=ProjectStatus::Active||!succeeded)return;
  const int previous_progress=agent.project.progress;
  if(agent.project.key=="build_shelter")
    agent.project.progress=static_cast<int>(std::count_if(agent.map_memory.begin(),agent.map_memory.end(),[](const auto& cell){return cell.second==Terrain::Tree;}));
  else if(agent.project.key=="secure_food"&&succeeded&&decision.action=="deposit_food")
    ++agent.project.progress;
  else if(agent.project.key=="map_region")
    agent.project.progress=static_cast<int>(agent.map_memory.size());
  if(agent.project.progress>previous_progress){agent.project.last_progress_cycle=simulation_cycle_;add_emotion(agent,EmotionType::Hope,30,"progression du projet "+agent.project.title,"maintenir l'effort",4,day_);}
  if(agent.project.progress<agent.project.target)return;

  if(agent.project.key=="secure_food"){
    agent.project.status=ProjectStatus::Completed;
    agent.celebration_pending=true;
    agent.project.blocked_reason.clear();agent.project.missing_capability.clear();
    agent.remember("La première réserve commune de nourriture est constituée.");
    return;
  }
  agent.project.status=ProjectStatus::Blocked;
  if(agent.project.key=="build_shelter"){
    agent.project.blocked_reason="Je connais assez d'arbres, mais personne ne sait encore construire un abri.";
    agent.project.missing_capability="build_shelter";
  }else if(agent.project.key=="secure_food"){
    agent.project.blocked_reason="La chasse réussit, mais la nourriture ne peut pas être conservée pour le groupe.";
    agent.project.missing_capability="food_storage";
  }else if(agent.project.key=="map_region"){
    agent.project.blocked_reason="Mes découvertes restent dans ma mémoire et ne forment pas une carte partageable.";
    agent.project.missing_capability="persistent_cartography";
  }
  agent.remember("Mon projet "+agent.project.title+" est bloque : "+agent.project.blocked_reason);
  agent.boredom=clamp_stat(agent.boredom-20);
}

void Simulation::run_day(){
  (void)run_day(nullptr);
}

bool Simulation::run_day(IUserInterface* interface){
  ++day_;
  date_=date_from_absolute_day(day_);
  climate_=climate_for(date_);
  world_.advance_nature(rng_);
  const auto ecology=world_.ecology();
  if(ecology.births>0||ecology.predations>0||ecology.regrown_food>0)
    logger_.message("Écosystème : "+std::to_string(ecology.births)+" naissance(s), "+
                    std::to_string(ecology.predations)+" prédation(s), "+
                    std::to_string(ecology.regrown_food)+" repousse(s), "+
                    std::to_string(ecology.depleted_patches)+" parcelle(s) épuisée(s).");
  const int spoiled_food=world_.age_stored_food();
  if(spoiled_food>0)logger_.message(std::to_string(spoiled_food)+" ration(s) du foyer se sont avariées.");
  world_.apply_climate(date_,climate_);
  for(auto& agent:agents_) {
    if(agent.carried_food&&++agent.carried_food->age_days>=agent.carried_food->shelf_life_days){
      agent.carried_food.reset();
      agent.remember("Ma ration transportée s'est avariée.");
      logger_.message(agent.name+" perd une ration transportée avariée.");
    }
    if(!agent.alive||agent.sleeping_days<=0) continue;
    --agent.sleeping_days;
    agent.fatigue=clamp_stat(agent.fatigue-(10+agent.attributes.recuperation/10));
    logger_.message("Jour "+std::to_string(day_)+" / cycle elementaire "+std::to_string(simulation_cycle_)+" — "+agent.name+" dort.");
  }
  for(int action_index=0;action_index<cycles_per_day_;++action_index){
    cycle_in_day_=action_index+1;
    ++simulation_cycle_;
    for(auto& agent:agents_){
      if(!agent.alive||agent.sleeping_days!=0) continue;
      advance_action_needs(agent,action_index);
      Agent before=agent;
      Decision decision=decider_.decide(perceive(agent));
      std::string error;
      if(!validate_decision(decision,agent,world_,agents_,error,day_,
                            day_phase_for(cycle_in_day_,cycles_per_day_))){
        decision=Decision{};
        decision.reason="invalid decision";
        logger_.message("Jour "+std::to_string(day_)+" / cycle elementaire "+std::to_string(simulation_cycle_)+" — "+agent.name+" decision invalide : "+error);
      }
      const auto result=execute(agent,decision);
      const bool succeeded=action_succeeded(decision,result);
      update_behavior_after_action(agent,before,decision,result,succeeded);
      auto& planning=planning_history_[agent.id];
      planning.push_back({{"action",decision.type==DecisionType::Blocked?"blocked":decision.action},{"outcome",succeeded?"success":"failure"},{"reason",decision.reason},{"parameters",decision.parameters},{"x",agent.position.x},{"y",agent.position.y},{"project",agent.project.key},{"project_status",project_status_name(agent.project.status)},{"boredom",agent.boredom}});
      while(planning.size()>20) planning.erase(planning.begin());
      auto& history=action_history_[agent.id];
      std::string entry="day="+std::to_string(day_)+" simulation_cycle="+std::to_string(simulation_cycle_)+
                        " cycle_in_day="+std::to_string(cycle_in_day_)+
                        " phase="+day_phase_name(day_phase_for(cycle_in_day_,cycles_per_day_))+
                        " action="+(decision.type==DecisionType::Blocked?"blocked":decision.action)+
                        " outcome="+(succeeded?"success":"failure")+" result="+result;
      if(!decision.reason.empty()) entry+=" reason="+decision.reason;
      if(!decision.need.empty()) entry+=" need="+decision.need;
      if(!decision.obstacle.empty()) entry+=" obstacle="+decision.obstacle;
      if(!decision.desired_result.empty()) entry+=" desired_result="+decision.desired_result;
      entry+=" project="+agent.project.key+" project_status="+project_status_name(agent.project.status)+" project_progress="+std::to_string(agent.project.progress)+"/"+std::to_string(agent.project.target)+" boredom="+std::to_string(agent.boredom);
      entry+=" calendar="+calendar_label(date_)+" temperature_c="+std::to_string(climate_.temperature_c)+" climate="+climate_.condition;
      if(!agent.project.missing_capability.empty())entry+=" missing_capability="+agent.project.missing_capability;
      history.push_back(entry);
      if(history.size()>1200) history.erase(history.begin());
      logger_.event(simulation_cycle_,day_,before,decision,result,agent,date_,climate_);
    }
    if(interface){
      const auto snapshot=make_ui_snapshot(date_,simulation_cycle_,climate_,world_,agents_,
                                           logger_.recent(),cycle_in_day_,cycles_per_day_);
      if(!interface->present(snapshot)){
        logger_.message("Interface graphique fermée par l'utilisateur.");
        return false;
      }
    }
  }
  for(auto& agent:agents_) if(agent.alive){update_needs(agent);update_health_conditions(agent);update_emotions(agent);apply_climate_effects(agent,date_,climate_);}
  update_population();
  return true;
}

SimulationRunResult Simulation::run(int days,int delay_ms,int render_every_days,
                                    const ValidationGate& validation_gate,
                                    IUserInterface* interface){
  bool stop_requested=false;
  for(int i=0;i<days;++i){
    if(!run_day(interface))break;
    bool period_complete=day_%report_every_days_==0;
    bool all_dead=std::none_of(agents_.begin(),agents_.end(),[](const Agent&a){return a.alive;});
    if(all_dead) logger_.message("Simulation arrêtée : tous les personnages sont morts.");
    if(!interface&&((render_every_days>0&&day_%render_every_days==0)||all_dead))
      render(date_,simulation_cycle_,climate_,world_,agents_,logger_);

    if(period_complete){
      std::cout << "\n=== FENETRE IA : " << calendar_label(date_) << " / Jour absolu " << day_ << " / Cycle elementaire "
                << simulation_cycle_ << " ===\n" << std::flush;
      if(!reporter_){
        std::cout << "API désactivée : aucun appel à effectuer.\n" << std::flush;
      } else {
        const auto total_calls=agents_.size()*2;
        std::size_t call_number=0;
        for(auto& agent:agents_){
          auto& history=action_history_[agent.id];
          std::cout << "Appel " << ++call_number << "/" << total_calls
                    << " — bilan de " << agent.name << " (en cours...)\n" << std::flush;
          const PeriodContext period_context{date_,climate_,logger_.period_memories(agent.id,12)};
          auto report_result=run_with_activity(interface,
              {UiActivityKind::PeriodReport,date_,simulation_cycle_,agent.id,agent.name,
               call_number,total_calls,0},
              [&]{return reporter_->report_period(simulation_cycle_,day_,agent,history,period_context);});
          auto report=std::move(report_result.payload);
          std::cout << "Appel " << call_number << "/" << total_calls << " — bilan de "
                    << agent.name << (report.is_null()?" indisponible":" terminé");
          if(report.is_null()&&!reporter_->last_error().empty())std::cout << "\n  Diagnostic API : " << reporter_->last_error();
          std::cout << "\n" << std::flush;
          if(!report.is_null()) logger_.ai_report(simulation_cycle_,day_,agent,report,date_,climate_);
          if(!report_result.interface_open){stop_requested=true;break;}

          std::cout << "Appel " << ++call_number << "/" << total_calls
                    << " — demande d'évolution pour " << agent.name << " (en cours...)\n" << std::flush;
          const EvolutionContext evolution_context{active_world_mechanisms(),
              logger_.evolution_memory(24),available_actions(agent,world_,agents_,day_,
                  day_phase_for(cycle_in_day_,cycles_per_day_))};
          auto request_result=run_with_activity(interface,
              {UiActivityKind::EvolutionRequest,date_,simulation_cycle_,agent.id,agent.name,
               call_number,total_calls,0},
              [&]{return reporter_->request_evolution(simulation_cycle_,day_,agent,history,report,
                                                       evolution_context);});
          auto request=std::move(request_result.payload);
          std::cout << "Appel " << call_number << "/" << total_calls << " — demande d'évolution pour "
                    << agent.name << (request.is_null()?" indisponible":" terminée");
          if(request.is_null()&&!reporter_->last_error().empty())std::cout << "\n  Diagnostic API : " << reporter_->last_error();
          std::cout << "\n" << std::flush;
          if(!request.is_null()) logger_.ai_feature_request(simulation_cycle_,day_,agent,report,request);
          if(!request_result.interface_open){stop_requested=true;break;}
          history.clear();
        }
        if(stop_requested)
          logger_.message("Interface graphique fermée pendant la fenêtre IA.");
        else
          std::cout << "Fenêtre IA terminée : " << total_calls << " appels tentés.\n" << std::flush;
      }
      if(stop_requested)break;
      const auto constraint=devil_.draw(day_,simulation_cycle_,world_,agents_,logger_.known_evolution_keys());
      if(constraint){
        const auto id=logger_.devil_constraint(simulation_cycle_,day_,*constraint);
        if(!id.empty())std::cout << "\n=== APPARITION DU DIABLE ===\n"
                                << "Une contrainte réelle est proposée : " << constraint->value("title","sans titre") << "\n"
                                << "Elle attend la validation prévue par la configuration.\n" << std::flush;
      }else{
        std::cout << "Tirage du Diable : aucune apparition cette fenêtre.\n" << std::flush;
      }
    }
    save_checkpoint();
    if(all_dead) break;
    if(period_complete&&validation_gate){
      if(!validation_gate(day_,simulation_cycle_)){
        logger_.message("Simulation mise en pause après la validation humaine.");
        break;
      }
      if(interface)delay_ms=std::clamp(interface->simulation_delay_ms(delay_ms),0,10000);
      if(interface&&interface->restart_requested())return {true,days-i-1};
    }
    if(delay_ms>0){
      if(interface){if(!interface->idle_for(delay_ms)){logger_.message("Interface graphique fermée par l'utilisateur.");break;}}
      else std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
  }
  return {false,0};
}
}
