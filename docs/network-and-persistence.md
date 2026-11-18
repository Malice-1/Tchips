# Contrats réseau et persistance

La phase 02 active la génération Protobuf et le transport GameNetworkingSockets par défaut. Voir [phase02-network.md](phase02-network.md) pour les limites, la fiabilité par message et les cycles de connexion. La persistance reste future.

## Versionnement

La constante partagée `tchips::contracts::protocol_version` vaut 2. `protocol/session.proto` décrit le handshake, l'acceptation, les intentions numérotées, les snapshots, apparitions/disparitions et motifs de déconnexion dans `tchips.protocol.v2`.

Le serveur vérifie version et empreinte géométrique de la carte avant de créer une session. Les numéros de champs Protobuf retirés seront réservés ; un changement incompatible exigera une nouvelle version, sans recycler les identifiants.

## Transport

GameNetworkingSockets assure les connexions UDP. Les événements critiques sont fiables ; les intentions redondantes et snapshots sont non fiables à 30 Hz, avec séquences explicites. Aucune garantie de latence ou de topologie Steam n'est supposée.

Le client envoie uniquement direction et angle de visée numérotés. Il n'envoie jamais de position finale. Le serveur contrôle session, finitude/bornes des valeurs, collisions, séquence et quotas. Les tailles et files sont bornées. Les validations de combat et d'économie seront ajoutées uniquement avec ces systèmes.

## Agrégats persistants futurs

Séparer définitions de contenu versionnées, états durables (identité, classe, apprentissage, maîtrise, compagnons, inventaires, économie) et états de zone temporaires (positions de simulation, activations, projectiles, statuts). La liste précise des positions à sauvegarder sera une décision de phase.

PostgreSQL/libpqxx sera un adaptateur serveur, sans SQL partagé avec le client ou dans les systèmes de simulation. Opérations de craft, échange, récompense et progression : transaction atomique, contraintes d'unicité et d'appartenance, version d'état et identifiant idempotent. Les données publiques d'un personnage ne devront pas exposer son inventaire privé ou les informations de session.

Aucun serveur PostgreSQL n'est démarré, aucun schéma métier ou migration n'est créé et aucune durabilité n'est revendiquée dans cette première mission.
