# TODO — Reprise d’Autopoiesis

Ce fichier sert de point de reprise lors du changement d’ordinateur.

## Feuille de route gameplay

Chaque ligne représente un mécanisme jouable à livrer en TDD. L'ordre est
intentionnel : une phase doit produire une boucle observable avant d'ajouter la
suivante.

### Phase 1 - Faire société autour du camp

- [x] **G01 - Boucle de survie collective** : relier collecte, transport, dépôt, consommation et pénurie dans un même scénario automatisé. Premier flux livré : une nourriture portée vers la réserve du feu puis consommable par un autre personnage.
- [x] **G02 - Foyer collectif** : le premier feu devient le foyer partagé, sa fumée le signale et chaque personnage possède une place nocturne distincte persistante.
- [x] **G03 - Inventaires complets** : bois, branches et ration partagent une charge bornée dérivée de la force ; le décideur et le moteur empêchent tout dépassement et l'interface expose la capacité.
- [x] **G04 - Transport des ressources** : cibler des branches connues, les ramasser, rejoindre le foyer par BFS et transférer atomiquement bois et branches dans sa réserve persistante.
- [x] **G09 - Alimentation planifiée** : préférences persistantes, sélection anti-gaspillage, ration crue ou cuite, nutrition améliorée et péremption quotidienne des stocks comme du transport.
- [x] **G14 - Vie collective** : rôles spontanés selon les aptitudes, repas partagés, veillées nocturnes, célébrations et deuils bornés qui modifient relations et souvenirs.

Critère de sortie atteint : un personnage collecte une nourriture, la rapporte
au camp et un autre peut la consommer ; les matériaux suivent le même trajet,
les repas et activités sociales sont observables, et l'interface montre les
charges, rôles et réserves.

### Phase 2 - Transformer le monde

- [x] **G05 - Chaînes de fabrication** : registre de recettes explicites, coûts vérifiés et transformation atomique du bois ou des branches en manches, charbon et cordes persistants.
- [x] **G06 - Outils nécessaires** : collecter le fer, fondre un lingot avec du charbon, fabriquer et équiper une hache obligatoire, puis l'user et la réparer avec du bois.
- [x] **G07 - Compétences progressives** : huit savoirs gagnent de l'expérience sur les seules réussites, produisent des niveaux et spécialisations visibles, améliorent l'entretien des outils et se transmettent au foyer par une leçon quotidienne bornée.
- [x] **G08 - Construction spatiale** : réserver atomiquement les coûts, désigner puis bâtir avec une hache murs, portes, lits, réserves et ateliers persistants, visibles et fonctionnels sur la carte.
- [x] **G10 - Écosystème renouvelable** : repousses bornées, récupération lente des parcelles épuisées, reproduction sous capacité, prédation réelle des loups et indicateurs écologiques persistants.

Critère de sortie atteint : un abri fonctionnel exige une chaîne matière → outil →
travail → bâtiment, avec coût, durée et compétence observables.

### Phase 3 - Produire des histoires humaines

- [x] **G11 - Santé détaillée** : blessures et maladies causales, infection des plaies négligées, soin par un compagnon et convalescence quotidienne accélérée par abri ou lit.
- [x] **G12 - Émotions causales** : joie, peur, colère, tristesse, espoir et stress relient une cause mémorisée à une intensité, une durée et un effet décisionnel borné.
- [x] **G13 - Relations actives** : entraide, avertissement, accompagnement temporaire, conflit émotionnel et réconciliation modifient les liens et les comportements.
- [x] **G15 - Population évolutive** : vieillissement quotidien, arrivées et naissances conditionnées par le foyer, départ par détresse et mort naturelle persistants.
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
