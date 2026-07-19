#include "autopoiesis/raylib_interface.hpp"

using namespace apo;

int main() {
  RaylibInterface interface;
  UiActivity activity;
  activity.kind=UiActivityKind::EvolutionRequest;
  activity.date=date_from_absolute_day(93);
  activity.simulation_cycle=22320;
  activity.agent_id="a2";
  activity.agent_name="Borin";
  activity.call_number=4;
  activity.total_calls=6;
  activity.elapsed_ms=12600;
  interface.present_activity(activity);
  interface.present_activity(activity);
}
