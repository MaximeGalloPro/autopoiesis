# Glossaire, règles et patterns

Ce document est évolutif. Il sert de référence commune pour les humains, Codex et les futurs composants d'Autopoiesis.

## Vocabulaire

### Personnage

Individu simulé. Il possède un état, des besoins, une personnalité, une mémoire et une perception limitée.

### Décideur IA

Composant qui reçoit la perception structurée d'un personnage et propose une décision. Il ne modifie jamais directement le monde.

### Personnification

Voix et point de vue subjectif d'un personnage. Elle sert aux décisions, aux bilans et aux demandes formulées à l'humain.

### Instance architecte

Agent qui conçoit et configure le système d'évolution. Cette instance intervient lors de la mise sur pied du dispositif, mais ne participe pas au circuit opérationnel des demandes une fois celui-ci lancé.

### Instance validatrice

Processus Codex séparé qui lit une demande `pending` en lecture seule, contrôle son contrat et sa politique d'évolution, puis produit une décision structurée. Il ne modifie ni le moteur ni le worktree de Dieu.

### Moteur d'exécution

Noyau local qui applique les règles du monde. Il valide les décisions, modifie l'état et reste déterministe pour des entrées et une graine identiques.

### Validateur d'action

Barrière locale qui vérifie le nom de l'action, ses paramètres, ses préconditions et ses permissions avant toute exécution.

### Dieu

Architecte du monde, des capacités et des règles de simulation. Dieu transforme une demande approuvée en modification technique incrémentale. Il ne s'auto-autorise jamais : l'autorisation d'implémenter doit précéder toute modification, puis l'implémentation reste soumise aux tests et à la vérification.

### Mécanisme

Ensemble cohérent de règles déterministes ajoutées au moteur : nouvelle ressource, nouvel objet, nouvelle action, nouveau besoin, transformation du monde ou interaction entre personnages. Un mécanisme doit être livré avec ses tests avant d'être considéré actif.

### Capacité

Action ou aptitude disponible pour un personnage lorsque ses préconditions locales sont satisfaites. Une capacité peut dépendre d'un mécanisme, d'un outil, d'une ressource, d'un état interne ou d'une connaissance acquise.

### Demande d'évolution

Proposition structurée indiquant un besoin, un obstacle, un mécanisme souhaité et des critères d'acceptation. Son statut initial est `pending`. Elle ne peut devenir `approved` qu'après le contrôle de l'instance validatrice et l'autorisation prévue par la politique active. La politique actuelle exige une validation humaine explicite. Le Validator peut être humain (`VALIDATOR_MODE=human`) ou Codex (`VALIDATOR_MODE=codex`), mais le mode Codex ne fait qu'émettre une recommandation et ne franchit jamais cette garde.

### Cycle

Une journée de simulation contenant 240 créneaux d'action par personnage. Les bilans IA et les éventuelles demandes d'évolution sont produits à chaque fin de cycle.

## Règles d'architecture

1. Le décideur IA propose ; le moteur d'exécution dispose.
2. Les 240 actions quotidiennes sont exécutées localement ; les appels IA sont réservés aux bilans et demandes d'évolution périodiques.
3. Aucune réponse textuelle, justification ou demande IA ne peut modifier directement une variable du monde.
4. Toute action est refusée par défaut si elle est inconnue, mal paramétrée ou indisponible.
5. Une erreur IA ou réseau ne doit pas arrêter la simulation.
6. Le mode local doit rester utilisable sans réseau ni clé et doit être reproductible avec une graine identique.
7. La carte complète et les états invisibles ne doivent pas être transmis au décideur ou à l'observateur IA.
8. Les propositions d'évolution sont journalisées, contrôlées par l'instance validatrice, puis seulement transmises à Dieu après l'autorisation prévue par la politique d'approbation.
9. Les secrets restent dans l'environnement et ne sont jamais committés.
10. Toute évolution du moteur suit le TDD : test échouant d'abord, implémentation minimale, tests verts, puis revue.

## Patterns

### Décision structurée

Les décideurs retournent un objet JSON strict. Le moteur effectue toujours une validation locale secondaire.

### Fallback sûr

En cas de réponse invalide, d'erreur réseau ou de budget API épuisé : journaliser l'incident, exécuter `wait`, poursuivre la simulation.

### Mémoire séparée

La mémoire narrative et la mémoire spatiale sont stockées séparément. La première est courte et FIFO ; la seconde conserve les cases explorées par personnage.

### Évolution contrôlée

Une demande suit le flux `pending → approved → implementation → tests → verification → nouvelle version`. Aucune transition ne doit être implicite. `approved` autorise Dieu à travailler sur la demande ; il n'autorise pas à ignorer les tests ni à activer un résultat non vérifié.

Une recommandation `reformulate` crée une nouvelle demande liée à la précédente. Le nombre maximal de reformulations est `VALIDATOR_MAX_REFORMULATIONS` et vaut `3` par défaut. Une demande qui dépasse cette limite devient `rejected` et ne peut pas être transmise à Dieu.

### Changelog de Dieu

Journal généré pour chaque exécution de Dieu. Il décrit la demande approuvée, le mécanisme visé, les fichiers modifiés et le résultat de la vérification. Il est conservé dans les artefacts d'exécution et ne constitue ni un commit ni une autorisation d'activation.

### Interface de validation

Vue terminal minimale lancée après une simulation interactive lorsqu'une demande attend une décision humaine. Elle affiche les données structurées et délègue toute transition aux scripts existants ; elle ne devient jamais une source d'état parallèle.

Après approbation, elle orchestre l'observation d'une seule évolution : démarrage de Dieu, attente de son compte rendu, lancement de la vérification, affichage du bilan, puis choix de revenir aux demandes. Elle n'exécute aucune règle du moteur et ne fusionne aucun worktree.

### Protocole d'intervention

Le protocole d'une évolution est une expérience complète, et non l'ajout direct d'une capacité :

1. La personnification d'un personnage formule une demande d'évolution structurée à la fin d'un cycle.
2. L'instance validatrice examine le besoin, le périmètre, le mécanisme et les critères d'acceptation en lecture seule. Elle peut accepter, refuser ou demander une reformulation.
3. Une demande acceptée par la politique d'approbation passe explicitement de `pending` à `approved`. Cette transition constitue l'autorisation donnée à Dieu.
4. Dieu, l'orchestrateur d'évolution, transforme uniquement cette demande approuvée en test et en modification isolée du moteur. Il ne traite pas les autres demandes en attente.
5. Le test doit d'abord échouer, puis l'implémentation minimale doit le rendre vert. Dieu ne profite pas de l'occasion pour ajouter des mécanismes voisins.
6. Un vérificateur déterministe exécute la compilation, les tests ciblés et la suite complète. Il contrôle aussi que la demande initiale est réellement satisfaite et que les invariants du monde sont conservés.
7. Un compte rendu critique décrit le résultat, les écarts, les effets secondaires et la plus petite amélioration éventuellement justifiée. Cette critique ne déclenche aucune nouvelle évolution automatiquement.
8. La nouvelle capacité n'est considérée active qu'après cette vérification et la revue finale.

L'instance architecte qui a conçu ce dispositif ne joue aucun de ces rôles opérationnels. Elle ne valide pas les demandes et ne lance pas Dieu après la mise en service.

Le projet conserve actuellement une validation humaine explicite comme garde d'activation. Un mode entièrement autonome, dans lequel une politique préautorisée ferait passer une demande à `approved` et activerait une version, devra être décidé, documenté et testé séparément.

### Demande à Dieu

À chaque fin de cycle, l'observateur IA reçoit l'historique pertinent d'un personnage. Il peut transformer un besoin récurrent ou un obstacle en proposition concrète d'évolution du monde, des capacités ou des algorithmes. La proposition n'est jamais exécutée dans la simulation en cours : elle attend le contrôle de l'instance validatrice, l'autorisation donnée à Dieu et une intégration vérifiée dans une version ultérieure du moteur.

Une demande `pending` doit identifier un titre, le besoin, l'obstacle, le changement proposé et un mécanisme unique. Ce mécanisme décrit ses ressources, actions, préconditions, effets déterministes et tests d'acceptation non vides. Une demande IA qui ne respecte pas ce contrat est journalisée comme rejetée et ne devient pas `pending`.

### Croissance par mécanismes

La simulation se complexifie par petites extensions testées. Un personnage peut d'abord manger des baies ou chasser, puis demander de nouveaux moyens : couper des arbres, fabriquer une hache, ramasser du fer, fondre du métal, construire un abri, etc. Chaque étape doit introduire les ressources, capacités, préconditions et effets strictement nécessaires, sans court-circuiter le moteur déterministe.

### Reproductibilité

Tout hasard du moteur local passe par une graine configurable. Les réponses IA ne sont pas considérées comme déterministes ; pour les rejouer, il faut enregistrer ou simuler les décisions.

## Ajouts futurs

Ajouter ici les termes, invariants et patterns validés par le projet. Éviter d'y documenter une idée non encore acceptée comme règle.
