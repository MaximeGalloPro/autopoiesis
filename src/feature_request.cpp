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
      {{"key","planned_food_and_preservation"},
       {"summary","Les rations ont un type, une valeur nutritive, un âge et une durée de conservation. Les préférences guident les repas ; la cuisson privilégie les denrées urgentes, améliore leur nutrition et prolonge leur durée de vie."},
       {"resources",{"ration crue","ration cuite","fraîcheur","préférences alimentaires"}},
       {"actions",{"cook_camp_food","eat_camp_food","eat_carried_food"}}},
      {{"key","collective_camp_life"},
       {"summary","Le foyer attribue des rôles selon les aptitudes et permet des repas partagés, veillées nocturnes, célébrations et deuils bornés, persistants et reliés aux relations."},
       {"resources",{"rôle collectif","mémoire sociale","accomplissement","deuil"}},
       {"actions",{"share_camp_meal","hold_vigil","celebrate","mourn"}}},
      {{"key","explicit_crafting_recipes"},
       {"summary","Un registre déterministe décrit les coûts et produits de chaque recette. Le foyer fabrique manches, charbon et cordes par transformations atomiques et persistantes, sans création implicite de matière."},
       {"resources",{"manche en bois","charbon","corde","registre de recettes"}},
       {"actions",{"craft_camp_item"}}},
      {{"key","required_tools_and_iron"},
       {"summary","Le minerai de fer est collecté et transporté. Charbon et minerai donnent un lingot, combiné à un manche pour fabriquer une hache équipable, usée par l'abattage et réparable avec du bois."},
       {"resources",{"minerai de fer","lingot de fer","hache","durabilité"}},
       {"actions",{"collect_iron_ore","craft_camp_item","equip_axe","harvest_wood","repair_axe"}}},
      {{"key","progressive_skills"},
       {"summary","Huit compétences gagnent de l'expérience uniquement par une pratique réussie. Les niveaux créent des spécialisations visibles, améliorent l'efficacité et peuvent être transmis une fois par jour au foyer par un mentor expérimenté."},
       {"resources",{"expérience","niveau de compétence","spécialisation","leçon"}},
       {"actions",{"teach_skill","harvest_wood","collect_iron_ore","craft_camp_item","repair_axe","build_shelter","collect_food","hunt_animal","cook_camp_food"}}},
      {{"key","spatial_building_designations"},
       {"summary","Le foyer réserve atomiquement les coûts de murs, portes, lits, réserves et ateliers avant de créer un chantier spatial. Un bâtisseur équipé rejoint le site, fournit un travail mesuré par sa compétence et produit un bâtiment persistant avec un effet propre."},
       {"resources",{"chantier","mur","porte","lit","réserve","atelier"}},
       {"actions",{"designate_building","work_on_building"}}},
      {{"key","renewable_ecosystem"},
       {"summary","Les végétaux repoussent sous capacité, avec une récupération lente après épuisement total. Chaque espèce animale se reproduit à une cadence bornée par sa capacité locale, tandis que les loups prélèvent des herbivores réellement adjacents."},
       {"resources",{"capacité écologique","parcelle épuisée","population","naissance","prédation"}},
       {"actions",{"collect_food","eat_food","hunt_animal"}}},
      {{"key","detailed_health"},
       {"summary","Blessures, maladies et infections conservent cause, sévérité, âge et traitement. Une blessure négligée peut s'infecter ; un compagnon soigne au foyer et le patient convalesce dans un abri ou un lit."},
       {"resources",{"blessure","maladie","infection","traitement","convalescence"}},
      {"actions",{"treat_condition","convalesce","hunt_animal","eat_food"}}},
      {{"key","causal_emotions"},
       {"summary","Joie, peur, colère, tristesse, espoir et stress conservent une cause, un souvenir, une intensité, une durée et un effet décisionnel borné. La peur peut notamment écarter une chasse trop dangereuse."},
       {"resources",{"émotion","cause","intensité","durée","effet"}},
       {"actions",{"hunt_animal","celebrate","mourn"}}},
      {{"key","evolving_population"},
       {"summary","La population vieillit chaque jour. Un foyer abrité et approvisionné accueille périodiquement un voyageur ou une naissance, tandis que la détresse sans soutien provoque un départ et le grand âge une mort explicite."},
       {"resources",{"âge","origine","parents","arrivée","départ","cause de mort"}},
       {"actions",json::array()}},
      {{"key","active_dangers"},
       {"summary","Prédateurs, tempêtes, départs de feu, risques d'accident et pénuries produisent une alerte persistante un jour avant leurs effets localisés. Chaque danger nomme sa cause, sa sévérité et une mitigation possible."},
       {"resources",{"alerte","prédateur","tempête","incendie","accident","pénurie"}},
       {"actions",{"move","rest","collect_food","build_shelter"}}},
      {{"key","needs_and_recovery"},
       {"summary","Santé, faim, soif et fatigue avec sommeil, repos stationnaire et priorisation locale des urgences vitales."},
       {"resources",json::array()},
       {"actions",{"sleep","rest"}}},
      {{"key","obstacle_aware_navigation"},
       {"summary","Exploration déterministe utilisant la mémoire des cases, les obstacles connus, les visites et un routage BFS vers nourriture et eau."},
       {"resources",{"mémoire spatiale","carte connue"}},
       {"actions",{"observe","move"}}},
      {{"key","social_interaction"},
       {"summary","Relations actives entre personnages adjacents : dialogue, partage d'un danger, aide à la récupération, accompagnement temporaire, conflit causal et réconciliation persistante."},
       {"resources",{"relations","conflit","compagnon","connaissance partagée"}},
       {"actions",{"talk","warn_danger","help_companion","accompany","confront","reconcile"}}},
      {{"key","durable_projects_and_memory"},
       {"summary","Aspirations, projets durables, blocages explicites et mémoire narrative bornée entre les bilans IA."},
       {"resources",{"projet","souvenirs de période"}},
       {"actions",json::array()}}
  });
}
}
