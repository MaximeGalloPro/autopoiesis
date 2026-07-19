# Autopoiesis

Autopoiesis est désormais un observatoire web. React présente l’interface,
Three.js met le monde en scène et Elysia sert le client tout en relayant un
backend C++ déterministe. Le navigateur n’est jamais l’autorité du monde : il
affiche des instantanés en lecture seule et envoie uniquement des commandes
bornées que le moteur valide avant de les appliquer.

```text
React + Three.js ── HTTP commandes ──▶ Elysia ── JSONL v1/stdin ──▶ C++
       ▲                              │                           │
       └──────── WebSocket ───────────┴── événements/stdout ◀────┘
```

Cette frontière conserve les invariants du projet : le décideur IA propose,
le moteur décide de ce qui est exécutable et toute évolution reste `pending`
jusqu’à une validation humaine explicite.

## L’observatoire

L’interface web conserve les fonctions de l’ancienne présentation native et
ajoute une expérience responsive :

- monde torique `40 × 24` en 3D, cycle jour/nuit, saisons, climat et ressources ;
- sélection des personnages et animaux, inspecteur des besoins, attributs,
  humeur, projet, inventaire, relations et actions disponibles ;
- feu de camp, abris, nourriture portée et réserve commune visibles ;
- pause, vitesses `0,25×`, `0,5×`, `1×`, `2×`, `4×` et délai inter-journées de
  `0` à `10000 ms` ;
- progression en direct des bilans IA, demandes d’évolution, workflow de Dieu,
  vérification et recompilation ;
- garde humaine intégrée sous forme de rappel compact, avec panneau réductible
  et cartes de validation pour les personnages comme pour le Diable ;
- reconnexion WebSocket sans rejouer un cycle ni une commande ;
- raccourcis clavier : espace, touches `1` à `5`, `[` et `]`, puis `?` pour
  l’aide.

À `2×` et `4×`, tous les cycles et toutes les décisions restent exécutés. Seule
la fréquence des instantanés transportés peut être réduite. Fermer un onglet ne
modifie pas le monde ; l’arrêt est une commande explicite validée par le C++.
Elysia coalesce également les rendus destinés à un navigateur lent en gardant
le dernier instantané, tout en livrant immédiatement les gardes et erreurs. La
cadence maximale se règle avec `AUTOPOIESIS_WEB_SNAPSHOT_MS` (`100 ms` par
défaut, `500 ms` dans la preview Nuagent sans accélération graphique).

## Horloge et appels IA

Un cycle élémentaire représente une action locale pour chaque personnage. La
durée d’une journée vient de `CYCLES_PER_DAY` et la fenêtre IA de
`REPORT_EVERY_DAYS` journées. La configuration de développement historique
utilise `2400` cycles par journée ; la preview Nuagent emploie `240` cycles afin
de rendre observable la garde de trois journées au cycle `720`.

À la fin d’une fenêtre, chaque personnage déclenche strictement deux appels,
dans cet ordre : bilan puis demande d’évolution. Avec Ada, Borin et Cyra, cela
fait six appels séquentiels. Le moteur écrit ensuite son checkpoint et attend
la validation humaine avant de reprendre. Aucun rendu web ne crée d’appel ou
ne change cet ordre.

La simulation conserve le calendrier, les mémoires bornées, les projets, la
survie, la carte connue, les relations et les générateurs pseudo-aléatoires
dans `data/simulation-state.json`. Les bilans sont journalisés dans
`data/ai_reports.jsonl` et les demandes dans `data/feature_requests.jsonl`.

## Démarrage normal

Prérequis : Docker avec Compose, CMake, un compilateur C++20 et Bun 1.3 ou plus
récent.

```bash
cp .env.example .env
./run.sh
```

Le lanceur :

1. vérifie le build Docker obligatoire ;
2. compile `autopoiesis_backend` ;
3. installe, type-checke et construit le client web ;
4. démarre le daemon d’évolution si `EVOLUTION_AUTOSTART=1` ;
5. sert l’observatoire sur `http://localhost:3000` par défaut.

Le port se règle avec `PORT`. Les options du moteur traversent le serveur sans
interprétation :

```bash
./run.sh --days 3 --delay-ms 0 --new-world
```

Pour exécuter le monde sans accès réseau :

```bash
USE_API=0 ./run.sh --days 3
```

Le rendu terminal demeure disponible pour les diagnostics et l’automatisation :

```bash
./run.sh --terminal --days 3
```

`AUTOPOIESIS_UI=web` est la valeur normale ; `terminal` sélectionne le fallback.
L’ancien nom `gui` est accepté comme alias de transition, mais ouvre le web et
ne construit plus aucune application raylib.

L’image complète peut aussi être lancée directement :

```bash
docker compose up --build
```

Le `./run.sh` hôte reste le chemin recommandé lorsqu’une évolution doit être
approuvée, vérifiée, compilée puis activée dans la même session.

## Nuagent Preview

[`nuagent.json`](nuagent.json) et
[`scripts/setup-nuagent.sh`](scripts/setup-nuagent.sh) préparent automatiquement
le backend et le bundle web. La preview utilise des données isolées dans
`.context/preview-data`, désactive les appels LLM et le daemon d’évolution, et
active `AUTOPOIESIS_SAFE_PREVIEW=1`. Ce dernier verrou refuse avec HTTP 403
toute approbation réelle ; la pause, les vitesses, le délai, la reprise et
l’arrêt restent testables contre le vrai moteur C++.

## Contrat Elysia ↔ C++

Le backend écrit une ligne par événement, préfixée par
`AUTOPOIESIS_EVENT `, avec une enveloppe JSON `{version: 1, type, payload}`.
Les types autorisés sont `status`, `snapshot`, `activity`,
`validation_prompt`, `evolution_progress`, `evolution_completion` et
`recompilation`. Les diagnostics ordinaires vont sur stderr et ne peuvent pas
être confondus avec l’état autoritaire.

Elysia expose :

- `GET /bridge/health` : disponibilité du processus C++ ;
- `GET /bridge/state` : dernier état projeté en lecture seule ;
- `POST /bridge/commands` : commandes strictement validées ;
- `GET /ws` : instantané initial puis événements descendants.

Le navigateur emploie `/bridge/*` afin de rester sur le port web dans les
previews Nuagent, où `/api/*` est réservé au port API dédié. Les trois routes
restent aussi exposées sous `/api/*` pour la compatibilité locale et Docker.

Les seules commandes filaires acceptées par le C++ sont pause/reprise/arrêt,
vitesse, délai et texte de validation autorisé pour la garde courante. Les
identifiants de carte sont résolus par Elysia vers les choix numériques de la
garde, puis `HumanValidation` les valide de nouveau. Il n’existe aucun endpoint
d’écriture arbitraire dans `data/`.

## Évolution contrôlée

Une fenêtre ne traite qu’une demande. Le navigateur permet de sélectionner,
examiner, approuver, refuser ou revenir ; il ne change aucun statut lui-même.
Après approbation, le daemon hôte lance le Validator puis Dieu dans un worktree
isolé. Le vérificateur externe possède seul la responsabilité du build Docker.
Une version n’est activée qu’après compilation, tests, vérification, commit et
push réussis. Le backend recharge alors le checkpoint au premier cycle de la
journée suivante.

Les variables principales sont documentées dans [`.env.example`](.env.example).
`LLM_API_KEY` reste exclusivement dans `.env` ou l’environnement et n’est
jamais envoyé au navigateur, écrit dans les logs ou incorporé dans une image.

## Développement et vérification

Backend C++ :

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Frontend et passerelle :

```bash
bun install --cwd web --frozen-lockfile
bun run --cwd web typecheck
bun test --cwd web
bun run --cwd web build
```

Vérification de livraison :

```bash
docker compose build
```

Le build Docker exécute lui-même la compilation et les tests C++, puis le
type-check, les tests et le build web. Une modification n’est livrée qu’après
ces gardes, un commit Git et un push de la branche.

Le vocabulaire et les invariants complets vivent dans
[`normes/glossaire.md`](normes/glossaire.md). La feuille de route gameplay est
dans [`TODO.md`](TODO.md).
