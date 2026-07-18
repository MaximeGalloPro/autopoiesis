#pragma once

#include "world.hpp"
#include <optional>
#include <random>
#include <set>

namespace apo {
class Devil {
 public:
  explicit Devil(unsigned seed, int one_in = 10);
  std::optional<json> draw(int day, int simulation_cycle, const World& world,
                           const std::vector<Agent>& agents,
                           const std::set<std::string>& known_evolution_keys);

 private:
  std::mt19937 rng_;
  int one_in_;
};
}
