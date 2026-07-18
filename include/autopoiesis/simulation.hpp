#pragma once
#include "decision.hpp"
#include "logger.hpp"
#include <functional>

namespace apo {
class IDecider { public: virtual ~IDecider() = default; virtual Decision decide(const Perception&) = 0; };
class ICycleReporter {
 public:
  virtual ~ICycleReporter() = default;
  virtual json report_period(int simulation_cycle, int day, const Agent& agent,
                             const std::vector<std::string>& history) = 0;
  virtual json request_evolution(int, int, const Agent&, const std::vector<std::string>&,
                                 const json&) { return nullptr; }
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
           const ValidationGate& validation_gate = {});
  void run_day();
  const World& world() const { return world_; } const std::vector<Agent>& agents() const { return agents_; }
  friend struct SimulationTestAccess;
 private:
  World world_; std::vector<Agent> agents_; IDecider& decider_; Logger& logger_; ICycleReporter* reporter_; std::mt19937 rng_;
  int cycles_per_day_{240}; int report_every_days_{1}; int day_{0}; int simulation_cycle_{0};
  std::map<std::string,std::vector<std::string>> action_history_;
  std::map<std::string,json> planning_history_;
  Perception perceive(Agent&); void update_needs(Agent&); void advance_action_needs(Agent&, int action_index); std::string execute(Agent&, const Decision&);
};
}
