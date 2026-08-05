# Standard library do W

> **Status:** rascunho source. Não existe build da standard library.

[`DESIGN.md`](../DESIGN.md) define os tiers e a semântica. Estes arquivos
testam se os contratos podem ser escritos em W.

## Camadas

| Camada | Conteúdo |
|---|---|
| T0 | tipos e operações necessários para compilar W e executar o core |
| T1 | process, files, network, tasks, services e integração comum de host |
| T2 | HTTP, database, SI, tensor, accelerator, crypto e domínios maiores |

Um tier não define distribuição separada. O SDK pode enviar todas as camadas.
O product inclui somente o grafo alcançável.

## Estrutura inicial

```text
std/
  abort/
    contracts.w
  build/
    contracts.w
  cache/
    contracts.w
  database/
    contracts.w
  http/
    contracts.w
  io/
    contracts.w
  runtime/
    task.w
    transaction.w
    work.w
    workflow.w
  stream/
    contracts.w
  url/
    contracts.w
```

`build/contracts.w` materializa bindings de transforms herméticas.
`runtime/task.w` materializa reasons, budget kinds, outcomes e timeout de tasks
lexicais. `Duration` é um intrinsic T1 signed e exato, com resolução de
nanosecond. `runtime/transaction.w` materializa o contrato de transação
estruturada. `runtime/work.w` materializa os tipos públicos usados por trabalho
supervisionado. `runtime/workflow.w` materializa effect policies, waits e event
delivery de workflows por steps. Uma suspensão pública contém duração restante,
não o alarm privado do adapter. `io/contracts.w` materializa byte I/O de T1.
`stream/contracts.w` materializa o carrier readable do profile Web como owner
move-only que atende diretamente a `Stream` e, para bytes, a `ByteSource`. Tee
sempre recebe limite de lag. Item count é estrutural e usa o allocation budget.
O overload de bytes possui bound exato em bytes. O provider intrinsic interno
`std.readable-stream@1` continua missing. O arquivo fecha a interface, não cria
um runtime paralelo nem alega execução. `from` pode pagar box e indirection. O
handle tipado preserva `Item` e `Failure`. `cancel` consome o owner também em
Failure, deixa o handle inert e mantém o drain no root estruturado.
`abort/contracts.w` materializa o adapter Web de aborto. `AbortSignal` é um
handle de observação duplicável, `AbortController` é move-only e o primeiro
reason `Copy` bounded permanece estável. Timeout usa timer-resource monotônico
independente do creator/root. `any` limita primeiro os argumentos diretos e
depois as folhas pending únicas após flatten/dedup. Esse fan-in é por result; o
total vivo usa o allocation/admission budget do provider. Nenhum signal concede
authority ou vira `WireValue`. O provider intrinsic `std.abort-state@1`
continua missing.
`url/contracts.w` materializa os values portáteis de URL e parâmetros. Seu
provider intrinsic interno `std.url-record@1` segue o mecanismo da seção 19.3.1
e precisa implementar o URL Standard completo. A interface está em draft, mas
o provider executável continua missing; o arquivo não contém um parser
substituto. `URL` mantém backing canônico opaco, oferece views textuais O(1) e
materializa snapshots owned de `URLSearchParams` somente por call explícita.
`editSearchParams` mantém a mutação do URL scoped. Os outros arquivos
materializam values e protocols de T2.

`std.process` é um módulo T1 planejado. Ele fornece `Arguments`, `Context`,
`ExitCode`, `Signal` e o registry de signals. Named imports são recomendados.
Um namespace alias, como `process.Arguments`, continua válido. O módulo não
fornece um singleton global chamado `process`. Os SDKs de target podem
fornecer namespaces como `std.device`, `std.mobile` e `std.audio`. Seus tipos
participam das assinaturas dos handlers que um product liga por `hostBindings`.
Os nomes dos slots pertencem ao host profile, não a esses módulos.

O rascunho fixa nove fronteiras:

- build transforms recebem somente inputs e outputs declarados;
- workflows persistem points e outcomes, não task frames;
- I/O preserva short progress, borrows e cancellation até completion;
- streams readable mantêm um cursor e um pull em voo. Item lag é estrutural e
  byte lag é bounded em bytes. BYOB reutiliza `ByteSource`, sem reader object;
- abort signals espelham cancellation em boundaries Web, mas não substituem o
  control outcome de task. Controller drop não aborta e composição é bounded;
- HTTP valida tokens e fields antes de entregar uma mensagem a uma API safe;
  `Method.query` representa o método QUERY do RFC 10008;
- URL preserva serialização canônica, snapshots explícitos e edição live scoped
  de parâmetros, sem conceder network ou filesystem authority;
- database exige SQL const em parâmetros de chamada, usa binds nomeados, rows
  tipadas e transactions;
- cache local possui capacidade, devolve values owned e nunca vira rede por
  configuração.

`ByteSink.writeMany` usa segments borrowed e um fallback sem allocation.
Scatter read e transferência zero-copy permanecem em **Pesquisa**.

`StepEffect.atMostOnce` é o default seguro. Retry só ocorre quando o effect
contract permite. Timer e event wait não mantêm um worker ativo.

Um cache remoto usa um `ServiceRef` async. Um adapter database ou HTTP pode
otimizar transporte, mas não pode mudar statements, results, ownership ou
failure semantics.

`Duration`, `Task`, `Deadline`, `Cancellation`, `CancellationId`, `WorkId`,
`WorkflowPointId`, `EffectId`, `EventId`, `WorkContext`, `StepContext`,
`build.Context`, `ProcessContext`, `Request`, `Response`, `http.Context` e
handles de capability ainda são intrinsics do compiler/runtime.

Não serão criadas classes utilitárias quando uma operação pertence ao próprio
tipo. Exemplos:

- construção incremental pertence a `String`;
- projections read-only usam `view T`;
- IDs não concedem authority;
- handles de capability possuem os métodos que autorizam.

## Regra de promoção

Um módulo entra na std quando:

1. possui semântica no design;
2. possui ao menos um consumer no produto Última Luz;
3. possui oracle positivo e negativo;
4. tem custo, error, cancellation e target profile definidos;
5. não depende de uma implementação específica sem adapter.

O produto de referência pode usar uma API antes da promoção. Nesse caso, a API
permanece uma requirement de pesquisa e aparece no gate de build.
