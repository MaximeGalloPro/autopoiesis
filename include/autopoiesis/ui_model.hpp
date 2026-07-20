#pragma once

#include "calendar.hpp"
#include "decision.hpp"
#include <optional>

namespace apo {
struct UiCell {
  Position position;
  Terrain terrain{Terrain::Ground};
  int food{};
  int wood{};
  int fibers{};
  int shelter_level{};
  int branches{};
  bool campfire{};
  int stored_food{};
  int stored_wood{};
  int stored_branches{};
  int cooked_food{};
  int crafted_items{};
  int iron_ore{};
  int stored_iron_ore{};
  std::optional<Building> building;
};

struct UiAgent {
  Agent state;
  std::string mood;
  std::vector<std::string> available_actions;
};

struct UiSnapshot {
  CalendarDate date;
  int simulation_cycle{};
  ClimateState climate;
  EcologyState ecology;
  DayPhase phase{DayPhase::Day};
  int cycle_in_day{1};
  int cycles_per_day{2400};
  int width{};
  int height{};
  std::vector<UiCell> cells;
  std::vector<UiAgent> agents;
  std::vector<Animal> animals;
  std::vector<DangerEvent> dangers;
  std::vector<std::string> recent_events;
};

enum class UiActivityKind { PeriodReport, EvolutionRequest };

struct UiActivity {
  UiActivityKind kind{UiActivityKind::PeriodReport};
  CalendarDate date;
  int simulation_cycle{};
  std::string agent_id;
  std::string agent_name;
  std::size_t call_number{};
  std::size_t total_calls{};
  long long elapsed_ms{};
};

enum class RecompileStage { Compiling, Ready, Failed };
struct RecompileProgress {
  RecompileStage stage{RecompileStage::Compiling};
  long long elapsed_ms{};
  std::string detail;
};

struct MapViewport {
  float x{};
  float y{};
  float width{};
  float height{};
};

class SimulationSpeedControl {
 public:
  void slower();
  void faster();
  void toggle_pause() { paused_=!paused_; }
  bool paused() const { return paused_; }
  float multiplier() const;
  int render_stride() const;
  int target_fps() const;
 private:
  int level_{2};
  bool paused_{};
};

class IUserInterface {
 public:
  virtual ~IUserInterface() = default;
  virtual bool present(const UiSnapshot& snapshot) = 0;
  virtual bool idle_for(int milliseconds) = 0;
  virtual bool present_activity(const UiActivity&) { return true; }
  virtual int simulation_delay_ms(int fallback) const { return fallback; }
  virtual bool restart_requested() const { return false; }
  virtual bool present_recompilation(const RecompileProgress&) { return true; }
};

std::string mood_for(const Agent& agent);
UiSnapshot make_ui_snapshot(const CalendarDate& date, int simulation_cycle,
                            const ClimateState& climate, const World& world,
                            const std::vector<Agent>& agents,
                            const std::vector<std::string>& recent_events,
                            int cycle_in_day = 1, int cycles_per_day = 2400,
                            const std::vector<DangerEvent>& dangers = {});
std::optional<Position> map_position_at_pixel(const MapViewport& viewport, float pixel_x,
                                               float pixel_y, int map_width, int map_height);
const UiAgent* agent_at_position(const UiSnapshot& snapshot, Position position);
int simulation_delay_from_slider(float pixel_x, float track_start, float track_end);
}
