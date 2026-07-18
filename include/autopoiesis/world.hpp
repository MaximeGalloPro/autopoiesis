#pragma once
#include "types.hpp"
#include <random>

namespace apo {
class World {
 public:
  static constexpr int width = 40, height = 24;
  explicit World(unsigned seed = 42);
  Position wrap(Position p) const;
  Position step(Position p, const std::string& direction) const;
  int toroidal_distance(Position a, Position b) const;
  bool adjacent(Position a, Position b) const { return toroidal_distance(a,b)==1; }
  std::vector<Position> neighbors(Position p) const;
  Terrain terrain(Position p) const;
  bool in_bounds(Position p) const { return p.x >= 0 && p.x < width && p.y >= 0 && p.y < height; }
  bool passable(Position p) const;
  bool drinkable(Position p) const;
  int food(Position p) const;
  const std::vector<FoodResource>& food_resources() const { return food_resources_; }
  const std::vector<Animal>& animals() const { return animals_; }
  const Animal* animal(const std::string& id) const;
  bool consume_food(Position p, FoodType* eaten = nullptr, int* nutrition = nullptr);
  bool hunt_animal(Position hunter, const std::string& animal_id, Animal* hunted = nullptr);
  Position rabbit() const { return rabbit_; }
  bool rabbit_alive() const { return rabbit_alive_; }
  bool hunt_rabbit(Position hunter);
  int berries(Position p) const;
  bool eat_berries(Position p);
  void advance_nature(std::mt19937& rng);
  std::string ascii(const std::vector<Agent>& agents) const;
 private:
  std::vector<Terrain> cells_; Position rabbit_; bool rabbit_alive_{true};
  std::vector<FoodResource> food_resources_;
  std::vector<Animal> animals_;
  int index(Position p) const { const auto canonical=wrap(p); return canonical.y * width + canonical.x; }
};
}
