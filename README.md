# Autopoiesis

Autopoiesis est une expérience de simulation évolutive à long terme. Des personnages vivent dans un monde persistant, perçoivent une partie limitée de ce monde, prennent des décisions locales et accumulent une mémoire. Le projet cherche à explorer une séparation stricte entre un moteur local déterministe et une intelligence capable de proposer l'évolution de ses propres possibilités.

Le principe directeur est le suivant :

```text
État du monde
→ perception locale
→ décision du personnage
→ validation par le moteur
→ exécution déterministe
→ nouvel état
→ bilan et éventuelle demande d'évolution
```

Le moteur local reste reproductible à graine identique : règles physiques, déplacements, besoins, collisions, consommation des ressources et exécution des actions sont calculés localement. L'IA est le **décideur** et la **personnification** ; l'algorithme déterministe est le **moteur d'exécution**. Le décideur propose une intention parmi les actions autorisées, puis le moteur la valide et l'applique. Le décideur peut aussi formuler une demande à « Dieu », l'architecte contrôlé du monde, des mécanismes et des capacités.

## Protocole d'évolution piloté

Cette instance Codex construit le dispositif, mais n'en sera pas un composant opérationnel. Une fois installé sur le Mac, la chaîne doit fonctionner avec des processus indépendants : une instance validatrice Codex en lecture seule, une instance Dieu dans un worktree isolé, puis un vérificateur déterministe. Une demande de personnage ne déclenche jamais directement une modification.

La personnification décrit un besoin ; l'instance validatrice contrôle la demande selon le contrat et la politique configurée ; une demande autorisée est transmise à Dieu, l'orchestrateur d'évolution. Dieu travaille ensuite uniquement sur cette demande, dans un périmètre isolé et testable. L'instance architecte présente dans cette conversation ne valide pas les demandes et ne lance pas Dieu après l'installation du dispositif.

Le protocole complet est :

1. produire le bilan et la demande structurée à la fin du cycle ;
2. contrôler localement la demande et la conserver avec le statut `pending` ;
3. lancer l'instance validatrice Codex en lecture seule ;
4. faire passer une demande autorisée à `approved` selon la politique d'approbation ;
5. lancer l'instance Dieu dans un worktree isolé pour écrire le test échouant, puis l'implémentation minimale ;
6. exécuter le vérificateur déterministe : tests ciblés, suite complète et compilation Docker ;
7. vérifier que le besoin initial est satisfait et qu'aucun mécanisme non demandé n'a été ajouté ;
8. rédiger une critique du résultat et décider séparément de la prochaine évolution.

Le test exceptionnel précédent a utilisé cette instance comme validateur et Dieu. Ce fonctionnement était volontairement provisoire et ne constitue pas l'architecture cible. Le runtime cible repose sur deux instances Codex distinctes ; les autres demandes restent `pending`, et aucune capacité n'est activée sans vérification finale.

En mode `--no-api`, toute la boucle est locale et déterministe. En mode API, le moteur reste déterministe pour une décision donnée, mais les réponses de l'IA peuvent varier ; la reproductibilité complète nécessite donc de rejouer des décisions enregistrées ou d'utiliser un faux client.

Le MVP inclut besoins, personnalité, mémoire FIFO de dix éléments, baies limitées, sommeil, conversation adjacente, lapin visible, journaux JSONL et reprise sur erreur API. La direction produit prioritaire est l'évolution contrôlée : les personnages doivent formuler à chaque fin de cycle des demandes d'évolution que Dieu transformera progressivement en mécanismes testés. Le palier de départ vise la survie simple par baies et chasse ; si une capacité attendue n'existe pas encore dans le moteur, elle doit être ajoutée par une demande validée puis par TDD.

## Construire et lancer

La commande la plus simple (mode local, sans clé API) est :

```bash
./run.sh
```

Elle accepte les options du programme, par exemple `./run.sh --cycles 20 --seed 42 --delay-ms 100`.
Pour utiliser OpenAI ou un serveur compatible, renseignez `LLM_API_KEY` et `OPENAI_MODEL` dans `.env`, puis lancez `USE_API=1 ./run.sh --cycles 20`.

Chaque tentative HTTP consomme une unité du budget `LIMIT_LLM_API_CALLS` (100 par défaut), y compris les retries et les bilans IA de fin de cycle. Les actions quotidiennes locales ne consomment aucun appel API. Le compteur est conservé dans `/data/api_budget.json` et verrouillé entre processus. Pour reprendre le même budget après un redémarrage, utilisez le même `AUTOPOIESIS_RUN_ID` ; sinon un nouvel identifiant de run est créé.

La commande manuelle équivalente reste :

```bash
docker compose build
docker compose run --rm autopoiesis --no-api --cycles 100 --seed 42
```

Avec l'API, copiez `.env.example` vers votre configuration d'environnement, renseignez `LLM_API_KEY` et `OPENAI_MODEL`, puis lancez la même commande sans `--no-api`. La clé n'est jamais écrite dans le dépôt ou les journaux. `OPENAI_BASE_URL` est facultative et peut viser un serveur compatible.

Options : `--cycles`, `--seed`, `--delay-ms`, `--render-every`, `--no-api`.

## Tests et journaux

```bash
docker run --rm --entrypoint /usr/local/bin/autopoiesis_tests autopoiesis
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

Les fichiers `data/simulation.log` et `data/events.jsonl` sont montés dans `/data`. Arrêtez une exécution avec Ctrl-C. Les appels API consomment des tokens et peuvent être facturés ; utilisez `--no-api` pour les essais gratuits et reproductibles.

## Architecture conceptuelle et évolution

Un cycle représente une journée de simulation. Chaque personnage dispose de 240 créneaux d'action par journée. Les besoins évoluent par petites touches au fil de ces créneaux, afin d'éviter les changements brusques : la faim augmente toutes les 80 actions et la fatigue toutes les 12 actions, ce qui rend le sommeil pertinent vers la fin de la journée. Le décideur local est utilisé pour ces actions quotidiennes, y compris lorsque le mode API est activé.

Chaque personnage possède également une mémoire spatiale de la carte. Les cases observées sont conservées dans sa perception sous `known_map` et restent disponibles aux itérations suivantes, même lorsqu'elles sont hors de son rayon actuel. Cette mémoire contient uniquement les cases effectivement explorées par ce personnage.

Les buissons contiennent huit portions de baies et sont régénérés uniquement lorsqu'ils sont configurés au démarrage. Lorsqu'un personnage atteint une faim critique (90), il bénéficie de trois cycles complets avant que sa santé ne commence à diminuer. La simulation s'arrête automatiquement dès que les trois personnages sont morts.

Le projet est conçu en trois couches :

- **Noyau déterministe local / moteur d'exécution** : monde, règles, besoins, mémoire, validation et exécution. Il doit produire le même résultat avec la même graine et les mêmes entrées.
- **Personnification et observateur IA** : à chaque fin de cycle, l'IA reçoit l'historique pertinent d'un personnage, rédige son bilan à la première personne et peut exprimer un besoin ou une demande d'évolution.
- **Boucle d'évolution contrôlée** : une demande contrôlée par l'instance validatrice autorise Dieu à préparer une modification des actions disponibles, des algorithmes d'exécution, des règles du monde ou du contenu. Cette évolution doit être testée, vérifiée, critiquée et intégrée comme une nouvelle version du moteur.

Dans le MVP actuel, seule la deuxième couche est connectée à l'API. Le code du moteur, ses actions et son monde restent fixes pendant une simulation. L'API n'a ni accès au dépôt, ni outil d'écriture, ni permission d'exécuter du code ; elle ne peut donc pas appliquer une évolution directement.

Dieu est destiné à devenir un architecte contrôlé : il analyse les demandes des personnages, propose le prochain mécanisme cohérent, puis laisse un workflow de développement l'ajouter au moteur. La complexité doit croître par paliers testables : baies et chasse, puis outils, bois, haches, minerai, fabrication, construction, nouveaux besoins, nouvelles contraintes et nouvelles capacités. Chaque mécanisme doit préciser ses ressources, préconditions, effets, validations locales et tests d'acceptation.

Le développement se fait en TDD. Une évolution acceptée commence par un test qui échoue, puis une implémentation minimale du moteur, puis la validation des tests et de la compilation Docker. Une demande non testable doit être reformulée avant d'être intégrée.

Le flux prévu pour l'évolution est :

1. exécuter 240 actions locales par personnage pendant une journée ;
2. à chaque fin de cycle, produire un bilan JSON pour chaque personnage ;
3. demander à l'IA un objet `feature_request` séparé, décrivant un besoin, un obstacle, un mécanisme et des critères de test ;
4. enregistrer cette demande dans `/data/feature_requests.jsonl` avec le statut `pending` ;
5. faire contrôler une seule demande par l'instance validatrice ;
6. transmettre la demande autorisée à l'instance Dieu, qui écrit d'abord le test échouant puis la modification minimale ;
7. compiler, tester, vérifier le comportement demandé et relire la modification ;
8. rédiger la critique avant toute nouvelle demande ou extension de périmètre.

Une mise à jour automatique du code nécessitera un agent séparé, un dépôt de travail isolé, des permissions explicites, une revue humaine et une pipeline de tests. Cette capacité est une direction du projet, pas une permission accordée au personnage pendant une simulation.

Lorsqu'un agent signale `blocked`, le moteur ajoute une demande dans `data/feature_requests.jsonl` et l'affiche dans le terminal. Après validation humaine :

```bash
./scripts/approve-feature.sh cycle-12-a1-3
./scripts/codex-feature-hook.sh
```

La seconde commande constitue le point d'entrée d'un hook Codex local : elle ne lit que `data/approved_feature_requests.jsonl` et lance Dieu via `codex exec` dans un worktree isolé. Le vérificateur déterministe reste indépendant de Dieu.

Les lanceurs séparés sont désormais :

```bash
./scripts/validator-feature-hook.sh <feature-request-id>
./scripts/codex-feature-hook.sh <approved-feature-request-id>
./scripts/verify-evolution.sh <feature-request-id>
```

Le premier lance une instance Codex en lecture seule. Le second lance Dieu avec `codex exec` dans `worktrees/god-<id>`, sans fusionner ses changements. Les deux utilisent par défaut `gpt-5.6-sol` avec `model_reasoning_effort=low`, réglages surchargeables par `CODEX_MODEL` et `CODEX_REASONING_EFFORT`. Le troisième compile et teste ce worktree, puis écrit un rapport `verification.json`. Aucun de ces lanceurs n'active automatiquement la modification.

Pour un fonctionnement périodique sur macOS, `scripts/evolution-daemon.sh` traite une demande à la fois. Le modèle de tâche `scripts/launchd/com.autopoiesis.evolution.plist.example` peut être copié dans `~/Library/LaunchAgents` après remplacement des deux chemins absolus. Le daemon valide automatiquement les nouvelles demandes, mais ne franchit pas la garde `pending → approved` sans autorisation conforme à la politique active.

## Choix et limites connues

La perception utilise une distance Manhattan de trois cases et ne contient jamais la carte complète. Le sommeil est une action de deux cycles. Les paramètres de besoins sont volontairement simples et pourront être calibrés après observation du MVP. Le modèle de capacités ajoutables reste à concevoir pour permettre à Dieu d'introduire proprement de nouveaux mécanismes sans casser la reproductibilité du moteur. L'activation entièrement autonome reste à décider : la règle actuelle conserve une validation humaine explicite avant activation.
