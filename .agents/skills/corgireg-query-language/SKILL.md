---
name: corgireg-query-language
description: Modifier le langage de motifs de CorGIReg, son lexer, son parser, son AST et ses interprètes IR et TG.
---

# Faire évoluer le langage de graphes

Lire les fichiers correspondants dans `include/CompileTool/CompileTransitionGraph`
et `src/CompileTool/CompileTransitionGraph`, ainsi que les tests homonymes.
Le pipeline est PartitionGraphLexer → PartitionGraphParser → AstNode, puis deux
consommateurs : PGInterpreterIR pour GraphNN et PGInterpreterTG pour le moteur.

Avant d'éditer, écrire quelques entrées acceptées/rejetées et le résultat attendu.
Les opérateurs existants incluent `->`, `<-`, `+`, `*`, `?`, `;`, les parenthèses,
les clés communes `A#`/`A#1`, strictes `A$1` et le marqueur de fin `$`.
Vérifier le lexer pour les ambiguïtés de suffixe ; les commentaires des enums
ne suffisent pas à définir la grammaire.

Le parser est récursif et construit des nœuds dont l'ordre et l'arité sont consommés
par les deux visiteurs. Les groupes parenthésés passent actuellement par
constructAstDomain, qui exige un quantificateur après `)`. Ne pas importer implicitement
la grammaire des regex de texte ou supposer l'existence d'un opérateur `|`.

Pour un token ou une production nouvelle, suivre les impacts : enum, reconnaissance,
consommation, AST, deux visiteurs et erreurs. Si une construction est propre à un
interprète, expliciter ce choix et tester le rejet dans l'autre plutôt que l'ignorer.
Préserver le comportement de reset après parse réussi ; tester les appels répétés
si le cycle de vie du parser ou de l'interprète change.

Vérifier token/lexème, structure AST, graphes produits, entrée tronquée, suffixe
invalide et résultat final pertinent dans test/App. Ne pas définir le succès seulement
par l'absence d'exception. Pour changer les messages, préserver le contexte utile
de la requête. Charger `$corgireg-matching` si la sémantique des transitions change.
Le langage des prédicats de NodeTest est distinct ; utiliser son skill pour celui-ci.
