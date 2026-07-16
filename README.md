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

Le moteur local reste reproductible à graine identique : règles physiques, déplacements, besoins, collisions, consommation des ressources et exécution des actions sont calculés localement. L'IA est le **décideur** et la **personnification** ; l'algorithme déterministe est le **moteur d'exécution**. Le décideur propose une intention parmi les actions autorisées, puis le moteur la valide et l'applique. Le décideur peut aussi formuler une demande à « Dieu », le générateur futur du monde et des capacités.

En mode `--no-api`, toute la boucle est locale et déterministe. En mode API, le moteur reste déterministe pour une décision donnée, mais les réponses de l'IA peuvent varier ; la reproductibilité complète nécessite donc de rejouer des décisions enregistrées ou d'utiliser un faux client.

Le MVP inclut besoins, personnalité, mémoire FIFO de dix éléments, baies limitées, sommeil, conversation adjacente, lapin visible mais impossible à attraper, journaux JSONL et reprise sur erreur API. Il n'inclut ni chasse, combat, inventaire, craft, apprentissage ou modification de code.

## Construire et lancer

La commande la plus simple (mode local, sans clé API) est :

```bash
./run.sh
```

Elle accepte les options du programme, par exemple `./run.sh --cycles 20 --seed 42 --delay-ms 100`.
Pour utiliser OpenAI ou un serveur compatible, renseignez `LLM_API_KEY` et `OPENAI_MODEL` dans `.env`, puis lancez `USE_API=1 ./run.sh --cycles 20`.

Chaque tentative HTTP consomme une unité du budget `LIMIT_LLM_API_CALLS` (100 par défaut), y compris les retries et les bilans IA tous les cinq cycles. Les actions quotidiennes locales ne consomment aucun appel API. Le compteur est conservé dans `/data/api_budget.json` et verrouillé entre processus. Pour reprendre le même budget après un redémarrage, utilisez le même `AUTOPOIESIS_RUN_ID` ; sinon un nouvel identifiant de run est créé.

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
- **Personnification et observateur IA** : tous les cinq cycles, l'IA reçoit l'historique pertinent d'un personnage, rédige son bilan à la première personne et peut exprimer un besoin ou une demande d'évolution.
- **Boucle d'évolution contrôlée** : une demande validée par l'humain pourra, à terme, modifier les actions disponibles, les algorithmes d'exécution, les règles du monde ou son contenu. Cette évolution devra être testée, revue et intégrée comme une nouvelle version du moteur.

Dans le MVP actuel, seule la deuxième couche est connectée à l'API. Le code du moteur, ses actions et son monde restent fixes. L'API n'a ni accès au dépôt, ni outil d'écriture, ni permission d'exécuter du code ; elle ne peut donc pas encore appliquer une évolution.

Le flux prévu pour l'évolution est :

1. exécuter 240 actions locales par personnage pendant une journée ;
2. tous les cinq cycles, produire un bilan JSON pour chaque personnage ;
3. demander à l'IA un objet `feature_request` séparé, décrivant un besoin, une justification et des critères de test ;
4. enregistrer cette demande dans `/data/feature_requests.jsonl` ;
5. faire valider la proposition dans le terminal ;
6. laisser Codex préparer une modification isolée du moteur ;
7. compiler, tester et relire la modification avant de déployer une nouvelle image Docker.

Une mise à jour automatique du code nécessitera un agent séparé, un dépôt de travail isolé, des permissions explicites, une revue humaine et une pipeline de tests. Cette capacité est une direction du projet, pas une permission accordée au personnage pendant une simulation.

Lorsqu'un agent signale `blocked`, le moteur ajoute une demande dans `data/feature_requests.jsonl` et l'affiche dans le terminal. Après validation humaine :

```bash
./scripts/approve-feature.sh cycle-12-a1-3
./scripts/codex-feature-hook.sh
```

La seconde commande constitue le point d'entrée d'un hook Codex local : elle ne lit que `data/approved_feature_requests.jsonl` et ne modifie aucun fichier. Elle peut être branchée à votre mécanisme de hook Codex ou exécutée avant une session Codex. L'approbation reste une action explicite dans le terminal.

## Choix et limites connues

La perception utilise une distance Manhattan de trois cases et ne contient jamais la carte complète. Le sommeil est une action de deux cycles. Les paramètres de besoins sont volontairement simples et pourront être calibrés après observation du MVP.
