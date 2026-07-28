# Requisitos derivados do restaurante

> **Working Draft · corpus top-down, não implementação disponível**

Este documento converte o pseudocódigo do restaurante em trabalho verificável.
Ele não declara que a sintaxe ou as APIs já foram implementadas. Cada linha deve
virar teste positivo, teste negativo, lowering inspecionável ou medição antes de
uma candidata se tornar normativa.

## Decisões e alternativas visíveis

| Tema | Forma usada no corpus | Alternativas ainda válidas | O que decide |
|---|---|---|---|
| std implícita | prelude T0 curada + poucos nomes T1 por edição; namespaces sempre disponíveis | todo export único · somente namespaces · tudo explícito | tokens, autocomplete, colisões, atualização de edição e capability visível |
| impressão | `print(...)` como nome T1 curto | `io.print(...)` · import de `print` | scripts/TUI, efeitos inferidos e diagnóstico de authority |
| visibilidade | privado por default + `export` | fields exportados individualmente · tipo opaco/factory · package visibility | invariantes, pattern matching, ABI e evolução |
| múltiplas alternativas | `value in (a, b)` | `value.isOneOf(a, b)` · `value == a || value == b` · pattern alternativo | tokens, narrowing, lista estática e garantia de zero allocation |
| formatter | largura preferida 120; horizontal se couber | outra largura antes da v1 · exceção documentada para fórmula | corpus formatado, diffs e leitura lado a lado |
| labels | primeiro argumento posicional; seguintes nomeados | todos nomeados · todos posicionais + lint | tokens, leitura, refactor e autocomplete |
| mutação | `mut fn` para receiver; `inout` para argumento explícito | `mut` como effect em toda função mutante | redundância, function types e diagnostics de borrow |
| unidades | forma canônica `180[degC]`; sugars `180C`/`180[°C]`; `64[KiB]`/`64KiB` | `Number<Unit>` · declaração longa/wrapper nominal | parser, prefixes SI/IEC, offsets, generics, ABI e zero overhead |
| exponenciação | `flow ** 2` em W; `m/s^2` na subgramática de unidade | `pow(flow, 2)` · multiplicação explícita · APIs por família | precedência, dimensions, overflow e lowering |
| ranges | quatro closures, `in`, `clamp` em closed range e `stride` separado | producer lazy com step · `Interval` separado · unbounded como values | floats, totalidade, iteration, count/last, zero allocation e diagnostics |
| condição PID | helper com range patterns + `where` | expressão `||`/`&&` direta · tuple-pattern · combinador nomeado | intenção, duplicação do body, narrowing e HIR simples |
| property behavior | `var Lazy heatProfile = ...` | wrapper nominal · accessor explícito · `by`/`with` históricos | init, get/set/modify, effects, exclusivity, layout e composição |
| atomic | `var atomic completed: u64 = 0` | `Atomic<u64>` explícito · lock/serviço serial | shared mutation, ordering, target fallback e borrow do payload |
| cancelamento | `cancel task, reason: .shutdown` | `task.cancel()` · cancelamento somente pelo scope | coerência async/spawn, tipos canceláveis, cleanup e join |
| documentação/testes | `///`, fence `w test`, `test "..." for symbol` | `*.test.w` · tags JSDoc · DSL histórica `@`/`@@` | attachment, release stripping, compile-fail, capabilities e runner |
| serviço | `service State as Api` + `ServiceRef` | object + descriptor explícito · IDL/codegen | lifecycle, error, call local/remota e capacidade de remover açúcar |
| HTTP | `http`/`json` first-party | pacote oficial fora da std · somente transporte na std | portabilidade, TLS, codecs, tamanho e ritmo de evolução |
| outra linguagem | `foreign c` para ABI; body inline para o primeiro `fn<C>` | `fn<C> from` · namespace `C::unit` · adapter declarado | migração, ABI, ownership, parser injection, debug, cache e provenance |
| domínio de execução | call isolada via `await`; placement apenas no ensaio aberto | `async/spawn on .domain` · `async/spawn<.domain>` · descriptor/manifest · API explícita | isolamento versus placement, paralelismo, portabilidade, starvation e diagnostics |
| entrypoint | descriptor tipado liga handlers comuns a slots de host | `entry { statements }` para default único · binding para closure · conformance · apenas manifest | colisões, testabilidade, profiles versionados, capabilities e adapters |
| matrizes/tensores | arrays aninhados + `Matrix`/`matmul` como baseline | literal com `;` · operador `@` · métodos/operators distintos | shapes, promotion, broadcasting, aliasing, devices, autodiff e lowering |
| parâmetros de valor | `<...>` somente quando o tipo declara os parâmetros | `T where P` · `T(where: P)` · `T<where: (P)>`/`T<where(P)>` · labels/posicionais · tipo dedicado | kinds, fase/comptime, inferência, diagnostics, ABI, layout e monomorphization |
| closures | `(args) => expression/block` como baseline provisória | `fn(args) {}` · `{ args in ... }` · block contextual | ambiguidade, captures, lifetime, effects, C callbacks e sendability |

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
| `export struct`, `export protocol`, `service State as Api` privado | construir interface/descriptor sem expor storage/implementação acidentalmente |
| mapa std da edição | resolver deterministicamente e registrar a origem de cada símbolo implícito |
| `Temperature`, `Power`, `Energy` | normalizar dimensões, rejeitar operações inválidas e tratar temperatura absoluta/delta separadamente |
| `Money` em minor units | overflow checked e rounding escolhido pelo domínio, nunca binary float implícito |
| `copy jobs`, `take entries`, partial field moves | análise de move por path, borrow exclusivo e diagnostics de uso após move |
| `async let`/`spawn let` | distinguir suspensão concorrente de transferência paralela e inferir `Send`/`Sync` |
| preferência de executor e call isolada | separar placement, isolamento, afinidade e paralelismo; verificar herança e hops |
| descriptor de entry | validar slots contra um profile de host versionado sem tornar o handler magicamente público |
| tensor com shape/dtype/device | provar shapes e promotion, tornar transfers/copies observáveis e rejeitar broadcast ambíguo |
| generic value/refinement/layout | kind-check de argumentos compile-time e preservar identidade, invariante e representação separadamente |
| `cancel task`, `var atomic` e `var Lazy` | verificar cancellability, ordering/borrow atômico e expansão de storage/accessors |
| closures do sort/HTTP | inferir captures, ownership, lifetime, effects e sendability |
| typed errors compostos | não injetar um error set em outro silenciosamente |
| `print`, `http`, `json` | nome curto não apaga effect, capability, dependency ou custo |
| `///`, doctest e `test ... for` | anexar ao símbolo, gerar grafo de teste e eliminar integralmente do release |

## HIR e lowering

O frontend não pode baixar cedo todos esses programas a calls C genéricas. O HIR
W precisa preservar pelo menos:

- owner, borrow, move path, drop scope e representação ainda não escolhida;
- tipo dimensional normalizado e unidade/literal original para diagnostics;
- overflow, rounding, strict/reproducible/fast floating-point mode;
- árvore de tasks, join order, cancelamento, captures e intenção `spawn`;
- isolamento, preferência de executor, afinidade exigida e hops entre domínios;
- call local versus `ServiceRef`, typed error e suspension point;
- slots de entry resolvidos contra profile, handler e capabilities requeridas;
- parâmetros compile-time normalizados, shape/dtype/device e regras de alias/broadcast;
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
4. registry lógico de domains que não promete a mesma thread física e mantém
   isolamento, preferência e afinidade como fatos distintos;
5. cancel token, join determinístico e cleanup antes de propagar erro;
6. mailbox limitada, service turn e backpressure;
7. `ServiceRef` com call local/remota observável, serialization somente quando a
   boundary física exigir;
8. logical stack trace que atravesse await/service call;
9. scheduler determinístico de teste e injection de clock/hardware.

`planning.w` e `billing.w` provam que lógica comum não precisa do runtime async.
`oven.w` separa `predictStep`/`controlStep` puros de `regulateOven`; essa fronteira
permite testar matemática sem sensor e I/O sem duplicar o algoritmo.

## Stdlib sugerida pelo corpus

| Nível | Necessidade imediata | Não pressupõe |
|---|---|---|
| T0 foundation | primitivos, option, typed errors, ownership, strings/collections/ranges puros | console, HTTP, allocator específico ou threads |
| T1 systems | `print`/console, tasks, clocks, filesystem, process, TCP/UDP/DNS e adapters | mesma implementação em todo target |
| T2 domains | HTTP/TLS, SI/information units, JSON, SQLite, tensor/linalg, numerics e TUI first-party | promoção automática ao core ou carregamento no programa mínimo |
| T2 experimental | autodiff, model interchange, sparse/shard e kernels especializados | estabilidade junto à edição da linguagem ou semântica escondida de device |
| packages externos | UI rica, banco/codecs especiais e integrações | confiança sem lock/provenance |

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
11. extrair docs/doctests/testes co-localizados, confirmar `compile-fail` por ID e
    provar que nenhum byte do grafo de teste entra no payload release.
12. baixar `var Lazy` e `var atomic`, incluindo negativos de init order, borrow do
    payload atômico e mutação não-atômica por receiver compartilhado.
13. comparar W-O100–W-O103 e a dependência W-O052 no ensaio do adendo antes de
    adicionar qualquer token novo ao parser/highlighter; cada promoção gera corpus
    positivo e negativo.
