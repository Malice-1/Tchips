# Contenu piloté par les données

Cet espace accueillera les définitions immuables/versionnées de cartes, classes, spécialisations, compétences, ressources, objets et recettes lorsqu'une phase les demandera.

La phase 01 charge `maps/prototype.map` et `sprites/temporary.sprite`. Leurs formats sont versionnés et validés par les modules WorldMap et AssetManager ; voir `docs/phase01-client.md`. Le build copie ces fichiers dans `bin/assets/` ; relancer le build après modification, ou utiliser `--map` pour charger directement une autre carte.

Les classes et règles acquises restent documentées dans `docs/GDD.md`. Ne pas ajouter un parseur généraliste ou une base métier avant d'avoir un format et un consommateur autorisés.

Les futures définitions seront séparées des états persistants et des composants temporaires EnTT ; aucun code exécutable ou secret dans les données.
