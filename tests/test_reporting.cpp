#include "autopoiesis/simulation.hpp"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
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
  int delay_ms{};

  json report_period(int simulation_cycle, int day, const Agent& agent,
                     const std::vector<std::string>&,
                     const PeriodContext& context) override {
    if(delay_ms>0)std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
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
    if(delay_ms>0)std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    assert(report.value("day_summary", "") == "three days");
    events.push_back("request:" + agent.id + ":" + std::to_string(day) + ":" +
                     std::to_string(simulation_cycle));
    return nullptr;
  }
};

struct ActivityInterface final : IUserInterface {
  bool present(const UiSnapshot&) override { return true; }
  bool idle_for(int) override { return true; }
  bool present_activity(const UiActivity& activity) override {
    activities.push_back(activity);
    return true;
  }
  std::vector<UiActivity> activities;
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

  SplitReporter delayed_reporter;
  delayed_reporter.delay_ms=30;
  ActivityInterface activity_interface;
  Simulation animated(42,decider,logger,&delayed_reporter);
  animated.run(1,0,0,{},&activity_interface);
  assert(delayed_reporter.events.size()==6);
  assert(!activity_interface.activities.empty());
  std::size_t previous_call=1;
  std::vector<std::size_t> observed_calls;
  for(const auto& activity:activity_interface.activities){
    assert(activity.total_calls==6);
    assert(activity.call_number>=previous_call);
    assert(activity.call_number<=6);
    assert(activity.elapsed_ms>=0);
    if(observed_calls.empty()||observed_calls.back()!=activity.call_number)
      observed_calls.push_back(activity.call_number);
    previous_call=activity.call_number;
  }
  assert(observed_calls==std::vector<std::size_t>({1,2,3,4,5,6}));
  assert(activity_interface.activities.front().kind==UiActivityKind::PeriodReport);
  assert(activity_interface.activities.front().agent_name=="Ada");
  assert(activity_interface.activities.back().kind==UiActivityKind::EvolutionRequest);
  assert(activity_interface.activities.back().agent_name=="Cyra");

  unsetenv("CYCLES_PER_DAY");
  unsetenv("REPORT_EVERY_DAYS");
}
