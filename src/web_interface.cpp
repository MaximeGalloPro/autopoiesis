#include "autopoiesis/web_interface.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <iostream>
#include <poll.h>
#include <thread>
#include <unistd.h>

namespace apo {
namespace {
bool exact_fields(const json& value,std::initializer_list<std::string_view> names) {
  if(value.size()!=names.size())return false;
  return std::all_of(names.begin(),names.end(),[&](std::string_view name){
    return value.contains(std::string(name));
  });
}

std::string terrain_name(Terrain terrain) {
  switch(terrain){
    case Terrain::Ground:return "ground";
    case Terrain::Wall:return "wall";
    case Terrain::Water:return "water";
    case Terrain::Tree:return "tree";
    case Terrain::Bush:return "bush";
  }
  return "unknown";
}

json position_json(Position position) { return {{"x",position.x},{"y",position.y}}; }

json agent_json(const Agent& agent) {
  json memories=json::array();
  for(const auto& memory:agent.memories)memories.push_back(memory);
  json map_memory=json::array();
  for(const auto& [position,terrain]:agent.map_memory)
    map_memory.push_back({{"x",position.first},{"y",position.second},
                          {"terrain",terrain_name(terrain)}});
  json map_visit_counts=json::array();
  for(const auto& [position,count]:agent.map_visit_counts)
    map_visit_counts.push_back({{"x",position.first},{"y",position.second},{"count",count}});
  json observed_animals=json::array();
  for(const auto& animal:agent.observed_animals)observed_animals.push_back(animal);
  json known_campfires=json::array();
  for(const auto& position:agent.known_campfires)
    known_campfires.push_back({{"x",position.first},{"y",position.second}});
  json carried_food=nullptr;
  if(agent.carried_food)
    carried_food={{"type",food_type_name(agent.carried_food->type)},
                  {"nutrition",agent.carried_food->nutrition}};
  json shelter_construction=nullptr;
  if(agent.shelter_construction)
    shelter_construction={{"position",position_json(agent.shelter_construction->position)},
                          {"progress",agent.shelter_construction->progress}};
  return {{"id",agent.id},{"name",agent.name},{"position",position_json(agent.position)},
          {"health",agent.health},{"hunger",agent.hunger},{"thirst",agent.thirst},
          {"fatigue",agent.fatigue},{"personality",personality_json(agent.personality)},
          {"attributes",attributes_json(agent.attributes)},{"memories",std::move(memories)},
          {"alive",agent.alive},{"sleeping_days",agent.sleeping_days},
          {"critical_hunger_days",agent.critical_hunger_days},
          {"critical_thirst_days",agent.critical_thirst_days},
          {"map_memory",std::move(map_memory)},
          {"map_visit_counts",std::move(map_visit_counts)},
          {"behavior",behavior_json(agent.behavior)},{"project",project_json(agent.project)},
          {"boredom",agent.boredom},{"relationships",relationships_json(agent.relationships)},
          {"observed_animals",std::move(observed_animals)},
          {"known_campfires",std::move(known_campfires)},
          {"wood_inventory",agent.wood_inventory},{"branch_inventory",agent.branch_inventory},
          {"carried_food",std::move(carried_food)},
          {"shelter_construction",std::move(shelter_construction)}};
}

std::string validation_stage_name(ValidationStage stage) {
  switch(stage){
    case ValidationStage::Empty:return "empty";
    case ValidationStage::Choose:return "choose";
    case ValidationStage::Confirm:return "confirm";
    case ValidationStage::Complete:return "complete";
  }
  return "empty";
}

std::string validation_kind_name(ValidationPromptKind kind) {
  return kind==ValidationPromptKind::Devil?"devil":"feature";
}

std::string activity_kind_name(UiActivityKind kind) {
  return kind==UiActivityKind::EvolutionRequest?"evolution_request":"period_report";
}

std::string evolution_stage_name(EvolutionProgressStage stage) {
  switch(stage){
    case EvolutionProgressStage::Queued:return "queued";
    case EvolutionProgressStage::Preparing:return "preparing";
    case EvolutionProgressStage::Implementing:return "implementing";
    case EvolutionProgressStage::Reporting:return "reporting";
    case EvolutionProgressStage::Verifying:return "verifying";
    case EvolutionProgressStage::Correcting:return "correcting";
    case EvolutionProgressStage::Activating:return "activating";
    case EvolutionProgressStage::Complete:return "complete";
    case EvolutionProgressStage::Failed:return "failed";
    case EvolutionProgressStage::TimedOut:return "timed_out";
  }
  return "queued";
}

std::string recompile_stage_name(RecompileStage stage) {
  switch(stage){
    case RecompileStage::Compiling:return "compiling";
    case RecompileStage::Ready:return "ready";
    case RecompileStage::Failed:return "failed";
  }
  return "compiling";
}

json evolution_progress_json(const EvolutionProgress& progress) {
  return {{"stage",evolution_stage_name(progress.stage)},
          {"request_id",progress.request_id},{"message",progress.message},
          {"detail",progress.detail},{"elapsed_seconds",progress.elapsed_seconds},
          {"successful",progress.successful}};
}

json allowed_validation_commands(const ValidationPrompt& prompt) {
  json allowed=json::array();
  if(prompt.stage==ValidationStage::Empty){allowed.push_back("o");allowed.push_back("q");}
  else if(prompt.stage==ValidationStage::Choose){
    for(std::size_t index=1;index<=prompt.requests.size();++index){
      allowed.push_back(std::to_string(index));
      allowed.push_back("d "+std::to_string(index));
    }
    allowed.push_back("n");allowed.push_back("q");
  }else if(prompt.stage==ValidationStage::Confirm){
    allowed={"a","r","b","d","q"};
    if(prompt.kind==ValidationPromptKind::Devil)allowed.erase(allowed.begin()+2);
  }else{
    allowed.push_back("o");
    for(std::size_t index=1;index<=prompt.requests.size();++index)
      allowed.push_back("d "+std::to_string(index));
    allowed.push_back("q");
  }
  return allowed;
}

json validation_prompt_json(const ValidationPrompt& prompt) {
  return {{"kind",validation_kind_name(prompt.kind)},
          {"stage",validation_stage_name(prompt.stage)},
          {"day",prompt.day},{"simulation_cycle",prompt.simulation_cycle},
          {"requests",prompt.requests},{"selected_index",prompt.selected_index},
          {"allowed_commands",allowed_validation_commands(prompt)}};
}

bool supported_speed(float speed) {
  constexpr std::array<float,5> speeds{0.25F,0.5F,1.0F,2.0F,4.0F};
  return std::find(speeds.begin(),speeds.end(),speed)!=speeds.end();
}

std::string web_command_name(WebCommandKind kind) {
  switch(kind){
    case WebCommandKind::Pause:return "pause";
    case WebCommandKind::Resume:return "resume";
    case WebCommandKind::TogglePause:return "toggle_pause";
    case WebCommandKind::SetSpeed:return "set_speed";
    case WebCommandKind::SetDelayMs:return "set_delay_ms";
    case WebCommandKind::SetApiEnabled:return "set_api_enabled";
    case WebCommandKind::Validation:return "validation";
    case WebCommandKind::Stop:return "stop";
  }
  return "unknown";
}
}

std::optional<WebCommand> parse_web_command(std::string_view line,std::string& error) {
  error.clear();
  json value;
  try{value=json::parse(line);}catch(const json::exception&){error="invalid_json";return std::nullopt;}
  try{
  if(!value.is_object()){error="command_must_be_an_object";return std::nullopt;}
  if(!value.contains("version")||!value["version"].is_number_integer()||
     value["version"].get<int>()!=web_protocol_version){
    error="unsupported_version";return std::nullopt;
  }
  if(!value.contains("command")||!value["command"].is_string()){
    error="command_must_be_a_string";return std::nullopt;
  }
  const auto name=value["command"].get<std::string>();
  if(name=="pause"||name=="resume"||name=="toggle_pause"||name=="stop"){
    if(!exact_fields(value,{"version","command"})){error="unexpected_command_fields";return std::nullopt;}
    if(name=="pause")return WebCommand{WebCommandKind::Pause};
    if(name=="resume")return WebCommand{WebCommandKind::Resume};
    if(name=="toggle_pause")return WebCommand{WebCommandKind::TogglePause};
    return WebCommand{WebCommandKind::Stop};
  }
  if(name=="set_speed"){
    if(!exact_fields(value,{"version","command","speed"})||!value["speed"].is_number()){
      error="speed_must_be_a_supported_number";return std::nullopt;
    }
    const float speed=value["speed"].get<float>();
    if(!supported_speed(speed)){error="unsupported_speed";return std::nullopt;}
    WebCommand result{WebCommandKind::SetSpeed};result.speed=speed;return result;
  }
  if(name=="set_delay_ms"){
    if(!exact_fields(value,{"version","command","delay_ms"})||
       !value["delay_ms"].is_number_integer()){
      error="delay_ms_must_be_an_integer";return std::nullopt;
    }
    const auto delay=value["delay_ms"].get<long long>();
    if(delay<0||delay>10000){error="delay_ms_out_of_range";return std::nullopt;}
    WebCommand result{WebCommandKind::SetDelayMs};result.delay_ms=static_cast<int>(delay);return result;
  }
  if(name=="set_api_enabled"){
    if(!exact_fields(value,{"version","command","enabled"})||!value["enabled"].is_boolean()){
      error="api_enabled_must_be_a_boolean";return std::nullopt;
    }
    WebCommand result{WebCommandKind::SetApiEnabled};
    result.enabled=value["enabled"].get<bool>();return result;
  }
  if(name=="validation"){
    if(!exact_fields(value,{"version","command","text"})||!value["text"].is_string()){
      error="validation_text_must_be_a_string";return std::nullopt;
    }
    const auto text=value["text"].get<std::string>();
    if(text.empty()||text.find_first_of("\r\n")!=std::string::npos){
      error="invalid_validation_text";return std::nullopt;
    }
    WebCommand result{WebCommandKind::Validation};result.validation_text=text;return result;
  }
  error="unknown_command";
  return std::nullopt;
  }catch(const json::exception&){
    error="invalid_command_value";
    return std::nullopt;
  }
}

bool web_validation_command_allowed(const ValidationPrompt& prompt,std::string_view command) {
  const auto allowed=allowed_validation_commands(prompt);
  return std::any_of(allowed.begin(),allowed.end(),[&](const json& value){
    return value.is_string()&&value.get_ref<const std::string&>()==command;
  });
}

json web_snapshot_json(const UiSnapshot& snapshot) {
  json cells=json::array();
  for(const auto& cell:snapshot.cells)
    cells.push_back({{"position",position_json(cell.position)},
                     {"terrain",terrain_name(cell.terrain)},{"food",cell.food},
                     {"wood",cell.wood},{"fibers",cell.fibers},
                     {"shelter_level",cell.shelter_level},{"branches",cell.branches},
                     {"campfire",cell.campfire},{"stored_food",cell.stored_food}});
  json agents=json::array();
  for(const auto& agent:snapshot.agents)
    agents.push_back({{"state",agent_json(agent.state)},{"mood",agent.mood},
                      {"available_actions",agent.available_actions}});
  json animals=json::array();
  for(const auto& animal:snapshot.animals)
    animals.push_back({{"id",animal.id},{"type",animal_type_name(animal.type)},
                       {"position",position_json(animal.position)},{"alive",animal.alive},
                       {"danger",animal.danger},{"nutrition",animal.nutrition}});
  return {{"date",calendar_json(snapshot.date)},
          {"simulation_cycle",snapshot.simulation_cycle},
          {"climate",climate_json(snapshot.climate)},
          {"phase",day_phase_name(snapshot.phase)},
          {"cycle_in_day",snapshot.cycle_in_day},{"cycles_per_day",snapshot.cycles_per_day},
          {"width",snapshot.width},{"height",snapshot.height},{"cells",std::move(cells)},
          {"agents",std::move(agents)},{"animals",std::move(animals)},
          {"recent_events",snapshot.recent_events}};
}

WebInterface::WebInterface(std::istream& input,std::ostream& output,int initial_delay_ms,
                           int frame_interval_ms)
    :input_(input),output_(output),delay_ms_(std::clamp(initial_delay_ms,0,10000)),
     frame_interval_ms_(std::max(0,frame_interval_ms)) {
  emit_status("ready");
}

void WebInterface::emit(std::string_view type,json payload) {
  output_<<web_event_prefix
         <<json{{"version",web_protocol_version},{"type",type},{"payload",std::move(payload)}}.dump()
         <<'\n'<<std::flush;
}

void WebInterface::emit_status(std::string message,std::optional<bool> accepted,
                               std::string command) {
  std::string state=completed_?"completed":stop_requested_?"stopping":paused_?"paused":"running";
  json payload={{"state",state},{"paused",paused_},{"speed",speed_},
                {"delay_ms",delay_ms_},{"api_available",api_available_},
                {"api_enabled",api_enabled_},{"message",std::move(message)}};
  if(accepted)payload["accepted"]=*accepted;
  if(!command.empty())payload["command"]=std::move(command);
  emit("status",std::move(payload));
}

void WebInterface::configure_api(bool available,bool enabled) {
  api_available_=available;
  api_enabled_=available&&enabled;
  emit_status("api_configured");
}

WebInterface::ReadResult WebInterface::read_line(bool blocking,std::string& line) {
  if(input_.rdbuf()->in_avail()>0){
    return std::getline(input_,line)?ReadResult::Line:ReadResult::Closed;
  }
  if(&input_!=&std::cin){
    if(!blocking)return ReadResult::None;
    return std::getline(input_,line)?ReadResult::Line:ReadResult::Closed;
  }
  pollfd descriptor{STDIN_FILENO,static_cast<short>(POLLIN|POLLHUP|POLLERR|POLLNVAL),0};
  int result{};
  do{result=poll(&descriptor,1,blocking?-1:0);}while(result<0&&errno==EINTR);
  if(result<0){stop_requested_=true;emit_status("stdin_poll_error",false);return ReadResult::Closed;}
  if(result==0)return ReadResult::None;
  if((descriptor.revents&(POLLERR|POLLNVAL))!=0){
    stop_requested_=true;emit_status("stdin_poll_error",false);return ReadResult::Closed;
  }
  if((descriptor.revents&(POLLIN|POLLHUP))!=0)
    return std::getline(input_,line)?ReadResult::Line:ReadResult::Closed;
  return ReadResult::None;
}

bool WebInterface::apply_runtime_command(const WebCommand& command) {
  const auto name=web_command_name(command.kind);
  switch(command.kind){
    case WebCommandKind::Pause:paused_=true;break;
    case WebCommandKind::Resume:paused_=false;break;
    case WebCommandKind::TogglePause:paused_=!paused_;break;
    case WebCommandKind::SetSpeed:speed_=command.speed;break;
    case WebCommandKind::SetDelayMs:delay_ms_=command.delay_ms;break;
    case WebCommandKind::SetApiEnabled:
      if(command.enabled&&!api_available_){
        emit_status("api_credentials_unavailable",false,name);return false;
      }
      api_enabled_=command.enabled;break;
    case WebCommandKind::Stop:stop_requested_=true;paused_=false;break;
    case WebCommandKind::Validation:
      emit_status("validation_command_unavailable",false,name);return false;
  }
  emit_status("command_accepted",true,name);
  return true;
}

bool WebInterface::drain_runtime_commands() {
  while(!stop_requested_){
    std::string line;
    const auto result=read_line(false,line);
    if(result==ReadResult::None)return true;
    if(result==ReadResult::Closed){stop_requested_=true;paused_=false;emit_status("stdin_closed");return false;}
    std::string error;
    const auto command=parse_web_command(line,error);
    if(!command){emit_status(error,false);continue;}
    apply_runtime_command(*command);
  }
  return false;
}

bool WebInterface::wait_while_paused() {
  while(paused_&&!stop_requested_){
    std::string line;
    const auto result=read_line(true,line);
    if(result==ReadResult::Closed){stop_requested_=true;paused_=false;emit_status("stdin_closed");break;}
    if(result!=ReadResult::Line)continue;
    std::string error;
    const auto command=parse_web_command(line,error);
    if(!command){emit_status(error,false);continue;}
    apply_runtime_command(*command);
  }
  return !stop_requested_;
}

bool WebInterface::present(const UiSnapshot& snapshot) {
  if(!drain_runtime_commands()||!wait_while_paused())return false;
  ++snapshot_count_;
  const auto stride=speed_>=2.0F?static_cast<std::size_t>(speed_):1U;
  if(snapshot_count_%stride!=0)return true;
  emit("snapshot",web_snapshot_json(snapshot));
  if(frame_interval_ms_>0){
    const float slow_factor=speed_<1.0F?1.0F/speed_:1.0F;
    std::this_thread::sleep_for(std::chrono::milliseconds(
        static_cast<int>(std::lround(frame_interval_ms_*slow_factor))));
  }
  return !stop_requested_;
}

bool WebInterface::idle_for(int milliseconds) {
  const auto deadline=std::chrono::steady_clock::now()+
      std::chrono::milliseconds(std::max(0,milliseconds));
  do{
    if(!drain_runtime_commands()||!wait_while_paused())return false;
    const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline-std::chrono::steady_clock::now());
    if(remaining.count()<=0)break;
    std::this_thread::sleep_for(std::min(remaining,std::chrono::milliseconds(25)));
  }while(std::chrono::steady_clock::now()<deadline);
  return !stop_requested_;
}

bool WebInterface::present_activity(const UiActivity& activity) {
  if(!drain_runtime_commands())return false;
  emit("activity",{{"kind",activity_kind_name(activity.kind)},
                   {"date",calendar_json(activity.date)},
                   {"simulation_cycle",activity.simulation_cycle},
                   {"agent_id",activity.agent_id},{"agent_name",activity.agent_name},
                   {"call_number",activity.call_number},{"total_calls",activity.total_calls},
                   {"elapsed_ms",activity.elapsed_ms}});
  return !stop_requested_;
}

int WebInterface::simulation_delay_ms(int fallback) const {
  (void)fallback;
  return delay_ms_;
}

bool WebInterface::restart_requested() const { return restart_requested_; }

bool WebInterface::present_recompilation(const RecompileProgress& progress) {
  if(!drain_runtime_commands())return false;
  emit("recompilation",{{"stage",recompile_stage_name(progress.stage)},
                        {"elapsed_ms",progress.elapsed_ms},{"detail",progress.detail}});
  return !stop_requested_;
}

std::string WebInterface::wait_for_validation(const ValidationPrompt& prompt,
                                              std::string_view event_type,json payload) {
  emit(event_type,std::move(payload));
  while(!stop_requested_){
    std::string line;
    const auto result=read_line(true,line);
    if(result==ReadResult::Closed){stop_requested_=true;paused_=false;emit_status("stdin_closed");break;}
    if(result!=ReadResult::Line)continue;
    std::string error;
    const auto command=parse_web_command(line,error);
    if(!command){emit_status(error,false);continue;}
    if(command->kind!=WebCommandKind::Validation){apply_runtime_command(*command);continue;}
    if(!web_validation_command_allowed(prompt,command->validation_text)){
      emit_status("validation_command_unavailable",false,"validation");
      continue;
    }
    emit_status("command_accepted",true,"validation");
    return command->validation_text;
  }
  return "q";
}

std::string WebInterface::request_command(const ValidationPrompt& prompt) {
  return wait_for_validation(prompt,"validation_prompt",validation_prompt_json(prompt));
}

bool WebInterface::present_evolution_progress(const EvolutionProgress& progress) {
  if(!drain_runtime_commands())return false;
  emit("evolution_progress",evolution_progress_json(progress));
  return !stop_requested_;
}

std::string WebInterface::request_evolution_completion(const EvolutionProgress& progress) {
  ValidationPrompt prompt{ValidationStage::Complete,0,0,{},0,ValidationPromptKind::Feature};
  auto payload=evolution_progress_json(progress);
  payload["allowed_commands"]={"o","q"};
  const auto command=wait_for_validation(prompt,"evolution_completion",std::move(payload));
  restart_requested_=command=="o"&&progress.stage==EvolutionProgressStage::Complete&&
                     progress.successful;
  return command;
}

void WebInterface::complete() {
  if(completed_||stop_requested_)return;
  completed_=true;
  emit_status("simulation_completed");
}

void WebInterface::fail(const std::string& message) {
  stop_requested_=true;
  paused_=false;
  emit_status(message,false);
}
}
