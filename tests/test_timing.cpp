#include "autopoiesis/simulation.hpp"
#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>

using namespace apo;

struct TimingDecider final : IDecider {
  Decision decide(const Perception&) override {
    return {DecisionType::Action, "wait", json::object(), "rest"};
  }
};

struct TimingReporter final : ICycleReporter {
  std::vector<std::string> events;

  json report_period(int simulation_cycle, int day, const Agent& agent,
                     const std::vector<std::string>&) override {
    events.push_back("report:" + agent.id + ":" + std::to_string(day) + ":" +
                     std::to_string(simulation_cycle));
    return {{"character_voice", agent.name},
            {"day_summary", "three days"},
            {"state_assessment", "stable"},
            {"ask_god", true}};
  }

  json request_evolution(int simulation_cycle, int day, const Agent& agent,
                         const std::vector<std::string>&,
                         const json& report) override {
    assert(report.value("day_summary", "") == "three days");
    events.push_back("request:" + agent.id + ":" + std::to_string(day) + ":" +
                     std::to_string(simulation_cycle));
    return nullptr;
  }
};

int main() {
  setenv("CYCLES_PER_DAY", "240", 1);
  setenv("REPORT_EVERY_DAYS", "3", 1);

  TimingDecider decider;
  Logger logger("/tmp/autopoiesis-timing-tests");
  TimingReporter reporter;
  Simulation simulation(42, decider, logger, &reporter);

  simulation.run(2, 0, 0);
  assert(reporter.events.empty());

  simulation.run(1, 0, 0);
  assert(reporter.events.size() == 6);
  assert(reporter.events[0] == "report:a1:3:720");
  assert(reporter.events[1] == "request:a1:3:720");
  assert(reporter.events[2] == "report:a2:3:720");
  assert(reporter.events[3] == "request:a2:3:720");
  assert(reporter.events[4] == "report:a3:3:720");
  assert(reporter.events[5] == "request:a3:3:720");

  int validation_gates = 0;
  simulation.run(3, 0, 0, [&](int day, int simulation_cycle) {
    ++validation_gates;
    assert(day == 6);
    assert(simulation_cycle == 1440);
    return false;
  });
  assert(validation_gates == 1);

  unsetenv("CYCLES_PER_DAY");
  unsetenv("REPORT_EVERY_DAYS");
}
