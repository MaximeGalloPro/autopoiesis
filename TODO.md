# TODO — Reprise d’Autopoiesis

Ce fichier sert de point de reprise lors du changement d’ordinateur.

## Prochaines étapes

- [ ] Effectuer un run de vérification sur 3 jours et confirmer les 6 appels API au cycle élémentaire 720, sans appel avant ce point, puis vérifier la pause humaine.
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
- [x] Présenter les trois propositions sous forme de cartes et intégrer leur sélection à la fenêtre graphique.
- [ ] Afficher le suivi détaillé de Dieu directement dans la fenêtre graphique.
- [ ] Étendre progressivement le catalogue du Diable lorsque de nouvelles capacités rendent des contraintes réelles jouables.
- [ ] Ajouter reproduction et renouvellement déterministe des populations et ressources.
- [ ] Ajouter inventaires, conservation des aliments et partage entre personnages.
- [ ] Ajouter des actions sociales spécialisées : partager une connaissance, demander de l'aide, avertir et accompagner.

## Garde-fous

- L’IA personnifie le personnage et propose des évolutions ; elle ne modifie pas directement le monde.
- Toute évolution validée doit être appliquée de façon isolée, testée, puis versionnée.
- Toute modification du moteur commence par un test qui échoue et se termine par tests verts.
- Le fichier `.env` et les clés d’API ne doivent jamais être commités.
