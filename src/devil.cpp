#include "autopoiesis/devil.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

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
  for(int y=0;y<World::height;++y)for(int x=0;x<World::width;++x){
    const Position position{x,y};
    has_water=has_water||world.terrain(position)==Terrain::Water;
  }

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
  values.push_back(constraint(
      "winter_exposure","climat","Le froid saisonnier menace les résidents exposés","Rendre l'abri utile pendant les périodes froides.",
      "Le froid ne produit pas encore de crise collective annoncée.","Annoncer trois jours froids et augmenter fatigue et faim hors abri.",
      "Vague de froid",{"température","abri","combustible"},{"annoncer","s'abriter","chauffer"},{"température sous zéro"},
      {"l'alerte précède l'effet","un abri annule la fatigue supplémentaire"},{"une alerte apparaît","un résident abrité est protégé"},
      "Le froid augmente les besoins énergétiques et le risque d'hypothermie.",{"Construire un abri","Conserver du combustible"},"Le froid fait émerger vêtements et chauffage."));
  values.push_back(constraint(
      "contagious_outbreak","santé","Une maladie contagieuse atteint le foyer","Mettre à l'épreuve les soins et la proximité sociale.",
      "Les maladies individuelles ne se propagent pas.","Une maladie grave non isolée peut contaminer un compagnon du même foyer.",
      "Contagion locale",{"maladie","proximité","résistance"},{"détecter","contaminer","isoler"},{"affection non traitée"},
      {"une seule transmission quotidienne","la résistance réduit la sévérité"},{"la contagion exige une proximité","un personnage isolé est protégé"},
      "Les infections transmissibles se diffusent par contacts rapprochés.",{"Soigner","Convalescer à distance"},"La contagion fait émerger hygiène et infirmerie."));
  values.push_back(constraint(
      "infrastructure_decay","construction","Les bâtiments s'usent sous le climat","Créer un coût d'entretien après la construction.",
      "Les structures achevées restent parfaites sans entretien.","Tempêtes et gel réduisent une durabilité persistante des bâtiments.",
      "Usure des infrastructures",{"durabilité","bâtiment","climat"},{"inspecter","réparer"},{"bâtiment achevé"},
      {"l'usure est bornée","une réparation consomme des matériaux"},{"une tempête use un mur","une réparation restaure la durabilité"},
      "Les structures se dégradent sous les intempéries et les usages.",{"Stocker du bois","Réparer avant rupture"},"L'usure fait émerger maintenance et architecture robuste."));
  values.push_back(constraint(
      "social_fragmentation","société","Une crise divise les rôles du foyer","Tester la cohésion d'un groupe devenu stable.",
      "Les désaccords n'affectent pas l'organisation collective.","Une pénurie prolongée augmente les conflits entre rôles incompatibles jusqu'à médiation.",
      "Crise de cohésion",{"rôles","pénurie","relations"},{"contester","médiatiser","réorganiser"},{"pénurie active","groupe d'au moins quatre"},
      {"les conflits nomment leur cause","une médiation peut les résoudre"},{"la pénurie crée une tension","la réconciliation restaure la coopération"},
      "Les groupes sous pression renégocient ressources, autorité et responsabilités.",{"Partager les réserves","Réconcilier","Réattribuer les rôles"},"La crise fait émerger gouvernance et règles communes."));
  values.push_back(constraint(
      "compound_ecological_crisis","écosystème","Sécheresse et épuisement se renforcent","Mettre un foyer avancé face à une crise écologique composée.",
      "Chaque pression environnementale reste isolée.","Une saison sèche ralentit simultanément eau, repousses et reproduction pendant une durée annoncée.",
      "Crise écologique composée",{"eau","repousse","faune","saison"},{"annoncer","ralentir","restaurer"},{"foyer stable","pression avancée"},
      {"la durée est déterministe","les capacités ne deviennent jamais négatives"},{"l'alerte précède la crise","la fin restaure les rythmes normaux"},
      "Les crises réelles combinent souvent sécheresse, rendement végétal et dynamique animale.",{"Diversifier les stocks","Préserver plusieurs sources"},"La crise fait émerger irrigation et gestion des écosystèmes."));
  const std::map<std::string,int> difficulties{{"perishable_fresh_food",1},{"dry_period_water_pressure",2},
      {"untreated_wound_recovery",2},{"territorial_predators",3},{"winter_exposure",2},
      {"contagious_outbreak",3},{"infrastructure_decay",4},{"social_fragmentation",5},
      {"compound_ecological_crisis",5}};
  for(auto& value:values)value["difficulty"]=difficulties.at(value.value("evolution_key",""));
  return values;
}

int stability_score(const World& world,const std::vector<Agent>& agents){
  if(agents.empty())return 0;
  int living=0,health=0,needs=0,tools=0;
  for(const auto& agent:agents)if(agent.alive){++living;health+=agent.health;needs+=200-agent.hunger-agent.thirst;if(agent.equipped_tool)++tools;}
  if(living==0)return 0;
  int score=(health/living)*35/100+std::max(0,needs/living)*25/200+(tools*10/living);
  if(const auto camp=world.primary_campfire()){
    score+=10;score+=std::min(10,world.stored_food(*camp)*2/living);
    bool shelter=world.shelter_level(*camp)>0;for(const auto position:world.neighbors(*camp))shelter=shelter||world.shelter_level(position)>0;
    if(shelter||world.has_completed_building(BuildingType::Bed))score+=10;
  }
  return std::clamp(score,0,100);
}
}

Devil::Devil(unsigned seed,int one_in):rng_(seed^0xD3A11u),one_in_(std::max(1,one_in)){}

std::string Devil::rng_checkpoint() const {
  std::ostringstream output;output<<rng_;return output.str();
}

void Devil::restore_rng_checkpoint(const std::string& state) {
  std::istringstream input(state);input>>rng_;
  if(!input)throw std::runtime_error("checkpoint devil RNG is invalid");
}

std::optional<json> Devil::draw(int day,int simulation_cycle,const World& world,
                                const std::vector<Agent>& agents,
                                const std::set<std::string>& known_evolution_keys){
  if(std::uniform_int_distribution<int>(1,one_in_)(rng_)!=1)return std::nullopt;
  auto candidates=catalog(world,agents);
  if(candidates.empty())return std::nullopt;
  const int stability=stability_score(world,agents);
  const int pressure_level=std::clamp(1+day/90+static_cast<int>(known_evolution_keys.size())+(stability>=70?1:0),1,5);
  std::vector<json> eligible;for(auto& candidate:candidates)if(!known_evolution_keys.contains(candidate.value("evolution_key",""))&&candidate.value("difficulty",1)<=pressure_level)eligible.push_back(candidate);
  if(eligible.empty())for(auto& candidate:candidates)if(!known_evolution_keys.contains(candidate.value("evolution_key","")))eligible.push_back(candidate);
  if(eligible.empty())return std::nullopt;
  const int target_difficulty=std::max_element(eligible.begin(),eligible.end(),[](const json& left,const json& right){return left.value("difficulty",1)<right.value("difficulty",1);})->value("difficulty",1);
  std::vector<json> tier;for(const auto& candidate:eligible)if(candidate.value("difficulty",1)==target_difficulty)tier.push_back(candidate);
  auto proposal=tier[std::uniform_int_distribution<std::size_t>(0,tier.size()-1)(rng_)];
  proposal["day"]=day;proposal["simulation_cycle"]=simulation_cycle;
  proposal["adaptation"]={{"stability_score",stability},{"pressure_level",pressure_level},
      {"historical_pressures",known_evolution_keys.size()},
      {"rationale","Difficulté "+std::to_string(target_difficulty)+" choisie pour une stabilité de "+std::to_string(stability)+"/100 et un niveau de pression "+std::to_string(pressure_level)+"."}};
  return proposal;
}
}
