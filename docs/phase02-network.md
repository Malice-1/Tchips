# Phase 02 — serveur autoritaire et réseau

## Périmètre et architecture

Une zone en mémoire, un serveur C++23 sans rendu, plusieurs clients natifs SDL_GPU. GameNetworkingSockets assure les connexions UDP et Protocol Buffers les messages. Aucun combat ni accès à une base de données.

- `apps/server/world.*` : registre EnTT, sessions/entités, commandes bornées, tick fixe, vitesse, collision et visibilité ; aucun SDK réseau, SDL, Protobuf ou temps réel dans cette simulation.
- `apps/server/network_server.*` : états de connexion, handshake, validation des enveloppes, limites, échanges et nettoyage. Le bootstrap fournit l'horloge monotone et appelle `World::step` à 30 Hz.
- `apps/client/prediction.*` : commandes locales, rejeu après acquittement et interpolation distante, testables sans SDL ou transport.
- `apps/client/network_client.*` : connexion, reconnexion, protocole, transport et réplicas visibles ; GameNetworkingSockets est initialisé par le processus qui possède l'adaptateur.
- `protocol/session.proto` et `protocol/codec.*` : contrat v2 et validation stricte des messages.
- `shared/geometry` : données de carte et balayage AABB extraits de la phase 01, sans règles d'entité, de vitesse, de tick ou d'autorité. Les anciens en-têtes client restent des alias vers cette implémentation unique.

Le partage concerne uniquement la géométrie et les contrats. Le serveur charge sa propre carte et décide de tous les déplacements. Le client ne peut ni modifier cette carte, ni envoyer un résultat de collision ou une position finale. Le rendu reste en lecture seule et dessine les autres joueurs en violet, le joueur local en turquoise.

## Protocole v2

Chaque `Envelope` contient `protocol_version`, le `session_id` attribué par le serveur et exactement un corps. Les messages de la version 1 ne sont pas compatibles. Les données sensibles et les messages bruts ne sont pas journalisés.

| Corps | Sens | Fiabilité | Contenu utile |
| --- | --- | --- | --- |
| Handshake | Client → serveur | Fiable | Empreinte géométrique de la carte locale ; session 0 avant acceptation. |
| ConnectionAccepted | Serveur → client | Fiable | Session, entité locale, état initial, paramètres serveur et tick courant. |
| InputCommand dans InputBatch | Client → serveur | Non fiable, sans Nagle | Séquence, axes de déplacement et angle. Aucun delta temporel ni position. |
| WorldSnapshot | Serveur → client | Non fiable, sans Nagle | Tick, séquence acquittée et états complets des entités visibles. |
| EntitySpawn | Serveur → client | Fiable | Tick et état de l'entité entrant dans la visibilité. |
| EntityDespawn | Serveur → client | Fiable | Tick et identifiant sortant de la visibilité. |
| DisconnectReason | Deux sens selon le cas | Fiable | Code borné : normal, version/carte incompatibles, message invalide, débit, capacité, délai ou arrêt. |

La fermeture du transport complète le code applicatif. Un processus interrompu brutalement ne peut garantir la livraison d'un dernier message : le timeout nettoie alors la session.

Un snapshot non fiable peut arriver avant l'acceptation fiable : le client l'ignore tant que le handshake n'est pas terminé. Les snapshots anciens sont ignorés par tick. Les événements de présence utilisent aussi leur tick : un état antérieur à une disparition ne peut réintroduire l'entité, et un ancien snapshot ne retire pas une apparition plus récente. Les marqueurs de disparition sont bornés et purgés dès qu'un snapshot plus récent les rend inutiles.

L'empreinte FNV-1a porte sur les valeurs géométriques et leur ordre, avec représentation binaire déterministe. C'est un contrôle de compatibilité, pas une preuve d'intégrité du client ou une authentification. Le serveur n'utilise jamais cette empreinte pour faire confiance à ses positions.

## Connexions et cycle de vie

Transport entrant → connexion acceptée par le SDK → attente du handshake → joueur actif. Aucun joueur n'existe avant un handshake valide. Chaque session et chaque entité reçoivent un identifiant monotone distinct pendant la vie du serveur. Une reconnexion crée une nouvelle session et une nouvelle entité ; elle ne restaure pas de progression persistante.

À la fermeture, le serveur détruit l'entité et sa file d'intentions. Les autres observateurs reçoivent sa disparition ; le prochain snapshot complet confirme l'absence. Les slots EnTT réutilisés ne réutilisent pas l'identifiant public.

Le client passe par connexion, négociation, actif, reconnexion ou rejet de compatibilité. Un incident transitoire entraîne une nouvelle tentative après une seconde et efface prédiction, commandes en attente et réplicas de l'ancienne session. Une incompatibilité de version/carte ou un protocole invalide nécessite de corriger la configuration et relancer le client. La perte de connexion est affichée ; il n'y a pas de bascule silencieuse en jeu hors ligne.

## Simulation et commandes

Paramètres initiaux : **30 ticks/s**, **240 unités/s**, demi-taille de collision **12 unités**. Unités monde et écran restent des types distincts. Le serveur normalise les axes et traite au maximum une commande par joueur et par tick : une rafale réseau ne permet jamais de dépasser huit unités par tick. Sans commande, il ne poursuit pas automatiquement le dernier mouvement.

Les séquences sont positives, dans une fenêtre de 120 commandes au-delà du dernier acquittement. Un doublon est ignoré, y compris si son contenu a changé : la première intention reçue gagne. Le réordonnancement dans la fenêtre est accepté dans un tableau borné. Une séquence arbitrairement éloignée est rejetée.

Pour supporter les pertes, le client renvoie jusqu'aux 16 dernières commandes non acquittées dans chaque paquet. Si une commande manquante n'est pas récupérée et que des commandes futures attendent, le serveur patiente trois ticks puis traite la plus ancienne disponible, sans simuler les commandes perdues. L'acquittement peut donc sauter une séquence. Le client retire les commandes acquittées/sautées et rejoue uniquement les suivantes : les pertes ne créent ni déplacement supplémentaire ni trou bloquant indéfiniment le joueur.

Les snapshots sont publiés à 30 Hz. La visibilité dépend uniquement de la position serveur, avec un rayon initial de 1 024 unités ; l'observateur est toujours inclus. Les événements fiables suivent les entrées/sorties de cette visibilité. Cette phase utilise une seule zone et un plafond de 128 joueurs, pas une promesse de capacité mesurée à 1 000.

## Prédiction et présentation

Le client échantillonne ses intentions au même pas de 1/30 s, prédit avec la géométrie locale puis garde chaque intention jusqu'à acquittement. Un snapshot replace la base sur l'état serveur et rejoue la file. L'interpolation du rendu local est conservée ; aucune correction n'est renvoyée comme position au serveur.

Les autres joueurs possèdent un historique borné à 32 états. La présentation vise environ trois ticks (100 ms) derrière le dernier tick serveur estimé, interpole positions et angles et se fige au dernier état lorsqu'il manque des données, sans extrapolation de collision.

Une ligne diagnostique SDL_GPU, également visible en plein écran, affiche état, **PING** en millisecondes, **CORR** (dernière correction en unités monde), **FILE** (commandes non acquittées) et **JOUEURS** visibles. Un ping inconnu est affiché `--`. Une file non vide est normale sous latence ; elle n'est pas à elle seule une désynchronisation. Les journaux du scénario de test mesurent séparément l'écart à l'état autoritaire après arrêt.

## Bornes et sécurité

- Messages client : 4 Kio maximum ; serveur : 32 Kio. Le SDK borne aussi la réception et les octets en attente ; les messages sont libérés par RAII.
- Maximum 16 commandes par lot, axes finis dans [-1,1], angle fini dans [-π,π], séquences positives. Champs Protobuf inconnus, corps interdits et versions incorrectes rejetés.
- Session du paquet obligatoirement égale à celle liée à la connexion. Les commandes ne contiennent pas d'identifiant de joueur cible.
- Seau par connexion : capacité de 120 messages, recharge de 90 messages/s. Budgets de lecture par passage : 512 messages serveur, 256 client. Aucune progression de simulation supplémentaire en fonction de ce débit.
- Connexions en attente bornées ; délai applicatif de cinq secondes sans message/handshake ; timeout SDK connecté de trois secondes.
- Prédiction bornée à 120 commandes ; timeout client de trois secondes sans snapshot. Les réplicas et marqueurs de présence sont également bornés.
- Aucun compte, certificat applicatif, authentification ou autorisation persistante dans ce prototype. Un identifiant de session n'est pas un secret d'authentification.

## Compilation et lancement

Depuis T:, alias ASCII du projet Windows déjà utilisé dans les phases précédentes :

```powershell
cmake --preset windows-mingw-debug
cmake --build --preset windows-mingw-debug
ctest --preset windows-mingw-debug
```

L'option réseau est activée par défaut dans les presets. Le premier build installe GameNetworkingSockets 1.6.1, Protobuf 7.36.2, OpenSSL et leurs dépendances épinglées ; il peut prendre plusieurs dizaines de minutes. Aucun pin de registre n'est changé.

Terminal serveur :

```powershell
.\out\build\windows-mingw-debug\bin\tchips_server.exe --port 27020
```

Deux autres terminaux, chacun lançant :

```powershell
.\out\build\windows-mingw-debug\bin\tchips_client.exe --connect 127.0.0.1:27020
```

Adresse IP numérique requise ; port UDP. Le serveur écoute les interfaces locales. Pour un test LAN, employer l'adresse du serveur et autoriser ce port dans le pare-feu. Les deux processus doivent utiliser la même carte ; `--map PATH` existe sur le serveur et le client.

WASD physiques, souris, F11 et Échap restent inchangés. Le prototype de phase 01 reste disponible avec `--offline` ; `--smoke-test` exécute son scénario hors ligne.

## Tests et simulation réseau

Tests de domaine : deux entités, identifiants, nettoyage, entrées invalides, vitesse/diagonale, plafond par tick, doublons, réordonnancement, trou perdu, collisions et visibilité. Tests de prédiction : acquittement, rejeu, snapshots retardés/perdus/anciens et convergence après arrêt. Tests de codec : tailles, types, séquences et champs inconnus.

Tests GameNetworkingSockets réels sur UDP local : deux connexions, mouvement, départ/reconnexion sans fantôme, résultats client interdits, commandes répétées et trop éloignées, version/carte incompatibles, latence/pertes/gigue, interruption totale et reconnexion automatique. Aucun de ces tests n'initialise SDL ou un GPU.

La simulation SDK de pertes et latence est **globale au processus**, au niveau UDP : les tests la restaurent après usage. L'application expose `--lag-ms N` (retard sortant par processus, 0..1000) et `--loss-percent N` (pertes sortantes, 0..50). Les valeurs extrêmes ne constituent pas une garantie de connexion malgré les timeouts.

Scénario séparé à deux fenêtres GPU, avec serveur borné et journaux sous `out/native-network/` :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test-native-network.ps1
```

Le scénario applique par défaut 60 ms de retard sortant et 10 % de pertes à chacun des trois processus. Les clients se déplacent vers l'obstacle, observent l'autre en mouvement puis s'arrêtent pour vérifier la convergence. Le serveur s'arrête après 600 ticks. Le résultat n'est validé que si les deux clients réussissent et que le serveur nettoie les deux joueurs. Consulter `docs/validation.md` pour distinguer les tests réellement exécutés des procédures proposées.

## Limites et suite

Une zone, pas de persistance, de comptes, de combat ou de collisions entre joueurs. Apparitions espacées sur une grille de points valides, réutilisables si nécessaire. Pas de DNS, de transfert entre zones, de matchmaking ou de migration de protocole à chaud. Le contenu doit être déployé à l'identique sur les deux rôles.

La correction locale peut être visible après une perte importante. Sous surcharge, les boucles abandonnent un arriéré borné plutôt que de simuler un temps illimité ; elles peuvent ralentir. La visibilité est une recherche linéaire bornée et les snapshots sont complets ; profiling et tests de charge restent à faire. Le budget GPU de présentation est augmenté pour plusieurs personnages, mais aucun débit à 1 000 joueurs n'est démontré.

Prochaine étape proposée : validation manuelle sur deux postes, mesures de réseau/CPU, puis affinage de visibilité et compensation de latence dans un périmètre distinct. Aucun besoin de base de données ou de combat n'est introduit ici.
