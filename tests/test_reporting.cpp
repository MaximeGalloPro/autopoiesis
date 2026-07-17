#include "autopoiesis/simulation.hpp"
#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>

using namespace apo;

struct WaitDecider final : IDecider {
  Decision decide(const Perception&) override { return {DecisionType::Action, "wait", json::object(), "rest"}; }
};

struct SplitReporter final : ICycleReporter {
  std::vector<std::string> events;

  json report_cycle(int cycle, const Agent& agent, const std::vector<std::string>&) override {
    events.push_back("report:" + agent.id + ":" + std::to_string(cycle));
    return {{"character_voice", "Ada"}, {"day_summary", "three days"}, {"state_assessment", "stable"}, {"ask_god", true}};
  }

  json request_evolution(int cycle, const Agent& agent, const std::vector<std::string>&, const json& report) override {
    assert(report.value("day_summary", "") == "three days");
    events.push_back("request:" + agent.id + ":" + std::to_string(cycle));
    return nullptr;
  }
};

int main() {
  setenv("REPORT_EVERY_CYCLES", "3", 1);
  WaitDecider decider;
  Logger logger("/tmp/autopoiesis-reporting-tests");
  SplitReporter reporter;
  Simulation simulation(42, decider, logger, &reporter);
  simulation.run(3, 0, 0);

  assert(reporter.events.size() == 6);
  assert(reporter.events[0] == "report:a1:3");
  assert(reporter.events[1] == "request:a1:3");
  assert(reporter.events[2] == "report:a2:3");
  assert(reporter.events[3] == "request:a2:3");
  assert(reporter.events[4] == "report:a3:3");
  assert(reporter.events[5] == "request:a3:3");
  unsetenv("REPORT_EVERY_CYCLES");
}
