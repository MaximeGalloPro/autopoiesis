#include "autopoiesis/simulation.hpp"
#include "autopoiesis/renderer.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <thread>

namespace apo {
static int report_interval_from_env() {
  const char* configured=std::getenv("REPORT_EVERY_CYCLES");
  if(!configured) return 1;
  try {
    int interval=std::stoi(configured);
    return interval>0 ? interval : 1;
  } catch(...) {
    return 1;
  }
}

static bool feature_requests_required() {
  const char* configured=std::getenv("FEATURE_REQUESTS_REQUIRED");
  return !configured || std::string(configured)!="0";
}

Decision LocalDecider::decide(const Perception& p) {
  const auto& me=p.value["self"];
  auto acts=p.value["available_actions"].get<std::vector<std::string>>();
  if(me["hunger"]>=75&&std::find(acts.begin(),acts.end(),"hunt_rabbit")!=acts.end()) return {DecisionType::Action,"hunt_rabbit",json::object(),"I need food","food","","be fed"};
  if(me["hunger"]>=75&&std::find(acts.begin(),acts.end(),"eat_berries")!=acts.end()) return {DecisionType::Action,"eat_berries",json::object(),"I need food","food","","be fed"};
  if(me["fatigue"]>=75) return {DecisionType::Action,"sleep",json::object(),"I need rest","rest","","be rested"};
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
  static const std::vector<std::string> dirs{"north","south","east","west"};
  return {DecisionType::Action,"move",{{"direction",dirs[std::uniform_int_distribution<size_t>(0,3)(rng_)]}},me["hunger"]>=60?"I explore to find food":"I explore","exploration","","discover the world"};
}

static bool action_succeeded(const Decision& decision, const std::string& result) {
  if (decision.type == DecisionType::Blocked) return false;
  if (decision.action == "move") return result != "deplacement bloque";
  if (decision.action == "eat_berries") return result == "mange des baies";
  if (decision.action == "hunt_rabbit") return result == "chasse le lapin";
  return true;
}

Simulation::Simulation(unsigned seed,IDecider& d,Logger& l,ICycleReporter* reporter):world_(seed),decider_(d),logger_(l),reporter_(reporter),rng_(seed),report_every_cycles_(report_interval_from_env()){
  agents_={{"a1","Ada",{3,2},100,45,20,{90,20,30,30,40}},{"a2","Borin",{10,5},100,55,15,{25,85,70,90,40}},{"a3","Cyra",{16,7},100,35,60,{40,45,95,55,90}}};
}

Perception Simulation::perceive(Agent& a) {
  json cells=json::array();
  for(int dy=-3;dy<=3;++dy) for(int dx=-3;dx<=3;++dx){Position p{a.position.x+dx,a.position.y+dy};if(std::abs(dx)+std::abs(dy)<=3&&world_.in_bounds(p)){auto terrain=world_.terrain(p);a.remember_map(p,terrain);cells.push_back({{"x",p.x},{"y",p.y},{"terrain",static_cast<int>(terrain)},{"rabbit",world_.rabbit_alive()&&p==world_.rabbit()}});}}
  json known=json::array();for(const auto&[position,terrain]:a.map_memory)known.push_back({{"x",position.first},{"y",position.second},{"terrain",static_cast<int>(terrain)}});
  json visible=json::array();for(const auto&o:agents_)if(o.alive&&o.id!=a.id&&std::abs(o.position.x-a.position.x)+std::abs(o.position.y-a.position.y)<=3)visible.push_back({{"id",o.id},{"name",o.name},{"x",o.position.x},{"y",o.position.y}});
  json mem=json::array();for(const auto&s:a.memories)mem.push_back(s);
  return Perception{json{{"self",{{"id",a.id},{"name",a.name},{"x",a.position.x},{"y",a.position.y},{"health",a.health},{"hunger",a.hunger},{"fatigue",a.fatigue},{"personality",personality_json(a.personality)}}},{"cells",cells},{"known_map",known},{"visible_agents",visible},{"memories",mem},{"available_actions",available_actions(a,world_,agents_)}}};
}

void Simulation::advance_action_needs(Agent& a,int action_index){if(action_index%80==0)a.hunger=clamp_stat(a.hunger+1);if(action_index%12==0)a.fatigue=clamp_stat(a.fatigue+1);}
void Simulation::update_needs(Agent& a){if(a.hunger>=90){++a.critical_hunger_cycles;if(a.critical_hunger_cycles>3)a.health=clamp_stat(a.health-5);}else a.critical_hunger_cycles=0;if(a.health==0)a.alive=false;}

std::string Simulation::execute(Agent&a,const Decision&d){
  if(d.type==DecisionType::Blocked){a.remember("I reported a blockage: "+d.obstacle);return "signale un blocage : "+d.obstacle;}
  if(d.action=="wait"||d.action=="observe"){a.remember(d.action=="wait"?"J'ai attendu.":"J'ai observe mon environnement.");return "attend";}
  if(d.action=="sleep"){a.sleeping_cycles=2;a.fatigue=clamp_stat(a.fatigue-20);a.remember("Je commence a dormir.");return "commence a dormir";}
  if(d.action=="eat_berries"){if(world_.eat_berries(a.position)){a.hunger=clamp_stat(a.hunger-35);return "mange des baies";}return "ne peut pas manger ici";}
  if(d.action=="hunt_rabbit"){if(world_.hunt_rabbit(a.position)){a.hunger=clamp_stat(a.hunger-35);a.remember("J'ai chasse le lapin.");return "chasse le lapin";}return "ne peut pas chasser ici";}
  if(d.action=="move"){auto dir=d.parameters["direction"].get<std::string>();Position p=a.position;if(dir=="north")--p.y;if(dir=="south")++p.y;if(dir=="east")++p.x;if(dir=="west")--p.x;if(world_.passable(p)){a.position=p;a.remember("Je me suis deplace vers "+dir+".");return "se deplace vers "+dir;}a.remember("Mon deplacement vers "+dir+" a ete bloque par un obstacle.");return "deplacement bloque";}
  if(d.action=="talk"){auto id=d.parameters["target_agent_id"].get<std::string>();for(auto&o:agents_)if(o.id==id){std::string msg=d.parameters.value("message","Bonjour.");a.remember("J'ai parle a "+o.name+" : "+msg);o.remember(a.name+" m'a parle : "+msg);return "parle a "+o.name;}}
  return "attend";
}

void Simulation::cycle(){
  ++cycle_; world_.advance_nature(rng_);
  for(auto& agent:agents_) if(agent.alive&&agent.sleeping_cycles>0){--agent.sleeping_cycles;agent.fatigue=clamp_stat(agent.fatigue-15);logger_.message("Cycle "+std::to_string(cycle_)+" — "+agent.name+" dort.");}
  for(int action_index=0;action_index<actions_per_cycle_;++action_index) for(auto& agent:agents_) if(agent.alive&&agent.sleeping_cycles==0){
    advance_action_needs(agent,action_index); Agent before=agent; Decision d=decider_.decide(perceive(agent)); std::string e;
    if(!validate_decision(d,agent,world_,agents_,e)){d={DecisionType::Action,"wait",json::object(),"invalid decision"};logger_.message("Cycle "+std::to_string(cycle_)+" — "+agent.name+" decision invalide : "+e);}
    if(d.type==DecisionType::Blocked) logger_.feature_request(cycle_,agent,d); auto result=execute(agent,d); auto& history=action_history_[agent.id]; std::string entry="cycle="+std::to_string(cycle_)+" action="+(d.type==DecisionType::Blocked?"blocked":d.action)+" outcome="+(action_succeeded(d,result)?"success":"failure")+" result="+result; if(!d.reason.empty()) entry+=" reason="+d.reason; if(!d.need.empty()) entry+=" need="+d.need; if(!d.obstacle.empty()) entry+=" obstacle="+d.obstacle; if(!d.desired_result.empty()) entry+=" desired_result="+d.desired_result; history.push_back(entry); if(history.size()>1200)history.erase(history.begin()); logger_.event(cycle_,before,d,result,agent);
  }
  for(auto& agent:agents_) if(agent.alive) update_needs(agent);
}

void Simulation::run(int cycles,int delay_ms,int render_every){
  for(int i=0;i<cycles;++i){
    cycle();
    if(reporter_&&cycle_%report_every_cycles_==0) for(auto& agent:agents_){auto& history=action_history_[agent.id];auto report=reporter_->report_cycle(cycle_,agent,history);if(!report.is_null()){logger_.ai_report(cycle_,agent,report);if(feature_requests_required()||report.value("ask_god",false)){auto request=reporter_->request_evolution(cycle_,agent,history,report);if(!request.is_null())logger_.ai_feature_request(cycle_,agent,report,request);}}history.clear();}
    bool all_dead=std::none_of(agents_.begin(),agents_.end(),[](const Agent&a){return a.alive;}); if(all_dead)logger_.message("Simulation arrêtée : tous les personnages sont morts."); if((render_every>0&&cycle_%render_every==0)||all_dead)render(cycle_,world_,agents_,logger_); if(all_dead)break; if(delay_ms>0)std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
  }
}
}
