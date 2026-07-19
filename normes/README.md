# Normes Autopoiesis

Ce dossier contient les conventions durables du projet : vocabulaire partagé, règles d'architecture et patterns de conception.

Le glossaire est le point d'entrée principal. Ajoutez-y les nouveaux termes ou patterns avant de les utiliser dans le code, les prompts ou la documentation.

Règle d'horloge à retenir : un cycle élémentaire est une action locale ; 240 cycles élémentaires forment une journée ; après 3 journées, au cycle élémentaire 720, chaque personnage déclenche exactement deux appels IA, d'abord le bilan puis la demande d'évolution, puis le moteur attend une confirmation humaine.

Règle calendaire : 30 journées forment un mois, 12 mois une année et quatre saisons déterministes influencent nourriture, soif, faim et fatigue sans ajouter d'appel API. Chaque bilan reçoit seulement les douze couples `bilan` et `ressenti` précédents du personnage.

Règle de livraison : après chaque run de modifications, les tests et le build Docker doivent être suivis d'un commit puis d'un push. Une modification non poussée n'est pas livrée.
