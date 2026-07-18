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
  if(me["hunger"]>=75&&std::find(acts.begin(),acts.end(),"hunt_rabbit")!=acts.end()) return {DecisionType::Action,"hunt_rabbit",json::object(),"I need food","food","","be fed"};
  if(me["hunger"]>=40&&std::find(acts.begin(),acts.end(),"eat_berries")!=acts.end()) return {DecisionType::Action,"eat_berries",json::object(),"I need food","food","","be fed"};
  if(me["hunger"]>=40&&std::find(acts.begin(),acts.end(),"hunt_rabbit")!=acts.end()) return {DecisionType::Action,"hunt_rabbit",json::object(),"I need food","food","","be fed"};
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
  if(repetition&&me["hunger"]>=70){
    const auto& known=p.value["known_map"];
    Position self{me.value("x",0),me.value("y",0)};
    std::map<std::pair<int,int>,const json*> traversable;
    for(const auto& cell:known) if(cell.value("status","")=="traversable")
      traversable[{cell.value("x",0),cell.value("y",0)}]=&cell;
    std::queue<Position> pending;
    std::map<std::pair<int,int>,std::pair<int,std::string>> reached;
    pending.push(self); reached[{self.x,self.y}]={0,""};
    static const std::vector<std::pair<std::string,Position>> loop_dirs{
      {"north",{0,-1}},{"east",{1,0}},{"south",{0,1}},{"west",{-1,0}}};
    while(!pending.empty()){
      Position current=pending.front(); pending.pop();
      const auto current_info=reached.at({current.x,current.y});
      for(const auto&[direction,offset]:loop_dirs){
        Position next{current.x+offset.x,current.y+offset.y};
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
    for(const auto&[direction,offset]:loop_dirs){
      Position next{self.x+offset.x,self.y+offset.y};
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
  if(me["fatigue"]>=75) return {DecisionType::Action,"sleep",json::object(),"I need rest","rest","","be rested"};
  if(me["fatigue"]>=45&&std::find(acts.begin(),acts.end(),"rest")!=acts.end()) return {DecisionType::Action,"rest",json::object(),"I need rest","rest","","be rested"};
  if(me["hunger"]>=60){
    const auto& cells=p.value["cells"];
    Position self{me.value("x",0),me.value("y",0)};
    int best_distance=std::numeric_limits<int>::max();
    std::string best_direction;
    for(const auto& cell:cells){
      if(!cell.value("rabbit",false)&&cell.value("terrain",-1)!=static_cast<int>(Terrain::Bush)) continue;
      Position target{cell.value("x",self.x),cell.value("y",self.y)};
      int dx=target.x-self.x,dy=target.y-self.y;
      int distance=std::abs(dx)+std::abs(dy);
      if(distance==0||distance>=best_distance) continue;
      std::vector<std::pair<std::string,Position>> candidates;
      if(dx>0)candidates.push_back({"east",{self.x+1,self.y}});else if(dx<0)candidates.push_back({"west",{self.x-1,self.y}});
      if(dy>0)candidates.push_back({"south",{self.x,self.y+1}});else if(dy<0)candidates.push_back({"north",{self.x,self.y-1}});
      for(const auto&[direction,step]:candidates){
        bool known_passable=false;
        for(const auto& neighbor:cells) if(neighbor.value("x",-1)==step.x&&neighbor.value("y",-1)==step.y){int terrain=neighbor.value("terrain",-1);known_passable=terrain==static_cast<int>(Terrain::Ground)||terrain==static_cast<int>(Terrain::Bush);break;}
        if(known_passable){best_distance=distance;best_direction=direction;break;}
      }
    }
    if(!best_direction.empty()) return {DecisionType::Action,"move",{{"direction",best_direction}},"I seek food","food","","be fed"};
  }
  static const std::vector<std::pair<std::string,Position>> dirs{
      {"north",{0,-1}},{"east",{1,0}},{"south",{0,1}},{"west",{-1,0}}};
  const auto& known=p.value["known_map"];
  int lowest_visits=std::numeric_limits<int>::max();
  std::string selected;
  Position self{me.value("x",0),me.value("y",0)};
  for(const auto&[direction,offset]:dirs){
    Position adjacent{self.x+offset.x,self.y+offset.y};
    int visits=0;
    bool excluded=false;
    for(const auto& cell:known){
      if(cell.value("x",-1)!=adjacent.x||cell.value("y",-1)!=adjacent.y) continue;
      const std::string status=cell.value("status","unknown");
      excluded=status=="blocked"||status=="out_of_bounds";
      visits=cell.value("visit_count",0);
      break;
    }
    if(!excluded&&visits<lowest_visits){lowest_visits=visits;selected=direction;}
  }
  if(selected.empty()) return {DecisionType::Action,"wait",json::object(),"No eligible exploration direction","exploration","","discover the world"};
  return {DecisionType::Action,"move",{{"direction",selected}},me["hunger"]>=60?"I explore to find food":"I explore","exploration","","discover the world"};
}

static bool action_succeeded(const Decision& decision, const std::string& result) {
  if (decision.type == DecisionType::Blocked) return false;
  if (decision.action == "move") return result != "deplacement bloque";
  if (decision.action == "eat_berries") return result == "mange des baies";
  if (decision.action == "hunt_rabbit") return result == "chasse le lapin";
  return true;
}

Simulation::Simulation(unsigned seed,IDecider& d,Logger& l,ICycleReporter* reporter):world_(seed),decider_(d),logger_(l),reporter_(reporter),rng_(seed),cycles_per_day_(positive_int_from_env("CYCLES_PER_DAY",240)),report_every_days_(positive_int_from_env("REPORT_EVERY_DAYS",3)){
  agents_={{"a1","Ada",{3,2},100,45,20,{90,20,30,30,40}},{"a2","Borin",{10,5},100,55,15,{25,85,70,90,40}},{"a3","Cyra",{16,7},100,35,60,{40,45,95,55,90}}};
}

Perception Simulation::perceive(Agent& a) {
  json known=json::array();for(const auto&[position,terrain]:a.map_memory){Position p{position.first,position.second};bool traversable=terrain==Terrain::Ground||terrain==Terrain::Bush;known.push_back({{"x",position.first},{"y",position.second},{"terrain",static_cast<int>(terrain)},{"status",!world_.in_bounds(p)?"out_of_bounds":traversable?"traversable":"blocked"},{"visit_count",a.map_visit_counts[position]},{"food",world_.berries(p)}});}
  json cells=json::array();
  for(int dy=-3;dy<=3;++dy) for(int dx=-3;dx<=3;++dx){Position p{a.position.x+dx,a.position.y+dy};if(std::abs(dx)+std::abs(dy)<=3&&world_.in_bounds(p)){auto terrain=world_.terrain(p);a.remember_map(p,terrain);cells.push_back({{"x",p.x},{"y",p.y},{"terrain",static_cast<int>(terrain)},{"rabbit",world_.rabbit_alive()&&p==world_.rabbit()}});}}
  json visible=json::array();for(const auto&o:agents_)if(o.alive&&o.id!=a.id&&std::abs(o.position.x-a.position.x)+std::abs(o.position.y-a.position.y)<=3)visible.push_back({{"id",o.id},{"name",o.name},{"x",o.position.x},{"y",o.position.y}});
  json mem=json::array();for(const auto&s:a.memories)mem.push_back(s);
  return Perception{json{{"self",{{"id",a.id},{"name",a.name},{"x",a.position.x},{"y",a.position.y},{"health",a.health},{"hunger",a.hunger},{"fatigue",a.fatigue},{"personality",personality_json(a.personality)}}},{"cells",cells},{"known_map",known},{"action_history",planning_history_[a.id]},{"visible_agents",visible},{"memories",mem},{"available_actions",available_actions(a,world_,agents_)}}};
}

void Simulation::advance_action_needs(Agent& a,int action_index){if(action_index%80==0)a.hunger=clamp_stat(a.hunger+1);if(action_index%12==0)a.fatigue=clamp_stat(a.fatigue+1);}
void Simulation::update_needs(Agent& a){if(a.hunger>=90){++a.critical_hunger_days;if(a.critical_hunger_days>3)a.health=clamp_stat(a.health-5);}else a.critical_hunger_days=0;if(a.health==0)a.alive=false;}

std::string Simulation::execute(Agent&a,const Decision&d){
  if(d.type==DecisionType::Blocked){a.remember("I reported a blockage: "+d.obstacle);return "signale un blocage : "+d.obstacle;}
  if(d.action=="wait"||d.action=="observe"){a.remember(d.action=="wait"?"J'ai attendu.":"J'ai observe mon environnement.");return "attend";}
  if(d.action=="rest"){if(!a.alive||a.fatigue<=0||!world_.passable(a.position))return "attend";a.fatigue=clamp_stat(a.fatigue-20);return "rested";}
  if(d.action=="sleep"){a.sleeping_days=2;a.fatigue=clamp_stat(a.fatigue-20);a.remember("Je commence a dormir.");return "commence a dormir";}
  if(d.action=="eat_berries"){if(world_.eat_berries(a.position)){a.hunger=clamp_stat(a.hunger-35);return "mange des baies";}return "ne peut pas manger ici";}
  if(d.action=="hunt_rabbit"){if(world_.hunt_rabbit(a.position)){a.hunger=clamp_stat(a.hunger-35);a.remember("J'ai chasse le lapin.");return "chasse le lapin";}return "ne peut pas chasser ici";}
  if(d.action=="move"){auto dir=d.parameters["direction"].get<std::string>();Position p=a.position;if(dir=="north")--p.y;if(dir=="south")++p.y;if(dir=="east")++p.x;if(dir=="west")--p.x;if(world_.passable(p)){a.position=p;a.remember_map(p,world_.terrain(p));++a.map_visit_counts[{p.x,p.y}];a.remember("Je me suis deplace vers "+dir+".");return "se deplace vers "+dir;}a.remember_map(p,world_.terrain(p));a.remember("Mon deplacement vers "+dir+" a ete bloque par un obstacle.");return "deplacement bloque";}
  if(d.action=="talk"){auto id=d.parameters["target_agent_id"].get<std::string>();for(auto&o:agents_)if(o.id==id){std::string msg=d.parameters.value("message","Bonjour.");a.remember("J'ai parle a "+o.name+" : "+msg);o.remember(a.name+" m'a parle : "+msg);return "parle a "+o.name;}}
  return "attend";
}

void Simulation::run_day(){
  ++day_; world_.advance_nature(rng_);
  for(auto& agent:agents_) if(agent.alive&&agent.sleeping_days>0){--agent.sleeping_days;agent.fatigue=clamp_stat(agent.fatigue-15);logger_.message("Jour "+std::to_string(day_)+" / cycle elementaire "+std::to_string(simulation_cycle_)+" — "+agent.name+" dort.");}
  for(int action_index=0;action_index<cycles_per_day_;++action_index){
    ++simulation_cycle_;
    for(auto& agent:agents_) if(agent.alive&&agent.sleeping_days==0){
    advance_action_needs(agent,action_index); Agent before=agent; Decision d=decider_.decide(perceive(agent)); std::string e;
    if(!validate_decision(d,agent,world_,agents_,e)){d={DecisionType::Action,"wait",json::object(),"invalid decision"};logger_.message("Jour "+std::to_string(day_)+" / cycle elementaire "+std::to_string(simulation_cycle_)+" — "+agent.name+" decision invalide : "+e);}
    if(d.type==DecisionType::Blocked) logger_.feature_request(simulation_cycle_,day_,agent,d); auto result=execute(agent,d); const bool succeeded=action_succeeded(d,result); auto& planning=planning_history_[agent.id]; planning.push_back({{"action",d.type==DecisionType::Blocked?"blocked":d.action},{"outcome",succeeded?"success":"failure"},{"x",agent.position.x},{"y",agent.position.y}}); while(planning.size()>20)planning.erase(planning.begin()); auto& history=action_history_[agent.id]; std::string entry="day="+std::to_string(day_)+" simulation_cycle="+std::to_string(simulation_cycle_)+" action="+(d.type==DecisionType::Blocked?"blocked":d.action)+" outcome="+(succeeded?"success":"failure")+" result="+result; if(!d.reason.empty()) entry+=" reason="+d.reason; if(!d.need.empty()) entry+=" need="+d.need; if(!d.obstacle.empty()) entry+=" obstacle="+d.obstacle; if(!d.desired_result.empty()) entry+=" desired_result="+d.desired_result; history.push_back(entry); if(history.size()>1200)history.erase(history.begin()); logger_.event(simulation_cycle_,day_,before,d,result,agent);
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
