# Autopoiesis

Autopoiesis simule des personnages dans un monde déterministe. Le moteur local
fait avancer leurs besoins et leurs actions ; l'IA intervient seulement pour
faire le bilan d'une période et proposer une évolution contrôlée.

Le lancement normal ouvre une fenêtre graphique native raylib. Elle montre la
carte, le calendrier et le climat ; un clic sur un personnage ouvre son état,
son humeur, son projet, ses actions disponibles et ses attributs. Cette vue ne
modifie jamais directement la simulation. Les personnages, cartes et boutons
réagissent au survol pour rendre les interactions visibles.

Les commandes `-`, pause et `+` en haut de la fenêtre règlent le run à
`0,25×`, `0,5×`, `1×`, `2×` ou `4×`. La barre d'espace met également en pause.
Même en accéléré, chaque cycle et chaque décision sont exécutés ; seule la
fréquence de rendu diminue à `2×` et `4×`.

Pendant les six appels de fin de fenêtre, l'interface reste animée et affiche
l'étape active, les étapes terminées, le personnage concerné et le temps
écoulé. Les appels restent strictement séquentiels : ce rendu n'ajoute aucun
appel et ne change pas leur ordre.

À la fin d'une fenêtre IA, les trois propositions les plus récentes prennent la
forme de cartes à jouer dans cette même fenêtre. Un clic ouvre la confirmation :
approuver transmet la demande à Dieu, refuser la clôt, revenir conserve les
trois cartes et « aucune évolution » reprend sans décision. Une seule carte
peut toujours être traitée par fenêtre.

```text
actions locales → 3 journées → bilan IA → demande IA à Dieu
                                  (2 appels par personnage)
              → tirage local du Diable (1 chance sur 10)
                                  → Validator → approbation → Dieu → tests
```

## Horloge et appels API

- Un **cycle élémentaire** est un créneau d'action pour chaque personnage.
- Une **journée** contient `CYCLES_PER_DAY=2400` cycles élémentaires.
- Elle comprend `1800` cycles de jour puis `600` cycles de nuit : la nuit dure donc un tiers de la période diurne.
- Un **mois** contient 30 journées et une **année** contient 12 mois, soit 360 journées.
- Les mois 1 à 3 forment le printemps, 4 à 6 l'été, 7 à 9 l'automne et 10 à 12 l'hiver. Le calendrier ne se remet pas à zéro lors d'une fenêtre IA.
- La fenêtre IA par défaut est `REPORT_EVERY_DAYS=3`, donc `7200` cycles élémentaires.
- À la fin de cette fenêtre, chaque personnage déclenche exactement deux appels, dans cet ordre : bilan, puis demande d'évolution.
- Les bilans et tous les champs des demandes d'évolution sont produits en français dans ces mêmes appels.
- Avec trois personnages, cela représente six appels API au cycle élémentaire `7200` d'un nouveau monde configuré ainsi.
- Aucun appel API n'est lancé entre les cycles élémentaires `1` et `7199`, et aucun retry HTTP n'est effectué.
- L'interface graphique publie un nouvel instantané après chaque cycle élémentaire. À 60 FPS, les déplacements restent visibles pendant les 7200 cycles qui précèdent la fenêtre IA.
- Le rendu terminal reste un résumé journalier réglé par `SIMULATION_RENDER_EVERY_DAYS` ; cette variable ne ralentit ni ne masque les cycles dans l'interface graphique.
- Chaque nouveau bilan reçoit au maximum les douze souvenirs précédents du personnage, chacun réduit à une phrase de bilan et une phrase de ressenti de 180 caractères maximum. Cette mémoire vient de `ai_reports.jsonl`, survit aux relancements et n'ajoute aucun appel API.
- Le second appel reçoit aussi le catalogue compact des mécanismes actifs, les actions actuellement disponibles et au maximum 24 évolutions antérieures `pending`, `approved` ou `activated`. Il doit proposer un mécanisme réellement nouveau plutôt que rebaptiser une ancienne demande ; ce contexte n'ajoute aucun appel API.
- Le moteur s'arrête à la fin de chaque fenêtre IA et attend une confirmation humaine avant de poursuivre. `o` reprend, `q` arrête le run.
- La fenêtre graphique et le terminal affichent l'avancement des six appels (`en cours`, `terminé` ou `indisponible`) avant d'ouvrir cette pause.
- Après l'approbation d'une évolution, la fenêtre graphique suit aussi Dieu : file d'attente, préparation, TDD, compte rendu, vérification et activation. Le dernier retour utile et le temps écoulé restent visibles, tandis que les logs complets continuent dans le terminal.
- Une activation terminée affiche « C'est terminé. Voulez-vous passer à l'étape suivante ? ». Continuer ouvre l'écran « Recompilation », construit la nouvelle version en arrière-plan, puis lui transfère le monde au jour suivant ; arrêter termine proprement le run.
- La validation est intégrée au même terminal et ne présente que les trois propositions les plus récentes de la fenêtre : choisir `1`, `2`, `3` ou `n` pour aucune, puis `a` approuve ou `r` refuse la proposition sélectionnée. `o` reprend et `q` ou `exit` arrête proprement. Les autres demandes restent `pending`.

## Le Diable

À chaque fenêtre de trois journées, un tirage local et reproductible donne au
Diable une chance sur dix d'apparaître. Il choisit dans un catalogue de
contraintes du monde réel compatibles avec les capacités présentes : froid,
périssabilité, sécheresse, territorialité animale ou convalescence. Chaque
contrainte conserve des moyens immédiats de mitigation tout en créant une
pression susceptible de faire émerger une future demande.

Le tirage et le catalogue ne déclenchent aucun appel API. Une contrainte du
Diable est proposée dans une étape séparée et ne remplace aucune des trois
demandes des personnages. Avec `DEVIL_AUTO_APPROVE=0`, elle attend `a` ou `r`
dans le terminal. `DEVIL_AUTO_APPROVE=1` constitue une autorisation préalable :
la contrainte passe automatiquement à `approved`, puis suit exactement le même
workflow Dieu, TDD, vérification Docker, commit, push et activation. La
probabilité se règle avec `DEVIL_CHANCE_ONE_IN=10`.

Les décisions quotidiennes utilisent actuellement le décideur local. Une réponse
IA ne modifie jamais directement le monde ni le code ; le moteur valide toute
action et les demandes d'évolution restent `pending` jusqu'à leur approbation.

## Monde et survie

- La carte mesure `40 × 24`, soit quatre fois la surface initiale, et forme un tore : franchir un bord ramène au bord opposé.
- La saison produit chaque jour une température, une pluviométrie et une condition déterministes. Le printemps régénère les plantes avec la pluie, l'automne favorise racines et champignons, la chaleur estivale augmente la soif et l'hiver raréfie progressivement les végétaux.
- Les jours de gel augmentent faim et fatigue ; un abri réduit fortement cette exposition. Une pluie importante fatigue légèrement les personnages non abrités.
- Des branches apparaissent sur les cases praticables au pied des arbres et se renouvellent progressivement. Un personnage peut en ramasser trois pour allumer un feu de camp persistant.
- En fin de journée et pendant la nuit, un personnage rejoint par pathfinding un feu qu'il a déjà observé, puis reste sur une case adjacente pour s'y reposer. Les urgences de faim et de soif restent prioritaires.
- Le premier feu devient le foyer collectif. Sa fumée en indique la direction sans révéler les cases intermédiaires ; chaque personnage doit trouver son chemin sur sa carte connue et dispose d'une place nocturne distincte autour du feu.
- Un personnage connaissant un feu peut porter une nourriture à la fois, la rapporter et la déposer dans sa réserve. Cette nourriture quitte réellement sa case d'origine et devient consommable par tout personnage présent près du même feu.
- La survie suit santé, faim, soif et fatigue. L'eau est recherchée puis consommée avec l'action locale `drink`.
- Les aliments sont les baies, racines, champignons, poissons et venaison.
- La faune comprend lapins, cerfs, sangliers, loups et poissons, avec danger, nutrition et habitat distincts.
- Chaque personnage possède force, agilité, endurance, robustesse, récupération, résistance aux maladies, concentration, volonté, mémoire et sens spatial.
- L'IA locale sélectionne un objectif par utilité, lui associe une destination persistante et utilise un BFS déterministe sur les seules cases connues. Explorer signifie rejoindre une frontière non cartographiée ; un déplacement sans cible n'est qu'un fallback.

## Personnages et projets

- Ada est bâtisseuse et prépare un abri, Borin est pourvoyeur et cherche à constituer des réserves, Cyra est exploratrice et veut produire une cartographie partageable.
- Chaque personnage possède une aspiration, des préférences, un projet durable avec progression et blocage, un niveau de monotonie et des relations évolutives.
- Les urgences vitales restent prioritaires. Hors urgence, l'IA locale arbitre entre projet, exploration et coopération sans appel API.
- Un projet bloqué enregistre une capacité manquante dans l'historique ; il ne crée jamais directement une demande à Dieu.
- Lors de la fenêtre IA, le bilan relie les actions au projet. La demande privilégie ce blocage concret et évite les simples réglages de navigation ou de seuils.

## Démarrage

Le lancement normal lit `.env` et utilise l'API :

```bash
./run.sh
```

Sur macOS, `./run.sh` compile puis ouvre l'interface graphique native. Le
terminal qui a lancé le jeu reste utilisé pour les appels IA et le suivi de
Dieu ; le choix des propositions se fait dans la fenêtre. Le rendu et la
validation terminal historiques restent disponibles :

```bash
./run.sh --terminal
```

`AUTOPOIESIS_UI=gui` et `AUTOPOIESIS_UI=terminal` configurent le mode par
défaut ; `--gui` et `--terminal` restent prioritaires. La fenêtre peut être
fermée à tout moment : le processus détecte cette fermeture et termine le run
proprement.

Sur macOS, la fenêtre graphique s'ouvre sans prendre le focus afin de ne pas
changer automatiquement d'espace de travail. `AUTOPOIESIS_FOCUS_WINDOW=1`
restaure l'activation immédiate de la fenêtre si ce comportement est préféré.

Les valeurs par défaut sont `SIMULATION_DAYS=100`, `CYCLES_PER_DAY=2400`,
`SIMULATION_DELAY_MS=500`, `SIMULATION_RENDER_EVERY_DAYS=1` et
`REPORT_EVERY_DAYS=3`. Les options de commande sont prioritaires :

```bash
./run.sh --days 3 --delay-ms 0 --render-every-days 1
```

Pour un run local sans réseau :

```bash
USE_API=0 ./run.sh --days 3 --delay-ms 0 --render-every-days 1
```

Le délai est appliqué entre deux journées, jamais entre deux cycles élémentaires.
`SIMULATION_DELAY_MS=0` supprime cette pause ; le rafraîchissement raylib reste
borné à 60 FPS. Les commandes de vitesse de la fenêtre agissent pendant la
journée et sont indépendantes du slider de délai entre deux journées. Le budget API est indépendant de l'horloge ; avec
`LIMIT_LLM_API_CALLS=100`, il s'arrête lorsque cette limite est atteinte.
`LLM_API_TIMEOUT_SECONDS=120` borne chaque appel sans ajouter de retry. En cas
d'échec, le terminal et `data/simulation.log` indiquent la durée, l'erreur cURL
ou le statut HTTP et les champs d'erreur API utiles, avec masquage des secrets.
`WAIT_FOR_HUMAN_VALIDATION=1` active la pause et l'interface de validation
intégrée à chaque fin de fenêtre ; `0` est réservé aux runs automatisés.

Avant chaque reprise graphique, un slider permet de remplacer ce délai pour la
suite du run entre `0 ms` et `10000 ms`, par pas de `100 ms`. Le choix reste en
mémoire pendant le processus courant sans réécrire `.env`.

La feuille de route des vingt prochains mécanismes jouables est tenue dans
[`TODO.md`](TODO.md). Chaque phase possède un critère de sortie observable et
reste traitée par incréments TDD.

Les personnages disposent d'un inventaire de transport borné par leur force. Bois, branches et ration utilisent la même capacité ; la charge courante est visible dans l'inspecteur graphique et toute collecte excédentaire est refusée par le moteur.

Le foyer stocke séparément nourriture, bois et branches. Les personnages rejoignent les ressources repérées puis rapportent leur charge par la carte qu'ils connaissent ; survoler le feu affiche le détail de la réserve.

Les rations vieillissent chaque jour. Près du feu, un personnage affamé peut cuisiner avant de manger : les aliments urgents sont préparés d'abord, la cuisson améliore leur nutrition et leur conservation, et les préférences propres à chacun orientent le choix dans la réserve.

Le foyer est aussi un lieu social. Les personnages y adoptent un rôle lié à leurs aptitudes, partagent des repas, veillent ensemble la nuit et marquent les réussites ou les morts. Ces activités sont limitées, persistantes et changent leurs relations plutôt que de produire un simple texte d'ambiance.

La fabrication suit un registre de recettes explicites. Le foyer peut transformer ses matières en manches, charbon et cordes ; une recette indisponible ne consomme rien, tandis qu'une réussite retire exactement ses coûts et stocke son produit.

Abattre un arbre exige désormais une hache. Le groupe doit trouver du minerai, le rapporter, produire charbon, lingot et manche, assembler la hache puis l'équiper. Chaque arbre use l'outil ; une réparation au foyer consomme du bois et sa durabilité est visible dans l'inspecteur.

Les pratiques réussies font progresser huit compétences locales : coupe du bois, extraction, fabrication, construction, cueillette, chasse, cuisine et sociabilité. Un niveau de coupe améliore la réparation des haches ; les trois spécialisations dominantes sont visibles dans l'inspecteur. Au foyer, un mentor ayant deux niveaux d'avance peut transmettre une unité d'expérience, au maximum une leçon donnée et reçue par journée.

Le foyer peut désormais réserver les matériaux d'un chantier puis placer sur la carte un mur, une porte, un lit, une réserve ou un atelier. Un bâtisseur équipé d'une hache rejoint une case voisine et fournit plusieurs unités de travail ; sa compétence accélère le chantier. Les murs bloquent les chemins, les portes restent praticables, les lits améliorent le repos et les ateliers renforcent les réparations.

L'écosystème progresse une fois par journée sans appel API. Les plantes repoussent jusqu'à leur capacité, mais une parcelle totalement vidée exige six jours avant sa première repousse. Lapins, cerfs, sangliers, loups et poissons se reproduisent à des rythmes distincts sans dépasser leur capacité ; un loup peut tuer un herbivore adjacent. Naissances, prédations, repousses et zones épuisées apparaissent dans les événements et l'en-tête graphique.

La santé conserve désormais les blessures de chasse, maladies et infections avec leur cause, sévérité, âge et état de traitement. Une plaie importante négligée trois jours s'infecte. Au même foyer, un compagnon peut traiter l'affection la plus grave ; le patient peut ensuite convalescer une fois par jour près du feu, dans un abri ou dans un lit. L'affection dominante est visible dans l'inspecteur.

Les bilans sont dans `data/ai_reports.jsonl` et les demandes dans
`data/feature_requests.jsonl`. Chaque événement structuré conserve aussi sa date
calendaire et son climat. Le jour absolu, le mois et l'année continuent entre
les fenêtres du run actif ; les deux phrases de mémoire restent disponibles
entre plusieurs lancements grâce au journal.

L'état exécutable du monde est sauvegardé atomiquement dans
`data/simulation-state.json` après chaque journée complète et avant toute
validation. Après une activation, l'ancienne version ne reprend pas la
simulation : elle recompile `autopoiesis_gui`, se remplace par le nouveau
binaire dans le même processus, puis recharge exactement le monde, les
personnages, le calendrier et les générateurs aléatoires. La fenêtre peut
clignoter brièvement lors du remplacement, mais conserve sa taille et sa
position. `./run.sh --new-world` efface volontairement ce checkpoint et démarre
un nouveau monde.

## Évolution contrôlée

1. Après la fenêtre de trois journées, la personnification produit un bilan puis une demande structurée.
2. Le moteur se met en pause et attend la confirmation humaine d'une seule validation à effectuer.
3. Le Validator contrôle la demande sélectionnée en lecture seule et recommande `approve`, `reject` ou `reformulate`.
4. L'approbation humaine explicite autorise Dieu à travailler sur cette seule demande.
5. Le même `./run.sh` démarre le daemon d'évolution en arrière-plan ; il lance Dieu automatiquement après l'approbation.
6. L'interface reste en attente et affiche les étapes, les derniers logs, le compte rendu de Dieu et le résultat du vérificateur.
7. Dieu applique le TDD dans un worktree isolé : test rouge, changement minimal, test vert.
8. En cas d'échec, Dieu reçoit le diagnostic et peut corriger jusqu'à `GOD_MAX_CORRECTIONS=2` fois.
9. Le vérificateur lance les tests, la compilation et le build Docker ; après succès, l'orchestrateur committe, pousse et active la modification.
10. L'interface affiche la recompilation, remplace le binaire actif et reprend au jour suivant depuis le checkpoint.

Les détails de compilation sont écrits dans `data/recompilation.log`. Si la
compilation ou le transfert échoue, la simulation ne reprend pas avec un
binaire partiel : le checkpoint reste disponible pour le prochain lancement.

Dieu ne tente pas d'accéder à la socket Docker du Mac depuis sa sandbox. Ce
n'est ni une erreur ni une vérification manquante : le build Docker appartient
exclusivement au vérificateur externe et reste obligatoire avant l'activation.

`VALIDATOR_MODE=human` attend la décision dans l'interface intégrée. `VALIDATOR_MODE=codex`
lance Codex en lecture seule, mais la garde d'approbation humaine reste active.
`VALIDATOR_MAX_REFORMULATIONS=3` limite les reformulations successives.
Au démarrage, le daemon mémorise la fin du journal des demandes et ne traite que
les propositions créées pendant le run courant. L'historique reste conservé sans
encombrer la file du Validator.

L'interface terminal est affichée directement par le binaire après chaque
fenêtre IA. Les hooks restent disponibles pour l'automatisation du Validator
et de Dieu, mais ne sont plus nécessaires pour valider une demande :

```bash
./scripts/validator-feature-hook.sh <feature-request-id>
./scripts/codex-feature-hook.sh <approved-feature-request-id>
./scripts/verify-evolution.sh <feature-request-id>
```

Chaque évolution est journalisée dans `data/evolution_runs/<id>/` et
`data/god-changelog.md`.

`EVOLUTION_AUTOSTART=1` active ce lancement intégré (valeur par défaut).
Sur macOS, `MACOS_NOTIFICATIONS=1` affiche une notification native lorsque
l'évolution est activée, échoue ou dépasse le délai d'attente. Le daemon hôte
reste responsable de cet affichage, y compris si Dieu continue après le timeout
de l'interface. `MACOS_NOTIFICATIONS=0` le désactive. macOS peut demander une
autorisation lors de la première notification.
`GOD_QUEUE_TIMEOUT_SECONDS=900` borne l'attente avant le démarrage de Dieu.
Une fois Dieu lancé, `GOD_WAIT_TIMEOUT_SECONDS=900` borne son workflow complet.
L'interface affiche un suivi toutes les 15 secondes et, en cas de dépassement ou
d'erreur, les dernières lignes utiles ainsi que le chemin de tous les artefacts.
Le daemon peut continuer en arrière-plan après un timeout.
Le runner cherche automatiquement Codex dans le `PATH` puis dans l'installation
VS Code ; `CODEX_BIN` permet de fournir un chemin absolu si nécessaire.
`GOD_MAX_CORRECTIONS=2` borne les retours de correction de Dieu après un échec de vérification.

## Configuration des modèles

```dotenv
OPENAI_MODEL=gpt-5.6-terra
CODEX_VALIDATOR_MODEL=gpt-5.6-sol
CODEX_VALIDATOR_REASONING_EFFORT=low
CODEX_GOD_MODEL=gpt-5.6-sol
CODEX_GOD_REASONING_EFFORT=low
```

`LLM_API_KEY` reste dans `.env` et n'est jamais écrit dans le code, les logs ou
l'image Docker.

## Vérifier

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
docker compose build
```

Le modèle de l'interface est testé sans fenêtre. Pour un contrôle visuel local,
`AUTOPOIESIS_GUI_SCREENSHOT=interface.png` enregistre le framebuffer du jeu
après le premier instantané, sans capturer les autres fenêtres du Mac.
`AUTOPOIESIS_GUI_ACTIVITY_SCREENSHOT=activity.png` permet de capturer l'écran
de transition des appels dans le scénario de prévisualisation dédié.

La suite inclut un scénario local de six journées vérifiant que les trois
personnages explorent, boivent, mangent, récupèrent et restent sous les seuils
critiques sans aucun appel API.

Une modification n'est livrée qu'après ces vérifications, un commit Git et un
push vers le dépôt distant. Cette étape est obligatoire pour que le jeu soit
récupérable et lançable depuis ton environnement.

Le vocabulaire et les invariants complets sont dans
[`normes/glossaire.md`](normes/glossaire.md). Le suivi de reprise est dans
[`TODO.md`](TODO.md).
