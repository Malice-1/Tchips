# Dépendances et décisions de build

## Gestion et reproductibilité

Le manifeste `vcpkg.json` épingle le registre à `0df93a05b9de080129e653ec4776d7e14a1b2e4f`. Les scripts `tools/bootstrap.ps1` et `tools/bootstrap.sh` sélectionnent exactement ce commit. Aucun registre flottant ni installation globale par vcpkg.

Les sources/outils et téléchargements sont dans `.tools/`, les paquets compilés sous `out/build/<preset>/vcpkg_installed/`. Ils sont ignorés par Git ; le manifeste, ses fonctionnalités et le pin constituent la description reproductible. Le premier build nécessite Internet et peut télécharger les versions d'outils requises par ce registre en plus des bibliothèques.

## Audit du registre épinglé

Audit effectué le 17 novembre 2026. Commande portable : `cmake -P tools/check-dependencies.cmake` ; variante Windows : `powershell -NoProfile -ExecutionPolicy Bypass -File tools/check-dependencies.ps1`.

| Besoin | Port/version du registre | Activation CMake |
| --- | --- | --- |
| Fenêtre et GPU | `sdl3` 3.4.20, fonctionnalité Vulkan activée | `TCHIPS_BUILD_CLIENT=ON` |
| ECS | `entt` 4.0.0 | Base du manifeste |
| Transport | `gamenetworkingsockets` 1.6.1, ICE/P2P non demandé | `TCHIPS_ENABLE_NETWORK=ON` |
| Messages | `protobuf` 7.36.2 | Même option réseau |
| Interface | `rmlui` 6.3 | `TCHIPS_ENABLE_UI=ON` |
| Audio | `miniaudio` 0.11.26 | `TCHIPS_ENABLE_AUDIO=ON` |
| Client SQL | `libpqxx` 8.0.2, dépendant de libpq/PostgreSQL | `TCHIPS_ENABLE_PERSISTENCE=ON` |
| Tests | `gtest` 1.18.0 | `TCHIPS_BUILD_TESTS=ON` |
| Profiling | `tracy` 0.15.0 | `TCHIPS_ENABLE_PROFILING=ON` |

Tous les ports sont présents dans le registre épinglé. Cette observation ne signifie pas que toutes les combinaisons de fonctionnalités/triplets ont été compilées. Les commandes et résultats réellement vérifiés seront enregistrés dans `docs/validation.md`.

## SDL_GPU et rendu

« SDL_GPU » désigne l'API native de SDL3 (`SDL_GPUDevice`, command buffers et render passes). Il n'existe donc pas de dépendance séparée `sdl-gpu` dans ce manifeste. Le client n'utilise pas SDL_Renderer et ne remplace pas silencieusement le GPU par un autre moteur.

La phase 01 dessine des triangles 2D colorés avec les shaders précompilés fournis par les tests de SDL3 3.4.20 (DXIL, SPIR-V, MSL). Sources et licence sont conservées dans `apps/client/shaders/` ; aucun compilateur de shaders ni port supplémentaire n'est requis. Le personnage temporaire est une grille de pixels locaux, pas encore une texture ou un atlas animé. L'adaptateur RmlUi et les futurs sprites texturés resteront des étapes distinctes.

Référence : [API SDL3 GPU](https://wiki.libsdl.org/SDL3/CategoryGPU), [création du périphérique](https://wiki.libsdl.org/SDL3/SDL_CreateGPUDevice).

## Plateformes

- Windows MinGW : preset utilisable avec GCC/Ninja dans le PATH, triplets cible **et hôte** `x64-mingw-dynamic`, sans dépendre de MSVC. Les triplets MinGW sont communautaires et non couverts par la CI vcpkg ; le build minimal ne garantit pas GameNetworkingSockets/libpqxx ou toute la stack. [Documentation vcpkg](https://learn.microsoft.com/en-us/vcpkg/users/platforms/mingw)
- Windows MSVC : preset séparé, à exécuter dans un environnement développeur x64 récent avec C++23 et Ninja. Il ne peut pas être validé sur une machine sans MSVC.
- Linux serveur : preset headless, sans SDL3, RmlUi ou miniaudio. Compilateur avec `std::expected` (GCC 13+ recommandé), CMake et Ninja. Ce preset demande une vérification sur une machine Linux ; une compilation Windows n'est pas cette vérification.

Le chemin actuel contient `Privé`. La première configuration Ninja/vcpkg a échoué sur le lancement d'un outil sous ce chemin. Un alias de lecteur ASCII évite ce problème sans déplacer les sources : voir `README.md`. Conserver le même chemin de source pour un répertoire de build ; utiliser `--fresh` ou un nouveau `binaryDir` après changement.

## Activation ultérieure de toute la stack

Après installation d'une chaîne compatible, par exemple MSVC x64 :

```powershell
cmake --preset windows-msvc-debug `
  -DTCHIPS_ENABLE_NETWORK=ON `
  -DTCHIPS_ENABLE_UI=ON `
  -DTCHIPS_ENABLE_AUDIO=ON `
  -DTCHIPS_ENABLE_PERSISTENCE=ON `
  -DTCHIPS_ENABLE_PROFILING=ON
cmake --build --preset windows-msvc-debug
```

Cette commande est une procédure prévue pour les options restantes, pas une compilation complète de la stack annoncée réussie. La phase 02 active déjà le réseau et génère les contrats Protobuf ; UI RmlUi, audio et connexion SQL restent absents.

En phase 02, GameNetworkingSockets 1.6.1, Protobuf 7.36.2, OpenSSL 3.6.5 et Abseil 20260107.1 ont été compilés avec le triplet communautaire `x64-mingw-dynamic`, puis liés aux exécutables. Le premier téléchargement/build a pris environ une heure ; les builds suivants réutilisent le cache. Le port OpenSSL émet un avertissement de validation concernant un répertoire vide ; aucun port n'a été modifié pour le masquer. Les résultats d'exécution sont consignés dans [validation.md](validation.md).

Le serveur PostgreSQL est un processus externe, pas quelque chose que libpqxx démarre. Son installation, version, migrations et secrets seront choisis dans la phase de persistance ; aucune base n'est requise pour tester ce socle.
