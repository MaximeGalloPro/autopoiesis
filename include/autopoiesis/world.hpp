#pragma once
#include "calendar.hpp"
#include "capability_registry.hpp"
#include "types.hpp"
#include <random>

namespace apo {
class World {
 public:
  static constexpr int width = 40, height = 24;
  explicit World(unsigned seed = 42);
  std::optional<Position> step(Position p, const std::string& direction) const;
  int distance(Position a, Position b) const;
  bool adjacent(Position a, Position b) const { return distance(a,b)==1; }
  std::vector<Position> neighbors(Position p) const;
  Terrain terrain(Position p) const;
  bool in_bounds(Position p) const { return p.x >= 0 && p.x < width && p.y >= 0 && p.y < height; }
  bool passable(Position p) const;
  bool drinkable(Position p) const;
  int food(Position p) const;
  const std::vector<FoodResource>& food_resources() const { return food_resources_; }
  const std::vector<Animal>& animals() const { return animals_; }
  int population(AnimalType type) const;
  int carrying_capacity(AnimalType type) const;
  EcologyState ecology() const;
  const Animal* animal(const std::string& id) const;
  bool consume_food(Position p, FoodType* eaten = nullptr, int* nutrition = nullptr);
  bool hunt_animal(Position hunter, const std::string& animal_id, Animal* hunted = nullptr);
  Position rabbit() const { return rabbit_; }
  bool rabbit_alive() const { return rabbit_alive_; }
  bool hunt_rabbit(Position hunter);
  int berries(Position p) const;
  bool eat_berries(Position p);
  int wood(Position p) const;
  int fibers(Position p) const;
  int shelter_level(Position p) const;
  int living_trees(Position p) const;
  bool harvest_tree(Position p);
  int branches(Position p) const;
  bool take_branch(Position p);
  int iron_ore(Position p) const;
  bool take_iron_ore(Position p);
  bool campfire(Position p) const;
  bool adjacent_campfire(Position p) const;
  std::optional<Position> nearby_campfire(Position p) const;
  std::optional<Position> primary_campfire() const { return primary_campfire_; }
  bool place_campfire(Position p);
  static constexpr int initial_camp_chest_capacity = 48;
  static constexpr int upgraded_camp_chest_capacity = 96;
  static constexpr int camp_chest_upgrade_wood_cost = 2;
  static constexpr int camp_chest_upgrade_branch_cost = 2;
  std::optional<Position> camp_chest_position(Position campfire_position) const;
  int camp_chest_level(Position campfire_position) const;
  int camp_chest_capacity(Position campfire_position) const;
  int camp_chest_occupation(Position campfire_position) const;
  bool can_upgrade_camp_chest(Position campfire_position) const;
  bool upgrade_camp_chest(Position campfire_position);
  int stored_food(Position campfire_position) const;
  bool can_store_food(Position campfire_position, const FoodItem& food) const;
  bool store_food(Position campfire_position, const FoodItem& food);
  bool take_stored_food(Position campfire_position, FoodItem* food = nullptr,
                        const std::vector<FoodType>& preferences = {});
  int raw_stored_food(Position campfire_position) const;
  int cooked_stored_food(Position campfire_position) const;
  bool cook_stored_food(Position campfire_position);
  int age_stored_food();
  int stored_wood(Position campfire_position) const;
  int stored_branches(Position campfire_position) const;
  bool can_store_materials(Position campfire_position, int wood, int branches,
                           int iron_ore = 0) const;
  bool store_materials(Position campfire_position, int wood, int branches, int iron_ore = 0);
  int stored_iron_ore(Position campfire_position) const;
  bool consume_stored_wood(Position campfire_position, int amount);
  int stored_item(Position campfire_position, CraftItem item) const;
  int stored_item(Position campfire_position, const std::string& item) const;
  int stored_crafted_items(Position campfire_position) const;
  std::vector<std::string> craftable_recipes(Position campfire_position) const;
  bool craft(Position campfire_position, const std::string& recipe_key);
  bool take_stored_item(Position campfire_position, CraftItem item);
  bool take_stored_item(Position campfire_position, const std::string& item);
  bool can_designate_building(Position site, Position campfire_position, BuildingType type) const;
  bool designate_building(Position site, Position campfire_position, BuildingType type);
  std::optional<Building> building(Position site) const;
  std::vector<std::pair<Position,Building>> buildings() const;
  bool work_on_building(Position site, int amount = 1);
  bool has_completed_building(BuildingType type) const;
  int rest_bonus(Position p) const;
  int workshop_bonus(Position p) const;
  bool create_shelter(Position p);
  void add_materials(Position p, int wood, int fibers);
  bool build_shelter(Position p);
  void apply_climate(const CalendarDate& date, const ClimateState& climate);
  void advance_nature(std::mt19937& rng);
  std::string ascii(const std::vector<Agent>& agents) const;
  json checkpoint() const;
  void restore_checkpoint(const json& state);
 private:
  std::vector<Terrain> cells_; Position rabbit_; bool rabbit_alive_{true};
  std::vector<FoodResource> food_resources_;
  std::vector<Animal> animals_;
  std::map<std::pair<int,int>,int> iron_ore_resources_;
  struct ConstructionCell {
    int wood{};
    int fibers{};
    int shelter_level{};
    int loose_branches{};
    bool campfire{};
    int wood_stockpile{};
    int branch_stockpile{};
    int iron_ore_stockpile{};
    std::map<std::string,int> crafted_stockpile;
    std::vector<FoodItem> food_stockpile;
    std::optional<Position> camp_chest_position;
    int camp_chest_level{};
  };
  std::map<std::pair<int,int>, ConstructionCell> construction_cells_;
  std::map<std::pair<int,int>, Building> buildings_;
  std::optional<Position> primary_campfire_;
  EcologyState ecology_;
  int next_animal_id_{2};
  void replenish_branches();
  std::optional<Position> available_camp_chest_position(Position campfire_position) const;
  bool camp_chest_has_space(Position campfire_position, int amount) const;
  int index(Position p) const { return p.y * width + p.x; }
};
}
