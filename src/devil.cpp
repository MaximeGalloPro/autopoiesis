#include "autopoiesis/devil.hpp"

#include <algorithm>

namespace apo {
namespace {
json constraint(std::string key,std::string domain,std::string title,std::string need,
                std::string obstacle,std::string change,std::string mechanism,
                std::vector<std::string> resources,std::vector<std::string> actions,
                std::vector<std::string> preconditions,std::vector<std::string> effects,
                std::vector<std::string> tests,std::string basis,
                std::vector<std::string> mitigations,std::string pressure){
  return {{"requested",true},{"evolution_key",std::move(key)},{"domain",std::move(domain)},
          {"title",std::move(title)},{"need",std::move(need)},{"obstacle",std::move(obstacle)},
          {"proposed_change",std::move(change)},
          {"mechanism",{{"name",mechanism},{"summary",mechanism},{"resources",std::move(resources)},
                        {"actions",std::move(actions)},{"preconditions",std::move(preconditions)},
                        {"deterministic_effects",std::move(effects)}}},
          {"acceptance_tests",std::move(tests)},{"real_world_basis",std::move(basis)},
          {"current_mitigations",std::move(mitigations)},{"future_pressure",std::move(pressure)},
          {"constraint_source","devil_catalog"}};
}

std::vector<json> catalog(const World& world,const std::vector<Agent>& agents){
  std::vector<json> values;
  const bool has_food=!world.food_resources().empty();
  const bool has_animals=!world.animals().empty();
  bool has_water=false;
  bool has_materials=false;
  for(int y=0;y<World::height;++y)for(int x=0;x<World::width;++x){
    const Position position{x,y};
    has_water=has_water||world.terrain(position)==Terrain::Water;
    has_materials=has_materials||world.wood(position)>0||world.fibers(position)>0;
  }

  if(has_materials)values.push_back(constraint(
      "night_cold_exposure","survie","Le froid nocturne met les personnages à l’épreuve",
      "Rendre l’abri et le regroupement utiles face aux variations normales de température.",
      "Le monde ne distingue actuellement ni la chaleur du jour ni le froid de la nuit.",
      "Ajouter une température journalière déterministe et une fatigue de froid atténuée par un abri ou la présence d’un autre personnage.",
      "Exposition thermique journalière",
      {"Phase du cycle élémentaire dans la journée","Température ambiante","Niveau d’abri de la case","Personnages présents sur la case"},
      {"Calculer la température courante","Appliquer l’exposition en fin de journée","Journaliser la protection utilisée"},
      {"Le personnage est vivant","La journée se termine","La température et les protections sont calculées avant toute mutation"},
      {"La température vaut 6 la nuit, 14 à l’aube et au crépuscule, 22 le jour","Sous 10 degrés, un personnage sans protection gagne 8 fatigue","Un abri annule cette fatigue et la présence d’un autre personnage la réduit à 4","Aucun effet thermique n’ajoute d’appel API"},
      {"Une nuit froide ajoute 8 fatigue à un personnage seul et exposé","Deux personnages sur la même case ne gagnent que 4 fatigue","Un personnage dans un abri ne gagne aucune fatigue de froid","La même journée et la même graine produisent les mêmes températures"},
      "La température baisse la nuit et l’exposition au froid augmente la dépense énergétique.",
      {"Se déplacer vers un abri existant","Se regrouper sur une même case","Construire progressivement un abri"},
      "Des nuits répétées rendent nécessaires plusieurs abris, le feu ou des vêtements."));

  if(has_food)values.push_back(constraint(
      "perishable_fresh_food","survie","La nourriture fraîche commence à se dégrader",
      "Introduire la conservation comme conséquence réaliste de la récolte et de la chasse.",
      "Les aliments frais restent utilisables indéfiniment sans stockage ni transformation.",
      "Faire perdre une unité par jour aux ressources fraîches non protégées, sans affecter les aliments explicitement conservés.",
      "Dégradation quotidienne des aliments frais",
      {"Ressources alimentaires fraîches présentes dans le monde","Jour courant","Éventuel stockage de conservation"},
      {"Identifier les aliments frais non protégés","Réduire leur quantité en fin de journée","Journaliser les pertes"},
      {"Une journée complète vient de se terminer","La ressource est fraîche et sa quantité est positive","La protection éventuelle est déterminée avant la diminution"},
      {"Chaque pile fraîche non protégée perd exactement une unité par jour sans devenir négative","Les ressources séchées ou explicitement conservées ne diminuent pas","La règle est déterministe"},
      {"Une pile fraîche de trois unités passe à deux après une journée","Une pile d’une unité passe à zéro sans valeur négative","Une ressource conservée reste inchangée","Deux mondes identiques subissent les mêmes pertes"},
      "Les aliments frais se dégradent sous l’action des microbes, de l’oxydation et des animaux opportunistes.",
      {"Consommer rapidement une ressource atteinte","Chercher une autre source de nourriture","Chasser ou récolter de nouveau"},
      "La perte régulière de nourriture fait émerger séchage, stockage, cuisine et réserves partagées."));

  if(has_water)values.push_back(constraint(
      "dry_period_water_pressure","survie","Une période sèche réduit temporairement l’eau disponible",
      "Faire de l’accès à plusieurs sources d’eau une vraie question de survie.",
      "Toutes les sources d’eau restent également accessibles quels que soient les jours écoulés.",
      "Tous les douze jours, rendre temporairement non buvable une source sur trois pendant trois jours selon ses coordonnées.",
      "Période sèche déterministe",
      {"Jour courant","Coordonnées canoniques des cases d’eau","État temporaire de disponibilité"},
      {"Détecter le début et la fin d’une période sèche","Marquer les sources affectées","Journaliser les changements"},
      {"Le jour est un multiple de douze pour le déclenchement","Seules les cases d’eau existantes peuvent être affectées","Au moins une source reste disponible"},
      {"Une source est affectée si (x+y) modulo 3 vaut zéro","L’indisponibilité dure exactement trois jours","Au moins une source reste buvable","Les terrains ne sont pas détruits"},
      {"Le jour 12 démarre une période sèche de trois jours","Deux sources de classes différentes n’ont pas forcément le même état","Une source redevient buvable à la fin","La simulation conserve toujours une source disponible"},
      "Les sécheresses réduisent temporairement le débit ou la qualité de certaines sources.",
      {"Rejoindre une autre eau connue","Explorer pour découvrir plusieurs sources","Boire avant une longue traversée"},
      "La répétition des sécheresses fait émerger récipients, puits et réserves d’eau."));

  if(has_animals)values.push_back(constraint(
      "territorial_predators","survie","Les prédateurs défendent désormais leur territoire",
      "Donner un coût réel à la proximité prolongée des animaux dangereux.",
      "Un animal dangereux adjacent reste passif tant qu’un personnage ne le chasse pas.",
      "À la fin d’une journée, un prédateur dangereux adjacent inflige une blessure déterministe à un personnage exposé.",
      "Territorialité des animaux dangereux",
      {"Animaux vivants et danger","Positions toriques","Robustesse des personnages","Abri éventuel"},
      {"Détecter les voisinages dangereux","Choisir la menace la plus dangereuse","Appliquer et journaliser la blessure"},
      {"Le personnage et l’animal sont vivants","Ils sont adjacents en topologie torique","Le danger de l’animal dépasse 50"},
      {"La perte de santé vaut au minimum 1 et dépend du danger moins la robustesse","Un abri annule l’attaque","Une seule attaque, la plus dangereuse, s’applique par personnage et par jour"},
      {"Un loup adjacent blesse un personnage exposé","Un personnage robuste subit moins de dégâts","Un abri empêche l’attaque","Deux prédateurs ne produisent qu’une attaque quotidienne"},
      "Les prédateurs et grands animaux défendent un espace vital et réagissent à une présence prolongée.",
      {"S’éloigner par déplacement","Chasser une menace accessible","Rejoindre un abri"},
      "Les territoires dangereux font émerger veille, armes, clôtures et coopération."));

  if(!agents.empty())values.push_back(constraint(
      "untreated_wound_recovery","survie","Les blessures non soignées guérissent lentement",
      "Donner une durée et un risque aux blessures au lieu d’une santé immédiatement stable.",
      "La santé perdue ne possède ni état de blessure ni conséquence persistante explicite.",
      "Créer une gravité de blessure après une perte de santé et réduire naturellement cette gravité d’une unité par jour de repos.",
      "Convalescence déterministe",
      {"Perte de santé subie","Gravité de blessure","Actions de repos de la journée","Robustesse et récupération"},
      {"Créer ou aggraver une blessure","Évaluer la convalescence quotidienne","Journaliser guérison ou aggravation"},
      {"Une perte de santé positive a eu lieu","La gravité reste bornée entre zéro et cent","Le bilan quotidien utilise uniquement les actions vérifiées"},
      {"La gravité augmente de la santé perdue","Une journée comprenant du repos réduit la gravité de 1 plus récupération divisée par 25","Une blessure grave augmente la fatigue de 4 par jour","Aucune guérison ne dépasse la santé maximale"},
      {"Une perte de cinq santé crée cinq gravité","Une journée de repos réduit la gravité selon la récupération","Une blessure grave augmente la fatigue","Deux replays identiques produisent la même convalescence"},
      "Une blessure demande du repos et peut continuer à limiter l’organisme avant sa guérison.",
      {"Se reposer","Dormir","Éviter temporairement chasse et prédateurs"},
      "Les blessures répétées font émerger bandages, médecine, plantes et entraide."));
  return values;
}
}

Devil::Devil(unsigned seed,int one_in):rng_(seed^0xD3A11u),one_in_(std::max(1,one_in)){}

std::optional<json> Devil::draw(int day,int simulation_cycle,const World& world,
                                const std::vector<Agent>& agents,
                                const std::set<std::string>& known_evolution_keys){
  if(std::uniform_int_distribution<int>(1,one_in_)(rng_)!=1)return std::nullopt;
  auto candidates=catalog(world,agents);
  if(candidates.empty())return std::nullopt;
  const auto first=std::uniform_int_distribution<std::size_t>(0,candidates.size()-1)(rng_);
  for(std::size_t offset=0;offset<candidates.size();++offset){
    auto proposal=candidates[(first+offset)%candidates.size()];
    if(known_evolution_keys.contains(proposal.value("evolution_key","")))continue;
    proposal["day"]=day;
    proposal["simulation_cycle"]=simulation_cycle;
    return proposal;
  }
  return std::nullopt;
}
}
