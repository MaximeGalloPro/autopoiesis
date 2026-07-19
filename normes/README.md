# Normes Autopoiesis

Ce dossier contient les conventions durables du projet : vocabulaire partagé, règles d'architecture et patterns de conception.

Le glossaire est le point d'entrée principal. Ajoutez-y les nouveaux termes ou patterns avant de les utiliser dans le code, les prompts ou la documentation.

Règle d'horloge à retenir : un cycle élémentaire est une opportunité d'action locale ; 2400 cycles élémentaires forment une journée par défaut ; après 3 journées, au cycle élémentaire 7200 d'un nouveau monde, chaque personnage déclenche exactement deux appels IA, d'abord le bilan puis la demande d'évolution, puis le moteur attend une confirmation humaine.

Règle calendaire : 30 journées forment un mois, 12 mois une année et quatre saisons déterministes influencent nourriture, soif, faim et fatigue sans ajouter d'appel API. Chaque bilan reçoit seulement les douze couples `bilan` et `ressenti` précédents du personnage.

Règle d'interface : la fenêtre raylib reçoit uniquement un instantané en lecture seule du moteur. Pendant les appels IA, elle anime une progression sans paralléliser ni ajouter d'appel. Pour la validation, elle affiche les trois demandes sous forme de cartes et renvoie seulement une commande à `HumanValidation`, qui reste l'unique autorité sur les statuts. Le mode terminal reste disponible.

Règle de contexte d'évolution : le second appel reçoit le catalogue des mécanismes actifs, les actions disponibles et une mémoire compacte de 24 demandes antérieures au maximum. L'IA demandeuse doit distinguer une capacité absente d'une capacité active mais mal intégrée et ne jamais recréer une ancienne proposition sous une nouvelle clé.

Règle de livraison : après chaque run de modifications, les tests et le build Docker doivent être suivis d'un commit puis d'un push. Une modification non poussée n'est pas livrée.
