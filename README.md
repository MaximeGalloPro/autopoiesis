# Autopoiesis

Autopoiesis simule des personnages dans un monde déterministe. Le moteur local
fait avancer leurs besoins et leurs actions ; l'IA intervient seulement pour
faire le bilan d'une période et proposer une évolution contrôlée.

```text
actions locales → 3 journées → bilan IA → demande IA à Dieu
                                  (2 appels par personnage)
                                  → Validator → approbation → Dieu → tests
```

## Horloge et appels API

- Un **cycle élémentaire** est un créneau d'action pour chaque personnage.
- Une **journée** contient `CYCLES_PER_DAY=240` cycles élémentaires.
- La fenêtre IA par défaut est `REPORT_EVERY_DAYS=3`, donc `720` cycles élémentaires.
- À la fin de cette fenêtre, chaque personnage déclenche exactement deux appels, dans cet ordre : bilan, puis demande d'évolution.
- Les bilans et tous les champs des demandes d'évolution sont produits en français dans ces mêmes appels.
- Avec trois personnages, cela représente six appels API au cycle élémentaire `720`.
- Aucun appel API n'est lancé entre les cycles élémentaires `1` et `719`, et aucun retry HTTP n'est effectué.
- Le moteur s'arrête à la fin de chaque fenêtre IA et attend une confirmation humaine avant de poursuivre. `o` reprend, `q` arrête le run.
- Le terminal affiche l'avancement des six appels (`en cours`, `terminé` ou `indisponible`) avant d'ouvrir cette pause.
- La validation est intégrée au même terminal et ne présente que les trois propositions les plus récentes de la fenêtre : choisir `1`, `2`, `3` ou `n` pour aucune, puis `a` approuve ou `r` refuse la proposition sélectionnée. `o` reprend et `q` ou `exit` arrête proprement. Les autres demandes restent `pending`.

Les décisions quotidiennes utilisent actuellement le décideur local. Une réponse
IA ne modifie jamais directement le monde ni le code ; le moteur valide toute
action et les demandes d'évolution restent `pending` jusqu'à leur approbation.

## Monde et survie

- La carte mesure `40 × 24`, soit quatre fois la surface initiale, et forme un tore : franchir un bord ramène au bord opposé.
- La survie suit santé, faim, soif et fatigue. L'eau est recherchée puis consommée avec l'action locale `drink`.
- Les aliments sont les baies, racines, champignons, poissons et venaison.
- La faune comprend lapins, cerfs, sangliers, loups et poissons, avec danger, nutrition et habitat distincts.
- Chaque personnage possède force, agilité, endurance, robustesse, récupération, résistance aux maladies, concentration, volonté, mémoire et sens spatial.
- L'IA locale sélectionne un objectif par utilité, le conserve brièvement pour éviter les oscillations et utilise un BFS déterministe sur les seules cases connues.

## Démarrage

Le lancement normal lit `.env` et utilise l'API :

```bash
./run.sh
```

Les valeurs par défaut sont `SIMULATION_DAYS=100`, `CYCLES_PER_DAY=240`,
`SIMULATION_DELAY_MS=500`, `SIMULATION_RENDER_EVERY_DAYS=1` et
`REPORT_EVERY_DAYS=3`. Les options de commande sont prioritaires :

```bash
./run.sh --days 3 --delay-ms 0 --render-every-days 1
```

Pour un run local sans réseau :

```bash
USE_API=0 ./run.sh --days 3 --delay-ms 0 --render-every-days 1
```

Le délai est appliqué entre deux journées. `SIMULATION_DELAY_MS=0` lance le
moteur à pleine vitesse. Le budget API est indépendant de l'horloge ; avec
`LIMIT_LLM_API_CALLS=100`, il s'arrête lorsque cette limite est atteinte.
`WAIT_FOR_HUMAN_VALIDATION=1` active la pause et l'interface de validation
intégrée à chaque fin de fenêtre ; `0` est réservé aux runs automatisés.

Les bilans sont dans `data/ai_reports.jsonl` et les demandes dans
`data/feature_requests.jsonl`.

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

La suite inclut un scénario local de six journées vérifiant que les trois
personnages explorent, boivent, mangent, récupèrent et restent sous les seuils
critiques sans aucun appel API.

Une modification n'est livrée qu'après ces vérifications, un commit Git et un
push vers le dépôt distant. Cette étape est obligatoire pour que le jeu soit
récupérable et lançable depuis ton environnement.

Le vocabulaire et les invariants complets sont dans
[`normes/glossaire.md`](normes/glossaire.md). Le suivi de reprise est dans
[`TODO.md`](TODO.md).
