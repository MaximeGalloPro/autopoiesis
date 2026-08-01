#include "autopoiesis/world.hpp"

#include <cassert>

using namespace apo;

int main() {
  World world(42);
  const Position campfire{13,2};
  assert(world.place_campfire(campfire));

  const auto chest=world.camp_chest_position(campfire);
  assert(chest.has_value());
  assert(world.adjacent(campfire,*chest));
  assert(world.camp_chest_level(campfire)==1);
  const int initial_capacity=world.camp_chest_capacity(campfire);
  assert(initial_capacity>0);
  assert(world.camp_chest_occupation(campfire)==0);

  assert(world.store_materials(campfire,4,5,0));
  assert(world.camp_chest_occupation(campfire)==9);
  assert(world.upgrade_camp_chest(campfire));
  assert(world.camp_chest_level(campfire)==2);
  assert(world.camp_chest_capacity(campfire)>initial_capacity);
  assert(world.stored_wood(campfire)==2);
  assert(world.stored_branches(campfire)==3);

  assert(world.craft(campfire,"wooden_handle"));
  assert(world.craft(campfire,"rope"));
  assert(world.stored_item(campfire,CraftItem::WoodenHandle)==1);
  assert(world.stored_item(campfire,CraftItem::Rope)==1);
  assert(world.camp_chest_occupation(campfire)==3);

  assert(world.store_materials(campfire,world.camp_chest_capacity(campfire)-
                               world.camp_chest_occupation(campfire),0,0));
  assert(world.camp_chest_occupation(campfire)==world.camp_chest_capacity(campfire));
  const auto full=world.checkpoint();
  assert(!world.store_food(campfire,FoodItem{FoodType::Berries,35}));
  assert(!world.store_materials(campfire,1,0,0));
  assert(world.checkpoint()==full);

  World restored(7);
  restored.restore_checkpoint(world.checkpoint());
  assert(restored.camp_chest_position(campfire)==chest);
  assert(restored.camp_chest_level(campfire)==2);
  assert(restored.camp_chest_capacity(campfire)==world.camp_chest_capacity(campfire));
  assert(restored.camp_chest_occupation(campfire)==world.camp_chest_occupation(campfire));
  assert(restored.stored_item(campfire,CraftItem::WoodenHandle)==1);
  assert(restored.stored_item(campfire,CraftItem::Rope)==1);

  auto legacy=world.checkpoint();
  for(auto& cell:legacy["construction"])cell.erase("camp_chest");
  World legacy_restored(8);
  legacy_restored.restore_checkpoint(legacy);
  assert(legacy_restored.camp_chest_position(campfire).has_value());
  assert(legacy_restored.camp_chest_level(campfire)==2);
}
