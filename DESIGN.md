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

`<...>` marca uma aplicação estática conhecida pelo elemento à esquerda. Um
número aceita uma expressão de unidade. Um tipo aceita seus parâmetros
declarados. `fn<C>` aceita uma tag de frontend registrada.

Chaves não representam units, sets ou object literals. Essa restrição mantém
chaves como um sinal forte de scope.

### 3.1 Hipótese de contrato estático fechado

**Pesquisa.** Uma interpretação mais geral pode chamar `<...>` de cláusula de
contrato estático. Cada elemento à esquerda fornece um schema fechado. O schema
declara slots, tipos, defaults e cardinalidade.

Essa hipótese não cria um mapa universal de modifiers. Um head só aceita slots
que sua declaração ou o compilador publicou. Cada slot aparece no máximo uma
vez. Um argumento omitido precisa ter default ou inferência inequívoca.

“Contrato estático” é um nome útil na HIR. A documentação de source usa
“aplicação estática” enquanto W não tiver preconditions e postconditions. Isso
evita confundir `<...>` com Design by Contract ou com o contrato completo de
ownership de uma API.

Uma representação possível no compilador self-hosted é:

```w
enum StaticArgument {
  typeValue(TypeId)
  constValue(ConstValue)
  enumCase(EnumCaseId)
  unit(UnitExpression)
  predicate(PredicateId)
}

struct StaticSlot {
  name: Symbol
  kind: StaticKind
  defaultValue: ConstValue?
}

struct StaticContract {
  head: StaticHead
  slots: Array<StaticSlot>
}
```

`SourceLanguage`, `TaskKind` e `ExecutionDomain` são enums fechados na HIR. O
compilador não representa esses valores como strings ou nomes livres.

O frontend pode normalizar açúcares diferentes para records e enums desse
modelo. A HIR não precisa preservar uma construção especial para cada grafia:

```text
f64 where (value in 0.0...1.0)
  → TypeContract(base: f64, refinement: predicate)

spawn on .compute let plan = optimize(order)
  → TaskContract(kind: .parallel, domain: .compute)

unsafe fn<C> checksum(...)
  → FunctionContract(language: .c, safety: .unsafe)
```

Essa normalização separa a ergonomia do source da estrutura interna. O
self-host não exige que toda propriedade interna apareça dentro de `<...>`.

Os contratos estáticos seguem estas regras:

1. O head declara um schema fechado.
2. O evaluator aceita somente valores compile-time herméticos.
3. Um slot nomeado faz parte da interface source.
4. O formatter usa a ordem declarada pelo schema.
5. Argumentos posicionais precedem argumentos nomeados.
6. `w explain` mostra defaults, inferências e a forma normalizada.
7. Nenhum slot concede authority, memory safety ou capability implicitamente.

Nomes de argumentos melhoram leitura e permitem omitir parâmetros inferidos.
Eles também criam compatibilidade source. Renomear um slot público quebra seus
callers. O experimento de
[named type arguments do Scala 3](https://docs.scala-lang.org/scala3/reference/experimental/named-typeargs-spec.html)
documenta o mesmo custo.

Um schema pode declarar um slot posicional primário:

```w
spawn<.io> let report = reconcile()
```

Essa forma permanece **Pesquisa**. O compilador não procura um slot pelo nome do
case. O head precisa publicar qual slot é primário. A adição de outro slot não
pode reinterpretar source anterior. `spawn` já informa paralelismo, portanto o
schema não precisa de um slot `TaskKind`.

As aplicações estáticas públicas seguem estas regras adicionais:

1. Parâmetros de tipo posicionais continuam naturais em `Array<u8>` e
   `Map<K, V>`.
2. Um valor usa label quando a função do valor não é evidente pelo head.
3. Um enum case sem label só preenche o slot primário declarado pelo schema.
4. Dois slots do mesmo kind não usam inferência por nome de case.
5. Um slot repetido é sempre erro.
6. A ordem canônica pertence ao schema, não ao call site.

Essa regra permite representar os argumentos como enums e records no compiler
self-hosted. Ela não transforma `<...>` num mapa aberto de modifiers.

### 3.2 Formas angulares em avaliação

| Tema | Líder DB2 | Candidato de contrato | Observação |
|---|---|---|---|
| tipo e shape | `Tensor<f32, shape: [8, 4]>` | igual | já possui slots declarados |
| frontend inline | `fn<C>` | `fn<lang: .c>` | o label explica o papel de `C` |
| domínio | `spawn on .compute` | `spawn<domain: .compute>` | `on` lê como relação; `<>` escala para mais slots |
| domínio abreviado | — | `spawn<.compute>` | curto, mas depende de inferência contextual |
| refinement | `f64 where (predicate)` | `f64<where: (predicate)>` | postfix restringe; angular agrupa |
| unit literal | `9.81<m/s^2>` | `9.81<unit: m/s^2>` | o label não acrescenta informação no slot único |

`where` possui três usos relacionais: refinement, generic constraint e case
guard. A forma postfix mantém essa família visível. Ela também evita conflito
visual entre o fechamento `>` e comparações dentro do predicate.

`<where: (...)>` mantém toda a identidade do tipo em uma expressão compacta. Ele
também compõe bem dentro de outro generic. O corpus deve comparar as duas formas
antes de trocar a líder.

`on` informa placement sem expor uma policy record. A forma angular cresce
melhor se tasks ganharem vários slots estáticos. Policies runtime, como deadline
e executor escolhido pelo usuário, continuam em APIs normais.

Uniformidade interna não exige uniformidade gráfica. `where` e `on` são
cláusulas relacionais. `<...>` aplica argumentos ao head. As três formas podem
produzir o mesmo record HIR sem ter a mesma grammar.

Essa diferença também reduz o bootstrap:

- o parser reconhece `where` até o fim de uma expressão de tipo ou pattern;
- o parser reconhece `on` somente entre a intenção de task e o binding;
- o parser de `<...>` usa o schema do head depois da construção da AST;
- o type checker, não o lexer, resolve slots, defaults e enum cases.

`T<where: (...)>` faz sentido somente se `where` virar uma propriedade pública
do tipo. Hoje o predicate restringe um tipo já formado.
`spawn<domain: .compute>` faz sentido como aplicação estática, mas `spawn`
possui apenas um slot estático útil na DB2. `on` continua mais curto e informa a
relação.

| Critério | `where` / `on` | `<slot: value>` |
|---|---|---|
| leitura sem schema | nomeia a relação | exige conhecer o head |
| vários argumentos | não escala como record | escala com labels e defaults |
| predicate com `<`/`>` | recovery simples | disputa visual com o delimitador |
| HIR e self-host | normaliza para record | normaliza para o mesmo record |
| evolução | keyword mantém o papel | renomear slot quebra source |
| uso por modelos | forma comum e localizada | label reduz ambiguidade rara |

O gate reabre `spawn<domain: ...>` se `spawn` adquirir dois slots estáticos
públicos que não sejam redundantes. Deadline, budget runtime, prioridade
dinâmica e executor value não contam para esse gate.

**Líder DB2:** manter `where`, `async/spawn on`, `fn<C>` e unit sem label.

**Pesquisa:** implementar uma única representação HIR de contrato estático.
Comparar as grafias angulares antes de congelar a gramática.

### 3.3 Gate para trocar a forma líder

Uma grafia angular só substitui `where`, `on` ou a tag curta quando cumprir
todos estes critérios:

1. Leitores explicam o efeito sem consultar o schema.
2. Uma edição incorreta recebe um diagnostic que nomeia o slot.
3. O parser recupera depois de `<`, `>`, `<=` e generic nesting incompletos.
4. O formatter produz uma forma única sem esconder argumentos.
5. A adição de um slot não muda o significado de source existente.
6. Humanos e modelos corrigem o mesmo erro com menos tentativas.

O corpus usa três escalas: uma linha, uma assinatura nested e um módulo
completo. Token count é uma métrica secundária. Clareza semântica e estabilidade
de edição têm prioridade.

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

Campos usam uma forma mais curta:

```w
struct Ticket {
  id: TicketId
  var attempts: u8
  var Lazy summary = buildSummary()
}
```

Um campo sem prefixo é imutável depois da inicialização. `var` permite mutation.
`let field: T` não é uma segunda grafia. Essa forma economiza tokens sem perder
o contraste visual com estado mutável. Um campo sem initializer vira input
obrigatório do constructor ou do instance descriptor.

### 7.2 Funções

```w
export unsafe async fn update(
  order: inout Order,
  with event: take Event,
): Receipt throws UpdateError {
  // ...
}
```

A ordem canônica é:

1. visibilidade;
2. `static`, para member sem receiver;
3. `unsafe`, se necessário;
4. `mut`, para receiver mutável;
5. `async`;
6. `fn` ou `fn<Language>`.

`throws E` fica depois do return type. `Void` pode ser omitido.

O primeiro argumento é posicional por default. Os seguintes usam o nome como
label. Um label explícito substitui o default. `_` remove um label.

Dentro de type, protocol, service ou extension, `fn` recebe `self` por borrow.
`mut fn` recebe `self` com mutation exclusiva. `static fn` não recebe `self`:

```w
enum Course {
  soup
  cake

  static fn fromOrdinal(value: usize): Course { ... }
  fn isSweet(): Bool { ... }
}
```

`static mut fn` é erro. Um receiver consuming continua **Pesquisa**. A
alternativa atual é uma função livre com parâmetro `take`.

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
importantes. Os modos são `copy`, `ref`, `take` e `weak`. Uma closure armazenada
não captura `inout`.

Um child estruturado pode manter um borrow exclusivo passado como `inout`. O
owner e o task frame precisam ficar estáveis. O parent não acessa o valor antes
do join. Um runtime owner ou `SharedTask` não recebe esse borrow.

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
registro de alternativas. A seção 3.2 trata `T<where: (...)>` como candidato de
contrato fechado. As outras formas misturam construção e refinement sem declarar
essa relação.

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

### 9.1 Quatro contratos separados

W separa quatro contratos:

1. **Semântica:** owner, move, copy, borrow, shared, cleanup e erro.
2. **Lowering:** escape, placement, task frame, drop edge e specialization.
3. **Representação:** stack, register, heap, arena, niche, tag e allocator.
4. **Host:** quota, isolation boundary, telemetry e policy de OOM.

Somente o primeiro contrato define o significado normal do programa. Os outros
podem mudar por target ou profile sem alterar o resultado observável.

Todo valor que exige cleanup possui um owner. O compilador controla
inicialização, move, borrow, escape e drop. A escada semântica é:

1. valor `Copy`;
2. owner único com drop determinístico;
3. borrow `ref` ou `inout`;
4. região lexical quando muitos valores possuem o mesmo lifetime;
5. `shared T` quando existem múltiplos owners reais;
6. owner de service para estado serializado por instância;
7. ponteiro manual somente em `unsafe` ou FFI.

Nenhum assignment escolhe silenciosamente entre move, reference counting e
arena. O tipo e a assinatura determinam a operação.

### 9.2 Owner único, move e borrow

`object`, buffer e resource são move-first. Last-use inference move um valor
move-only quando nenhum caminho posterior o usa. `take` continua obrigatório
quando a assinatura exige transferência:

```w
fn inspect(value: ref Recipe)
fn replace(value: inout Recipe)
fn enqueue(value: take Recipe)
```

As regras são:

- `ref T` permite leitura compartilhada;
- `inout T` cria acesso exclusivo durante a call;
- um borrow nunca estende o lifetime do owner;
- um move invalida o binding em todos os caminhos que o executam;
- joins de controle exigem o mesmo estado de inicialização;
- partial move de field não existe na DB2;
- destructuring move o aggregate inteiro e inicializa novos bindings;
- reatribuição avalia o novo valor antes de destruir o valor anterior.

O frontend calcula lifetimes por uso e controle de fluxo. O source não contém
annotations de lifetime. Quando a prova falha, o diagnostic mostra o owner, o
borrow, o uso conflitante e a menor correção conhecida.

Um borrow pode permanecer vivo após `await` somente quando o compiler prova:

1. que o owner permanece válido;
2. que owner e borrow ocupam storage estável antes da suspensão;
3. que o owner não é movido ou substituído;
4. que não existe acesso mutável conflitante;
5. que cancelamento também encerra o borrow.

O compiler pode colocar os valores no mesmo task frame. Se não puder provar
essas condições, ele exige ownership, copy ou uma API de pinning. Um raw pointer
não contorna essa regra.

### 9.3 Pinning e valores sensíveis ao endereço

A maioria dos valores W pode mudar de endereço. Pinning só existe quando uma API
depende de endereço estável, como uma callback C persistente, uma estrutura
self-referential ou um task frame que contém borrows internos.

Task frames gerados pelo compiler ficam estáveis enquanto uma suspensão exigir
isso. Essa escolha não aparece no source e não exige annotation.

**Líder DB2:** a API pública inicial usa um tipo de biblioteca, não keyword:

```w
let state = try Pinned.make(take callbackState)

unsafe { register_callback(state.asOpaqueCPtr()) }
```

`Pinned.make` aloca storage estável e pode falhar. `Pinned<T>` pode mudar de
endereço; o `T` apontado por ele não pode. O raw pointer só é válido enquanto o
owner `Pinned<T>` permanece vivo. O handle é move-only; pinning não cria um
segundo owner.

**Pesquisa:** os nomes `PinnedRef<T>`, `PinnedMut<T>` e `withMut` para borrows
scoped ainda precisam de corpus. O contrato já é fixo:

- pinning não é ownership compartilhado;
- pinning não prova alias, validade ou thread safety;
- drop ocorre antes de o storage estável ser reutilizado;
- projection para um field pinned precisa de prova do compiler ou `unsafe`;
- um tipo comum não paga por pinning.

O modelo segue a separação usada pela
[API `Pin` do Rust](https://doc.rust-lang.org/std/pin/): estabilidade de endereço
é um contrato de API para valores sensíveis ao endereço, não uma propriedade de
todo ponteiro.

### 9.4 `shared`, `weak` e ciclos

`shared T` cria múltiplos owners. A implementação portátil usa reference
counting. `weak T?` não mantém o valor vivo. `upgrade()` retorna `shared T?`.

```w
object MenuSection {
  title: String
  parent: weak MenuSection?
  children: Array<shared MenuSection>
}
```

W não possui cycle collector por default. Um ciclo forte precisa de uma destas
soluções:

- uma aresta `weak`;
- remoção ou `close` explícito;
- uma região que destrói o grafo;
- um owner de lifecycle, como service ou request scope.

O compiler pode remover retains e releases quando prova owner único. Um handle
`shared T` que cruza `spawn` usa contagem thread-safe e exige que `T` seja
`shareable`. A implementação pode usar contagem local somente quando prova que o
handle não cruza uma fronteira paralela.

Overflow de contador nunca faz wrap. Ele encerra a isolation boundary antes de
perder um owner. O último release executa `deinit` uma vez. `weak` expira antes
de o storage ser reutilizado.

`ServiceRef<T>` não é `shared T`. O host controla o lifecycle da instance. Um
handle de service mantém identity e capability, não ownership direto do estado.

`w explain ownership` mostra retains, releases, possíveis ciclos e a razão de
uma contagem atômica. Isso é evidence de tooling, não prova global de ausência
de ciclo.

### 9.5 Regiões

Região agrupa lifetimes. Budget limita recursos. Eles são conceitos diferentes.
A primeira implementação oferece uma API de arena. Uma forma de bloco continua
**Pesquisa**:

```w
region request(limit: 64<MiB>) {
  let document = try parse(payload, in: request)
  respond(document)
}
```

Um borrow não escapa da região. Um move para fora só ocorre quando a operação
transfere storage para outro owner. Um objeto com `deinit` entra numa lista de
drop da região; plain data pode usar bulk release.

Todo filho async que usa a região termina antes do bloco. Cancelamento faz join
dos filhos, executa drops em ordem inversa e só então libera o storage. Uma
região não concede um budget de CPU, file descriptor ou network.

### 9.6 Allocator e origem

O profile portátil começa com o allocator do sistema. O host pode selecionar
mimalloc ou outro allocator compatível. A seleção participa da recipe, do
artifact fingerprint e do profile de performance. Ela não altera ownership.

[mimalloc](https://github.com/microsoft/mimalloc) permanece uma opção forte para
benchmark. Sua portabilidade, heaps separados e modos de segurança justificam o
teste. Nenhum benchmark autoriza torná-lo universal sem matriz de target,
sanitizer, override, unload e cross-thread free.

Cada allocation possui uma origem lógica:

```text
origin = allocator identity + instance + deallocator contract
```

A origem pode ficar no owner, num control block ou numa side table. Ela não
precisa ocupar bits do pointer. Move preserva a origem. FFI preserva o
deallocator estrangeiro. W nunca chama `free` num pointer de origem
desconhecida.

Alocações que precisam de recovery usam API fallible ou uma região com budget.
OOM geral encerra a isolation boundary conforme a seção de panic.

### 9.7 Provenance, pointer e address

Um pointer não é somente um número. Ele carrega um endereço e a autorização para
acessar uma allocation durante um intervalo. Essa autorização inclui alcance,
lifetime e mutabilidade.

As regras de W são:

- `ref`, `inout`, `Slice` e views seguras preservam provenance;
- `c.ptr<T>` só permite dereference, arithmetic ou cast em `unsafe`;
- offset válido permanece na mesma allocation;
- comparar endereços não prova que dois pointers possuem a mesma provenance;
- converter pointer em address não transfere autoridade;
- converter um integer arbitrário em pointer não recria provenance;
- null de C entra em W como `c.ptr<T>?` ou wrapper tipado.

**Pesquisa:** `Address` será um valor inteiro próprio para logging, hashing e
comparação. `address(of:)` não retorna um pointer dereferenceable. Uma API
separada e `unsafe` poderá expor provenance somente para adapters que realmente
recebem essa autoridade do host.

Essa direção acompanha a distinção entre endereço e provenance descrita pela
[documentação de pointers do Rust](https://doc.rust-lang.org/stable/std/ptr/) e
pela [LLVM LangRef](https://llvm.org/docs/LangRef.html#ptrtoaddr-to-instruction).
O lowering usa operações que preservam a distinção. Ele não faz round-trip
pointer-integer por conveniência.

### 9.8 Layout, addressability e ABI

Layout W comum é opaco entre builds. O compiler pode reorder fields privados,
usar niches, eliminar aggregates ou especializar storage não escapante.

Layout observável exige uma fronteira:

- `foreign c` para ABI C;
- schema explícito para wire ou persistência;
- profile e fingerprint para ABI W binária;
- tipo de storage dedicado para capacity ou alignment observável.

Obter address, criar `ref`/`inout`, exportar por ABI ou persistir bytes cria uma
barreira de representação. O compiler não compacta o storage atrás de um proxy
com write-back oculto.

`packed` e `aligned` são modifiers de layout seguros e restritos. Eles não são
annotations genéricas. Unaligned access nunca produz uma referência W normal.

### 9.9 Seleção de representação

A HIR mantém tipo lógico, ownership, provenance e layout boundary. Um passe
tardio recebe:

- data layout e ABI do target;
- address width e alignment provados;
- visibility e profile dos módulos;
- allocator e runtime capabilities;
- sanitizer, debugger e hardening ativos;
- range, niche e escape proofs.

O passe escolhe uma destas classes:

| Classe | Uso | Promessa |
|---|---|---|
| portátil | tag e payload explícitos | funciona em todo target suportado |
| niche | null e bit patterns inválidos | não muda valores válidos |
| low-bit | alignment interno provado | somente storage não exposto |
| high-bit | tagged address ou NaN boxing | pesquisa target-specific |

`Option<ref T>` não aloca. O tamanho exato depende do profile. A
[null pointer optimization do Rust](https://doc.rust-lang.org/core/option/#representation)
e os [extra inhabitants do ABI Swift](https://github.com/swiftlang/swift/blob/main/docs/ABI/TypeLayout.rst)
demonstram a utilidade de niches. W não copia suas garantias de ABI sem declarar
uma fronteira equivalente.

Tags de endereço não provam owner, lifetime, thread safety ou validade. Elas
podem guardar metadata imutável ou ajudar instrumentação. Reference count,
generation mutável e deallocator permanecem fora dos bits de endereço.

As seguintes garantias não dependem do profile:

- `f64` preserva todos os bits e valores;
- integers preservam o range declarado;
- FFI recebe representação C canônica;
- capability pointers usam sua representação nativa;
- fallback expandido produz o mesmo resultado;
- nenhum source pede `compact` ou tagged address.

### 9.10 Negociação, hardening e instrumentação

Cada object W registra o profile de representação usado nas interfaces
compiladas. O linker só compartilha ABI W quando fingerprints compatíveis
conferem. Caso contrário, recompila do source, usa adapter canônico ou rejeita o
link.

Hardening e diagnóstico têm precedência sobre compactação opcional. ASan,
HWASan, TSan, MTE, pointer authentication, debugger e profiler podem desativar
tags de W sem alterar semântica.

Essa precedência é necessária porque:

- o [Tagged Address ABI do Linux](https://docs.kernel.org/arch/arm64/tagged-address-abi.html)
  possui enablement por thread e exceções de syscall;
- o [MTE do Linux](https://docs.kernel.org/arch/arm64/memory-tagging-extension.html)
  usa tags e granules próprios;
- [pointer authentication no LLVM](https://llvm.org/docs/PointerAuth.html) usa
  bits não utilizados e invariantes de IR;
- [CHERI](https://ctsrd-cheri.github.io/cheri-c-programming/background/cheri-capabilities.html)
  usa capabilities mais largas e uma tag de validade fora dos bytes normais.

Uma otimização de high-bit não pode ser inferida somente pelo nome da CPU. O
processo, OS, allocator, ABI e toolchain precisam confirmar a capacidade.

Testes diferenciais executam o mesmo corpus em profile portátil e compacto.
Sanitizers executam o fallback. Um resultado diferente bloqueia a otimização.

### 9.11 Destruição e recuperação de storage

- locals são destruídos na ordem inversa da inicialização;
- fields owned morrem em ordem inversa da inicialização completa;
- branches mantêm drop state explícito na HIR;
- `deinit` é síncrono e não usa `throws`;
- `defer` cobre todas as saídas estruturadas;
- cancelamento executa cleanup;
- panic não continua numa boundary parcialmente destruída;
- foreign callbacks registram owner, context e destroy function;
- o valor shared morre após o último owner; o control block morre após o último
  weak handle;
- pinned storage executa drop antes de perder estabilidade de endereço.

O compiler pode eliminar drop flags depois de provar definite initialization.
Ele não remove um cleanup observável.

### 9.12 Explicação e medição

`w explain memory` separa fatos, estimates e medições:

- owner, move, borrow e drop são fatos semânticos;
- stack, heap, region e tag são escolhas do artifact;
- tamanho importado e peak runtime são estimates;
- allocator calls, resident bytes e retain count são medições.

Uma lens de import pode estimar código, static data e peak memory. Ela não
publica um único número como previsão de runtime universal.

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
uma API nomeada. Behavior não concede mobilidade ou atomicidade.

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

Uma boundary tipada pode acrescentar outro error effect fechado. `ServiceRef`
acrescenta `ServiceFailure`, por exemplo. `try` converte cada effect por uma rota
única. A função caller continua declarando um único error set nominal.

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

Um remote lease não pode depender de `deinit`. Destruction é síncrona. A forma
líder registra o cleanup logo após a aquisição:

```w
let lease = try await ovens.acquire(plan)
defer async {
  do {
    try await lease.close()
  } catch error {
    Trace.current.recordCleanupError(error)
  }
}
```

**Pesquisa:** um protocol padrão pode criar uma obrigação linear de close. Move
transfere a obrigação. Um `defer async` reconhecido a descarrega. Sair do scope
sem close produz diagnostic. O compilador não cria uma task detached no
destructor.

## 12. Concorrência, paralelismo e execução

### 12.1 Vocabulário e eixos independentes

| Termo | Significado em W |
|---|---|
| task | unidade lógica com lifetime, resultado e estado de cancelamento |
| child | task owned por um scope ou runtime owner explícito |
| concorrência | progresso intercalado; não exige execução simultânea |
| paralelismo | trabalhos podem executar ao mesmo tempo em recursos distintos |
| suspension point | ponto onde a task pode ceder o executor |
| executor | runtime que agenda jobs em threads, loops, queues ou devices |
| preference | placement herdável; não protege estado |
| isolation | exclusão lógica que protege estado |
| affinity | exigência física do host, como uma UI thread |
| join | consumo estruturado do resultado e do cleanup de um child |

W separa quatro eixos:

1. a árvore de lifetime define ownership, join e cancelamento;
2. `async` e `spawn` definem intenção de execução;
3. `on` define preference de placement;
4. service, entry e profile definem isolation ou affinity.

Um executor não cria isolation por existir. Um executor serial pode atender mais
de uma isolation boundary. Uma boundary pode migrar entre threads.

O [JEP 525](https://openjdk.org/jeps/525) usa uma árvore de tasks para preservar
lifetime e observabilidade. W aplica a estrutura no type checker. Ela não é
somente uma convenção de biblioteca.

### 12.2 Quatro formas de executar

```w
let digest = hash(data)
let menu = try await fetchMenu()
async let stock = pantry.reserve(order)
spawn on .compute let plan = optimize(take snapshot)
```

| Forma | Início | Intenção | Resultado |
|---|---|---|---|
| call síncrona | agora | execução atual | valor ou error |
| `await` direto | agora | suspender o caller | valor ou error |
| `async let` | na declaração | child concorrente | `Task<T, E>` |
| `spawn let` | na declaração | child com intenção paralela | `Task<T, E>` |

Uma chamada async precisa de `await`, `async let` ou `spawn let`. W não cria uma
Promise/Future silenciosa. `async let` não promete outro core. `spawn let`
autoriza e solicita capacidade paralela.

O runtime pode executar um `spawn` inline para limitar oversubscription. Ele deve
preservar os suspension points e as regras de alias. O programa não pode usar
simultaneidade como resultado sem synchronization explícita.

### 12.3 `Task` e ownership

`Task<T, E>` é lexical, linear e one-shot. O estado conceitual é:

```text
created → scheduled → running → success(T) | error(E) | canceled
```

As regras são:

1. cada child pertence ao scope criador;
2. o scope não termina antes do cleanup de todos os children;
3. `await` consome o handle e move um resultado owned;
4. `cancel` solicita cancelamento, mas não consome o handle;
5. retorno antecipado cancela e faz join dos children restantes;
6. esquecer ou destruir o handle não destaca a task;
7. o compiler diagnostica um handle sem consumo.

Um segundo `await` é inválido. `SharedTask<T, E>` é explícito e guarda o outcome
para vários observers. O scope produtor continua sendo o único owner de
cancelamento. Um observer não recebe autoridade para cancelar por possuir acesso
ao resultado.

W não possui task “detached” sem owner. Trabalho que ultrapassa o scope lexical
precisa de um owner runtime explícito. Entries, service instances e supervisors
podem exercer esse papel. O owner deve definir shutdown, deadline e trace.

### 12.4 Join, erro e outcome

`try await task` consome o handle:

- success move o valor para o caller;
- error propaga `E`;
- canceled propaga o exit de cancelamento.

Uma API explícita expõe todos os outcomes sem transformar cancelamento em `E`:

```w
let outcome: TaskOutcome<Menu, MenuError> = await task.outcome()
```

`TaskOutcome<T, E>` possui `.success(T)`, `.error(E)` e `.canceled(Cancellation)`.
Panic não é um outcome recuperável. Ele encerra a isolation boundary conforme a
seção 11.

Um join de tuple usa ordem lexical:

```w
let (stock, plan) = try await (stock, plan)
```

O runtime observa `stock` antes de selecionar o outcome de `plan`. Uma falha
posterior não cancela um child lexicalmente anterior que ainda pode definir o
error primário. Depois da seleção, o scope cancela os children restantes e
aguarda o cleanup.

Erros de siblings e cleanup ficam anexos ao error primário. Eles aparecem em
trace e diagnostics. APIs `collect` retornam todos os outcomes. APIs `race`
declaram que completion order faz parte do resultado.

### 12.5 Cancelamento

```w
cancel report
cancel batch, reason: .shutdown
```

Cancelamento é uma solicitação idempotente. Ele não usa `pthread_cancel` e não
faz unwind assíncrono de foreign frames.

Uma task observa o sinal:

- antes e depois de um suspension point;
- em I/O que aceita cancelamento;
- em uma boundary de task group;
- em `Task.checkCancellation()` para loops longos.

O cancelamento do parent propaga para descendants. Cancelar um child não cancela
siblings, salvo policy explícita do group. Um deadline cria o mesmo sinal com
metadata de causa.

O motivo serve a policy e observabilidade. O programa não deve usar a ordem de
dois motivos concorrentes como dado de domínio. O trace mantém todos os sinais e
sua causalidade.

Cancelamento não é rollback. Uma operação externa deve declarar um commit point
ou um outcome desconhecido. Antes do commit, o adapter pode garantir ausência de
efeito. Depois do commit, cleanup não desfaz o efeito sem compensação explícita.

**Líder DB2:** cancellation safety é uma propriedade da operação e de seu estado.
Não existe um marker público `CancelSafe`. O verifier usa ownership, cleanup,
commit points e metadata do adapter. Uma API que não informa o contrato recebe a
policy conservadora.

### 12.6 Isolation, preference, paralelismo e affinity

| Contrato | Owner | Pode ser remapeado? | Protege estado? |
|---|---|---:|---:|
| required isolation | service/entry | não pode ser removido | sim |
| executor preference | task subtree | sim | não |
| parallel intent | `spawn`/parallel group | limitado pelo host | não |
| host affinity | profile/adapter | somente por target compatível | pode compor |

```w
async on .network let catalog = fetchCatalog()
spawn on .compute let plan = optimize(take snapshot)
```

`on` seleciona uma preference estática do profile. Ele não promete thread,
affinity ou isolation. O profile define domínios, capacity e fallback.

A resolução usa esta ordem:

1. required isolation e host affinity precisam ser compatíveis;
2. a preference explícita substitui a herdada;
3. a preference herdada substitui o default do profile;
4. o callee isolado sempre executa em sua isolation boundary.

Uma call por `ServiceRef` não muda o placement do callee. Aplicar `on` ao child
caller só muda o trabalho não isolado do child. `async let` e task groups herdam
a preference. Um future owner runtime precisa declará-la de novo.

`spawn` em um domínio estritamente serial é error. Trabalho UI deve chamar o
owner isolado:

```w
await renderer.show(plan)
```

`spawn on .ui` confundiria affinity serial com paralelismo.

A [SE-0417](https://www.swift.org/swift-evolution/#SE-0417) também separa
executor preference de actor isolation. W mantém essa separação na HIR.

Seleção dinâmica usa API:

```w
let task = Task.spawn(on: executor, operation: work)
```

`spawn<domain: .compute>` e `spawn<.compute>` ficam como **Alternativa**. `on`
continua **Líder DB2** porque nomeia a relação de placement.

### 12.7 Mobilidade e captures

W prova duas propriedades em uma fronteira concorrente:

| Propriedade | Pergunta |
|---|---|
| `transferable` | o owner ou acesso exclusivo pode mudar de domínio? |
| `shareable` | referências ao mesmo valor podem ser usadas por domínios paralelos? |

As propriedades são independentes. Um buffer mutável com owner único pode ser
`transferable` sem ser `shareable`. Um recurso com cleanup preso ao domínio de
origem pode expor uma view `shareable` sem transferir o owner. Um tipo comum pode
provar ambas.

| Capture | Prova mínima |
|---|---|
| `take value` | `transferable(value)` |
| value copiado | cópia independente e `transferable` |
| `ref value` | `shareable(value)` e lifetime dentro do scope |
| `inout value` | acesso exclusivo transferido; parent fica bloqueado |
| `ServiceRef<P>` | handle `shareable`; state não cruza |

Um child que continua na mesma isolation boundary pode acessar state isolado. Um
child que sai da boundary precisa de snapshot, copy ou move. `spawn` nunca
captura state mutável de uma service instance.

Structs, enums, tuples e closures derivam mobilidade de fields ou captures. Raw
pointers, thread-local state e destructors affine são locais por default.
`Pinned<T>`, `shared T` e pointer C não ganham mobilidade pelo nome.

O [Rust Reference](https://doc.rust-lang.org/reference/special-types-and-traits.html)
separa `Send` de `Sync`. A
[SE-0302](https://www.swift.org/swift-evolution/#SE-0302) usa `Sendable` para
transfer e referências sincronizadas. W mantém duas provas, mas não usa os nomes
de Rust como API.

**Líder DB2:** `transferable` e `shareable` são predicates intrínsecos. Código
comum não declara annotations. Uma prova manual sempre é `unsafe`.

O compiler infere o predicate exigido pelo body generic e o grava na interface
do módulo. Documentation gerada mostra o contrato. Adicionar um predicate
inferido a uma API publicada é uma mudança de compatibilidade. O author pode
fixar o contrato no source com `where (transferable(T))` ou
`where (shareable(T))`. Essas formas são predicates, não traits para conformar.

**Alternativa:** `<mobility: .transferable>` mantém o contrato explícito, mas cria
outro uso de `<>`. Marker protocols públicos ficam rejeitados por enquanto.

### 12.8 Task groups e backpressure

Estrutura estática usa `async let` ou `spawn let`. Coleções dinâmicas usam
`TaskGroup`. O primeiro SDK oferece:

```w
let pages = try await TaskGroup.concurrentMap(
  take requests,
  limit: 16,
  ordering: .input,
  using: fetchPage,
)

let mixtures = try await TaskGroup.parallelMap(
  take jobs,
  limit: 8,
  ordering: .input,
  on: .compute,
  using: mixJob,
)
```

`concurrentMap` usa children concorrentes. `parallelMap` adiciona intenção
paralela e as provas de mobilidade. As duas APIs cancelam trabalho restante no
primeiro error selecionado pela ordem declarada.

As variantes `concurrentCollect` e `parallelCollect` retornam
`Array<TaskOutcome<T, E>>`. Elas não cancelam por error da aplicação.

Defaults:

- `limit` controla children ativos;
- o buffer de admissão também usa `limit`;
- producer suspende quando o buffer está cheio;
- `.input` preserva a ordem do input;
- `.completion` declara resultado dependente do scheduler;
- o profile pode reduzir `limit`, mas não criar uma fila ilimitada;
- cancellation fecha producer, children e resultados não consumidos.

Uma builder API futura precisa declarar memória por item, deadline, fairness e
policy de overload. O runtime nunca cria uma thread por item por default.

### 12.9 Streams e channels

**Direção:** `Stream<T, E>` usa pull. `next()` é async e move um elemento para o
consumer. Cancelar ou destruir o consumer fecha o producer scope.

Prefetch é explícito e bounded. Ordering, watermark e ownership fazem parte do
constructor. `yield` não entra na grammar antes de o verifier representar esses
contratos.

`Channel<T>` é um tipo separado. `send` move `T`. `receive` devolve ownership.
Capacity zero cria rendezvous. Capacity positiva é bounded. Ordering é FIFO por
sender, salvo policy mais forte.

### 12.10 Memory model, atomics e locks

Safe W não permite data races. Um programa sem data race observa uma ordem
sequencialmente consistente, salvo atomics com order mais fraca. Race conditions
de domínio ainda podem existir.

Uma data race dentro de `unsafe` viola o contrato de safety. Sanitizer profiles
devem detectá-la quando o target permite. O optimizer não precisa preservar um
resultado para source que viola esse contrato.

`var atomic value` baixa para `Atomic<T>`:

- read, write e read-modify-write comuns usam sequential consistency;
- methods nomeados escolhem acquire, release ou relaxed;
- `ref value` empresta `Atomic<T>`, nunca `ref T`;
- `inout` do payload é inválido;
- `atomic` não compõe com outro behavior sem regra específica.

T1 fornece `Mutex<T>`, `RwLock<T>`, condition, once e barrier. Services,
message passing e immutable snapshots continuam preferidos para state maior.

### 12.11 FFI, blocking calls e callbacks

Uma foreign function possui metadata verificada:

- thread-safe ou serializada;
- reentrant ou non-reentrant;
- blocking ou non-blocking;
- callback executor ou thread;
- suporte a cancelamento;
- ownership de buffers;
- global state e signal safety.

Sem metadata, o importer usa a policy conservadora. Uma call blocking exige um
adapter ou uma isolation boundary dedicada. Enviar a call para um blocking pool
não torna o código cancel-safe.

Um callback estrangeiro cria um job em executor conhecido. Ele não retoma uma
task arbitrária em qualquer thread. Raw pointers capturados continuam locais até
um wrapper `unsafe` provar mobilidade e lifetime.

### 12.12 HIR, lowering e runtime mínimo

A HIR preserva:

- task scope, parent, kind, start e join;
- outcome, error edges, cancellation e cleanup;
- captures, borrows e mobilidade;
- isolation, preference, parallel intent e affinity;
- deadline, budget e causal trace.

Somente depois dos verifiers o lowering usa o
[dialeto Async do MLIR](https://mlir.llvm.org/docs/Dialects/AsyncDialect/) ou
[LLVM coroutines](https://llvm.org/docs/Coroutines.html). Essas ferramentas
modelam tokens, groups e frames. Elas não definem a semântica W de lifetime,
cancelamento ou error primário.

O runtime mínimo possui:

- task control block e árvore parent/child;
- executor concorrente single-thread;
- pool paralelo bounded;
- wakeups, timers e uma integração I/O por plataforma de teste;
- cancellation state, cleanup e memory reclamation;
- task IDs, logical stack e trace.

O primeiro runtime não precisa de work stealing sofisticado, filas todas
lock-free, remote tasks, QoS completa ou GPU.

Testes usam scheduler, clock, entropy e I/O injetáveis. O scheduler registra e
reproduz decisões. O corpus explora joins, cancel points, overload, drain e
falha. Instrumentação não pode mudar ordering.

## 13. Módulos de execução, services e entries

### 13.1 Service e closed turn

```w
export service DiningRoom as DiningRoomApi {
  var tables: Map<TableId, TableState>

  mut async fn reserve(request: Reservation): Receipt throws ReservationError {
    // ...
  }
}
```

**Líder DB2:** cada instance usa um turn serial e fechado. Um handler externo
executa do início ao fim. `await` não admite outro handler da mesma instance.
Outras instances podem progredir.

Completions retomam o handler pelo strand lógico. O strand pode migrar entre
threads. Serial não significa affinity.

Dentro do handler:

- chamadas internas síncronas usam call normal;
- `async let` cria children do handler;
- `spawn` usa somente snapshots ou valores transferidos;
- state mutável da instance não cruza `spawn`;
- cleanup termina antes do próximo turn.

Uma self-call por `ServiceRef` é error. O compiler detecta o caso estático. O
runtime detecta o caso dinâmico antes de enqueue.

Closed turn mantém invariantes locais, mas cria head-of-line blocking. Uma
operação `cancel()` enfileirada não interrompe o turn ativo. APIs que precisam de
controle simultâneo devem usar request cancellation, turns curtos ou um
supervisor explícito.

Instances keyed fornecem paralelismo natural entre keys. Calls para a mesma key
continuam seriais. Uma instance `.process` com handler longo é um caso
adversarial de head-of-line blocking, não o modelo recomendado para todo
serviço. Se status ou controle precisam progredir durante uma operação longa, o
handler divide o trabalho em turns curtos ou entrega a operação a um owner
runtime supervisionado. Essa divisão não torna o state da instance reentrant.

O modelo difere dos actors reentrant da
[SE-0306](https://www.swift.org/swift-evolution/#SE-0306). Esses actors admitem
interleaving em `await`. W prefere previsibilidade no default.

**Pesquisa:** input gates e reentrância explícita podem aumentar throughput.
Eles precisam invalidar borrows e revalidar invariantes. Os
[input gates de Durable Objects](https://blog.cloudflare.com/durable-objects-easy-fast-correct-choose-three/)
são evidência útil, mas não definem a semântica W.

Uma falha lógica da instance não cria sandbox de memória. Código não confiável
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

`entry { ... }` é um handler curto. `entry Name { ... }` é um descriptor de
bindings. Bindings não usam vírgula.

Slots são símbolos tipados e versionados do profile. O build escolhe um
descriptor por product. Importar o módulo não registra nem executa o entry.

`Context` é uma capability tipada. Ele não é um mapa universal de environment.

### 13.3 Unidade lógica e packing físico

Uma service instance é uma unidade lógica endereçável. Ela pode ter identity,
lifecycle, state, mailbox, quotas, capabilities e trace próprios. Ela não exige
processo, thread, library ou conexão próprios.

O runtime pode:

- co-localizar instances;
- agrupar mailboxes;
- inlinear uma call local;
- usar fast path sem serialização;
- distribuir instances entre executors ou processos;
- mover uma instance quando o adapter permite.

A regra *as-if* preserva ordering, errors, cancellation, deadline, capability,
identity e observabilidade. O toolchain mede o custo da granularidade física.

### 13.4 Descriptor, identity e lifecycle

Um descriptor registra:

- implementação e protocol exportado;
- scope de identity: process, key, request ou deployment;
- required isolation;
- executor preference;
- parallel intent permitido;
- host affinity;
- mailbox, capabilities e resource budgets;
- durable adapter, restart policy e observabilidade.

Um módulo estático não possui esses campos. Importar não cria instance, thread,
queue ou authority.

Identity é:

```text
InstanceId = (service type, scope, logical key)
Generation = (InstanceId, generation number)
```

O instance manager mantém:

```text
declared → starting → ready → draining → stopped
               │         │         │
               └─────────┴─────────┴→ failed → starting(new generation)
```

`starting` não aceita calls. `ready` aceita até os limites. `draining` rejeita
novas calls e conclui ou cancela roots até o deadline. `failed` invalida state,
borrows, pointers e task frames da geração. Restart sempre cria outra geração.

Uma `ServiceRef<P>` mantém identity e authority. Ela pode resolver a geração
ativa conforme a policy. Ela nunca expõe um pointer para state da instance.

### 13.5 Mailbox, admission e ordering

Uma mailbox é limitada por três quotas:

1. itens;
2. bytes reservados;
3. trabalho em voo.

O descriptor fixa máximos. Deployment pode reduzir os valores. Ele não pode
expandir um limite que afeta uma garantia do programa.

Admission segue:

```text
resolve → validate schema/capability/deadline → reserve quota → enqueue
        → execute root → release quota → outcome
```

Validação de frame, profundidade e deadline ocorre antes de allocation grande.
A call normal aguarda capacity com cancellation. Uma API `tryCall` retorna
overload sem esperar. Drain rejeita antes de reservar quota.

Ordering default é FIFO por `(sender, instance)` na admissão. Não existe ordem
global entre senders. Priority não pode causar starvation silencioso.

### 13.6 Structured calls e falhas

Uma call transporta:

```text
callId, parentCallId, serviceId, generationHint, operationId, schemaVersion,
deadline, cancellationId, callerCapability, payload
```

`ServiceRef<P>` sempre exige `await`, inclusive no mesmo processo. O callee
mantém required isolation. O trace informa hop ou fast path.

Uma interface de service não aceita `ref` ou `inout` para state do caller.
Payloads usam value, `take` ou capability handles. Results também precisam ser
`transferable`. O fast path não enfraquece essa regra.

Um method `throws E` chamado por `ServiceRef` possui dois error effects:

1. `E`, para error da aplicação;
2. `ServiceFailure`, para a boundary.

`ServiceFailure` representa somente a boundary:

| Case | Significado | Retry seguro por default? |
|---|---|---|
| `overload` | admission recusada antes do efeito | sim, com backoff e deadline |
| `draining` | generation não aceita calls novas | não; resolver a instance de novo |
| `unavailable` | destino não aceitou a call | somente por policy explícita |
| `unauthorized` | capability não autoriza a operação | não |
| `incompatibleSchema` | caller e callee não negociaram schema | não |
| `callCycle` | ancestry formaria ciclo closed-turn | não |
| `unknownOutcome(effectId)` | entrega ou efeito ocorreu, mas não foi confirmado | só com idempotência |

Deadline e cancellation produzem `TaskOutcome.canceled`. Eles não são
`ServiceFailure`. O `try` pode injetar cada error effect em um case único do
error set do caller:

```w
enum RestaurantError: Error {
  dining(DiningRoomError)
  service(ServiceFailure)
}
```

O runtime propaga `parentCallId`. Se uma call estruturada retorna a uma instance
closed-turn que já está em sua ancestry, o runtime retorna `callCycle`. Ele não
espera um deadlock conhecido. Ciclos que atravessam sistemas sem metadata ainda
exigem deadline.

Retry mutante nunca é implícito. Idempotência ou deduplication podem autorizar
uma policy. Queda depois da entrega pode retornar `unknownOutcome`. W não promete
exactly-once sem protocol e storage adequados.

Uma API explícita `CallOutcome<T, E>` permite inspecionar `success(T)`,
`application(E)`, `canceled(Cancellation)` e `boundary(ServiceFailure)`. O
`try await` comum propaga os effects.

O fast path pode mover valores quando ABI e trust domain conferem. Ele ainda
preserva await, mobility, quotas, ordering, cancellation, errors e tracing.

Uma capability remota é um handle tipado. Ela não é uma URL livre. Importar a
interface não cria o handle.

**Pesquisa:** dependent calls podem usar um `CallPipeline` explícito para reduzir
round trips. O pipeline preserva capability lifetime, quotas, cancellation,
failure e `unknownOutcome`. Ele não muda `ServiceRef` para uma Promise lazy. O
[promise pipelining de Cap'n Web](https://blog.cloudflare.com/capnweb-javascript-rpc-library/)
é evidência útil para o protótipo, não uma decisão de syntax.

### 13.7 Estado durável e gates

Durability é um adapter explícito. Um handler declara transação, commit point e
a relação entre state e outputs. A baseline não presume state durável.

SQLite é o primeiro adapter oficial provável. Ele oferece transações, operação
local e portabilidade. Ele não é a semântica universal. Memory, files, remote KV
e engines especializadas podem implementar o contrato.

O baseline confirma o commit antes de liberar uma response. Um outbox
transacional é a alternativa para mensagens.

**Pesquisa:** um output gate pode reter outputs até confirmar writes. Se a write
falhar, o runtime descarta os outputs. O protótipo precisa provar causalidade,
limites, backpressure e cancelamento. Os
[output gates de Durable Objects](https://blog.cloudflare.com/durable-objects-easy-fast-correct-choose-three/)
são uma referência, não uma decisão automática.

### 13.8 Capabilities e sandbox

O contrato portátil usa capabilities tipadas para filesystem, network, clock,
random, process, environment, storage e devices. A enforcement boundary depende
do target:

- type system e HIR dentro do programa;
- processo/OS sandbox para código nativo não confiável;
- Wasm/component boundary quando compatível;
- seccomp, namespaces, sandbox-exec ou job objects como defense-in-depth.

Seccomp não protege “cada módulo importado”. Um módulo não é uma boundary
física. Um filtro de syscall não controla memory safety dentro do processo.

### 13.9 Wasm e playground

Wasm é um target e uma boundary possível. Ele não transforma W em substituto de
JavaScript. O playground compila um subset para Wasm e usa imports/exports
tipados do host. DOM, network e storage só existem quando o profile concede.

### 13.10 Observabilidade e teste

Cada task, call e instance registra:

- trace/span, parent e causalidade;
- queue, execution e suspension time;
- allocation e budget facts;
- hop local/remoto;
- cancellation e errors primário/adicionais;
- instance ID, generation e call outcome;
- logical stack e source mapping;
- build, module e package identity.

Logs humanos são projeções. Eventos estruturados possuem schema. Payloads,
secrets e capability tokens são redigidos.

O scheduler de teste injeta clock, entropy, storage e decisões de execução. Ele
reproduz ordering, overload, drain, panic e restart. Packing físico diferente
precisa passar o mesmo oracle observável.

## 14. Prelude e SDK

### 14.1 T0 — core independente do ambiente

T0 contém:

- tipos primitivos, Option, Result e Error;
- String, Array, Map, Set, Range e views;
- Slice, `Pinned<T>`, AllocationError e allocator hooks;
- protocols de igualdade, hash e iteração;
- intrinsics de ownership e dos predicates `transferable`/`shareable`;
- operações puras de texto, collection e matemática básica;
- intrinsics necessários para memória segura e compile time.

T0 pode usar o runtime/allocator do target. Ele não depende de console,
filesystem, rede, clock, locale ou OS API.

### 14.2 T1 — systems e adapters comuns

T1 contém:

- console e `print`;
- process, environment, filesystem e paths;
- clock, calendar, timezone e random;
- Task, TaskOutcome, TaskGroup, Stream e Channel;
- synchronization, executors e blocking adapters;
- ServiceRef, ServiceFailure e service host APIs;
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

Tipos de collection permanecem explícitos:

```w
Array<Ingredient>
Map<OrderId, Order>
Set<Capability>
[u8; 4096]
```

`Array<T>` é owned e possui tamanho dinâmico. `[T; count]` possui tamanho
estático. `Slice<T>` e `MutableSlice<T>` são views. `[T]` como tipo dinâmico
continua alternativa, mas perde como líder porque se parece com literal e shape.

Uma collection vazia usa `Array()`, `Map()` ou `Set()`. W não usa `[:]`.

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

Indexação multi-rank usa uma lista de índices:

```w
let score = forecast[table, course]
let row = forecast[table]
```

Fornecer todos os índices produz um elemento. Fornecer um prefixo produz uma
view da rank restante. `forecast[table][course]` é válido, mas materializa a
operação intermediária no source e pode formar uma view temporária.

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

`unsafe fn` move essa obrigação para o caller. Uma prova manual de
`transferable` ou `shareable` também exige `unsafe`.

O importer v0 aceita funções, enums, structs simples, opaque types, pointers,
arrays e callbacks com context. Varargs, bitfields e unions exigem wrapper ou
override explícito. Cada allocation mantém o deallocator de origem.

O importer não declara uma interface segura só porque conseguiu ler o header.
Uma wrapper W restabelece os contratos ausentes:

| Forma C | Forma segura W quando provada |
|---|---|
| nullable pointer | `T?`, `ref T?` ou owner opcional |
| pointer + length | `Slice<T>` ou `Array<T>` |
| out pointer | return value ou `inout` scoped |
| status code | `throws Error` |
| callback + context | closure e owner de registration |
| allocation + destroy | owner que preserva o deallocator |

Uma call síncrona pode criar um pointer scoped a partir de `inout`. A assinatura
C não pode armazená-lo. Uma callback que persiste usa storage pinned e um
destroy callback. O adapter registra se a função captura somente address,
provenance de leitura ou provenance de escrita.

`c.ptr<T>` preserva a provenance recebida da fronteira. `address(of:)`, quando
existir, não substitui o pointer original. Um `c.ptr<T>` criado de integer sem
authority do host não pode ser dereferenced em W seguro nem receber uma
provenance inventada pelo lowering.

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

`fn<lang: .c>` é o candidato nomeado do contrato estático. Ele explica o papel do
argumento e permite um futuro slot de ABI sem criar uma annotation aberta.
`fn<C>` continua líder até o corpus comparar leitura, edição e diagnostics.

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
- pointer provenance, address capture, pin e allocation origin;
- errors, cancellation, panic e cleanup scopes;
- task parent/child, mobilidade e execution preference;
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

**Direção:** o compiler principal fica self-hosted antes de tasks, services,
tensors e package registry. O bootstrap possui um perfil source próprio,
`bootstrap.w0`. Ele é W normal com uma lista menor de features.

O seed portátil usa C11, CMake e Ninja. Ele aceita `bootstrap.w0` e emite C11
portátil. Esse emitter existe somente para bootstrap, auditoria e recovery. O
backend normal continua W/MLIR.

MLIR fica atrás de um adapter C estreito e versionado. C++ e TableGen podem
implementar dialects e passes. Tipos C++/MLIR não entram na HIR W. A
[C API do MLIR](https://mlir.llvm.org/docs/CAPI/) é low-level e não possui
garantia de estabilidade; por isso o bundle fixa sua revisão e testa o adapter.

#### 19.5.1 Fechamento mínimo de `bootstrap.w0`

O subset precisa expressar um lexer, parser, type checker, HIR, diagnostics,
serializer e driver. Ele também precisa chamar o backend pelo adapter C.

| Família | Capacidade mínima |
|---|---|
| source | UTF-8, comentários, módulos, imports e visibility |
| bindings | `const`, `let`, `var`, assignment e definite initialization |
| controle | `if`, `guard`, `switch`, loops, break, continue e return |
| funções | funções livres, methods, `static fn`, labels, recursion e calls indiretas |
| tipos | scalars, tuples, structs, objects, enums, Option, typed Error e newtypes |
| protocols | requisitos para dispatch estático; sem existential |
| números | widths fixas, `usize`, checked arithmetic, bit operations e endian explícito |
| dados | String, views, Slice, ByteBuffer, Array, Map, Set e Range de T0 |
| generics | type parameters, constraints simples e monomorphization |
| memória | owner único, whole-value move, `ref`, `inout`, drop, `defer` e Arena API |
| falha | `throws E`, `try`, `do`/`catch`, panic e allocation fallible |
| C | opaque type, scalar, struct, pointer, function e callback + context |
| entry | forma curta para `process.main` |
| host | argv, filesystem, path, environment declarado e process adapter |
| build | modules herméticos, interface serializada e output determinístico |

O fechamento é uma propriedade do conjunto, não uma lista de syntax isolada.
Cada tipo e função usados por `compiler/core-w0` precisam estar no profile ou em
T0. Cada dependência de T0 usada pelo core também precisa ser expressável em W0.
O teste de fechamento compila o core sem carregar `compiler/extended`.

Closures com capture podem entrar quando reduzirem o compiler sem ampliar muito
o seed. O primeiro source W0 pode usar loops e funções nomeadas.

O profile impõe três regras de determinismo:

1. Iteração de `Map` ou `Set` não define ordem de output.
2. Output ordenado usa `Array`, sort explícito ou uma collection ordenada.
3. Clock, random, locale e environment não entram sem input declarado.

Estas features não pertencem ao fechamento mínimo:

- `async`, `spawn`, service e entries que não são `process.main`;
- `shared`, `weak`, region pública e tagged address;
- property behavior;
- units, tensors, GPU e SIMD explícito;
- existential `any`/`some`, refinements, const generics e contracts de valor;
- `fn<Language>` inline;
- package registry, portal, LSP e debugger;
- reflection, macro e annotation.

O compiler escrito em W0 pode reconhecer, verificar e baixar essas features. Ele
não precisa usá-las no próprio source. Isso mantém o seed pequeno sem criar uma
segunda linguagem.

#### 19.5.2 Camadas do compiler

O source fica dividido por dependência:

```text
compiler/core-w0
  source + syntax + AST + HIR + types + diagnostics + driver

compiler/backend-adapter
  ABI C fixada para MLIR/LLVM

compiler/extended
  passes, tooling e adapters que podem usar W além de W0
```

`compiler/core-w0` permanece compilável pelo seed. `compiler/extended` pode
usar a versão estável anterior de W. Um módulo extended não pode ser necessário
para reconstruir o core.

O SDK de bootstrap contém somente T0 e os adapters T1 listados na tabela. Ele
não executa scripts de package. Seus arquivos, flags e environment são inputs
da recipe.

#### 19.5.3 Estágios

```text
C compiler
  → w-seed-c
  → stage A: w-bootstrap, compilado de W0 para C
  → stage B: w, compilado por w-bootstrap via MLIR
  → stage C: w, recompilado pelo stage B
  → stage D: repetição hermética ou rota diversa
```

O stage C precisa compilar o mesmo source que produziu o stage B. O projeto
compara:

- AST, HIR e interfaces normalizadas;
- objects e payload quando todos os inputs estão capturados;
- diagnostics e testes do corpus;
- diferenças de target metadata em sidecars separados.

Igualdade entre stages detecta drift, mas não elimina por si só o problema de
trusting trust. A rota de release de alta confiança usa
[diverse double-compiling](https://dwheeler.com/trusting-trust/) com builds do
seed por toolchains C diversos e recipes publicadas.

O modelo de stage segue uma prática conhecida em compilers self-hosted. O
[Rust compiler](https://rustc-dev-guide.rust-lang.org/building/bootstrapping/what-bootstrapping-does.html)
separa stage 0 e recompila o compiler. O
[Go toolchain](https://go.dev/doc/install/source) usa uma versão Go anterior e
preserva o compiler Go 1.4 escrito em C como rota longa de bootstrap.

#### 19.5.4 Evolução e recovery

Uma release W `N` compila o core de `N + 1` dentro de uma janela publicada. Uma
mudança que quebra essa janela exige uma ponte source ou um novo seed versionado.
O seed antigo não precisa aceitar toda edição futura.

O seed é validado com Clang, GCC e MSVC quando o target permitir. Pelo menos uma
rota usa warnings máximos e sanitizers disponíveis. Dependable C informa a
matriz; ele não é um dialect e não autoriza undefined behavior.

O comando futuro `w bootstrap explain` lista stages, compiler parents, source
digests, adapters, environment e pontos de convergência. Um artifact sem essa
recipe pode funcionar, mas não recebe o estado “bootstrap reproduzido”.

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
| provenance separada de address | **Possível agora** | HIR e LLVM preservam a distinção |
| pinning interno de task frame | **Provável** | lowering conhecido; drop e projection exigem corpus |
| `Pinned<T>` público | **Provável** | contrato claro; FFI persistente precisa de protótipo |
| `shared` + `weak` sem cycle collector | **Provável** | RC é conhecido; tooling de ciclos precisa de avaliação |
| `async let`/`spawn let` estruturados | **Possível agora** | state machine e runtime mínimo delimitados |
| modules sem lifecycle e imports herméticos | **Possível agora** | contrato estático simples |
| UTF-8 owned e views | **Possível agora** | representação portátil com fallback |
| strict numerics e overflow verificado | **Possível agora** | backend oferece operações adequadas |
| schema fechado de contrato estático | **Possível agora** | AST/HIR simples; grafia pública ainda exige corpus |
| services serial-turn e `ServiceRef` async | **Provável** | exige protótipo de mailbox, deadlock e trace |
| `<unit>` e units customizadas | **Provável** | type/lowering coerentes; ergonomia precisa de corpus |
| refinements e value parameters | **Provável** | exige evaluator, proof budget e ABI identity |
| property behaviors | **Provável** | expansão HIR é viável; composição ainda precisa de teste |
| obrigação linear de async close | **Pesquisa** | evita leak oculto; receiver e cancellation precisam de protótipo |
| entries e host profiles | **Provável** | binding é claro; adapters precisam de schemas |
| tensors ranked, `@` e views | **Provável T2** | MLIR ajuda; API e device model precisam de protótipo |
| tagged pointers e high-bit addresses | **Pesquisa** | target-specific e sem vantagem sem benchmark |
| `bootstrap.w0` e self-host antes de tasks | **Provável** | subset fechado; seed C e adapter MLIR precisam de prova |
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
  → compiler de cardápio restrito a bootstrap.w0
  → Restaurant service
  → async calls com payloads owned
  → parallelMap bounded da brigada
  → controle PID com ranges e units
  → tensor de previsão
  → sensor C + fn<C>
  → grafo shared/weak + callback pinned
  → pricing + billing idempotente
  → DiningRoom service
  → mailbox, ServiceFailure e cycle oracle
  → HTTP/TUI response
  → cleanup, trace e provenance
```

Uma injeção de falha em cada seta não pode deixar task, lease, buffer, mailbox
item, shared owner, callback ou pagamento sem estado observável. O compiler de
cardápio precisa continuar dentro do fechamento W0.

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
- `spawn on .compute` contra `spawn<domain: .compute>` e `spawn<.compute>`;
- refinement postfix contra `T<where: (...)>`;
- `fn<C>` contra `fn<lang: .c>`;
- slot angular nomeado contra case enum posicional em erro e evolução de schema;
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
- seed C aceita o primeiro `bootstrap.w0` e emite C11;
- corpus diferencial entre o caminho seed-C e W/MLIR.

Saída: payload determinístico para programas síncronos nos dois caminhos.

### 26.5 Fase 3 — memória, errors e C

- initialization e whole-value move;
- borrows, drop e defer;
- typed errors e panic boundary;
- allocator hooks;
- `foreign c`, unsafe e wrappers.

Saída: sanitizers e corpus negativo não encontram dangling/double drop.

### 26.6 Fase 4 — bootstrap e self-host

- fechar o profile `bootstrap.w0`;
- escrever `compiler/core-w0` em W;
- fixar o adapter C para MLIR;
- gerar stages A, B e C;
- comparar HIR, interfaces, payloads e diagnostics;
- publicar a recipe de recovery pelo seed.

Saída: o core W compila o próprio source sem tasks, services ou packages.

#### 26.6.1 Gates internos do self-host

O self-host não começa no primeiro build completo. Cada gate adiciona somente a
capacidade necessária para o gate seguinte.

| Gate | Capacidade mínima de W | Prova |
|---|---|---|
| SH0 | bytes, UTF-8, source locations, lexer e diagnostics | tokeniza o próprio source |
| SH1 | parser, recovery, AST, modules, imports e names | cria a própria AST de forma estável |
| SH2 | scalars, aggregates, enums, generics e type checking | verifica os módulos do core |
| SH3 | initialization, move, borrow, drop, errors e collections | constrói HIR sem GC |
| SH4 | HIR tipada, verifier, serialization e deterministic order | round-trip preserva a HIR |
| SH5 | C ABI, filesystem, argv, path, environment e backend adapter | gera um compiler executável |
| SH6 | recipes herméticas, stages A/B/C e comparação normalizada | recompila o mesmo source |
| SH7 | seed C por toolchains diversos e recipe de recovery | reproduz a rota auditável |

Todos os gates usam allocation fallible. Nenhum gate depende de clock, random,
locale, ordem de `Map` ou variável de environment não declarada. SH5 é o menor
ponto que permite dizer “W compila W”. SH6 prova convergência. SH7 reduz a
confiança necessária no seed.

Em SH5, o parser self-hosted reconhece toda a grammar congelada da DB2. Ele pode
emitir um diagnostic de profile para semantics que ainda não possuem lowering.
As fases seguintes adicionam esses verificadores e lowerings ao compiler
self-hosted. O source de `compiler/core-w0` continua restrito a W0. Assim, tasks,
services, units, tensors e packages não ampliam a base de recovery.

### 26.7 Fase 5 — tasks

- async state machine;
- `async let`, `spawn let` e domains;
- linear Task, `TaskOutcome` e cancellation;
- `concurrentMap`/`parallelMap` bounded;
- blocking adapter e callback scheduling;
- HIR verificada antes do lowering async;
- executor cooperativo e pool paralelo bounded;
- deterministic test executor.

Saída: restaurante executa I/O concorrente e lotes paralelos com ordering,
backpressure e cleanup reproduzíveis.

### 26.8 Fase 6 — services e host entries

- `entry` e host profiles;
- service instance manager;
- closed turn, generation e drain;
- mailbox com três quotas;
- `ServiceFailure`, cycle detection e `ServiceRef`;
- tracing e local fast path;
- process/Wasm boundary experimental.

Saída: CLI e HTTP exibem hops, queues, overload, cycle e restart.

### 26.9 Fase 7 — packages e SDK

- package parser, resolver, lock e CAS;
- builds `--locked`/offline;
- T0/T1 mínimos;
- provenance, SBOM e reprodução local;
- lens por import.

Saída: uma máquina limpa reconstrói o mesmo payload sem rede durante o build.

### 26.10 Fase 8 — ciência e extração

- units/refinements completos;
- tensor CPU e `@`;
- rebuild em estágios;
- suíte de conformidade;
- decisão sobre mover W para repository próprio.

Saída: DB2 demonstrada de ponta a ponta e pronta para revisão pública.

### 26.11 Gates

| Gate | Pergunta | Evidência mínima |
|---|---|---|
| memória | `shared`, arena e allocator compõem sem surpresa? | benchmarks, cycles, FFI e cancellation |
| tasks | lowering preserva join, cancelamento e mobilidade? | testes diferenciais e scheduler reproduzível |
| services | closed turn, admission e cycle são previsíveis? | três workloads, failure injection e trace |
| units | `<>` supera `[]` em uso real? | estudo humano e modelo |
| ML | shape/operator reduzem erros sem esconder cost? | corpus CPU/SIMD/device |
| packages | resolver e evidence model são operáveis? | projeto real offline/reproduzido |
| self-host | SH0–SH7 fecham e convergem? | mini compiler, builds diversos e diff de outputs |

### 26.12 Checkpoint por fase

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
| pointer tagging | mecanismo de memória candidato | otimização de representação com fallback |
| bootstrap | seed C e self-host cedo | profile W0 fechado antes de tasks |

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
| D2-014 | refinement | `T where (predicate)` | `T<where: (predicate)>`; `T(where:)` |
| D2-015 | value generics | `const` parameters e labels | positional only; contrato universal aberto |
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
| D2-039 | execution domain | `async/spawn on .domain` | `<domain: .name>`; `<.name>`; descriptor-only |
| D2-040 | Task | linear, lexical, one-await | Future clonável; detached default |
| D2-041 | grupos | lexical e bounded | queue ilimitada; thread pool exposto |
| D2-042 | cancelamento | statement cooperativo | method only; async thread cancellation |
| D2-043 | erro concorrente | primário lexical + anexos | primeiro a concluir; aggregate always |
| D2-044 | atomics | seq-cst default, orders explícitas | C-like default; lock implicit |
| D2-045 | mobilidade | `transferable`/`shareable` derivados | `Send`/`Sync` públicos; runtime checks |
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
| D2-072 | inline language | `fn<C>` primeiro | `fn<lang: .c>`; library import; multi-language v0 |
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
| D2-096 | portal | gerar após design freeze; protótipo congelado | páginas manuais; escolher Astro agora |
| D2-097 | aplicação `<...>` | schema estático fechado por head | slots universais; cases sem labels; mapa aberto |
| D2-098 | campos | imutável sem prefixo; `var` para mutation | `let` obrigatório; `let` opcional |
| D2-099 | collection dinâmica | `Array<T>`, `Map<K, V>` e `Set<T>` | `[T]`; braces para map/set |
| D2-100 | tensor indexing | `tensor[i, j]`; prefixo retorna view | nesting obrigatório; método `at` |
| D2-101 | recurso async | `defer async`; obrigação linear em pesquisa | async destructor; `using await`; lint |
| D2-102 | receiver | `fn` borrow, `mut fn` exclusivo, `static fn` sem receiver | `self`; inferir static; função livre |
| D2-103 | camadas de memória | semântica separada de lowering, representação e host | tag ou allocator como semântica |
| D2-104 | borrow suspenso | permitido somente com owner, frame e alias provados | proibir sempre; lifetime annotation |
| D2-105 | pinning | interno sem annotation; `Pinned<T>` para storage estável | keyword universal; raw pointer |
| D2-106 | ciclos shared | `weak`, close, região ou lifecycle owner; sem collector default | cycle collector universal |
| D2-107 | pointer provenance | address separado; round-trip não restaura authority | pointer como integer |
| D2-108 | origem de allocation | owner/control block/side table preserva deallocator | bits do pointer obrigatórios |
| D2-109 | compactação | portátil → niche → low-bit; high-bit em pesquisa | tagged address obrigatório |
| D2-110 | hardening | sanitizer, PAC, MTE e capability têm precedência | compactação vence o profile |
| D2-111 | subset self-host | profile `bootstrap.w0` fechado | compiler exige a linguagem inteira |
| D2-112 | seed output | W0 para C11, backend normal W/MLIR | MLIR completo no seed; C como backend público |
| D2-113 | momento do self-host | depois de memória/FFI e antes de tasks | somente após DB2 completa |
| D2-114 | cláusula estática | `where`/`on` no source, record comum na HIR | toda propriedade dentro de `<...>` |
| D2-115 | slots angulares | schema declara posição, labels e slot primário | inferir slot pelo nome do enum case |
| D2-116 | evolução self-host | gates SH0–SH7; W0 fechado e core separado | marco único; compiler usa toda a DB2 |
| D2-117 | eixos de execução | lifetime, intent, preference, isolation e affinity separados | thread group único |
| D2-118 | início de child | `async let`/`spawn let` iniciam na declaração | lazy no primeiro await |
| D2-119 | task longa | owner runtime explícito; sem detached sem owner | drop destaca; task global |
| D2-120 | outcome de task | success/error/canceled; panic encerra boundary | cancel em `E`; panic como Result |
| D2-121 | seleção de error | ordem lexical declarada | primeira completion sempre vence |
| D2-122 | cancelamento | cooperativo, idempotente e sem rollback implícito | matar thread; transação implícita |
| D2-123 | resolução de domain | isolation/affinity vencem preference | `on` substitui isolation |
| D2-124 | grupos dinâmicos | concurrent/parallel map bounded e ordering explícito | queue ilimitada; intent oculto |
| D2-125 | stream/channel | pull e capacity bounded; `yield` adiado | generator unbounded |
| D2-126 | memory model | safe W data-race-free; DRF-SC salvo atomics explícitos | race definida em safe code |
| D2-127 | FFI concorrente | metadata conservadora e callback em executor conhecido | assumir non-blocking |
| D2-128 | async lowering | invariantes W antes de MLIR Async/LLVM coroutine | backend define semantics |
| D2-129 | lifecycle de instance | identity + generation; restart invalida state anterior | reuse de pointers/frames |
| D2-130 | admission | quotas de itens, bytes e in-flight | unbounded; limite só por item |
| D2-131 | falha de call | `E` e `ServiceFailure` são effects separados | transporte dentro de todo `E` |
| D2-132 | call cycle | ancestry causal rejeita ciclo closed-turn conhecido | esperar somente deadline |
| D2-133 | output durável | commit confirmado ou outbox; output gate em pesquisa | gate inferido na v0 |
| D2-134 | scheduler de teste | clock/I/O/schedule injetáveis e replay | teste somente por timing real |
| D2-135 | payload de service | value/`take`/capability; sem `ref`/`inout` do caller | borrow no fast path local |
| D2-136 | paralelismo de service | instances keyed; mesma key serial | singleton longo; reentrância implícita |
| D2-137 | RPC encadeado | `CallPipeline` explícito em pesquisa | toda `ServiceRef` vira Promise lazy |

Uma revisão pode responder por ID. Uma mudança deve atualizar o exemplo, a
grammar, o formatter, o corpus e a seção semântica correspondente.
