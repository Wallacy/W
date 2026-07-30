# Deployment plans do Última Luz

> **Status:** source de design. O CLI ainda não gera `deployment.lock`.

Estes planos testam a separação entre grafo lógico, packing e placement.
[`W/DESIGN.md`](../../../DESIGN.md) define a semântica.

- [`local.w`](local.w) usa uma unit e adapters locais de desenvolvimento.
- [`distributed.w`](distributed.w) usa vários artifacts e hosts.

O artifact index já fixa as edges entre as units de `restaurant-core`.
O plano só escolhe a rota entre os placements. A seção `bindings` satisfaz
imports abertos do grafo; ela não religa providers internos.

Um plano usa recipes e releases legíveis. O futuro comando `w deploy resolve`
deve trocar essas referências por digests no lock.

O plano não contém secrets. Ele também não autoriza build durante
`w deploy apply --locked`.
