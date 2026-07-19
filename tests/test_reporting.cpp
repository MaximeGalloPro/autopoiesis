#include "autopoiesis/simulation.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace apo;

struct WaitDecider final : IDecider {
  Decision decide(const Perception&) override {
    return {DecisionType::Action, "wait", json::object(), "rest"};
  }
};

struct SplitReporter final : ICycleReporter {
  std::vector<std::string> events;
  std::vector<std::size_t> memory_sizes;
  std::vector<CalendarDate> dates;

  json report_period(int simulation_cycle, int day, const Agent& agent,
                     const std::vector<std::string>&,
                     const PeriodContext& context) override {
    events.push_back("report:" + agent.id + ":" + std::to_string(day) + ":" +
                     std::to_string(simulation_cycle));
    memory_sizes.push_back(context.previous_memories.size());
    dates.push_back(context.date);
    return {{"character_voice", agent.name},
            {"day_summary", "three days"},
            {"state_assessment", "stable"},
            {"memory_summary", "Bilan bref."},
            {"memory_feeling", "Ressenti bref."},
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

struct FailingReporter final : ICycleReporter {
  json report_period(int, int, const Agent&, const std::vector<std::string>&) override {
    return nullptr;
  }
  json request_evolution(int, int, const Agent&, const std::vector<std::string>&,
                         const json&) override {
    return nullptr;
  }
  std::string last_error() const override {
    return "HTTP 429 en 0.42 s | type=rate_limit_error";
  }
};

int main() {
  setenv("CYCLES_PER_DAY", "240", 1);
  setenv("REPORT_EVERY_DAYS", "3", 1);

  WaitDecider decider;
  const std::filesystem::path directory="/tmp/autopoiesis-reporting-tests";
  std::filesystem::remove_all(directory);
  Logger logger(directory.string());
  SplitReporter reporter;
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
  assert(reporter.memory_sizes == std::vector<std::size_t>({0, 0, 0}));
  assert(reporter.dates.front().absolute_day == 3);
  assert(reporter.dates.front().month == 1);

  simulation.run(3, 0, 0);
  assert(reporter.events.size() == 12);
  assert(reporter.events[6] == "report:a1:6:1440");
  assert(reporter.events[7] == "request:a1:6:1440");
  assert(reporter.memory_sizes == std::vector<std::size_t>({0, 0, 0, 1, 1, 1}));
  assert(reporter.dates.back().absolute_day == 6);

  setenv("CYCLES_PER_DAY", "1", 1);
  setenv("REPORT_EVERY_DAYS", "1", 1);
  FailingReporter failing;
  Simulation failed(42, decider, logger, &failing);
  std::ostringstream terminal;
  auto* previous_buffer = std::cout.rdbuf(terminal.rdbuf());
  failed.run(1, 0, 0);
  std::cout.rdbuf(previous_buffer);
  assert(terminal.str().find("Diagnostic API : HTTP 429") != std::string::npos);

  unsetenv("CYCLES_PER_DAY");
  unsetenv("REPORT_EVERY_DAYS");
}
