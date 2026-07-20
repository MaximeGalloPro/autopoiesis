#include "autopoiesis/simulation.hpp"
#include "autopoiesis/runtime_handoff.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using namespace apo;

namespace {
class RestartInterface final : public IUserInterface {
 public:
  bool present(const UiSnapshot&) override { return true; }
  bool idle_for(int) override { return true; }
  bool restart_requested() const override { return restart; }
  bool restart{true};
};

class RecompileInterface final : public IUserInterface {
 public:
  bool present(const UiSnapshot&) override { return true; }
  bool idle_for(int) override { return true; }
  bool present_recompilation(const RecompileProgress& progress) override {
    ++updates;
    final_stage=progress.stage;
    return true;
  }
  int updates{};
  RecompileStage final_stage{RecompileStage::Compiling};
};

void assert_same_agents(const std::vector<Agent>& left,const std::vector<Agent>& right) {
  assert(left.size()==right.size());
  for(std::size_t index=0;index<left.size();++index){
    assert(left[index].id==right[index].id);
    assert(left[index].position==right[index].position);
    assert(left[index].health==right[index].health);
    assert(left[index].hunger==right[index].hunger);
    assert(left[index].thirst==right[index].thirst);
    assert(left[index].fatigue==right[index].fatigue);
    assert(left[index].memories==right[index].memories);
    assert(left[index].map_memory==right[index].map_memory);
    assert(left[index].map_visit_counts==right[index].map_visit_counts);
    assert(left[index].known_campfires==right[index].known_campfires);
    assert(left[index].home_camp==right[index].home_camp);
    assert(left[index].camp_rest_position==right[index].camp_rest_position);
    assert(left[index].wood_inventory==right[index].wood_inventory);
    assert(left[index].branch_inventory==right[index].branch_inventory);
    assert(left[index].iron_ore_inventory==right[index].iron_ore_inventory);
    assert(left[index].carried_food==right[index].carried_food);
    assert(left[index].equipped_tool==right[index].equipped_tool);
    assert(left[index].community_role==right[index].community_role);
    assert(left[index].last_shared_meal_day==right[index].last_shared_meal_day);
    assert(left[index].last_vigil_day==right[index].last_vigil_day);
    assert(left[index].celebration_pending==right[index].celebration_pending);
    assert(left[index].mourned_agents==right[index].mourned_agents);
    assert(left[index].skills==right[index].skills);
    assert(left[index].last_taught_day==right[index].last_taught_day);
    assert(left[index].last_lesson_day==right[index].last_lesson_day);
    assert(left[index].conditions==right[index].conditions);
    assert(left[index].next_condition_id==right[index].next_condition_id);
    assert(left[index].last_convalescence_day==right[index].last_convalescence_day);
    assert(left[index].emotions==right[index].emotions);
    assert(left[index].next_emotion_id==right[index].next_emotion_id);
    assert(left[index].companion_id==right[index].companion_id);
    assert(left[index].companion_until_day==right[index].companion_until_day);
    assert(left[index].last_help_day==right[index].last_help_day);
    assert(left[index].last_warning_day==right[index].last_warning_day);
    assert(left[index].age_days==right[index].age_days);
    assert(left[index].origin==right[index].origin);
    assert(left[index].arrival_day==right[index].arrival_day);
    assert(left[index].parent_ids==right[index].parent_ids);
    assert(left[index].departure_day==right[index].departure_day);
    assert(left[index].departure_reason==right[index].departure_reason);
    assert(left[index].death_cause==right[index].death_cause);
    assert(project_json(left[index].project)==project_json(right[index].project));
    assert(relationships_json(left[index].relationships)==relationships_json(right[index].relationships));
  }
}
}

int main() {
  const auto arguments=restart_arguments(
      {"/tmp/autopoiesis_gui","--no-api","--days","100","--delay-ms","500",
       "--seed","42","--new-world"},2,2500);
  assert((arguments==std::vector<std::string>{"/tmp/autopoiesis_gui","--no-api","--seed","42",
                                              "--days","2","--delay-ms","2500"}));
  const pid_t replacement=fork();
  assert(replacement>=0);
  if(replacement==0)replace_process("/usr/bin/true",{"/usr/bin/true"});
  int replacement_status{};
  assert(waitpid(replacement,&replacement_status,0)==replacement);
  assert(WIFEXITED(replacement_status)&&WEXITSTATUS(replacement_status)==0);
  if(const char* project_root=std::getenv("AUTOPOIESIS_TEST_RECOMPILE_ROOT")){
    RecompileInterface recompile_interface;
    assert(recompile_gui(project_root,"/tmp/autopoiesis-recompile-test",recompile_interface)==0);
    assert(recompile_interface.updates>0);
    assert(recompile_interface.final_stage==RecompileStage::Ready);
  }
  setenv("CYCLES_PER_DAY","4",1);
  setenv("REPORT_EVERY_DAYS","3",1);
  const auto root=std::filesystem::path("/tmp")/
      ("autopoiesis-checkpoint-"+std::to_string(getpid()));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto checkpoint=root/"simulation-state.json";

  std::mt19937 uninterrupted_rng(42);
  LocalDecider uninterrupted_decider(uninterrupted_rng);
  Logger uninterrupted_logger((root/"uninterrupted").string());
  Simulation uninterrupted(42,uninterrupted_decider,uninterrupted_logger);
  uninterrupted.run(3,0,0);

  std::mt19937 first_rng(42);
  LocalDecider first_decider(first_rng);
  Logger first_logger((root/"split").string());
  Simulation first(42,first_decider,first_logger,nullptr,checkpoint.string());
  first.run(2,0,0);
  assert(std::filesystem::exists(checkpoint));
  assert(first.date().absolute_day==2);
  assert(first.simulation_cycle()==8);

  std::mt19937 resumed_rng(999);
  LocalDecider resumed_decider(resumed_rng);
  Logger resumed_logger((root/"split").string());
  Simulation resumed(999,resumed_decider,resumed_logger,nullptr,checkpoint.string());
  assert(resumed.restored_checkpoint());
  assert(resumed.date().absolute_day==2);
  assert(resumed.simulation_cycle()==8);
  resumed.run(1,0,0);

  assert(resumed.date().absolute_day==3);
  assert(resumed.simulation_cycle()==12);
  assert(resumed.world().checkpoint()==uninterrupted.world().checkpoint());
  assert_same_agents(resumed.agents(),uninterrupted.agents());

  const auto restart_checkpoint=root/"restart-state.json";
  std::mt19937 restart_rng(42);
  LocalDecider restart_decider(restart_rng);
  Logger restart_logger((root/"restart").string());
  setenv("REPORT_EVERY_DAYS","1",1);
  Simulation restart_simulation(42,restart_decider,restart_logger,nullptr,
                                restart_checkpoint.string());
  RestartInterface interface;
  const auto result=restart_simulation.run(3,0,0,[](int,int){return true;},&interface);
  assert(result.restart_requested);
  assert(result.remaining_days==2);
  assert(std::filesystem::exists(restart_checkpoint));

  unsetenv("CYCLES_PER_DAY");
  unsetenv("REPORT_EVERY_DAYS");
  std::filesystem::remove_all(root);
}
