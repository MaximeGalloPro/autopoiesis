#include "autopoiesis/navigation.hpp"

#include <cassert>
#include <iostream>

using namespace apo;

namespace {
NavigationGrid traversable_grid(int width, int height) {
  NavigationGrid grid{width, height};
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) grid.set_cell({x, y}, NavigationCell::Traversable);
  return grid;
}
}

int main() {
  const BoundedGridNavigationAdapter navigation;

  auto open = traversable_grid(5, 1);
  const auto border_route = navigation.plan(open, {0, 0}, {4, 0});
  assert(border_route.status == NavigationStatus::PathFound);
  assert((border_route.waypoints == std::vector<Position>{{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}}));

  auto detour = traversable_grid(5, 3);
  detour.set_cell({1, 1}, NavigationCell::Blocked);
  const auto canonical_route = navigation.plan(detour, {0, 1}, {4, 1});
  assert(canonical_route.status == NavigationStatus::PathFound);
  assert((canonical_route.waypoints ==
          std::vector<Position>{{0, 1}, {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {4, 1}}));

  auto partial_knowledge = traversable_grid(3, 1);
  partial_knowledge.set_cell({1, 0}, NavigationCell::Unknown);
  const auto hidden_route = navigation.plan(partial_knowledge, {0, 0}, {2, 0});
  assert(hidden_route.status == NavigationStatus::Unreachable);
  assert(hidden_route.waypoints.empty());

  auto sealed = traversable_grid(3, 1);
  sealed.set_cell({1, 0}, NavigationCell::Blocked);
  const auto blocked_route = navigation.plan(sealed, {0, 0}, {2, 0});
  assert(blocked_route.status == NavigationStatus::Unreachable);

  const auto already_there = navigation.plan(open, {2, 0}, {2, 0});
  assert(already_there.status == NavigationStatus::AlreadyAtGoal);
  assert((already_there.waypoints == std::vector<Position>{{2, 0}}));

  open.set_cell({0, 0}, NavigationCell::Blocked);
  const auto blocked_start = navigation.plan(open, {0, 0}, {4, 0});
  assert(blocked_start.status == NavigationStatus::StartNotTraversable);

  const auto out_of_bounds_goal = navigation.plan(open, {1, 0}, {5, 0});
  assert(out_of_bounds_goal.status == NavigationStatus::GoalOutOfBounds);

  std::cout << "bounded navigation contract tests passed\n";
}
