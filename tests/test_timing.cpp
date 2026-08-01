#include "autopoiesis/simulation.hpp"
#include <cassert>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

using namespace apo;

struct TimingDecider final : IDecider {
  std::map<std::string, int> calls;
  Decision decide(const Perception& perception) override {
    ++calls[perception.value.at("self").at("id").get<std::string>()];
    return {DecisionType::Action, "wait", json::object(), "rest"};
  }
};

struct TimingReporter final : ICycleReporter {
  std::vector<std::string> events;
  std::mutex mutex;
  std::condition_variable condition;
  bool first_call_started{};
  bool release_first_call{};

  json report_period(int simulation_cycle, int day, const Agent& agent,
                     const std::vector<std::string>&) override {
    {
      std::unique_lock lock(mutex);
      if(!first_call_started){
        first_call_started=true;
        condition.notify_all();
        condition.wait(lock,[&]{return release_first_call;});
      }
    }
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

  void wait_until_first_call_starts() {
    std::unique_lock lock(mutex);
    condition.wait(lock,[&]{return first_call_started;});
  }

  void release() {
    std::lock_guard lock(mutex);
    release_first_call=true;
    condition.notify_all();
  }
};

int main() {
  setenv("CYCLES_PER_DAY", "4", 1);
  setenv("REPORT_EVERY_DAYS", "1", 1);

  TimingDecider decider;
  Logger logger("/tmp/autopoiesis-timing-tests");
  TimingReporter reporter;
  Simulation simulation(42, decider, logger, &reporter);

  simulation.run(1, 0, 0);
  reporter.wait_until_first_call_starts();

  // The first report remains blocked, but day two and day three still execute all ticks.
  simulation.run(2, 0, 0);
  assert(simulation.date().absolute_day == 3);
  assert(simulation.simulation_cycle() == 12);
  assert(decider.calls["a1"] == 3);
  assert(decider.calls["a2"] == 3);
  assert(decider.calls["a3"] == 3);

  reporter.release();
  simulation.wait_for_reporting_idle();
  assert(reporter.events.size() == 12);
  assert(reporter.events[0] == "report:a1:1:4");
  assert(reporter.events[1] == "request:a1:1:4");
  assert(reporter.events[2] == "report:a2:1:4");
  assert(reporter.events[3] == "request:a2:1:4");
  assert(reporter.events[4] == "report:a3:1:4");
  assert(reporter.events[5] == "request:a3:1:4");
  assert(reporter.events[6] == "report:a1:2:8");

  int validation_opens = 0;
  int validation_polls = 0;
  Logger validation_logger("/tmp/autopoiesis-timing-validation-tests");
  Simulation validation_simulation(42, decider, validation_logger);
  validation_simulation.run(3, 0, 0, [&](int day, int simulation_cycle, bool open_window) {
    ++validation_polls;
    assert(day == 1);
    assert(simulation_cycle == 4);
    if(open_window)++validation_opens;
    return ValidationWindowState::Pending;
  });
  assert(validation_opens == 1);
  assert(validation_polls == 3);
  assert(validation_simulation.date().absolute_day == 3);
  assert(validation_simulation.simulation_cycle() == 12);

  validation_simulation.run(1, 0, 0, [](int, int, bool) {
    return ValidationWindowState::Resolved;
  });
  assert(validation_simulation.date().absolute_day == 4);

  unsetenv("CYCLES_PER_DAY");
  unsetenv("REPORT_EVERY_DAYS");
}
