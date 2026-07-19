#include "autopoiesis/simulation.hpp"
#include "autopoiesis/feature_request.hpp"
#include "autopoiesis/renderer.hpp"
#include <algorithm>
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
  json campfires=json::array();for(const auto& position:agent.known_campfires)campfires.push_back({{"x",position.first},{"y",position.second}});
  json carried=nullptr;if(agent.carried_food)carried={{"type",static_cast<int>(agent.carried_food->type)},{"nutrition",agent.carried_food->nutrition}};
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
          {"wood_inventory",agent.wood_inventory},
          {"branch_inventory",agent.branch_inventory},
          {"carried_food",std::move(carried)},
          {"shelter_construction",std::move(shelter)}};
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
      value.at("trust").get<int>(),value.at("affinity").get<int>(),value.at("interactions").get<int>()};
  for(const auto& animal:state.at("observed_animals"))agent.observed_animals.insert(animal.get<std::string>());
  for(const auto& fire:state.value("known_campfires",json::array()))
    agent.known_campfires.insert({fire.at("x").get<int>(),fire.at("y").get<int>()});
  agent.wood_inventory=state.at("wood_inventory").get<int>();
  agent.branch_inventory=state.value("branch_inventory",0);
  const auto carried=state.value("carried_food",json(nullptr));
  if(!carried.is_null())agent.carried_food=FoodItem{
      static_cast<FoodType>(carried.at("type").get<int>()),carried.at("nutrition").get<int>()};
  if(!state.at("shelter_construction").is_null()){
    const auto& shelter=state.at("shelter_construction");
    agent.shelter_construction=ShelterConstruction{
        {shelter.at("x").get<int>(),shelter.at("y").get<int>()},shelter.at("progress").get<int>()};
  }
  return agent;
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
  const auto attributes=me.value("attributes",json::object());
  const auto personality=me.value("personality",json::object());
  const auto behavior=me.value("behavior",json::object());
  const auto project=me.value("project",json::object());
  const int boredom=me.value("boredom",0);
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
  if(best_goal=="eat"&&has_action("eat_camp_food"))
    return {DecisionType::Action,"eat_camp_food",json::object(),"Je prends un repas dans la réserve commune.","food","","be fed"};
  const bool vital_emergency=hunger>=75||thirst>=75;
  const bool carrying_food=!me.value("carried_food",json(nullptr)).is_null();
  if(carrying_food&&has_action("deposit_food"))
    return {DecisionType::Action,"deposit_food",json::object(),
            "Je dépose ma nourriture dans la réserve commune.","réserve","","approvisionner le camp"};
  if(camp_time&&!vital_emergency&&has_action("rest_by_campfire"))
    return {DecisionType::Action,"rest_by_campfire",json::object(),
            "Je passe la nuit près du feu de camp.","repos nocturne","","rester au camp"};
  if(camp_time&&!vital_emergency&&has_action("build_campfire"))
    return {DecisionType::Action,"build_campfire",json::object(),
            "J'allume un feu avant la nuit.","repos nocturne","","établir un camp"};
  if(!vital_emergency&&me.value("branch_inventory",0)<3&&has_action("collect_branch"))
    return {DecisionType::Action,"collect_branch",json::object(),
            "Je ramasse une branche pour préparer un feu.","camp","","réunir du combustible"};
  if(best_goal=="project"&&project.value("key","")=="build_shelter"){
    if(has_action("assemble_shelter"))
      return {DecisionType::Action,"assemble_shelter",json::object(),"J'assemble mon abri.","projet","","construire un abri"};
    if(has_action("build_shelter"))
      return {DecisionType::Action,"build_shelter",json::object(),"Je construis mon abri.","projet","","construire un abri"};
    if(has_action("harvest_wood"))
      return {DecisionType::Action,"harvest_wood",json::object(),"Je prélève du bois pour mon abri.","projet","","construire un abri"};
  }
  if(best_goal=="social"){
    for(const auto& other:p.value.value("visible_agents",json::array()))if(other.value("adjacent",false))
      return {DecisionType::Action,"talk",{{"target_agent_id",other.value("id","")},{"message","Partageons ce que nous avons appris."}},"Je renforce notre coopération.","relation","","mieux coopérer"};
  }
  if((best_goal=="eat"||best_goal=="project")&&has_action("hunt_animal")&&p.value.contains("animals")){
    const json* selected=nullptr;
    for(const auto& animal:p.value["animals"]){
      if(!animal.value("adjacent",false))continue;
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
  std::set<std::pair<int,int>> campfire_cells;
  for(const auto& cell:known){
    const std::pair<int,int> coordinates{cell.value("x",0),cell.value("y",0)};
    known_cells[coordinates]=cell;
    if(cell.value("status","")=="traversable")traversable.insert(coordinates);
    if(cell.value("food",0)>0)food_targets.insert(coordinates);
    if(cell.value("terrain",-1)==static_cast<int>(Terrain::Water))water_cells.insert(coordinates);
    if(cell.value("branches",0)>0)branch_targets.insert(coordinates);
    if(cell.value("campfire",false))campfire_cells.insert(coordinates);
  }
  for(const auto& cell:p.value.value("cells",json::array())){
    const std::pair<int,int> coordinates{cell.value("x",0),cell.value("y",0)};
    const int terrain=cell.value("terrain",-1);
    if(terrain==static_cast<int>(Terrain::Ground)||terrain==static_cast<int>(Terrain::Bush))traversable.insert(coordinates);
    if(cell.value("food",0)>0||terrain==static_cast<int>(Terrain::Bush))food_targets.insert(coordinates);
    if(terrain==static_cast<int>(Terrain::Water))water_cells.insert(coordinates);
    if(cell.value("branches",0)>0)branch_targets.insert(coordinates);
    if(cell.value("campfire",false))campfire_cells.insert(coordinates);
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
  if(!carrying_food&&!vital_emergency&&!campfire_cells.empty()&&has_action("collect_food"))
    return {DecisionType::Action,"collect_food",json::object(),
            "Je collecte cette nourriture pour la réserve commune.","réserve","","approvisionner le camp"};
  if(camp_time&&!vital_emergency){
    std::set<std::pair<int,int>> camp_positions;
    for(const auto& coordinates:campfire_cells){
      Position fire{coordinates.first,coordinates.second};
      for(const auto& direction:directions){
        const auto candidate=step(fire,direction);
        if(traversable.contains({candidate.x,candidate.y}))camp_positions.insert({candidate.x,candidate.y});
      }
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
  if (decision.action == "build_campfire") return result == "allume un feu de camp";
  if (decision.action == "rest_by_campfire") return result == "reste pres du feu de camp";
  if (decision.action == "collect_food") return result == "ramasse de la nourriture";
  if (decision.action == "deposit_food") return result == "depose de la nourriture au camp";
  if (decision.action == "eat_carried_food") return result == "mange sa nourriture transportee";
  if (decision.action == "eat_camp_food") return result == "mange une reserve du camp";
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
      {"decider",decider_.checkpoint()}};
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
  for(int dy=-3;dy<=3;++dy) for(int dx=-3;dx<=3;++dx)if(std::abs(dx)+std::abs(dy)<=3){Position p=world_.wrap({a.position.x+dx,a.position.y+dy});if(!perceived.insert({p.x,p.y}).second)continue;auto terrain=world_.terrain(p);a.remember_map(p,terrain);if(world_.campfire(p))a.known_campfires.insert({p.x,p.y});json animals=json::array();for(const auto& animal:world_.animals())if(animal.alive&&animal.position==p){const auto type=animal_type_name(animal.type);a.observed_animals.insert(type);animals.push_back({{"id",animal.id},{"type",type},{"danger",animal.danger},{"nutrition",animal.nutrition}});}cells.push_back({{"x",p.x},{"y",p.y},{"terrain",static_cast<int>(terrain)},{"food",world_.food(p)},{"branches",world_.branches(p)},{"campfire",world_.campfire(p)},{"stored_food",world_.stored_food(p)},{"water",terrain==Terrain::Water},{"rabbit",world_.rabbit_alive()&&p==world_.rabbit()},{"animals",animals}});}
  json known=json::array();for(const auto&[position,terrain]:a.map_memory){Position p{position.first,position.second};bool traversable=terrain==Terrain::Ground||terrain==Terrain::Bush;const bool known_fire=a.known_campfires.contains(position);known.push_back({{"x",position.first},{"y",position.second},{"terrain",static_cast<int>(terrain)},{"status",traversable?"traversable":"blocked"},{"visit_count",a.map_visit_counts[position]},{"food",world_.food(p)},{"branches",world_.branches(p)},{"campfire",known_fire},{"stored_food",known_fire?world_.stored_food(p):0}});}
  json visible=json::array();for(const auto&o:agents_)if(o.alive&&o.id!=a.id&&world_.toroidal_distance(o.position,a.position)<=3)visible.push_back({{"id",o.id},{"name",o.name},{"x",o.position.x},{"y",o.position.y},{"adjacent",world_.adjacent(a.position,o.position)},{"relationship",relationships_json(a.relationships).value(o.id,json::object())}});
  json animals=json::array();for(const auto& animal:world_.animals())if(animal.alive&&world_.toroidal_distance(animal.position,a.position)<=3)animals.push_back({{"id",animal.id},{"type",animal_type_name(animal.type)},{"x",animal.position.x},{"y",animal.position.y},{"danger",animal.danger},{"nutrition",animal.nutrition},{"adjacent",world_.adjacent(a.position,animal.position)}});
  json mem=json::array();for(const auto&s:a.memories)mem.push_back(s);
  const auto actions=available_actions(a,world_,agents_);
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
  const auto phase=day_phase_for(cycle_in_day_,cycles_per_day_);
  json carried=nullptr;if(a.carried_food)carried={{"type",food_type_name(a.carried_food->type)},{"nutrition",a.carried_food->nutrition}};
  return Perception{json{{"world_width",World::width},{"world_height",World::height},{"calendar",calendar_json(date_)},{"climate",climate_json(climate_)},{"time",{{"phase",day_phase_name(phase)},{"cycle_in_day",cycle_in_day_},{"cycles_per_day",cycles_per_day_},{"cycles_until_night",std::max(0,daylight_cycles(cycles_per_day_)-cycle_in_day_+1)}}},{"self",{{"id",a.id},{"name",a.name},{"x",a.position.x},{"y",a.position.y},{"health",a.health},{"hunger",a.hunger},{"thirst",a.thirst},{"fatigue",a.fatigue},{"boredom",a.boredom},{"wood_inventory",a.wood_inventory},{"branch_inventory",a.branch_inventory},{"carried_food",carried},{"shelter_construction",construction},{"personality",personality_json(a.personality)},{"attributes",attributes_json(a.attributes)},{"behavior",behavior_json(a.behavior)},{"project",project_json(a.project)},{"relationships",relationships_json(a.relationships)},{"observed_animals",a.observed_animals}}},{"cells",cells},{"known_map",known},{"action_history",planning_history_[a.id]},{"visible_agents",visible},{"animals",animals},{"memories",mem},{"available_actions",actions}}};
}

void Simulation::advance_action_needs(Agent& a,int action_index){if(action_index%80==0)a.hunger=clamp_stat(a.hunger+1);const int fatigue_interval=12+std::max(0,a.attributes.endurance-50)/10;if(action_index%fatigue_interval==0)a.fatigue=clamp_stat(a.fatigue+1);const int thirst_interval=60+std::max(0,a.attributes.endurance-40);if(action_index%thirst_interval==0)a.thirst=clamp_stat(a.thirst+1);}
void Simulation::update_needs(Agent& a){if(a.hunger>=90){++a.critical_hunger_days;if(a.critical_hunger_days>3)a.health=clamp_stat(a.health-std::max(1,7-a.attributes.willpower/20));}else a.critical_hunger_days=0;if(a.thirst>=90){++a.critical_thirst_days;if(a.critical_thirst_days>1)a.health=clamp_stat(a.health-std::max(2,10-a.attributes.willpower/15));}else a.critical_thirst_days=0;if(a.health==0)a.alive=false;}

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
  if(d.action=="rest"){if(!a.alive||a.fatigue<=0||!world_.passable(a.position))return "attend";a.fatigue=clamp_stat(a.fatigue-(15+a.attributes.recuperation/10));return "rested";}
  if(d.action=="sleep"){a.sleeping_days=2;a.fatigue=clamp_stat(a.fatigue-(15+a.attributes.recuperation/10));a.remember("Je commence a dormir.");return "commence a dormir";}
  if(d.action=="drink"){if(!world_.drinkable(a.position))return "ne trouve pas d'eau";a.thirst=clamp_stat(a.thirst-(40+a.attributes.recuperation/10));return "boit de l'eau";}
  if(d.action=="eat_food"){FoodType type{};int nutrition=0;if(world_.consume_food(a.position,&type,&nutrition)){a.hunger=clamp_stat(a.hunger-nutrition);if(type==FoodType::Mushrooms)a.health=clamp_stat(a.health-std::max(0,5-a.attributes.disease_resistance/20));return "mange "+food_type_name(type);}return "ne peut pas manger ici";}
  if(d.action=="eat_berries"){if(world_.eat_berries(a.position)){a.hunger=clamp_stat(a.hunger-35);return "mange des baies";}return "ne peut pas manger ici";}
  if(d.action=="hunt_rabbit"){if(world_.hunt_rabbit(a.position)){a.hunger=clamp_stat(a.hunger-35);a.remember("J'ai chasse le lapin.");return "chasse le lapin";}return "ne peut pas chasser ici";}
  if(d.action=="hunt_animal"){Animal hunted;if(world_.hunt_animal(a.position,d.parameters.value("animal_id",""),&hunted)){const int injury=std::max(0,hunted.danger-(a.attributes.strength+a.attributes.toughness)/2)/5;a.health=clamp_stat(a.health-injury);a.hunger=clamp_stat(a.hunger-hunted.nutrition);a.remember("J'ai chasse "+animal_type_name(hunted.type)+".");return "chasse "+animal_type_name(hunted.type);}return "ne peut pas chasser ici";}
  if(d.action=="build_shelter"){const bool existing=world_.shelter_level(a.position)>0;if(!a.alive||!world_.build_shelter(a.position))return "materiaux insuffisants pour construire";a.remember(existing?"J'ai ameliore l'abri.":"J'ai construit un abri.");return existing?"ameliore un abri":"construit un abri";}
  if(d.action=="harvest_wood"){if(!a.alive||!world_.harvest_tree(a.position))return "aucun arbre vivant a prelever";++a.wood_inventory;a.remember("J'ai preleve une unite de bois.");return "preleve du bois";}
  if(d.action=="collect_branch"){if(!a.alive||!world_.take_branch(a.position))return "aucune branche a ramasser";++a.branch_inventory;a.remember("J'ai ramasse une branche pour le camp.");return "ramasse une branche";}
  if(d.action=="build_campfire"){if(!a.alive||a.branch_inventory<3||!world_.place_campfire(a.position))return "ne peut pas allumer de feu";a.branch_inventory-=3;a.known_campfires.insert({a.position.x,a.position.y});a.remember("J'ai allume un feu de camp.");return "allume un feu de camp";}
  if(d.action=="rest_by_campfire"){if(!a.alive||!world_.adjacent_campfire(a.position))return "aucun feu a proximite";a.fatigue=clamp_stat(a.fatigue-1);return "reste pres du feu de camp";}
  if(d.action=="collect_food"){FoodItem food;if(!a.alive||a.carried_food||!world_.consume_food(a.position,&food.type,&food.nutrition))return "aucune nourriture a ramasser";a.carried_food=food;a.remember("J'ai pris une nourriture pour la réserve commune.");return "ramasse de la nourriture";}
  if(d.action=="deposit_food"){const auto fire=world_.nearby_campfire(a.position);if(!a.alive||!a.carried_food||!fire||!world_.store_food(*fire,*a.carried_food))return "ne peut pas deposer de nourriture";a.carried_food.reset();a.remember("J'ai approvisionne la réserve commune.");return "depose de la nourriture au camp";}
  if(d.action=="eat_carried_food"){if(!a.alive||!a.carried_food)return "aucune nourriture transportee";const auto food=*a.carried_food;a.carried_food.reset();a.hunger=clamp_stat(a.hunger-food.nutrition);if(food.type==FoodType::Mushrooms)a.health=clamp_stat(a.health-std::max(0,5-a.attributes.disease_resistance/20));return "mange sa nourriture transportee";}
  if(d.action=="eat_camp_food"){const auto fire=world_.nearby_campfire(a.position);FoodItem food;if(!a.alive||!fire||!world_.take_stored_food(*fire,&food))return "aucune reserve au camp";a.hunger=clamp_stat(a.hunger-food.nutrition);if(food.type==FoodType::Mushrooms)a.health=clamp_stat(a.health-std::max(0,5-a.attributes.disease_resistance/20));return "mange une reserve du camp";}
  if(d.action=="assemble_shelter"){
    if(!a.alive||a.wood_inventory<1||!world_.passable(a.position)||world_.shelter_level(a.position)>0||(a.shelter_construction&&a.shelter_construction->position!=a.position))return "assemblage impossible";
    if(!a.shelter_construction)a.shelter_construction=ShelterConstruction{a.position,0};
    --a.wood_inventory;++a.shelter_construction->progress;
    if(a.shelter_construction->progress<3){a.remember("J'ai ajoute une unite de bois a l'abri.");return "assemble l'abri";}
    if(!world_.create_shelter(a.position)){--a.shelter_construction->progress;++a.wood_inventory;return "assemblage impossible";}
    a.shelter_construction.reset();a.remember("J'ai construit un abri en bois.");return "construit un abri";
  }
  if(d.action=="move"){auto dir=d.parameters["direction"].get<std::string>();Position p=world_.step(a.position,dir);if(world_.passable(p)){a.position=p;a.fatigue=clamp_stat(a.fatigue+std::max(0,(50-a.attributes.agility)/20));a.remember_map(p,world_.terrain(p));++a.map_visit_counts[{p.x,p.y}];a.remember("Je me suis deplace vers "+dir+".");return "se deplace vers "+dir;}a.remember_map(p,world_.terrain(p));a.remember("Mon deplacement vers "+dir+" a ete bloque par un obstacle.");return "deplacement bloque";}
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
  }else if(decision.action=="talk"||decision.action=="hunt_animal"||decision.action=="hunt_rabbit")boredom_delta=-12;
  else if(decision.action=="eat_food"||decision.action=="eat_berries"||decision.action=="eat_carried_food"||decision.action=="eat_camp_food"||decision.action=="drink")boredom_delta=-5;
  agent.boredom=clamp_stat(agent.boredom+boredom_delta);

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
  if(agent.project.progress>previous_progress)agent.project.last_progress_cycle=simulation_cycle_;
  if(agent.project.progress<agent.project.target)return;

  if(agent.project.key=="secure_food"){
    agent.project.status=ProjectStatus::Completed;
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
  world_.apply_climate(date_,climate_);
  for(auto& agent:agents_) {
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
      if(!validate_decision(decision,agent,world_,agents_,error)){
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
  for(auto& agent:agents_) if(agent.alive){update_needs(agent);apply_climate_effects(agent,date_,climate_);}
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
              logger_.evolution_memory(24),available_actions(agent,world_,agents_)};
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
