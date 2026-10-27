# Tests CorGIReg

Catch2 v3.4.0 avec Catch2WithMain : ne pas ajouter un second main.
Les sources `*.cpp` sont collectées récursivement dans `test/CMakeLists.txt`.
Lire `$corgireg-build-test` pour exécuter et vérifier la découverte des tests.

- Suivre les TEST_CASE/SECTION et les assertions REQUIRE existants. Utiliser des
  assertions sur les résultats, les liens ou les exceptions ; une sortie console
  ou un export Mermaid ne constitue pas une assertion.
- Pour un bogue, construire un petit graphe explicite avec résultat attendu, et un
  contre-exemple. Ne pas utiliser uniquement PGInterpreterIR comme oracle du moteur :
  les deux interprètes peuvent partager la même erreur de syntaxe.
- NodeDictionary est un singleton sans reset public. Utiliser des clés propres au
  test et vérifier les retours d'enregistrement lorsque c'est l'objet du test.
  Ne pas présumer que les SECTION isolent les statiques du processus.
- Ne pas dépendre des adresses de shared_ptr, des compteurs globaux ou de l'ordre des tests.
  Pour un test aléatoire, conserver une graine reproductible et afficher l'entrée en échec.
- Tester les changements du langage au niveau lexer/parser, des interprètes concernés,
  puis du résultat public dans `test/App` si le matching est affecté.
- Un filtre sans correspondance ou un test commenté n'est pas une couverture.
