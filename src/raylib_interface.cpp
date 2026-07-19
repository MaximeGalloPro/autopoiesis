#include "autopoiesis/raylib_interface.hpp"
#include "raylib.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace apo {
namespace {
constexpr Color background{24,26,27,255};
constexpr Color panel{34,37,38,255};
constexpr Color divider{70,74,73,255};
constexpr Color primary_text{235,237,233,255};
constexpr Color secondary_text{166,172,166,255};
constexpr Color accent{230,180,72,255};

Color terrain_color(Terrain terrain) {
  switch(terrain){
    case Terrain::Wall:return {91,86,82,255};
    case Terrain::Water:return {53,122,153,255};
    case Terrain::Tree:return {38,92,59,255};
    case Terrain::Bush:return {104,126,54,255};
    case Terrain::Ground:return {74,91,65,255};
  }
  return MAGENTA;
}

Color agent_color(const std::string& id) {
  if(id=="a1")return {235,191,78,255};
  if(id=="a2")return {213,101,77,255};
  return {92,185,174,255};
}

std::string action_label(const std::string& action) {
  if(action=="observe")return "Observer";
  if(action=="wait")return "Attendre";
  if(action=="move")return "Se déplacer";
  if(action=="sleep")return "Dormir";
  if(action=="rest")return "Se reposer";
  if(action=="drink")return "Boire";
  if(action=="eat_food"||action=="eat_berries")return "Manger";
  if(action=="hunt_animal"||action=="hunt_rabbit")return "Chasser";
  if(action=="talk")return "Échanger";
  if(action=="harvest_wood")return "Prélever du bois";
  if(action=="assemble_shelter"||action=="build_shelter")return "Construire un abri";
  return action;
}

std::string status_label(ProjectStatus status) {
  switch(status){
    case ProjectStatus::Candidate:return "candidat";
    case ProjectStatus::Active:return "actif";
    case ProjectStatus::Blocked:return "bloqué";
    case ProjectStatus::Completed:return "terminé";
    case ProjectStatus::Abandoned:return "abandonné";
  }
  return "inconnu";
}

std::string clipped(std::string text, int maximum_width, int font_size) {
  if(MeasureText(text.c_str(),font_size)<=maximum_width)return text;
  while(text.size()>3&&MeasureText((text+"...").c_str(),font_size)>maximum_width){
    std::size_t start=text.size()-1;
    while(start>0&&(static_cast<unsigned char>(text[start])&0xC0)==0x80)--start;
    text.erase(start);
  }
  return text+"...";
}

std::vector<std::string> action_lines(const std::vector<std::string>& actions, int maximum_width,
                                      int font_size) {
  std::vector<std::string> lines;
  for(const auto& action:actions){
    const auto label=action_label(action);
    const auto candidate=lines.empty()||lines.back().empty()?label:lines.back()+"  "+label;
    if(lines.empty()||MeasureText(candidate.c_str(),font_size)>maximum_width){
      if(lines.size()==2)break;
      lines.push_back(label);
    }else lines.back()=candidate;
  }
  return lines;
}

void draw_stat(const char* label, int value, int x, int y, int width, Color color) {
  DrawText(label,x,y,15,secondary_text);
  DrawText(TextFormat("%d",value),x+width-30,y,15,primary_text);
  DrawRectangle(x,y+20,width,6,{58,62,61,255});
  DrawRectangle(x,y+20,static_cast<int>(width*std::clamp(value,0,100)/100.0F),6,color);
}

MapViewport calculate_map_viewport(int screen_width, int screen_height) {
  constexpr float left=24.0F;
  constexpr float top=92.0F;
  const float right_panel=std::clamp(screen_width*0.29F,330.0F,390.0F);
  const float available_width=screen_width-right_panel-left-38.0F;
  const float available_height=screen_height-top-112.0F;
  const float cell=std::max(8.0F,std::floor(std::min(available_width/World::width,
                                                    available_height/World::height)));
  return {left,top,cell*World::width,cell*World::height};
}
}

struct RaylibInterface::Impl {
  UiSnapshot snapshot;
  bool has_snapshot{};
  std::string selected_agent_id;
  std::string screenshot_path;
  bool screenshot_taken{};

  Impl() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT|FLAG_WINDOW_HIGHDPI);
    InitWindow(1280,760,"Autopoiesis");
    SetWindowMinSize(1040,680);
    SetTargetFPS(60);
    if(const char* configured=std::getenv("AUTOPOIESIS_GUI_SCREENSHOT"))screenshot_path=configured;
  }

  const UiAgent* selected_agent() const {
    for(const auto& agent:snapshot.agents)if(agent.state.id==selected_agent_id)return &agent;
    return snapshot.agents.empty()?nullptr:&snapshot.agents.front();
  }

  void handle_input() {
    if(!has_snapshot||!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))return;
    const auto mouse=GetMousePosition();
    const auto position=map_position_at_pixel(calculate_map_viewport(GetScreenWidth(),GetScreenHeight()),
                                               mouse.x,mouse.y,snapshot.width,snapshot.height);
    if(!position)return;
    if(const auto* agent=agent_at_position(snapshot,*position))selected_agent_id=agent->state.id;
  }

  void draw_map(const MapViewport& viewport) const {
    const float cell_width=viewport.width/snapshot.width;
    const float cell_height=viewport.height/snapshot.height;
    DrawRectangle(static_cast<int>(viewport.x-2),static_cast<int>(viewport.y-2),
                  static_cast<int>(viewport.width+4),static_cast<int>(viewport.height+4),divider);
    for(const auto& cell:snapshot.cells){
      const int x=static_cast<int>(viewport.x+cell.position.x*cell_width);
      const int y=static_cast<int>(viewport.y+cell.position.y*cell_height);
      DrawRectangle(x,y,static_cast<int>(std::ceil(cell_width)),static_cast<int>(std::ceil(cell_height)),
                    terrain_color(cell.terrain));
      if(cell.food>0)DrawCircle(x+static_cast<int>(cell_width*0.72F),y+static_cast<int>(cell_height*0.28F),
                                std::max(2.0F,cell_width*0.10F),{232,203,92,255});
      if(cell.shelter_level>0){
        DrawRectangle(x+3,y+3,std::max(4,static_cast<int>(cell_width)-6),
                      std::max(4,static_cast<int>(cell_height)-6),{151,105,67,255});
        DrawRectangleLines(x+3,y+3,std::max(4,static_cast<int>(cell_width)-6),
                           std::max(4,static_cast<int>(cell_height)-6),{238,213,171,255});
      }
      DrawRectangleLines(x,y,static_cast<int>(std::ceil(cell_width)),
                         static_cast<int>(std::ceil(cell_height)),{34,40,36,75});
    }
    for(const auto& animal:snapshot.animals){
      if(!animal.alive)continue;
      const int x=static_cast<int>(viewport.x+(animal.position.x+0.5F)*cell_width);
      const int y=static_cast<int>(viewport.y+(animal.position.y+0.5F)*cell_height);
      const Color color=animal.danger>=60?Color{190,68,62,255}:Color{214,215,190,255};
      DrawCircle(x,y,std::max(2.5F,cell_width*0.18F),color);
    }
    for(const auto& agent:snapshot.agents){
      if(!agent.state.alive)continue;
      const int x=static_cast<int>(viewport.x+(agent.state.position.x+0.5F)*cell_width);
      const int y=static_cast<int>(viewport.y+(agent.state.position.y+0.5F)*cell_height);
      const float radius=std::max(5.0F,cell_width*0.34F);
      DrawCircle(x,y,radius,agent_color(agent.state.id));
      if(agent.state.id==selected_agent_id)DrawCircleLines(x,y,radius+3,primary_text);
      const std::string initial=agent.state.name.empty()?"?":agent.state.name.substr(0,1);
      const int font=std::max(10,static_cast<int>(cell_width*0.48F));
      DrawText(initial.c_str(),x-MeasureText(initial.c_str(),font)/2,y-font/2,font,background);
    }
  }

  void draw_inspector(int x, int y, int width, int height) const {
    DrawRectangle(x,76,width,height-76,panel);
    const auto* selected=selected_agent();
    if(!selected){DrawText("Aucun personnage",x+22,y+20,18,secondary_text);return;}
    const auto& agent=selected->state;
    int cursor=y+18;
    DrawText(agent.name.c_str(),x+22,cursor,26,primary_text);
    DrawText(clipped(agent.behavior.archetype,width-44,15).c_str(),x+22,cursor+32,15,accent);
    cursor+=66;
    DrawText("HUMEUR",x+22,cursor,13,secondary_text);
    DrawText(clipped(selected->mood,width-44,17).c_str(),x+22,cursor+20,17,primary_text);
    cursor+=56;
    const int bar_width=width-44;
    draw_stat("Santé",agent.health,x+22,cursor,bar_width,{86,184,111,255});cursor+=38;
    draw_stat("Faim",agent.hunger,x+22,cursor,bar_width,{220,151,64,255});cursor+=38;
    draw_stat("Soif",agent.thirst,x+22,cursor,bar_width,{62,151,194,255});cursor+=38;
    draw_stat("Fatigue",agent.fatigue,x+22,cursor,bar_width,{189,107,157,255});cursor+=48;
    DrawText("PROJET",x+22,cursor,13,secondary_text);cursor+=20;
    DrawText(clipped(agent.project.title,width-44,16).c_str(),x+22,cursor,16,primary_text);cursor+=22;
    DrawText(TextFormat("%s  %d/%d",status_label(agent.project.status).c_str(),
                        agent.project.progress,agent.project.target),x+22,cursor,14,secondary_text);cursor+=34;
    DrawText("ACTIONS DISPONIBLES",x+22,cursor,13,secondary_text);cursor+=20;
    const auto actions=action_lines(selected->available_actions,width-44,14);
    for(const auto& line:actions){DrawText(line.c_str(),x+22,cursor,14,primary_text);cursor+=18;}
    cursor+=16;
    DrawText("ATTRIBUTS",x+22,cursor,13,secondary_text);cursor+=21;
    const std::array<std::pair<const char*,int>,10> attributes{{
      {"Force",agent.attributes.strength},{"Agilité",agent.attributes.agility},
      {"Endurance",agent.attributes.endurance},{"Robustesse",agent.attributes.toughness},
      {"Récupération",agent.attributes.recuperation},{"Résistance",agent.attributes.disease_resistance},
      {"Concentration",agent.attributes.focus},{"Volonté",agent.attributes.willpower},
      {"Mémoire",agent.attributes.memory},{"Sens spatial",agent.attributes.spatial_sense}}};
    for(std::size_t index=0;index<attributes.size();++index){
      const int column=static_cast<int>(index%2),row=static_cast<int>(index/2);
      const int column_x=x+22+column*(width-44)/2;
      DrawText(TextFormat("%s %d",attributes[index].first,attributes[index].second),
               column_x,cursor+row*21,13,index%2==0?primary_text:secondary_text);
    }
    cursor+=112;
    DrawText(TextFormat("Bois : %d   Monotonie : %d",agent.wood_inventory,agent.boredom),
             x+22,std::min(cursor,height-28),13,secondary_text);
  }

  void draw() const {
    BeginDrawing();
    ClearBackground(background);
    if(!has_snapshot){
      DrawText("AUTOPOIESIS",32,30,28,primary_text);
      DrawText("Initialisation de la simulation...",32,78,18,secondary_text);
      EndDrawing();return;
    }
    const int screen_width=GetScreenWidth(),screen_height=GetScreenHeight();
    const auto viewport=calculate_map_viewport(screen_width,screen_height);
    const int panel_x=static_cast<int>(viewport.x+viewport.width+22);
    DrawRectangle(0,0,screen_width,76,{30,33,34,255});
    DrawText("AUTOPOIESIS",24,18,24,primary_text);
    DrawText(TextFormat("Année %d · Mois %d · Jour %d · %s",snapshot.date.year,snapshot.date.month,
                        snapshot.date.day_of_month,season_name(snapshot.date.season).c_str()),
             210,17,19,primary_text);
    DrawText(TextFormat("%d °C · %s · jour absolu %d · cycle %d",snapshot.climate.temperature_c,
                        snapshot.climate.condition.c_str(),snapshot.date.absolute_day,
                        snapshot.simulation_cycle),210,44,14,secondary_text);
    draw_map(viewport);
    const int legend_y=static_cast<int>(viewport.y+viewport.height+23);
    DrawText("Terrain",static_cast<int>(viewport.x),legend_y-7,14,secondary_text);
    int legend_x=static_cast<int>(viewport.x+84);
    DrawCircle(legend_x,legend_y,6,agent_color("a1"));
    DrawText("personnage",legend_x+12,legend_y-7,14,secondary_text);legend_x+=112;
    DrawCircle(legend_x,legend_y,4,{214,215,190,255});
    DrawText("animal",legend_x+10,legend_y-7,14,secondary_text);legend_x+=82;
    DrawCircle(legend_x,legend_y,3,{232,203,92,255});
    DrawText("nourriture",legend_x+9,legend_y-7,14,secondary_text);legend_x+=110;
    DrawCircleLines(legend_x,legend_y,7,primary_text);
    DrawText("sélection",legend_x+12,legend_y-7,14,secondary_text);
    int event_y=static_cast<int>(viewport.y+viewport.height+44);
    DrawText("ÉVÉNEMENTS RÉCENTS",static_cast<int>(viewport.x),event_y,13,secondary_text);
    if(!snapshot.recent_events.empty())
      DrawText(clipped(snapshot.recent_events.back(),static_cast<int>(viewport.width),14).c_str(),
               static_cast<int>(viewport.x),event_y+21,14,primary_text);
    draw_inspector(panel_x,76,screen_width-panel_x,screen_height);
    EndDrawing();
  }

  void capture_if_requested() {
    if(screenshot_taken||screenshot_path.empty())return;
    Image framebuffer=LoadImageFromScreen();
    ExportImage(framebuffer,screenshot_path.c_str());
    UnloadImage(framebuffer);
    screenshot_taken=true;
  }
};

RaylibInterface::RaylibInterface():impl_(std::make_unique<Impl>()) { impl_->draw(); }
RaylibInterface::~RaylibInterface() { if(IsWindowReady())CloseWindow(); }

bool RaylibInterface::present(const UiSnapshot& snapshot) {
  const bool already_had_snapshot=impl_->has_snapshot;
  impl_->snapshot=snapshot;
  impl_->has_snapshot=true;
  if(impl_->selected_agent_id.empty()&&!snapshot.agents.empty())
    impl_->selected_agent_id=snapshot.agents.front().state.id;
  impl_->handle_input();
  impl_->draw();
  if(already_had_snapshot)impl_->capture_if_requested();
  return !WindowShouldClose();
}

bool RaylibInterface::idle_for(int milliseconds) {
  const double deadline=GetTime()+std::max(0,milliseconds)/1000.0;
  do{
    if(WindowShouldClose())return false;
    impl_->handle_input();
    impl_->draw();
    impl_->capture_if_requested();
  }while(GetTime()<deadline);
  return !WindowShouldClose();
}
}
