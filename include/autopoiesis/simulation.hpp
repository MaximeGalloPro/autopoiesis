#pragma once
#include "calendar.hpp"
#include "capability_registry.hpp"
#include "decision.hpp"
#include "devil.hpp"
#include "group.hpp"
#include "logger.hpp"
#include "ui_model.hpp"
#include "validation.hpp"
#include <chrono>
#include <deque>
#include <functional>
#include <future>

namespace apo {
struct PeriodContext {
  CalendarDate date;
  ClimateState climate;
  json previous_memories=json::array();
};
struct EvolutionContext {
  json active_world_mechanisms=json::array();
  json evolution_history=json::array();
  std::vector<std::string> currently_available_actions;
};
class IDecider {
 public:
  virtual ~IDecider() = default;
  virtual Decision decide(const Perception&) = 0;
  virtual json checkpoint() const { return json::object(); }
  virtual void restore_checkpoint(const json&) {}
};
class ICycleReporter {
 public:
  virtual ~ICycleReporter() = default;
  virtual bool enabled() const { return true; }
  virtual json report_period(int, int, const Agent&,
                             const std::vector<std::string>&) { return nullptr; }
  virtual json report_period(int simulation_cycle, int day, const Agent& agent,
                             const std::vector<std::string>& history,
                             const PeriodContext&) {
    return report_period(simulation_cycle,day,agent,history);
  }
  virtual json request_evolution(int, int, const Agent&, const std::vector<std::string>&,
                                 const json&) { return nullptr; }
  virtual json request_evolution(int simulation_cycle, int day, const Agent& agent,
                                 const std::vector<std::string>& history, const json& report,
                                 const EvolutionContext&) {
    return request_evolution(simulation_cycle,day,agent,history,report);
  }
  virtual std::string last_error() const { return {}; }
};
class LocalDecider final : public IDecider {
 public:
  explicit LocalDecider(std::mt19937& rng) : rng_(rng) {}
  Decision decide(const Perception&) override;
  json checkpoint() const override;
  void restore_checkpoint(const json& state) override;
 private:
  struct GoalState {
    std::string name;
    int remaining{};
    std::optional<Position> exploration_target;
  };
  std::mt19937& rng_;
  std::map<std::string,GoalState> goals_;
};
struct SimulationRunResult {
  bool restart_requested{};
  int remaining_days{};
};
class Simulation {
 public:
  using ValidationGate = std::function<ValidationWindowState(
      int day, int simulation_cycle, bool open_window)>;
  Simulation(unsigned seed, IDecider& decider, Logger& logger, ICycleReporter* reporter = nullptr,
             std::string checkpoint_path = {});
  SimulationRunResult run(int days, int delay_ms, int render_every_days,
                          const ValidationGate& validation_gate = {},
                          IUserInterface* interface = nullptr);
  void run_day();
  const World& world() const { return world_; } const std::vector<Agent>& agents() const { return agents_; }
  const Group& group() const { return group_; }
  const CalendarDate& date() const { return date_; }
  const ClimateState& climate() const { return climate_; }
  int simulation_cycle() const { return simulation_cycle_; }
  bool restored_checkpoint() const { return restored_checkpoint_; }
  const std::vector<ActiveFeature>& active_features() const { return active_features_; }
  bool feature_active(const std::string& key, int version = 0) const;
  bool activate_feature(const std::string& key, int version = 0);
  void save_checkpoint() const;
  // Deliberately explicit: normal simulation ticks never wait for reporting work.
  // It is useful to drain the bounded queue in deterministic tests or at a controlled shutdown.
  void wait_for_reporting_idle(IUserInterface* interface = nullptr);
  friend struct SimulationTestAccess;
 private:
  enum class ReporterTaskKind { PeriodReport, EvolutionRequest };
  struct ReporterTask {
    ReporterTaskKind kind{ReporterTaskKind::PeriodReport};
    int day{};
    int simulation_cycle{};
    CalendarDate date;
    ClimateState climate;
    Agent agent;
    std::vector<std::string> history;
    PeriodContext period_context;
    EvolutionContext evolution_context;
    json report{nullptr};
    std::size_t call_number{};
    std::size_t total_calls{};
  };
  struct ReporterResult {
    ReporterTask task;
    json payload{nullptr};
    std::string diagnostic;
  };
  struct AiWindow {
    int day{};
    int simulation_cycle{};
    std::vector<ReporterTask> initial_reports;
    std::size_t total_calls{};
    std::size_t completed_calls{};
    bool requires_validation{};
  };
  struct ValidationWindow {
    int day{};
    int simulation_cycle{};
  };
  World world_; Group group_; std::vector<Agent> agents_; IDecider& decider_; Logger& logger_; ICycleReporter* reporter_; std::mt19937 rng_; Devil devil_;
  CalendarDate date_{date_from_absolute_day(1)};
  ClimateState climate_{climate_for(date_)};
  int cycles_per_day_{2400}; int report_every_days_{1}; int day_{0}; int simulation_cycle_{0};
  int cycle_in_day_{1};
  int next_agent_id_{4};
  std::vector<DangerEvent> dangers_;
  int next_danger_id_{1};
  std::map<std::string,std::vector<std::string>> action_history_;
  std::map<std::string,json> planning_history_;
  std::string checkpoint_path_;
  std::vector<ActiveFeature> active_features_;
  bool restored_checkpoint_{};
  bool validation_pending_{};
  int validation_day_{};
  int validation_cycle_{};
  bool validation_window_opening_{};
  std::optional<ValidationWindow> deferred_validation_window_;
  std::optional<AiWindow> active_ai_window_;
  std::optional<AiWindow> deferred_ai_window_;
  std::deque<ReporterTask> reporter_queue_;
  std::optional<ReporterTask> active_reporter_task_;
  std::optional<std::future<ReporterResult>> active_reporter_future_;
  std::chrono::steady_clock::time_point active_reporter_started_{};
  bool run_day(IUserInterface* interface);
  void schedule_ai_window(bool requires_validation);
  void activate_ai_window(AiWindow window);
  void complete_ai_window();
  bool advance_reporting(IUserInterface* interface);
  void start_next_reporter_task();
  void complete_reporter_task(ReporterResult result);
  void queue_validation_window(int day, int simulation_cycle);
  bool advance_validation(const ValidationGate& validation_gate, IUserInterface* interface,
                          int& delay_ms);
  void load_checkpoint();
  Perception perceive(Agent&); void update_needs(Agent&); void advance_action_needs(Agent&, int action_index); std::string execute(Agent&, const Decision&);
  void update_health_conditions(Agent&);
  void update_emotions(Agent&);
  void update_population();
  void update_dangers();
  void apply_climate_effects(Agent&, const CalendarDate&, const ClimateState&);
  void update_behavior_after_action(Agent&, const Agent& before, const Decision&,
                                    const std::string& result, bool succeeded);
};
}
