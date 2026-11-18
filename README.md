# Tchips — prototype 2D natif C++23

Client desktop SDL3/SDL_GPU et serveur headless C++23 séparés. La **phase 02** ajoute le déplacement multijoueur autoritaire à 30 Hz : GameNetworkingSockets, Protobuf versionné, prédiction locale, réconciliation et interpolation des autres joueurs. Aucun combat ni base de données.

## Windows — chaîne disponible ici : MinGW

Prérequis dans le PATH : Git, CMake 3.28+, Ninja et GCC/MinGW avec C++23 et `std::expected`. La chaîne examinée est GCC 16.1.0, CMake 4.3.2 et Ninja 1.13.2. Une connexion Internet est nécessaire à l'initialisation et au premier téléchargement des bibliothèques.

Le dossier actuel comporte un accent. Pour éviter le problème de lancement observé avec Ninja/vcpkg, ouvrir PowerShell dans `Tchips` et utiliser un lecteur libre (T: dans les commandes ci-dessous) :

```powershell
subst T: (Get-Location).Path
Set-Location T:\
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
cmake --preset windows-mingw-debug
cmake --build --preset windows-mingw-debug
ctest --preset windows-mingw-debug
```

Si T: est déjà un alias vers ce dossier, commencer à `Set-Location`. Ne pas remplacer un lecteur occupé ; choisir une autre lettre libre si nécessaire. L'option PowerShell ne change pas la politique globale d'exécution. Les scripts épinglent vcpkg et créent les caches locaux. Si un build a déjà été configuré depuis le chemin accentué, employer une fois `cmake --fresh --preset windows-mingw-debug` depuis T:.

**Démarrer le serveur**, dans un premier terminal :

```powershell
.\out\build\windows-mingw-debug\bin\tchips_server.exe --port 27020
```

**Démarrer deux clients**, dans deux autres terminaux placés à la racine :

```powershell
.\out\build\windows-mingw-debug\bin\tchips_client.exe --connect 127.0.0.1:27020
```

La carte et le personnage sont chargés depuis les assets copiés près de l'exécutable lors du build. Les obstacles bleus/gris sont solides ; le personnage turquoise suit la visée. La carte mesure 2 560 × 1 920 unités, plus que la fenêtre, et la caméra suit le personnage en restant dans ses limites.

| Contrôle | Action |
| --- | --- |
| WASD physiques | Déplacement ; positions ZQSD sur un clavier AZERTY |
| Souris | Orientation du personnage et indicateur de visée |
| F11 | Plein écran bureau / retour en fenêtre |
| Échap ou fermeture | Quitter proprement |

Le personnage s'arrête ou glisse contre les obstacles. La perte de focus annule les touches maintenues. Le serveur valide les intentions à 30 Hz ; le client prédit puis corrige son déplacement. Le HUD indique connexion, ping, correction, commandes en attente et joueurs visibles. Sans serveur, le client tente de se reconnecter. `--offline` permet de retrouver le prototype local à 60 Hz.

Options utiles :

```powershell
.\out\build\windows-mingw-debug\bin\tchips_client.exe --fullscreen
.\out\build\windows-mingw-debug\bin\tchips_client.exe --map content/maps/prototype.map
.\out\build\windows-mingw-debug\bin\tchips_client.exe --smoke-test
```

Le dernier scénario utilise les événements SDL pour tester automatiquement mouvement/collision, redimensionnement, plein écran et fermeture. Il ouvre brièvement une vraie fenêtre GPU ; il reste séparé des tests headless.

**Démarrer le serveur**, dans un second PowerShell placé sur T: :

```powershell
.\out\build\windows-mingw-debug\bin\tchips_server.exe
```

Ctrl+C l'arrête et nettoie les sessions. Le port UDP par défaut est 27020 ; pour un autre ordinateur, autoriser ce port dans le pare-feu et remplacer l'adresse de connexion par l'IP numérique du serveur. Exécution courte de diagnostic :

```powershell
.\out\build\windows-mingw-debug\bin\tchips_server.exe --ticks 3 --no-sleep
.\out\build\windows-mingw-debug\bin\tchips_client.exe --offline --frames 3
```

`--no-sleep` est réservé aux exécutions bornées de diagnostic, pas au serveur temps réel. Le client `--check-sdl` teste uniquement l'initialisation SDL sans GPU ; ce n'est pas un test graphique.

Le terminal doit conserver les DLL runtime de MinGW accessibles dans son PATH. Les DLL vcpkg nécessaires au client sont déployées près de l'exécutable par l'intégration vcpkg. Cette phase ne fournit pas encore d'installateur autonome.

## Windows — alternative MSVC

Installer Visual Studio Build Tools récent avec le workload C++, SDK Windows et Ninja. Dans un terminal développeur x64, depuis un chemin ASCII vers le projet :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
.\out\build\windows-msvc-debug\bin\tchips_client.exe
.\out\build\windows-msvc-debug\bin\tchips_server.exe
```

Le serveur lancé sans limite occupe son terminal ; lancer les exécutables dans deux terminaux. Ce preset est préparé, mais n'est pas validé sur cette machine dépourvue de MSVC.

## Linux — serveur headless

Installer Git, un compilateur C++23 avec `std::expected` (GCC 13+ recommandé), CMake 3.28+, Ninja, curl, zip/unzip, tar et pkg-config via les paquets de votre distribution. Depuis la racine :

```sh
bash tools/bootstrap.sh
cmake --preset linux-server-debug
cmake --build --preset linux-server-debug
ctest --preset linux-server-debug
./out/build/linux-server-debug/bin/tchips_server
```

Pas de dépendance SDL/affichage dans ce preset. Ctrl+C ou SIGTERM arrête le serveur. Le build Linux doit encore être exécuté sur Linux.

## Dépendances et documents

Les ports obligatoires de la stack sont déclarés dans `vcpkg.json`. Le réseau est activé par défaut : EnTT, SDL3, GoogleTest, GameNetworkingSockets et Protobuf sont requis. Le premier build compile aussi OpenSSL et Abseil et peut être long. UI, audio, SQL et Tracy restent optionnels. Un audit du registre ne prouve pas que toutes les bibliothèques compilent sur toutes les plateformes.

```powershell
cmake -P tools/check-dependencies.cmake
```

- [Architecture et frontières](docs/architecture.md)
- [Phase 01 : modules, carte, contrôles et tests](docs/phase01-client.md)
- [Phase 02 : protocole, simulation, sécurité et tests réseau](docs/phase02-network.md)
- [Vision et classes officielles](docs/GDD.md)
- [Réseau et persistance futurs](docs/network-and-persistence.md)
- [Audit des dépendances et options](docs/dependencies.md)
- [Phases proposées](docs/roadmap.md)
- [Résultats réellement vérifiés](docs/validation.md)
- [Conventions des prochaines sessions](AGENTS.md)

Scénario automatisé avec un serveur et deux vrais clients GPU (latence et pertes simulées) :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test-native-network.ps1
```

Les limites actuelles : une zone, 128 joueurs maximum configurés, aucune authentification ni persistance, personnage provisoire et collisions AABB sans collision entre joueurs. Les performances et les 1 000 joueurs ne sont pas démontrés. Linux/MSVC restent non vérifiés. Voir le journal de validation pour les commandes réellement exécutées.
