# Design integral da linguagem W

> **Status:** **Candidato experimental** · 27 de julho de 2026

Este é o documento canônico de design do W. Ele reúne linguagem, runtime, SDK,
compilador, packages, distribuição, tooling, plano e alternativas. A forma
integrada atual se chama Baseline de Design 2 (DB2). Ela aplica as decisões da
DB1, corrige contradições entre os documentos e seleciona líderes para as
questões recuperadas depois da DB1.

A implementação pode usar estes líderes antes da ratificação humana. Cada líder
tem uma alternativa preservada. O objetivo é permitir uma revisão visual com
parser, realce, Book e exemplos coerentes.

## 0. Como ler este documento

Este arquivo substitui os documentos de design espalhados em `W/`. A ordem é:

1. visão e regras de decisão;
2. sintaxe e semântica da linguagem;
3. memória, execução e SDK;
4. compilador, packages, releases e tooling;
5. viabilidade, alternativas e plano de implementação.

Os estados usados são:

| Estado | Significado |
|---|---|
| **Direção** | princípio estável que limita as soluções |
| **Líder DB2** | forma implementada para avaliação visual |
| **Alternativa** | solução legítima que continua no corpus de comparação |
| **Pesquisa** | hipótese com baseline funcional que não depende dela |
| **Rejeitado por enquanto** | não entra na DB2 sem nova evidência |

### Mapa do sistema

| Bloco | Seções | Resultado que o bloco define |
|---|---:|---|
| produto e método | 0–4 | promessa, evidência, invariantes, símbolos e superfície integrada |
| linguagem | 5–8 | source, módulos, funções, closures, tipos e conversões |
| segurança semântica | 9–11 | memória, layout, behaviors, errors, panic, OOM e cleanup |
| execução | 12–13 | tasks, paralelismo, domains, services, entries, estado e sandbox |
| SDK e domínios | 14–18 | tiers, numéricos, units, texto, collections, tensors, C e ilhas de linguagem |
| implementação e produto | 19–23 | frontend, MLIR, runtime, bootstrap, packages, releases, tooling e pesquisas |
| validação e sequência | 24–28 | ensaio integrado, revisão, roadmap, delta DB1 e registro de alternativas |

Leia o bloco que contém a dúvida e depois use o ID D2 correspondente. Não é
necessário reconstruir uma decisão a partir do histórico.

O histórico da DB1 e as notas anteriores ficam em
[`Y/W/`](../Y/W/). O Git preserva autoria, datas e diffs. Este arquivo é a única
fonte de verdade para o estado atual.

### 0.1 Promessa

> **Prazer para humanos. Clareza para máquinas.**

W é uma linguagem nativa, segura e previsível. Ela oferece um caminho contínuo
entre aplicação legível, sistemas concorrentes/paralelos e controle explícito de
ABI ou layout.

“Fazer para C o que TypeScript fez para JavaScript” descreve adoção incremental
e interoperabilidade. W não é um superset de C. Ela não herda preprocessor,
undefined behavior ou todos os dialetos de C.

O público inicial inclui pessoas que escrevem:

- CLIs, serviços, runtimes e bibliotecas nativas;
- sistemas com I/O concorrente e trabalho paralelo;
- ferramentas, jogos, áudio, vídeo e software embarcado;
- simulações, computação científica e ML;
- software que precisa explicar dependências, custo e provenance.

### 0.2 Princípios de produto

1. O source deve permitir prever a execução.
2. O caminho comum deve ser leve.
3. Segurança e baixo nível são camadas compatíveis.
4. Concorrência é estruturada e paralelismo é intencional.
5. Uma hipótese só restringe a linguagem depois de um protótipo.
6. Uma forma canônica vale mais que sinônimos.
7. Diagnósticos são parte da linguagem.
8. Build e distribuição são partes do produto.
9. Portabilidade vem de semântica e profiles, não do menor denominador comum.
10. IA amplifica clareza; ela não justifica source opaco.
11. Granularidade lógica não exige fragmentação física.
12. Açúcar de domínio precisa revelar tipos, custos e efeitos.

### 0.3 Não objetivos

A DB2 não tenta:

- substituir JavaScript no navegador;
- criar um sistema operacional ou uma plataforma serverless completa;
- suportar toda linguagem em `fn<Language>`;
- tornar wQL ou wRPC um protocolo universal;
- tornar toda estrutura lock-free;
- provar o máximo exato de memória e threads para todo programa;
- exigir SQLite em toda aplicação;
- substituir UTF-8 por uma representação de String experimental;
- estabilizar uma ABI W eterna antes da semântica;
- esconder device transfer, blocking ou rede atrás de um field access.

### 0.4 Critérios de sucesso

- Pessoas que conhecem C, Swift ou TypeScript entendem o Tour sem treinamento
  longo.
- O formatter produz uma representação estável.
- Um exemplo curto explica a diferença entre `async` e `spawn`.
- Debug e release passam os mesmos testes observáveis.
- Wrappers C simples não exigem cópias ou allocations ocultas.
- O toolchain reproduz e explica um artefato com dependências.
- Ownership e task lifetime falhos geram diagnostics com fix-its úteis.
- Modelos consomem schemas, HIR e diagnostics sem extrair fatos de prosa.

### 0.5 Regra para novas propostas

Toda proposta deve informar:

1. o problema observável;
2. por que linguagem, biblioteca ou tooling é a camada correta;
3. o que aparece no source, tipo e runtime;
4. erro, cancelamento, cleanup e FFI;
5. comportamento em dois targets;
6. alternativa mais simples;
7. teste, oracle e critério de remoção.

## 1. Limite da alegação

Não existe um estudo que prove a melhor sintaxe para W. Familiaridade também não
prova facilidade de uso. A DB2 usa três classes de evidência:

1. contratos que podem ser verificados pelo compilador;
2. precedentes de linguagens e ferramentas em produção;
3. testes controlados com pessoas e modelos.

O processo [PLIERS](https://arxiv.org/abs/1912.04719) recomenda protótipos e
avaliações formativas durante o design da linguagem. As
[Cognitive Dimensions of Notations](https://www.cl.cam.ac.uk/~afb21/CognitiveDimensions/papers/Green1989.pdf)
ajudam a localizar inconsistência, erro provável, custo de mudança e falta de
clareza de função. Elas não produzem uma pontuação vencedora.

Os estudos de
[Stefik e Siebert](https://doi.org/10.1145/2534973) e sua
[replicação](https://doi.org/10.1007/s11219-023-09631-7) mostram outro limite:
uma escolha comum para especialistas pode continuar difícil para iniciantes.
Por isso, a DB2 mede acerto antes de medir preferência.

## 2. Invariantes

Estas regras limitam todas as escolhas da DB2:

1. O source mostra efeitos que podem suspender, falhar, bloquear uma fronteira
   ou transferir ownership.
2. Um import não executa código e não concede authority.
3. Um módulo não possui thread, heap, singleton ou lifecycle implícito.
4. Concorrência e paralelismo continuam distintos.
5. Todo filho concorrente pertence a um scope.
6. Safe W não permite dangling references, double drop ou data race.
7. Debug e release têm a mesma semântica observável.
8. Layout, pointer tagging e placement físicos não viram promessa sem uma
   fronteira explícita.
9. Conversão implícita exige uma rota única, total e sem perda.
10. O formatter produz uma forma canônica.
11. Uma edição pode adicionar açúcar, mas deve conseguir mostrar sua expansão.
12. Uma otimização dependente do target possui fallback correto.
13. Artefatos reproduzíveis dependem somente de inputs declarados.
14. Uma assinatura não substitui reprodução, auditoria ou platform signing.
15. Recursos para máquinas também devem ajudar pessoas ou ferramentas comuns.

## 3. Orçamento de símbolos

Cada delimitador possui uma função mental principal:

| Forma | Função principal na DB2 | Exemplos |
|---|---|---|
| `{...}` | corpo, scope ou descriptor | função, tipo, `entry`, `service`, `unsafe` |
| `(...)` | chamada, parâmetros, agrupamento ou product literal | `cook(order)`, `(x: 1, y: 2)` |
| `[...]` | coleção, mapa, indexação, slice ou shape | `[1, 2]`, `map[key]`, `[T; 16]` |
| `<...>` | aplicação estática conhecida pelo head | `Array<u8>`, `fn<C>`, `9.81<m/s^2>` |
| `:` | introduz um contrato associado | tipo, return, label, field ou case body |
| `=` | define ou atualiza um valor/binding | binding, assignment, alias, slot |
| `.` | qualificação, member ou case abreviado | `std.http`, `value.count`, `.none` |
| `=>` | separa parâmetros e corpo de closure | `(item) => transform(item)` |
| `where` | restringe a forma anterior | refinement, generic constraint, case guard |
| `on` | seleciona preferência de execução | `spawn on .compute let plan = ...` |
| `@` | contração matricial | `features @ weights` |

`<...>` não é um mapa universal de modificadores. O elemento à esquerda declara
o tipo de argumento estático que aceita. Um número aceita uma expressão de
unidade. Um nome de tipo ou função aceita parâmetros declarados. `fn<C>` aceita
uma tag de frontend registrada.

Chaves não representam units, sets ou object literals. Essa restrição mantém
chaves como um sinal forte de scope.

## 4. Superfície integrada

O exemplo abaixo mostra a forma líder. Ele não tenta mostrar toda a biblioteca.

```w
import { Request, Response } from std.http
import std.tensor as tensor

export type OrderId = u64
export type Ratio = f64 where (value in 0.0...1.0)

export enum KitchenError: Error {
  unavailable
  thermal(ThermalError)
}

export struct Order {
  id: OrderId
  guests: u16
}

export protocol KitchenApi {
  async fn prepare(order: take Order): Dish throws KitchenError
}

export service Kitchen as KitchenApi {
  var Lazy calibration = loadCalibration()
  var atomic completed: u64 = 0

  mut async fn prepare(order: take Order): Dish throws KitchenError {
    async on .network let stock = checkStock(order)
    spawn on .compute let plan = optimizePlan(order)

    let (stock, plan) = try await (stock, plan)
    return try await execute(stock, plan: plan)
  }
}

fn score(features: ref Tensor<f32, shape: [1, 8]>,
         weights: ref Tensor<f32, shape: [8, 4]>): Tensor<f32, shape: [1, 4]> {
  return features @ weights
}

entry LastLight {
  process.main = run
  http.fetch = fetch
}
```

O formatter mantém uma assinatura em uma linha quando ela cabe em 120 colunas.
O exemplo `score` quebra porque a forma completa ultrapassa esse limite.

## 5. Source, nomes e edição

### 5.1 Source

- A forma canônica usa UTF-8 sem BOM e LF.
- Keywords são ASCII, lowercase e case-sensitive.
- Identificadores usam Unicode conforme UAX #31 e são normalizados para NFC.
- O compilador rejeita dois nomes que normalizam para a mesma sequência.
- Confusables, scripts mistos e caracteres invisíveis produzem erro em API
  pública. Código privado recebe erro ou warning conforme a policy da edição.
- O lockfile registra a edição e a versão do bundle Unicode.

O primeiro seed pode aceitar somente ASCII. Isso é uma limitação do seed, não a
semântica final.

### 5.2 Comentários e documentação

```w
// comentário

/* bloco aninhável */

/// Resumo da API.
///
/// ```w test
/// expect clamp(2, to: 0...3) == 2
/// ```
export fn clamp(...)
```

`///` contém Markdown e pode produzir doctests. Comentários não são annotations.
Debug symbols e documentação compilada são artefatos separados e removíveis.

### 5.3 Statements

O formatter não emite `;`. O parser aceita `;` como separador de migração.
Semicolon não separa linhas de matriz na DB2.

## 6. Módulos, imports e visibilidade

O manifest define os arquivos de cada módulo. Um módulo pode conter vários
arquivos. O source não repete uma declaração `module`.

```w
import std.http
import std.tensor as tensor
import { Request, Response as HttpResponse } from std.http

export import { GuestId } from restaurant.domain
```

Regras:

- `import path` importa o namespace com o último segmento como nome local;
- `as` troca esse nome;
- `{...} from path` importa símbolos;
- `export import` cria uma facade;
- módulos formam um DAG;
- um ciclo interno precisa ser um único módulo;
- URL, versão e digest nunca aparecem no import.

Declarations são privadas por default:

```w
fn localHelper()
package fn packageHelper()
export fn publicOperation()
```

`package` concede visibilidade ao package atual. `friend` não existe. Um
`export { ... }` coletivo fica preservado como alternativa, mas não é a forma
líder: o modifier local melhora diff, busca e geração de interface.

O top-level aceita imports, declarations e `const`. Ele não aceita I/O, `var`
global ou inicialização runtime.

## 7. Bindings, funções e closures

### 7.1 Bindings

```w
const pageSize = 4096
let name = "Last Light"
var orders = 0
var Lazy map = loadMap()
var atomic completed: u64 = 0
```

- `const` é avaliado pelo ambiente compile-time hermético;
- `let` cria um binding runtime que não pode ser reatribuído;
- `var` cria um binding runtime reatribuível;
- um behavior fica entre `var` e o nome;
- `atomic` ocupa a mesma posição, mas é um modifier verificado pelo compilador.

### 7.2 Funções

```w
export unsafe mut async fn update<T: Send>(
  order: inout Order,
  with event: take Event,
): Receipt throws UpdateError {
  // ...
}
```

A ordem canônica é:

1. visibilidade;
2. `unsafe`, se necessário;
3. `mut`, para receiver mutável;
4. `async`;
5. `fn` ou `fn<Language>`.

`throws E` fica depois do return type. `Void` pode ser omitido.

O primeiro argumento é posicional por default. Os seguintes usam o nome como
label. Um label explícito substitui o default. `_` remove um label.

### 7.3 Parâmetros e ownership

```w
fn inspect(value: ref Value)
fn edit(value: inout Value)
fn store(value: take Value)
fn transform(value: Value): Result
```

Um parâmetro `T` é pass-by-value. Um tipo `Copy` produz uma cópia semântica. Um
tipo move-only pode ser movido no último uso. `take T` exige transferência e
mostra essa exigência na assinatura e no call site.

```w
inspect(value)
edit(inout value)
store(take value)
let duplicate = copy value
```

`ref` não aparece no call site porque não altera ownership. `inout`, `take` e
`copy` aparecem.

Partial move exige destructuring. A DB2 não permite mover um field e continuar a
usar o aggregate parcialmente inicializado.

### 7.4 Closures

```w
let double = (value: Int) => value * 2

let task = capture(take model, ref cache) (input) => {
  return model.run(input, cache: cache)
}
```

Captures são inferidos. `capture(...)` substitui a inferência nos casos
importantes. Os modos são `copy`, `ref`, `take` e `weak`. `inout` não pode
escapar de um scope síncrono.

`(args) => body` é a única forma de closure da DB2. `{ args in body }` e
`fn(args) { body }` ficam como alternativas de corpus.

## 8. Tipos e conversões

### 8.1 Categorias

| Forma | Semântica |
|---|---|
| `struct` | product type com valor independente |
| `object` | identidade e owner único por default |
| `enum` | sum type fechado |
| `protocol` | requisitos sem storage implícito |
| `type` | nova identidade nominal |
| `alias` | outro nome para a mesma identidade |
| `any P` | existential com identidade concreta apagada |
| `some P` | tipo concreto preservado e oculto do caller |

Herança de implementação não entra na DB2. Composição, protocols e funções
livres são a baseline.

Extensions não adicionam storage:

```w
extension Dish: Displayable {
  fn display(): String { ... }
}
```

Uma conformance pode ser declarada no módulo do tipo ou no módulo do protocol.
Essa regra evita conformances órfãs e conflitos dependentes da ordem de import.

### 8.2 Option e ausência

`T?` é `Option<T>` e possui somente `.some(T)` ou `.none`.

```w
if let guest = findGuest(id) {
  greet(guest)
}

guard let guest = findGuest(id) else return .notFound
let name = guest?.name ?? "Anonymous"
```

Não existem `null`, `undefined`, `uninitialized` ou `empty` universais.

### 8.3 Newtype, alias e refinement

```w
type GuestId = u64
alias VisitorId = GuestId

type Ratio = f64 where (value in 0.0...1.0)

type BoundedString<const min: usize, const max: usize> =
  String where (value.scalars.count in min...max)

type ShortLabel = BoundedString<min: 1, max: 40>
```

`where (...)` restringe a declaração anterior. Ele não é um argumento runtime e
não ocupa `<...>`. Parênteses delimitam o predicate.

Um literal válido pode ser provado em compile time. Um valor runtime usa um
construtor fallible:

```w
const full: Ratio = 1.0
let current = try Ratio(input)
```

Refined-to-base é implícito. Base-to-refined é fallible. O refinement não muda o
layout canônico em structs, ABI, FFI, persistência ou borrows. O optimizer pode
estreitar register, SIMD e storage interno não escapante e depois reestender.

As formas `T(where: predicate)`, `T<where: (...)>` e `T<where(...)>` continuam no
registro de alternativas. Elas perdem como baseline porque misturam construção,
generic application e refinement.

### 8.4 Generics

Parâmetros são declarados antes do uso:

```w
fn get<T, const count: usize>(values: ref [T; count]): T
```

Cada argumento de `<...>` possui kind declarado: tipo ou valor `const`. Labels
de argumentos são aceitos quando a declaração possui labels. Um modifier map
aberto não existe.

Generics usam monomorphization e specialization dentro do build. Interfaces
podem usar witnesses para reduzir code size e preservar separate compilation.

### 8.5 Conversões

Uma conversão implícita é permitida somente se:

1. é total para todos os valores do tipo de origem;
2. preserva o valor;
3. existe uma única rota canônica;
4. não muda ownership ou authority de forma oculta.

Narrowing, parsing, rounding, reinterpretation, ponteiro e conversão ambígua são
explícitos. O mesmo princípio permite `T` para `any P` quando `T: P`.

## 9. Memória, layout e alocação

### 9.1 Modelo semântico

Todo valor que exige cleanup possui um owner. O compilador controla
inicialização, move, borrow, escape e drop. Stack, heap, register, arena e
pointer tag são escolhas de representação.

A escada da DB2 é:

1. valor inline ou promovido para register;
2. owner único com drop determinístico;
3. borrow `ref` ou `inout`;
4. região lexical/arena quando o lifetime comum é útil;
5. `shared T` para múltiplos owners reais;
6. owner de service para estado serializado por instância;
7. ponteiro manual somente na fronteira unsafe/FFI.

Nenhum assignment escolhe silenciosamente entre move, ARC e arena. O tipo e a
assinatura determinam a operação.

### 9.2 Owner único e shared

`object`, buffers e resources são move-first. Last-use inference remove
marcadores no caminho comum. `take` continua obrigatório quando a API exige
transferência.

`shared T` é a forma de múltiplos owners. A implementação portátil usa reference
counting. `weak T?` quebra ciclos. O compilador pode eliminar retains/releases
quando prova ownership. Cruzar `spawn` exige contador thread-safe e `T: Send`.

ARC não é o default universal. Um valor sem compartilhamento não paga pelo
shared ownership.

### 9.3 Regiões

Região agrupa lifetimes; budget limita recursos. Eles são conceitos diferentes.
A primeira implementação oferece uma API de arena. Uma forma de bloco continua
experimental:

```w
region request(limit: 64<MiB>) {
  let document = try parse(payload, in: request)
  respond(document)
}
```

Um valor não escapa da região por borrow. Um move para fora só é permitido
quando o tipo e a operação transferem storage corretamente. Filhos async que
usam a região precisam terminar antes do bloco.

### 9.4 Allocator

O profile portátil começa com o allocator do sistema. O host pode selecionar
mimalloc ou outro allocator compatível. A seleção participa da recipe e do
profile de performance; ela não muda ownership nem a API do programa.

Um resource estrangeiro mantém seu deallocator. W nunca chama `free` num pointer
de origem desconhecida.

Alocações que precisam de recovery usam uma API fallible ou uma região com
budget. OOM geral encerra a isolation boundary conforme a seção de panic.

### 9.5 Layout e ABI

Layout W comum é opaco entre builds. O compilador pode reorder fields privados,
usar niches, eliminar aggregates ou especializar storage não escapante.

Layout observável exige uma fronteira:

- `foreign c` para ABI C;
- schema explícito para wire/persistência;
- profile/fingerprint para ABI W binária;
- tipo de storage dedicado para capacity/alignment observável.

`packed` e `aligned` são modifiers de layout seguros e restritos. Eles não são
annotations genéricas. Unaligned access nunca produz uma referência W normal.

### 9.6 Tagged values

Niches convencionais são a primeira otimização. Low-bit e high-bit tags só
entram quando o target, allocator, sanitizer e pointer authentication permitem.

Regras:

- `Option<ref T>` não aloca;
- o tamanho exato depende do profile;
- `f64` não perde bits;
- `Int` não perde range;
- FFI nunca observa uma representação tagged W;
- capability pointers e targets sem bits livres usam fallback expandido;
- tooling explica quando um box/tag foi usado.

Tagged address não participa do source e não é requisito do modelo de memória.

### 9.7 Destruição

- locals são destruídos na ordem inversa da inicialização;
- fields owned morrem com o owner;
- `deinit` não usa `throws`;
- `defer` cobre todas as saídas estruturadas;
- cancelamento executa cleanup;
- panic não continua numa boundary parcialmente destruída;
- foreign callbacks registram owner, context e destroy function.

## 10. Property behaviors

O uso continua simples:

```w
var Lazy heatProfile = deriveHeatProfile(model)
```

Uma declaração experimental é:

```w
behavior Lazy<Value> for Value {
  storage var cached: Value?
  initialValue

  init { cached = .none }

  mut get {
    if let value = cached { return value }
    let value = initialValue()
    cached = .some(value)
    return value
  }

  set(newValue) { cached = .some(newValue) }
}
```

A primeira implementação aceita somente `init`, `get`, `set` e `modify`
síncronos e sem `throws`. Um accessor que suspende, faz rede ou bloqueia exige
uma API nomeada. Behavior não concede `Send`, `Sync` ou atomicidade.

Composição v0 usa um behavior composto nomeado. Lista por vírgula e nesting
arbitrário ficam como alternativas até que ordem, exclusivity e drop tenham uma
regra simples.

Range continua responsável por `contains` e `clamp`. Um behavior `Clamped` só
serve quando a propriedade precisa aplicar uma policy em toda atribuição. Ele
não substitui o refined type nem o `Range`.

## 11. Erros, panic, OOM e cleanup

### 11.1 Erro recuperável

```w
enum ParseError: Error {
  unexpectedToken(Token)
  incompleteDocument
}

fn parse(source: ref String): Document throws ParseError
let document = try parse(source)
```

`throws E` faz parte do tipo. `try` propaga `E`. Se o error set do caller possui
exatamente um case que aceita `E`, o compilador pode inserir essa injeção total:

```w
enum AppError: Error {
  parse(ParseError)
  storage(StorageError)
}
```

Duas rotas possíveis tornam a conversão ambígua e exigem `do`/`catch`.

### 11.2 Panic e OOM

`panic` informa uma invariante quebrada. O profile encerra a isolation boundary.
A DB2 não faz unwind recuperável através de FFI.

Alocações explicitamente fallible retornam `AllocationError`. OOM do allocator
geral encerra a isolation boundary. O compilador não promete que todo OOM é
recuperável.

### 11.3 Cleanup

`defer` executa cleanup síncrono em ordem LIFO. Destruction segue a ordem inversa
da inicialização onde ela é observável.

Cleanup que precisa suspender usa uma forma distinta:

```w
defer async {
  await connection.close()
}
```

`defer async` só existe em função async. Ele executa em LIFO durante a saída do
scope. O body deve tratar seu próprio error. O runtime fornece uma janela de
cleanup limitada pelo profile; outro cancelamento não interrompe o mesmo cleanup
indefinidamente.

## 12. Concorrência, paralelismo e execução

### 12.1 Três intenções

```w
let value = calculate()                  // chamada atual
async let menu = fetchMenu()             // filho concorrente
spawn let plan = optimize(menu)          // filho paralelo
```

Uma função async precisa de `await`, `async let` ou `spawn let`. W não cria uma
Promise/Future silenciosa.

`Task<T, E>` é lexical, linear e one-shot. `SharedTask<T, E>` é uma construção
explícita para múltiplos observers.

### 12.2 Scope e falha

- todo filho pertence ao scope criador;
- o scope faz join antes de sair;
- erro ou saída antecipada solicita cancelamento dos filhos restantes;
- o erro primário segue a ordem lexical do join;
- erros adicionais ficam anexos e observáveis;
- uma API de agregação retorna todos os resultados quando isso é a intenção.

### 12.3 Cancelamento

```w
cancel task
cancel task, reason: .shutdown
```

Cancelamento é um exit separado de `E`. Ele é cooperativo. Pontos de suspensão,
I/O cancel-aware e `Task.checkCancellation()` observam o sinal. Cleanup ainda
executa. W não usa cancelamento assíncrono de thread.

### 12.4 Domínios de execução

```w
async on .network let menu = fetchMenu()
spawn on .compute let plan = optimize(menu)
```

`on` informa uma preferência de executor estática do profile. Ele não promete
uma thread física, affinity ou isolation. O profile define os domínios, limites
e fallback. `spawn` em um domínio estritamente serial é erro.

Seleção dinâmica usa API:

```w
let task = Task.spawn(on: executor, operation: work)
```

`spawn<.compute>` fica preservado como alternativa. Ele perde como líder porque
parece generic application e não nomeia a relação de placement.

### 12.5 Grupos, streams e backpressure

Task groups dinâmicos são lexicais e bounded. Criar um filho quando o limite foi
atingido aguarda capacity ou retorna um error de policy. Não existe fila
dinâmica ilimitada implícita.

Async streams usam pull por default. Channels são tipos separados, bounded e
declaram ordering, demand e ownership de cada elemento.

### 12.6 Atomics e shared state

`Send` e `Sync` são protocols derivados pelo compilador. Conformance manual
exige `unsafe`.

`var atomic value` baixa para `Atomic<T>`. Operações comuns são sequentially
consistent. Memory orders mais fracas exigem métodos explícitos. Shared mutable
state usa atomics, locks ou uma isolation boundary. Ele não nasce de alias
normal.

## 13. Módulos de execução, services e entries

### 13.1 Service

```w
export service DiningRoom as DiningRoomApi {
  var tables: Map<TableId, TableState>

  mut async fn reserve(request: Reservation): Receipt throws ReservationError {
    // ...
  }
}
```

O default é um turn serial e fechado. `await` não permite que outro handler da
mesma instância observe estado intermediário. Outras instâncias podem progredir.

Uma `ServiceRef<P>` sempre exige `await`, mesmo quando a instância está no mesmo
processo. O trace informa se houve hop. O compilador/runtime pode co-localizar e
fundir chamadas com a regra *as-if*.

Mailbox é bounded. Saturação aguarda capacity ou retorna overload conforme o
contrato. O runtime nunca descarta uma mensagem silenciosamente.

Uma falha lógica da instância não é uma sandbox de memória. Código não confiável
exige processo, OS sandbox ou Wasm.

### 13.2 Entry

Forma curta:

```w
entry {
  print("Hello, final service window")
}
```

Ela só é válida quando o product escolhe um profile com um slot default único.
O body ignora os parâmetros do slot. Para usar argumentos ou Context, o source
declara uma função normal e um descriptor:

```w
entry LastLight {
  process.main = run
  process.stdinLine = readCommand
  http.fetch = fetch
  process.signal = handleSignal
}
```

O nome diferencia as duas produções. `entry { ... }` é um handler curto.
`entry Name { ... }` é um descriptor de bindings. Bindings não usam vírgula.

Slots são símbolos tipados e versionados do profile. O build escolhe um
descriptor por product e gera `main`, `WinMain`, WASI export ou harness. Importar
o módulo não registra nem executa o entry.

`Context` é uma capability tipada. Ele não é um mapa universal de environment.

### 13.3 Unidade lógica e packing físico

Service/instância é uma unidade lógica endereçável. Ela pode ter identity,
lifecycle, state, mailbox, quotas, capabilities e trace próprios. Ela não exige
processo, thread, library ou conexão próprios.

O runtime pode:

- co-localizar instâncias;
- agrupar mailboxes;
- inlinear uma call local;
- usar um fast path sem serialização;
- distribuir instâncias entre executors ou processos;
- mover uma instância entre hosts quando o adapter permite.

A regra *as-if* preserva ordering, errors, cancelamento, deadline, capability,
identity e observabilidade. Fine-grained compute é um modelo lógico. O toolchain
mede se a granularidade física cria overhead excessivo.

### 13.4 Descriptor e instance manager

Um descriptor de instância registra:

- implementação e protocol exportado;
- scope de identity: process, key, request ou deployment;
- execution domain;
- mailbox policy;
- capabilities;
- durable adapter;
- restart policy;
- resource budget e observabilidade.

O instance manager mantém estados explícitos: declared, starting, running,
draining, stopped e failed. Startup e shutdown podem falhar. Import não altera
essa máquina.

### 13.5 Calls, ordering e falha distribuída

Uma call tipada possui call ID, caller, callee, deadline, cancellation ID,
capabilities e payload. A API local e remota usa o mesmo error set de domínio.
Falhas de transporte, protocol e autorização continuam categorias separadas.

Retry de operação mutante nunca é implícito. Idempotência pode autorizar uma
policy. Queda depois do envio pode produzir resultado desconhecido; W não promete
exactly-once sem protocolo e storage adequados.

Uma capability remota é um handle tipado. Ela não é uma URL livre nem um nome
global. Importar a interface não cria o handle.

### 13.6 Estado durável

Durability é um adapter explícito. Um handler declara a transação, o ponto de
commit e a relação entre state e outputs. A baseline não presume que todo state
é durável.

SQLite é o primeiro adapter oficial provável porque oferece transações, operação
local e boa portabilidade. Ele não é a semântica universal. Memory, files, KV
remoto e engines especializadas podem implementar o mesmo contrato.

Output gates e alarms/timers duráveis continuam em pesquisa. O protótipo precisa
definir cancelamento antes, durante e depois do commit.

### 13.7 Capabilities e sandbox

O contrato portátil usa capabilities tipadas para filesystem, network, clock,
random, process, environment, storage e devices. A enforcement boundary depende
do target:

- type system e HIR dentro do programa;
- processo/OS sandbox para código nativo não confiável;
- Wasm/component boundary para isolamento portátil quando compatível;
- seccomp, namespaces, sandbox-exec ou job objects como defense-in-depth local.

Seccomp não pode proteger “cada módulo importado”. Um módulo não é uma boundary
física, e um filtro de syscall não controla memory safety dentro do processo.

### 13.8 Wasm e playground

Wasm é um target e uma boundary possível. Ele não transforma W em substituto de
JavaScript. O playground compila um subset para Wasm e usa imports/exports
tipados do host. DOM, network e storage só existem quando o profile concede.

### 13.9 Observabilidade

Cada task, service call e instance pode fornecer:

- trace/span e causalidade;
- queue wait, execution time e suspension time;
- allocation/budget facts;
- hop local/remoto;
- cancellation e error primário/adicional;
- logical stack e source mapping;
- identity de build, module e package.

Logs humanos são projeções. Eventos estruturados possuem schema e limites de
privacidade.

## 14. Prelude e SDK

### 14.1 T0 — core independente do ambiente

T0 contém:

- tipos primitivos, Option, Result e Error;
- String, Array, Map, Set, Range e views;
- protocols de igualdade, hash, iteração, ownership, Send e Sync;
- operações puras de texto, collection e matemática básica;
- intrinsics necessários para memória segura e compile time.

T0 pode usar o runtime/allocator do target. Ele não depende de console,
filesystem, rede, clock, locale ou OS API.

### 14.2 T1 — systems e adapters comuns

T1 contém:

- console e `print`;
- process, environment, filesystem e paths;
- clock, calendar, timezone e random;
- tasks, synchronization e blocking adapters;
- TCP, UDP, TLS e DNS;
- crypto, codecs, JSON e FFI C;
- storage e observabilidade básicas.

`print` é um nome normal da prelude T1. Ele só está disponível num scope de host
que concede Console. Uma função exportada fora desse scope recebe `Console`
como capability explícita.

### 14.3 T2 — domínios oficiais

T2 contém módulos bundled e reachability-linked:

- HTTP client/server, CLI/TUI e adapters de UI;
- SI, quantities, análise numérica e constants versionadas;
- tensor, linear algebra e ML experimental;
- SQLite/durable adapter;
- formatos e protocols de domínio com suporte oficial.

Álgebra simbólica completa, CAS, dataframe e stacks de vendor continuam packages
first-party experimentais. Tier não significa import implícito, linking ou
disponibilidade universal em todo target.

## 15. Números, ranges e unidades

### 15.1 Números

- literal inteiro sem contexto usa `Int`;
- literal decimal sem contexto usa `f64`;
- o expected type pode especializar um literal representável;
- operadores binários exigem o mesmo tipo depois de uma conversão segura única;
- overflow de inteiros, divisão por zero e shift inválido causam panic;
- APIs `checked`, `wrapping` e `saturating` tornam outra policy explícita;
- divisão inteira trunca em direção a zero;
- resto possui o sinal do dividendo;
- float usa IEEE strict, preserva subnormals e não reassocia por default;
- NaN segue comparação IEEE; total ordering exige API nomeada.

`fast` e `reproducible` começam como APIs/scopes T2 explícitos. Uma flag de
release não muda a semântica numérica sozinha.

### 15.2 Ranges

```w
1...5
1..<5
1>..5
1>..<5
```

Range é intervalo. `in` testa membership sem allocation:

```w
next in (.preparing, .cancelled)
temperature in 30<degC>...300<degC>
```

O tuple depois de `in` é um conjunto finito intrínseco. Flags usam `hasAny` e
`hasAll`. `where` refina um pattern; ele não substitui `&&` em um `if`.

Somente tipos discretos/strideable podem iterar um Range. Outros usam
`stride`. `clamp` exige um intervalo fechado.

### 15.3 Delimitador de unidade

A DB2 compara quatro formas:

| Forma | Estado DB2 | Motivo |
|---|---|---|
| `9.81<m/s^2>` | **Líder experimental** | preserva `[]`, comunica aplicação estática e possui precedente no F# |
| `9.81[m/s^2]` | **Reserva DB1** | parse simples, mas parece indexação e sobrecarrega `[]` |
| `9.81{m/s^2}` | **Rejeitado por enquanto** | chaves devem continuar a indicar body/scope |
| `9.81 m/s^2` | **Pesquisa** | aproxima SI, mas não mostra onde a unit expression termina |

O [F#](https://learn.microsoft.com/en-us/dotnet/fsharp/language-reference/units-of-measure)
usa angle brackets em quantidades e apaga units no runtime. Isso é um precedente,
não uma prova de preferência.

Na DB2, o literal exige adjacência:

```w
let gravity = 9.80665<m/s^2>
let setpoint = -40<degC>
let memory = 64<KiB>
```

A produção aceita somente um literal numérico com sinal opcional, seguido por
`<unit-expression>`. Uma expressão runtime não usa essa forma. Ela usa
`Quantity(value, unit: m)`.

Dentro da unit expression, a DB2 aceita nomes de units, `*`, `/`, `^`,
parênteses e expoentes inteiros. `^` continua XOR fora desse contexto. Nomes
qualificados são permitidos. O resolver aceita somente símbolos de kind `Unit`.

### 15.4 Units customizadas

Uma nova dimensão e sua unit base:

```w
dimension Applause
unit clap: Applause
unit ovation = 1_000<clap>
```

Uma unit linear derivada:

```w
unit smoot = 1.7018<m>
unit kiloSmoot = 1_000<smoot>
```

O RHS precisa ser uma constante exata e dimensionalmente válida. Prefixos não
são gerados automaticamente para units customizadas. A biblioteca pode gerar
declarações explícitas e detectar colisões.

Units afins usam um metaconstrutor explícito:

```w
unit degC = Unit.affine(reference: K, scale: 1, offset: 27315/100)
unit degF = Unit.affine(reference: K, scale: 5/9, offset: 45967/180)
```

O contrato é `referenceValue = value * scale + offset`. Um point e um delta são
tipos diferentes. Somar dois temperature points é erro. Point menos point
produz delta.

Units logarítmicas usam `Unit.logarithmic` e produzem `Level`, não uma Quantity
linear. Currency e duração calendárica não são units físicas. Money exige
currency, rate, instante e fonte explícitos. Mês e ano pertencem a Calendar.

```w
unit smoot = 1.7018<m>

let measured = Quantity(input, unit: smoot)
let meters = measured.converted(to: m)
```

Import de unit usa o mesmo sistema de módulos. A metadata registra dimensão,
escala, offset, símbolo e origem versionada. Units são apagadas quando
reflection/formatting não as alcança.

Sugars como `90C`, `90°F`, `5km` e `64KiB` continuam num mapa da edição. Tooling
mostra a expansão. Source gerado e API pública preferem a forma delimitada.

## 16. Texto e collections

`String` contém UTF-8 válido em buffer owned contíguo. SSO pode existir, mas é
invisível. COW não é contrato.

Views explícitas:

```w
text.bytes
text.scalars
text.graphemes
```

Cada view possui índice próprio. `String` não aceita `text[i]`. Slice preserva
uma boundary válida para a view escolhida. Equality de String compara a sequência
UTF-8; normalização Unicode é uma operação explícita.

Literais:

```w
"text ${value}"
#"raw \ ${notInterpolation}"#
"""
multiline
"""
'λ'       // UnicodeScalar
b'A'      // u8 ASCII
```

Um scalar literal contém exatamente um Unicode scalar. Um byte literal aceita
ASCII ou escape de byte. Grapheme clusters continuam String.

Collections:

```w
[1, 2, 3]
["red": 0xff0000]
(x: 10, y: 20)
[u8; 4096]
```

Arrays e maps usam `[]`. Records anônimos usam `()`. Sets usam `Set(...)`; não
ganham literal com chaves. Um slice de array retorna view borrowed por default.

## 17. Matrizes, tensors e ML

Nested arrays são a forma canônica:

```w
let transform: Matrix<f32, rows: 2, columns: 3> = [
  [1.0, 0.0, 10.0],
  [0.0, 1.0, 20.0],
]
```

`[1 2; 3 4]` fica preservado como alternativa. Ele perde na DB2 porque usa
whitespace e semicolon como parte do shape e não generaliza com simplicidade.

Shapes usam value parameters:

```w
fn classify<const batch: usize>(
  input: ref Tensor<f32, shape: [batch, 784]>,
  weights: ref Tensor<f32, shape: [784, 128]>,
): Tensor<f32, shape: [batch, 128]> {
  return input @ weights
}
```

Regras:

- `*`, `/`, `+`, `-` são elementwise para shape igual;
- scalar expansion é total;
- `@` é matrix/tensor contraction;
- broadcast entre shapes diferentes é explícito;
- slicing retorna `TensorView`;
- `copy()` materializa;
- host/device transfer é explícita;
- random recebe generator/seed;
- reduction declara a policy numérica;
- autodiff é transformação tipada de biblioteca/IR, não annotation.

StableHLO e ONNX são adapters. Eles não definem a semântica completa de W.

## 18. FFI, unsafe e ilhas de linguagem

### 18.1 Fronteira C

```w
foreign c from "sensor.h" {
  type Sensor
  struct Sample {
    value: c.double
    status: c.int
  }
  fn sensor_read(sensor: c.ptr<Sensor>, output: c.ptr<Sample>): c.int
}
```

Layouts dentro de `foreign c` seguem a ABI C do target. Calls e raw pointer
operations exigem `unsafe`:

```w
fn read(sensor: ref SensorHandle): Sample throws SensorError {
  unsafe {
    // chama C, valida status e restabelece invariantes W
  }
}
```

`unsafe fn` move essa obrigação para o caller. Conformance manual a `Send` ou
`Sync` também exige unsafe.

O importer v0 aceita funções, enums, structs simples, opaque types, pointers,
arrays e callbacks com context. Varargs, bitfields e unions exigem wrapper ou
override explícito. Cada allocation mantém o deallocator de origem.

### 18.2 `fn<Language>`

```w
unsafe fn<C> legacyChecksum(data: c.ptr<const c.uchar>, size: c.size): c.uint {
  // body C hermético
}
```

A assinatura usa tipos W/C conhecidos pela fronteira. O body é entregue ao
frontend C fixado pela receita. Includes, flags, target, diagnostics, source map
e provenance são inputs declarados.

`fn<C>` é uma ilha da aplicação, como inline assembly, não uma importação de
biblioteca externa. Um frontend adicional só entra quando possui parser,
toolchain, ABI, runtime, ownership adapter e lowering herméticos. Gerar LLVM IR
não basta para tornar duas linguagens compatíveis.

## 19. Compilador e bootstrap

### 19.1 Planos do sistema

| Plano | Responsabilidade |
|---|---|
| linguagem | source, tipos, effects, ownership e comportamento |
| implementação | frontend, HIR, verificadores, lowering e backends |
| execução | cleanup, tasks, domains, entries, I/O, panic e services |
| SDK | T0, T1 e T2 |
| build/package | resolução, profiles, cache, recipes e artefatos |
| distribuição | mirrors, updates, provenance, rebuilds e policy |
| experiência | formatter, docs, tests, LSP, debugger, lens e portal |

Governança atravessa todos os planos. Uma implementação não transforma sua AST,
ABI interna ou scheduler em semântica pública por acidente.

### 19.2 Pipeline

```text
UTF-8 source
  → lexer lossless
  → CST com recovery e spans
  → AST
  → HIR tipada com ownership, effects e tasks
  → dialeto W/MLIR
  → dialetos MLIR de domínio
  → LLVM | Wasm | EmitC de inspeção
```

O parser normativo usa recursive descent e Pratt, com EBNF publicada. Tree-sitter
é a projeção incremental para editores. Ambos compartilham tokens, corpus e
fixtures; eles não compartilham uma CST semântica obrigatória.

O frontend lossless retém trivia, source ranges e recovery nodes. AST remove
detalhes puramente sintáticos. HIR registra:

- símbolos, generics, constraints e overload escolhido;
- tipo, conversion e numeric policy;
- initialization, ownership, borrow e drop edges;
- errors, cancellation, panic e cleanup scopes;
- task parent/child, sendability e execution preference;
- effects/capabilities;
- layout/ABI boundaries;
- source map, diagnostic origin e expansion de sugars.

Um verifier rejeita HIR incompleta antes do lowering.

### 19.3 Dialeto W/MLIR

O dialeto W mantém as invariantes que LLVM e os dialetos genéricos não conhecem.
Passes só apagam uma distinção depois de prová-la. O lowering pode usar:

- `func`, `arith`, `math`, `cf` e `scf`;
- `async` como ferramenta de state machine;
- `memref`, `bufferization` e LLVM para memória;
- `tensor`, `shape`, `linalg` e `vector` para ciência;
- `gpu`/SPIR-V/vendor adapters quando o profile permite;
- EmitC para auditoria ou bootstrap limitado.

MLIR bytecode é cache do toolchain, não formato público eterno.

### 19.4 ABI e runtime

Há quatro contratos:

1. API source;
2. interface compilada para type-check/cache;
3. ABI W keyed por toolchain, target e profile;
4. ABI C explícita.

Source-first é o fallback. A interface compilada possui schema, reader e
fingerprint. ABI W só é aceita quando a chave completa confere.

`libwrt` é uma família reachability-linked:

- `core`: panic, allocation hooks, metadata e cleanup;
- `task`: scopes, cancellation, timers e executors;
- `platform`: filesystem, network, clocks e target adapters;
- `service`: instances, mailboxes, calls e durability;
- `observe`: tracing, symbolization e logical task stacks.

Um target freestanding pode usar somente `core`.

### 19.5 Bootstrap

O seed portátil usa C11, CMake e Ninja. Ele compila o primeiro subset W. O
compilador se torna self-hosted cedo. Versões posteriores usam uma versão W
anterior e mantêm o seed C como rota de auditoria.

MLIR fica atrás de um adapter C estreito. C++ e TableGen podem implementar o
dialeto sem contaminar toda a base self-hosted. EmitC não define a semântica de
W.

O seed é validado em dois compiladores C e dois targets comuns. Builds em estágios
comparam outputs e preservam divergências. Dependable C informa a matriz e o
estilo do seed; ele não é um dialeto nem autoriza undefined behavior.

### 19.6 Incrementalidade

Cache é content-addressed por source normalizado, interface das dependências,
edition, target, profile, toolchain e flags semânticas. Type checking ocorre por
módulo. Instâncias generics e outputs de passes possuem chaves próprias.

Um cache miss afeta performance, não resultado. A ferramenta registra o motivo
do miss. Ela não usa timestamps como identidade.

### 19.7 Diagnostics e debug

Diagnostics possuem código estável, spans, related spans, fix-its e saída
estruturada. O compilador preserva a regra violada até produzir o diagnóstico.

Debug symbols ficam em sidecar removível. Logical task stacks e source maps de
`fn<Language>` preservam a origem. Remover debug não remove reflection solicitada
nem muda o payload executável além das sections declaradas.

### 19.8 Targets e profiles

Uma linha de suporte declara architecture, operating environment, ABI, profile,
SDK capabilities, compiler/runtime version, status e test evidence.

| Tier | Garantia |
|---|---|
| experimental | subset declarado, sem garantia de upgrade |
| tier 3 | build conhecido e manutenção comunitária |
| tier 2 | CI compila e executa o corpus relevante |
| tier 1 | CI obrigatória e releases oficiais |
| long-term | janela de suporte e policy de segurança/depreciação |

Tier não mede a segurança de um programa. Um target estreito pode ser correto
sem oferecer rede, threads, dynamic linking ou Unicode completo.

## 20. Packages, builds e releases

### 20.1 Manifest e resolução

`package.w` usa um subset data-only. Ele aceita records, lists, strings,
numbers, booleans e enum values. Ele não executa imports, loops, funções ou I/O.

```w
package {
  schema: "w.package/1"
  name: "last-light/restaurant"
  version: "0.1.0"
  edition: "2026"

  products: [
    {
      name: "last-light"
      kind: .executable
      module: "restaurant.app"
      entry: "LastLight"
    },
  ]

  modules: [
    { name: "restaurant.app", sources: ["src/app/**.w"] },
    { name: "restaurant.core", sources: ["src/core/**.w"] },
  ]

  dependencies: [
    {
      alias: "http"
      package: "w/http"
      version: "^1.0"
      source: .registry("w")
    },
  ]

  build: {
    network: .deny
    environment: []
  }
}
```

Unknown fields são erro. Extensões usam namespace. O parser do manifest é menor
e independente do parser W completo.

- `package.w` é um formato data-only;
- `package.lock` é obrigatório para build reprodutível;
- o resolver é determinístico e registra sua versão;
- a DB2 usa uma versão por package identity em cada product;
- o resolver escolhe a maior versão compatível no snapshot assinado;
- pre-release exige opt-in;
- aliases são locais e não mudam identity;
- múltiplas versões ficam fora da v0;
- features são aditivas, locais à instância e entram na chave do artefato.

O algoritmo inicial deve ser PubGrub ou outro solver que produza explicações
equivalentes e determinísticas. O resultado, não o nome do algoritmo, é o
contrato.

O lock registra versões, digests, origem imutável, grafo, features, target,
profile, artifact key, toolchain, provenance e snapshot de metadata. Alteração
manual invalida o arquivo.

### 20.2 Build

- source é o fallback normativo;
- binaries são otimização sob uma chave ABI completa;
- static linkage é preferido quando compatível;
- build scripts não recebem rede ou filesystem irrestrito;
- code generation é uma tool target hermética;
- cache é content-addressed;
- recipe fixa toolchain, target, profile, inputs e environment permitido;
- CBOR determinístico é a representação canônica inicial;
- SHA-256 tagged é o digest inicial e possui algorithm agility.

`w build --locked` falha se manifest, context ou lock divergirem. CI/release usa
esse modo. `w update package` mostra o diff mínimo do grafo. Offline não acessa a
rede; frozen pode buscar somente objetos já fixados.

Mesma fonte, recipe e ambiente fixado devem produzir o mesmo payload bit a bit.
Data, commit, paths, locale, timezone, seeds e environment são inputs explícitos
ou são removidos.

### 20.3 Verificação

O sistema separa:

1. bytes determinísticos do payload;
2. envelope e assinatura da plataforma;
3. provenance e attestations;
4. autorização do maintainer;
5. reprodução independente;
6. análise de segurança.

O payload pode conter uma nota W mínima com identity e referências por digest.
Ele não contém um JWT autorreferente. OS signing e notarization ficam no
envelope.

Registry publica facts e metadata assinada. Mirrors transportam bytes por
digest. Uma versão pode ter estados independentes para autorização, reprodução,
provenance, auditoria, advisories e revogação. A UI não combina tudo numa nota
de estrelas que esconda o motivo.

Código fechado pode provar que um builder autorizado o reconstruiu. Ele não pode
alegar reprodução pública sem acesso independente ao source.

Maintainers autorizam releases. Builders atestam recipes. Registries publicam
snapshots. Plataformas assinam envelopes. Chaves e policies para esses papéis não
devem ser a mesma authority.

Metadata deve suportar delegação, threshold, expiração, rotação, revogação e
proteção contra rollback/freeze. TUF e Sigstore são integrações preferidas, não
keywords nem raízes universais.

### 20.4 Registry, mirrors e estado de segurança

Registry governa identity e metadata. Mirror hospeda bytes por digest. Trocar
GitHub Releases, CDN, bucket ou cache corporativo não troca o package.

O portal mostra eixos separados:

- maintainer authorization;
- source availability;
- provenance;
- independent reproduction e quorum;
- SBOM/license;
- static analysis e auditoria humana;
- advisories, yank e revocation;
- freshness da metadata.

“Verificado” sempre informa qual eixo e qual policy. Uma estrela agregada não é
evidência técnica.

### 20.5 Scripts e supply chain

Install scripts arbitrários são rejeitados. Tool targets de geração declaram
inputs, outputs e capabilities. Network é denied por default. Outputs entram no
CAS e na provenance.

Dependências transitivas não recebem capabilities do app. Build-time tools e
runtime dependencies aparecem como relações distintas no SBOM.

### 20.6 CLI

```text
w resolve
w update <package>
w fetch --locked
w build --locked
w test --locked
w explain dependency <package>
w diff-lock
w verify <artifact>
w reproduce <release>
w bundle offline
w cache import <bundle>
```

Saída humana é curta. `--json` fornece o grafo, diagnostics e evidências
completos.

### 20.7 Evolução e governança

Cada package declara uma edition. Editions podem alterar grammar, prelude e
lints com migração automatizada. Elas não mudam resultado ou effects
silenciosamente.

Schemas de interface, lock, package, diagnostics e provenance têm readers
versionados. Deprecation informa replacement e janela de remoção. O projeto
publica suporte de targets e security policy.

Identidade do nome W, domínio, executable e trademark precisa de validação antes
do lançamento público. A frase “A última linguagem que você vai precisar
aprender” fica como alternativa de marca; não é promessa técnica.

## 21. Tooling e interface para máquinas

### 21.1 Tooling humano

- `w fmt` produz a forma canônica de 120 colunas;
- `w check` não gera artefato final;
- `w test` reúne unit, doc, compile-fail, property e fuzz;
- `w explain` mostra resolução, tipos, moves, layout, effects e custos;
- `w build --locked` usa somente o grafo fixado;
- `w audit` verifica policy, advisories, provenance e reprodução.

LSP usa a HIR para semantic tokens, tipos, effects e rename. Tree-sitter mantém
realce e estrutura durante source incompleto. TextMate é fallback.

Formatter:

- largura preferida de 120 colunas;
- indent de dois espaços;
- construction numa linha quando cabe;
- trailing comma em listas multilinha;
- uma declaração/statement por linha;
- LF e espaços, sem tabs na forma canônica;
- comentários preservados e anexados de forma determinística;
- `w fmt` idempotente.

#### Portal gerado

O portal atual é um **protótipo congelado**. Ele demonstra direção visual, tema,
navegação e playground lexical. Ele não é uma segunda documentação e não precisa
acompanhar cada mudança durante o endurecimento da linguagem.

Depois do design freeze, um gerador usa:

1. a estrutura Markdown e o registro D2 deste arquivo;
2. snippets extraídos dos arquivos `.w` canônicos;
3. resultados estruturados da grammar, formatter e testes;
4. metadata de versão e provenance do build.

O source do portal não duplica contratos nem snippets. Cada bloco gerado aponta
para arquivo, região e revisão de origem. CI falha quando um include desaparece,
um snippet não parseia ou um ID D2 não existe.

Astro é uma opção de renderização, não uma decisão de arquitetura. Ele só será
comparado depois do freeze com um gerador mínimo. A escolha depende de build
hermético, output estático, acessibilidade, dependências, tempo de build e
facilidade de remover o framework.

### 21.2 Documentação e testes

`///` documenta a próxima declaração. Fences `w test` são doctests. Testes
co-localizados usam:

```w
test "range preserva valor interno" for clampRatio {
  expect clampRatio(0.5) == 0.5
}
```

Suites maiores usam `*.test.w`. Compile-fail fixtures possuem diagnostic code e
spans esperados. Property/fuzz tests registram seed e limits. Testes não entram
no release payload.

O runner produz um grafo reproduzível. Snapshot e golden são arquivos explícitos
e revisáveis. IA pode propor testes em diff; ela não substitui o oracle aceito.

### 21.3 Lens de recursos

Uma previsão por import separa:

- delta do artefato e reachability;
- custo estático;
- custo por instância;
- custo por operação;
- intervalo ou unknown;
- fact, estimate ou measurement;
- proveniência e target/profile.

O lens nunca apresenta memória runtime geral como número exato quando isso não
pode ser provado. Budgets são contratos separados.

Feedback medido pode melhorar estimates locais. Ele é opt-in, vinculado à recipe
e não envia source/dados sem consentimento.

### 21.4 Interface para modelos

W não possui uma sintaxe curta exclusiva para IA. Em vez disso, oferece:

- formatter determinístico;
- grammar e schema versionados;
- diagnostics estruturados;
- interface compilada com tipos, effects e ownership;
- `w explain --json`;
- exemplos positivos e negativos;
- testes de compilação e comportamento;
- source maps para código gerado e ilhas de linguagem.

Contagem de tokens depende do tokenizer. A DB2 mede vários modelos antes de
trocar uma keyword por pontuação. Compile success, testes e edit distance têm
mais peso que token count isolado.

## 22. Protocolos e pesquisas de ecossistema

Nenhum item desta seção reserva keyword. Cada hipótese usa os contratos do core
e pode evoluir como package separado.

### 22.1 Contrato tipado e wRPC

Um protocol de service pode gerar um contrato independente de transporte. O
contrato registra operações, inputs, outputs, error sets, idempotência, limits e
schema dos tipos alcançáveis.

wRPC é um possível envelope de call, não query language nem codec. O mínimo
possui protocol version, message kind, call ID, service ID, operation ID, codec,
metadata, payload e tamanho validado antes da alocação.

Retry mutante não é implícito. Deadline/cancelamento não promete rollback.
Falhas de aplicação, transporte, codec, protocol e autorização são distintas.

### 22.2 JSON, WLO e wStruct

- JSON é o primeiro codec de interoperabilidade e debug.
- WLO/WLON é pesquisa de formato data-only canônico para valores W.
- wStruct pesquisa IPC sob target, ABI e layout idênticos.

WLO precisa de grammar menor que W, canonical bytes, limits e fuzzing. wStruct
não serializa pointers, padding ou handles crus. Ambos precisam de fallback.

### 22.3 wQL e RestPC

wQL começa como AST tipada com query, command e introspection. Uma DSL textual
só entra depois. Parameters ficam separados do texto. Projection, pagination,
authorization, cost e partial failure são explícitos.

RestPC é um profile HTTP/JSON:

- GET para leitura;
- POST para call/create;
- PUT para replace;
- DELETE para remoção;
- OPTIONS para introspection autorizada.

Ele não finge que toda operação é um resource. Typed errors e HTTP status são
camadas distintas. GraphQL, SQL e OpenAPI não são compatibilidade automática.

### 22.4 V6 e Computer Units

V6 continua pesquisa de runtime/host serverless. Computer Unit é uma instância
com entrypoints, limits e capabilities. Esses conceitos podem hospedar services
W, mas não definem a linguagem, wRPC ou o package manager.

### 22.5 Tree strings

Tree strings continuam uma estrutura especializada para interning, índices ou
edição. `String` público permanece UTF-8 contíguo. Codec e ABI observam o valor
lógico, não a representação experimental.

### 22.6 GPU, HDL, PGO e geração assistida

GPU começa por um kernel puro com baseline CPU, device transfer explícita e
comparação de resultado/custo. HDL exige um modelo próprio de timing e
verificação; não é lowering automático de código CPU.

PGO, snapshots e casos gerados por IA são artefatos de tooling. Seed, workload,
provenance e oracle são explícitos. Nenhum deles muda a semântica source.

### 22.7 Gate de promoção

Uma pesquisa só avança quando possui:

1. problema e métrica;
2. implementação pequena e fallback;
3. alternativa mais simples;
4. erro, cancelamento, FFI e dois targets;
5. impacto em parser, formatter, metadata e packages;
6. decisão registrada por diff e teste.

## 23. Classificação de viabilidade

| Família | Classe DB2 | Motivo |
|---|---|---|
| owner único, borrow e whole-value move | **Possível agora** | análise e lowering conhecidos |
| `async let`/`spawn let` estruturados | **Possível agora** | state machine e runtime mínimo delimitados |
| modules sem lifecycle e imports herméticos | **Possível agora** | contrato estático simples |
| UTF-8 owned e views | **Possível agora** | representação portátil com fallback |
| strict numerics e overflow verificado | **Possível agora** | backend oferece operações adequadas |
| services serial-turn e `ServiceRef` async | **Provável** | exige protótipo de mailbox, deadlock e trace |
| `<unit>` e units customizadas | **Provável** | type/lowering coerentes; ergonomia precisa de corpus |
| refinements e value parameters | **Provável** | exige evaluator, proof budget e ABI identity |
| property behaviors | **Provável** | expansão HIR é viável; composição ainda precisa de teste |
| entries e host profiles | **Provável** | binding é claro; adapters precisam de schemas |
| tensors ranked, `@` e views | **Provável T2** | MLIR ajuda; API e device model precisam de protótipo |
| tagged pointers e high-bit addresses | **Pesquisa** | target-specific e sem vantagem sem benchmark |
| mimalloc universal | **Pesquisa** | profile possível; default exige matriz de targets |
| SQLite como durability universal | **Rejeitado** | adapter oficial é útil; semântica universal não é portátil |
| seccomp por módulo importado | **Rejeitado** | import não é uma security boundary |
| sandbox portátil por process/Wasm | **Provável** | depende do host, mas preserva o contrato |
| `fn<Rust>`/`fn<Swift>` | **Pesquisa** | toolchain/runtime/ABI maiores que C |
| álgebra simbólica completa no core | **Rejeitado** | package T2 experimental preserva evolução |
| custom operators e precedência do usuário | **Rejeitado** | piora parser, tooling e previsibilidade |
| macros/annotations universais | **Rejeitado** | cria uma segunda linguagem e hidden behavior |

## 24. Ensaio do restaurante cósmico

O corpus DB2 usa um restaurante original de escala cósmica. Ele não copia
personagens, frases ou eventos de outra obra. O humor vem de situações técnicas:
reservas em fusos relativísticos, cozinha térmica, estoque por telemetria,
previsão tensorial, cobrança idempotente e burocracia de encerramento.

O gate final é o **Turno do Horizonte Violeta**:

```text
entry
  → parser streaming de comanda
  → DiningRoom service
  → Kitchen service
  → async stock + spawn planning
  → controle térmico com units
  → tensor de previsão
  → sensor C
  → billing e compensação
  → HTTP/TUI response
  → cleanup, trace e provenance
```

Uma injeção de falha em cada seta não pode deixar task, lease, buffer, mailbox
item ou pagamento sem estado observável.

O ensaio detalhado está no
[Restaurante Última Luz](examples/restaurant/README.md).

## 25. Protocolo de revisão

Cada comparação usa o mesmo programa e quatro tarefas:

1. explicar o que o código faz;
2. reproduzir uma parte após intervalo curto;
3. corrigir um erro preparado;
4. mudar um requisito sem reescrever o programa.

Métricas humanas:

- acerto semântico;
- erro de sintaxe e de runtime esperado;
- tempo e número de consultas;
- confidence;
- preferência, medida por último.

Métricas de modelos:

- parse e type-check;
- testes aprovados;
- diagnostics necessários até a correção;
- tokens de source e de contexto;
- edit distance da forma canônica;
- consistência entre modelos e tokenizers.

O corpus compara, no mínimo:

- units `<>` contra `[]`;
- `spawn on` contra `spawn<...>`;
- refinement postfix contra as formas angulares;
- closure `=>` contra `fn(...)`;
- nested matrix contra `;`;
- import de namespace compacto contra a forma DB1 com `from`.

## 26. Plano de implementação

Cada fase entrega um corte vertical, tests e uma demonstração reproduzível no
corpus ou na CLI. O portal gerado começa somente depois do design freeze.

### 26.1 Fase -1 — design e corpus

- consolidar este documento;
- criar corpus DB2 positivo, negativo e comparativo;
- completar o restaurante cósmico;
- fixar diagnostic IDs e formatter examples.

Saída: toda forma implementada possui contrato, alternativa e teste.

### 26.2 Fase 0 — lexer, parser e formatter

- lexer lossless;
- recursive-descent/Pratt;
- EBNF;
- CST/recovery;
- formatter idempotente;
- Tree-sitter e semantic highlight projetados do corpus.

Saída: parse/format/parse estável e diagnostics preparados.

### 26.3 Fase 1 — AST, nomes e tipos

- AST e module graph;
- imports/visibility;
- primitives, structs, enums, functions, Option e error sets;
- inference local, generics mínimos e refinements;
- interface compilada inicial.

Saída: `w check` verifica o subset síncrono do restaurante.

### 26.4 Fase 2 — HIR, MLIR e executável nativo

- HIR tipada;
- dialeto W/MLIR e verifiers;
- arithmetic/control lowering;
- LLVM/native e runtime core;
- seed C gera e executa o primeiro programa.

Saída: payload determinístico para programas síncronos.

### 26.5 Fase 3 — memória, errors e C

- initialization e whole-value move;
- borrows, drop e defer;
- typed errors e panic boundary;
- allocator hooks;
- `foreign c`, unsafe e wrappers.

Saída: sanitizers e corpus negativo não encontram dangling/double drop.

### 26.6 Fase 4 — tasks

- async state machine;
- `async let`, `spawn let` e domains;
- linear Task, task groups e cancellation;
- executor cooperativo e pool paralelo bounded;
- deterministic test executor.

Saída: restaurante executa I/O concorrente e planejamento paralelo com cleanup.

### 26.7 Fase 5 — services e host entries

- `entry` e host profiles;
- service instance manager;
- serial turn, mailbox e ServiceRef;
- tracing e local fast path;
- process/Wasm boundary experimental.

Saída: CLI e HTTP usam o mesmo service e exibem hops/queues.

### 26.8 Fase 6 — packages e SDK

- package parser, resolver, lock e CAS;
- builds `--locked`/offline;
- T0/T1 mínimos;
- provenance, SBOM e reprodução local;
- lens por import.

Saída: uma máquina limpa reconstrói o mesmo payload sem rede durante o build.

### 26.9 Fase 7 — ciência, self-host e extração

- units/refinements completos;
- tensor CPU e `@`;
- self-host W;
- rebuild em estágios;
- suíte de conformidade;
- decisão sobre mover W para repository próprio.

Saída: DB2 demonstrada de ponta a ponta e pronta para revisão pública.

### 26.10 Gates

| Gate | Pergunta | Evidência mínima |
|---|---|---|
| memória | `shared`, arena e allocator compõem sem surpresa? | benchmarks, cycles, FFI e cancellation |
| tasks | lowering preserva erro, cleanup e sendability? | testes diferenciais e stress |
| services | closed turn evita races sem deadlock inaceitável? | três workloads e trace |
| units | `<>` supera `[]` em uso real? | estudo humano e modelo |
| ML | shape/operator reduzem erros sem esconder cost? | corpus CPU/SIMD/device |
| packages | resolver e evidence model são operáveis? | projeto real offline/reproduzido |
| self-host | seed e stages são auditáveis? | builds diversos e diff de outputs |

### 26.11 Checkpoint por fase

Cada checkpoint executa:

1. testes afetados e corpus negativo;
2. formatter idempotente;
3. comparação debug/release;
4. benchmark proporcional ao risco;
5. `git diff --check`;
6. atualização deste documento;
7. commit pequeno com resultado e limitações.

## 27. Mudanças da DB1 para a DB2

| Tema | DB1 | Líder DB2 |
|---|---|---|
| unit literal | `9.81[m/s^2]` | `9.81<m/s^2>` |
| namespace import | `import name as alias from path` | `import path [as alias]` |
| refinement | `T where predicate`, com alternativas | `T where (predicate)` |
| execution preference | superfície aberta | `async/spawn on .domain` |
| entry | superfície aberta | forma curta + descriptor tipado |
| tensors | nested baseline, operadores abertos | nested + `@`, broadcast explícito |
| value generics | aberto | `const` parameters e labels declarados |
| unsafe | decisão sem grammar completa | `unsafe fn` e `unsafe {}` |
| async cleanup | lacuna | `defer async` |
| scalar literal | lacuna | `'x'` e `b'x'` |
| modules multi-file | DB1 ratificada, spec divergente | manifest é source of truth |
| resolver/digest | DB1 ratificada, docs divergentes | contrato consolidado |

Estas mudanças são experimentais. A fotografia completa da DB1 continua
acessível no
[arquivo histórico](../Y/W/archive/db1-2026-07-27/README.md) e no histórico do
Git.

## 28. Registro de decisões e alternativas

Esta tabela é o checklist de revisão humana. **Líder** significa “implementar e
experimentar”, não “decisão irreversível”.

| ID | Tema | Líder DB2 | Alternativas preservadas |
|---|---|---|---|
| D2-001 | função | `fn name(...): Return` | `func`; retorno `->`; sem keyword |
| D2-002 | bindings | `const`/`let`/`var` | `let mut`; uma única keyword |
| D2-003 | modifiers | ordem fixa antes de `fn` | ordem livre; effects após retorno |
| D2-004 | labels | primeiro posicional, demais nomeados | todos nomeados; todos posicionais |
| D2-005 | closure | `(args) => body` | `fn(args) {}`; `{ args in }` |
| D2-006 | capture | inferência + `capture(...)` | `[capture]`; somente inferência |
| D2-007 | visibility | private default, `package`, `export` | public default; `public/private`; block export |
| D2-008 | import seletivo | `{X} from path` | `path.{X}`; imports livres |
| D2-009 | import namespace | `import path as alias` | forma DB1 `name as alias from path` |
| D2-010 | módulos | manifest multi-file, DAG | declaração `module`; cycles de interface |
| D2-011 | runtime top-level | declarations/const somente | init global; ordem de inicializadores |
| D2-012 | tipos nominais | `type X = T` | wrapper struct; `newtype` |
| D2-013 | alias | `alias X = T` | `typealias`; context-dependent `type` |
| D2-014 | refinement | `T where (predicate)` | `T(where:)`; `T<where(...)>` |
| D2-015 | value generics | `const` parameters e labels | positional only; modifier map |
| D2-016 | existential | `any P` | `P` sozinho; `dyn P`; `Any` universal |
| D2-017 | opaque return | `some P` | existential; generic nomeado |
| D2-018 | reflection | conformance opt-in | metadata universal; annotations |
| D2-019 | Option | `T?` com some/none | null; sentinel; result-like |
| D2-020 | conversão | total, única e sem perda | tudo explícito; promotions amplas |
| D2-021 | owner | único/move-first | ARC universal; GC |
| D2-022 | borrow | `ref` e `inout` | lifetime annotations públicas; pointers |
| D2-023 | transfer | last-use + `take` obrigatório na API | move sempre explícito; move implícito amplo |
| D2-024 | copy | implicit só para `Copy`; `copy` deliberado | clone method universal; COW default |
| D2-025 | shared | `shared T` + `weak` | ARC implícito; region-only |
| D2-026 | region | API primeiro, bloco experimental | region annotations; heap por módulo |
| D2-027 | allocator | system portable, profile substituível | mimalloc universal; allocator por import |
| D2-028 | OOM | fallible explícito; geral aborta boundary | throws universal; abort de process sempre |
| D2-029 | layout | W opaco; C/schema explícitos | layout W estável universal |
| D2-030 | tagged values | otimização invisível com fallback | tagged address obrigatório; annotation |
| D2-031 | property behavior | `var Behavior name` | prefix before var; `by`; wrapper type |
| D2-032 | behavior composition | composite nomeado | lista ordenada; nesting arbitrário |
| D2-033 | erro | `throws E` + `try` | exceptions abertas; Result em toda assinatura |
| D2-034 | error widening | case único compatível | mapping sempre explícito; `From` livre |
| D2-035 | panic | abort da isolation boundary | unwind recuperável; abort process global |
| D2-036 | async cleanup | `defer async` | RAII sync only; `using`; cleanup solto |
| D2-037 | concorrência | `async let` | Future/Promise; task API somente |
| D2-038 | paralelismo | `spawn let` | mesma keyword de async; parallel loop apenas |
| D2-039 | execution domain | `async/spawn on .domain` | angle modifier; descriptor-only |
| D2-040 | Task | linear, lexical, one-await | Future clonável; detached default |
| D2-041 | grupos | lexical e bounded | queue ilimitada; thread pool exposto |
| D2-042 | cancelamento | statement cooperativo | method only; async thread cancellation |
| D2-043 | erro concorrente | primário lexical + anexos | primeiro a concluir; aggregate always |
| D2-044 | atomics | seq-cst default, orders explícitas | C-like default; lock implicit |
| D2-045 | Send/Sync | protocols derivados | annotations; runtime checks |
| D2-046 | service | keyword + protocol + closed turn | object+metadata; actor reentrant |
| D2-047 | service call | ServiceRef sempre async | local sync/remoto async; RPC explícito |
| D2-048 | mailbox | bounded com backpressure | drop; unbounded |
| D2-049 | entry curto | default slot único | main mágico; manifest-only |
| D2-050 | entry composto | descriptor de slots tipados | handlers inline; conformance |
| D2-051 | units | `9.81<m/s^2>` | `[]`; `{}`; whitespace SI |
| D2-052 | custom unit | `dimension`/`unit` declarations | wrapper types; runtime registry |
| D2-053 | affine/log units | metaconstrutores distintos | scale universal; runtime-only |
| D2-054 | range | quatro closures + interval semantics | dois ranges; producer universal |
| D2-055 | membership | `value in (a, b)` | `.isOneOf`; equality chain |
| D2-056 | exponent | `**`; `^` somente em unit grammar | `^` universal; `pow` only |
| D2-057 | integer safety | checked, panic; APIs alternatives | wrapping default; Result operators |
| D2-058 | float | IEEE strict default | fast default; build-mode semantics |
| D2-059 | String | owned UTF-8 contíguo | tree/rope default; COW contract |
| D2-060 | String indexing | views tipadas, sem `string[i]` | scalar index; grapheme index default |
| D2-061 | raw string | `#"..."#` | `r"..."`; backtick |
| D2-062 | scalar/byte | `'x'` e `b'x'` | constructor only; char=grapheme |
| D2-063 | arrays/maps | `[]` e `[key: value]` | braces para map/set |
| D2-064 | matrix literal | nested arrays | semicolon/whitespace; constructor only |
| D2-065 | matrix multiply | `@` | `*` + `.*`; `matmul` only |
| D2-066 | broadcast | diferente shape explícito | Array API implicit; dotted operators |
| D2-067 | device | transfer explícita | automatic placement |
| D2-068 | SDK | T0/T1/T2 | uma stdlib plana; packages somente |
| D2-069 | prelude | pequena, edition-frozen | toda std implícita; nada implícito |
| D2-070 | print | T1 contextual ao host | T0 intrinsic; `io.print` obrigatório |
| D2-071 | C | `foreign c` + unsafe wrapper | C superset; generated bridge only |
| D2-072 | inline language | `fn<C>` primeiro | library import; multi-language v0 |
| D2-073 | parser | recursive-descent/Pratt + EBNF | generated parser; Tree-sitter compiler |
| D2-074 | editor parser | Tree-sitter projection | compiler CST compartilhada |
| D2-075 | IR | W/MLIR antes de lowering | C IR público; LLVM direto |
| D2-076 | bootstrap | C11 seed, self-host cedo | TypeScript/Bun; C++ compiler inteiro |
| D2-077 | build tool | CMake/Ninja no seed | xmake; custom builder antes do self-host |
| D2-078 | packages | manifest data-only + lock | executable manifest; lock opcional |
| D2-079 | resolver | determinístico, uma versão por identity | múltiplas versões default |
| D2-080 | artifact | source-first, static preferred | binary-only; dynamic-only |
| D2-081 | canonical bytes | deterministic CBOR | WLO imediato; JSON assinada |
| D2-082 | digest | tagged SHA-256 inicial | hash fixo eterno; hash recebido sem metadata |
| D2-083 | registry | metadata authority; mirrors por digest | registry hospeda tudo e define trust |
| D2-084 | evidence | eixos separados | selo único; estrelas |
| D2-085 | resource lens | facts/estimates/measurements | número exato universal; nada no import |
| D2-086 | formatter | 120 colunas e uma forma | user-configurable style amplo |
| D2-087 | tests | runner único com modos | ferramentas sem grafo comum |
| D2-088 | AI | schemas/diagnostics comuns | dialeto AI; token count como objetivo único |
| D2-089 | SQLite | durable adapter T2 | storage universal |
| D2-090 | sandbox | capability + process/OS/Wasm | seccomp por módulo |
| D2-091 | wRPC/wQL | packages após core | keywords DB2; protocolo universal |
| D2-092 | WLO/tree strings | pesquisa com fallback | formato/representação default |
| D2-093 | GPU/HDL | lowerings posteriores | requisito da v0 |
| D2-094 | custom operators | rejeitado | precedência e operators do usuário |
| D2-095 | annotations/macros | rejeitado na v0 | `@annotations`; macro AST universal |
| D2-096 | portal | gerar das fontes após design freeze; protótipo congelado | manter páginas manuais; escolher Astro agora |

Uma revisão pode responder por ID. Uma mudança deve atualizar o exemplo, a
grammar, o formatter, o corpus e a seção semântica correspondente.
