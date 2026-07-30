# Standard library do W

> **Status:** rascunho source. Não existe build da standard library.

[`W/DESIGN.md`](../DESIGN.md) define os tiers e a semântica. Estes arquivos
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
  cache/
    contracts.w
  database/
    contracts.w
  http/
    contracts.w
  runtime/
    work.w
```

`runtime/work.w` materializa os tipos públicos usados por trabalho
supervisionado. Os outros arquivos materializam values e protocols de T2 para
HTTP, database e cache local com limite.

O rascunho fixa três fronteiras:

- HTTP valida tokens e fields antes de entregar uma mensagem a uma API safe;
- database exige SQL const em parâmetros de chamada, usa binds nomeados, rows
  tipadas e transactions;
- cache local possui capacidade, devolve values owned e nunca vira rede por
  configuração.

Um cache remoto usa um `ServiceRef` async. Um adapter database ou HTTP pode
otimizar transporte, mas não pode mudar statements, results, ownership ou
failure semantics.

`WorkId`, `EffectId`, `Cancellation`, `Request`, `Response`, `http.Context` e
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
