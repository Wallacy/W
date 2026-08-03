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
```

`build/contracts.w` materializa bindings de transforms herméticas.
`runtime/task.w` materializa reasons, budget kinds, outcomes e timeout de tasks
lexicais. `Duration` é um intrinsic T1 signed e exato, com resolução de
nanosecond. `runtime/transaction.w` materializa o contrato de transação
estruturada. `runtime/work.w` materializa os tipos públicos usados por trabalho
supervisionado. `runtime/workflow.w` materializa effect policies, waits e event
delivery de workflows por steps. Uma suspensão pública contém duração restante,
não o alarm privado do adapter. `io/contracts.w` materializa byte I/O de T1.
Os outros arquivos materializam values e protocols de T2.

`std.process` é um módulo T1 planejado. Ele fornece `Arguments`, `Context`,
`ExitCode`, `Signal` e o registry de signals. Named imports são recomendados.
Um namespace alias, como `process.Arguments`, continua válido. O módulo não
fornece um singleton global chamado `process`. Os SDKs de target podem
fornecer namespaces como `std.device`, `std.mobile` e `std.audio`. Seus tipos
participam das assinaturas dos handlers que um product liga por `hostBindings`.
Os nomes dos slots pertencem ao host profile, não a esses módulos.

O rascunho fixa seis fronteiras:

- build transforms recebem somente inputs e outputs declarados;
- workflows persistem points e outcomes, não task frames;
- I/O preserva short progress, borrows e cancellation até completion;
- HTTP valida tokens e fields antes de entregar uma mensagem a uma API safe;
  `Method.query` representa o método QUERY do RFC 10008;
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
