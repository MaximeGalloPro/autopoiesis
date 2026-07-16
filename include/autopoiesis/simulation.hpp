#pragma once
#include "decision.hpp"
#include "logger.hpp"
#include <functional>

namespace apo {
class IDecider { public: virtual ~IDecider() = default; virtual Decision decide(const Perception&) = 0; };
class ICycleReporter { public: virtual ~ICycleReporter() = default; virtual json report_cycle(int cycle, const Agent& agent, const std::vector<std::string>& history) = 0; };
class LocalDecider final : public IDecider {
 public: explicit LocalDecider(std::mt19937& rng) : rng_(rng) {} Decision decide(const Perception&) override;
 private: std::mt19937& rng_;
};
class Simulation {
 public:
  Simulation(unsigned seed, IDecider& decider, Logger& logger, ICycleReporter* reporter = nullptr);
  void run(int cycles, int delay_ms, int render_every);
  void cycle();
  const World& world() const { return world_; } const std::vector<Agent>& agents() const { return agents_; }
 private:
  static constexpr int actions_per_cycle_=240, report_every_cycles_=5;
  World world_; std::vector<Agent> agents_; IDecider& decider_; Logger& logger_; ICycleReporter* reporter_; std::mt19937 rng_; int cycle_{0};
  std::map<std::string,std::vector<std::string>> action_history_;
  Perception perceive(Agent&); void update_needs(Agent&); void advance_action_needs(Agent&, int action_index); std::string execute(Agent&, const Decision&);
};
}
