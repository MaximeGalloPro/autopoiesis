#include "autopoiesis/feature_request.hpp"

#include <cctype>

namespace apo {
namespace {
bool non_empty_string(const json& value) {
  if (!value.is_string()) return false;
  for (const auto character : value.get<std::string>()) {
    if (!std::isspace(static_cast<unsigned char>(character))) return true;
  }
  return false;
}

bool non_empty_strings(const json& value) {
  if (!value.is_array() || value.empty()) return false;
  for (const auto& item : value) {
    if (!non_empty_string(item)) return false;
  }
  return true;
}

bool require_string(const json& object, const char* field, std::string& error) {
  if (object.contains(field) && non_empty_string(object[field])) return true;
  error = std::string("champ requis invalide : ") + field;
  return false;
}

bool require_strings(const json& object, const char* field, std::string& error) {
  if (object.contains(field) && non_empty_strings(object[field])) return true;
  error = std::string("liste requise invalide : ") + field;
  return false;
}
}

bool validate_feature_request(const json& request, std::string& error) {
  error.clear();
  if (!request.is_object()) {
    error = "la demande doit etre un objet";
    return false;
  }
  if (!request.contains("requested") || !request["requested"].is_boolean() || !request["requested"].get<bool>()) {
    error = "la demande doit etre explicitement demandee";
    return false;
  }
  for (const char* field : {"evolution_key", "domain", "title", "need", "obstacle", "proposed_change"}) {
    if (!require_string(request, field, error)) return false;
  }
  if (!request.contains("mechanism") || !request["mechanism"].is_object()) {
    error = "champ requis invalide : mechanism";
    return false;
  }
  const auto& mechanism = request["mechanism"];
  for (const char* field : {"name", "summary"}) {
    if (!require_string(mechanism, field, error)) return false;
  }
  for (const char* field : {"resources", "actions", "preconditions", "deterministic_effects"}) {
    if (!require_strings(mechanism, field, error)) return false;
  }
  return require_strings(request, "acceptance_tests", error);
}

json active_world_mechanisms() {
  return json::array({
      {{"key","toroidal_world_and_climate"},
       {"summary","Carte torique de 40 par 24 cases, calendrier persistant, saisons, température, pluie et effets climatiques déterministes."},
       {"resources",{"terrain","eau","arbres","buissons","abris"}},
       {"actions",json::array()}},
      {{"key","food_water_and_hunting"},
       {"summary","Faim et soif, eau potable, cinq nourritures et cinq animaux avec consommation, chasse, nutrition et danger."},
       {"resources",{"baies","racines","champignons","poisson","venaison","eau"}},
       {"actions",{"drink","eat_food","eat_berries","hunt_animal","hunt_rabbit"}}},
      {{"key","shelter_construction"},
       {"summary","Récolte locale de bois et construction déterministe d'un abri à partir de matériaux disponibles."},
       {"resources",{"arbres","bois","fibres","abri"}},
       {"actions",{"harvest_wood","build_shelter","assemble_shelter"}}},
      {{"key","campfire_night_routine"},
       {"summary","Les journées comportent 1800 cycles de jour puis 600 cycles de nuit. Le premier feu devient un foyer collectif signalé par sa fumée, avec une place distincte par personnage et une réserve commune pour partager la nourriture."},
       {"resources",{"branches","feu principal","foyer mémorisé","place de repos","phase jour-nuit","réserve commune","nourriture transportée"}},
       {"actions",{"collect_branch","build_campfire","rest_by_campfire","collect_food","deposit_food","eat_carried_food","eat_camp_food"}}},
      {{"key","bounded_inventory"},
       {"summary","Chaque personnage possède une capacité de transport bornée dérivée de sa force. Bois, branches et ration occupent chacun une unité et aucune collecte ne peut dépasser cette capacité."},
       {"resources",{"charge transportée","capacité d'inventaire"}},
       {"actions",{"harvest_wood","collect_branch","collect_food"}}},
      {{"key","camp_resource_transport"},
       {"summary","Les personnages ciblent des ressources connues, les portent par leur carte mémorisée et déposent nourriture, bois et branches dans les réserves persistantes du foyer."},
       {"resources",{"bois stocké","branches stockées","réserve du foyer"}},
       {"actions",{"collect_branch","harvest_wood","collect_food","deposit_materials","deposit_food"}}},
      {{"key","needs_and_recovery"},
       {"summary","Santé, faim, soif et fatigue avec sommeil, repos stationnaire et priorisation locale des urgences vitales."},
       {"resources",json::array()},
       {"actions",{"sleep","rest"}}},
      {{"key","obstacle_aware_navigation"},
       {"summary","Exploration déterministe utilisant la mémoire des cases, les obstacles connus, les visites et un routage BFS vers nourriture et eau."},
       {"resources",{"mémoire spatiale","carte connue"}},
       {"actions",{"observe","move"}}},
      {{"key","social_interaction"},
       {"summary","Dialogue local entre personnages adjacents avec confiance, affinité, interactions et monotonie persistantes pendant le run."},
       {"resources",{"relations"}},
       {"actions",{"talk"}}},
      {{"key","durable_projects_and_memory"},
       {"summary","Aspirations, projets durables, blocages explicites et mémoire narrative bornée entre les bilans IA."},
       {"resources",{"projet","souvenirs de période"}},
       {"actions",json::array()}}
  });
}
}
