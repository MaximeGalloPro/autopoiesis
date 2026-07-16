# Instructions Autopoiesis

Avant toute modification, lire [`normes/glossaire.md`](normes/glossaire.md). Ce fichier décrit le vocabulaire, les règles d'architecture et les patterns à respecter dans le projet.

## Règles permanentes

- Préserver la séparation entre le décideur IA et le moteur d'exécution déterministe.
- Ne jamais laisser une réponse IA modifier directement l'état du monde ou le code source.
- Toute action proposée par un décideur doit être validée par le moteur avant exécution.
- Toute évolution proposée par l'IA reste `pending` jusqu'à validation humaine explicite.
- Ne jamais écrire de secret dans le code, les journaux, les images Docker ou Git.
- Utiliser les variables d'environnement documentées pour les fournisseurs LLM.
- Ajouter ou mettre à jour les tests lorsqu'une règle du moteur change.
- Vérifier la compilation Docker et les tests avant de considérer une modification terminée.

Les normes peuvent être enrichies dans `normes/`, mais une nouvelle règle doit rester compatible avec ces invariants.
