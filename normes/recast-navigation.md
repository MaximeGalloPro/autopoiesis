# Navigation Recast/Detour : point d’intégration

## État du moteur et décision de périmètre

Le moteur actuel est une grille de `40 × 24` volontairement torique :
`World::wrap`, `World::step`, `World::neighbors` et
`World::toroidal_distance` normalisent les coordonnées hors-bord. Son
déplacement est discret (une case cardinale) et `Simulation::execute` vérifie
encore la praticabilité au moment où il applique l’action. Cet invariant est
incompatible avec une navmesh bornée non torique : remplacer directement les
routes du moteur par Detour créerait des divergences aux frontières et
contredirait le glossaire.

Ce lot ne branche donc pas une navmesh non torique au monde courant. Il crée le
contrat `INavigation`, indépendant de `World` et d’`Agent`, avec un premier
`BoundedGridNavigationAdapter`. Cette isolation est volontaire : le planificateur
produit une trajectoire descriptive, jamais une action ou une mutation. Le
moteur garde la validation de chaque `move` et reste la source de vérité.

## Contrat livré

`NavigationGrid` représente seulement les cases connues d’un demandeur :

- `Unknown` et `Blocked` sont infranchissables ; aucune carte complète ne peut
  être déduite par la navigation.
- Les limites sont fermées : aucun voisin ne peut franchir un bord puis réapparaître
  sur le bord opposé.
- Une trajectoire réussie contient départ et but ; deux points successifs sont
  voisins cardinaux, dans les limites et traversables.
- La recherche est déterministe. À coût égal, elle explore nord, est, sud puis
  ouest.
- L’adaptateur ne possède ni référence mutable vers le monde ni API qui exécute
  un déplacement.

Les tests de contrat couvrent l’absence de wrap, le détour déterministe, les
cases inconnues, les obstacles, le départ déjà atteint et les entrées invalides.

## Mapping précis vers Recast/Detour

Quand un monde **borné** sera adopté explicitement, un
`DetourNavigationAdapter` pourra implémenter la même interface sans exposer les
types Detour au reste du moteur :

1. Construire la géométrie autoritaire à partir d’un instantané du monde validé.
   Une case `(x, y)` devient un carré sur le plan `(x, z)` de `[(x, y),
   (x + 1, y + 1)]`, avec une hauteur constante. Les bornes de la navmesh sont
   strictement `[0, largeur] × [0, hauteur]` : aucune tuile miroir ou connexion
   de bord ne doit être créée.
2. Marquer eau, arbres, murs achevés et autres obstacles comme non marchables.
   Les portes restent marchables selon la règle moteur. Pour la carte de mémoire
   d’un personnage, ne fournir au query que la géométrie déjà observée ;
   l’inconnu reste absent de la navmesh ou filtré par `dtQueryFilter`.
3. Interroger `findNearestPoly`, `findPath`, puis `findStraightPath`. Le chemin
   continu résultant doit être rasterisé en segments cardinaux connus avant de
   devenir une succession de propositions `move`; Detour ne peut pas téléporter
   un personnage à un coin de corridor.
4. Chaque proposition doit encore passer par `validate_decision`, puis par
   `Simulation::execute`, qui revalide la case après une modification dynamique.
   Un chemin n’est jamais une autorisation d’exécuter ni un état persistant de
   référence.
5. À toute construction qui bloque une case, invalider le cache associé à la
   version du monde. Sur une carte de cette taille, reconstruire entièrement la
   navmesh est plus simple et plus robuste qu’un `dtTileCache`; ne jamais
   sérialiser des `dtPolyRef` dans les checkpoints.

Les rayons, hauteurs, pente maximale et coûts d’aire devront être des constantes
versionnées du mécanisme de monde. Les off-mesh links sont exclus tant qu’une
capacité moteur validée ne les définit pas.

## Dépendance et prochaine étape

Recast/Detour n’est pas ajouté à ce stade : pour une carte de grille petite et
des déplacements cardinaux, son coût de build, de mise à jour et de conversion
continue n’apporte pas encore de capacité moteur. L’ajouter maintenant
introduirait une dépendance native sans chemin d’exécution autoritaire.

Lorsqu’un monde non torique sera approuvé, l’étape suivante est un backend
optionnel, épinglé à une révision RecastNavigation, derrière `INavigation` :
tests de parité avec `BoundedGridNavigationAdapter`, tests de brouillard de
guerre, invalidation après mur, puis intégration dans le décideur local. Cette
intégration devra être accompagnée de la décision explicite de faire évoluer la
topologie canonique du monde et ses tests transversaux.
