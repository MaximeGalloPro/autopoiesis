# TODO — Reprise d’Autopoiesis

Ce fichier sert de point de reprise lors du changement d’ordinateur.

## Prochaines étapes

- [ ] Effectuer un run de vérification sur 5 jours et confirmer le rapport IA périodique.
- [ ] Vérifier la persistance du budget d’appels API et le comportement de la limite configurée.
- [ ] Améliorer le regroupement des événements pertinents transmis à l’IA pour chaque personnage.
- [ ] Finaliser le format des demandes d’évolution adressées à Dieu et leur validation dans le terminal.
- [ ] Relier le hook de validation à une prise en charge Codex contrôlée.
- [ ] Concevoir le registre des actions et capacités ajoutables au moteur.
- [ ] Implémenter l’application isolée d’une évolution validée, avec tests avant activation.
- [ ] Ajouter des tests de rejeu pour garantir le déterminisme du moteur local.

## Garde-fous

- L’IA personnifie le personnage et propose des évolutions ; elle ne modifie pas directement le monde.
- Toute évolution validée doit être appliquée de façon isolée, testée, puis versionnée.
- Le fichier `.env` et les clés d’API ne doivent jamais être commités.
