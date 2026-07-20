#include "autopoiesis/world.hpp"

#include <algorithm>
#include <cassert>

using namespace apo;

int main() {
  World depleted(42);
  const Position berry_patch{14,2};
  while(depleted.food(berry_patch)>0)assert(depleted.consume_food(berry_patch));
  assert(depleted.ecology().depleted_patches==1);
  std::mt19937 depleted_rng(12);
  for(int day=1;day<=5;++day){
    depleted.advance_nature(depleted_rng);
    assert(depleted.food(berry_patch)==0);
  }
  depleted.advance_nature(depleted_rng);
  assert(depleted.food(berry_patch)==1);
  assert(depleted.ecology().regrown_food>0);

  World renewing(42);
  std::mt19937 renewing_rng(42);
  for(int day=0;day<30;++day)renewing.advance_nature(renewing_rng);
  for(const auto type:{AnimalType::Rabbit,AnimalType::Deer,AnimalType::Boar,
                       AnimalType::Wolf,AnimalType::Fish}){
    assert(renewing.population(type)>0);
    assert(renewing.population(type)<=renewing.carrying_capacity(type));
  }
  assert(renewing.ecology().total_births>0);
  for(const auto& resource:renewing.food_resources())
    assert(resource.amount>=0&&resource.amount<=resource.capacity);

  World hunted(42);
  auto controlled=hunted.checkpoint();
  for(auto& animal:controlled["animals"]){
    if(animal["type"].get<int>()==static_cast<int>(AnimalType::Wolf)){
      animal["x"]=20;animal["y"]=20;
    }
    if(animal["type"].get<int>()==static_cast<int>(AnimalType::Deer)){
      animal["x"]=20;animal["y"]=19;
    }
  }
  hunted.restore_checkpoint(controlled);
  const int deer_before=hunted.population(AnimalType::Deer);
  std::mt19937 hunted_rng(7);
  hunted.advance_nature(hunted_rng);
  assert(hunted.population(AnimalType::Deer)==deer_before-1);
  assert(hunted.ecology().predations==1);

  World first(99),second(99);
  std::mt19937 first_rng(123),second_rng(123);
  for(int day=0;day<20;++day){first.advance_nature(first_rng);second.advance_nature(second_rng);}
  assert(first.checkpoint()==second.checkpoint());
  World restored(1);
  restored.restore_checkpoint(first.checkpoint());
  assert(restored.checkpoint()==first.checkpoint());
  assert(restored.ecology().day==20);
}
