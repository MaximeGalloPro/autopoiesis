# Instructions Autopoiesis

Avant toute modification, lire [`normes/glossaire.md`](normes/glossaire.md). Ce fichier décrit le vocabulaire, les règles d'architecture et les patterns à respecter dans le projet.

## Règles permanentes

- Préserver la séparation entre le décideur IA et le moteur d'exécution déterministe.
- Ne jamais laisser une réponse IA modifier directement l'état du monde ou le code source.
- Toute action proposée par un décideur doit être validée par le moteur avant exécution.
- Toute évolution proposée par l'IA reste `pending` jusqu'à validation humaine explicite.
- Ne jamais écrire de secret dans le code, les journaux, les images Docker ou Git.
- Utiliser les variables d'environnement documentées pour les fournisseurs LLM.
- Respecter l'horloge et sa garde : un cycle élémentaire est une action locale, une journée vaut 240 cycles élémentaires et la fenêtre IA de référence vaut 3 journées, soit 720 cycles ; seuls le bilan puis la demande d'évolution sont appelés à ce moment, puis le moteur attend une confirmation humaine avant de reprendre.
- La validation humaine doit rester intégrée au processus interactif ; ne pas demander à l'utilisateur de lancer un second script ou terminal pour débloquer la simulation. Une fenêtre ne traite qu'une seule demande ; les autres restent `pending`.
- Ajouter ou mettre à jour les tests lorsqu'une règle du moteur change.
- Vérifier la compilation Docker et les tests avant de considérer une modification terminée.
- Après chaque run de modifications, committer puis pousser la branche courante ; une modification non poussée n'est pas livrée et ne permet pas de lancer le jeu depuis un autre environnement.

Les normes peuvent être enrichies dans `normes/`, mais une nouvelle règle doit rester compatible avec ces invariants.
