# Shaders de couleur du prototype

`sdl-3.4.20/` contient, sans modification, les sources HLSL et les variantes précompilées DXIL, SPIR-V et MSL des shaders `test/testgpu/cube.*` de SDL **release-3.4.20**. Licence Zlib reproduite dans `sdl-3.4.20/LICENSE.txt`.

Origine : https://github.com/libsdl-org/SDL/tree/release-3.4.20/test/testgpu

Leur nom d'origine est conservé pour la traçabilité. Le prototype les utilise pour des triangles 2D colorés : position `float3`, couleur `float3`, matrice identité dans l'uniforme vertex 0. La conversion monde → écran → NDC est effectuée sur les copies de présentation CPU, jamais sur les composants de mouvement.

Ces tableaux de bytecode font partie des sources du projet : le build ne dépend pas de l'arbre de travail local de vcpkg. Aucun compilateur de shaders supplémentaire n'est requis. Toute modification future des shaders devra régénérer les variantes correspondantes et conserver leur provenance ; il ne faut pas modifier les tableaux à la main.
