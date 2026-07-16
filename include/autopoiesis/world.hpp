#pragma once
#include "types.hpp"
#include <random>

namespace apo {
class World {
 public:
  static constexpr int width = 20, height = 12;
  explicit World(unsigned seed = 42);
  Terrain terrain(Position p) const;
  bool in_bounds(Position p) const { return p.x >= 0 && p.x < width && p.y >= 0 && p.y < height; }
  bool passable(Position p) const;
  Position rabbit() const { return rabbit_; }
  bool rabbit_alive() const { return rabbit_alive_; }
  bool hunt_rabbit(Position hunter);
  int berries(Position p) const;
  bool eat_berries(Position p);
  void advance_nature(std::mt19937& rng);
  std::string ascii(const std::vector<Agent>& agents) const;
 private:
  std::vector<Terrain> cells_; Position rabbit_; bool rabbit_alive_{true};
  std::map<int,int> berries_;
  int index(Position p) const { return p.y * width + p.x; }
};
}
