#include "autopoiesis/simulation.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <set>

using namespace apo;

struct WaitDecider final : IDecider {
  Decision decide(const Perception&) override { return {}; }
};

struct BlockedDecider final : IDecider {
  Decision decide(const Perception&) override {
    return {DecisionType::Blocked, "wait", json::object(), "", "abri",
            "aucune capacité de construction", "construire un abri"};
  }
};

namespace apo {
struct SimulationTestAccess {
  static void make_adjacent(Simulation& simulation) {
    simulation.agents_[1].position = simulation.world_.step(simulation.agents_[0].position, "east");
  }
  static std::string execute(Simulation& simulation, Agent& agent, const Decision& decision) {
    return simulation.execute(agent, decision);
  }
  static void update_behavior(Simulation& simulation, Agent& agent, const Agent& before,
                              const Decision& decision, const std::string& result,
                              bool succeeded) {
    simulation.update_behavior_after_action(agent, before, decision, result, succeeded);
  }
};
}

int main() {
  WaitDecider decider;
  Logger logger("/tmp/autopoiesis-behavior-tests");
  Simulation simulation(42, decider, logger);
  const auto& agents = simulation.agents();

  assert(agents.size() == 3);
  std::set<std::string> archetypes;
  std::set<std::string> projects;
  for (const auto& agent : agents) {
    archetypes.insert(agent.behavior.archetype);
    projects.insert(agent.project.key);
    assert(agent.project.status == ProjectStatus::Active);
    assert(!agent.behavior.aspiration.empty());
    assert(!agent.behavior.preferred_foods.empty());
    assert(agent.boredom == 0);
  }
  assert(archetypes == std::set<std::string>({"builder", "explorer", "provider"}));
  assert(projects == std::set<std::string>({"build_shelter", "map_region", "secure_food"}));

  Agent social = agents.front();
  social.relationships["a2"] = {12, 7, 3};
  const auto relationship = relationships_json(social.relationships);
  assert(relationship["a2"]["trust"] == 12);
  assert(relationship["a2"]["affinity"] == 7);
  assert(relationship["a2"]["interactions"] == 3);

  const auto project = project_json(social.project);
  assert(project["key"] == "build_shelter");
  assert(project["status"] == "active");

  Agent& ada = const_cast<Agent&>(simulation.agents().front());
  const Decision observe{DecisionType::Action, "observe", json::object(),
                         "Je réévalue mon environnement."};
  assert(SimulationTestAccess::execute(simulation, ada, observe) ==
         "observe son environnement");
  ada.remember_map({5, 5}, Terrain::Tree);
  ada.remember_map({6, 5}, Terrain::Tree);
  const Agent before_project = ada;
  SimulationTestAccess::update_behavior(simulation, ada, before_project, {}, "attend", true);
  assert(ada.project.status == ProjectStatus::Blocked);
  assert(ada.project.missing_capability == "build_shelter");

  SimulationTestAccess::make_adjacent(simulation);
  Decision talk{DecisionType::Action, "talk",
                {{"target_agent_id", "a2"}, {"message", "Partageons nos observations."}},
                "Je consolide notre coopération."};
  const auto result = SimulationTestAccess::execute(simulation, ada, talk);
  assert(result == "parle a Borin");
  assert(ada.relationships.at("a2").interactions == 1);
  assert(simulation.agents()[1].relationships.at("a1").trust > 0);

  setenv("CYCLES_PER_DAY", "240", 1);
  std::mt19937 rng(42);
  LocalDecider local(rng);
  Simulation replay(42, local, logger);
  replay.run(3, 0, 0);
  int blocked_projects = 0;
  for (const auto& agent : replay.agents()) {
    if (agent.project.status == ProjectStatus::Blocked) {
      ++blocked_projects;
      assert(!agent.project.missing_capability.empty());
    }
    assert(agent.boredom >= 0 && agent.boredom <= 100);
  }
  assert(blocked_projects >= 2);

  std::mt19937 second_rng(42);
  LocalDecider second_local(second_rng);
  Logger second_logger("/tmp/autopoiesis-behavior-replay-tests");
  Simulation second_replay(42, second_local, second_logger);
  second_replay.run(3, 0, 0);
  for (std::size_t index = 0; index < replay.agents().size(); ++index) {
    const auto& first = replay.agents()[index];
    const auto& second = second_replay.agents()[index];
    assert(first.position == second.position);
    assert(first.health == second.health);
    assert(first.hunger == second.hunger);
    assert(first.thirst == second.thirst);
    assert(first.fatigue == second.fatigue);
    assert(first.boredom == second.boredom);
    assert(project_json(first.project) == project_json(second.project));
    assert(relationships_json(first.relationships) == relationships_json(second.relationships));
  }

  const std::filesystem::path blocked_directory = "/tmp/autopoiesis-local-blocked-tests";
  std::filesystem::remove_all(blocked_directory);
  BlockedDecider blocked;
  Logger blocked_logger(blocked_directory.string());
  setenv("CYCLES_PER_DAY", "1", 1);
  Simulation blocked_simulation(42, blocked, blocked_logger);
  blocked_simulation.run_day();
  assert(!std::filesystem::exists(blocked_directory / "feature_requests.jsonl"));
  unsetenv("CYCLES_PER_DAY");
}
