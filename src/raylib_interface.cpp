#include "autopoiesis/raylib_interface.hpp"
#include "raylib.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <sstream>

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
  if(action=="collect_branch")return "Ramasser une branche";
  if(action=="collect_iron_ore")return "Ramasser du fer";
  if(action=="build_campfire")return "Allumer un feu";
  if(action=="rest_by_campfire")return "Rester près du feu";
  if(action=="collect_food")return "Ramasser pour le camp";
  if(action=="deposit_food")return "Déposer au camp";
  if(action=="deposit_materials")return "Déposer les matériaux";
  if(action=="eat_carried_food")return "Manger la ration portée";
  if(action=="eat_camp_food")return "Manger à la réserve";
  if(action=="cook_camp_food")return "Cuisiner une ration";
  if(action=="craft_camp_item")return "Fabriquer un objet";
  if(action=="equip_axe")return "Équiper la hache";
  if(action=="repair_axe")return "Réparer la hache";
  if(action=="share_camp_meal")return "Partager un repas";
  if(action=="hold_vigil")return "Tenir une veillée";
  if(action=="celebrate")return "Célébrer";
  if(action=="mourn")return "Faire mémoire";
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

std::vector<std::string> wrapped_lines(const std::string& text, int maximum_width,
                                       int font_size, std::size_t maximum_lines) {
  std::vector<std::string> lines;
  std::istringstream words(text);
  std::string word;
  bool truncated=false;
  while(words>>word){
    const auto candidate=lines.empty()||lines.back().empty()?word:lines.back()+" "+word;
    if(lines.empty()||MeasureText(candidate.c_str(),font_size)>maximum_width){
      if(lines.size()==maximum_lines){truncated=true;break;}
      lines.push_back(clipped(word,maximum_width,font_size));
    }else lines.back()=candidate;
  }
  if(truncated&&!lines.empty())lines.back()=clipped(lines.back()+"...",maximum_width,font_size);
  return lines;
}

void draw_lines(const std::vector<std::string>& lines, int x, int& y, int font_size,
                int spacing, Color color) {
  for(const auto& line:lines){DrawText(line.c_str(),x,y,font_size,color);y+=spacing;}
}

std::vector<Rectangle> validation_card_rectangles(std::size_t count, int screen_width,
                                                  int screen_height) {
  std::vector<Rectangle> rectangles;
  if(count==0)return rectangles;
  constexpr float margin=36.0F,gap=18.0F,top=142.0F;
  const float width=(screen_width-margin*2-gap*(count-1))/count;
  const float height=std::max(330.0F,screen_height-top-104.0F);
  for(std::size_t index=0;index<count;++index)
    rectangles.push_back({margin+index*(width+gap),top,width,height});
  return rectangles;
}

Rectangle validation_bottom_button(int index, int screen_width, int screen_height) {
  constexpr float width=190.0F,height=44.0F,gap=14.0F;
  const float total=width*2+gap;
  return {(screen_width-total)/2+index*(width+gap),screen_height-58.0F,width,height};
}

void draw_button(Rectangle rectangle, const std::string& label, Color fill, Vector2 mouse) {
  const bool hovered=CheckCollisionPointRec(mouse,rectangle);
  if(hovered)fill=ColorBrightness(fill,0.12F);
  DrawRectangleRec(rectangle,fill);
  DrawRectangleLinesEx(rectangle,hovered?2.0F:1.0F,
                       hovered?primary_text:ColorBrightness(fill,0.25F));
  const int font_size=16;
  DrawText(label.c_str(),static_cast<int>(rectangle.x+(rectangle.width-MeasureText(label.c_str(),font_size))/2),
           static_cast<int>(rectangle.y+(rectangle.height-font_size)/2),font_size,primary_text);
}

Color request_color(const json& request) {
  return agent_color(request.value("agent_id",""));
}

void draw_request_card(const json& request, Rectangle rectangle, bool expanded, bool hovered=false) {
  const auto card_color=request_color(request);
  const Color fill=hovered?Color{51,55,53,255}:Color{42,45,44,255};
  DrawRectangleRec(rectangle,fill);
  DrawRectangleLinesEx(rectangle,hovered?3.0F:2.0F,
                       hovered?ColorBrightness(card_color,0.25F):card_color);
  DrawRectangle(static_cast<int>(rectangle.x),static_cast<int>(rectangle.y),
                static_cast<int>(rectangle.width),7,card_color);
  const int x=static_cast<int>(rectangle.x+20),maximum_width=static_cast<int>(rectangle.width-40);
  int y=static_cast<int>(rectangle.y+23);
  DrawText(request.value("agent_name","Personnage").c_str(),x,y,15,card_color);y+=30;
  draw_lines(wrapped_lines(request.value("title","Proposition sans titre"),maximum_width,22,3),
             x,y,22,27,primary_text);
  y+=18;
  DrawText("BESOIN",x,y,12,secondary_text);y+=19;
  draw_lines(wrapped_lines(request.value("need","Non précisé"),maximum_width,15,expanded?4:3),
             x,y,15,20,primary_text);
  y+=16;
  DrawText("CHANGEMENT PROPOSÉ",x,y,12,secondary_text);y+=19;
  draw_lines(wrapped_lines(request.value("proposed_change","Non précisé"),maximum_width,15,
                           expanded?6:4),x,y,15,20,primary_text);
  if(expanded){
    y+=16;
    DrawText("MÉCANISME",x,y,12,secondary_text);y+=19;
    const auto mechanism=request.value("mechanism",json::object());
    draw_lines(wrapped_lines(mechanism.value("summary","Non précisé"),maximum_width,15,4),
               x,y,15,20,primary_text);
  }else{
    DrawLine(x,static_cast<int>(rectangle.y+rectangle.height-42),
             static_cast<int>(rectangle.x+rectangle.width-20),
             static_cast<int>(rectangle.y+rectangle.height-42),divider);
    DrawText("Cliquer pour choisir",x,static_cast<int>(rectangle.y+rectangle.height-29),14,
             hovered?primary_text:secondary_text);
  }
}
}

struct RaylibInterface::Impl {
  UiSnapshot snapshot;
  bool has_snapshot{};
  std::string selected_agent_id;
  std::string screenshot_path;
  bool screenshot_taken{};
  std::string validation_screenshot_path;
  std::string activity_screenshot_path;
  bool activity_screenshot_taken{};
  int activity_rendered_frames{};
  std::string automation_command;
  int selected_delay_ms{};
  bool delay_dragging{};
  bool wants_restart{};
  SimulationSpeedControl speed;
  int simulation_frames{};

  explicit Impl(int initial_delay_ms):selected_delay_ms(std::clamp(initial_delay_ms,0,10000)) {
    SetTraceLogLevel(LOG_WARNING);
    unsigned int flags=FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT|FLAG_WINDOW_HIGHDPI;
#ifdef __APPLE__
    const char* focus_window=std::getenv("AUTOPOIESIS_FOCUS_WINDOW");
    if(!focus_window||std::string(focus_window)=="0")flags|=FLAG_WINDOW_UNFOCUSED;
#endif
    SetConfigFlags(flags);
    InitWindow(1280,760,"Autopoiesis");
    SetWindowMinSize(1040,680);
    if(const char* width=std::getenv("AUTOPOIESIS_WINDOW_WIDTH")){
      const char* height=std::getenv("AUTOPOIESIS_WINDOW_HEIGHT");
      try{SetWindowSize(std::max(1040,std::stoi(width)),std::max(680,std::stoi(height?height:"760")));}
      catch(...){}
    }
    if(const char* x=std::getenv("AUTOPOIESIS_WINDOW_X")){
      const char* y=std::getenv("AUTOPOIESIS_WINDOW_Y");
      try{SetWindowPosition(std::stoi(x),std::stoi(y?y:"0"));}catch(...){}
    }
    SetTargetFPS(60);
    if(const char* configured=std::getenv("AUTOPOIESIS_GUI_SCREENSHOT"))screenshot_path=configured;
    if(const char* configured=std::getenv("AUTOPOIESIS_GUI_VALIDATION_SCREENSHOT"))
      validation_screenshot_path=configured;
    if(const char* configured=std::getenv("AUTOPOIESIS_GUI_ACTIVITY_SCREENSHOT"))
      activity_screenshot_path=configured;
    if(const char* configured=std::getenv("AUTOPOIESIS_GUI_AUTOMATION_COMMAND"))
      automation_command=configured;
  }

  Rectangle delay_slider_hitbox(float x,float y,float width) const {
    return {x-10.0F,y+24.0F,width+20.0F,38.0F};
  }

  Rectangle speed_button(int index) const {
    return {static_cast<float>(GetScreenWidth()-232+index*42),20.0F,34.0F,34.0F};
  }

  bool speed_controls_hovered() const {
    const auto mouse=GetMousePosition();
    return CheckCollisionPointRec(mouse,speed_button(0))||
           CheckCollisionPointRec(mouse,speed_button(1))||
           CheckCollisionPointRec(mouse,speed_button(2));
  }

  void handle_speed_controls() {
    const auto mouse=GetMousePosition();
    const bool clicked=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if(IsKeyPressed(KEY_SPACE)||(clicked&&CheckCollisionPointRec(mouse,speed_button(1))))
      speed.toggle_pause();
    if(IsKeyPressed(KEY_MINUS)||(clicked&&CheckCollisionPointRec(mouse,speed_button(0))))
      speed.slower();
    if(IsKeyPressed(KEY_EQUAL)||(clicked&&CheckCollisionPointRec(mouse,speed_button(2))))
      speed.faster();
    SetTargetFPS(speed.paused()?60:speed.target_fps());
  }

  void draw_speed_controls() const {
    const auto mouse=GetMousePosition();
    const std::array<const char*,3> labels{"-",speed.paused()?">":"||","+"};
    for(int index=0;index<3;++index){
      const auto rectangle=speed_button(index);
      const bool hovered=CheckCollisionPointRec(mouse,rectangle);
      DrawRectangleRec(rectangle,hovered?ColorBrightness(panel,0.25F):panel);
      DrawRectangleLinesEx(rectangle,1.0F,hovered?accent:divider);
      const int font=index==1?17:21;
      DrawText(labels[index],static_cast<int>(rectangle.x+rectangle.width/2-MeasureText(labels[index],font)/2),
               static_cast<int>(rectangle.y+rectangle.height/2-font/2),font,primary_text);
    }
    const std::string value=TextFormat("%.2gx",speed.multiplier());
    DrawText(value.c_str(),GetScreenWidth()-96,29,15,speed.paused()?secondary_text:accent);
  }

  void handle_delay_slider(float x,float y,float width) {
    const auto mouse=GetMousePosition();
    const auto hitbox=delay_slider_hitbox(x,y,width);
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&CheckCollisionPointRec(mouse,hitbox))
      delay_dragging=true;
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))delay_dragging=false;
    if(delay_dragging&&IsMouseButtonDown(MOUSE_BUTTON_LEFT))
      selected_delay_ms=simulation_delay_from_slider(mouse.x,x,x+width);
    if(CheckCollisionPointRec(mouse,hitbox)){
      if(IsKeyPressed(KEY_LEFT))selected_delay_ms=std::max(0,selected_delay_ms-100);
      if(IsKeyPressed(KEY_RIGHT))selected_delay_ms=std::min(10000,selected_delay_ms+100);
    }
  }

  void draw_delay_slider(float x,float y,float width) const {
    DrawText("PAUSE ENTRE JOURNÉES",static_cast<int>(x),static_cast<int>(y),13,secondary_text);
    const std::string value=std::to_string(selected_delay_ms)+" ms";
    DrawText(value.c_str(),static_cast<int>(x+width)-MeasureText(value.c_str(),16),
             static_cast<int>(y)-2,16,primary_text);
    const Rectangle track{x,y+36.0F,width,6.0F};
    DrawRectangleRec(track,{61,66,64,255});
    const float ratio=selected_delay_ms/10000.0F;
    DrawRectangleRec({track.x,track.y,track.width*ratio,track.height},accent);
    const float knob_x=track.x+track.width*ratio;
    const bool hovered=CheckCollisionPointRec(GetMousePosition(),delay_slider_hitbox(x,y,width));
    DrawCircle(static_cast<int>(knob_x),static_cast<int>(track.y+track.height/2),
               hovered||delay_dragging?10.0F:8.0F,hovered?ColorBrightness(accent,0.2F):accent);
    DrawText("0 ms · vitesse maximale",static_cast<int>(x),static_cast<int>(y+54),13,secondary_text);
    const char* slow="10000 ms";
    DrawText(slow,static_cast<int>(x+width)-MeasureText(slow,13),static_cast<int>(y+54),13,secondary_text);
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
    const auto mouse=GetMousePosition();
    const auto hovered_position=map_position_at_pixel(viewport,mouse.x,mouse.y,
                                                       snapshot.width,snapshot.height);
    const auto* hovered_agent=hovered_position?agent_at_position(snapshot,*hovered_position):nullptr;
    const UiCell* hovered_cell=nullptr;
    if(hovered_position){
      const auto found=std::find_if(snapshot.cells.begin(),snapshot.cells.end(),[&](const UiCell& cell){
        return cell.position==*hovered_position;
      });
      if(found!=snapshot.cells.end())hovered_cell=&*found;
    }
    DrawRectangle(static_cast<int>(viewport.x-2),static_cast<int>(viewport.y-2),
                  static_cast<int>(viewport.width+4),static_cast<int>(viewport.height+4),divider);
    for(const auto& cell:snapshot.cells){
      const int x=static_cast<int>(viewport.x+cell.position.x*cell_width);
      const int y=static_cast<int>(viewport.y+cell.position.y*cell_height);
      DrawRectangle(x,y,static_cast<int>(std::ceil(cell_width)),static_cast<int>(std::ceil(cell_height)),
                    terrain_color(cell.terrain));
      if(cell.food>0)DrawCircle(x+static_cast<int>(cell_width*0.72F),y+static_cast<int>(cell_height*0.28F),
                                std::max(2.0F,cell_width*0.10F),{232,203,92,255});
      if(cell.branches>0){
        DrawLine(x+4,y+static_cast<int>(cell_height)-5,x+static_cast<int>(cell_width*0.45F),
                 y+static_cast<int>(cell_height*0.58F),{154,108,65,255});
        DrawLine(x+7,y+static_cast<int>(cell_height)-4,x+static_cast<int>(cell_width*0.58F),
                 y+static_cast<int>(cell_height*0.72F),{194,142,82,255});
      }
      if(cell.iron_ore>0)DrawCircle(x+static_cast<int>(cell_width*0.28F),
          y+static_cast<int>(cell_height*0.72F),std::max(2.0F,cell_width*0.11F),
          {171,177,181,255});
      if(cell.campfire){
        const int center_x=x+static_cast<int>(cell_width/2),center_y=y+static_cast<int>(cell_height/2);
        DrawCircle(center_x,center_y,std::max(4.0F,cell_width*0.22F),{219,78,45,255});
        DrawCircle(center_x,center_y,std::max(2.0F,cell_width*0.10F),{249,197,66,255});
        const int stored_total=cell.stored_food+cell.stored_wood+cell.stored_branches;
        if(stored_total>0)DrawText(TextFormat("%d",stored_total),x+2,y+2,10,primary_text);
      }
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
      if(hovered_agent&&hovered_agent->state.id==agent.state.id){
        DrawCircleLines(x,y,radius+5,ColorBrightness(agent_color(agent.state.id),0.35F));
        const int label_width=MeasureText(agent.state.name.c_str(),14)+18;
        const int label_x=std::clamp(x-label_width/2,static_cast<int>(viewport.x),
                                     static_cast<int>(viewport.x+viewport.width)-label_width);
        const int label_y=std::max(static_cast<int>(viewport.y),y-static_cast<int>(radius)-31);
        DrawRectangle(label_x,label_y,label_width,23,{24,26,27,235});
        DrawRectangleLines(label_x,label_y,label_width,23,agent_color(agent.state.id));
        DrawText(agent.state.name.c_str(),label_x+9,label_y+5,14,primary_text);
      }
      const std::string initial=agent.state.name.empty()?"?":agent.state.name.substr(0,1);
      const int font=std::max(10,static_cast<int>(cell_width*0.48F));
      DrawText(initial.c_str(),x-MeasureText(initial.c_str(),font)/2,y-font/2,font,background);
    }
    if(hovered_cell&&hovered_cell->campfire&&!hovered_agent){
      const std::string label=clipped(
          TextFormat("Réserve · nourriture %d (%d cuite) · bois %d · branches %d · fer %d · objets %d",
                     hovered_cell->stored_food,hovered_cell->cooked_food,
                     hovered_cell->stored_wood,hovered_cell->stored_branches,
                     hovered_cell->stored_iron_ore,hovered_cell->crafted_items),
          std::max(80,static_cast<int>(viewport.width)-16),14);
      const int label_width=MeasureText(label.c_str(),14)+18;
      const int center_x=static_cast<int>(viewport.x+(hovered_cell->position.x+0.5F)*cell_width);
      const int cell_y=static_cast<int>(viewport.y+hovered_cell->position.y*cell_height);
      const int label_x=std::clamp(center_x-label_width/2,static_cast<int>(viewport.x),
                                   static_cast<int>(viewport.x+viewport.width)-label_width);
      const int label_y=std::max(static_cast<int>(viewport.y),cell_y-29);
      DrawRectangle(label_x,label_y,label_width,23,{24,26,27,235});
      DrawRectangleLines(label_x,label_y,label_width,23,accent);
      DrawText(label.c_str(),label_x+9,label_y+5,14,primary_text);
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
    if(!agent.community_role.empty())
      DrawText(clipped("Rôle au foyer : "+agent.community_role,width-44,13).c_str(),
               x+22,cursor+52,13,secondary_text);
    cursor+=agent.community_role.empty()?66:84;
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
    const int inventory_y=std::min(cursor,height-88);
    DrawText(TextFormat("Charge %d/%d · Ration %s",inventory_load(agent),
                        inventory_capacity(agent),agent.carried_food?"oui":"non"),
             x+22,inventory_y,13,secondary_text);
    DrawText(TextFormat("Bois %d · Branches %d · Fer %d",agent.wood_inventory,
                        agent.branch_inventory,agent.iron_ore_inventory),
             x+22,inventory_y+20,13,secondary_text);
    std::string equipment=agent.equipped_tool?
        "Hache "+std::to_string(agent.equipped_tool->durability)+"/"+
            std::to_string(agent.equipped_tool->maximum_durability):"Sans outil";
    if(agent.home_camp)equipment+=" · Foyer "+std::to_string(agent.home_camp->x)+","+
        std::to_string(agent.home_camp->y);
    DrawText(clipped(equipment,width-44,13).c_str(),x+22,inventory_y+40,13,secondary_text);
    DrawText(TextFormat("Monotonie : %d",agent.boredom),x+22,inventory_y+60,13,secondary_text);
  }

  void draw() const {
    bool agent_hovered=false;
    if(has_snapshot){
      const auto mouse=GetMousePosition();
      const auto viewport=calculate_map_viewport(GetScreenWidth(),GetScreenHeight());
      const auto position=map_position_at_pixel(viewport,mouse.x,mouse.y,snapshot.width,snapshot.height);
      agent_hovered=position&&agent_at_position(snapshot,*position);
    }
    SetMouseCursor(agent_hovered||speed_controls_hovered()?MOUSE_CURSOR_POINTING_HAND:MOUSE_CURSOR_DEFAULT);
    BeginDrawing();
    ClearBackground(has_snapshot&&snapshot.phase==DayPhase::Night?Color{13,18,27,255}:background);
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
    DrawText(snapshot.phase==DayPhase::Night?"NUIT":"JOUR",112,48,12,
             snapshot.phase==DayPhase::Night?Color{137,170,219,255}:accent);
    draw_speed_controls();
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

  void draw_validation(const ValidationPrompt& prompt) const {
    const int screen_width=GetScreenWidth(),screen_height=GetScreenHeight();
    const auto mouse=GetMousePosition();
    bool clickable_hovered=false;
    if(prompt.stage==ValidationStage::Choose){
      const auto rectangles=validation_card_rectangles(prompt.requests.size(),screen_width,screen_height);
      clickable_hovered=std::any_of(rectangles.begin(),rectangles.end(),[&](Rectangle rectangle){
        return CheckCollisionPointRec(mouse,rectangle);
      })||CheckCollisionPointRec(mouse,validation_bottom_button(0,screen_width,screen_height))
        ||CheckCollisionPointRec(mouse,validation_bottom_button(1,screen_width,screen_height));
    }else if(prompt.stage==ValidationStage::Confirm){
      const float card_width=std::min(720.0F,screen_width*0.64F);
      const float action_x=36.0F+card_width+30.0F;
      const float action_width=screen_width-action_x-36.0F;
      clickable_hovered=CheckCollisionPointRec(mouse,{action_x,200.0F,action_width,54.0F})
        ||CheckCollisionPointRec(mouse,{action_x,270.0F,action_width,54.0F})
        ||CheckCollisionPointRec(mouse,{action_x,340.0F,action_width,48.0F})
        ||CheckCollisionPointRec(mouse,{action_x,screen_height-90.0F,action_width,44.0F});
    }else{
      clickable_hovered=CheckCollisionPointRec(mouse,{36.0F,246.0F,250.0F,54.0F})
        ||CheckCollisionPointRec(mouse,{304.0F,246.0F,210.0F,54.0F})
        ||CheckCollisionPointRec(mouse,delay_slider_hitbox(36.0F,340.0F,
                                                           std::min(620.0F,screen_width-72.0F)));
    }
    SetMouseCursor(clickable_hovered?MOUSE_CURSOR_POINTING_HAND:MOUSE_CURSOR_DEFAULT);
    BeginDrawing();
    ClearBackground(background);
    DrawRectangle(0,0,screen_width,86,{30,33,34,255});
    DrawText("CHOIX D'ÉVOLUTION",36,18,27,primary_text);
    DrawText(TextFormat("Jour %d · cycle %d",prompt.day,prompt.simulation_cycle),36,52,14,secondary_text);

    if(prompt.stage==ValidationStage::Choose){
      DrawText("Choisissez une carte. Une seule évolution pourra être autorisée pendant cette fenêtre.",
               36,102,16,secondary_text);
      const auto rectangles=validation_card_rectangles(prompt.requests.size(),screen_width,screen_height);
      for(std::size_t index=0;index<rectangles.size();++index)
        draw_request_card(prompt.requests[index],rectangles[index],false,
                          CheckCollisionPointRec(mouse,rectangles[index]));
      draw_button(validation_bottom_button(0,screen_width,screen_height),"Aucune évolution",
                  {71,76,74,255},mouse);
      draw_button(validation_bottom_button(1,screen_width,screen_height),"Arrêter le run",
                  {117,61,58,255},mouse);
    }else if(prompt.stage==ValidationStage::Confirm&&prompt.selected_index>0&&
             prompt.selected_index<=prompt.requests.size()){
      DrawText("Confirmez cette autorisation avant de transmettre la demande à Dieu.",
               36,102,16,secondary_text);
      const float card_width=std::min(720.0F,screen_width*0.64F);
      const Rectangle card{36.0F,138.0F,card_width,screen_height-174.0F};
      draw_request_card(prompt.requests[prompt.selected_index-1],card,true);
      const float action_x=card.x+card.width+30.0F;
      const float action_width=screen_width-action_x-36.0F;
      DrawText("VOTRE DÉCISION",static_cast<int>(action_x),166,13,secondary_text);
      draw_button({action_x,200.0F,action_width,54.0F},"Approuver",{50,116,77,255},mouse);
      draw_button({action_x,270.0F,action_width,54.0F},"Refuser",{129,61,57,255},mouse);
      draw_button({action_x,340.0F,action_width,48.0F},"Revenir aux cartes",{71,76,74,255},mouse);
      draw_button({action_x,screen_height-90.0F,action_width,44.0F},"Arrêter le run",{71,76,74,255},mouse);
    }else{
      const bool empty=prompt.stage==ValidationStage::Empty;
      DrawText(empty?"Aucune évolution disponible":"Choix enregistré",
               36,150,28,primary_text);
      DrawText(empty?"La simulation peut reprendre sans modifier le monde.":
                     "Les autres propositions restent en attente pour l'historique.",
               36,194,17,secondary_text);
      const Rectangle resume{36.0F,246.0F,250.0F,54.0F};
      const Rectangle stop{304.0F,246.0F,210.0F,54.0F};
      draw_button(resume,"Reprendre",{50,116,77,255},mouse);
      draw_button(stop,"Arrêter le run",{117,61,58,255},mouse);
      draw_delay_slider(36.0F,340.0F,std::min(620.0F,screen_width-72.0F));
    }
    EndDrawing();
  }

  void draw_activity(const UiActivity& activity) const {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    const int screen_width=GetScreenWidth(),screen_height=GetScreenHeight();
    const float margin=std::clamp(screen_width*0.07F,42.0F,92.0F);
    const float content_width=screen_width-margin*2.0F;
    const float gap=10.0F;
    const std::size_t total_steps=std::max<std::size_t>(1,activity.total_calls);
    const float step_width=(content_width-gap*static_cast<float>(total_steps-1))/total_steps;
    const double pulse=(std::sin(GetTime()*4.0)+1.0)*0.5;
    BeginDrawing();
    ClearBackground(background);
    DrawRectangle(0,0,screen_width,86,{30,33,34,255});
    DrawText("FENÊTRE IA",static_cast<int>(margin),18,27,primary_text);
    DrawText(TextFormat("Année %d · mois %d · jour %d · cycle %d",activity.date.year,
                        activity.date.month,activity.date.day_of_month,activity.simulation_cycle),
             static_cast<int>(margin),52,14,secondary_text);

    DrawText(TextFormat("Appel %zu sur %zu",activity.call_number,activity.total_calls),
             static_cast<int>(margin),112,15,secondary_text);
    for(std::size_t index=0;index<total_steps;++index){
      const Rectangle step{margin+index*(step_width+gap),145.0F,step_width,9.0F};
      Color color={58,62,61,255};
      if(index+1<activity.call_number)color={69,147,98,255};
      if(index+1==activity.call_number)
        color=ColorBrightness(accent,static_cast<float>(pulse*0.16));
      DrawRectangleRec(step,color);
      DrawText(TextFormat("%zu",index+1),static_cast<int>(step.x),164,13,
               index+1<=activity.call_number?primary_text:secondary_text);
    }

    const int center_x=screen_width/2;
    const Color character_color=agent_color(activity.agent_id);
    DrawCircle(center_x,266,38,character_color);
    const std::string initial=activity.agent_name.empty()?"?":activity.agent_name.substr(0,1);
    DrawText(initial.c_str(),center_x-MeasureText(initial.c_str(),25)/2,253,25,background);
    const std::string operation=activity.kind==UiActivityKind::PeriodReport?
        "Création du bilan":"Formulation d'une évolution";
    DrawText(activity.agent_name.c_str(),center_x-MeasureText(activity.agent_name.c_str(),18)/2,
             323,18,character_color);
    DrawText(operation.c_str(),center_x-MeasureText(operation.c_str(),30)/2,359,30,primary_text);
    DrawText("Réponse en attente...",center_x-MeasureText("Réponse en attente...",17)/2,
             408,17,secondary_text);

    const Rectangle track{margin,474.0F,content_width,6.0F};
    DrawRectangleRec(track,{52,56,55,255});
    const float sweep_width=std::min(180.0F,content_width*0.22F);
    const float phase=static_cast<float>(std::fmod(GetTime()*0.32,1.0));
    DrawRectangleRec({track.x+phase*(track.width-sweep_width),track.y,sweep_width,track.height},accent);
    const int seconds=static_cast<int>(activity.elapsed_ms/1000);
    const std::string elapsed=seconds<1?"Connexion au modèle...":
        "Traitement en cours · "+std::to_string(seconds)+" s";
    DrawText(elapsed.c_str(),static_cast<int>(margin),500,15,primary_text);
    DrawText("La simulation reprendra après le choix d'évolution.",static_cast<int>(margin),
             screen_height-55,15,secondary_text);
    EndDrawing();
  }

  void draw_evolution_progress(const EvolutionProgress& progress,bool completion) const {
    const int screen_width=GetScreenWidth(),screen_height=GetScreenHeight();
    const float margin=std::clamp(screen_width*0.07F,42.0F,92.0F);
    const float content_width=screen_width-margin*2.0F;
    const auto mouse=GetMousePosition();
    const Rectangle next_button{margin,static_cast<float>(screen_height-118),330.0F,56.0F};
    const Rectangle stop_button{margin+350.0F,static_cast<float>(screen_height-118),220.0F,56.0F};
    const float slider_y=std::max(430.0F,screen_height-320.0F);
    const bool clickable=completion&&(CheckCollisionPointRec(mouse,next_button)||
                                      CheckCollisionPointRec(mouse,stop_button)||
                                      CheckCollisionPointRec(mouse,delay_slider_hitbox(
                                          margin,slider_y,std::min(620.0F,content_width))));
    SetMouseCursor(clickable?MOUSE_CURSOR_POINTING_HAND:MOUSE_CURSOR_DEFAULT);
    BeginDrawing();
    ClearBackground(background);
    DrawRectangle(0,0,screen_width,86,{30,33,34,255});
    DrawText("ÉVOLUTION DU MONDE",static_cast<int>(margin),18,27,primary_text);
    DrawText(clipped(progress.request_id,static_cast<int>(content_width),14).c_str(),
             static_cast<int>(margin),53,14,secondary_text);

    if(completion){
      const bool complete=progress.stage==EvolutionProgressStage::Complete&&progress.successful;
      const std::string title=complete?"C'est terminé.":
          progress.stage==EvolutionProgressStage::TimedOut?"Le suivi est interrompu.":
          "Le traitement est terminé avec une erreur.";
      DrawText(title.c_str(),static_cast<int>(margin),154,36,complete?accent:Color{218,116,96,255});
      DrawText("Voulez-vous passer à l'étape suivante ?",static_cast<int>(margin),211,24,primary_text);
      DrawText(progress.message.c_str(),static_cast<int>(margin),270,18,secondary_text);
      if(!progress.detail.empty()){
        int detail_y=310;
        draw_lines(wrapped_lines(progress.detail,static_cast<int>(content_width),15,5),
                   static_cast<int>(margin),detail_y,15,22,primary_text);
      }
      DrawText(TextFormat("Durée du suivi : %lld s",progress.elapsed_seconds),
               static_cast<int>(margin),screen_height-168,14,secondary_text);
      draw_delay_slider(margin,slider_y,std::min(620.0F,content_width));
      draw_button(next_button,"Passer à l'étape suivante",{50,116,77,255},mouse);
      draw_button(stop_button,"Arrêter le run",{117,61,58,255},mouse);
      EndDrawing();
      return;
    }

    DrawText("Dieu transforme la proposition approuvée en mécanisme testé et activable.",
             static_cast<int>(margin),112,16,secondary_text);
    const std::array<const char*,7> labels{{"File","Préparation","TDD","Compte rendu",
                                            "Vérification","Activation","Terminé"}};
    const auto rank=[](EvolutionProgressStage stage){
      switch(stage){
        case EvolutionProgressStage::Queued:return 0;
        case EvolutionProgressStage::Preparing:return 1;
        case EvolutionProgressStage::Implementing:return 2;
        case EvolutionProgressStage::Reporting:return 3;
        case EvolutionProgressStage::Verifying:
        case EvolutionProgressStage::Correcting:return 4;
        case EvolutionProgressStage::Activating:return 5;
        case EvolutionProgressStage::Complete:return 6;
        case EvolutionProgressStage::Failed:
        case EvolutionProgressStage::TimedOut:return 5;
      }
      return 0;
    };
    const int active=rank(progress.stage);
    const float gap=10.0F;
    const float width=(content_width-gap*6.0F)/7.0F;
    const double pulse=(std::sin(GetTime()*4.0)+1.0)*0.5;
    for(int index=0;index<7;++index){
      const Rectangle step{margin+index*(width+gap),160.0F,width,10.0F};
      Color color={58,62,61,255};
      if(index<active)color={69,147,98,255};
      if(index==active)color=ColorBrightness(accent,static_cast<float>(pulse*0.16));
      DrawRectangleRec(step,color);
      DrawText(clipped(labels[index],static_cast<int>(width),13).c_str(),
               static_cast<int>(step.x),181,13,index<=active?primary_text:secondary_text);
    }
    DrawText(progress.message.c_str(),static_cast<int>(margin),264,28,primary_text);
    DrawText(TextFormat("Traitement en cours · %lld s",progress.elapsed_seconds),
             static_cast<int>(margin),310,16,accent);
    if(!progress.detail.empty()){
      DrawText("DERNIER RETOUR",static_cast<int>(margin),372,13,secondary_text);
      DrawRectangle(static_cast<int>(margin),398,static_cast<int>(content_width),1,divider);
      int detail_y=422;
      draw_lines(wrapped_lines(progress.detail,static_cast<int>(content_width),15,7),
                 static_cast<int>(margin),detail_y,15,23,primary_text);
    }
    const Rectangle track{margin,static_cast<float>(screen_height-85),content_width,6.0F};
    DrawRectangleRec(track,{52,56,55,255});
    const float sweep_width=std::min(180.0F,content_width*0.22F);
    const float phase=static_cast<float>(std::fmod(GetTime()*0.32,1.0));
    DrawRectangleRec({track.x+phase*(track.width-sweep_width),track.y,sweep_width,track.height},accent);
    DrawText("Les logs détaillés restent disponibles dans le terminal.",static_cast<int>(margin),
             screen_height-55,14,secondary_text);
    EndDrawing();
  }

  void draw_recompilation(const RecompileProgress& progress) const {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    const int screen_width=GetScreenWidth(),screen_height=GetScreenHeight();
    const float margin=std::clamp(screen_width*0.07F,42.0F,92.0F);
    const float content_width=screen_width-margin*2.0F;
    const bool failed=progress.stage==RecompileStage::Failed;
    const bool ready=progress.stage==RecompileStage::Ready;
    BeginDrawing();
    ClearBackground(background);
    DrawRectangle(0,0,screen_width,86,{30,33,34,255});
    DrawText("TRANSFERT DU MONDE",static_cast<int>(margin),18,27,primary_text);
    DrawText("Le checkpoint est enregistré. Aucun cycle n'est exécuté pendant le transfert.",
             static_cast<int>(margin),53,14,secondary_text);
    const std::string title=failed?"La recompilation a échoué":
                            ready?"La nouvelle version est prête":"Recompilation en arrière-plan";
    DrawText(title.c_str(),static_cast<int>(margin),170,34,
             failed?Color{218,116,96,255}:primary_text);
    const std::string subtitle=failed?"Le monde reste sauvegardé pour un prochain lancement.":
       ready?"Transfert vers le nouveau moteur...":"Construction du nouveau binaire C++ et liaison de l'interface raylib.";
    DrawText(subtitle.c_str(),static_cast<int>(margin),225,18,secondary_text);
    if(!progress.detail.empty()){
      DrawText("DERNIER RETOUR",static_cast<int>(margin),310,13,secondary_text);
      int detail_y=344;
      draw_lines(wrapped_lines(progress.detail,static_cast<int>(content_width),15,6),
                 static_cast<int>(margin),detail_y,15,23,primary_text);
    }
    const Rectangle track{margin,static_cast<float>(screen_height-105),content_width,7.0F};
    DrawRectangleRec(track,{52,56,55,255});
    if(ready)DrawRectangleRec(track,accent);
    else if(!failed){
      const float sweep_width=std::min(220.0F,content_width*0.22F);
      const float phase=static_cast<float>(std::fmod(GetTime()*0.28,1.0));
      DrawRectangleRec({track.x+phase*(track.width-sweep_width),track.y,sweep_width,track.height},accent);
    }
    DrawText(TextFormat("%lld s",progress.elapsed_ms/1000),static_cast<int>(margin),
             screen_height-72,15,failed?Color{218,116,96,255}:accent);
    DrawText("Le processus sera remplacé, puis le checkpoint sera restauré.",
             static_cast<int>(margin)+70,screen_height-72,15,secondary_text);
    EndDrawing();
  }

  void capture(const std::string& path) {
    if(path.empty())return;
    Image framebuffer=LoadImageFromScreen();
    ExportImage(framebuffer,path.c_str());
    UnloadImage(framebuffer);
  }

  void capture_if_requested() {
    if(screenshot_taken||screenshot_path.empty())return;
    capture(screenshot_path);
    screenshot_taken=true;
  }
};

RaylibInterface::RaylibInterface(int initial_delay_ms):impl_(std::make_unique<Impl>(initial_delay_ms)) {
  impl_->draw();
}
RaylibInterface::~RaylibInterface() { if(IsWindowReady())CloseWindow(); }

bool RaylibInterface::present(const UiSnapshot& snapshot) {
  const bool already_had_snapshot=impl_->has_snapshot;
  impl_->snapshot=snapshot;
  impl_->has_snapshot=true;
  ++impl_->simulation_frames;
  if(impl_->selected_agent_id.empty()&&!snapshot.agents.empty())
    impl_->selected_agent_id=snapshot.agents.front().state.id;
  impl_->handle_speed_controls();
  while(impl_->speed.paused()){
    if(WindowShouldClose())return false;
    impl_->handle_input();
    impl_->draw();
    impl_->capture_if_requested();
    impl_->handle_speed_controls();
  }
  if(impl_->simulation_frames%impl_->speed.render_stride()!=0)return !WindowShouldClose();
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

bool RaylibInterface::present_activity(const UiActivity& activity) {
  if(WindowShouldClose())return false;
  impl_->draw_activity(activity);
  ++impl_->activity_rendered_frames;
  if(impl_->activity_rendered_frames>=2&&!impl_->activity_screenshot_taken&&
     !impl_->activity_screenshot_path.empty()){
    impl_->capture(impl_->activity_screenshot_path);
    impl_->activity_screenshot_taken=true;
  }
  return !WindowShouldClose();
}

int RaylibInterface::simulation_delay_ms(int fallback) const {
  (void)fallback;
  return impl_->selected_delay_ms;
}

bool RaylibInterface::restart_requested() const { return impl_->wants_restart; }

bool RaylibInterface::present_recompilation(const RecompileProgress& progress) {
  if(WindowShouldClose())return false;
  impl_->draw_recompilation(progress);
  return !WindowShouldClose();
}

void RaylibInterface::prepare_restart_environment() const {
  const auto position=GetWindowPosition();
  setenv("AUTOPOIESIS_WINDOW_X",std::to_string(static_cast<int>(position.x)).c_str(),1);
  setenv("AUTOPOIESIS_WINDOW_Y",std::to_string(static_cast<int>(position.y)).c_str(),1);
  setenv("AUTOPOIESIS_WINDOW_WIDTH",std::to_string(GetScreenWidth()).c_str(),1);
  setenv("AUTOPOIESIS_WINDOW_HEIGHT",std::to_string(GetScreenHeight()).c_str(),1);
}

std::string RaylibInterface::request_command(const ValidationPrompt& prompt) {
  int rendered_frames=0;
  while(true){
    if(WindowShouldClose()||IsKeyPressed(KEY_ESCAPE))return "q";
    const int screen_width=GetScreenWidth(),screen_height=GetScreenHeight();
    const auto mouse=GetMousePosition();
    const bool clicked=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if(prompt.stage==ValidationStage::Choose){
      const auto rectangles=validation_card_rectangles(prompt.requests.size(),screen_width,screen_height);
      for(std::size_t index=0;index<rectangles.size();++index){
        const int key=KEY_ONE+static_cast<int>(index);
        if(IsKeyPressed(key)||(clicked&&CheckCollisionPointRec(mouse,rectangles[index])))
          return std::to_string(index+1);
      }
      if(IsKeyPressed(KEY_N)||(clicked&&CheckCollisionPointRec(
          mouse,validation_bottom_button(0,screen_width,screen_height))))return "n";
      if(clicked&&CheckCollisionPointRec(mouse,validation_bottom_button(1,screen_width,screen_height)))
        return "q";
    }else if(prompt.stage==ValidationStage::Confirm){
      const float card_width=std::min(720.0F,screen_width*0.64F);
      const float action_x=36.0F+card_width+30.0F;
      const float action_width=screen_width-action_x-36.0F;
      if(IsKeyPressed(KEY_A)||(clicked&&CheckCollisionPointRec(
          mouse,{action_x,200.0F,action_width,54.0F})))return "a";
      if(IsKeyPressed(KEY_R)||(clicked&&CheckCollisionPointRec(
          mouse,{action_x,270.0F,action_width,54.0F})))return "r";
      if(IsKeyPressed(KEY_B)||(clicked&&CheckCollisionPointRec(
          mouse,{action_x,340.0F,action_width,48.0F})))return "b";
      if(clicked&&CheckCollisionPointRec(
          mouse,{action_x,screen_height-90.0F,action_width,44.0F}))return "q";
    }else{
      impl_->handle_delay_slider(36.0F,340.0F,std::min(620.0F,screen_width-72.0F));
      if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_O)||(clicked&&CheckCollisionPointRec(
          mouse,{36.0F,246.0F,250.0F,54.0F})))return "o";
      if(clicked&&CheckCollisionPointRec(mouse,{304.0F,246.0F,210.0F,54.0F}))return "q";
    }

    impl_->draw_validation(prompt);
    ++rendered_frames;
    if(rendered_frames>=2&&!impl_->automation_command.empty()){
      impl_->capture(impl_->validation_screenshot_path);
      return impl_->automation_command;
    }
  }
}

bool RaylibInterface::present_evolution_progress(const EvolutionProgress& progress) {
  if(WindowShouldClose())return false;
  impl_->draw_evolution_progress(progress,false);
  return !WindowShouldClose();
}

std::string RaylibInterface::request_evolution_completion(const EvolutionProgress& progress) {
  int rendered_frames=0;
  while(true){
    if(WindowShouldClose()||IsKeyPressed(KEY_ESCAPE))return "q";
    const int screen_height=GetScreenHeight();
    const float margin=std::clamp(GetScreenWidth()*0.07F,42.0F,92.0F);
    const Rectangle next_button{margin,static_cast<float>(screen_height-118),330.0F,56.0F};
    const Rectangle stop_button{margin+350.0F,static_cast<float>(screen_height-118),220.0F,56.0F};
    const auto mouse=GetMousePosition();
    const float content_width=GetScreenWidth()-margin*2.0F;
    impl_->handle_delay_slider(margin,std::max(430.0F,screen_height-320.0F),
                               std::min(620.0F,content_width));
    if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_O)||
       (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&CheckCollisionPointRec(mouse,next_button))){
      impl_->wants_restart=progress.stage==EvolutionProgressStage::Complete&&progress.successful;
      return "o";
    }
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&CheckCollisionPointRec(mouse,stop_button))return "q";
    impl_->draw_evolution_progress(progress,true);
    ++rendered_frames;
    if(rendered_frames>=2&&!impl_->automation_command.empty()){
      impl_->capture(impl_->validation_screenshot_path);
      impl_->wants_restart=progress.stage==EvolutionProgressStage::Complete&&progress.successful&&
                           impl_->automation_command=="o";
      return impl_->automation_command;
    }
  }
}
}
