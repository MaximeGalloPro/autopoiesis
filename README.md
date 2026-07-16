# Autopoiesis

Autopoiesis est un prototype C++20 de simulation déterministe : trois individus perçoivent une petite portion d'un monde ASCII, choisissent une action autorisée (localement ou via l'API Responses d'OpenAI), puis le moteur valide et exécute cette action.

Le MVP inclut besoins, personnalité, mémoire FIFO de dix éléments, baies limitées, sommeil, conversation adjacente, lapin visible mais impossible à attraper, journaux JSONL et reprise sur erreur API. Il n'inclut ni chasse, combat, inventaire, craft, apprentissage ou modification de code.

## Construire et lancer

La commande la plus simple (mode local, sans clé API) est :

```bash
./run.sh
```

Elle accepte les options du programme, par exemple `./run.sh --cycles 20 --seed 42 --delay-ms 100`.
Pour utiliser OpenAI ou un serveur compatible, renseignez `LLM_API_KEY` et `OPENAI_MODEL` dans `.env`, puis lancez `USE_API=1 ./run.sh --cycles 20`.

Chaque tentative HTTP consomme une unité du budget `LIMIT_LLM_API_CALLS` (100 par défaut), y compris les retries et les bilans de fin de cycle. Le compteur est conservé dans `/data/api_budget.json` et verrouillé entre processus. Pour reprendre le même budget après un redémarrage, utilisez le même `AUTOPOIESIS_RUN_ID` ; sinon un nouvel identifiant de run est créé.

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

## Cycles, bilans et évolution du moteur

Actuellement, un cycle correspond à un tour de décision des trois individus : chacun effectue au plus une action, puis le monde est rendu et journalisé. Il ne contient donc pas encore 240 actions. Pour passer à 240 actions, il faut définir si ce nombre représente 240 actions globales ou 240 actions par individu, puis ajouter une notion de sous-tour et un bilan à la fin du cycle. Ce changement modifierait le rythme de la simulation et le coût API ; il vaut mieux le faire après mesure du MVP.

Chaque personnage possède également une mémoire spatiale de la carte. Les cases observées sont conservées dans sa perception sous `known_map` et restent disponibles aux itérations suivantes, même lorsqu'elles sont hors de son rayon actuel. Cette mémoire contient uniquement les cases effectivement explorées par ce personnage.

Les buissons contiennent huit portions de baies et sont régénérés uniquement lorsqu'ils sont configurés au démarrage. Lorsqu'un personnage atteint une faim critique (90), il bénéficie de trois cycles complets avant que sa santé ne commence à diminuer. La simulation s'arrête automatiquement dès que les trois personnages sont morts.

Le moteur actuel ne peut pas être mis à jour par l'API, volontairement. L'API peut uniquement retourner une décision structurée (`action` ou `blocked`) ; elle ne reçoit ni accès au dépôt, ni outil d'écriture, ni permission d'exécuter du code. Le moteur valide cette décision et reste le seul composant qui modifie l'état du monde.

Une évolution sûre serait :

1. produire un bilan JSON en fin de cycle ;
2. demander à l'IA un objet `feature_request` séparé, décrivant un besoin, une justification et des critères de test ;
3. enregistrer cette demande dans `/data/feature_requests.jsonl` ;
4. faire relire et implémenter la modification par un développeur ;
5. compiler et exécuter les tests avant de déployer une nouvelle image Docker.

Une mise à jour automatique du code nécessiterait un agent séparé, un dépôt de travail isolé, des permissions explicites, une revue humaine et une pipeline de tests. Ce n'est pas une extension à activer directement dans le MVP.

Lorsqu'un agent signale `blocked`, le moteur ajoute une demande dans `data/feature_requests.jsonl` et l'affiche dans le terminal. Après validation humaine :

```bash
./scripts/approve-feature.sh cycle-12-a1-3
./scripts/codex-feature-hook.sh
```

La seconde commande constitue le point d'entrée d'un hook Codex local : elle ne lit que `data/approved_feature_requests.jsonl` et ne modifie aucun fichier. Elle peut être branchée à votre mécanisme de hook Codex ou exécutée avant une session Codex. L'approbation reste une action explicite dans le terminal.

## Choix et limites connues

La perception utilise une distance Manhattan de trois cases et ne contient jamais la carte complète. Le sommeil est une action de deux cycles. Les paramètres de besoins sont volontairement simples et pourront être calibrés après observation du MVP.
