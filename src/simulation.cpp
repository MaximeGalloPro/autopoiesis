#include "autopoiesis/simulation.hpp"
#include "autopoiesis/renderer.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <queue>
#include <thread>

namespace apo {
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
  const auto attributes=me.value("attributes",json::object());
  const auto personality=me.value("personality",json::object());
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
  std::string best_goal="explore";
  for(const auto&[goal,score]:utilities)if(score>utilities[best_goal])best_goal=goal;
  const std::string agent_id=me.value("id","");
  if(!agent_id.empty()){
    auto& state=goals_[agent_id];
    const double hysteresis=5.0+focus/5.0;
    if(state.remaining>0&&utilities[state.name]>0&&utilities[state.name]+hysteresis>=utilities[best_goal]){
      best_goal=state.name;--state.remaining;
    }else{
      state={best_goal,2+focus/20};
    }
  }

  if(best_goal=="hydrate"&&has_action("drink"))
    return {DecisionType::Action,"drink",json::object(),"Je bois avant de poursuivre.","water","","be hydrated"};
  if(best_goal=="eat"&&has_action("eat_food"))
    return {DecisionType::Action,"eat_food",json::object(),"Je mange la ressource disponible.","food","","be fed"};
  if(best_goal=="eat"&&has_action("eat_berries"))
    return {DecisionType::Action,"eat_berries",json::object(),"I need food","food","","be fed"};
  if(best_goal=="eat"&&has_action("hunt_animal")&&p.value.contains("animals")){
    const json* selected=nullptr;
    for(const auto& animal:p.value["animals"]){
      if(!animal.value("adjacent",false))continue;
      if(!selected||animal.value("danger",100)<selected->value("danger",100)||(animal.value("danger",100)==selected->value("danger",100)&&animal.value("id","")<selected->value("id","")))selected=&animal;
    }
    if(selected)return {DecisionType::Action,"hunt_animal",{{"animal_id",selected->value("id","")}},"Je chasse la proie la moins risquee.","food","","be fed"};
  }
  if(best_goal=="eat"&&hunger>=75&&has_action("hunt_rabbit")) return {DecisionType::Action,"hunt_rabbit",json::object(),"I need food","food","","be fed"};

  const auto history=p.value.value("action_history",json::array());
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
  for(const auto& cell:known){
    const std::pair<int,int> coordinates{cell.value("x",0),cell.value("y",0)};
    known_cells[coordinates]=cell;
    if(cell.value("status","")=="traversable")traversable.insert(coordinates);
    if(cell.value("food",0)>0)food_targets.insert(coordinates);
    if(cell.value("terrain",-1)==static_cast<int>(Terrain::Water))water_cells.insert(coordinates);
  }
  for(const auto& cell:p.value.value("cells",json::array())){
    const std::pair<int,int> coordinates{cell.value("x",0),cell.value("y",0)};
    const int terrain=cell.value("terrain",-1);
    if(terrain==static_cast<int>(Terrain::Ground)||terrain==static_cast<int>(Terrain::Bush))traversable.insert(coordinates);
    if(cell.value("food",0)>0||terrain==static_cast<int>(Terrain::Bush))food_targets.insert(coordinates);
    if(terrain==static_cast<int>(Terrain::Water))water_cells.insert(coordinates);
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

  int lowest_visits=std::numeric_limits<int>::max();
  std::string selected;
  const int spatial_sense=attributes.value("spatial_sense",50);
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
    const int weighted_visits=visits*(1+spatial_sense/25)+(known_direction?spatial_sense/10:0);
    if(!excluded&&weighted_visits<lowest_visits){lowest_visits=weighted_visits;selected=direction;}
  }
  if(selected.empty()) return {DecisionType::Action,"wait",json::object(),"No eligible exploration direction","exploration","","discover the world"};
  const std::string reason=best_goal=="hydrate"?"J'explore pour trouver de l'eau.":best_goal=="eat"?"I explore to find food":"I explore";
  return {DecisionType::Action,"move",{{"direction",selected}},reason,"exploration","","discover the world"};
}

static bool action_succeeded(const Decision& decision, const std::string& result) {
  if (decision.type == DecisionType::Blocked) return false;
  if (decision.action == "move") return result != "deplacement bloque";
  if (decision.action == "drink") return result == "boit de l'eau";
  if (decision.action == "eat_food") return result.starts_with("mange ");
  if (decision.action == "eat_berries") return result == "mange des baies";
  if (decision.action == "hunt_animal") return result.starts_with("chasse ");
  if (decision.action == "hunt_rabbit") return result == "chasse le lapin";
  return true;
}

static Agent initial_agent(std::string id,std::string name,Position position,int hunger,int fatigue,
                           Personality personality,Attributes attributes,int thirst){
  Agent agent;
  agent.id=std::move(id);agent.name=std::move(name);agent.position=position;
  agent.hunger=hunger;agent.fatigue=fatigue;agent.personality=personality;
  agent.attributes=attributes;agent.thirst=thirst;
  return agent;
}

Simulation::Simulation(unsigned seed,IDecider& d,Logger& l,ICycleReporter* reporter):world_(seed),decider_(d),logger_(l),reporter_(reporter),rng_(seed),cycles_per_day_(positive_int_from_env("CYCLES_PER_DAY",240)),report_every_days_(positive_int_from_env("REPORT_EVERY_DAYS",3)){
  agents_={initial_agent("a1","Ada",{3,2},45,20,{90,20,30,30,40},{55,65,50,45,50,45,70,60,75,80},25),
           initial_agent("a2","Borin",{10,5},55,15,{25,85,70,90,40},{75,45,70,75,55,60,50,80,45,50},30),
           initial_agent("a3","Cyra",{16,7},35,60,{40,45,95,55,90},{45,80,60,50,65,55,75,55,70,85},20)};
}

Perception Simulation::perceive(Agent& a) {
  json cells=json::array();
  std::set<std::pair<int,int>> perceived;
  for(int dy=-3;dy<=3;++dy) for(int dx=-3;dx<=3;++dx)if(std::abs(dx)+std::abs(dy)<=3){Position p=world_.wrap({a.position.x+dx,a.position.y+dy});if(!perceived.insert({p.x,p.y}).second)continue;auto terrain=world_.terrain(p);a.remember_map(p,terrain);json animals=json::array();for(const auto& animal:world_.animals())if(animal.alive&&animal.position==p)animals.push_back({{"id",animal.id},{"type",animal_type_name(animal.type)},{"danger",animal.danger},{"nutrition",animal.nutrition}});cells.push_back({{"x",p.x},{"y",p.y},{"terrain",static_cast<int>(terrain)},{"food",world_.food(p)},{"water",terrain==Terrain::Water},{"rabbit",world_.rabbit_alive()&&p==world_.rabbit()},{"animals",animals}});}
  json known=json::array();for(const auto&[position,terrain]:a.map_memory){Position p{position.first,position.second};bool traversable=terrain==Terrain::Ground||terrain==Terrain::Bush;known.push_back({{"x",position.first},{"y",position.second},{"terrain",static_cast<int>(terrain)},{"status",traversable?"traversable":"blocked"},{"visit_count",a.map_visit_counts[position]},{"food",world_.food(p)}});}
  json visible=json::array();for(const auto&o:agents_)if(o.alive&&o.id!=a.id&&world_.toroidal_distance(o.position,a.position)<=3)visible.push_back({{"id",o.id},{"name",o.name},{"x",o.position.x},{"y",o.position.y}});
  json animals=json::array();for(const auto& animal:world_.animals())if(animal.alive&&world_.toroidal_distance(animal.position,a.position)<=3)animals.push_back({{"id",animal.id},{"type",animal_type_name(animal.type)},{"x",animal.position.x},{"y",animal.position.y},{"danger",animal.danger},{"nutrition",animal.nutrition},{"adjacent",world_.adjacent(a.position,animal.position)}});
  json mem=json::array();for(const auto&s:a.memories)mem.push_back(s);
  return Perception{json{{"world_width",World::width},{"world_height",World::height},{"self",{{"id",a.id},{"name",a.name},{"x",a.position.x},{"y",a.position.y},{"health",a.health},{"hunger",a.hunger},{"thirst",a.thirst},{"fatigue",a.fatigue},{"personality",personality_json(a.personality)},{"attributes",attributes_json(a.attributes)}}},{"cells",cells},{"known_map",known},{"action_history",planning_history_[a.id]},{"visible_agents",visible},{"animals",animals},{"memories",mem},{"available_actions",available_actions(a,world_,agents_)}}};
}

void Simulation::advance_action_needs(Agent& a,int action_index){if(action_index%80==0)a.hunger=clamp_stat(a.hunger+1);const int fatigue_interval=12+std::max(0,a.attributes.endurance-50)/10;if(action_index%fatigue_interval==0)a.fatigue=clamp_stat(a.fatigue+1);const int thirst_interval=60+std::max(0,a.attributes.endurance-40);if(action_index%thirst_interval==0)a.thirst=clamp_stat(a.thirst+1);}
void Simulation::update_needs(Agent& a){if(a.hunger>=90){++a.critical_hunger_days;if(a.critical_hunger_days>3)a.health=clamp_stat(a.health-std::max(1,7-a.attributes.willpower/20));}else a.critical_hunger_days=0;if(a.thirst>=90){++a.critical_thirst_days;if(a.critical_thirst_days>1)a.health=clamp_stat(a.health-std::max(2,10-a.attributes.willpower/15));}else a.critical_thirst_days=0;if(a.health==0)a.alive=false;}

std::string Simulation::execute(Agent&a,const Decision&d){
  if(d.type==DecisionType::Blocked){a.remember("I reported a blockage: "+d.obstacle);return "signale un blocage : "+d.obstacle;}
  if(d.action=="wait"||d.action=="observe"){a.remember(d.action=="wait"?"J'ai attendu.":"J'ai observe mon environnement.");return "attend";}
  if(d.action=="rest"){if(!a.alive||a.fatigue<=0||!world_.passable(a.position))return "attend";a.fatigue=clamp_stat(a.fatigue-(15+a.attributes.recuperation/10));return "rested";}
  if(d.action=="sleep"){a.sleeping_days=2;a.fatigue=clamp_stat(a.fatigue-(15+a.attributes.recuperation/10));a.remember("Je commence a dormir.");return "commence a dormir";}
  if(d.action=="drink"){if(!world_.drinkable(a.position))return "ne trouve pas d'eau";a.thirst=clamp_stat(a.thirst-(40+a.attributes.recuperation/10));return "boit de l'eau";}
  if(d.action=="eat_food"){FoodType type{};int nutrition=0;if(world_.consume_food(a.position,&type,&nutrition)){a.hunger=clamp_stat(a.hunger-nutrition);if(type==FoodType::Mushrooms)a.health=clamp_stat(a.health-std::max(0,5-a.attributes.disease_resistance/20));return "mange "+food_type_name(type);}return "ne peut pas manger ici";}
  if(d.action=="eat_berries"){if(world_.eat_berries(a.position)){a.hunger=clamp_stat(a.hunger-35);return "mange des baies";}return "ne peut pas manger ici";}
  if(d.action=="hunt_rabbit"){if(world_.hunt_rabbit(a.position)){a.hunger=clamp_stat(a.hunger-35);a.remember("J'ai chasse le lapin.");return "chasse le lapin";}return "ne peut pas chasser ici";}
  if(d.action=="hunt_animal"){Animal hunted;if(world_.hunt_animal(a.position,d.parameters.value("animal_id",""),&hunted)){const int injury=std::max(0,hunted.danger-(a.attributes.strength+a.attributes.toughness)/2)/5;a.health=clamp_stat(a.health-injury);a.hunger=clamp_stat(a.hunger-hunted.nutrition);a.remember("J'ai chasse "+animal_type_name(hunted.type)+".");return "chasse "+animal_type_name(hunted.type);}return "ne peut pas chasser ici";}
  if(d.action=="move"){auto dir=d.parameters["direction"].get<std::string>();Position p=world_.step(a.position,dir);if(world_.passable(p)){a.position=p;a.fatigue=clamp_stat(a.fatigue+std::max(0,(50-a.attributes.agility)/20));a.remember_map(p,world_.terrain(p));++a.map_visit_counts[{p.x,p.y}];a.remember("Je me suis deplace vers "+dir+".");return "se deplace vers "+dir;}a.remember_map(p,world_.terrain(p));a.remember("Mon deplacement vers "+dir+" a ete bloque par un obstacle.");return "deplacement bloque";}
  if(d.action=="talk"){auto id=d.parameters["target_agent_id"].get<std::string>();for(auto&o:agents_)if(o.id==id){std::string msg=d.parameters.value("message","Bonjour.");a.remember("J'ai parle a "+o.name+" : "+msg);o.remember(a.name+" m'a parle : "+msg);return "parle a "+o.name;}}
  return "attend";
}

void Simulation::run_day(){
  ++day_;
  world_.advance_nature(rng_);
  for(auto& agent:agents_) {
    if(!agent.alive||agent.sleeping_days<=0) continue;
    --agent.sleeping_days;
    agent.fatigue=clamp_stat(agent.fatigue-(10+agent.attributes.recuperation/10));
    logger_.message("Jour "+std::to_string(day_)+" / cycle elementaire "+std::to_string(simulation_cycle_)+" — "+agent.name+" dort.");
  }
  for(int action_index=0;action_index<cycles_per_day_;++action_index){
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
      if(decision.type==DecisionType::Blocked) logger_.feature_request(simulation_cycle_,day_,agent,decision);
      const auto result=execute(agent,decision);
      const bool succeeded=action_succeeded(decision,result);
      auto& planning=planning_history_[agent.id];
      planning.push_back({{"action",decision.type==DecisionType::Blocked?"blocked":decision.action},{"outcome",succeeded?"success":"failure"},{"x",agent.position.x},{"y",agent.position.y}});
      while(planning.size()>20) planning.erase(planning.begin());
      auto& history=action_history_[agent.id];
      std::string entry="day="+std::to_string(day_)+" simulation_cycle="+std::to_string(simulation_cycle_)+" action="+(decision.type==DecisionType::Blocked?"blocked":decision.action)+" outcome="+(succeeded?"success":"failure")+" result="+result;
      if(!decision.reason.empty()) entry+=" reason="+decision.reason;
      if(!decision.need.empty()) entry+=" need="+decision.need;
      if(!decision.obstacle.empty()) entry+=" obstacle="+decision.obstacle;
      if(!decision.desired_result.empty()) entry+=" desired_result="+decision.desired_result;
      history.push_back(entry);
      if(history.size()>1200) history.erase(history.begin());
      logger_.event(simulation_cycle_,day_,before,decision,result,agent);
    }
  }
  for(auto& agent:agents_) if(agent.alive) update_needs(agent);
}

void Simulation::run(int days,int delay_ms,int render_every_days,const ValidationGate& validation_gate){
  for(int i=0;i<days;++i){
    run_day();
    bool period_complete=day_%report_every_days_==0;
    bool all_dead=std::none_of(agents_.begin(),agents_.end(),[](const Agent&a){return a.alive;});
    if(all_dead) logger_.message("Simulation arrêtée : tous les personnages sont morts.");
    if((render_every_days>0&&day_%render_every_days==0)||all_dead) render(day_,simulation_cycle_,world_,agents_,logger_);

    if(period_complete){
      std::cout << "\n=== FENETRE IA : Jour " << day_ << " / Cycle elementaire "
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
          auto report=reporter_->report_period(simulation_cycle_,day_,agent,history);
          std::cout << "Appel " << call_number << "/" << total_calls << " — bilan de "
                    << agent.name << (report.is_null()?" indisponible":" terminé") << "\n" << std::flush;
          if(!report.is_null()) logger_.ai_report(simulation_cycle_,day_,agent,report);

          std::cout << "Appel " << ++call_number << "/" << total_calls
                    << " — demande d'évolution pour " << agent.name << " (en cours...)\n" << std::flush;
          auto request=reporter_->request_evolution(simulation_cycle_,day_,agent,history,report);
          std::cout << "Appel " << call_number << "/" << total_calls << " — demande d'évolution pour "
                    << agent.name << (request.is_null()?" indisponible":" terminée") << "\n" << std::flush;
          if(!request.is_null()) logger_.ai_feature_request(simulation_cycle_,day_,agent,report,request);
          history.clear();
        }
        std::cout << "Fenêtre IA terminée : " << total_calls << " appels tentés.\n" << std::flush;
      }
    }
    if(all_dead) break;
    if(period_complete&&validation_gate&&!validation_gate(day_,simulation_cycle_)){logger_.message("Simulation mise en pause après la validation humaine.");break;}
    if(delay_ms>0) std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
  }
}
}
