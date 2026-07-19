#include "autopoiesis/world.hpp"
#include <cstdlib>
#include <sstream>

namespace apo {
World::World(unsigned) : cells_(width * height, Terrain::Ground), rabbit_{15, 4} {
  for (Position p : {Position{5,2},Position{8,3},Position{13,7},Position{26,4},Position{32,15},Position{2,20}}) cells_[index(p)] = Terrain::Water;
  for (Position p : {Position{6,1},Position{12,2},Position{20,8},Position{29,14},Position{37,21}}) cells_[index(p)] = Terrain::Tree;
  for (Position p : {Position{14,2},Position{4,8},Position{17,8},Position{28,5},Position{35,18}}) cells_[index(p)] = Terrain::Bush;
  food_resources_={
    {FoodType::Berries,{14,2},8,35},{FoodType::Berries,{4,8},8,35},{FoodType::Berries,{28,5},8,35},
    {FoodType::Roots,{9,6},10,30},{FoodType::Roots,{33,9},10,30},
    {FoodType::Mushrooms,{22,10},7,28},{FoodType::Mushrooms,{3,19},7,28},
    {FoodType::Fish,{5,2},6,45},{FoodType::Fish,{32,15},6,45},
    {FoodType::Venison,{30,6},1,55}};
  for(auto& resource:food_resources_)resource.capacity=resource.amount;
  animals_={{"rabbit-1",AnimalType::Rabbit,rabbit_,true,0,35},
            {"deer-1",AnimalType::Deer,{30,6},true,10,55},
            {"boar-1",AnimalType::Boar,{24,16},true,45,50},
            {"wolf-1",AnimalType::Wolf,{35,20},true,70,40},
            {"fish-1",AnimalType::Fish,{5,2},true,0,45}};
}
Position World::wrap(Position p) const { return {((p.x%width)+width)%width,((p.y%height)+height)%height}; }
Position World::step(Position p,const std::string& direction) const { if(direction=="north")--p.y;if(direction=="south")++p.y;if(direction=="east")++p.x;if(direction=="west")--p.x;return wrap(p); }
int World::toroidal_distance(Position a,Position b) const { a=wrap(a);b=wrap(b);const int dx=std::abs(a.x-b.x),dy=std::abs(a.y-b.y);return std::min(dx,width-dx)+std::min(dy,height-dy); }
std::vector<Position> World::neighbors(Position p) const { return {step(p,"north"),step(p,"east"),step(p,"south"),step(p,"west")}; }
Terrain World::terrain(Position p) const { return cells_[index(p)]; }
bool World::passable(Position p) const { auto t=terrain(p); return t==Terrain::Ground || t==Terrain::Bush; }
bool World::drinkable(Position p) const { for(const auto neighbor:neighbors(p))if(terrain(neighbor)==Terrain::Water)return true;return terrain(p)==Terrain::Water; }
int World::food(Position p) const { p=wrap(p);int total=0;for(const auto& resource:food_resources_)if(resource.position==p)total+=resource.amount;return total; }
int World::berries(Position p) const { p=wrap(p);for(const auto& resource:food_resources_)if(resource.type==FoodType::Berries&&resource.position==p)return resource.amount;return 0; }
bool World::consume_food(Position p,FoodType* eaten,int* nutrition) { p=wrap(p);for(auto& resource:food_resources_)if(resource.position==p&&resource.amount>0&&terrain(p)!=Terrain::Water){--resource.amount;if(eaten)*eaten=resource.type;if(nutrition)*nutrition=resource.nutrition;return true;}return false; }
bool World::eat_berries(Position p) { p=wrap(p);for(auto& resource:food_resources_)if(resource.type==FoodType::Berries&&resource.position==p&&resource.amount>0){--resource.amount;return true;}return false; }
int World::wood(Position p) const { p=wrap(p);const auto found=construction_cells_.find({p.x,p.y});return found==construction_cells_.end()?0:found->second.wood; }
int World::fibers(Position p) const { p=wrap(p);const auto found=construction_cells_.find({p.x,p.y});return found==construction_cells_.end()?0:found->second.fibers; }
int World::shelter_level(Position p) const { p=wrap(p);const auto found=construction_cells_.find({p.x,p.y});return found==construction_cells_.end()?0:found->second.shelter_level; }
int World::living_trees(Position p) const { return terrain(p)==Terrain::Tree?1:0; }
bool World::harvest_tree(Position p) { p=wrap(p);if(terrain(p)!=Terrain::Tree)return false;cells_[index(p)]=Terrain::Ground;return true; }
bool World::create_shelter(Position p) { p=wrap(p);if(!passable(p)||shelter_level(p)>0)return false;construction_cells_[{p.x,p.y}].shelter_level=1;return true; }
void World::add_materials(Position p,int wood_amount,int fiber_amount) { p=wrap(p);auto& cell=construction_cells_[{p.x,p.y}];cell.wood+=std::max(0,wood_amount);cell.fibers+=std::max(0,fiber_amount); }
bool World::build_shelter(Position p) { p=wrap(p);if(!passable(p))return false;auto& cell=construction_cells_[{p.x,p.y}];if(cell.wood<3||cell.fibers<2)return false;cell.wood-=3;cell.fibers-=2;++cell.shelter_level;return true; }
void World::apply_climate(const CalendarDate& date,const ClimateState& climate) {
  for(auto& resource:food_resources_){
    const bool plant=resource.type==FoodType::Berries||resource.type==FoodType::Roots||resource.type==FoodType::Mushrooms;
    if(!plant)continue;
    if(date.season==Season::Spring&&climate.rainfall_mm>=8)resource.amount=std::min(resource.capacity,resource.amount+1);
    else if(date.season==Season::Autumn&&climate.rainfall_mm>=8&&resource.type!=FoodType::Berries)resource.amount=std::min(resource.capacity,resource.amount+1);
    else if(date.season==Season::Summer&&climate.temperature_c>=29&&date.day_of_month%5==0)resource.amount=std::max(0,resource.amount-1);
    else if(date.season==Season::Winter&&date.day_of_month%10==0)resource.amount=std::max(0,resource.amount-1);
  }
}
const Animal* World::animal(const std::string& id) const { for(const auto& candidate:animals_)if(candidate.id==id)return &candidate;return nullptr; }
bool World::hunt_animal(Position hunter,const std::string& animal_id,Animal* hunted) { for(auto& candidate:animals_)if(candidate.id==animal_id&&candidate.alive&&adjacent(hunter,candidate.position)){candidate.alive=false;if(hunted)*hunted=candidate;if(candidate.type==AnimalType::Rabbit)rabbit_alive_=false;return true;}return false; }
bool World::hunt_rabbit(Position hunter) {
  return hunt_animal(hunter,"rabbit-1");
}
void World::advance_nature(std::mt19937& rng) {
  for(auto& candidate:animals_){
    if(!candidate.alive||candidate.type==AnimalType::Fish||std::uniform_int_distribution<int>(0,3)(rng)!=0)continue;
    std::vector<Position> options;for(const auto p:neighbors(candidate.position))if(passable(p))options.push_back(p);
    if(!options.empty())candidate.position=options[std::uniform_int_distribution<size_t>(0,options.size()-1)(rng)];
    if(candidate.type==AnimalType::Rabbit)rabbit_=candidate.position;
  }
}
std::string World::ascii(const std::vector<Agent>& agents) const {
  std::ostringstream out;
  for (int y=0;y<height;++y) { for (int x=0;x<width;++x) { Position p{x,y}; char c='.'; switch(terrain(p)){case Terrain::Wall:c='#';break;case Terrain::Water:c='~';break;case Terrain::Tree:c='T';break;case Terrain::Bush:c='B';break;default:break;} for(const auto& resource:food_resources_)if(resource.position==p&&resource.amount>0){if(resource.type==FoodType::Roots)c='u';if(resource.type==FoodType::Mushrooms)c='m';} for(const auto& animal:animals_)if(animal.alive&&animal.position==p){if(animal.type==AnimalType::Rabbit)c='r';if(animal.type==AnimalType::Deer)c='d';if(animal.type==AnimalType::Boar)c='b';if(animal.type==AnimalType::Wolf)c='w';if(animal.type==AnimalType::Fish)c='f';} for (const auto& a:agents) if (a.alive && a.position==p) c='A'; out<<c; } out<<'\n'; }
  return out.str();
}
}
