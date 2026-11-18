# AGENTS.md — Règles du MMORPG

## Objectif

Développer progressivement un MMORPG 2D natif en C++23, avec un client SDL3/SDL_GPU et un serveur autoritaire.

## Règles fondamentales

1. Ne jamais ajouter de moteur de jeu complet.
2. Ne jamais mélanger le rendu client avec la simulation serveur.
3. Ne jamais faire confiance aux données du client ; valider les intentions côté serveur.
4. Ne jamais implémenter plusieurs systèmes indépendants pour un même besoin.
5. Privilégier les composants réutilisables.
6. Garder les systèmes de gameplay testables sans interface graphique.
7. Utiliser C++23 et RAII.
8. Éviter les pointeurs propriétaires bruts.
9. Préférer les types explicites et les interfaces simples.
10. Ne pas introduire de complexité sans justification.

## Périmètre et méthode

- Le projet repart de zéro en C++23. Ne pas réintroduire les anciens prototypes Go, TypeScript, navigateur ou WebSocket.
- Avant chaque phase : inspecter le code, annoncer le plan, les risques et les tests. N'implémenter que le périmètre demandé.
- Après chaque phase : compiler, exécuter les tests disponibles, corriger les erreurs, mettre à jour les documents et rapporter les limites. Ne jamais annoncer une commande réussie sans l'avoir exécutée.
- Répondre et documenter en français. Identifiants et noms de fichiers de code en anglais, namespace `tchips`.
- Ne pas supprimer ni écraser le travail de l'utilisateur. Aucun secret ni jeton dans le dépôt.
- Travailler par étapes petites et vérifiables. Ne pas modifier les modules sans rapport avec la tâche.
- Ne pas changer silencieusement les règles de gameplay. Toute suppression de code existant doit être justifiée.
- À la fin de chaque tâche, indiquer les fichiers modifiés, les tests réellement exécutés, les limites et la prochaine étape.

## Architecture

- `apps/client/` : rendu, entrées, audio, UI, prédiction et interpolation.
- `apps/server/` : simulation, réseau, combat, monde, économie et persistance.
- `shared/` : mathématiques, géométrie, identifiants et contrats ; `protocol/` : schémas réseau versionnés.
- `content/` : définitions statiques du jeu, distinctes des états persistants et temporaires.
- `tests/` : tests unitaires, d'intégration et de charge.
- Le code partagé ne doit pas permettre au client de décider de la vérité serveur.

## Stack et frontières

- Client desktop et serveur en C++23 ; Windows prioritaire pour le client, Linux pour le serveur.
- SDL3 pour fenêtre/entrées et son API native `SDL_GPU*` pour le rendu, pas SDL_Renderer ni un moteur de jeu complet.
- EnTT pour entités/composants ; GameNetworkingSockets et Protocol Buffers pour le réseau ; RmlUi pour la future UI, miniaudio pour le futur audio.
- PostgreSQL et libpqxx pour la future persistance transactionnelle ; CMake/Ninja/vcpkg pour le build ; GoogleTest pour les tests ; Tracy pour le profiling.
- `shared/` se limite aux types, mathématiques, géométrie et contrats. Ne pas y déplacer la logique des classes, les règles de combat ou la persistance pour partager un moteur.
- Simulation serveur indépendante du rendu, transport, SQL et horloge réelle. Le bootstrap fournit l'horloge à pas fixe ; les systèmes de gameplay seront testables sans SDL.
- Un seul moteur de règles serveur modulaire ; pas de moteur généraliste, de microservices, Redis ou Kubernetes sans besoin mesuré.
- Le client transmet des intentions. Le serveur valide positions, collisions, attaques, dégâts, compétences, recharges, ressources, récompenses, inventaires, fabrication, échanges et progression.

## Simulation

- Utiliser un pas fixe configurable, indépendant du framerate. Les systèmes reçoivent le pas et le tick explicitement.
- Éviter les allocations inutiles dans les boucles critiques ; borner les travaux et files.
- Utiliser des horloges monotones pour les durées ; garder l'horloge réelle hors des règles de simulation.
- Documenter les unités. Distinguer explicitement coordonnées monde et coordonnées écran.

## Réseau

- Protocole versionné et validation stricte des messages.
- Messages fiables pour les transactions ; messages non fiables adaptés aux mouvements et instantanés, avec séquences explicites.
- Numéros de séquence, limites de débit, tailles et files bornées ; rejeter les entrées invalides avant application métier.
- Gérer les déconnexions et nettoyer sessions, entités temporaires et commandes en attente.
- Journaliser les erreurs sans données sensibles.

## Persistance

- PostgreSQL et libpqxx pour les données persistantes ; aucun accès SQL depuis le rendu ni directement depuis les systèmes de simulation.
- Transactions atomiques, opérations idempotentes lorsque nécessaire et migrations versionnées.
- Les requêtes répétées ne doivent jamais dupliquer un objet, une récompense ou une progression.

## Conventions C++ et build

- RAII pour toutes les ressources ; ownership explicite, pas de `new`/`delete` manuels sans nécessité documentée. `std::expected` pour erreurs attendues, erreurs fatales journalisées et code de sortie non nul.
- Cibles CMake avec dépendances `PRIVATE`/`PUBLIC` explicites ; pas de chemins absolus propres à une machine, pas de glob de sources, pas de standard antérieur à C++23.
- Le registre vcpkg est épinglé par `builtin-baseline` dans `vcpkg.json` ; `tools/bootstrap.*` sélectionne le même commit. Ne pas mettre à jour le pin au hasard pour résoudre un build.
- Ne pas modifier `.tools/vcpkg/ports` ni masquer une dépendance manquante. Distinguer port présent, compilation réussie et fonctionnalité intégrée.
- Conserver les presets Windows MinGW, Windows MSVC et Linux serveur. Les triplets MinGW sont communautaires ; leur réussite locale ne prouve pas le support de toute la stack.
- Pas de chrono variable, rendu ou allocations non bornées dans la future boucle de simulation. Prévoir budgets, métriques, limites de files et instrumentation Tracy.
- Les tests ne doivent pas exiger GPU, réseau externe ou PostgreSQL sauf test d'intégration explicitement identifié. Ne pas inclure un test GPU dans les tests CI headless.
- Compiler avec des avertissements élevés ; utiliser les sanitizers lorsque disponibles et documenter les configurations vérifiées.
- Utiliser GoogleTest pour les tests et Tracy pour le profiling. Documenter les décisions ; ne pas ajouter de fonctionnalités non demandées.

## Décisions de jeu

- Monde ouvert exclusivement PvE ; PvP en arènes séparées. Équipements exclusivement fabriqués, monstres fournissant des composants. Objectif de 1 000 joueurs répartis entre zones/instances, à mesurer.
- Classes fixes : Gunner (Pistolero, Exécuteur), Pirate (Corsaire, Capitaine), Viking (Berserker, Jarl), Cartomancien (Arcaniste, Oracle).
- Gunner : deux chargeurs indépendants, recharge manuelle, aucun consommable ordinaire de munitions fabriquées.
- Pirate : Audace, perroquets apprivoisables persistants de plusieurs espèces, un seul actif, IA serveur et aucun farming autonome.
- Viking : Rage, mêlée, zones et résistance. Cartomancien : cartes choisies, aucun tirage aléatoire obligatoire, combos de séquences déterministes.
- Compétences apprises auprès des maîtres, améliorées par objectifs de maîtrise validés par le serveur. Ne jamais accepter de progression déclarée directement par le client.
- Échanges libres et marché/hôtel des ventes, avec validation et transactions côté serveur.
- Périmètre autorisé en phase 02 : serveur autoritaire à 30 Hz, réseau GameNetworkingSockets/Protobuf, déplacement multijoueur, prédiction, réconciliation, interpolation et diagnostics. Ne pas ajouter combat, persistance ou interfaces complexes sans nouvelle demande.
- Le mouvement de la phase 01 est un prototype client hors ligne, pas une simulation autoritaire partagée. Maintenir `tchips_client_core` sans SDL et le rendu en lecture seule sur les états ; ne pas déplacer ces règles dans `shared/`.

## Vérification

Lire `README.md`, `docs/architecture.md` et `docs/dependencies.md`. Après changements C++ : configurer le preset concerné, compiler puis `ctest --preset ...`. Après changement de dépendance : refaire l'audit du registre et signaler ce qui n'a pas été compilé ou exécuté.
