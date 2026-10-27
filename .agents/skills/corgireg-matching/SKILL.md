---
name: corgireg-matching
description: Modifier les automates et le matching CorGIReg, les transitions, les contextes, les nœuds communs ou les solutions.
---

# Préserver la sémantique du matching

Lire les interfaces et implémentations de State, Transition, TransitionGraph,
RunTimeContext et MatchSolution dans `include/TransitionGraph` et `src/TransitionGraph`.
Lire aussi PGInterpreterTG pour comprendre comment le motif construit l'automate.
La frontière publique à vérifier est TGCompiled::test, avec un vecteur de départs,
et les résultats SolutionFromStartingPoint/MatchSolution.

Tracer sur un petit graphe les états de départ/validation, les origines, les groupes,
la direction, les nœuds validés par clé et les nœuds communs avant de refactorer.
L'ordre des départs est significatif : ne pas remplacer le vecteur par un ensemble.
Ne pas assimiler fusionG, unionG et mergeOneStartOneValid sans comparer leur effet.

Points à contrôler selon la zone modifiée :

- State garde des références faibles ; Transition possède ses états. Vérifier les
  mises à jour des relations lors des fusions et reSetSourceState/reSetDestState.
- EMPTY et NNULL ont des rôles distincts. Vérifier séparément progression sans test,
  fin de graphe, direction parent/enfant et références vers un nœud commun.
- Les contextes portent des validations et identités de nœuds communs ; leur
  compatibilité autorise les communs cohérents et refuse les chevauchements non communs.
  Préserver les origines requises par la composition des sous-motifs.
- Examiner CommonMode NONE/FRONT/BACK/ALL et les rejets dans Transition.cpp, sans
  déduire une sémantique seulement des noms.
- RunTimeContext::mRejectedNodes et TransitionCommon::mCommonIdxMap sont statiques ;
  State a un compteur statique. Toute modification de leur cycle de vie exige des
  essais de requêtes et compilations successives. Aucun support multithread n'est établi.

Choisir des régressions liées au changement : succès/refus, branchement, direction
inverse, quantificateur vide, nœud commun cohérent/incohérent, cycle et appels répétés.
Comparer les identités et ensembles de résultats, pas seulement isMatch si les
captures sont affectées. Pour les performances, mesurer sur un cas reproductible
avant d'ajouter cache ou déduplication qui pourrait supprimer des chemins valides.
