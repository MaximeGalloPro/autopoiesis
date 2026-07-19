#include "autopoiesis/ui_model.hpp"
#include "autopoiesis/simulation.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <unistd.h>

using namespace apo;

namespace {
class FakeInterface final : public IUserInterface {
 public:
  bool present(const UiSnapshot& snapshot) override {
    ++present_count;
    last_snapshot=snapshot;
    return stay_open;
  }
  bool idle_for(int milliseconds) override {
    idle_milliseconds+=milliseconds;
    return true;
  }
  int simulation_delay_ms(int fallback) const override {
    return selected_delay_ms>=0?selected_delay_ms:fallback;
  }
  int present_count{};
  int idle_milliseconds{};
  bool stay_open{true};
  int selected_delay_ms{-1};
  UiSnapshot last_snapshot;
};
}

int main() {
  World world(42);
  Agent ada;
  ada.id = "a1";
  ada.name = "Ada";
  ada.position = {3, 2};
  ada.health = 92;
  ada.hunger = 44;
  ada.thirst = 81;
  ada.fatigue = 37;
  ada.boredom = 12;
  ada.behavior.archetype = "builder";
  ada.behavior.aspiration = "Créer un foyer sûr";
  ada.project = {"build_shelter", "Préparer un abri", ProjectStatus::Active,
                 0, 1, 3, "", "", 1, 10};

  const auto date = date_from_absolute_day(95);
  const auto climate = climate_for(date);
  const auto snapshot = make_ui_snapshot(date, 22800, climate, world, {ada},
                                         {"Ada explore", "Le printemps revient"});

  assert(snapshot.date.absolute_day == 95);
  assert(snapshot.width == World::width);
  assert(snapshot.height == World::height);
  assert(snapshot.cells.size() == static_cast<std::size_t>(World::width * World::height));
  assert(snapshot.agents.size() == 1);
  assert(snapshot.agents[0].state.name == "Ada");
  assert(snapshot.agents[0].mood == "Tension liée à la soif");
  assert(std::find(snapshot.agents[0].available_actions.begin(),
                   snapshot.agents[0].available_actions.end(), "move") !=
         snapshot.agents[0].available_actions.end());
  assert(snapshot.recent_events.size() == 2);

  const MapViewport viewport{20.0F, 80.0F, 800.0F, 480.0F};
  const auto position = map_position_at_pixel(viewport, 20.0F + 3.5F * 20.0F,
                                               80.0F + 2.5F * 20.0F,
                                               snapshot.width, snapshot.height);
  assert((position == Position{3, 2}));
  const auto* selected = agent_at_position(snapshot, *position);
  assert(selected != nullptr);
  assert(selected->state.id == "a1");
  assert(!map_position_at_pixel(viewport, 19.0F, 90.0F, snapshot.width, snapshot.height));

  const std::string data_directory="/tmp/autopoiesis-ui-model-"+std::to_string(getpid());
  Logger logger(data_directory);
  setenv("CYCLES_PER_DAY", "4", 1);
  setenv("REPORT_EVERY_DAYS", "3", 1);
  std::mt19937 rng(42);
  LocalDecider decider(rng);
  Simulation simulation(42,decider,logger);
  FakeInterface interface;
  simulation.run(1,7,1,{},&interface);
  assert(interface.present_count==4);
  assert(interface.idle_milliseconds==7);
  assert(interface.last_snapshot.date.absolute_day==1);
  assert(interface.last_snapshot.simulation_cycle==4);
  assert(interface.last_snapshot.agents.size()==3);

  Logger closing_logger(data_directory+"-closing");
  std::mt19937 closing_rng(42);
  LocalDecider closing_decider(closing_rng);
  Simulation closing_simulation(42,closing_decider,closing_logger);
  FakeInterface closing_interface;
  closing_interface.stay_open=false;
  closing_simulation.run(3,0,1,{},&closing_interface);
  assert(closing_interface.present_count==1);
  assert(closing_simulation.date().absolute_day==1);
  assert(closing_interface.last_snapshot.simulation_cycle==1);

  setenv("REPORT_EVERY_DAYS", "1", 1);
  Logger speed_logger(data_directory+"-speed");
  std::mt19937 speed_rng(42);
  LocalDecider speed_decider(speed_rng);
  Simulation speed_simulation(42,speed_decider,speed_logger);
  FakeInterface speed_interface;
  speed_interface.selected_delay_ms=2500;
  speed_simulation.run(1,7,1,[](int,int){return true;},&speed_interface);
  assert(speed_interface.idle_milliseconds==2500);

  assert(simulation_delay_from_slider(100.0F,100.0F,500.0F)==0);
  assert(simulation_delay_from_slider(300.0F,100.0F,500.0F)==5000);
  assert(simulation_delay_from_slider(500.0F,100.0F,500.0F)==10000);

  unsetenv("CYCLES_PER_DAY");
  unsetenv("REPORT_EVERY_DAYS");

  std::cout << "graphical ui model tests passed\n";
}
