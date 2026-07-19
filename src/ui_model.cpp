#include "autopoiesis/ui_model.hpp"
#include <array>
#include <cmath>

namespace apo {
namespace {
constexpr std::array<float,5> simulation_speeds{0.25F,0.5F,1.0F,2.0F,4.0F};
}

void SimulationSpeedControl::slower() { level_=std::max(0,level_-1); }
void SimulationSpeedControl::faster() { level_=std::min(4,level_+1); }
float SimulationSpeedControl::multiplier() const { return simulation_speeds[level_]; }
int SimulationSpeedControl::render_stride() const {
  return multiplier()>=1.0F?static_cast<int>(multiplier()):1;
}
int SimulationSpeedControl::target_fps() const {
  return multiplier()<1.0F?static_cast<int>(60.0F*multiplier()):60;
}

std::string mood_for(const Agent& agent) {
  if(!agent.alive)return "Sans vie";
  if(agent.sleeping_days>0)return "Repos profond";
  if(agent.health<=30)return "Inquiétude pour sa santé";
  if(agent.thirst>=75)return "Tension liée à la soif";
  if(agent.hunger>=75)return "Tension liée à la faim";
  if(agent.fatigue>=75)return "Épuisement";
  if(agent.project.status==ProjectStatus::Blocked)return "Frustration face au projet";
  if(agent.boredom>=70)return "Lassitude";
  if(agent.boredom<=20)return "Engagement";
  return "État stable";
}

UiSnapshot make_ui_snapshot(const CalendarDate& date, int simulation_cycle,
                            const ClimateState& climate, const World& world,
                            const std::vector<Agent>& agents,
                            const std::vector<std::string>& recent_events,
                            int cycle_in_day, int cycles_per_day) {
  UiSnapshot snapshot;
  snapshot.date=date;
  snapshot.simulation_cycle=simulation_cycle;
  snapshot.climate=climate;
  snapshot.cycle_in_day=cycle_in_day;
  snapshot.cycles_per_day=cycles_per_day;
  snapshot.phase=day_phase_for(cycle_in_day,cycles_per_day);
  snapshot.width=World::width;
  snapshot.height=World::height;
  snapshot.cells.reserve(static_cast<std::size_t>(World::width*World::height));
  for(int y=0;y<World::height;++y)for(int x=0;x<World::width;++x){
    const Position position{x,y};
    snapshot.cells.push_back({position,world.terrain(position),world.food(position),
                              world.wood(position),world.fibers(position),
                              world.shelter_level(position),world.branches(position),
                              world.campfire(position),world.stored_food(position),
                              world.stored_wood(position),world.stored_branches(position)});
  }
  snapshot.agents.reserve(agents.size());
  for(const auto& agent:agents)
    snapshot.agents.push_back({agent,mood_for(agent),available_actions(agent,world,agents)});
  snapshot.animals=world.animals();
  snapshot.recent_events=recent_events;
  return snapshot;
}

std::optional<Position> map_position_at_pixel(const MapViewport& viewport, float pixel_x,
                                               float pixel_y, int map_width, int map_height) {
  if(map_width<=0||map_height<=0||viewport.width<=0||viewport.height<=0||
     pixel_x<viewport.x||pixel_y<viewport.y||pixel_x>=viewport.x+viewport.width||
     pixel_y>=viewport.y+viewport.height)return std::nullopt;
  const int x=static_cast<int>(std::floor((pixel_x-viewport.x)/(viewport.width/map_width)));
  const int y=static_cast<int>(std::floor((pixel_y-viewport.y)/(viewport.height/map_height)));
  if(x<0||x>=map_width||y<0||y>=map_height)return std::nullopt;
  return Position{x,y};
}

const UiAgent* agent_at_position(const UiSnapshot& snapshot, Position position) {
  for(const auto& agent:snapshot.agents)
    if(agent.state.alive&&agent.state.position==position)return &agent;
  return nullptr;
}

int simulation_delay_from_slider(float pixel_x,float track_start,float track_end) {
  if(track_end<=track_start)return 0;
  const float ratio=std::clamp((pixel_x-track_start)/(track_end-track_start),0.0F,1.0F);
  return static_cast<int>(std::lround(ratio*100.0F))*100;
}
}
