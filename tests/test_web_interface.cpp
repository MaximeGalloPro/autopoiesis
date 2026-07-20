#include "autopoiesis/web_interface.hpp"

#include <cassert>
#include <sstream>
#include <string>
#include <vector>

using namespace apo;

namespace {
std::vector<json> emitted_events(const std::string& output) {
  constexpr std::string_view prefix="AUTOPOIESIS_EVENT ";
  std::vector<json> events;
  std::istringstream lines(output);
  std::string line;
  while(std::getline(lines,line)){
    assert(line.starts_with(prefix));
    const auto event=json::parse(line.substr(prefix.size()));
    assert(event.at("version")==1);
    assert(event.at("type").is_string());
    assert(event.at("payload").is_object());
    events.push_back(event);
  }
  return events;
}

UiSnapshot sample_snapshot() {
  UiSnapshot snapshot;
  snapshot.date=date_from_absolute_day(95);
  snapshot.simulation_cycle=22801;
  snapshot.climate=climate_for(snapshot.date);
  snapshot.phase=DayPhase::Night;
  snapshot.cycle_in_day=1801;
  snapshot.cycles_per_day=2400;
  snapshot.width=40;
  snapshot.height=24;
  snapshot.cells.push_back({{3,2},Terrain::Bush,2,4,1,2,3,true,5});

  Agent agent;
  agent.id="a1";
  agent.name="Ada";
  agent.position={3,2};
  agent.health=92;
  agent.hunger=44;
  agent.thirst=81;
  agent.fatigue=37;
  agent.boredom=12;
  agent.memories={"Un souvenir"};
  agent.map_memory[std::pair{3,2}]=Terrain::Bush;
  agent.map_visit_counts[std::pair{3,2}]=4;
  agent.behavior.archetype="builder";
  agent.behavior.aspiration="Créer un foyer sûr";
  agent.behavior.preferred_foods={FoodType::Berries};
  agent.project={"build_shelter","Préparer un abri",ProjectStatus::Active,
                 1,2,3,"","",1,10};
  agent.relationships["a2"]={6,7,2};
  agent.observed_animals.insert("rabbit");
  agent.known_campfires.insert({3,2});
  agent.wood_inventory=2;
  agent.branch_inventory=1;
  agent.carried_food=FoodItem{FoodType::Roots,18};
  agent.shelter_construction=ShelterConstruction{{3,2},2};
  snapshot.agents.push_back({agent,"Tension liée à la soif",{"move","drink"}});
  snapshot.animals.push_back({"rabbit-1",AnimalType::Rabbit,{4,2},true,0,35});
  snapshot.recent_events={"Ada explore"};
  return snapshot;
}
}

int main() {
  std::string error;
  auto command=parse_web_command(R"({"version":1,"command":"pause"})",error);
  assert(command&&command->kind==WebCommandKind::Pause);
  command=parse_web_command(R"({"version":1,"command":"resume"})",error);
  assert(command&&command->kind==WebCommandKind::Resume);
  command=parse_web_command(R"({"version":1,"command":"toggle_pause"})",error);
  assert(command&&command->kind==WebCommandKind::TogglePause);
  for(const float speed:{0.25F,0.5F,1.0F,2.0F,4.0F}){
    command=parse_web_command(json{{"version",1},{"command","set_speed"},{"speed",speed}}.dump(),error);
    assert(command&&command->kind==WebCommandKind::SetSpeed&&command->speed==speed);
  }
  command=parse_web_command(R"({"version":1,"command":"set_delay_ms","delay_ms":0})",error);
  assert(command&&command->delay_ms==0);
  command=parse_web_command(R"({"version":1,"command":"set_delay_ms","delay_ms":10000})",error);
  assert(command&&command->delay_ms==10000);
  command=parse_web_command(R"({"version":1,"command":"set_api_enabled","enabled":true})",error);
  assert(command&&command->kind==WebCommandKind::SetApiEnabled&&command->enabled);
  command=parse_web_command(R"({"version":1,"command":"validation","text":"a"})",error);
  assert(command&&command->validation_text=="a");
  assert(!parse_web_command(R"({"version":2,"command":"pause"})",error));
  assert(!parse_web_command(R"({"version":1,"command":"set_speed","speed":1.5})",error));
  assert(!parse_web_command(R"({"version":1,"command":"set_delay_ms","delay_ms":10001})",error));
  assert(!parse_web_command(R"({"version":1,"command":"validation","text":2})",error));
  assert(!parse_web_command(R"({"version":1,"command":"set_api_enabled","enabled":1})",error));
  assert(!parse_web_command(R"({"version":1,"command":"pause","extra":true})",error));
  assert(!parse_web_command(R"({"version":1,"command":"unknown"})",error));
  assert(!parse_web_command("not json",error));

  ValidationPrompt choose{ValidationStage::Choose,3,720,{json{{"id","one"}},json{{"id","two"}}},0};
  assert(web_validation_command_allowed(choose,"1"));
  assert(web_validation_command_allowed(choose,"2"));
  assert(web_validation_command_allowed(choose,"d 2"));
  assert(web_validation_command_allowed(choose,"n"));
  assert(!web_validation_command_allowed(choose,"3"));
  assert(!web_validation_command_allowed(choose,"a"));
  ValidationPrompt devil{ValidationStage::Confirm,3,720,{json{{"id","devil-1"}}},1,
                          ValidationPromptKind::Devil};
  assert(web_validation_command_allowed(devil,"a"));
  assert(web_validation_command_allowed(devil,"r"));
  assert(web_validation_command_allowed(devil,"d"));
  assert(!web_validation_command_allowed(devil,"o"));

  const auto snapshot=sample_snapshot();
  const auto serialized=web_snapshot_json(snapshot);
  assert(serialized.at("date").at("absolute_day")==95);
  assert(serialized.at("phase")=="night");
  assert(serialized.at("cells").at(0).at("stored_food")==5);
  assert(serialized.at("agents").at(0).at("state").at("carried_food").at("type")=="roots");
  assert(serialized.at("agents").at(0).at("state").at("map_memory").at(0).at("terrain")=="bush");
  assert(serialized.at("agents").at(0).at("state").at("relationships").at("a2").at("trust")==6);
  assert(serialized.at("animals").at(0).at("type")=="rabbit");

  std::istringstream runtime_input(
      R"({"version":1,"command":"set_speed","speed":4})" "\n"
      R"({"version":1,"command":"set_delay_ms","delay_ms":10000})" "\n"
      R"({"version":1,"command":"set_api_enabled","enabled":true})" "\n"
      R"({"version":1,"command":"pause"})" "\n"
      R"({"version":1,"command":"resume"})" "\n"
      R"({"version":1,"command":"toggle_pause"})" "\n"
      R"({"version":1,"command":"toggle_pause"})" "\n"
      R"({"version":1,"command":"set_speed","speed":3})" "\n");
  std::ostringstream runtime_output;
  WebInterface runtime(runtime_input,runtime_output,500,0);
  runtime.configure_api(true,false);
  for(int index=0;index<4;++index)assert(runtime.present(snapshot));
  assert(runtime.speed_multiplier()==4.0F);
  assert(runtime.simulation_delay_ms(0)==10000);
  assert(!runtime.paused());
  assert(runtime.api_enabled());
  const auto runtime_events=emitted_events(runtime_output.str());
  int snapshot_events=0;
  bool rejected_speed=false;
  for(const auto& event:runtime_events){
    if(event.at("type")=="snapshot")++snapshot_events;
    if(event.at("type")=="status"&&!event.at("payload").value("accepted",true))
      rejected_speed=event.at("payload").value("message","")=="unsupported_speed";
  }
  assert(snapshot_events==1);
  assert(rejected_speed);

  std::istringstream validation_input(
      R"({"version":1,"command":"validation","text":"o"})" "\n"
      R"({"version":1,"command":"validation","text":"a"})" "\n");
  std::ostringstream validation_output;
  WebInterface validation(validation_input,validation_output,500,0);
  assert(validation.request_command(devil)=="a");
  const auto validation_events=emitted_events(validation_output.str());
  bool saw_devil_prompt=false;
  bool rejected_unavailable_command=false;
  for(const auto& event:validation_events){
    if(event.at("type")=="validation_prompt")
      saw_devil_prompt=event.at("payload").at("kind")=="devil";
    if(event.at("type")=="status"&&!event.at("payload").value("accepted",true))
      rejected_unavailable_command=event.at("payload").value("command","")=="validation";
  }
  assert(saw_devil_prompt);
  assert(rejected_unavailable_command);

  const EvolutionProgress completed{EvolutionProgressStage::Complete,"request-1",
                                    "Active","Commit abc",12,true};
  std::istringstream completion_input(
      R"({"version":1,"command":"validation","text":"o"})" "\n");
  std::ostringstream completion_output;
  WebInterface completion(completion_input,completion_output,500,0);
  assert(completion.request_evolution_completion(completed)=="o");
  assert(completion.restart_requested());
  const auto completion_events=emitted_events(completion_output.str());
  assert(completion_events.at(1).at("type")=="evolution_completion");

  std::istringstream progress_input;
  std::ostringstream progress_output;
  WebInterface progress(progress_input,progress_output,500,0);
  assert(progress.present_activity({UiActivityKind::EvolutionRequest,snapshot.date,
                                    snapshot.simulation_cycle,"a1","Ada",2,6,42}));
  assert(progress.present_evolution_progress(completed));
  assert(progress.present_recompilation({RecompileStage::Compiling,84,"building"}));
  const auto progress_events=emitted_events(progress_output.str());
  assert(progress_events.at(1).at("type")=="activity");
  assert(progress_events.at(1).at("payload").at("kind")=="evolution_request");
  assert(progress_events.at(2).at("type")=="evolution_progress");
  assert(progress_events.at(2).at("payload").at("stage")=="complete");
  assert(progress_events.at(3).at("type")=="recompilation");
  assert(progress_events.at(3).at("payload").at("stage")=="compiling");

  std::istringstream stop_input(R"({"version":1,"command":"stop"})" "\n");
  std::ostringstream stop_output;
  WebInterface stopped(stop_input,stop_output,500,0);
  assert(!stopped.present(snapshot));
  assert(stopped.stop_requested());

  std::istringstream closed_input;
  std::ostringstream closed_output;
  WebInterface closed(closed_input,closed_output,500,0);
  assert(closed.request_command(devil)=="q");
  assert(closed.stop_requested());
}
