#include "autopoiesis/capability_registry.hpp"
#include "autopoiesis/group.hpp"

#include <cassert>
#include <stdexcept>

using namespace apo;

int main() {
  const auto& registry = FeatureRegistry::defaults();

  auto profile = registry.default_profile();
  assert(registry.valid_profile(profile));
  assert(registry.profile_active(profile, "core_survival", 1));
  assert(!registry.profile_active(profile, "camp_cooking"));
  assert(registry.feature("woodcutting", 1)->kind == FeatureKind::IndividualSkill);

  // A world profile contains collective mechanisms only. Skills remain learned
  // state on each Agent and cannot be unlocked for everybody.
  assert(!registry.activate_for_startup(profile, "woodcutting"));
  assert(registry.activate_for_startup(profile, "camp_cooking", 1));
  assert(registry.profile_active(profile, "camp_cooking", 1));
  assert(!registry.deactivate_for_startup(profile, "core_survival", 1));
  assert(registry.deactivate_for_startup(profile, "camp_cooking", 1));
  assert(registry.deactivate_for_startup(profile, "core_survival", 1));
  assert(profile.active_features.empty());

  assert(registry.activate_for_startup(profile, "core_survival", 1));
  assert(registry.activate_for_startup(profile, "camp_cooking", 1));
  const auto saved_profile = registry.profile_json(profile);
  const auto reloaded_profile = registry.profile_from_json(saved_profile);
  assert(reloaded_profile.active_features == profile.active_features);

  Group group(std::string{primary_group_id}, reloaded_profile, registry);
  assert(group.feature_active("core_survival", 1));
  assert(group.feature_active("camp_cooking", 1));
  const auto group_state = group.checkpoint();
  assert(group_state.at("world_profile") == saved_profile);

  Group restored;
  restored.restore_checkpoint(group_state, registry);
  assert(restored.active_features() == profile.active_features);
  assert(restored.feature_active("camp_cooking", 1));

  bool unknown_feature_rejected = false;
  try {
    static_cast<void>(registry.profile_from_json(
        {{"schema_version", 1},
         {"active_features", {{{"id", "unknown_feature"}, {"version", 1}}}}}));
  } catch (const std::runtime_error&) {
    unknown_feature_rejected = true;
  }
  assert(unknown_feature_rejected);

  Agent ada;
  Agent borin;
  add_skill_experience(ada, Skill::Woodcutting, 5);
  assert(skill_level(ada, Skill::Woodcutting) == 1);
  assert(skill_level(borin, Skill::Woodcutting) == 0);
}
