---
name: corgireg-node-predicates
description: Modifier les conditions de nœuds, le registre NodeDictionary et les expressions de prédicats CorGIReg.
---

# Modifier les prédicats

Lire les interfaces et implémentations de `NodeTest` : NodeDictionary,
NodeCondition, PredefinedNodeCondition, Expression, ExpressionParser et FunctionBuilder.
Les tests principaux sont `test/NodeTest/NodeDictionary_test.cpp` et
`test/NodeTest/FunctionBuilder_test.cpp`. Ce parser de prédicats est distinct du
PartitionGraphParser des motifs de graphes.

NodeDictionary expose un singleton et des callbacks bool sur shared_ptr<NodeNN>.
L'enregistrement refuse les clés hors `[a-zA-Z0-9_]+` et les doublons (retour false) ;
at lève runtime_error pour une clé inconnue. Préserver ces différences ou documenter
et tester explicitement l'évolution d'API.

Avant un changement d'expression, vérifier dans ExpressionParser.cpp les priorités
AND/OR, les parenthèses, appels, chaînes et comparaisons réellement reconnus.
Examiner les callbacks construits et le court-circuit dans Expression.cpp.
Ajouter des cas qui différencient les priorités et les effets de court-circuit si
ces mécanismes changent ; tester une entrée invalide et les attributs absents ou mal typés.

Respecter les types std::any et la casse des clés utilisés par les prédicats.
Pour un nouveau prédicat, tester directement le callback et un motif qui l'utilise
si son intégration au registre est concernée. Ne pas introduire d'état global caché
dans une capture de lambda. Les tests doivent tenir compte du registre persistant
pendant le processus et ne pas dépendre de l'ordre d'exécution.

Si l'API publique change, suivre les usages dans Transition.cpp et test/App ;
charger `$corgireg-matching` pour une modification de la validation des transitions.
