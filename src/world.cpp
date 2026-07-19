#include "autopoiesis/world.hpp"
#include <cstdlib>
#include <stdexcept>
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
  replenish_branches();
  replenish_branches();
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
int World::branches(Position p) const { p=wrap(p);const auto found=construction_cells_.find({p.x,p.y});return found==construction_cells_.end()?0:found->second.loose_branches; }
bool World::take_branch(Position p) { p=wrap(p);auto found=construction_cells_.find({p.x,p.y});if(found==construction_cells_.end()||found->second.loose_branches<=0)return false;--found->second.loose_branches;return true; }
bool World::campfire(Position p) const { p=wrap(p);const auto found=construction_cells_.find({p.x,p.y});return found!=construction_cells_.end()&&found->second.campfire; }
bool World::adjacent_campfire(Position p) const { for(const auto neighbor:neighbors(p))if(campfire(neighbor))return true;return false; }
std::optional<Position> World::nearby_campfire(Position p) const { for(const auto neighbor:neighbors(p))if(campfire(neighbor))return neighbor;return std::nullopt; }
bool World::place_campfire(Position p) { p=wrap(p);if(!passable(p)||campfire(p)||primary_campfire_)return false;construction_cells_[{p.x,p.y}].campfire=true;primary_campfire_=p;return true; }
int World::stored_food(Position p) const { p=wrap(p);const auto found=construction_cells_.find({p.x,p.y});return found==construction_cells_.end()?0:static_cast<int>(found->second.food_stockpile.size()); }
bool World::store_food(Position p,const FoodItem& food) { p=wrap(p);if(!campfire(p)||food.nutrition<=0)return false;FoodItem stored=food;if(stored.shelf_life_days<=0)stored.shelf_life_days=food_shelf_life(stored.type);stored.age_days=std::max(0,stored.age_days);construction_cells_[{p.x,p.y}].food_stockpile.push_back(stored);return true; }
bool World::take_stored_food(Position p,FoodItem* food,const std::vector<FoodType>& preferences) { p=wrap(p);auto found=construction_cells_.find({p.x,p.y});if(found==construction_cells_.end()||!found->second.campfire||found->second.food_stockpile.empty())return false;auto& stock=found->second.food_stockpile;auto selected=stock.begin();auto rank=[&](FoodType type){const auto match=std::find(preferences.begin(),preferences.end(),type);return match==preferences.end()?static_cast<int>(preferences.size()):static_cast<int>(std::distance(preferences.begin(),match));};for(auto candidate=stock.begin()+1;candidate!=stock.end();++candidate){const auto candidate_score=std::tuple{rank(candidate->type),candidate->shelf_life_days-candidate->age_days,candidate->cooked?0:1};const auto selected_score=std::tuple{rank(selected->type),selected->shelf_life_days-selected->age_days,selected->cooked?0:1};if(candidate_score<selected_score)selected=candidate;}if(food)*food=*selected;stock.erase(selected);return true; }
int World::raw_stored_food(Position p) const { p=wrap(p);const auto found=construction_cells_.find({p.x,p.y});if(found==construction_cells_.end())return 0;return static_cast<int>(std::count_if(found->second.food_stockpile.begin(),found->second.food_stockpile.end(),[](const FoodItem& food){return !food.cooked;})); }
int World::cooked_stored_food(Position p) const { p=wrap(p);const auto found=construction_cells_.find({p.x,p.y});if(found==construction_cells_.end())return 0;return static_cast<int>(std::count_if(found->second.food_stockpile.begin(),found->second.food_stockpile.end(),[](const FoodItem& food){return food.cooked;})); }
bool World::cook_stored_food(Position p) { p=wrap(p);auto found=construction_cells_.find({p.x,p.y});if(found==construction_cells_.end()||!found->second.campfire)return false;auto& stock=found->second.food_stockpile;auto selected=std::min_element(stock.begin(),stock.end(),[](const FoodItem& left,const FoodItem& right){if(left.cooked!=right.cooked)return !left.cooked;return left.shelf_life_days-left.age_days<right.shelf_life_days-right.age_days;});if(selected==stock.end()||selected->cooked)return false;selected->cooked=true;selected->age_days=0;selected->shelf_life_days+=3;selected->nutrition+=std::max(5,selected->nutrition/5);return true; }
int World::age_stored_food() { int spoiled=0;for(auto&[_,cell]:construction_cells_){for(auto& food:cell.food_stockpile)++food.age_days;const auto before=cell.food_stockpile.size();std::erase_if(cell.food_stockpile,[](const FoodItem& food){return food.age_days>=food.shelf_life_days;});spoiled+=static_cast<int>(before-cell.food_stockpile.size());}return spoiled; }
int World::stored_wood(Position p) const { p=wrap(p);const auto found=construction_cells_.find({p.x,p.y});return found==construction_cells_.end()?0:found->second.wood_stockpile; }
int World::stored_branches(Position p) const { p=wrap(p);const auto found=construction_cells_.find({p.x,p.y});return found==construction_cells_.end()?0:found->second.branch_stockpile; }
bool World::store_materials(Position p,int wood_amount,int branch_amount) { p=wrap(p);if(!campfire(p)||wood_amount<0||branch_amount<0||wood_amount+branch_amount==0)return false;auto& cell=construction_cells_[{p.x,p.y}];cell.wood_stockpile+=wood_amount;cell.branch_stockpile+=branch_amount;return true; }
int World::stored_item(Position p,CraftItem item) const { p=wrap(p);const auto found=construction_cells_.find({p.x,p.y});if(found==construction_cells_.end())return 0;const auto stored=found->second.crafted_stockpile.find(item);return stored==found->second.crafted_stockpile.end()?0:stored->second; }
int World::stored_crafted_items(Position p) const { p=wrap(p);const auto found=construction_cells_.find({p.x,p.y});if(found==construction_cells_.end())return 0;int total=0;for(const auto&[_,amount]:found->second.crafted_stockpile)total+=amount;return total; }
std::vector<std::string> World::craftable_recipes(Position p) const { p=wrap(p);std::vector<std::string> available;if(!campfire(p))return available;const auto& cell=construction_cells_.at({p.x,p.y});for(const auto& recipe:crafting_recipes()){bool possible=cell.wood_stockpile>=recipe.wood&&cell.branch_stockpile>=recipe.branches;for(const auto&[item,amount]:recipe.items)possible=possible&&stored_item(p,item)>=amount;if(possible)available.push_back(recipe.key);}return available; }
bool World::craft(Position p,const std::string& recipe_key) { p=wrap(p);if(!campfire(p))return false;const auto recipe=std::find_if(crafting_recipes().begin(),crafting_recipes().end(),[&](const CraftingRecipe& candidate){return candidate.key==recipe_key;});if(recipe==crafting_recipes().end())return false;const auto available=craftable_recipes(p);if(std::find(available.begin(),available.end(),recipe_key)==available.end())return false;auto& cell=construction_cells_[{p.x,p.y}];cell.wood_stockpile-=recipe->wood;cell.branch_stockpile-=recipe->branches;for(const auto&[item,amount]:recipe->items)cell.crafted_stockpile[item]-=amount;cell.crafted_stockpile[recipe->output]+=recipe->output_count;return true; }
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
  replenish_branches();
  for(auto& candidate:animals_){
    if(!candidate.alive||candidate.type==AnimalType::Fish||std::uniform_int_distribution<int>(0,3)(rng)!=0)continue;
    std::vector<Position> options;for(const auto p:neighbors(candidate.position))if(passable(p))options.push_back(p);
    if(!options.empty())candidate.position=options[std::uniform_int_distribution<size_t>(0,options.size()-1)(rng)];
    if(candidate.type==AnimalType::Rabbit)rabbit_=candidate.position;
  }
}
void World::replenish_branches() {
  for(int y=0;y<height;++y)for(int x=0;x<width;++x){
    Position tree{x,y};
    if(terrain(tree)!=Terrain::Tree)continue;
    for(const auto foot:neighbors(tree))if(passable(foot)){
      auto& amount=construction_cells_[{foot.x,foot.y}].loose_branches;
      amount=std::min(3,amount+1);
    }
  }
}
std::string World::ascii(const std::vector<Agent>& agents) const {
  std::ostringstream out;
  for (int y=0;y<height;++y) { for (int x=0;x<width;++x) { Position p{x,y}; char c='.'; switch(terrain(p)){case Terrain::Wall:c='#';break;case Terrain::Water:c='~';break;case Terrain::Tree:c='T';break;case Terrain::Bush:c='B';break;default:break;} if(branches(p)>0)c=',';if(campfire(p))c='*'; for(const auto& resource:food_resources_)if(resource.position==p&&resource.amount>0){if(resource.type==FoodType::Roots)c='u';if(resource.type==FoodType::Mushrooms)c='m';} for(const auto& animal:animals_)if(animal.alive&&animal.position==p){if(animal.type==AnimalType::Rabbit)c='r';if(animal.type==AnimalType::Deer)c='d';if(animal.type==AnimalType::Boar)c='b';if(animal.type==AnimalType::Wolf)c='w';if(animal.type==AnimalType::Fish)c='f';} for (const auto& a:agents) if (a.alive && a.position==p) c='A'; out<<c; } out<<'\n'; }
  return out.str();
}

json World::checkpoint() const {
  json cells=json::array();
  for(const auto terrain:cells_)cells.push_back(static_cast<int>(terrain));
  json foods=json::array();
  for(const auto& resource:food_resources_)foods.push_back({
      {"type",static_cast<int>(resource.type)},
      {"x",resource.position.x},{"y",resource.position.y},
      {"amount",resource.amount},{"nutrition",resource.nutrition},{"capacity",resource.capacity}});
  json animals=json::array();
  for(const auto& animal:animals_)animals.push_back({
      {"id",animal.id},{"type",static_cast<int>(animal.type)},
      {"x",animal.position.x},{"y",animal.position.y},{"alive",animal.alive},
      {"danger",animal.danger},{"nutrition",animal.nutrition}});
  json construction=json::array();
  for(const auto&[position,cell]:construction_cells_){
    json stored=json::array();for(const auto& food:cell.food_stockpile)stored.push_back({{"type",static_cast<int>(food.type)},{"nutrition",food.nutrition},{"cooked",food.cooked},{"age_days",food.age_days},{"shelf_life_days",food.shelf_life_days}});
    json crafted=json::array();for(const auto&[item,amount]:cell.crafted_stockpile)if(amount>0)crafted.push_back({{"item",static_cast<int>(item)},{"amount",amount}});
    construction.push_back({{"x",position.first},{"y",position.second},{"wood",cell.wood},
      {"fibers",cell.fibers},{"shelter_level",cell.shelter_level},
      {"loose_branches",cell.loose_branches},{"campfire",cell.campfire},
      {"stored_wood",cell.wood_stockpile},{"stored_branches",cell.branch_stockpile},
      {"crafted_stockpile",std::move(crafted)},
      {"stored_food",std::move(stored)}});
  }
  return {{"width",width},{"height",height},{"cells",std::move(cells)},
          {"rabbit",{{"x",rabbit_.x},{"y",rabbit_.y},{"alive",rabbit_alive_}}},
          {"food_resources",std::move(foods)},{"animals",std::move(animals)},
          {"construction",std::move(construction)},
          {"primary_campfire",primary_campfire_?json{{"x",primary_campfire_->x},{"y",primary_campfire_->y}}:json(nullptr)}};
}

void World::restore_checkpoint(const json& state) {
  if(state.at("width").get<int>()!=width||state.at("height").get<int>()!=height)
    throw std::runtime_error("checkpoint world dimensions are incompatible");
  const auto& cells=state.at("cells");
  if(!cells.is_array()||cells.size()!=static_cast<std::size_t>(width*height))
    throw std::runtime_error("checkpoint world cells are invalid");
  std::vector<Terrain> restored_cells;
  restored_cells.reserve(cells.size());
  for(const auto& value:cells)restored_cells.push_back(static_cast<Terrain>(value.get<int>()));
  std::vector<FoodResource> restored_foods;
  for(const auto& value:state.at("food_resources"))restored_foods.push_back({
      static_cast<FoodType>(value.at("type").get<int>()),
      {value.at("x").get<int>(),value.at("y").get<int>()},
      value.at("amount").get<int>(),value.at("nutrition").get<int>(),
      value.at("capacity").get<int>()});
  std::vector<Animal> restored_animals;
  for(const auto& value:state.at("animals"))restored_animals.push_back({
      value.at("id").get<std::string>(),static_cast<AnimalType>(value.at("type").get<int>()),
      {value.at("x").get<int>(),value.at("y").get<int>()},value.at("alive").get<bool>(),
      value.at("danger").get<int>(),value.at("nutrition").get<int>()});
  std::map<std::pair<int,int>,ConstructionCell> restored_construction;
  bool has_branch_state=false;
  for(const auto& value:state.at("construction")){
    has_branch_state=has_branch_state||value.contains("loose_branches");
    ConstructionCell cell{value.at("wood").get<int>(),value.at("fibers").get<int>(),
        value.at("shelter_level").get<int>(),value.value("loose_branches",0),
        value.value("campfire",false),value.value("stored_wood",0),
        value.value("stored_branches",0),{}, {}};
    for(const auto& crafted:value.value("crafted_stockpile",json::array()))
      cell.crafted_stockpile[static_cast<CraftItem>(crafted.at("item").get<int>())]=
          crafted.at("amount").get<int>();
    for(const auto& food:value.value("stored_food",json::array()))cell.food_stockpile.push_back({
        static_cast<FoodType>(food.at("type").get<int>()),food.at("nutrition").get<int>(),
        food.value("cooked",false),food.value("age_days",0),
        food.value("shelf_life_days",food_shelf_life(static_cast<FoodType>(food.at("type").get<int>()))) });
    restored_construction[{value.at("x").get<int>(),value.at("y").get<int>()}]=std::move(cell);
  }
  const auto& rabbit=state.at("rabbit");
  cells_=std::move(restored_cells);
  food_resources_=std::move(restored_foods);
  animals_=std::move(restored_animals);
  construction_cells_=std::move(restored_construction);
  primary_campfire_.reset();
  const auto primary=state.value("primary_campfire",json(nullptr));
  if(!primary.is_null())primary_campfire_=wrap({primary.at("x").get<int>(),primary.at("y").get<int>()});
  if(!primary_campfire_)for(const auto&[coordinates,cell]:construction_cells_)if(cell.campfire){primary_campfire_=Position{coordinates.first,coordinates.second};break;}
  rabbit_={rabbit.at("x").get<int>(),rabbit.at("y").get<int>()};
  rabbit_alive_=rabbit.at("alive").get<bool>();
  if(!has_branch_state){replenish_branches();replenish_branches();}
}
}
