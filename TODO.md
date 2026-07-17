# TODO — Reprise d’Autopoiesis

Ce fichier sert de point de reprise lors du changement d’ordinateur.

## Prochaines étapes

- [ ] Effectuer un run de vérification sur 3 jours et confirmer les 6 appels API au cycle élémentaire 720, sans appel avant ce point, puis vérifier la pause humaine.
- [ ] Vérifier la persistance du budget d’appels API et le comportement de la limite configurée.
- [ ] Améliorer le regroupement des événements pertinents transmis à l’IA pour chaque personnage.
- [ ] Finaliser le format des demandes d’évolution adressées à Dieu avec critères d’acceptation testables.
- [x] Relier l'approbation humaine au daemon de Dieu et afficher son suivi dans le même terminal.
- [x] Limiter l'interface aux trois propositions les plus récentes et ajouter la correction TDD bornée de Dieu.
- [x] Activer automatiquement une évolution vérifiée par commit et push.
- [ ] Effectuer un run de bout en bout avec une demande approuvée et vérifier les logs de Dieu.
- [ ] Concevoir le registre des actions, ressources, outils et capacités ajoutables au moteur.
- [ ] Implémenter l’application isolée d’une évolution validée, avec test échouant avant activation.
- [ ] Clarifier ou implémenter par TDD le palier de chasse du lapin si la capacité attendue n’est pas encore active dans le moteur.
- [ ] Préparer les mécanismes suivants : bois, arbres abattables, haches, fer, fabrication et préconditions associées.
- [ ] Ajouter des tests de rejeu pour garantir le déterminisme du moteur local.

## Garde-fous

- L’IA personnifie le personnage et propose des évolutions ; elle ne modifie pas directement le monde.
- Toute évolution validée doit être appliquée de façon isolée, testée, puis versionnée.
- Toute modification du moteur commence par un test qui échoue et se termine par tests verts.
- Le fichier `.env` et les clés d’API ne doivent jamais être commités.
