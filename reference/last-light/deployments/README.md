# Deployment plans do Última Luz

> **Status:** source de design. O CLI ainda não gera `deployment.lock`.

Estes planos testam a separação entre grafo lógico, packing e placement.
[`W/DESIGN.md`](../../../DESIGN.md) define a semântica.

- [`local.w`](local.w) usa uma unit e adapters locais de desenvolvimento.
- [`distributed.w`](distributed.w) usa venue, edge, satellite, horizon e device
  artifacts em vários hosts.
- [`benchmark.w`](benchmark.w) fixa HTTP, PostgreSQL e cache local para o
  corpus TechEmpower.

O artifact index grava cada service identity, default provider e override
policy. A seção `bindings` destes planos satisfaz os imports abertos. Um plano
também poderia trocar uma binding `.startup` dentro do envelope declarado.

O resolver seleciona um `ServiceLink` permitido pelo runtime graph. Co-location
usa `.local`. Uma component boundary usa `.component`. IPC e network usam
`.wrpc` na baseline. Um foreign RPC exige um adapter autorizado por digest.
`ServiceTransport` não representa essas quatro opções. Ele carrega frames
somente dentro do link wRPC.

Os paths de deployment nomeiam artifact e import do grafo. O source usa IDs
tipados criados por `export service` ou `import service`. Ele não conhece esses
paths.

A seção `limits.execution` reduz o execution profile de cada unit. Ela não
altera domains, pools, capabilities ou fallbacks:

- o plano local usa uma CPU lógica para o scheduler adversarial;
- o plano distribuído dá budgets distintos a gateway, planning, finance,
  dining, edge e observatory;
- o plano de benchmark mantém o envelope alto, mas continua bounded.

Cada valor precisa ser menor ou igual ao máximo de `package.w`. O futuro
`deployment.lock` grava a redução, link, codec, transport, peers e profile
digest de cada edge.

A seção `adapters` satisfaz roles fechadas pelo artifact. Os planos local e
distribuído selecionam `w.std/sqlite-workflow@1` para o journal de
`fulfillment`. O lock futuro grava seu digest. Um adapter em memória não atende
o requisito `recovery: .required`. O host também precisa fornecer storage com
encryption at rest para satisfazer `.hostEncrypted`.

Um plano usa recipes e releases legíveis. O futuro comando `w deploy resolve`
deve trocar essas referências por digests no lock.

O plano não contém secrets. Ele também não autoriza build durante
`w deploy apply --locked`.
