---
name: corgireg-graph-memory
description: Modifier NodeNN et GraphNN, leurs liens, copies et fusions, ou examiner la propriété mémoire des graphes CorGIReg.
---

# Modifier le graphe et sa propriété mémoire

Lire `include/BasicGraphIr/Graph.hpp`, `src/BasicGraphIr/Graph.cpp` et
`test/BasicGraphIr/Graph_test.cpp`. Distinguer le graphe de données de l'automate.
GraphNN tient un vecteur de nœuds et un ensemble d'identités pour la déduplication ;
NodeNN tient des ensembles de parents et enfants par shared_ptr.

Lors d'une modification :

1. Définir l'identité attendue, les liens et la durée de vie. GraphNN::addEdge met
   à jour les deux sens et ajoute les extrémités. NodeNN::addChild/addParent ne
   mettent à jour que leur côté : ne pas changer implicitement ce contrat bas niveau.
2. Pour deepCopy, vérifier la table original → copie, enregistrée avant récursion,
   les cycles, les nœuds partagés et les exclusions qui gardent leur identité.
   Ne pas annoncer une indépendance totale si des nœuds exclus sont réutilisés.
3. Pour mergeNodes, vérifier les liens externes réciproques, les liens internes,
   le vecteur et l'ensemble d'appartenance, ainsi que les collisions de métadonnées.
   L'écrasement actuel dépend du parcours d'un ensemble de pointeurs : ne pas
   promettre un ordre métier stable pour des valeurs concurrentes.
4. Tester getNodeDelta avec signe positif, négatif, zéro et cycle si la traversée
   change. Préserver la stratégie de nœuds déjà vus ou expliciter la nouvelle sémantique.

Les liens forts réciproques peuvent former des cycles de propriété. Une conversion
en weak_ptr nécessite une stratégie de propriétaire et l'examen des API exposées,
pas un remplacement mécanique. shared_from_this exige un propriétaire partagé déjà
établi ; ne pas l'appeler pendant une construction. Pour un correctif de durée de
vie, tester destruction/expiration et, si disponible, un outil mémoire compatible
avec la chaîne. Distinguer un risque lu dans le code d'une fuite mesurée.

Pour std::any, couvrir clé absente, bon type et mauvais type : la surcharge sans
défaut lève runtime_error, celle avec défaut rend le défaut. Ne pas homogénéiser
ces comportements ni les clés sensibles à la casse hors du périmètre demandé.
