#include "autopoiesis/devil.hpp"
#include "autopoiesis/feature_request.hpp"
#include "autopoiesis/logger.hpp"
#include "autopoiesis/simulation.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <set>

using namespace apo;

struct WaitDecider final : IDecider {
  Decision decide(const Perception&) override { return {}; }
};

int main() {
  World world(42);
  std::vector<Agent> agents{{"a1", "Ada", {3, 2}},
                            {"a2", "Borin", {10, 5}},
                            {"a3", "Cyra", {16, 7}}};

  Devil forced(42, 1);
  std::set<std::string> known_keys;
  int previous_difficulty=0;
  for (int window = 1; window <= 4; ++window) {
    const auto proposal = forced.draw(window * 90, window * 720, world, agents,
                                      known_keys);
    assert(proposal.has_value());
    std::string error;
    assert(validate_feature_request(*proposal, error));
    assert((*proposal)["requested"] == true);
    assert((*proposal)["real_world_basis"].is_string());
    assert(!(*proposal)["current_mitigations"].empty());
    assert((*proposal)["future_pressure"].is_string());
    assert((*proposal)["adaptation"]["stability_score"].is_number_integer());
    assert((*proposal)["adaptation"]["pressure_level"].is_number_integer());
    assert((*proposal)["adaptation"]["rationale"].is_string());
    assert((*proposal)["difficulty"].get<int>()>=previous_difficulty);
    previous_difficulty=(*proposal)["difficulty"].get<int>();
    assert(known_keys.insert((*proposal)["evolution_key"].get<std::string>()).second);
  }

  Devil first(1234, 10);
  Devil second(1234, 10);
  int appearances = 0;
  for (int window = 1; window <= 100; ++window) {
    const auto left = first.draw(window * 3, window * 720, world, agents, {});
    const auto right = second.draw(window * 3, window * 720, world, agents, {});
    assert(left.has_value() == right.has_value());
    if (left) {
      ++appearances;
      assert((*left)["evolution_key"] == (*right)["evolution_key"]);
    }
  }
  assert(appearances >= 4 && appearances <= 20);

  World stable_world(42);
  const Position camp{13,2};assert(stable_world.place_campfire(camp));assert(stable_world.create_shelter({14,2}));
  for(int index=0;index<20;++index)assert(stable_world.store_food(camp,FoodItem{FoodType::Roots,25,false,0,5}));
  for(auto& agent:agents){agent.health=100;agent.hunger=10;agent.thirst=10;agent.equipped_tool=Tool{};}
  Devil fragile_devil(9,1),stable_devil(9,1);
  std::vector<Agent> fragile=agents;for(auto& agent:fragile){agent.health=35;agent.hunger=85;agent.thirst=85;agent.equipped_tool.reset();}
  const auto fragile_pressure=fragile_devil.draw(3,720,world,fragile,{});
  const auto stable_pressure=stable_devil.draw(360,86400,stable_world,agents,{"past-1","past-2","past-3"});
  assert(fragile_pressure&&stable_pressure);
  assert((*stable_pressure)["adaptation"]["stability_score"].get<int>()>
         (*fragile_pressure)["adaptation"]["stability_score"].get<int>());
  assert((*stable_pressure)["difficulty"].get<int>()>(*fragile_pressure)["difficulty"].get<int>());

  const std::filesystem::path directory = "/tmp/autopoiesis-devil-tests";
  std::filesystem::remove_all(directory);
  Logger logger(directory.string());
  Devil writer(7, 1);
  const auto proposal = writer.draw(3, 720, world, agents, {});
  assert(proposal);
  const auto id = logger.devil_constraint(720, 3, *proposal);
  assert(!id.empty());
  std::ifstream input(directory / "feature_requests.jsonl");
  json stored;
  input >> stored;
  assert(stored["source"] == "devil");
  assert(stored["status"] == "pending");
  assert(stored["agent_name"] == "Le Diable");
  assert(stored["id"] == id);

  const std::filesystem::path simulation_directory = "/tmp/autopoiesis-devil-simulation-tests";
  std::filesystem::remove_all(simulation_directory);
  setenv("CYCLES_PER_DAY", "1", 1);
  setenv("REPORT_EVERY_DAYS", "3", 1);
  setenv("DEVIL_CHANCE_ONE_IN", "1", 1);
  WaitDecider wait;
  Logger simulation_logger(simulation_directory.string());
  Simulation simulation(42, wait, simulation_logger);
  simulation.run(3, 0, 0);
  std::ifstream requests(simulation_directory / "feature_requests.jsonl");
  json generated;
  requests >> generated;
  assert(generated["source"] == "devil");
  assert(generated["day"] == 3);
  assert(generated["simulation_cycle"] == 3);
  unsetenv("CYCLES_PER_DAY");
  unsetenv("REPORT_EVERY_DAYS");
  unsetenv("DEVIL_CHANCE_ONE_IN");
}
