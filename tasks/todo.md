# Plan d'implémentation — MVP Autopoiesis

- [x] Initialiser le projet C++20/CMake et son squelette compilable.
- [x] Implémenter le monde, les individus, les besoins, les actions et la boucle déterministe.
- [x] Ajouter le décideur local, la perception limitée, la mémoire, les journaux et le rendu terminal.
- [x] Ajouter le client OpenAI Responses, le JSON Schema strict, la validation locale et le repli sur `wait`.
- [x] Couvrir le moteur, la validation et le mode `--no-api` par des tests sans réseau.
- [x] Ajouter Docker, Compose, l'exemple d'environnement et la documentation.
- [x] Compiler et exécuter les tests et simulations natives/Docker, puis corriger les erreurs.

## Revue

Compilation Docker GCC 12/CMake 3.25 réussie. Tests `autopoiesis_tests` réussis. Simulation Docker `--no-api --cycles 2` réussie après correction de la durée de vie du RNG local. Une exécution de trois cycles a produit 15 événements JSONL et 15 lignes lisibles dans `data/`.

## Extension demandée

- [x] Ajouter `run.sh` pour lancer simplement le mode local ou, avec `USE_API=1`, le mode API.
- [x] Documenter le sens actuel d'un cycle et la stratégie sûre de demandes de fonctionnalités.
- [x] Ajouter les demandes `blocked`, l'approbation terminale et le point d'entrée de hook Codex.
- [x] Ajouter une mémoire spatiale persistante par personnage (`known_map`).
- [x] Augmenter les baies, ajouter trois cycles de grâce contre la faim critique et arrêter la simulation quand tous sont morts.
- [x] Ajouter le bilan IA de fin de cycle et le flux de proposition d'évolution à valider humainement.
- [x] Ajouter un budget API persistant et verrouillé par run, configurable par `LIMIT_LLM_API_CALLS`.

## Priorité actuelle

- [x] Produire les bilans IA et demandes potentielles à chaque fin de cycle.
- [ ] Faire de Dieu un architecte contrôlé : il propose des mécanismes, mais ne modifie jamais directement le monde ou le code.
- [ ] Formaliser chaque évolution en TDD : test échouant, implémentation minimale, tests verts, revue.
- [ ] Construire la progression par mécanismes : baies/chasse, bois, haches, fer, fabrication, nouveaux besoins et nouvelles capacités.
- [ ] Concevoir un registre déterministe des mécanismes ajoutables au moteur.
