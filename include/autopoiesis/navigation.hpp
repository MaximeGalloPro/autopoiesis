#pragma once

#include "types.hpp"

#include <map>
#include <vector>

namespace apo {

// A navigation input is descriptive: neither its construction nor a planned
// trajectory is allowed to alter the authoritative world.
enum class NavigationCell { Unknown, Traversable, Blocked };

enum class NavigationStatus {
  PathFound,
  AlreadyAtGoal,
  StartOutOfBounds,
  GoalOutOfBounds,
  StartNotTraversable,
  GoalNotTraversable,
  Unreachable,
};

struct NavigationGrid {
  NavigationGrid() = default;
  NavigationGrid(int grid_width, int grid_height) : width(grid_width), height(grid_height) {}

  int width{};
  int height{};

  bool in_bounds(Position position) const;
  NavigationCell state_at(Position position) const;
  void set_cell(Position position, NavigationCell state);

 private:
  std::map<std::pair<int, int>, NavigationCell> cells_;
};

// On success, waypoints include both start and goal. Consecutive waypoints
// always differ by exactly one cardinal, in-bounds grid cell.
struct NavigationTrajectory {
  NavigationStatus status{NavigationStatus::Unreachable};
  std::vector<Position> waypoints;

  bool found() const {
    return status == NavigationStatus::PathFound || status == NavigationStatus::AlreadyAtGoal;
  }
};

class INavigation {
 public:
  virtual ~INavigation() = default;
  virtual NavigationTrajectory plan(const NavigationGrid& grid, Position start,
                                    Position goal) const = 0;
};

// Reference adapter for a bounded top-down world. It deliberately knows
// nothing about World or Agent; Detour can implement INavigation later without
// gaining authority over engine state.
class BoundedGridNavigationAdapter final : public INavigation {
 public:
  NavigationTrajectory plan(const NavigationGrid& grid, Position start,
                            Position goal) const override;
};

}  // namespace apo
