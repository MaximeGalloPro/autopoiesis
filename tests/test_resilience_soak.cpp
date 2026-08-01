#include "autopoiesis/simulation.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <string>
#include <unistd.h>

using namespace apo;

namespace apo {
struct SimulationTestAccess {
  static World& world(Simulation& simulation) { return simulation.world_; }
  static Agent& agent(Simulation& simulation, std::size_t index) {
    return simulation.agents_.at(index);
  }
  static void set_day(Simulation& simulation, int day) {
    simulation.day_ = day;
    simulation.date_ = date_from_absolute_day(day);
    simulation.climate_ = climate_for(simulation.date_);
  }
  static void update_population(Simulation& simulation) { simulation.update_population(); }
};
}  // namespace apo

namespace {
struct WaitingDecider final : IDecider {
  Decision decide(const Perception&) override {
    return {DecisionType::Action, "wait", json::object(), "préserver un scénario déterministe"};
  }
};

struct CountingReporter final : ICycleReporter {
  int period_reports{};
  int evolution_requests{};

  json report_period(int, int, const Agent&, const std::vector<std::string>&) override {
    ++period_reports;
    return json::object();
  }

  json request_evolution(int, int, const Agent&, const std::vector<std::string>&,
                         const json&) override {
    ++evolution_requests;
    return nullptr;
  }
};

struct HoldingReporter final : ICycleReporter {
  std::atomic<int> period_reports{};
  std::atomic<int> evolution_requests{};
  std::mutex mutex;
  std::condition_variable condition;
  bool started{};
  bool released{};

  json report_period(int, int, const Agent& agent, const std::vector<std::string>&) override {
    ++period_reports;
    {
      std::unique_lock lock(mutex);
      started = true;
      condition.notify_all();
      condition.wait(lock, [&] { return released; });
    }
    return {{"character_voice", agent.name}, {"day_summary", "Bilan lent."},
            {"state_assessment", "Stable."}, {"ask_god", true}};
  }

  json request_evolution(int, int, const Agent&, const std::vector<std::string>&,
                         const json&) override {
    ++evolution_requests;
    return nullptr;
  }

  void wait_until_started() {
    std::unique_lock lock(mutex);
    condition.wait(lock, [&] { return started; });
  }

  void release() {
    std::lock_guard lock(mutex);
    released = true;
    condition.notify_all();
  }
};

void stock_food(World& world, Position camp, int amount) {
  for (int index = 0; index < amount; ++index)
    assert(world.store_food(camp, FoodItem{FoodType::Roots, 25, false, 0, 5}));
}

void assert_ecology_is_valid(const World& world) {
  for (const auto& resource : world.food_resources()) {
    assert(world.in_bounds(resource.position));
    assert(resource.amount >= 0);
    assert(resource.amount <= resource.capacity);
    assert(resource.depleted_days >= 0);
  }
  for (const auto type : {AnimalType::Rabbit, AnimalType::Deer, AnimalType::Boar,
                          AnimalType::Wolf, AnimalType::Fish}) {
    assert(world.population(type) >= 0);
    assert(world.population(type) <= world.carrying_capacity(type));
  }
  for (const auto& animal : world.animals()) assert(world.in_bounds(animal.position));
}

const Agent& child_from(const Simulation& simulation) {
  const auto found = std::find_if(simulation.agents().begin(), simulation.agents().end(),
                                  [](const Agent& agent) { return agent.origin == "birth"; });
  assert(found != simulation.agents().end());
  return *found;
}
}  // namespace

int main() {
  setenv("CYCLES_PER_DAY", "1", 1);
  setenv("REPORT_EVERY_DAYS", "1", 1);

  const auto root = std::filesystem::path("/tmp") /
      ("autopoiesis-resilience-soak-" + std::to_string(getpid()));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto checkpoint = root / "simulation-state.json";

  const auto& registry = FeatureRegistry::defaults();
  auto profile = registry.default_profile();
  assert(registry.activate_for_startup(profile, "camp_cooking", 1));

  WaitingDecider decider;
  Logger logger((root / "source").string());
  Simulation source(42, decider, logger, nullptr, checkpoint.string(), profile);
  auto& world = SimulationTestAccess::world(source);
  const Position camp{13, 2};
  assert(world.place_campfire(camp));
  assert(world.create_shelter({14, 2}));
  stock_food(world, camp, 40);
  for (std::size_t index = 0; index < source.agents().size(); ++index) {
    auto& agent = SimulationTestAccess::agent(source, index);
    agent.position = {14, 2};
    agent.age_days = 25;
  }

  // Produce an arrival and a dependent child before checkpointing all authoritative state.
  SimulationTestAccess::set_day(source, 60);
  SimulationTestAccess::update_population(source);
  assert(source.agents().size() == 4);
  SimulationTestAccess::set_day(source, 90);
  SimulationTestAccess::update_population(source);
  assert(source.agents().size() == 5);
  const auto& child_before_checkpoint = child_from(source);
  assert(child_before_checkpoint.parent_ids.size() == 2);
  assert(child_before_checkpoint.generation == 1);
  assert(is_dependent_child(child_before_checkpoint));
  const auto child_actions = available_actions(child_before_checkpoint, world, source.agents(), 90,
                                               DayPhase::Day);
  assert(std::ranges::find(child_actions, "hunt_animal") == child_actions.end());
  assert(std::ranges::find(child_actions, "confront") == child_actions.end());
  std::set<std::string> names;
  for (const auto& agent : source.agents()) assert(names.insert(agent.name).second);

  // A pending validation is the engine-side midpoint of the card cycle; no reporter is needed
  // for that deterministic guard to be checkpointed alongside camp, population and capabilities.
  source.run(1, 0, 0, [](int day, int cycle, bool opening) {
    assert(day == 91);
    assert(cycle == 1);
    assert(opening);
    return ValidationWindowState::Pending;
  });
  assert(std::filesystem::exists(checkpoint));
  {
    std::ifstream input(checkpoint);
    json persisted;
    input >> persisted;
    assert(persisted.at("validation_pending") == true);
    assert(persisted.at("world").at("primary_campfire").at("x") == camp.x);
    assert(persisted.at("group").at("world_profile").at("active_features").size() >= 2);
  }

  CountingReporter after_extinction_reporter;
  Logger restored_logger((root / "restored").string());
  Simulation restored(999, decider, restored_logger, &after_extinction_reporter,
                      checkpoint.string());
  assert(restored.restored_checkpoint());
  assert(restored.feature_active("core_survival", 1));
  assert(restored.feature_active("camp_cooking", 1));
  assert(restored.world().campfire(camp));
  assert(restored.world().shelter_level({14, 2}) > 0);
  const auto& restored_child = child_from(restored);
  assert(restored_child.parent_ids == child_before_checkpoint.parent_ids);
  assert(restored_child.generation == child_before_checkpoint.generation);
  assert(restored_child.name == child_before_checkpoint.name);

  for (std::size_t index = 0; index < restored.agents().size(); ++index)
    SimulationTestAccess::agent(restored, index).alive = false;
  SimulationTestAccess::set_day(restored, 100);
  SimulationTestAccess::update_population(restored);
  assert(restored.civilization().status == CivilizationStatus::Extinct);
  const int cycle_at_extinction = restored.simulation_cycle();
  const int day_at_extinction = restored.date().absolute_day;
  const int ecology_at_extinction = restored.world().ecology().day;

  // More than 100 daily transitions remain deterministic and locally valid after extinction.
  for (int day = 1; day <= 101; ++day) {
    restored.run(1, 0, 0);
    assert(restored.simulation_cycle() == cycle_at_extinction + day);
    assert(restored.date().absolute_day == day_at_extinction + day);
    assert(restored.civilization().status == CivilizationStatus::Extinct);
    assert(restored.world().ecology().day == ecology_at_extinction + day);
    assert_ecology_is_valid(restored.world());
  }
  assert(after_extinction_reporter.period_reports == 0);
  assert(after_extinction_reporter.evolution_requests == 0);

  // A deliberately slow reporter cannot stall ticks and must not turn into an evolution request
  // once the population disappears while its period report is still in flight.
  HoldingReporter slow_reporter;
  Logger slow_logger((root / "slow").string());
  Simulation slow(77, decider, slow_logger, &slow_reporter);
  slow.run(1, 0, 0);
  slow_reporter.wait_until_started();
  for (std::size_t index = 0; index < slow.agents().size(); ++index)
    SimulationTestAccess::agent(slow, index).alive = false;
  slow.run(1, 0, 0);
  const bool ticks_continued = slow.date().absolute_day == 2 && slow.simulation_cycle() == 2 &&
                               slow.civilization().status == CivilizationStatus::Extinct;
  slow_reporter.release();
  slow.wait_for_reporting_idle();
  assert(ticks_continued);
  assert(slow_reporter.period_reports == 1);
  assert(slow_reporter.evolution_requests == 0);

  unsetenv("CYCLES_PER_DAY");
  unsetenv("REPORT_EVERY_DAYS");
  std::filesystem::remove_all(root);
}
