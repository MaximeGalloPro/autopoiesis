#include "autopoiesis/world.hpp"
#include <sstream>

namespace apo {
World::World(unsigned) : cells_(width * height, Terrain::Ground), rabbit_{15, 4} {
  for (int x=0; x<width; ++x) { cells_[index({x,0})]=Terrain::Wall; cells_[index({x,height-1})]=Terrain::Wall; }
  for (int y=0; y<height; ++y) { cells_[index({0,y})]=Terrain::Wall; cells_[index({width-1,y})]=Terrain::Wall; }
  for (Position p : {Position{5,2},Position{8,3},Position{13,7}}) cells_[index(p)] = Terrain::Water;
  for (Position p : {Position{6,1},Position{12,2}}) cells_[index(p)] = Terrain::Tree;
  for (Position p : {Position{14,2},Position{4,8},Position{17,8}}) { cells_[index(p)] = Terrain::Bush; berries_[index(p)] = 8; }
}
Terrain World::terrain(Position p) const { return in_bounds(p) ? cells_[index(p)] : Terrain::Wall; }
bool World::passable(Position p) const { auto t=terrain(p); return t==Terrain::Ground || t==Terrain::Bush; }
int World::berries(Position p) const { auto it=berries_.find(index(p)); return it == berries_.end() ? 0 : it->second; }
bool World::eat_berries(Position p) { auto it=berries_.find(index(p)); if (it==berries_.end() || it->second<=0) return false; --it->second; return true; }
void World::advance_nature(std::mt19937& rng) {
  if (std::uniform_int_distribution<int>(0,3)(rng) != 0) return;
  std::vector<Position> options; for (Position p : std::vector<Position>{{rabbit_.x+1,rabbit_.y},{rabbit_.x-1,rabbit_.y},{rabbit_.x,rabbit_.y+1},{rabbit_.x,rabbit_.y-1}}) if (passable(p)) options.push_back(p);
  if (!options.empty()) rabbit_ = options[std::uniform_int_distribution<size_t>(0, options.size()-1)(rng)];
}
std::string World::ascii(const std::vector<Agent>& agents) const {
  std::ostringstream out;
  for (int y=0;y<height;++y) { for (int x=0;x<width;++x) { Position p{x,y}; char c='.'; switch(terrain(p)){case Terrain::Wall:c='#';break;case Terrain::Water:c='~';break;case Terrain::Tree:c='T';break;case Terrain::Bush:c='B';break;default:break;} if (p==rabbit_) c='r'; for (const auto& a:agents) if (a.alive && a.position==p) c='A'; out<<c; } out<<'\n'; }
  return out.str();
}
}
