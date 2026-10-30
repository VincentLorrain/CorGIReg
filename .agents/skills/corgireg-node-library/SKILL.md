---
name: corgireg-node-library
description: Créer ou compléter une bibliothèque de définitions de nœuds CorGIReg (JSON schéma 3, op_types par format, règles where, symbole OpenMath) à partir d'un modèle ONNX, StableHLO ou d'un graphe JSON.
---

# Construire une bibliothèque de nœuds

Lire `doc/node-library.md` (schémas 1 à 3, section OpenMath) et `doc/node-rules.md`
si une condition `where` est nécessaire. Une définition est un fichier JSON qui
classe des opérations sources ; elle ne décrit ni sous-graphe ni équivalence numérique.

## 0. Décider avec quoi la bibliothèque sera chargée

Demander, ou déduire de la demande, si la bibliothèque sera chargée **seule** ou
**avec** d'autres (par exemple `examples/library/nodes`). Les bibliothèques chargées
ensemble ne peuvent pas définir deux fois le même nom (`Duplicate node definition`).
- Seule : définir toutes les opérations utiles, y compris `add` ou `conv`.
- Avec d'autres : ne définir que ce qui manque. Une définition existante qu'on ne
  peut pas modifier se complète par une nouvelle définition au nom distinct
  (`conv_custom`), pas par un fichier du même nom.

Dans les commandes suivantes, `LIBS` désigne les options `--node-library` des
bibliothèques chargées avec la nouvelle (rien si elle est seule).

## 1. Inventorier les opérations

Depuis la racine, avec le worker compilé (`$corgireg-build-test`) :

```sh
PYTHONPATH=python python3 .agents/skills/corgireg-node-library/inventory.py MODELE \
  LIBS --engine build/corgireg-match
```

Le script liste chaque nom d'opération original, son nombre de nœuds et les
définitions qui le couvrent déjà (`conditional` : toutes ces définitions ont un
`where`, la couverture dépend donc des attributs).
`--format` est déduit de l'extension (`.json`, `.onnx`, `.mlir`). ONNX demande le
paquet `onnx`, StableHLO des liaisons MLIR : en cas d'échec d'import, le signaler
sans inventer la liste des opérations.

## 2. Regrouper et nommer

- Une définition par famille sémantique (`add`, `conv`), pas par spelling d'IR.
  Compléter une définition existante plutôt que créer un doublon : un nom en double
  dans les bibliothèques chargées ensemble est une erreur.
- Nom : `[A-Za-z0-9_]+` (obligatoire), stable ; la convention du dépôt est en
  minuscules. Il devient la clé des motifs.
- `op_types` : noms originaux exacts, avant normalisation (`stablehlo.add`, `Add`,
  `domaine.Op` pour un domaine personnalisé, par exemple `com.vsora.Gelu`).
- Formats : ne renseigner que les formats que l'utilisateur importera, avec des
  spellings observés dans ses modèles ou vérifiés dans la spécification de l'IR.
  Pour `json`, lister les spellings réellement présents dans les graphes visés.
  Ne pas inventer de mapping ONNX/StableHLO pour un modèle seulement JSON.
- Contrainte d'attribut, de nom ou d'argument : règle `where` du format concerné.
  Un attribut absent du nœud ne satisfait pas `eq` : les valeurs par défaut ONNX ne
  sont pas complétées. Le dire dans la description, ou garder aussi une définition
  sans `where` (`conv` à côté de `conv3x3`).
- Une opération unique, même personnalisée (`com.vsora.Gelu`), est une définition ;
  une composition de plusieurs opérations (Gelu décomposé…) relève d'un motif.

## 3. Écrire la définition

Un fichier `<nom>.json` par définition, dans le répertoire de bibliothèque choisi
par l'utilisateur (pas de récursion dans les sous-répertoires) :

```json
{
  "schema_version": 3,
  "name": "add",
  "description": "Elementwise addition; dtypes and broadcasting are not constrained.",
  "openmath": {"cd": "arith1", "name": "plus"},
  "formats": {
    "onnx": {"op_types": ["Add"]},
    "stablehlo": {"op_types": ["stablehlo.add"]},
    "json": {"op_types": ["Add", "stablehlo.add"]}
  }
}
```

Utiliser le schéma 3 pour toute nouvelle définition. La description dit ce qui
n'est pas vérifié (attributs, types, diffusion).

## 4. Choisir le symbole OpenMath

`openmath` donne le sens indépendant de l'IR : CD et nom de symbole d'un
dictionnaire officiel (`https://openmath.org/cd/<cd>.ocd`). Il ne change pas le
matching. Ne l'ajouter que si le sens est exact, y compris l'arité :

| Opération | Symbole |
| --- | --- |
| Add / Sub / Mul / Div | `arith1.plus` / `arith1.minus` / `arith1.times` / `arith1.divide` |
| Neg / Abs / Pow | `arith1.unary_minus` / `arith1.abs` / `arith1.power` |
| Exp / Log (népérien) | `transc1.exp` / `transc1.ln` |
| Sin, Cos, Tan, Tanh… | `transc1.sin`, `transc1.cos`, `transc1.tan`, `transc1.tanh` |
| Floor / Ceil | `rounding1.floor` / `rounding1.ceiling` |
| Equal, Less… | `relation1.eq`, `relation1.lt`, `relation1.gt`, `relation1.leq`, `relation1.geq` |
| And / Or / Not / Xor | `logic1.and` / `logic1.or` / `logic1.not` / `logic1.xor` |

Omettre `openmath` lorsque :
- aucun symbole standard n'existe (convolution, Relu, Sigmoid, MatMul, Reshape) ;
- l'arité ou les paramètres diffèrent : `arith1.root` prend le degré, donc pas de
  symbole pour `Sqrt` unaire ;
- le comportement diffère (mode d'arrondi de `Round`, `Mod` selon le signe) ;
- le symbole n'a pas été vérifié dans son CD. En cas de doute, l'omettre et le dire.

`cdbase` sert uniquement aux CD non officiels. Ne pas mettre le symbole dans
`op_types` : un nœud source nommé `arith1.plus` ne serait pas sélectionné par lui.

## 5. Vérifier

`DIR` est la nouvelle bibliothèque, `TMP` un répertoire temporaire de travail.

1. Validation native, avec les bibliothèques chargées ensemble (doublons inclus) :
   `build/corgireg-match --node-library DIR LIBS --list-definitions`
2. Couverture : relancer l'inventaire avec `--node-library DIR LIBS` ; chaque
   opération voulue doit avoir une définition.
3. Génération, uniquement avec les **nouvelles clés sans `where`** : une clé
   inconnue reste silencieusement un nom d'opération brut (code 0), et une clé
   conditionnelle est refusée (code 2). Vérifier dans le JSON produit que chaque
   clé est devenue son premier `op_type` :
   `build/corgireg-generate -r 'cle1->cle2' --node-library DIR LIBS -o TMP/g.json`
4. Matching réel sur le modèle, avec un motif utilisant les nouvelles clés, y
   compris conditionnelles. Sans CLI `corgireg` installée :

   ```sh
   PYTHONPATH=python python3 -m corgireg_cli -i MODELE -r 'cle1->cle2' \
     --node-library DIR LIBS --engine build/corgireg-match -o TMP/report.json
   ```

   Vérifier dans `node_definitions` les `node_ids` attendus et `openmath`, puis un
   contre-exemple qui ne doit pas correspondre (`conv3x3->conv3x3` si un seul 3x3).

Pour une bibliothèque ajoutée sous `examples/library`, mettre à jour son README et
les attentes de `test/NodeTest/NodeLibrary_test.cpp`, puis exécuter la suite
complète. Rapporter les opérations restées sans définition, les symboles omis avec
la raison, et les symboles repris de la table sans vérification dans leur CD.
