# Glossaire, règles et patterns

Ce document est évolutif. Il sert de référence commune pour les humains, Codex et les futurs composants d'Autopoiesis.

## Vocabulaire

### Personnage

Individu simulé. Il possède un état, des besoins, une personnalité, une mémoire et une perception limitée.

### Décideur IA

Composant qui reçoit la perception structurée d'un personnage et propose une décision. Il ne modifie jamais directement le monde.

### Personnification

Voix et point de vue subjectif d'un personnage. Elle sert aux décisions, aux bilans et aux demandes formulées à l'humain.

### Instance architecte

Agent qui conçoit et configure le système d'évolution. Cette instance intervient lors de la mise sur pied du dispositif, mais ne participe pas au circuit opérationnel des demandes une fois celui-ci lancé.

### Instance validatrice

Processus Codex séparé qui lit une demande `pending` en lecture seule, contrôle son contrat et sa politique d'évolution, puis produit une décision structurée. Il ne modifie ni le moteur ni le worktree de Dieu.

### Moteur d'exécution

Noyau local qui applique les règles du monde. Il valide les décisions, modifie l'état et reste déterministe pour des entrées et une graine identiques.

### Validateur d'action

Barrière locale qui vérifie le nom de l'action, ses paramètres, ses préconditions et ses permissions avant toute exécution.

### Dieu

Architecte du monde, des capacités et des règles de simulation. Dieu transforme une demande approuvée en modification technique incrémentale. Il ne s'auto-autorise jamais : l'autorisation d'implémenter doit précéder toute modification, puis l'implémentation reste soumise aux tests et à la vérification.

### Diable

Générateur local de contraintes rationnelles du monde réel. À chaque fenêtre IA de référence de trois journées, il effectue un tirage reproductible avec une probabilité de `1 / DEVIL_CHANCE_ONE_IN`, soit une chance sur dix par défaut. Il ne réalise aucun appel API et ne modifie jamais directement le monde : son résultat est une demande d'évolution structurée, distincte de celles des personnages.

Une contrainte du Diable doit être compatible avec les capacités déjà actives, conserver au moins une mitigation immédiate, imposer une difficulté observable et ouvrir une pression cohérente vers une capacité future. Elle reste `pending` jusqu'à validation séparée. `DEVIL_AUTO_APPROVE=1` est une politique d'autorisation préalable explicite ; elle permet uniquement la transition vers `approved` et ne contourne ni Dieu, ni le TDD, ni la vérification, ni l'activation contrôlée.

### Mécanisme

Ensemble cohérent de règles déterministes ajoutées au moteur : nouvelle ressource, nouvel objet, nouvelle action, nouveau besoin, transformation du monde ou interaction entre personnages. Un mécanisme doit être livré avec ses tests avant d'être considéré actif.

### Capacité

Action ou aptitude disponible pour un personnage lorsque ses préconditions locales sont satisfaites. Une capacité peut dépendre d'un mécanisme, d'un outil, d'une ressource, d'un état interne ou d'une connaissance acquise.

### Demande d'évolution

Proposition structurée indiquant un besoin, un obstacle, un mécanisme souhaité et des critères d'acceptation. Son statut initial est `pending`. Elle ne peut devenir `approved` qu'après le contrôle de l'instance validatrice et l'autorisation prévue par la politique active. La politique actuelle exige une validation humaine explicite. Le Validator peut être humain (`VALIDATOR_MODE=human`) ou Codex (`VALIDATOR_MODE=codex`), mais le mode Codex ne fait qu'émettre une recommandation et ne franchit jamais cette garde.

### Cycle élémentaire

Un cycle élémentaire est un créneau pendant lequel chaque personnage vivant et éveillé reçoit au plus une décision locale. Il ne déclenche aucun appel API.

### Journée

Une journée contient `CYCLES_PER_DAY` cycles élémentaires, soit `2400` par défaut. Le paramètre `SIMULATION_DAYS` indique le nombre de journées exécutées par le programme.

### Jour et nuit

La phase diurne occupe les trois quarts d'une journée et la phase nocturne le quart restant. Avec l'horloge de référence, cela donne `1800` cycles de jour puis `600` cycles de nuit : la nuit dure un tiers du jour. Cette phase appartient au moteur déterministe, figure dans chaque perception, instantané graphique et historique d'action, et se déduit du cycle persistant.

### Feu de camp

Les cases praticables voisines d'un arbre produisent progressivement des branches. Un personnage peut ramasser une branche locale ; trois branches permettent d'allumer un feu de camp persistant sur sa case. Un feu n'entre dans sa mémoire spatiale qu'après avoir été perçu. Pendant le dernier sixième de la phase diurne et durant la nuit, il rejoint par la carte connue une case adjacente à un feu mémorisé et s'y repose, sauf urgence vitale prioritaire.

Le premier feu construit devient le foyer principal et empêche la création de foyers concurrents. Sa fumée transmet seulement ses coordonnées aux personnages : elle ne révèle aucun terrain ni chemin. Chaque membre adopte ce foyer, réserve la première case adjacente praticable encore libre et doit la rejoindre par sa propre carte connue avant de se reposer.

### Réserve commune

Chaque feu possède une réserve alimentaire persistante. Un personnage ne transporte initialement qu'un aliment à la fois, avec son type et sa nutrition. `collect_food` retire atomiquement cet aliment du monde, `deposit_food` exige un feu adjacent connu et transfère l'objet vers sa réserve, puis `eat_camp_food` permet à n'importe quel personnage adjacent de le consommer. Aucun aliment ne peut être dupliqué ou devenir commun sans ces transitions validées.

### Inventaire borné

La charge d'un personnage est la somme de son bois, de ses branches et de sa ration transportée, à raison d'une unité par élément. Sa capacité vaut `clamp(4 + force / 25, 4, 10)` et se déduit de sa force persistée. Le décideur masque les collectes impossibles et le moteur les refuse à nouveau avec `inventaire plein` : aucun appel IA ni décision invalide ne peut contourner cette borne.

### Transport de ressources

Une ressource connue devient une destination de navigation sur la seule carte mémorisée. Après collecte, le porteur conserve son objectif jusqu'à une case adjacente au foyer, où `deposit_food` ou `deposit_materials` transfère atomiquement sa charge vers la réserve correspondante. Le dépôt de matériaux vide ensemble le bois et les branches du porteur ; un échec ne modifie aucun des deux emplacements. Les stocks du foyer sont persistants et visibles au survol du feu.

### Repas et conservation

Une ration conserve son type, sa nutrition, son état cru ou cuit, son âge et sa durée de vie. Chaque début de journée vieillit les rations stockées et transportées ; atteindre la durée de vie les détruit avec un événement explicite. `cook_camp_food` choisit d'abord la ration crue la plus proche de la péremption, remet son âge à zéro, augmente sa nutrition de 20 % avec un minimum de 5 et ajoute trois jours de conservation. Pour manger, les préférences persistantes du personnage priment, puis la durée restante départage les rations. Une faim critique autorise un repas immédiat sans cuisson.

### Vie collective

L'adoption du foyer attribue une fois un rôle collectif selon la pulsion dominante : bâtisseur, intendant, éclaireur ou médiateur. Deux membres présents près du même feu peuvent partager un repas de deux rations, tenir une veillée uniquement la nuit, célébrer l'achèvement d'un projet ou honorer un mort. Repas et veillée marquent les deux participants pour la journée afin de rester bornés. Chaque activité produit des souvenirs et des effets déterministes sur faim, fatigue, monotonie ou relations ; rôles, dates d'activité, célébration en attente et deuils accomplis sont persistés.

### Recette de fabrication

Une recette porte une clé stable, des quantités de bois et de branches, une liste d'objets intermédiaires requis, un objet produit et une quantité de sortie. `craft_camp_item` n'est disponible qu'au foyer pour une recette entièrement approvisionnée. Le moteur vérifie toutes les entrées avant de les retirer, puis ajoute la sortie dans la même transition. Le registre initial produit manche en bois, charbon et corde ; ses objets et stocks sont typés, persistants et visibles dans le total de réserve.

### Outil et durabilité

Le minerai de fer apparaît en gisements finis sur des cases praticables et occupe une unité de charge. Deux minerais et un charbon produisent un lingot ; un lingot et un manche produisent une hache. `equip_axe` retire exactement une hache de la réserve. `harvest_wood` exige une hache équipée de durabilité positive, transforme un arbre en une unité de bois et retire un point de durabilité seulement en cas de réussite. Près du foyer, `repair_axe` consomme une unité de bois et restaure dix points sans dépasser le maximum de vingt.

### Compétence progressive

Les huit compétences sont coupe du bois, extraction, fabrication, construction, cueillette, chasse, cuisine et sociabilité. Une action n'accorde de l'expérience que si son effet moteur a réussi. Cinq points d'expérience donnent un niveau, avec un maximum de dix ; les niveaux et les trois spécialisations dominantes sont persistants et observables. La coupe du bois augmente directement l'efficacité d'une réparation. Au même foyer, un mentor ayant au moins deux niveaux d'avance peut transmettre un point de la compétence choisie ; chaque personnage donne et reçoit au plus une leçon par journée.

### Construction spatiale

Une désignation associe une case et un type parmi mur, porte, lit, réserve et atelier. Le foyer vérifie puis réserve toutes les matières et pièces requises dans une seule transition ; un échec ne crée aucun chantier et ne consomme rien. Depuis une case adjacente, un personnage vivant muni d'une hache fournit une unité de travail, augmentée tous les trois niveaux de construction, et use l'outil uniquement si le travail réussit. Un mur achevé bloque la navigation, une porte reste praticable, un lit améliore le repos et un atelier proche améliore la réparation. État, avancement et effets survivent au checkpoint.

### Mois, année et saison

Un mois contient 30 journées. Une année contient 12 mois, soit 360 journées. Les mois 1 à 3 forment le printemps, 4 à 6 l'été, 7 à 9 l'automne et 10 à 12 l'hiver. Le jour absolu est monotone pendant tout le run actif : une fenêtre IA ne remet à zéro ni le jour, ni le mois, ni l'année.

### Climat

État quotidien déterministe dérivé du jour absolu et de la saison : température, pluviométrie et condition. Il agit uniquement par des règles locales validées : régénération ou raréfaction progressive des aliments, soif lors des fortes chaleurs, faim et fatigue pendant le gel, avec protection mesurable des abris. Il ne déclenche aucun appel API.

### Monde torique

La carte canonique mesure `40 × 24`. Elle ne possède pas de bord bloquant : tout déplacement dépassant une coordonnée est normalisé vers le côté opposé. Les distances, perceptions, voisinages et chemins utilisent tous cette même topologie.

### Besoins vitaux

Un personnage suit sa santé, sa faim, sa soif et sa fatigue. L'eau et les aliments sont des ressources déterministes du monde ; une action locale validée reste nécessaire pour les consommer.

### Attributs

Les dix attributs de base sont la force, l'agilité, l'endurance, la robustesse, la récupération, la résistance aux maladies, la concentration, la volonté, la mémoire et le sens spatial. Un attribut ne doit pas être ajouté comme simple donnée décorative : son effet moteur ou décisionnel doit être explicite et testé.

### Aspiration et projet durable

L'aspiration donne une direction stable au personnage. Le projet durable est sa traduction déterministe avec une clé, un objectif mesurable, une progression et un statut parmi `candidate`, `active`, `blocked`, `completed` et `abandoned`. Un projet `blocked` nomme une capacité manquante concrète ; ce fait nourrit le bilan de fin de période sans créer lui-même une demande d'évolution.

### Monotonie et relation

La monotonie augmente lors des échecs, attentes et passages répétés, puis diminue avec une découverte, une réussite de projet ou une interaction. Une relation conserve confiance, affinité et nombre d'interactions entre deux personnages. Ces états appartiennent au moteur local, sont déterministes et peuvent influencer l'arbitrage sans appel API.

### Fenêtre IA

Une fenêtre IA regroupe `REPORT_EVERY_DAYS` journées. La configuration de référence est `REPORT_EVERY_DAYS=3`, donc `3 × 2400 = 7200` cycles élémentaires. À la fin de cette fenêtre, chaque personnage déclenche deux appels et seulement deux : un bilan, puis une demande d'évolution liée. Avec trois personnages, cela fait six appels. Pour un nouveau monde utilisant cette horloge, aucun appel n'est déclenché avant le cycle élémentaire `7200` et aucun retry HTTP ne doit ajouter un appel au quota.

Après ces six appels, le Diable effectue son tirage local. Ce tirage et la création éventuelle de sa contrainte n'ajoutent aucun appel API. La validation du Diable est une étape séparée et ne consomme pas le choix unique parmi les trois propositions des personnages.

Après chaque fenêtre IA, le moteur s'arrête et attend une confirmation humaine. La validation traite au maximum une demande parmi celles de la fenêtre ; les autres restent `pending`. La reprise est explicite (`o`) ; l'arrêt est explicite (`q`). Cette garde est active par défaut via `WAIT_FOR_HUMAN_VALIDATION=1`.
L'interface ne présente que les trois demandes les plus récentes de la fenêtre courante. Les autres propositions restent conservées dans les journaux et `pending`, mais ne sont pas proposées dans ce choix.

## Règles d'architecture

1. Le décideur IA propose ; le moteur d'exécution dispose.
2. Les cycles élémentaires sont exécutés localement ; à la fin de chaque fenêtre IA configurée, l'IA reçoit d'abord un bilan puis produit une demande d'évolution dans un second appel lié pour chaque personnage. Aucun appel IA ne décide les actions quotidiennes.
3. La simulation ne franchit jamais une fenêtre IA sans passer par la garde de confirmation humaine d'au plus une demande, sauf désactivation explicite pour un run automatisé.
4. Aucune réponse textuelle, justification ou demande IA ne peut modifier directement une variable du monde.
5. Toute action est refusée par défaut si elle est inconnue, mal paramétrée ou indisponible.
6. Une erreur IA ou réseau ne doit pas arrêter la simulation.
7. Le mode local doit rester utilisable sans réseau ni clé et doit être reproductible avec une graine identique.
8. La carte complète et les états invisibles ne doivent pas être transmis au décideur ou à l'observateur IA.
9. Les propositions d'évolution sont journalisées, contrôlées par l'instance validatrice, puis seulement transmises à Dieu après l'autorisation prévue par la politique d'approbation.
10. Les secrets restent dans l'environnement et ne sont jamais committés.
11. Toute évolution du moteur suit le TDD : test échouant d'abord, implémentation minimale, tests verts, puis revue.
12. Toute session de modification terminée doit se conclure par la compilation, les tests, un commit Git et un push vers le dépôt distant. Une modification non poussée n'est pas considérée comme livrée, car elle ne peut pas être récupérée pour lancer le jeu.
13. La topologie torique est un invariant transversal : aucune perception, distance ou navigation ne peut réintroduire implicitement un bord infranchissable.
14. Une aspiration ou un projet n'est pas décoratif : sa progression, son blocage et ses effets décisionnels doivent être observables et testés.
15. Un blocage local enrichit l'historique du personnage mais ne produit jamais directement une demande à Dieu ; seules les deux étapes IA de fin de fenêtre peuvent créer cette demande.
16. Le Diable ne crée que des demandes structurées issues d'un catalogue local testé ; il n'applique jamais lui-même une contrainte au monde.
17. Le calendrier et le climat progressent avec le jour absolu et ne se réinitialisent jamais à une frontière de fenêtre IA.
18. Un effet climatique doit être déterministe, borné, observable et laisser au moins une mitigation compatible avec les capacités actives.
19. Une interface graphique observe un instantané après chaque cycle élémentaire ; elle ne conserve aucun état du monde faisant autorité et ne contourne jamais le validateur d'action ou la validation humaine. Le rendu terminal peut rester journalier sans modifier la cadence réelle du moteur.
20. Le rendu animé d'une fenêtre IA ne modifie ni le nombre ni l'ordre des appels : un seul appel réseau est actif à la fois, puis l'étape suivante commence après son retour.
21. L'IA demandeuse reçoit avant sa proposition le catalogue des mécanismes actifs et une mémoire bornée des évolutions antérieures. Une nouvelle `evolution_key` ne rend jamais nouveau un mécanisme déjà proposé ou actif.
22. Après l'activation d'une évolution, l'ancien binaire ne peut pas exécuter la journée suivante : il doit sauvegarder, recompiler, transférer l'exécution à la version activée, puis restaurer le checkpoint.
23. Toute évolution qui modifie un état persistant doit conserver la lecture de la version précédente du checkpoint ou fournir une migration déterministe couverte par un test.
24. Pause et vitesse graphique ne changent ni l'ordre ni le nombre des cycles élémentaires. L'accélération peut réduire les rendus intermédiaires, jamais les décisions ou validations du moteur.
25. Une ressource transportée appartient à exactement un emplacement : monde, inventaire d'un personnage ou réserve. Chaque transfert est une action locale validée et persistante.
26. Une collecte ne peut jamais porter la charge au-delà de la capacité bornée du personnage. La disponibilité de l'action et son exécution appliquent toutes deux cette règle.
27. Un dépôt est conservatif et atomique : la quantité ajoutée à la réserve est exactement celle retirée de l'inventaire, ou aucun état ne change.
28. L'âge alimentaire avance une seule fois par journée complète et sans appel API. La cuisson et la consommation sont des actions locales validées ; une ration avariée n'est jamais consommable.
29. Une activité collective doit exiger la présence réelle de ses participants au même foyer, être bornée par personnage et par journée lorsque répétable, et modifier un état persistant observable.
30. Une recette ne peut consommer partiellement ses entrées : soit tous les coûts sont retirés et toutes les sorties ajoutées, soit le monde reste strictement identique.
31. Une action exigeant un outil doit être absente sans cet outil et revérifier son type et sa durabilité à l'exécution. Seule une action réussie peut user l'outil.
32. Une compétence ne progresse que sur une réussite validée. Une leçon exige la présence au même foyer, un écart de niveau suffisant et les limites quotidiennes du mentor comme de l'élève.
33. Une désignation de bâtiment réserve toutes ses entrées ou aucune. Le travail exige une proximité réelle et un outil valide ; un bâtiment incomplet ne produit aucun effet final.

## Patterns

### Décision structurée

Les décideurs retournent un objet JSON strict. Le moteur effectue toujours une validation locale secondaire.

### Fallback sûr

En cas de réponse invalide, d'erreur réseau ou de budget API épuisé : journaliser l'étape, la durée, le code cURL ou HTTP et les champs d'erreur utiles sans payload ni secret, exécuter le fallback prévu et poursuivre la simulation. Un diagnostic ne déclenche jamais de retry implicite.

### Mémoire séparée

La mémoire narrative et la mémoire spatiale sont stockées séparément. La première est courte et FIFO ; la seconde conserve les cases explorées par personnage.

### Mémoire persistante de période

Le bilan IA produit exactement deux fragments destinés à la continuité : une phrase factuelle `bilan` et une phrase subjective `ressenti`, chacune limitée à 180 caractères. Avant le bilan suivant, le moteur relit dans `ai_reports.jsonl` au maximum les douze périodes les plus récentes du même personnage. Il ne transmet ni les rapports complets ni les demandes passées. Cette mémoire bornée survit à un nouveau lancement et n'ajoute aucun appel aux deux appels réglementaires par personnage.

### Contexte de demande d'évolution

Le second appel de chaque personnage reçoit `active_world_mechanisms`, les actions actuellement disponibles, les propositions de la fenêtre et au maximum 24 évolutions antérieures compactes. Chaque souvenir d'évolution contient seulement son identifiant, son statut `pending`, `approved` ou `activated`, sa clé, son titre et un résumé court du mécanisme. Les rapports complets, tests et artefacts de Dieu ne sont pas retransmis.

Ce contexte sert au raisonnement de l'IA demandeuse, pas à un filtre sémantique local. Elle doit comparer le besoin observé aux mécanismes actifs et à l'historique avant de proposer. Si une capacité existe déjà mais n'est pas utilisée efficacement, la demande doit nommer l'intégration manquante démontrée par les actions ; elle ne doit ni recréer la capacité ni contourner le choix humain avec une nouvelle clé. Ajouter ou activer un mécanisme impose de tenir à jour le catalogue compact. Cette mémoire reste incluse dans le deuxième appel réglementaire et n'ajoute aucun appel API.

### Décision locale par utilité

Le décideur local compare les urgences de faim, soif et fatigue avec le projet, l'exploration et la coopération, maintient brièvement l'objectif retenu grâce à une hystérésis, puis cherche une route déterministe sur la carte connue. Il ne consulte jamais la carte complète. À état, mémoire et graine identiques, le choix reste identique.

Un cycle élémentaire est une opportunité de décision, pas l'obligation de se
déplacer. La décision locale suit la hiérarchie `besoin → objectif durable →
destination → chemin → exécution`. Manger, boire ou se reposer sur place est
prioritaire lorsque le besoin correspondant l'exige. Une ressource, un
personnage ou un projet connu fournit une destination précise. Explorer
signifie choisir une case frontière accessible de la carte mémorisée, voisine
d'une zone encore inconnue.

La destination d'exploration persiste entre les cycles et le chemin est calculé
sur la seule carte connue. Le personnage ne change de destination que si elle
est atteinte, devient inaccessible ou si un objectif plus urgent s'impose. Le
chemin peut être recalculé pour réagir à un obstacle sans abandonner
l'intention. Après douze déplacements consécutifs sans autre activité, le
personnage observe et réévalue son trajet. Un choix de case voisine sans cible
n'est autorisé qu'en fallback lorsque la mémoire ne permet aucun chemin.

### Évolution contrôlée

Une demande suit le flux `pending → approved → implementation → tests → verification → nouvelle version`. Aucune transition ne doit être implicite. `approved` autorise Dieu à travailler sur la demande ; il n'autorise pas à ignorer les tests ni à activer un résultat non vérifié.

Une recommandation `reformulate` crée une nouvelle demande liée à la précédente. Le nombre maximal de reformulations est `VALIDATOR_MAX_REFORMULATIONS` et vaut `3` par défaut. Une demande qui dépasse cette limite devient `rejected` et ne peut pas être transmise à Dieu.

Le daemon d'évolution traite uniquement les demandes ajoutées depuis le démarrage du `./run.sh` courant. Les demandes historiques restent journalisées avec leur statut, mais elles ne sont ni revalidées ni reformulées automatiquement lors d'une nouvelle session.

Après une première implémentation, le vérificateur peut renvoyer un diagnostic à Dieu. Dieu dispose de `GOD_MAX_CORRECTIONS` corrections, `2` par défaut. Chaque correction reprend le même worktree, ajoute ou ajuste un test, puis repasse par la vérification. Après la limite, le résultat reste rejeté et attend une décision humaine.

### Responsabilité Docker

La socket Docker de macOS se trouve hors du worktree et reste volontairement inaccessible à l'instance Dieu sandboxée. Dieu exécute la compilation et les tests locaux, puis indique que Docker est délégué. Le vérificateur externe `scripts/verify-evolution.sh` possède seul la responsabilité du build Docker obligatoire. Aucun commit, push ou activation ne peut avoir lieu si cette vérification échoue.

### Notification de fin

Sur macOS, le daemon hôte observe les artefacts terminaux du workflow et utilise `osascript` lorsque `MACOS_NOTIFICATIONS=1`. Il envoie au plus une notification par demande et par type pour une activation réussie, un échec terminal ou un timeout de l'interface. La notification est informative : elle ne modifie aucun statut et ne remplace ni les journaux ni la vérification.

### Changelog de Dieu

Journal généré pour chaque exécution de Dieu. Il décrit la demande approuvée, le mécanisme visé, les fichiers modifiés et le résultat de la vérification. Il est conservé dans les artefacts d'exécution et ne constitue ni un commit ni une autorisation d'activation.

### Interface de validation

Étape intégrée au processus de simulation lorsqu'une fenêtre IA est terminée. `HumanValidation` lit les demandes de cette fenêtre, tolère les lignes JSONL historiques invalides, demande d'abord de sélectionner une proposition ou `aucune`, permet ensuite de traiter au plus une demande et écrit les transitions humaines `approved` ou `rejected`. Elle reste l'unique autorité de cette transition.

En mode graphique, raylib présente les trois demandes les plus récentes sous forme de cartes à jouer. Le premier clic sélectionne une carte ; un second écran demande explicitement d'approuver, refuser ou revenir. La fenêtre ne modifie aucun journal elle-même : elle renvoie seulement les mêmes commandes structurées que l'interface terminal. En mode `--terminal`, le protocole historique reste disponible.

Après une approbation, raylib observe les mêmes artefacts que le suivi terminal et affiche les phases `file d'attente → préparation → TDD → compte rendu → vérification → activation`. Il montre la durée et le dernier retour utile sans interpréter ni modifier le résultat. Une activation réussie se termine par une confirmation explicite avant la reprise de la simulation ; cette confirmation remplace l'ancien second écran générique « Reprendre ».

Tout écran graphique de reprise expose le délai entre journées sous forme de slider borné de `0` à `10000 ms`. La valeur initiale vient de `SIMULATION_DELAY_MS`, puis le choix local s'applique aux journées suivantes du run sans modifier `.env`, le nombre de cycles ou le calendrier.

Après approbation, elle persiste la transition puis attend le workflow de Dieu lancé par le daemon d'évolution du même `./run.sh`. Elle affiche les phases et les artefacts disponibles, puis le résultat de la vérification ; elle n'exécute aucune règle du moteur et ne fusionne aucun worktree. L'attente est divisée en deux délais indépendants : `GOD_QUEUE_TIMEOUT_SECONDS` (900 secondes par défaut) avant le démarrage effectif, puis `GOD_WAIT_TIMEOUT_SECONDS` (900 secondes par défaut) pour le workflow de Dieu. L'interface affiche régulièrement la phase et la durée écoulée. En cas de dépassement ou d'erreur, elle montre les dernières lignes des journaux utiles et indique le dossier d'artefacts complet ; un timeout laisse le daemon continuer en arrière-plan.

### Interface graphique

Fenêtre native raylib superposée au run. Le moteur lui transmet un `UiSnapshot` copié et en lecture seule contenant la carte, le calendrier, le climat, les personnages, les animaux et les événements récents. Un clic sélectionne un personnage uniquement dans l'état local de l'interface ; il ne produit aucune décision et ne modifie jamais le monde. Lors d'une validation, elle devient une source de commande pour `HumanValidation`, jamais une source de statut parallèle.

Pendant un appel IA, le client réseau travaille hors du fil de rendu afin que la fenêtre continue à traiter ses images. L'écran indique le numéro de l'appel, le personnage, la nature de l'étape et le temps écoulé. Chaque travail est rejoint avant le suivant : cette séparation d'affichage ne crée jamais de parallélisme entre appels. Les éléments cliquables signalent leur disponibilité au survol sans déplacer ni redimensionner la mise en page.

Le mode graphique est le lancement normal sur le Mac. Le terminal reste attaché au même processus pour les appels IA, les validations et le suivi de Dieu, et `--terminal` conserve le rendu historique. Fermer la fenêtre constitue une demande d'arrêt propre détectée par le moteur.

Sur macOS, la fenêtre est créée avec `FLAG_WINDOW_UNFOCUSED` par défaut afin de ne pas imposer un changement d'espace de travail. `AUTOPOIESIS_FOCUS_WINDOW=1` constitue l'opt-in explicite au focus immédiat ; ce réglage ne change aucun état de simulation.

Pendant une journée, l'interface expose les vitesses `0,25×`, `0,5×`, `1×`, `2×` et `4×`, ainsi qu'une pause. À `2×` et `4×`, le moteur exécute toujours tous les cycles mais l'interface affiche respectivement un instantané sur deux ou sur quatre. Ce contrôle en cours de journée est distinct du slider qui règle le délai entre deux journées.

### Transfert de version

Le checkpoint `data/simulation-state.json` est une photographie versionnée et
atomique de l'état déterministe : calendrier, cycle élémentaire, monde,
personnages, mémoires, projets, historiques utiles, état du décideur local et
générateurs pseudo-aléatoires. Il est écrit après chaque journée complète ainsi
qu'avant la garde de validation. Les journaux narratifs ne remplacent jamais ce
checkpoint.

Après une activation réussie, l'interface affiche explicitement la phase
« Recompilation ». La compilation s'exécute dans un processus enfant et publie
ses détails dans `data/recompilation.log`, tandis que le fil graphique reste
réactif. Une compilation réussie conduit à un remplacement du processus par
`exec` : le même `run.sh`, le daemon, l'identifiant de budget API et les
descripteurs utiles restent en place, tandis que la nouvelle image du moteur
recharge le checkpoint. La géométrie de la fenêtre, le délai choisi et le
nombre de journées restantes sont transmis explicitement.

Le transfert reprend au premier cycle élémentaire de la journée suivante ; il
ne rejoue ni la fenêtre IA ni la validation déjà terminée. Un checkpoint
inconnu, incomplet ou incompatible provoque un arrêt explicite, jamais une
réinitialisation silencieuse. `--new-world` est la seule commande normale qui
autorise l'abandon volontaire de cet état. Si la compilation échoue, aucune
nouvelle version n'est exécutée et le checkpoint reste intact.

### Protocole d'intervention

Le protocole d'une évolution est une expérience complète, et non l'ajout direct d'une capacité :

1. Après chaque fenêtre IA, la personnification d'un personnage formule un bilan puis une demande d'évolution structurée.
2. L'instance validatrice examine le besoin, le périmètre, le mécanisme et les critères d'acceptation en lecture seule. Elle peut accepter, refuser ou demander une reformulation.
3. Une demande acceptée par la politique d'approbation passe explicitement de `pending` à `approved`. Cette transition constitue l'autorisation donnée à Dieu.
4. Dieu, l'orchestrateur d'évolution, transforme uniquement cette demande approuvée en test et en modification isolée du moteur. Il ne traite pas les autres demandes en attente.
5. Le test doit d'abord échouer, puis l'implémentation minimale doit le rendre vert. Dieu ne profite pas de l'occasion pour ajouter des mécanismes voisins.
6. Un vérificateur déterministe exécute la compilation, les tests ciblés et la suite complète. Il contrôle aussi que la demande initiale est réellement satisfaite et que les invariants du monde sont conservés.
7. Un compte rendu critique décrit le résultat, les écarts, les effets secondaires et la plus petite amélioration éventuellement justifiée. Cette critique ne déclenche aucune nouvelle évolution automatiquement.
8. Après une vérification réussie, l'orchestrateur committe le worktree, pousse `main` puis active la nouvelle version. Si la vérification échoue, la boucle de correction bornée est utilisée avant cette activation.

L'instance architecte qui a conçu ce dispositif ne joue aucun de ces rôles opérationnels. Elle ne valide pas les demandes et ne lance pas Dieu après la mise en service.

Le projet conserve actuellement une validation humaine explicite comme garde d'activation. Un mode entièrement autonome, dans lequel une politique préautorisée ferait passer une demande à `approved` et activerait une version, devra être décidé, documenté et testé séparément.

### Demande à Dieu

À chaque fin de fenêtre IA, l'observateur IA reçoit l'historique pertinent d'un personnage. Il peut transformer un besoin récurrent ou un obstacle en proposition concrète d'évolution du monde, des capacités ou des algorithmes. La proposition n'est jamais exécutée dans la simulation en cours : elle attend le contrôle de l'instance validatrice, l'autorisation donnée à Dieu et une intégration vérifiée dans une version ultérieure du moteur.

Une demande `pending` doit identifier un titre, le besoin, l'obstacle, le changement proposé et un mécanisme unique. Ce mécanisme décrit ses ressources, actions, préconditions, effets déterministes et tests d'acceptation non vides. Une demande IA qui ne respecte pas ce contrat est journalisée comme rejetée et ne devient pas `pending`.

Chaque demande IA possède aussi une `evolution_key` stable et un domaine. Les clés déjà proposées pendant la même fenêtre sont transmises aux appels suivants puis filtrées localement en cas de doublon. Cette déduplication ne déclenche aucun retry et ne modifie donc jamais la règle des deux appels par personnage.

Les bilans et demandes produits à la fin d'une fenêtre IA sont rédigés entièrement en français. Cette règle couvre tous les champs textuels structurés, notamment le titre, le mécanisme et les tests d'acceptation, sans ajouter d'appel de traduction.

### Croissance par mécanismes

La simulation se complexifie par petites extensions testées. Un personnage peut d'abord manger des baies ou chasser, puis demander de nouveaux moyens : couper des arbres, fabriquer une hache, ramasser du fer, fondre du métal, construire un abri, etc. Chaque étape doit introduire les ressources, capacités, préconditions et effets strictement nécessaires, sans court-circuiter le moteur déterministe.

### Reproductibilité

Tout hasard du moteur local passe par une graine configurable. Les réponses IA ne sont pas considérées comme déterministes ; pour les rejouer, il faut enregistrer ou simuler les décisions.

### Livraison obligatoire

Après chaque run de modifications : vérifier le diff et l'absence de secrets, compiler, exécuter les tests, construire Docker, committer les changements avec un message explicite, puis pousser la branche courante vers le dépôt distant. Dans un run automatisé de Dieu, la construction Docker est exclusivement exécutée par le vérificateur externe après les tests de Dieu. Si le commit ou le push échoue, le travail reste ouvert et l'échec doit être signalé ; il ne faut pas déclarer la modification terminée.

## Ajouts futurs

Ajouter ici les termes, invariants et patterns validés par le projet. Éviter d'y documenter une idée non encore acceptée comme règle.
