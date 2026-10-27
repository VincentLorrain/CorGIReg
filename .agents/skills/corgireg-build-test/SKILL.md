---
name: corgireg-build-test
description: Configurer, compiler et vérifier CorGIReg avec CMake et Catch2 ; utiliser pour les changements C++, les dépendances et la CI.
---

# Compiler et vérifier

Lire `CMakeLists.txt`, `test/CMakeLists.txt` et `.github/workflows/ci.yml` avant de
modifier le build. Les sources de la bibliothèque sont énumérées explicitement :
un nouveau `.cpp` doit être ajouté à `src_files`. Les tests utilisent un glob récursif.

## Parcours de référence depuis la racine

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build -N
ctest --test-dir build --output-on-failure
```

Privilégier un générateur à configuration unique pour reproduire la CI. Sous Windows,
un générateur MinGW peut être choisi si cette chaîne est installée. Avec un générateur
multi-configuration, conserver aussi CMAKE_BUILD_TYPE=Debug pour le garde actuel,
puis utiliser `--config Debug` au build et `-C Debug` avec CTest.

Vérifier qu'un nombre non nul de tests est découvert. Pour cibler une régression,
choisir un nom effectivement listé avec `ctest -N`, puis utiliser :

```sh
ctest --test-dir build -R "expression" --output-on-failure
```

Exécuter ensuite la suite complète pour un
changement partagé entre domaines. Noter le nombre de tests et les échecs observés.

## Contraintes réelles

- fmt 11.0.2 et Catch2 v3.4.0 sont récupérés par FetchContent : la configuration
  initiale nécessite Git et le réseau, ou des copies locales explicitement fournies
  via `FETCHCONTENT_SOURCE_DIR_FMT` et `FETCHCONTENT_SOURCE_DIR_CATCH2`.
- Le minimum CMake déclaré est 3.12, mais FetchContent_MakeAvailable est apparu en
  3.14 ; la CI demande 3.18.4. Ne pas promettre le support effectif de 3.12.
- Les includes de CorGIReg et fmt sont PRIVATE dans la cible actuelle, alors que
  certains en-têtes exposent fmt. Pour une tâche d'intégration consommateur, vérifier
  un petit client externe avant de choisir les visibilités de cibles.
- Un échec de téléchargement, de compilateur ou de découverte doit être distingué
  d'un échec de test. Ne pas modifier les versions pour contourner un problème réseau.
- Doxygen n'est pas activé dans le CMake racine. Ne pas annoncer sa génération.

Pour une modification uniquement documentaire, contrôler les liens et métadonnées ;
la compilation complète n'est pas une condition artificielle de réussite.
