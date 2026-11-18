# Phases proposées

1. **Socle livré** : build C++23 reproductible, client SDL3/SDL_GPU minimal, serveur headless, tests et audit des dépendances.
2. **Phase 01 livrée** : carte locale, personnage temporaire, WASD normalisé, visée, caméra, collisions, plein écran et tests sans GPU.
3. **Phase 02 livrée** : GameNetworkingSockets/Protobuf, handshake, mouvement/collisions autoritaires à 30 Hz, prédiction/réconciliation/interpolation, deux clients natifs et reconnexion testés. Présentation ultérieure : sprites texturés, RmlUi et miniaudio dans des périmètres explicitement demandés.
4. **Première tranche de combat** : compétences par données et premier Gunner autoritaire, chargeurs et projectiles, tests d'unicité et de collisions, aucune autre classe avant validation.
5. **Progression et persistance** : authentification, maîtres, objectifs de maîtrise, transactions PostgreSQL/libpqxx et reprises testées.
6. **Monde et économie** : zones/instances, monstres/composants, métiers interdépendants et fabrication, puis transactions d'échange/hôtel des ventes.
7. **Extensions mesurées** : autres classes, compagnons et cartes, arènes isolées, optimisation sous charge distribuée entre zones.

Chaque phase demande son propre plan, risques, tests et autorisation de périmètre. Cet ordre est une proposition de travail, pas une implémentation anticipée ni une promesse de capacité.
