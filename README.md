# Autopoiesis

Autopoiesis simule des personnages dans un monde déterministe. Ils perçoivent leur environnement, agissent selon leurs besoins, puis peuvent formuler des demandes d'évolution à la fin d'une période.

Le principe est strict :

```text
perception → décision IA → validation locale → exécution déterministe
                                      ↓
                         demande d'évolution pending
                                      ↓
                   Validator → approbation humaine → Dieu → tests
```

L'IA propose. Le moteur décide de ce qui est possible. Dieu ne travaille que sur une demande approuvée et vérifiée.

## Démarrage

Sans API, pour un essai reproductible :

```bash
./run.sh --no-api --cycles 3 --delay-ms 0
```

Avec l'API, renseigner `.env`, puis lancer :

```bash
./run.sh --cycles 3 --delay-ms 0 --render-every 0
```

Avec `REPORT_EVERY_CYCLES=3`, les bilans IA sont produits tous les trois cycles. Avec `FEATURE_REQUESTS_REQUIRED=1`, chaque bilan doit contenir une demande d'évolution structurée. `SIMULATION_DELAY_MS=500` définit le délai par défaut entre deux cycles ; `0` lance la simulation à pleine vitesse.

Les demandes apparaissent dans `data/feature_requests.jsonl`. Les bilans sont conservés dans `data/ai_reports.jsonl`.

Depuis un terminal interactif, `run.sh` ouvre automatiquement l'interface de validation lorsqu'une demande attend une décision. En mode automatisé ou sans demande, il se termine sans attendre une saisie.

Une option donnée au lancement est prioritaire sur `.env`, par exemple `./run.sh --delay-ms 0` ne modifie pas la configuration persistante.

## Évolution contrôlée

1. L'IA formule une demande `pending`.
2. Le Validator vérifie son contrat, son périmètre et sa testabilité en lecture seule.
3. L'utilisateur approuve ou refuse explicitement la demande.
4. Dieu travaille dans `worktrees/god-<id>` et applique le TDD : test rouge, changement minimal, test vert.
5. Le vérificateur exécute CMake, les tests et le build Docker.
6. Le résultat est conservé dans `data/evolution_runs/<id>/` et `data/god-changelog.md`.

Le Validator ne modifie ni le code ni le statut de la demande. Dieu ne fusionne rien et aucune capacité n'est active avant la revue finale.

### Choisir le Validator

Dans `.env` :

```dotenv
VALIDATOR_MODE=human
```

`human` laisse la demande en attente et attend :

```bash
./scripts/approve-feature.sh <feature-request-id>
```

`codex` lance une instance Codex en lecture seule. Elle écrit une recommandation dans `data/evolution_runs/<id>/validation-record.json`, puis attend toujours l'approbation humaine.

L'interface affiche la demande et l'avis du Validator. Les choix sont `a` pour approuver, `r` pour refuser, `d` pour afficher le JSON complet et `q` pour quitter. Elle appelle les scripts d'approbation existants ; elle ne contient pas de logique métier.

Après une approbation, elle reste ouverte : elle suit le démarrage de Dieu, son travail dans le worktree, la vérification CMake/tests/Docker, affiche le bilan puis demande si une autre évolution doit être lancée.

Avec `VALIDATOR_MAX_REFORMULATIONS=3`, une recommandation `reformulate` peut créer jusqu'à trois nouvelles versions liées de la demande. Au-delà, elle est classée `rejected`.

Le daemon macOS `launchd` exécute cette chaîne périodiquement avec :

```bash
./scripts/evolution-daemon.sh
```

Les commandes manuelles sont :

```bash
./scripts/validator-feature-hook.sh <feature-request-id>
./scripts/codex-feature-hook.sh <approved-feature-request-id>
./scripts/verify-evolution.sh <feature-request-id>
```

## Configuration

Les modèles sont séparés par rôle :

```dotenv
OPENAI_MODEL=gpt-5.6-terra
CODEX_VALIDATOR_MODEL=gpt-5.6-sol
CODEX_VALIDATOR_REASONING_EFFORT=low
CODEX_GOD_MODEL=gpt-5.6-sol
CODEX_GOD_REASONING_EFFORT=low
```

`LLM_API_KEY` reste uniquement dans `.env`. Il n'est jamais écrit dans le code, les journaux ou l'image Docker.

## État du MVP

Le moteur contient actuellement les besoins, les baies, le sommeil, la conversation adjacente et la chasse au lapin. Les personnages utilisent encore le décideur local pour leurs actions quotidiennes ; l'API intervient dans les bilans et les demandes d'évolution.

La croissance prévue reste incrémentale : couper des arbres, fabriquer une hache, extraire du fer, fabriquer des outils, construire, puis ajouter de nouveaux besoins. Chaque mécanisme doit rester déterministe, localement validé et livré avec des tests.

## Vérifier

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
docker compose build
```

Les règles d'architecture et le protocole complet sont dans [`normes/glossaire.md`](normes/glossaire.md). Les secrets et les données d'exécution sont exclus de Git.
