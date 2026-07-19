#pragma once
#include "calendar.hpp"
#include "decision.hpp"
#include "devil.hpp"
#include "logger.hpp"
#include "ui_model.hpp"
#include <functional>

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
class IDecider { public: virtual ~IDecider() = default; virtual Decision decide(const Perception&) = 0; };
class ICycleReporter {
 public:
  virtual ~ICycleReporter() = default;
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
 public: explicit LocalDecider(std::mt19937& rng) : rng_(rng) {} Decision decide(const Perception&) override;
 private:
  struct GoalState { std::string name; int remaining{}; };
  std::mt19937& rng_;
  std::map<std::string,GoalState> goals_;
};
class Simulation {
 public:
  using ValidationGate = std::function<bool(int day, int simulation_cycle)>;
  Simulation(unsigned seed, IDecider& decider, Logger& logger, ICycleReporter* reporter = nullptr);
  void run(int days, int delay_ms, int render_every_days,
           const ValidationGate& validation_gate = {}, IUserInterface* interface = nullptr);
  void run_day();
  const World& world() const { return world_; } const std::vector<Agent>& agents() const { return agents_; }
  const CalendarDate& date() const { return date_; }
  const ClimateState& climate() const { return climate_; }
  friend struct SimulationTestAccess;
 private:
  World world_; std::vector<Agent> agents_; IDecider& decider_; Logger& logger_; ICycleReporter* reporter_; std::mt19937 rng_; Devil devil_;
  CalendarDate date_{date_from_absolute_day(1)};
  ClimateState climate_{climate_for(date_)};
  int cycles_per_day_{240}; int report_every_days_{1}; int day_{0}; int simulation_cycle_{0};
  std::map<std::string,std::vector<std::string>> action_history_;
  std::map<std::string,json> planning_history_;
  bool run_day(IUserInterface* interface);
  Perception perceive(Agent&); void update_needs(Agent&); void advance_action_needs(Agent&, int action_index); std::string execute(Agent&, const Decision&);
  void apply_climate_effects(Agent&, const CalendarDate&, const ClimateState&);
  void update_behavior_after_action(Agent&, const Agent& before, const Decision&,
                                    const std::string& result, bool succeeded);
};
}
