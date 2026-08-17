# DYN1 — comportamento dinâmico versionado

Status: host design-oracle evidence para `DYN0-G1`. Este estudo não define um
contrato W e não afirma compiler, runtime, provider, FFI, sandbox, execução W,
stress, ou estudo humano/modelo.

DYN1 começa pelo problema do Restaurante no fim do Universo: a cozinha troca
um plugin durante o serviço, a sala mantém snapshots da receita, o observatório
recebe mensagens atrasadas e a conta pode atravessar um reinício. A pergunta é
se as composições atuais fecham essa mudança controlada sem `eval`/`exec`,
monkey patch, escrita de frame ativo, lookup ambiental ou unload de biblioteca
com callback vivo.

| Rota | Problema observado | Disposição da evidência |
| --- | --- | --- |
| A | REPL, snapshots committed, invalidação de dependência e export canônico | Componível com `SessionSnapshot`, `SnapshotCell`, generations e receipts existentes |
| B | plugin/service tipado, `SemanticInterfaceKey`, `WAbiKey`, `RuntimeClosureKey`, troca local/split e drain | Componível no escopo observado; os reducers local e split são independentes |
| C | referência de generation que atravessa restart/deploy | Research estreito `DYN0-persistent-generation-reference`; só facts read-only e migration receipt |
| D | avaliação arbitrária, mutação de fonte/frame/debugger, autoridade por nome, native como sandbox e `dlclose` prematuro | Intencionalmente rejeitado |

O corpus tem 70 casos; o snapshot deriva as contagens de rota/status e três
projeções pareadas. A máquina aceita somente fatos e eventos: recipe,
artifact/index/lock, interface e ABI, runtime closure, source-map/documentation,
schema, target, capabilities, effects, isolation, quotas e receipts. Ela deriva
`prepare → validate → preflight → ready → switch`, fecha a admissão antiga,
invalida completions/messages/capabilities da generation antiga e aplica a ordem
`cancel → drain → unregister → inFlight drain → destroy → unpin → release`.
Process, Wasm e component acrescentam `unmap`; native exact-WAbi retém o mapping
até o fim da runtime island. Um erro antes da publicação preserva a generation antiga; falha de drain
depois da publicação deriva `degraded`, nunca rollback. Rollback só deriva de
provider receipt estruturado antes da publicação. Crash antes da publicação
preserva o antigo como `fault-boundary`, ou deriva `unknown-effect` quando o
provider outcome foi perdido. Crash depois da troca mantém a nova generation
committed, com `degraded` somente se o drain falhar.

`SemanticInterfaceKey`, `WAbiKey`, `RuntimeClosureKey`, artifact/recipe,
source-map e documentation têm papéis separados. Schema `exact` rejeita drift;
`compatible` aceita somente mudanças fechadas e exige candidate
`SemanticInterfaceKey`/`ServiceIRKey` novos, ligados por receipt old/candidate
com decisão de compatibilidade. Target A/B usa facts reais com o mesmo problema,
mas com registry, WAbi e artifact físico distintos. A comparação compartilha o
resultado lógico e pode mudar o trace físico. Nenhuma identidade consulta PATH,
nome, mtime, ambiente ou registry implícito.

Export deriva da publicação source e package/lock digests, receipts, provenance, redactions e
limite de bytes. Import executa `reopen → parse → check → resolveReceipts` e
revalida a fonte. Não restaura heap, task, loan,
capability, `ServiceRef` ou provider handle. O corpus testa receipt stale,
missing, duplicate e forged, source-map stale, digests, quotas e callback/FFI
unload. Process, Wasm e component são isolamentos explícitos; uma biblioteca
native dinâmica nunca é tratada como sandbox.

O caso C não é um inspector comum: inspection read-only de snapshot committed
fica na rota A. A única lacuna isolada é a referência persistente de generation
entre restart/deploy. Mesmo nessa pesquisa, o valor transporta somente digests e
facts de interface; não transporta estado vivo. Isso não rebaixa DYN0 nem cria
syntax. O manifesto liga o resultado CAP0 `DYN0-versioned-change` como
Componível, com cases e snapshot por digest; os dois casos C usam o gate de
Research separado e não promovem std/provider.

## Limite de evidência

`dyn1-versioned-behavior-machine.mjs`, o checker, os testes host e o snapshot
derivam resultados a partir de eventos. `expect` é uma guarda de mutação, não um
seletor de resultado. A comparação local/split usa reducers separados e falha
com `projection-divergence` quando uma mutação muda o resultado lógico. Parse e
symbols são apenas referências source-backed; não são frontend semântico.

As referências oficiais são C23/POSIX, Rust trait objects/`TypeId`/Cargo build
scripts e Python importlib/inspect/eval. Os exemplos de comparação no manifesto
são pseudocódigo original curto e ficam na fila de documentação; não são
citações extensas nem APIs W.

Use os gates limitados:

```sh
bun test tooling/dyn1-versioned-behavior-reference.test.mjs tooling/studies/dyn1-versioned-behavior/oracle.test.mjs
bun tooling/check-dyn1-versioned-behavior.mjs --write
```

O gate não compila ou executa W e não lê `tooling/tree-sitter-w/src/`. Compiler,
runtime, provider, isolamento real, stress, ergonomia humana e estudos de modelo
continuam missing.

HRD0 consome esta evidência para a decisão de runner dev-only. Ele não muda as
rotas DYN1, a lacuna Research de referência persistente ou os mecanismos de
mutação dinâmica rejeitados.
