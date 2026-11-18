# Journal de vérification

## Phase 02 — serveur autoritaire et réseau

Vérification terminée le **18 novembre 2026**, Windows x64, alias T:, GCC/MinGW 16.1.0, CMake 4.3.2, Ninja 1.13.2 :

- `cmake --preset windows-mingw-debug` : réussi après installation des dépendances réseau. GameNetworkingSockets 1.6.1, Protobuf 7.36.2, Abseil et OpenSSL ont été compilés depuis le registre épinglé. Le premier build des dépendances a pris environ une heure.
- `cmake --build --preset windows-mingw-debug` : réussi pour le serveur headless, le client SDL_GPU, les bibliothèques et les tests. Recompilation après formatage également réussie.
- `ctest --preset windows-mingw-debug` : **46/46 tests réussis**, zéro échec, 15,93 secondes. Comprend les 24 vérifications précédentes, 11 tests de domaine/prédiction/géométrie, 4 de protocole et 7 d'intégration réseau.
- Les intégrations UDP réelles valident deux clients, refus des commandes invalides et résultats client, doublons/hors fenêtre, versions/cartes incompatibles, convergence sous retard sortant de 80 ms, pertes de 15 % et gigue, interruption totale à 100 % de pertes suivie d'une reconnexion, et arrêt/nettoyage serveur.
- `powershell -NoProfile -ExecutionPolicy Bypass -File tools/test-native-network.ps1` : **réussi**, code 0. Un serveur et deux processus clients natifs, GPU `direct3d12`, retard sortant de 60 ms et pertes de 10 % par processus. Chaque client a observé deux joueurs et le mouvement distant, puis un écart autoritaire nul après arrêt ; ping observé de 121 ms. Serveur arrêté après 600 ticks, journal `active=2` puis `active=0`.
- `tchips_client.exe --smoke-test` : non-régression hors ligne **réussie**, code 0, 150 frames, entrées, collision, redimensionnement, plein écran, caméra et fermeture.
- Exécution isolée supplémentaire des 11 tests de domaine/prédiction : réussie avant le build complet.
- `cmake -P tools/check-dependencies.cmake` : réussi, tous les ports obligatoires présents dans le registre épinglé ; les options UI/audio/SQL/Tracy restent non compilées.

Journaux : `out/build/windows-mingw-debug/Testing/Temporary/LastTest.log`, `out/native-network/{server,client1,client2}.{stdout,stderr}.log` et `out/phase01-smoke.*.log`. Ces fichiers locaux sont ignorés par Git. Les commandes, architecture et limites sont dans [phase02-network.md](phase02-network.md).

Résolution rencontrée : la configuration initiale avait lu le CMake racine avant l'ajout de la cible commune de copie de carte et signalait une cible manquante. Une nouvelle configuration après installation a réussi. Une compilation isolée manuelle cherchait initialement `gtest_main` hors du sous-répertoire `manual-link` ; le chemin corrigé puis la liaison officielle CMake ont réussi. Aucun port vcpkg modifié.

Non vérifié : Linux, MSVC, sanitizers, LAN/WAN, longue durée et charge à 1 000 joueurs. Le scénario automatique GPU valide des positions et mouvements rendus par deux processus, mais ne remplace pas une appréciation humaine de la fluidité. Aucune authentification, persistance, économie ou mécanique de combat livrée.

## Phase 01 — client 2D natif

Le 17 novembre 2026, depuis T:, Windows MinGW GCC 16.1.0 :

- `cmake --preset windows-mingw-debug` : configuration réussie, aucune nouvelle dépendance.
- `cmake --build --preset windows-mingw-debug` : core client sans SDL, plateforme GPU, exécutable et tests compilés.
- `ctest --preset windows-mingw-debug` : **24/24 tests réussis**, aucune régression serveur.
- `tchips_client.exe --smoke-test` : **réussi**, backend `direct3d12`, 150 frames, mouvement/collision, redimensionnement, plein écran/retour et fermeture par événement ; code 0.

Les journaux du scénario graphique sont dans `out/phase01-smoke.stdout.log` et `out/phase01-smoke.stderr.log` (ignorés par Git). Le détail des modules, formats et limites est dans [phase01-client.md](phase01-client.md). Linux/MSVC, Vulkan/Metal et sanitizers n'ont pas été exécutés pour cette phase.

## Historique — première mission

Vérification effectuée le **17 novembre 2026**, sur Windows x64, depuis l'alias ASCII `T:` du dossier de travail.

## Environnement observé

- GCC/MinGW 16.1.0, UCRT, POSIX, SEH (WinLibs).
- CMake système 4.3.2 et Ninja 1.13.2.
- vcpkg épinglé à `0df93a05b9de080129e653ec4776d7e14a1b2e4f`, outil 2026-10-29.
- vcpkg a téléchargé ses outils requis, notamment CMake 4.4.4 et PowerShell Core, dans `.tools/`.
- Triplets cible et hôte : `x64-mingw-dynamic`.

## Commandes réellement exécutées

| Commande / scénario | Résultat |
| --- | --- |
| `tools/bootstrap.ps1` via PowerShell | Succès après autorisation réseau ; checkout épinglé et outil vcpkg initialisé. |
| `tools/check-dependencies.ps1` | Neuf ports de la stack trouvés dans le registre. |
| `cmake -P tools/check-dependencies.cmake` | Succès ; versions des neuf ports vérifiées. |
| `cmake --fresh --preset windows-mingw-debug` depuis T: | Succès ; EnTT 4.0.0, SDL3 3.4.20 et GoogleTest 1.18.0 installés. |
| `cmake --build --preset windows-mingw-debug` | Succès ; client, serveur, bibliothèque de configuration et exécutables de tests compilés. |
| `ctest --preset windows-mingw-debug` | **8 tests sur 8 réussis**, aucun échec. |
| `tchips_server.exe --ticks 3` | Succès, horloge réelle active, journal `stopped, ticks=3`, code 0. |
| `tchips_client.exe --frames 3` | Succès, journal `GPU=direct3d12`, `stopped, frames=3`, code 0. |

Le scénario graphique a été lancé avec `Start-Process -WindowStyle Hidden -Wait`, en redirigeant stdout/stderr vers `out/client-smoke.*.log`. Il a effectivement initialisé un périphérique GPU, contrairement au simple test `--check-sdl`.

## Tests exécutés par CTest

1. `Protocol.AcceptsOnlyCurrentVersion`
2. `Entities.DestroyedEntityCannotRemainAlive`
3. `ServerConfig.DefaultsUseFixedClockAndInterruptibleRun`
4. `ServerConfig.AcceptsBoundedDiagnosticRun`
5. `ServerConfig.RejectsMalformedOrUnboundedDiagnosticArguments`
6. `server_bounded_run` — 3 ticks de diagnostic sans sommeil.
7. `server_rejects_invalid_arguments` — argument invalide rejeté avec code non nul.
8. `client_sdl_initialization` — initialisation SDL sans fenêtre/GPU.

Les sorties du test graphique, de CTest et du serveur ne constituent pas un benchmark de fluidité, de stabilité prolongée ou de capacité multijoueur.

## Échecs rencontrés et résolution

- Le sandbox bloquait les téléchargements : les commandes réseau nécessaires ont été relancées avec autorisation.
- Le lancement d'outils sous le chemin accentué `Privé` échouait dans Ninja (`CreateProcess failed`). Le fichier existait ; la configuration depuis `T:` a résolu la détection et les compilations, sans déplacer les sources ni modifier les ports.
- La compilation sous sandbox restait bloquée avant le lancement des compilateurs. Elle a été interrompue, puis exécutée hors sandbox avec autorisation : compilation complète réussie.
- L'audit CMake lisait initialement seulement le champ de version `version`. Il gère désormais aussi `version-semver`, `version-string` et `version-date`, nécessaires à certains ports.
- Le port SDL3 épinglé émet un avertissement amont sur `SDL_UNIX_CONSOLE_BUILD` non utilisé. L'installation et le test GPU réussissent ; le port n'a pas été modifié localement pour masquer cet avertissement.

## Non vérifié / non livré

MSVC n'est pas installé et aucun hôte Linux n'a été utilisé : les presets correspondants restent à compiler/tester. Les ports GameNetworkingSockets, Protobuf, RmlUi, miniaudio, libpqxx et Tracy ont été audités dans le registre, mais leurs options ne sont pas activées ni leurs intégrations annoncées compilées dans cette mission.

Pas de PostgreSQL démarré, schéma métier, réseau actif, authentification, gameplay, UI, audio, capture Tracy ou test à 1 000 joueurs. Le contrat `.proto` n'a pas été généré dans le build minimal. Le serveur est un bootstrap de processus, pas encore un serveur de jeu connecté.
