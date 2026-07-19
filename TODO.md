# TODO — Reprise d’Autopoiesis

Ce fichier sert de point de reprise lors du changement d’ordinateur.

## Feuille de route gameplay

Chaque ligne représente un mécanisme jouable à livrer en TDD. L'ordre est
intentionnel : une phase doit produire une boucle observable avant d'ajouter la
suivante.

### Phase 1 - Faire société autour du camp

- [x] **G01 - Boucle de survie collective** : relier collecte, transport, dépôt, consommation et pénurie dans un même scénario automatisé. Premier flux livré : une nourriture portée vers la réserve du feu puis consommable par un autre personnage.
- [x] **G02 - Foyer collectif** : le premier feu devient le foyer partagé, sa fumée le signale et chaque personnage possède une place nocturne distincte persistante.
- [ ] **G03 - Inventaires complets** : porter des aliments, matériaux et objets avec une capacité bornée.
- [ ] **G04 - Transport des ressources** : choisir une ressource connue, la ramasser, rejoindre une destination et la déposer.
- [ ] **G09 - Alimentation planifiée** : repas, nutrition, préférences, réserves, cuisson et conservation progressive.
- [ ] **G14 - Vie collective** : repas partagés, veillées, rôles spontanés, célébrations et deuils.

Critère de sortie : un personnage collecte une nourriture, la rapporte au camp
et un autre personnage peut la consommer ; les journaux expliquent chaque
intention et l'interface montre la réserve.

### Phase 2 - Transformer le monde

- [ ] **G05 - Chaînes de fabrication** : transformer des ressources par recettes explicites sans création implicite de matière.
- [ ] **G06 - Outils nécessaires** : fabriquer, équiper, user et réparer les premiers outils.
- [ ] **G07 - Compétences progressives** : apprendre par la pratique, se spécialiser et transmettre.
- [ ] **G08 - Construction spatiale** : désigner puis bâtir murs, portes, lits, réserves et ateliers sur la carte.
- [ ] **G10 - Écosystème renouvelable** : croissance, reproduction, prédation et surexploitation locale.

Critère de sortie : un abri fonctionnel exige une chaîne matière → outil →
travail → bâtiment, avec coût, durée et compétence observables.

### Phase 3 - Produire des histoires humaines

- [ ] **G11 - Santé détaillée** : blessures, maladies, infections, soins et convalescence.
- [ ] **G12 - Émotions causales** : relier chaque émotion à un souvenir et à un effet décisionnel borné.
- [ ] **G13 - Relations actives** : entraide, conflit, avertissement, accompagnement et réconciliation.
- [ ] **G15 - Population évolutive** : arrivées, départs, vieillissement, naissance et mort.
- [ ] **G16 - Dangers actifs** : animaux territoriaux, incendies, accidents, tempêtes et pénuries signalées.

Critère de sortie : une difficulté laisse une trace durable dans la santé, les
relations et les décisions d'au moins deux personnages.

### Phase 4 - Donner une trajectoire au joueur

- [ ] **G17 - Diable adaptatif** : choisir une pression selon stabilité, historique et capacités avec difficulté croissante.
- [ ] **G18 - Objectifs mesurables** : afficher des objectifs collectifs comme survivre à l'hiver ou constituer une réserve.
- [ ] **G19 - Lisibilité décisionnelle** : montrer objectif, destination, chemin, priorité et cause des changements.
- [ ] **G20 - Mise en scène vivante** : cycle lumineux, météo, animations, sons et chronologie des événements majeurs.

Critère de sortie : le joueur comprend ce qui menace le groupe, ce que chacun
tente et comment son choix d'évolution a changé la trajectoire du monde.

## Travaux techniques et validations

- [ ] Effectuer un run de vérification sur 3 jours et confirmer les 6 appels API au cycle élémentaire 7200, sans appel avant ce point, puis vérifier la pause humaine.
- [ ] Vérifier la persistance du budget d’appels API et le comportement de la limite configurée.
- [x] Relier les événements transmis à l’IA aux aspirations, projets, blocages, relations et à la monotonie de chaque personnage.
- [x] Finaliser le format des demandes d’évolution avec clé sémantique, domaine et critères d’acceptation testables.
- [x] Relier l'approbation humaine au daemon de Dieu et afficher son suivi dans le même terminal.
- [x] Limiter l'interface aux trois propositions les plus récentes et ajouter la correction TDD bornée de Dieu.
- [x] Activer automatiquement une évolution vérifiée par commit et push.
- [ ] Effectuer un run de bout en bout avec une demande approuvée et vérifier les logs de Dieu.
- [ ] Concevoir le registre des actions, ressources, outils et capacités ajoutables au moteur.
- [ ] Implémenter l’application isolée d’une évolution validée, avec test échouant avant activation.
- [ ] Clarifier ou implémenter par TDD le palier de chasse du lapin si la capacité attendue n’est pas encore active dans le moteur.
- [ ] Préparer les mécanismes suivants : bois, arbres abattables, haches, fer, fabrication et préconditions associées.
- [x] Ajouter des tests de rejeu pour garantir le déterminisme du moteur local et des projets.
- [x] Agrandir la carte à `40 × 24` et appliquer une topologie torique à tous les déplacements.
- [x] Ajouter eau, soif, cinq aliments, cinq espèces animales et dix attributs actifs.
- [x] Remplacer la décision locale purement réactive par utilité, persistance d'objectif et BFS sur mémoire limitée.
- [x] Ajouter le Diable : tirage local une fois par fenêtre, contraintes rationnelles, validation séparée et approbation automatique optionnelle.
- [x] Notifier macOS à la fin du workflow de Dieu ou lorsqu'il dépasse le délai d'attente.
- [x] Ajouter mois, années, saisons, climat déterministe et effets bornés sur nourriture et personnages.
- [x] Conserver douze mémoires de période par personnage sous forme d'une phrase de bilan et d'une phrase de ressenti.
- [x] Ajouter une interface raylib native avec carte, calendrier et inspecteur de personnage cliquable, tout en conservant le terminal.
- [x] Maintenir une transition graphique animée pendant les appels IA et rendre les interactions visibles au survol.
- [x] Donner à l'IA demandeuse le catalogue actif et une mémoire bornée des évolutions pour éviter les propositions rebaptisées.
- [x] Présenter les trois propositions sous forme de cartes et intégrer leur sélection à la fenêtre graphique.
- [x] Afficher le suivi détaillé de Dieu directement dans la fenêtre graphique.
- [ ] Étendre progressivement le catalogue du Diable avec **G17**.

## Garde-fous

- L’IA personnifie le personnage et propose des évolutions ; elle ne modifie pas directement le monde.
- Toute évolution validée doit être appliquée de façon isolée, testée, puis versionnée.
- Toute modification du moteur commence par un test qui échoue et se termine par tests verts.
- Le fichier `.env` et les clés d’API ne doivent jamais être commités.
