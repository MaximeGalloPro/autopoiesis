# Glossaire, règles et patterns

Ce document est évolutif. Il sert de référence commune pour les humains, Codex et les futurs composants d'Autopoiesis.

## Vocabulaire

### Personnage

Individu simulé. Il possède un état, des besoins, une personnalité, une mémoire et une perception limitée.

### Décideur IA

Composant qui reçoit la perception structurée d'un personnage et propose une décision. Il ne modifie jamais directement le monde.

### Personnification

Voix et point de vue subjectif d'un personnage. Elle sert aux décisions, aux bilans et aux demandes formulées à l'humain.

### Moteur d'exécution

Noyau local qui applique les règles du monde. Il valide les décisions, modifie l'état et reste déterministe pour des entrées et une graine identiques.

### Validateur d'action

Barrière locale qui vérifie le nom de l'action, ses paramètres, ses préconditions et ses permissions avant toute exécution.

### Dieu

Nom donné au futur générateur de monde et de capacités. Dans le MVP, Dieu ne modifie rien automatiquement : il produit uniquement des propositions soumises à validation humaine.

### Demande d'évolution

Proposition structurée indiquant un besoin, un obstacle, une évolution souhaitée et des critères d'acceptation. Son statut initial est `pending`.

### Cycle

Une journée de simulation contenant 240 créneaux d'action par personnage. Les bilans IA sont produits tous les cinq cycles.

## Règles d'architecture

1. Le décideur IA propose ; le moteur d'exécution dispose.
2. Les 240 actions quotidiennes sont exécutées localement ; les appels IA sont réservés aux bilans et demandes d'évolution périodiques.
3. Aucune réponse textuelle, justification ou demande IA ne peut modifier directement une variable du monde.
4. Toute action est refusée par défaut si elle est inconnue, mal paramétrée ou indisponible.
5. Une erreur IA ou réseau ne doit pas arrêter la simulation.
6. Le mode local doit rester utilisable sans réseau ni clé et doit être reproductible avec une graine identique.
7. La carte complète et les états invisibles ne doivent pas être transmis au décideur ou à l'observateur IA.
8. Les propositions d'évolution sont journalisées, validées dans le terminal, puis seulement transmises à un workflow de développement.
9. Les secrets restent dans l'environnement et ne sont jamais committés.

## Patterns

### Décision structurée

Les décideurs retournent un objet JSON strict. Le moteur effectue toujours une validation locale secondaire.

### Fallback sûr

En cas de réponse invalide, d'erreur réseau ou de budget API épuisé : journaliser l'incident, exécuter `wait`, poursuivre la simulation.

### Mémoire séparée

La mémoire narrative et la mémoire spatiale sont stockées séparément. La première est courte et FIFO ; la seconde conserve les cases explorées par personnage.

### Évolution contrôlée

Une demande suit le flux `pending → approved → implémentation isolée → tests → revue → nouvelle version`. Aucune transition ne doit être implicite.

### Demande à Dieu

Tous les cinq cycles, l'observateur IA reçoit l'historique pertinent d'un personnage. Il peut transformer un besoin récurrent ou un obstacle en proposition concrète d'évolution du monde, des capacités ou des algorithmes. La proposition n'est jamais exécutée dans la simulation en cours : elle attend une validation humaine et une intégration dans une version ultérieure du moteur.

### Reproductibilité

Tout hasard du moteur local passe par une graine configurable. Les réponses IA ne sont pas considérées comme déterministes ; pour les rejouer, il faut enregistrer ou simuler les décisions.

## Ajouts futurs

Ajouter ici les termes, invariants et patterns validés par le projet. Éviter d'y documenter une idée non encore acceptée comme règle.
