#include "autopoiesis/navigation.hpp"

#include <algorithm>
#include <array>
#include <queue>
#include <stdexcept>

namespace apo {
namespace {

using Coordinate = std::pair<int, int>;

Coordinate coordinate(Position position) { return {position.x, position.y}; }

constexpr std::array<Position, 4> cardinal_offsets{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};

Position add(Position position, Position offset) {
  return {position.x + offset.x, position.y + offset.y};
}

}  // namespace

bool NavigationGrid::in_bounds(Position position) const {
  return position.x >= 0 && position.x < width && position.y >= 0 && position.y < height;
}

NavigationCell NavigationGrid::state_at(Position position) const {
  if (!in_bounds(position)) return NavigationCell::Blocked;
  const auto found = cells_.find(coordinate(position));
  return found == cells_.end() ? NavigationCell::Unknown : found->second;
}

void NavigationGrid::set_cell(Position position, NavigationCell state) {
  if (!in_bounds(position)) throw std::out_of_range("navigation cell outside bounded grid");
  cells_[coordinate(position)] = state;
}

NavigationTrajectory BoundedGridNavigationAdapter::plan(const NavigationGrid& grid, Position start,
                                                         Position goal) const {
  if (!grid.in_bounds(start)) return {NavigationStatus::StartOutOfBounds, {}};
  if (!grid.in_bounds(goal)) return {NavigationStatus::GoalOutOfBounds, {}};
  if (grid.state_at(start) != NavigationCell::Traversable)
    return {NavigationStatus::StartNotTraversable, {}};
  if (grid.state_at(goal) != NavigationCell::Traversable)
    return {NavigationStatus::GoalNotTraversable, {}};
  if (start == goal) return {NavigationStatus::AlreadyAtGoal, {start}};

  std::queue<Position> frontier;
  std::map<Coordinate, Position> predecessor;
  frontier.push(start);
  predecessor.emplace(coordinate(start), start);

  while (!frontier.empty()) {
    const Position current = frontier.front();
    frontier.pop();
    if (current == goal) break;

    for (const Position offset : cardinal_offsets) {
      const Position candidate = add(current, offset);
      if (!grid.in_bounds(candidate) || grid.state_at(candidate) != NavigationCell::Traversable ||
          predecessor.contains(coordinate(candidate)))
        continue;
      predecessor.emplace(coordinate(candidate), current);
      frontier.push(candidate);
    }
  }

  if (!predecessor.contains(coordinate(goal))) return {NavigationStatus::Unreachable, {}};

  std::vector<Position> waypoints;
  for (Position current = goal;; current = predecessor.at(coordinate(current))) {
    waypoints.push_back(current);
    if (current == start) break;
  }
  std::reverse(waypoints.begin(), waypoints.end());
  return {NavigationStatus::PathFound, std::move(waypoints)};
}

}  // namespace apo
