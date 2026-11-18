# Vision de jeu et classes officielles

## Monde et combat

MMORPG 2D pixel art, vue de dessus. Lisibilité inspirée de Hero Siege, réactivité du combat inspirée de Battlerite. Déplacement WASD, visée souris, compétences directionnelles et projectiles. Monde ouvert persistant exclusivement PvE, donjons instanciés et arènes PvP séparées. Objectif de 1 000 joueurs simultanés distribués entre zones/instances, à tester par la charge.

Classe fixe à la création. Maîtres de classe pour apprendre les compétences ; objectifs de maîtrise pour leurs améliorations. Progression importante par grinding. Récolte, transformation, fabrication et économie liées aux joueurs, échanges libres et hôtel des ventes. Les monstres fournissent des composants, jamais d'équipements complets ; les équipements sont fabriqués par les joueurs.

## Classes

| Classe | Identité et décisions acquises | Spécialisations |
| --- | --- | --- |
| Gunner | Deux pistolets, deux chargeurs indépendants, recharge manuelle ; tirs ordinaires sans consommation de munitions fabriquées ; rafales, esquives et précision | Pistolero, Exécuteur |
| Pirate | Sabre majoritaire, pistolet occasionnel, Audace ; perroquets apprivoisables dans le monde, identités et progressions individuelles persistantes, un seul actif | Corsaire, Capitaine |
| Viking | Mêlée, Rage générée au combat, armes lourdes ou doubles armes, zones et résistance | Berserker, Jarl |
| Cartomancien | Cartes choisies par le joueur, aucun tirage aléatoire obligatoire, combinaisons et séquences déterministes avec effets spéciaux | Arcaniste, Oracle |

Les perroquets seront simulés côté serveur, sans farming autonome. Les cartes et leurs séquences seront validées par le serveur. Ni ces systèmes ni les classes ne sont implémentés dans cette première mission.

## Décisions à prendre dans les phases concernées

Capacités/durées/cadences des chargeurs, règles d'alternance et d'interruption ; génération/dépense d'Audace et Rage ; acquisition et changement de spécialisation ; fenêtres/consommation des combos ; apprivoisement, participation et défaite des compagnons ; seuils/rangs et récompenses de maîtrise ; métiers, emplacements et statistiques d'équipement.

Ces paramètres seront pilotés par les données et versionnés. Aucun réglage chiffré ou système du prototype antérieur n'est repris implicitement. Les règles client ne décideront jamais d'un gain de progression ou de récompense.
