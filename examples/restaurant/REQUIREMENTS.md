# Requisitos derivados do restaurante

> **Working Draft · corpus top-down, não implementação disponível**

Este documento converte o pseudocódigo do restaurante em trabalho verificável.
Ele não declara que a sintaxe ou as APIs já foram implementadas. Cada linha deve
virar teste positivo, teste negativo, lowering inspecionável ou medição antes de
uma candidata se tornar normativa.

## Decisões e alternativas visíveis

| Tema | Forma usada no corpus | Alternativas ainda válidas | O que decide |
|---|---|---|---|
| std implícita | prelude T0 curada e congelada pela edição; namespaces sempre disponíveis | todo export único · somente namespaces + poucos nomes livres · import explícito | tokens, autocomplete, colisões, atualização de edição e capability visível |
| impressão | `print(...)` | `io.print(...)` · import de `print` · açúcar fixo | scripts/TUI, efeitos inferidos e diagnóstico de authority |
| visibilidade | privado por default + `export` | fields exportados individualmente · tipo opaco/factory · package visibility | invariantes, pattern matching, ABI e evolução |
| múltiplas alternativas | `value in (a, b)` | `value.isOneOf(a, b)` · `value == a || value == b` · pattern alternativo | tokens, narrowing, lista estática e garantia de zero allocation |
| formatter | largura preferida 120; horizontal se couber | outra largura antes da v1 · exceção documentada para fórmula | corpus formatado, diffs e leitura lado a lado |
| labels | primeiro argumento posicional; seguintes nomeados | todos nomeados · todos posicionais + lint | tokens, leitura, refactor e autocomplete |
| mutação | `mut fn` para receiver; `inout` para argumento explícito | `mut` como effect em toda função mutante | redundância, function types e diagnostics de borrow |
| unidades | quantity literal delimitado `180[°C]`; corpus executável ainda usa `180_Celsius` | sufixo próprio · `Number<Unit>` · declaração longa/wrapper nominal | parser, conversão, offsets, generics, ABI e zero overhead |
| exponenciação | `flow ** 2`; corpus executável mantém `flow * flow` até a grammar mudar | `pow(flow, 2)` · multiplicação explícita · APIs por família | precedência, dimensions, overflow e lowering |
| ranges | quatro closures, `in`, `clamp` em closed range e `stride` separado | producer lazy com step · `Interval` separado · unbounded como values | floats, totalidade, iteration, count/last, zero allocation e diagnostics |
| condição PID | helper com range patterns + `where` | expressão `||`/`&&` direta · tuple-pattern · combinador nomeado | intenção, duplicação do body, narrowing e HIR simples |
| serviço | `object` privado + `protocol` exportado + `ServiceRef` | keyword `service` · IDL/codegen · object com metadata | lifecycle, error, call local/remota e capacidade de remover açúcar |
| HTTP | `http`/`json` first-party | pacote oficial fora da std · somente transporte na std | portabilidade, TLS, codecs, tamanho e ritmo de evolução |
| outra linguagem | `foreign c` para ABI; body inline para o primeiro `fn<C>` | `fn<C> from` · namespace `C::unit` · adapter declarado | migração, ABI, ownership, parser injection, debug, cache e provenance |

`in (a, b)` significa OR/membership finito e baixa para comparações estáticas.
`.isOneOf` preserva a mesma leitura como alternativa. Um enum simples não pode
ser simultaneamente dois cases. Sets/flags precisam de operações distintas como
`hasAny` e `hasAll`.

As três leituras preservadas para a condição de anti-windup são:

```w
// Direta: a álgebra booleana é toda local.
let canAccumulate = rawDuty in 0.0...1.0
  || (rawDuty < 0.0 && error > 0.0)
  || (rawDuty > 1.0 && error < 0.0)

// Candidata no corpus: o range classifica rawDuty; `where` relaciona error.
switch rawDuty {
  case 0.0...1.0: return true
  case ..<0.0 where error > 0.0: return true
  case 1.0>.. where error < 0.0: return true
  case _: return false
}

// Pesquisa: tuple-pattern + patterns alternativos no mesmo case.
switch (rawDuty, error) {
  case (0.0...1.0, _), (..<0.0, 0.0>..), (1.0>.., ..<0.0): return true
  case _: return false
}
```

`where` não é um spelling geral de `&&`: ele refina um pattern, constraint ou
query já estabelecido. Isso mantém uma semântica única para a keyword.
Se a implementação de `isOneOf` exigir List/array ou varargs com allocation, ela
perde para a forma explícita; o HIR precisa receber uma lista estática de patterns
ou comparações diretas.

## Requisitos do frontend e type checker

| Evidência no source | Obrigação |
|---|---|
| `export struct`, `export protocol`, object privado | construir interface de módulo sem expor storage/implementação acidentalmente |
| mapa std da edição | resolver deterministicamente e registrar a origem de cada símbolo implícito |
| `Temperature`, `Power`, `Energy` | normalizar dimensões, rejeitar operações inválidas e tratar temperatura absoluta/delta separadamente |
| `Money` em minor units | overflow checked e rounding escolhido pelo domínio, nunca binary float implícito |
| `copy jobs`, `take entries`, partial field moves | análise de move por path, borrow exclusivo e diagnostics de uso após move |
| `async let`/`spawn let` | distinguir suspensão concorrente de transferência paralela e inferir `Send`/`Sync` |
| closures do sort/HTTP | inferir captures, ownership, lifetime, effects e sendability |
| typed errors compostos | não injetar um error set em outro silenciosamente |
| `print`, `http`, `json` | nome curto não apaga effect, capability, dependency ou custo |

## HIR e lowering

O frontend não pode baixar cedo todos esses programas a calls C genéricas. O HIR
W precisa preservar pelo menos:

- owner, borrow, move path, drop scope e representação ainda não escolhida;
- tipo dimensional normalizado e unidade/literal original para diagnostics;
- overflow, rounding, strict/reproducible/fast floating-point mode;
- árvore de tasks, join order, cancelamento, captures e intenção `spawn`;
- call local versus `ServiceRef`, typed error e suspension point;
- effect/capability, símbolo std resolvido e provenance da edição;
- allocations/copies candidatas, escape e região possível;
- interfaces exportadas separadas de ABI C e contrato RPC.

Somente depois disso o pipeline escolhe `arith`/`math`/`vector`/`linalg`, corrotina,
thread pool, stack, heap, arena, C ABI ou boundary de serviço.

## Memória automática é viável aqui?

Sim, desde que “automática” signifique o compilador escolher placement após
provar ownership/lifetime — não ARC universal nem tracing GC invisível.

| Valor/lifetime | Contrato | Lowering provável |
|---|---|---|
| quantidades, PID e records térmicos | values locais sem escape | SSA/register/stack, scalar replacement |
| `List<BakeJob>` copiada para ordenar | owner único; `copy` deliberado | buffer heap unique; copy elidível só se semanticamente equivalente |
| `entries` retornada com `take` | transferência do buffer | retorno owned sem cópia; caller executa drop |
| body HTTP/request strings | owner do request | região/request arena ou heap unique; não escapa sem transferência |
| task frame de `regulateOven` | vive entre awaits | frame stackless heap/region somente porque suspende |
| captures de TUI/HTTP | `ServiceRef` imutável compartilhável | handle pequeno com runtime ownership explícito; RC interno é possível |
| `OrderState`/`FrontDeskState` | owner do service host | heap unique/arena da instância e cleanup no lifecycle |
| `Batter` fields em children | move paths distintos | partial move + join antes de destruir o agregado restante |

Gates obrigatórios: use-after-move e borrow que escapa falham no compile time;
cancelamento executa drops/defer; FFI conserva deallocator de origem; ciclo só
existe em um tipo shared explícito; allocation failure/budget tem rota tipada.
Mimalloc pode ser perfil do heap, nunca a semântica que prova segurança.

## Concorrência e paralelismo são implementáveis?

O corpus exige um runtime pequeno, mas real:

1. task tree lexical e frames para `async let`;
2. executor de I/O/timers para forno, terminal e HTTP;
3. executor CPU limitado para `spawn let`, sem uma thread por child;
4. cancel token, join determinístico e cleanup antes de propagar erro;
5. mailbox limitada, service turn e backpressure;
6. `ServiceRef` com call local/remota observável, serialization somente quando a
   boundary física exigir;
7. logical stack trace que atravesse await/service call;
8. scheduler determinístico de teste e injection de clock/hardware.

`planning.w` e `billing.w` provam que lógica comum não precisa do runtime async.
`oven.w` separa `predictStep`/`controlStep` puros de `regulateOven`; essa fronteira
permite testar matemática sem sensor e I/O sem duplicar o algoritmo.

## Stdlib sugerida pelo corpus

| Nível | Necessidade imediata | Não pressupõe |
|---|---|---|
| core | primitivos, option, typed errors, ownership, quantities no type system | HTTP, allocator específico ou threads |
| portable std | String/Bytes/List, sort, Duration/Instant, Decimal/Money helpers, math | filesystem/socket/clock do host |
| host adapters | terminal, timer, entropy/IDs, sockets e service host | mesma implementação em todo target |
| first-party protocol | HTTP, JSON, observability e storage | promoção automática ao core ou carregamento no programa mínimo |
| external packages | UI rica, banco, codecs especiais, BLAS/GPU | confiança sem lock/provenance |

HTTP no exemplo é uma hipótese de produto: ser “sem terceira parte” pode
significar pacote first-party versionado junto ao toolchain, não necessariamente
namespace congelado da stdlib. O bundle/reachability deve remover tudo que não é
usado.

## Próximos testes verticais

1. parse + formatter golden dos dezesseis arquivos;
2. resolver interfaces/imports e emitir tabela de símbolos implícitos usados;
3. type-check de `units.w`, `billing.w` e negativos dimensionais/overflow;
4. HIR de `planning.w` com moves, List e closure do sort;
5. lowering síncrono de `predictStep` e comparação numérica em dois targets;
6. task tree de `regulateOven`, incluindo timeout/cancelamento/cleanup;
7. `FrontDeskState` local com mailbox limitada, TUI e HTTP concorrentes;
8. trocar a boundary para outro processo sem mudar a assinatura e medir o custo
   explicitamente reportado;
9. comparar as alternativas desta página antes de promover qualquer candidata.
10. comparar o wrapper `foreign c` de `interop.w` com uma ilha inline `fn<C>` da
    própria aplicação; o adapter C deve produzir IR/object, diagnostics, source
    map e metadata sem fazer o parser W interpretar statements C.
