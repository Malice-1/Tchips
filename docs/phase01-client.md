# Phase 01 — prototype 2D natif local

## Modules

| Module | Responsabilité |
| --- | --- |
| Application | Arguments, durée de vie SDL/fenêtre, boucle monotone, orchestration du monde EnTT, bascule plein écran et erreurs fatales. |
| Renderer | Périphérique SDL_GPU, shaders, buffers/transfert et triangles de présentation ; aucune écriture dans WorldMap ou CharacterTransform. |
| Camera2D | Suivi borné au monde et conversions typées monde/écran, indépendantes de SDL. |
| InputManager | Événements SDL, touches physiques, position souris logique, perte de focus, F11 et fermeture. |
| AssetManager | Racine d'assets près de l'exécutable et chargement/validation du personnage temporaire. |
| WorldMap | Chargement et validation de la géométrie locale versionnée. |
| CollisionSystem | Boîte du personnage et résolution balayée par axe, limites du monde et glissement. |
| MovementSystem | Normalisation de l'entrée, mouvement EnTT à pas fixe, orientation et facteur d'interpolation. |

Les fichiers sont dans `apps/client/include/tchips/client/` et `apps/client/src/`. `tchips_client_core` ne dépend que de C++23, EnTT et la géométrie partagée ; `tchips_client_platform` ajoute SDL3 et les shaders. Le serveur reste inchangé. Aucun réseau, combat ou interface complexe n'est développé.

## Simulation et coordonnées

`WorldPosition`/`WorldVector` utilisent des unités du monde. `ScreenPosition` utilise les pixels logiques de la fenêtre ; les types ne se convertissent pas implicitement. La caméra transforme ces pixels logiques en coordonnées monde pour la visée. Le renderer convertit ensuite l'écran logique en NDC ; la swapchain utilise sa résolution physique. Cette séparation garde le curseur cohérent sur les écrans à haute densité.

Configuration initiale : vitesse 240 unités/s, demi-taille de collision 12 unités, pas de 1/60 s, huit pas de rattrapage maximum et delta de frame plafonné à 0,25 s. `MovementConfig` permet de régler l'horloge ; vitesse et forme sont dans le composant `CharacterBody`. Aucun déplacement n'est exprimé en pixels par frame.

Le bootstrap mesure le temps avec `std::chrono::steady_clock`. La simulation conserve position précédente/courante ; seul le rendu les interpole. Après une surcharge dépassant le budget de huit pas, l'arriéré est abandonné pour éviter une boucle non bornée : la vitesse n'est donc pas garantie sous framerate extrême inférieur au budget de rattrapage. Les fréquences usuelles de 30 à 240 images/s sont testées.

Les collisions balayent le déplacement X, puis Y ; le résultat s'arrête au premier obstacle rencontré sur chaque axe sans traverser une paroi mince. Une position exactement en contact est valide. Le collider est une boîte alignée au monde et ne pivote pas avec les pixels de présentation. La caméra suit la position interpolée et se bloque aux bords ; un monde plus petit que la fenêtre est centré.

Les angles sont en radians : zéro vers +X, +Y vers le bas. Le personnage temporaire et son indicateur doré s'orientent avec la souris ; il n'existe pas encore de tir.

## Assets locaux et validation

Le build déploie les assets dans `out/build/<preset>/bin/assets/`. L'application les résout depuis le dossier de l'exécutable avec SDL_GetBasePath, pas depuis le répertoire courant. Une carte fournie par `--map` est un chemin absolu ou relatif au répertoire courant. Aucun chargement d'asset n'a lieu dans la boucle de rendu.

### Carte `.map`, version 1

```text
TCHIPS_MAP 1
size 2560 1920
tile 64
spawn 256 256
obstacles 1
448 192 128 384
```

Ordre des champs fixe ; espaces/sauts de ligne équivalents. Chaque obstacle définit `x y largeur hauteur`. Aucun champ supplémentaire, commentaire ou donnée tronquée n'est accepté. Dimensions finies et positives, maximum 100 000 unités par axe ; taille de tuile 16 à 512 ; maximum 512 obstacles ; rectangles positifs entièrement dans le monde. Spawn dans le monde et hors obstacle. Application vérifie en plus que toute la boîte du personnage tient au spawn. Fichier limité à 128 Kio.

### Personnage `.sprite`, version 1

```text
TCHIPS_SPRITE 1 3 2
.c.
dbe
```

Dimensions de 1 à 32 pixels par axe, exactement autant de lignes/colonnes ; `.` transparent, `a` doré, `b` turquoise, `c` armure, `d` contour sombre, `e` clair. Palette fixe du prototype, aucun symbole inconnu ni sprite vide. Fichier limité à 4 Kio. Le personnage réel fait 12 × 12 pixels, rendu en petits quads colorés et tournés, sans texture ni animation.

### Shaders

Les variantes de couleur DXIL/SPIR-V/MSL des tests de SDL release-3.4.20 sont conservées avec sources et licence dans `apps/client/shaders/`. Le renderer choisit une variante supportée par le périphérique. Aucune compilation de shader n'est téléchargée au démarrage. La géométrie CPU et les buffers GPU sont préalloués ; le nombre de sommets est borné à 65 536. Les tuiles décoratives sont regroupées à des résolutions extrêmes sans changer les obstacles.

## Lancement et contrôles

Depuis T: (alias ASCII du projet sous Windows) :

```powershell
cmake --preset windows-mingw-debug
cmake --build --preset windows-mingw-debug
ctest --preset windows-mingw-debug
.\out\build\windows-mingw-debug\bin\tchips_client.exe
```

WASD physiques (positions ZQSD sur AZERTY), souris pour viser, F11 plein écran bureau/fenêtre, Échap ou fermeture pour quitter. `--fullscreen` démarre en plein écran. `--map PATH` remplace la carte locale. `--frames N` borne un test de démarrage. Aucune connexion au serveur n'est requise.

Le mode de diagnostic `--smoke-test` génère des entrées par SDL_PushEvent, avance un pas fixe par frame pour rendre le trajet reproductible, vérifie l'arrêt à x=436 contre le premier obstacle, déplace le personnage le long de la paroi, redimensionne à 1 024 × 640, bascule deux fois F11 et ferme par événement SDL_QUIT après 150 frames. Le GPU reste réel ; cette option n'est pas ajoutée aux tests headless.

## Validation exécutée

Le 17 novembre 2026, Windows/MinGW GCC 16.1.0 : configuration CMake et compilation réussies ; **24 tests CTest/GoogleTest réussis**, dont les nouveaux tests de mouvement, collisions, caméra, fichiers et entrées. Les tests de déplacement comparent 30, 60, 144 et 240 images/s sur une même durée.

Le scénario graphique `--smoke-test` a réussi sur **Direct3D 12**, 150 frames et code de sortie 0 : entrées, collision, redimensionnement, plein écran, caméra et événement de fermeture. Aucun nouveau paquet vcpkg n'a été requis. Les tests du socle serveur sont restés verts.

## Limites et suite

Ce mouvement est une simulation locale de prototype, pas une autorité serveur ni une implémentation réseau. Collision AABB par axe, pas de tuiles graphiques/textures, d'animation, de zoom, de combat ou d'UI. La palette est provisoire. La charge GPU n'est pas un benchmark ; les résolutions et densités variées n'ont pas toutes été testées. Les variantes Vulkan/Metal et les builds Linux/MSVC sont préparés, mais seul Direct3D 12/MinGW a été exécuté dans cette phase. Aucun sanitizer n'a été exécuté.

La fermeture restaure le curseur et libère, dans l'ordre, ressources GPU, claim de fenêtre, périphérique, fenêtre et SDL. Les erreurs de chargement et de GPU sont journalisées avec un code de sortie non nul.

Prochaine étape proposée : validation manuelle du prototype, puis transport versionné et déplacement autoritaire à deux clients dans une phase distincte.
