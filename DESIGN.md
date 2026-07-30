# Design integral da linguagem W

> **Status:** **Candidato experimental** · 30 de julho de 2026

Este é o documento canônico de design do W. Ele reúne linguagem, runtime, SDK,
compilador, packages, distribuição, tooling, plano e alternativas. Ele descreve
a forma integrada vigente. O documento corrige contradições das tentativas
anteriores e mantém alternativas rastreáveis para cada decisão.

A implementação pode usar estas formas antes da ratificação humana. Cada forma
mantém alternativas relevantes. O objetivo é permitir revisão visual com
parser, realce e o produto de referência.

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
| **Forma vigente** | forma integrada no design e no produto de referência |
| **Alternativa** | solução legítima que continua no corpus de comparação |
| **Pesquisa** | hipótese com baseline funcional que não depende dela |
| **Rejeitado por enquanto** | não entra no design vigente sem nova evidência |

### Mapa do sistema

| Bloco | Seções | Resultado que o bloco define |
|---|---:|---|
| produto e método | 0–4 | promessa, evidência, invariantes, símbolos e superfície integrada |
| linguagem | 5–8 | source, módulos, funções, closures, tipos e conversões |
| segurança semântica | 9–11 | memória, layout, behaviors, errors, panic, OOM e cleanup |
| execução | 12–13 | tasks, paralelismo, domains, services, entries, estado e sandbox |
| SDK e domínios | 14–19 | tiers, numéricos, texto, tensors, performance, C e ilhas de linguagem |
| implementação e produto | 20–24 | frontend, runtime, packages, tooling, pesquisas e viabilidade |
| validação e sequência | 25–29 | produto de referência, revisão, roadmap, histórico e decisões |

Leia o bloco que contém a dúvida e depois use o ID W correspondente. Não é
necessário reconstruir uma decisão a partir do histórico.

O histórico da DB1 e as notas anteriores ficam em
[`Y/W/`](../Y/W/). O Git preserva autoria, datas e diffs. Este arquivo é a única
fonte de verdade para o estado atual.

### 0.1 Promessa

**Exemplo:** `spawn<.compute>` mostra paralelismo no source e continua legível
sem conhecer o executor.

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

**Exemplo:** `task.cancel(reason: .shutdown)` nomeia target, ação e motivo sem
criar um statement especial.

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

**Exemplo:** o target Wasm não concede DOM nem transforma W em substituto de
JavaScript.

O design vigente não tenta:

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

**Exemplo:** uma pessoa identifica por que `async let` sobrepõe espera e
`spawn let` permite paralelismo após ler um único exemplo.

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

Cada contrato normativo deve incluir ou apontar para um exemplo verificável. O
exemplo pode ser source válido, diagnostic esperado ou cenário canônico. Uma
afirmação sobre runtime deve mostrar também o estado após erro ou cancelamento.

**Exemplo:** a decisão de callable mostra `fn`, `some fn` e `any fn` na seção
7.5. O ensaio correspondente fica em `reference/last-light`.

## 1. Limite da alegação

**Exemplo:** a comparação entre `<unit>` e `[unit]` precisa medir correção antes
de medir preferência.

Não existe um estudo que prove a melhor sintaxe para W. Familiaridade também não
prova facilidade de uso. O design vigente usa três classes de evidência:

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
Por isso, o design vigente mede acerto antes de medir preferência.

## 2. Invariantes

**Exemplo:** um build release não pode aceitar overflow que o mesmo programa
rejeita em debug.

Estas regras limitam todas as escolhas do design vigente:

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

## 3. Contratos estáticos e orçamento de símbolos

Cada delimitador mantém uma função mental principal:

| Forma | Função principal no design vigente | Exemplos |
|---|---|---|
| `{...}` | corpo, scope ou record delimitado por contexto | função, `entry`, `<{...}>` |
| `(...)` | chamada, parâmetros, agrupamento ou expressão estática | `cook(order)`, `<(value > 0)>` |
| `[...]` | coleção, indexação, shape ou lista estática | `[1, 2]`, `map[key]`, `<[.a, .b]>` |
| `<...>` | contrato estático fechado, conhecido pelo head | `Array<u8>`, `spawn<.compute>`, `fn<C>` |
| `:` | introduz um contrato associado | tipo, return, label, field ou case body |
| `=` | define ou atualiza um valor ou binding | binding, assignment, alias ou slot |
| `.` | qualificação, member ou case abreviado | `std.http`, `value.count`, `.none` |
| `=>` | introduz um corpo de expressão | closure ou accessor curto |
| `if` | introduz condição ou guard runtime | `case ..<0 if error > 0` |
| `@` | faz produto linear de rank 1 ou 2 | `features @ weights` |

`<...>` é o envelope único para informação estática local. O elemento à
esquerda é o head. O head publica um schema fechado com slots, tipos, defaults e
cardinalidade. Esse schema é um contrato estático. Ele não precisa ser um
`protocol`. Um slot pode exigir conformance a um `protocol`.

`where` e `on` não são keywords do design vigente. `where` permanece em comparação com
refinements angulares. `on` fica **Rejeitado por enquanto**. Ele cria uma frase
especial para uma informação que já pertence ao contrato de `spawn`.

### 3.1 Formas do payload estático

O delimitador interno informa a categoria do payload. Ele não define sozinho a
semântica:

| Forma | Payload | Regra |
|---|---|---|
| `<T>` | tipo, valor const ou case primário | o schema identifica o kind |
| `<name: value>` | argumento nomeado | o nome pertence à interface source |
| `<(expression)>` | uma expressão compile-time | o head decide como usar o resultado |
| `<{name: value}>` | um record compile-time | fields não criam storage no head |
| `<[a, b]>` | uma lista compile-time ordenada | a lista não vira set implicitamente |

Exemplos da forma vigente:

```w
Array<u8>
Tensor<f32, shape: [8, 4]>

type Port = u16<(1...65_535)>
type FixedCode = String<(.graphemes.count == 10)>
type SmallBuffer = Array<u8><(.count <= 64)>

struct Bounds {
  min: usize
  max: usize
}

type BoundedString<const _ bounds: Bounds> =
  String<(.scalars.count in bounds.min...bounds.max)>
type Label = BoundedString<{min: 1, max: 40}>

spawn<.compute> let plan = optimize(take snapshot)
unsafe fn<C> checksum(data: c.ptr<c.uchar>): c.uint { ... }
```

`StaticList<T>` é um tipo compile-time de T0. Ele é ordenado, imutável e
apagado depois da especialização. Um head pode publicar esse tipo como slot
primário:

```w
enum KitchenStage {
  reserve
  prepare
  bake
  plate
}

struct StagePlan<const _ stages: StaticList<KitchenStage>> {
  orderId: OrderId
}

fn standardPlan(
  orderId: OrderId,
): StagePlan<[.reserve, .prepare, .bake, .plate]> {
  return StagePlan(orderId: orderId)
}
```

Nesse exemplo, `<[...]>` fornece um único argumento `StaticList<KitchenStage>`.
A ordem faz parte da especialização. Duplicação e quantidade só mudam quando o
schema do head declara outra regra. A forma não cria índices nomeados runtime.

O slot implícito `cases` de um enum declara outra regra. Ele recebe a lista e
normaliza um conjunto pela ordem dos cases. A seção 8.6.1 define esse contrato.

Em um tipo, um payload Boolean é um refinement predicate. Um member iniciado
por `.` usa o valor refinado como subject implícito. Um range no slot primário
inclui esse subject e o operador `in`. Portanto, estas formas possuem a mesma
HIR:

```w
u16<(1...65_535)>
u16<(value in 1...65_535)>

String<(.graphemes.count == 10)>
String<(value.graphemes.count == 10)>
```

A forma curta é **Forma vigente**. `value` continua disponível para
desambiguação e diagnostics.

Um tipo generic já aplicado recebe o refinement em outro envelope.
`Array<u8><(.count <= 64)>` não mistura o element type com o predicate.

`Array<[u8, (.count <= 64)]>` não é uma grafia equivalente. A forma passa uma
única static list ao slot primário de `Array`. Esse slot exige um tipo de
elemento. A lista também não identifica se o predicate restringe o elemento ou
o `Array` resultante. `StagePlan` mostra um head cujo slot aceita a lista.

O parser chama toda forma `.name` de referência contextual. O type checker
resolve a referência com estas regras:

1. em um refinement, `.member` acessa o subject implícito;
2. quando o contexto espera um enum fechado, `.case` seleciona esse enum;
3. uma colisão exige `value.member` ou `EnumName.case`;
4. sem subject ou tipo esperado, `.name` produz diagnostic.

A HIR sempre guarda o subject ou o enum completo. `.name` não abre lookup
global e não abrevia um associated member sem contexto.

`Animal<Dog>` aplica `Dog` ao schema de `Animal`. A forma não cria inheritance.
Se o parâmetro exige um protocol, o type checker verifica a conformance
declarada pelo head.

`<{...}>` é um static record. Ele não significa extension, inheritance, union ou
storage adicional. `<[...]>` é uma static list ordenada. Ele não representa uma
lista de constraints sem um schema que declare essa ordem.

### 3.2 Schema fechado e HIR

Um head aceita somente os slots que publicou. Cada slot aparece no máximo uma
vez. Um argumento omitido precisa ter default ou inferência inequívoca.

Uma representação possível no compiler self-hosted é:

```w
enum StaticArgument {
  typeValue(TypeId)
  constValue(ConstValue)
  enumCase(EnumCaseId)
  adapter(LanguageAdapterId)
  unit(UnitExpression)
  expression(ConstExpressionId)
  record(StaticRecordId)
  list(StaticListId)
}

struct StaticSlot {
  name: Symbol
  kind: StaticKind
  defaultValue: ConstValue?
  primary: Bool
}

struct StaticContract {
  head: StaticHead
  arguments: Array<StaticArgument>
}
```

`StandardDomain` e os enums de domain registrados pelo product profile são
fechados. `C` e `Rust` resolvem para uma `LanguageAdapterId` fixada no lock. A
HIR guarda identity, version e digest do adapter. Ela não guarda uma string
livre.

O frontend normaliza a superfície para records tipados:

```text
u16<(1...65_535)>
  → TypeContract(base: u16, refinement: value in 1...65_535)

spawn<.compute> let plan = optimize(order)
  → TaskContract(kind: .parallel, domain: .compute)

unsafe fn<Rust> checksum(...)
  → ForeignFunctionContract(language: .rust, abi: .c, safety: .unsafe)
```

Os contratos estáticos seguem estas regras:

1. O head declara um schema fechado.
2. O evaluator aceita somente valores compile-time herméticos.
3. O formatter usa a ordem declarada pelo schema.
4. Argumentos posicionais precedem argumentos nomeados.
5. Um case sem label preenche somente o slot primário.
6. A adição de um slot não reinterpreta source anterior.
7. `w explain` mostra defaults, inferências e a HIR normalizada.
8. Nenhum slot concede authority, memory safety ou capability.

Nomes de argumentos melhoram a leitura. Eles também fazem parte da
compatibilidade source. O experimento de
[named type arguments do Scala 3](https://docs.scala-lang.org/scala3/reference/experimental/named-typeargs-spec.html)
mostra esse custo.

`spawn` publica `domain` como slot primário. Por isso, `spawn<.compute>` é a
forma vigente. `spawn<domain: .compute>` permanece **Alternativa** para corpus e
diagnostics.

### 3.3 Refinement, composição e layout

As operações abaixo permanecem distintas:

| Intenção | Forma vigente | Muda storage? |
|---|---|---:|
| restringir valores | `T<(predicate)>` | não |
| criar identidade nominal | `type X = T` | não por default |
| criar sinônimo | `alias X = T` | não |
| adicionar methods ou conformance | `extension X { ... }` | não |
| adicionar fields | `struct X { value: T ... }` | sim |
| representar um de vários cases | `enum X { a(A) b(B) }` | conforme layout do enum |
| sobrepor storage C | `foreign c union` ou wrapper `unsafe` | sim e explícito |

Uma extension nunca adiciona storage. Herança de implementação não entra na
design vigente. Um safe sum usa `enum`. Uma C union sobrepõe bytes e pertence à fronteira
de layout. Essas operações não usam `<{...}>`.

**Pesquisa:** `A | B` pode representar um anonymous sum. `A & B` pode
representar protocol composition ou structural intersection. Nenhuma forma
recebe layout ou subtyping implícito antes de um protótipo.

O [TypeScript Handbook](https://www.typescriptlang.org/docs/handbook/unions-and-intersections.html)
usa `A | B` para escolha e `A & B` para composição estrutural. Uma C union
sobrepõe storage. W não mistura os dois modelos.

**Pesquisa:** `{field: value}` pode virar um anonymous record literal runtime.
A forma precisa distinguir value struct de `object` com identidade. Ela também
precisa recuperar errors perto de blocks. O static record de `<{...}>` não
depende dessa decisão.

Um head futuro pode publicar um static record como configuração. Nesse caso,
`T<{...}>` aplica o schema de `T`. Ele não cria uma extensão universal.

### 3.4 Funções e listas de contratos

`fn<C>` usa o slot primário `language`. `fn<lang: .c>` permanece
**Alternativa**. Um futuro slot `abi` pode compor com o primeiro sem mudar seu
significado.

`fn<(...)>`, `fn<{...}>` e `fn<[...]>` não ganham significado por simetria. O
compiler rejeita a forma quando o schema de `fn` não publica o slot
correspondente.

Generic parameters continuam declarados. Um protocol constraint usa o contrato
associado por `:`:

```w
fn encode<T: Serializable>(value: ref T): Bytes
```

**Pesquisa:** `T<[P, Q]>` pode listar constraints. A forma é curta, mas `[]`
sugere ordem. Protocol constraints normalmente não possuem ordem. Um composite
protocol nomeado continua a baseline.

### 3.5 Parsing, formatter e gate

**Exemplo:** `Array<u8><(.count <= 64)>` deve manter a mesma CST após dois ciclos
de formatação.

Parênteses fecham a expressão antes do `>` externo. Essa regra reduz conflitos
com `<`, `>`, `<=`, `>=` e generic nesting.

O parser constrói o payload antes de consultar o schema. O type checker resolve
slot, kind, default e referência contextual. O formatter usa o subject curto
quando ele é inequívoco. Ele preserva a qualificação necessária para resolver
uma colisão.

O corpus precisa verificar:

1. recovery depois de delimitadores incompletos;
2. nesting de generic e refinement;
3. comparação dentro de `<(...)>`;
4. static records e lists;
5. diagnostics que nomeiam head e slot;
6. leitura humana e por modelos sem consulta à HIR.

**Forma vigente:** usar `T<(...)>`, `async/spawn<.domain>`, `fn<Language>` e unit
literal sem label.

**Alternativa:** preservar `where` e receiver implícito no corpus comparativo.
Slots primários nomeados continuam aceitos para comparação e diagnostics. O
formatter emite a forma curta quando o schema não é ambíguo.

**Rejeitado por enquanto:** `spawn on .domain`. O corpus preserva a forma para
medir leitura e migração. O parser vigente não a aceita.

### 3.6 Avaliação compile-time

W separa avaliação exigida de otimização:

```w
const pageSize = 4 * 1024 // deve ser avaliado durante a compilação
let pageSize = 4 * 1024   // o optimizer pode fazer constant folding
```

Uma falha na primeira linha produz diagnostic. A segunda linha mantém semântica
runtime mesmo quando o optimizer substitui a expressão por um literal.

#### 3.6.1 Contextos e superfície

Estes contextos exigem um valor compile-time:

| Contexto | Exemplo |
|---|---|
| initializer de `const` | `const maximum = 256` |
| argumento `const` generic | `Buffer<count: 4096>` |
| contrato estático | `Tensor<f32, shape: [8, 4]>` |
| tamanho de array fixo | `[u8; digestSize]` |
| quantidade de repeat literal | `[0; digestSize]` |
| definição de unit | `unit KiB = 1024<B>` |
| refinement e shape | `u16<(1...4096)>` |

Uma função usada nesses contextos declara `const fn`:

```w
export const fn opcode(name: ref String): u8 throws MenuCompileError {
  return switch name {
    case "ingredient": 0x01_u8
    case "heat": 0x02_u8
    case "wait": 0x03_u8
    case "serve": 0xff_u8
    case let unknown: throw .unknownInstruction(name: copy unknown, line: 0)
  }
}

const serveOpcode = try opcode("serve")
let selectedOpcode = try opcode(input)
```

`const fn` é um contrato de capacidade. A primeira call executa no evaluator. A
segunda call executa em runtime com a mesma semântica.

Um initializer usado em avaliação compile-time declara `const init`:

```w
struct Cell {
  value: u8

  const init(value: u8) {
    self.value = value
  }
}

const emptyCell = Cell(value: 0)
```

Literals, operators, enum cases e constructors sintetizados são const-safe por
definição. Uma call de usuário exige `const fn` ou `const init`. Um protocol
pode exigir o mesmo modifier:

```w
protocol ConstDecodable {
  static const fn decode(source: ref Bytes): Self throws DecodeError
}
```

Remover `const` de uma declaração exportada é source-breaking. Adicionar
`const` amplia o uso sem mudar calls runtime.

`const` não participa da forma de overload. Duas declarações não podem diferir
somente por esse modifier:

```w
fn decode(source: ref Bytes): Packet
const fn decode(source: ref Bytes): Packet // error: duplicate call shape
```

O modifier promete elegibilidade. Ele não promete termination ou sucesso dentro
de qualquer quota:

```w
const fn recurse(): Never { return recurse() } // valid body; evaluation hits quota
```

A ordem canônica dos modifiers é visibility, `static`, `const`, `unsafe`,
receiver mode, `async` e `fn`. `const` não combina com `unsafe` ou `async`.
Ele pode combinar com `mut fn` e `take fn`:

```w
struct ConstBuilder {
  var storage = Bytes()

  static const fn table(): StaticList<u8> { ... }
  const mut fn append(value: u8) { storage.append(value) }
  const take fn finish(): Bytes { return take storage }
}

const unsafe fn address(): usize { ... } // error: incompatible modifiers
```

Um receiver usado por `const mut fn` ou `const take fn` deve pertencer ao heap
virtual do evaluator. O receiver não pode referenciar storage runtime.

W não precisa de `comptime expression` na baseline. Um binding `const` força a
avaliação e dá um nome ao resultado:

```w
const table = buildTable()
use(table)
```

`comptime buildTable()` e `const { ... }` permanecem **Alternativa**. Elas só
entram se pipelines sem um binding mostrarem ganho mensurável.

Um parâmetro de chamada pode exigir um argumento compile-time com `const`:

```w
fn prepare(
  statement: const database.Query<WorldKey, WorldRow>,
  parameters: WorldKey,
): PreparedQuery

const worldById = database.Query<WorldKey, WorldRow>(...)
let prepared = prepare(worldById, parameters: WorldKey(id: 42))
let invalid = prepare(runtimeQuery, parameters: WorldKey(id: 42))
// error[W-CONST-0007]: statement requires a compile-time value
```

`const` fica no modo do parâmetro, depois de `:`. Ele não é C `const` nem um
borrow read-only. Ele não combina com `ref`, `inout` ou `take`. O argumento
precisa ser `ConstRepresentable` e conhecido pelo evaluator no call site. Uma
expressão const-safe direta também é aceita; o caller não precisa criar um
binding.

O body recebe um valor imutável normal. Ele não precisa ser `const fn` e pode
executar trabalho runtime. Um default também precisa ser compile-time. O
requisito entra na interface, mas não cria uma forma de overload. Duas funções
que diferem somente por `const` possuem a mesma call shape e são duplicadas.

O requisito faz parte do function type:

```w
type QueryPreparer =
  fn(const database.Query<WorldKey, WorldRow>, WorldKey): PreparedQuery
```

O lowering pode usar bytes static, um digest ou um handle interno. Ele não
precisa monomorphizar a função por valor. Uma chamada indireta mantém o mesmo
requisito no function type. O compiler grava o valor normalizado nas
dependências da recipe. Uma protocol boundary passa o descriptor estático e não
o transforma em input livre. Uma wire boundary precisa incluir esse descriptor
na operação versionada antes de aceitar a interface.

Essa forma serve a SQL, bindings, format strings, regex e outros descriptors
que precisam de validação e auditoria no build. Dados deliberadamente dinâmicos
usam outra API. W não adiciona `comptime` no call site.

#### 3.6.2 Programa const-safe

Um `const fn` pode usar:

- `let`, `var`, assignment e storage local ao evaluator;
- `if`, `guard`, `switch`, loops e recursion;
- structs, enums, Option, Result, String, Bytes e collections const-safe;
- typed errors, `try`, `try?`, `do`/`catch` e `defer`;
- outra call const-safe com dispatch estático;
- arithmetic que segue a mesma policy do target.

Este exemplo constrói dados sem gerar source:

```w
const fn buildOpcodes(): Map<String, u8> {
  var result = Map<String, u8>()
  result["ingredient"] = 0x01_u8
  result["heat"] = 0x02_u8
  result["wait"] = 0x03_u8
  result["serve"] = 0xff_u8
  return result
}

const instructionOpcodes = buildOpcodes()
```

As interfaces compiladas de T0 marcam cada operação const-safe. Por exemplo,
`Map.set` expõe a implementação ConstIR ou um intrinsic equivalente. O
evaluator não mantém uma segunda implementação semântica de Map.

Um `const fn` não pode usar:

- I/O, environment, clock, random ou outra capability;
- FFI, `unsafe`, raw pointer, address ou object identity;
- task, service, `async`, `spawn`, channel ou lock;
- `shared`, `weak` ou state global;
- dispatch por existential ou function value;
- API que observa allocation failure do evaluator.

O compiler aponta a primeira call proibida e mostra a cadeia const:

```text
error[W-CONST-0001]: clock.now is not const-safe
  called by buildExpiry at config.w:18
  required by const sessionExpiry at config.w:24
```

Allocation interna do evaluator não é um efeito observável. Se o evaluator
atinge sua quota, a compilação falha. `tryReserve` não consegue converter essa
falha em `AllocationError`.

`throws E` continua válido. Um error tratado mantém a avaliação. Um error não
tratado no initializer produz diagnostic:

```w
const port = try Port.parse("invalid")
// error[W-CONST-0005]: PortError escaped a required const context
```

Panic também produz diagnostic. Ele não cria uma fault boundary dentro do
compiler:

```w
const byte = [1, 2][4]
// error[W-CONST-0006]: bounds panic during const evaluation
```

W não expõe `isComptime`, `__ctfe` ou outra forma de escolher semântica pela
fase. Uma `const fn` recebe os mesmos inputs e produz o mesmo valor em compile
time e runtime:

```w
const fn phase(): Bool {
  return isComptime // error: no such intrinsic
}
```

Calls indiretas e closures const-safe ficam em **Pesquisa**. Elas exigem
`const fn(A): B`, `const mut fn(A): B` e `const take fn(A): B` como function
types. A baseline usa calls estaticamente resolvidas.

Um local `const` não pode depender de um parâmetro runtime. Dentro de uma
`const fn`, `let` e `var` recebem os argumentos da call em qualquer fase:

```w
const fn increment(value: u32): u32 {
  let result = value + 1
  return result
}

fn runtime(input: u32): u32 {
  const invalid = input + 1 // error: runtime value in const initializer
  return invalid
}
```

#### 3.6.3 Valores e materialização

`ConstRepresentable` é um predicate do compiler. Ele não é um protocol que um
tipo pode implementar. O predicate aceita valores estruturais sem authority ou
identidade:

| Aceito | Rejeitado |
|---|---|
| unit, Bool, números e UnicodeScalar | raw pointer e address |
| newtype, tuple, struct e enum | `ref`, `inout` e borrow escapante |
| Option, Result e fixed array | object com identidade |
| String, Bytes, Array, Map e Set | shared, weak, Task e ServiceRef |
| static record e StaticList | closure, existential e capability |

Um tipo com custom `deinit` não é ConstRepresentable na baseline. O comando
`w explain type T` mostra o primeiro requisito que impede a representação.

Map e Set exigem equality e hash const-safe para as keys. O evaluator pode usar
um hash interno determinístico, mas sempre confirma a equality do programa:

```w
const ids: Map<GuestId, u8> = [GuestId(7): 1]
```

`StaticArgumentRepresentable` é um predicate mais restrito. Ele controla valores
que entram na identidade de um tipo ou specialization:

| Aceito | Rejeitado |
|---|---|
| Bool, integer, UnicodeScalar e newtype | float e NaN |
| closed enum com payload aceito | object e existential |
| fixed struct e static record | Array, Map e Set runtime |
| fixed array, String, Bytes e StaticList | pointer, borrow e capability |

Todos os fields devem atender ao predicate. A serialização canônica inclui tipo,
field names, ordem e valor:

```w
BoundedText<{min: 1, max: 120}>
StagePath<[.accepted, .reserving, .preparing, .serving, .completed]>
```

Float fica fora da identidade de tipo porque NaN não possui equality reflexiva.
Uma API que precisa de escala usa integer, rational ou um enum nomeado.

ConstValue usa a semântica do valor. Ele não guarda layout, capacity, pointer,
hash seed ou endereço:

```w
const names: Map<String, u8> = ["heat": 2, "serve": 255]
```

O evaluator serializa esse Map como pares na ordem de inserção. O backend pode
baixar lookup para switch, tabela ordenada ou perfect hash. Essa escolha não
muda equality ou iteration order.

Um `const` não possui owner ou identidade runtime. Quando um contexto exige um
valor owned, cada uso cria uma materialização independente:

```w
const defaults: Array<u8> = [1, 2, 3]
let left: Array<u8> = defaults
let right: Array<u8> = defaults
left[0] = 9
expect right[0] == 1
```

Uma leitura pode usar storage imutável embutido sem allocation observável. Uma
materialização owned segue a policy normal de OOM. `take defaults` é erro porque
o const não possui owner:

```w
fn consume(values: take Array<u8>)

consume(take defaults) // error: const has no owner
consume(defaults)      // materializa um rvalue owned
```

Um borrow de materialização não pode escapar:

```w
fn leak(): ref Array<u8> {
  return defaults // error: const materialization cannot escape by borrow
}
```

#### 3.6.4 Target e inputs declarados

O evaluator usa a semântica do target, não a máquina do compiler. `usize`,
layout intrinsics e endian seguem a recipe:

```w
import w.target as target

const pointerBytes = target.pointerWidth / 8
```

`w.target` expõe somente facts fixados, como arch, OS, ABI, pointer width,
endianness e CPU features declaradas. Esses facts entram na chave do resultado.

Float compile-time usa a mesma policy IEEE strict da seção 15. O evaluator
recusa uma operação que não consiga reproduzir para o target:

```text
error[W-CONST-0007]: target float operation is not reproducible
```

Source W não lê environment, arquivo, commit, path ou clock durante const
evaluation. Uma tool target hermética pode gerar um módulo comum:

```w
// generated/build_info.w
export const sourceCommit = "7f43c2..."
```

O source importa esse módulo como qualquer outro:

```w
import generated.build_info as buildInfo

print(buildInfo.sourceCommit)
```

A recipe registra o generator, inputs, output e digest. Não existe
`env("COMMIT")` ou `#define` oculto no evaluator.

Quando um fact de target afeta um const exportado ou a identidade de um tipo, a
interface torna-se específica desse target. Seu digest registra o target.

Target selection usa adapters ou módulos declarados no build graph. W não
adiciona `#if`, `#include` ou conditional import à linguagem:

```w
import platform.clock
```

O build graph seleciona uma implementação de `platform.clock` para o target. A
interface importada permanece a mesma.

#### 3.6.5 Termination, quotas e cache

Loops e recursion são permitidos. W não promete provar termination geral. O
evaluator usa quatro quotas determinísticas:

- steps de ConstIR;
- bytes do heap virtual;
- call depth;
- bytes do resultado serializado.

O manifest fixa os limites efetivos:

```w
build: {
  constEval: {
    steps: 1_000_000
    heap: 64MiB
    callDepth: 256
    result: 8MiB
  }
}
```

Um dependency não pode aumentar essas quotas pelo source. O limit de wall-clock
é somente proteção do compiler. Ele não participa do resultado semântico.

Uma quota excedida produz um diagnostic com consumo, limite e cadeia de calls:

```text
error[W-CONST-0003]: const evaluation exceeded 1000000 steps
  742113 steps in buildDfa
  257887 steps in minimizeStates
```

Um ciclo no grafo de const falha antes da execução quando o compiler consegue
detectá-lo:

```w
const left = right + 1
const right = left + 1
// error[W-CONST-0002]: left -> right -> left
```

A chave de cache contém:

1. ConstIR e interface digests;
2. argumentos e tipos normalizados;
3. target, profile e edition;
4. tabelas Unicode e semantic bundles usados;
5. versão do evaluator;
6. quotas efetivas;
7. módulos gerados declarados.

Somente success completo entra no cache compartilhável. Error, panic, quota e
cancelamento não criam uma entrada reutilizável. Cancelar o compiler descarta a
avaliação incompleta.

`w explain const NAME` mostra valor, digest, dependências, target facts, steps,
heap máximo e materialização:

```text
w explain const restaurant.menu.instructionOpcodes
```

#### 3.6.6 Tipos, geração e feedback

Uma `const fn` retorna dados. Ela não retorna um tipo, AST ou fragmento de
source. Type identity continua declarada:

```w
type Time = String

extension Time {
  static const fn isValid(value: ref String): Bool {
    // Verificação total e sem I/O.
    ...
  }

  export static const fn parse(value: ref String): Time throws TimeError {
    guard isValid(value) else throw .invalid
    return Time(copy value)
  }
}

const closingTime = try Time.parse("23:45")
let requestedTime = try Time.parse(input)
```

Esse modelo valida o literal durante a compilação e o input em runtime.
Specialization pode remover uma validação já provada. `type(regex)` e funções
que constroem tipos arbitrários ficam **Rejeitado por enquanto**.

O evaluator produz ConstValue tipado. Ele não converte o valor em WLO, reparseia
source ou expande macro:

```text
ConstIR -> ConstValue -> HIR constant
```

WLO continua um codec de dados. Code generation usa uma tool target hermética
com outputs e source maps declarados. Essa separação preserva diagnostics,
cache, segurança e provenance.

PGO e feedback medido não alteram const, tipo, interface ou resultado. Um
profile pode orientar layout e optimization quando a recipe registra seu
digest:

```text
source + declared profile -> same semantics, different permitted optimization
```

A ideia histórica de substituir um valor source pelo resultado da execução
anterior fica **Rejeitado por enquanto**. Ela mudaria o programa por estado
externo implícito.

#### 3.6.7 ConstIR, MLIR e W0

O frontend baixa um `const fn` tipado para ConstIR. O evaluator executa ConstIR
antes do lowering para W/MLIR:

```text
typed HIR -> const dependency graph -> ConstIR evaluator -> verified HIR
           -> W/MLIR -> target
```

ConstIR preserva type, span, call edge, numeric policy e target dependency.
Valores finais viram attributes HIR. O adapter W/MLIR materializa constants
adequados ao tipo.

A interface de um `const fn` exportado inclui ConstIR normalizada e digest. Um
importer não precisa do source original para avaliar uma call:

```text
public signature + ConstIR digest + ConstIR body
```

Um const público pequeno pode ficar inline na interface. Um resultado grande
usa digest e blob no CAS. Os dois formatos representam o mesmo ConstValue
canônico e não alteram a semântica.

Const parameters ficam simbólicos até a instantiation:

```text
resolve kinds -> type-check parametric ConstIR -> substitute const arguments
              -> evaluate -> instantiate type/HIR
```

Overload e member lookup terminam antes da execução. O evaluator não cria novos
symbols nem reinicia name resolution.

MLIR constant folding continua uma otimização. A correção não depende do
canonicalizer. A
[documentação de canonicalization do MLIR](https://mlir.llvm.org/docs/Canonicalization/)
também trata o pass como best-effort e oferece materialização de constants por
attributes.

`bootstrap.w0` inclui a baseline CE0:

- literals, operators, constructors e `const fn`;
- scalars, tuples, structs, enums, Option e Result;
- String, Bytes, fixed array, Array, Map e Set;
- loops, recursion, typed errors e local mutation;
- target arithmetic e as quatro quotas.

CE0 não inclui const generics, reflection, type builders, FFI, closures
indiretas ou capabilities. O seed C e o compiler self-hosted devem produzir o
mesmo ConstValue normalizado.

Os precedentes principais são:

- [Rust const evaluation](https://doc.rust-lang.org/reference/const_eval.html),
  que separa const context e `const fn` e usa a semântica do target;
- [Zig comptime](https://ziglang.org/documentation/master/#comptime), que mostra
  execução rica e a necessidade de branch quota;
- [D CTFE](https://dlang.org/spec/function.html#interpretation), que reutiliza
  funções runtime em compile time.

W adota o mesmo corpo para as duas fases. W exige um contrato `const` visível,
quotas na recipe e nenhuma inspeção da fase.

## 4. Superfície integrada

O exemplo abaixo mostra a forma vigente. Ele não tenta mostrar toda a biblioteca.

```w
import { Request, Response } from std.http
import std.tensor as tensor

export type OrderId = u64
export type Ratio = f64<(0.0...1.0)>

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
  var completed: u64 = 0

  mut async fn prepare(order: take Order): Dish throws KitchenError {
    async<.network> let stock = checkStock(order)
    spawn<.compute> let plan = optimizePlan(order)

    let (stock, plan) = try await (stock, plan)
    return try await execute(stock, plan: plan)
  }
}

fn score(features: ref Tensor<f32, shape: [1, 8]>,
         weights: ref Tensor<f32, shape: [8, 4]>): Tensor<f32, shape: [1, 4]> {
  return features @ weights
}

entry(run) {
  process.signal = shutdown
}
```

O formatter mantém uma assinatura em uma linha quando ela cabe em 120 colunas.
O exemplo `score` quebra porque a forma completa ultrapassa esse limite.
O worker HTTP usa `entry LastLightWorker(fetch)` em outro módulo.

## 5. Source, nomes e edição

### 5.1 Source

**Exemplo:** dois identificadores Unicode que normalizam para o mesmo nome
produzem diagnostic antes do name lookup.

- A forma canônica usa UTF-8 sem BOM e LF.
- Keywords são ASCII, lowercase e case-sensitive.
- Identificadores usam Unicode conforme UAX #31 e são normalizados para NFC.
- O compilador rejeita dois nomes que normalizam para a mesma sequência.
- Confusables, scripts mistos e caracteres invisíveis produzem erro em API
  exportada. Código restrito ao módulo recebe erro ou warning conforme a edição.
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

**Exemplo:** `let answer = 42;` é aceito na migração, mas `w fmt` remove o
semicolon.

O formatter não emite `;`. O parser aceita `;` como separador de migração.
Semicolon não separa linhas de matriz no design vigente.

### 5.4 Controle e patterns

`if`, `guard`, `while` e `for` controlam statements. `switch` é uma expressão
exaustiva. Ele não possui fallthrough:

```w
enum PartySize {
  intimate
  regular
  cosmic
}

fn classify(
  stage: ServiceStage,
  guests: GuestCount,
): PartySize {
  return switch (stage, guests) {
    case (.accepted, 1...4): .intimate
    case (.accepted, 5...20): .regular
    case (.accepted, _): .cosmic
    case (_, _) if guests > 1_000: .cosmic
    case (_, _): .regular
  }
}
```

Uma tuple representa múltiplos scrutinees. W não adiciona uma forma especial
`switch a, b`. Tuple, enum, literal, range, struct e `_` são patterns fechados.
Bindings usam `let` dentro do pattern.

O compiler testa cases em ordem lexical. Um guard `if` executa depois que o
pattern combina. Um case sem guard que cobre um anterior gera diagnostic de case
inalcançável. Overlap restante mantém a regra first-match e aparece em
`w explain switch`.

Um enum fechado exige todos os cases ou `_`. Inteiros, strings e ranges exigem
`_` quando o compiler não prova cobertura. Todos os braços de um switch
expression produzem o mesmo tipo após conversões seguras.

`break` e `continue` pertencem a loops. Um switch não usa `break`. Um braço com
vários statements termina em expressão, `return`, `throw` ou `panic`.

**Pesquisa:** um protocol pode definir custom pattern matching. A proposta
precisa fechar pureza, custo, exhaustividade, captures e diagnostics. O design vigente usa
uma conversão nomeada ou um guard até esse contrato existir:

```w
switch request.routeKind() {
  case .menu: ...
  case .status: ...
  case .unknown: ...
}
```

### 5.5 Ordem de avaliação

W avalia receiver, operandos, argumentos, elementos de literal e entries de
collection da esquerda para a direita. Cada avaliação termina antes da próxima:

```w
let result = trace("receiver").transform(
  trace("first"),
  with: trace("second"),
)
expect Trace.events == ["receiver", "first", "second"]
```

`&&`, `||`, `??` e a expressão condicional avaliam somente o ramo necessário:

```w
let authorized = user?.canEnter() ?? false
let label = cachedLabel ?? loadLabel()
```

Uma assignment resolve o place uma vez, avalia o novo valor e só depois
substitui e destrói o valor anterior:

```w
buffer[nextIndex()] = makeValue()
// nextIndex() executa uma vez e antes de makeValue().
```

O optimizer pode reordenar operações somente quando o programa não consegue
observar diferença em valor, efeito, falha, cancelamento ou cleanup. Debug e
release preservam a mesma ordem observável.

## 6. Módulos, imports e visibilidade

O manifest define os arquivos de cada módulo. Um módulo pode conter vários
arquivos. O source não repete uma declaração `module`.

O mapeamento explícito atende módulos com vários arquivos:

```w
modules: [
  {
    name: "restaurant.menu"
    sources: ["menu/model.w", "menu/parser.w"]
  },
]
```

Um package com um módulo por arquivo pode usar uma expansão data-only:

```w
moduleSets: [
  {
    namespace: "restaurant"
    root: "."
    include: ["*.w"]
    exclude: ["package.w", "workspace.w"]
    layout: .fileStem
  },
]
```

`.fileStem` transforma `oracle.w` em `restaurant.oracle`. A expansão usa paths
portáteis, ordem lexical por bytes e erro em colisão. O resolver grava a lista
expandida no lock. Um arquivo novo não entra em um build `--locked` sem atualizar
essa lista.

Patterns não atravessam `root`, não seguem symlinks por default e não dependem
da ordem do filesystem. O mapeamento explícito continua a forma para exceções.

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

Declarations usam três níveis:

```w
fn localHelper()
package fn packageHelper()
export fn publicOperation()
```

| Forma | Alcance |
|---|---|
| sem modifier | módulo atual |
| `package` | package atual |
| `export` | importers do package |

O módulo é a menor boundary de encapsulamento. W não possui keyword `private`.
`friend` também não existe. Um `export { ... }` coletivo fica como
**Alternativa**. O modifier local melhora diff, busca e interface gerada.

A maioria dos membros usa o mesmo default de módulo. Existem três exceções
deliberadas:

1. stored fields de um `struct` transparente herdam a visibilidade do tipo;
2. cases de um `enum` herdam a visibilidade do enum;
3. requirements de um `protocol` herdam a visibilidade do protocol.

Um `struct` sem `init` explícito é transparente na interface source:

```w
export struct Guest {
  id: GuestId
  name: GuestName
}
```

`Guest.id`, `Guest.name` e o initializer memberwise são `export`. Um field
`var` também permite mutation aos callers com acesso e ownership adequado.
Essa transparência publica nomes, tipos, ordem lógica e mutabilidade. Ela não
fixa layout físico, ABI ou placement.

Um modifier explícito substitui a herança. `package field: T` pode estreitar um
componente de um struct exportado. Nesse caso, o initializer sintetizado usa o
nível menos visível entre tipo e fields. Para manter um field no módulo, o tipo
deve declarar `init` e ficar encapsulado. `w lint` detecta `export` redundante
em componente que já herdou esse nível.

Um `struct` com `init` explícito é encapsulado. Seus fields voltam ao default de
módulo. O autor publica somente os initializers, properties e methods
necessários:

```w
export struct PidController {
  proportionalGain: f64
  var accumulatedError: f64

  export init(proportionalGain: f64) throws KitchenError { ... }
  export isIdle: Bool { get => accumulatedError == 0.0 }
}
```

Adicionar um `init` a um struct transparente é uma mudança source-breaking. O
compiler mostra os membros que deixam a interface antes de aceitar a mudança.

`object` sempre é encapsulado. Um object exportado não publica storage nem
constructor por consequência:

```w
export object StockReservation {
  id: ReservationId
  export ingredients: Array<Ingredient>
  releaser: ServiceRef<PantryLeaseApi>

  export take async fn release() throws PantryError { ... }
}
```

Um field ou method de object precisa de `package` ou `export` para cruzar o
módulo. Um field `var` exportado permite mutation direta somente com acesso
exclusivo. Um `shared` owner continua sujeito às regras de alias e atomics.

Stored fields de `service` são sempre detalhes da implementação. O compiler
rejeita `package` e `export` nesses fields. Uma `ServiceRef<P>` publica somente
os requirements async de `P`. Uma computed property síncrona não atravessa uma
service boundary.

Cases de enum não repetem `export`. Um enum exportado seria inútil sem seus cases.
Protocol requirements também não repetem o modifier. Um witness recebe a
visibilidade efetiva do requirement:

```w
export protocol CompletionMetric {
  completionCount: u64 { get }
}

export object BrigadeMetrics: CompletionMetric {
  var atomic completed: u64 = 0
  completionCount: u64 { get => completed }
}
```

Methods, computed properties, associated members e initializers não herdam a
visibilidade do tipo. Um protocol witness é a única exceção. Essa regra evita
publicar behavior novo por acidente.

Uma declaração não pode expor um tipo menos visível em sua assinatura. A mesma
regra cobre generic constraints, error types, enum payloads e field types. Uma
reexportação também não amplia a visibilidade original.

`w interface` grava a visibilidade efetiva de cada membro.
`w explain visibility Type.member` mostra regra, origem e blockers. A HIR não
depende de defaults depois da normalização.

O default de módulo e as exceções de enum/protocol seguem o
[modelo de Rust](https://doc.rust-lang.org/reference/visibility-and-privacy.html).
O [Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/accesscontrol/)
exige opt-in para membros públicos e initializers memberwise públicos. W
preserva esse controle em tipos encapsulados. Para records simples, W adota a
concisão dos
[records Java](https://docs.oracle.com/en/java/javase/26/docs/api/java.base/java/lang/Record.html).

**Alternativa:** exigir `export` em cada field, como o opt-in de Swift e Rust.
Outra alternativa exporta todos os membros de qualquer tipo exportado. A forma
líder exporta somente os componentes de um struct transparente.

### 6.1 Evolução da interface exportada

W promete compatibilidade de source entre versões compatíveis de um package.
Uma atualização recompila os dependentes. O design vigente não promete substituir uma
library compilada por outra versão sem rebuild.

Um struct transparente exportado é resiliente no source por default. Um pattern
usado fora do package que define o tipo deve terminar com `...`. Essa regra vale
mesmo quando o pattern lista todos os fields conhecidos:

```w
let ref Order(guests, course, ...) = order
```

O marker confirma que o caller aceita fields futuros. Dentro do package, omitir
`...` exige todos os stored fields visíveis. O compiler verifica essa lista
quando o tipo muda.

`w interface diff OLD NEW` classifica duas interfaces normalizadas. O comando
considera estas regras:

| Mudança exportada | Classificação default |
|---|---|
| adicionar stored field com default | minor |
| adicionar stored field obrigatório | major |
| remover, renomear ou reordenar stored field | major |
| mudar tipo, mutabilidade ou reduzir visibilidade | major |
| mudar receiver mode entre `fn`, `mut fn` e `take fn` | major |
| adicionar `deinit` ou remover `Copy` | major |
| adicionar `init` explícito a struct transparente | major |
| adicionar forma disjunta a overload set existente | minor |
| criar o primeiro overload de uma função singular | major |
| adicionar initializer com forma disjunta | minor |
| adicionar função ou computed property sem mudar lookup | minor |
| adicionar ou mudar um default | revisão necessária |
| adicionar case a enum fechado | major |
| remover, renomear ou reordenar uma forma existente | major |
| alterar resolução de nome de uma call existente | major |

Uma classificação default deixa de ser automática quando a mudança altera
member lookup, overload resolution ou uma conformance existente.

Um field novo com default mantém calls existentes do initializer sintetizado.
Patterns externos continuam válidos porque já possuem `...`. Um field com
visibilidade menor também pode reduzir a visibilidade do initializer. Nesse
caso, a mudança é major.

Enums do design vigente são fechados. Um `switch` exaustivo recebe um diagnostic quando a
versão adiciona um case. W não inclui uma forma `nonexhaustive` no design vigente.

Compatibilidade de source não define layout, ABI, JSON, WLO ou wRPC. Um schema
de wire ou persistência possui versão e regras próprias. O compiler não deriva
uma mudança de schema somente porque um struct ganhou um field.

Esta direção usa a resiliência de
[Swift Library Evolution](https://www.swift.org/blog/library-evolution/) sem
publicar layout. O `...` externo cumpre o papel de abertura explícita do
[`non_exhaustive` de Rust](https://doc.rust-lang.org/reference/attributes/type_system.html).
A classificação segue
[Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html) e registra
casos de conflito para revisão.

**Alternativa:** todo pattern externo pode ser exaustivo e qualquer field novo
é major. Outra alternativa exige um modifier de resiliência no tipo. A forma
líder evita annotations e torna a aceitação de fields futuros visível no uso.

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
3. `const`, para call aceita pelo evaluator;
4. `unsafe`, se necessário;
5. `mut` ou `take`, para mudar o receiver mode;
6. `async`;
7. `fn` ou `fn<Language>`.

`throws E` fica depois do return type. Uma função sem return type retorna `()`.
Esse unit type possui um único valor, também escrito `()`. `Void` permanece
**Alternativa** de superfície. `Never` identifica uma função ou expressão que
não retorna ao caller.

O primeiro argumento é posicional por default. Os seguintes usam o nome como
label. Um label explícito substitui o default. `_` remove um label.
Os argumentos mantêm a ordem da declaração. Labels identificam papéis, mas não
permitem reordenar argumentos.

Dentro de type, protocol, service ou extension, `fn` recebe `self` por borrow.
`mut fn` recebe `self` com mutation exclusiva. `take fn` recebe ownership.
`static fn` não recebe `self`:

```w
enum Course {
  soup
  cake

  static fn fromOrdinal(value: usize): Course { ... }
  fn isSweet(): Bool { ... }
}
```

Os receiver modes são:

| Forma | Receiver | Efeito no caller |
|---|---|---|
| `fn` | `ref self` | preserva o owner |
| `mut fn` | `inout self` | preserva o owner e permite mutation |
| `take fn` | `take self` | transfere e invalida o binding |
| `static fn` | nenhum | não usa uma instance |

`mut`, `take` e `static` são exclusivos. Um `take fn` possui ownership local de
`self` e pode mutar esse valor. Essa mutation não aparece no caller.
`take fn` fora de um member é erro. Uma free function usa parâmetro `take`.

O call site transfere o receiver de forma explícita:

```w
let tail = try (take stream).finish()
try await (take reservation).release()
```

Os parênteses aplicam `take` ao receiver antes do member lookup. A forma
`take value.method()` não é válida. Ela poderia parecer uma transferência do
resultado. Uma free function continua usando `finish(take stream)`.

Um `take fn` pode transferir `self` inteiro para outro owner. Ele também pode
retornar `Self`. Se não transferir `self`, o método executa `deinit` e drop ao
terminar.

`: self` não é válido em `take fn`. Esse return type seria um borrow de um
receiver que termina na call. `: Self` continua um resultado owned.

O receiver fica consumido em success, error e cancellation. Um `catch` não
restaura o binding do caller. Uma operação que permite retry usa `mut fn` ou
retorna um outcome que carrega o owner no case de retry.

Um call com receiver `Copy` também invalida o binding quando usa `take`. O
caller escreve `(take copy value).method()` quando precisa preservar uma cópia.

Protocol requirements registram o receiver mode. O witness deve usar o mesmo
mode. Uma service instance não implementa `take fn`, pois o host controla seu
lifecycle. Um `take fn` também não pode ser chamado por `ref`, `inout`, `shared`,
`weak` ou `ServiceRef`.

W reutiliza `take` em vez de adicionar `consuming`. A direção acompanha os
ownership modifiers da
[SE-0377](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0377-parameter-ownership-modifiers.md)
e o receiver by-value do
[Rust Reference](https://doc.rust-lang.org/reference/items/associated-items.html#methods).

**Alternativa:** consumir o receiver sem marker no call site, como Swift e Rust.
Outra alternativa usa somente uma free function com parâmetro `take`. A forma
líder mantém a transferência visível e preserva method lookup.

Um método fluente declara `: self`:

```w
struct OrderState {
  var stage: ServiceStage

  mut fn advance(to next: ServiceStage): self throws DomainError {
    guard canMove(from: stage, to: next) else {
      throw .invalidTransition(from: stage, to: next)
    }

    stage = next
  }
}
```

`: self` retorna um borrow do mesmo receiver. Ele não copia, move ou aloca o
valor. `fn` retorna o borrow compartilhado. `mut fn` retorna o borrow exclusivo.
Fallthrough, `return` e `return self` concluem com esse receiver. Outra expressão
de retorno é erro. Dentro do método, `self.member` desambigua um field ou method
ocultado por um nome local.

`: Self` continua um return type normal e owned. `: self` não é válido em função
livre ou `static fn`. Um método `async` segue as regras normais para borrow
suspenso.

Omitir o return type não retorna `self`. O retorno implícito faria uma função de
efeito parecer uma transformação. Ele também ocultaria borrow, copy ou move em
um receiver com owner único.

#### 7.2.1 Overloads por forma de call

W permite overloads quando a sintaxe do call site seleciona uma declaração sem
consultar tipos:

```w
export fn expectedEnergy(
  telemetry: ref OvenTelemetry,
  during duration: Duration,
): Energy {
  return energy(telemetry.power * telemetry.duty, during: duration)
}

export fn expectedEnergy(
  power: Power,
  duty: DutyCycle,
  during duration: Duration,
): Energy {
  return energy(power * duty, during: duration)
}
```

As formas normalizadas são:

```text
expectedEnergy(_, during:)
expectedEnergy(_, duty:, during:)
```

Uma forma de call é a sequência ordenada dos labels externos. `_` identifica um
argumento posicional. A quantidade de itens identifica a aridade.

Name lookup primeiro encontra um único owner e seu overload set. Free functions,
instance methods e static functions pertencem a espaços de lookup distintos.
Imports de módulos diferentes não fundem overload sets. Um conflito exige alias
ou import seletivo.

O compiler resolve uma call nesta ordem:

1. calcula a forma de call;
2. seleciona a única declaração que aceita essa forma;
3. verifica tipos, generics, ownership, efeitos e conversões;
4. grava a declaração qualificada na HIR.

O compiler não volta a outra declaração quando a etapa 3 falha. Tipos de
parâmetros, return type, constraints, receiver mode, `async`, `throws`,
ownership e conversões não ordenam candidatos. Duas declarações do mesmo owner
não podem aceitar a mesma forma.

Um parâmetro com default cria formas adicionais. Cada forma deve mapear uma
única lista de parâmetros. Um argumento labeled com default pode ser omitido.
Entre os parâmetros `_`, somente um sufixo com default pode ser omitido. O
compiler rejeita a declaração se duas omissões produzirem a mesma forma.

Estas declarações entram em conflito:

```w
fn parse(value: String): GuestId
fn parse(value: Bytes): GuestId

fn serve(order: Order)
fn serve(order: Order, mode: ServiceMode = .normal)
```

A segunda família aceita `serve(_)` duas vezes. Use labels, generics,
protocols ou nomes diferentes:

```w
fn parseText(value: String): GuestId
fn parseBytes(value: Bytes): GuestId

fn serve(order: Order)
fn serve(order: Order, with mode: ServiceMode)
```

O nome de uma função singular pode ser um valor. Um nome que resolve para um
overload set não seleciona uma declaração. O programa cria uma closure explícita:

```w
let estimator = (power, duty, duration) =>
  expectedEnergy(power, duty: duty, during: duration)
```

O expected type da variável não escolhe a declaração. Um seletor explícito por
forma permanece **Alternativa**. A forma pode ser parecida com
`fn expectedEnergy(_:, duty:, during:)`.

A HIR registra o function type de todo callable. O design vigente infere o tipo de closures
e referências singulares. A seção 7.5 define a annotation e a representação
observável desses valores.

Somente uma extension no package do tipo pode ampliar um overload set existente.
A forma nova deve ser disjunta da interface completa do owner. Uma extension
externa não funde declarações nesse set. Um protocol requirement e seu witness
registram nome, forma, signature e receiver mode.

`w explain call` mostra owner, forma normalizada, declaração selecionada,
inference e conversões. `w interface diff` compara o conjunto de formas aceitas.
Adicionar uma forma disjunta a um set existente é minor por default. Criar o
primeiro overload de uma função singular é major, pois referências pelo nome
podem falhar. Initializers não são valores. Adicionar um initializer disjunto é
minor. Alterar uma forma existente é major. Adicionar um default exige revisão.

O modelo de labels acompanha a legibilidade dos
[argument labels de Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/declarations/).
W não adota o ranking de “melhor membro” da
[resolução de overloads de C#](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/language-specification/expressions#1264-overload-resolution).
A regra também evita a seleção frágil por tipos descrita no
[FAQ de Go](https://go.dev/doc/faq#overloading).

**Alternativa:** tipos e constraints podem escolher o melhor candidato. Outra
alternativa proíbe todo overload e exige nomes distintos. A forma vigente permite
APIs naturais e mantém a seleção local, finita e reproduzível.

Parâmetros rest homogêneos entram no design vigente. A forma `T...` aceita zero ou mais
argumentos do mesmo tipo. A seção 8.9.5 define labels, ownership, overlap e
lowering. Type packs heterogêneos continuam em **Pesquisa**.

### 7.3 Parâmetros e ownership

```w
fn inspect(value: ref Value)
fn edit(value: inout Value)
fn store(value: take Value)
fn transform(value: Value): Result
fn prepare(format: const Format): PreparedFormat
```

Um parâmetro `T` é pass-by-value. Um tipo `Copy` produz uma cópia semântica
implícita e de custo limitado. Um tipo owned que não atende a `Copy` pode ser
movido no último uso. `take T` exige transferência e mostra essa exigência na
assinatura e no call site.

```w
inspect(value)
edit(inout value)
store(take value)
let duplicate = copy value
```

`ref` não aparece no call site porque não altera ownership. `inout`, `take` e
`copy` aparecem. `copy value` usa `Duplicable` quando o valor não é `Copy`. A
operação é explícita porque pode percorrer storage ou alocar:

```w
protocol Duplicable {
  fn duplicate(): Self
}

let titleCopy = copy title           // String: O(bytes)
let menuCopy = copy menu             // Array<String>: O(elements + bytes)
let socketCopy = copy socket         // error: Socket is not Duplicable
```

`Copy` refina `Duplicable`, mas o compiler pode implementar a duplicação como
uma operação escalar. String, Bytes, Array, Map e Set atendem a `Duplicable`
quando seus elementos atendem. Um resource, capability ou owner singular não
ganha conformance automática.

`const` não é um ownership mode. Ele exige um argumento compile-time e não
adiciona syntax no call site. A seção 3.6.1 define o requisito.

Quando o operand é `ref T`, `copy` materializa um `T` owned. Ele não copia o
borrow:

```w
guard let ref recipe = recipes[course] else panic("recipe invariant failed")
let ownedRecipe: Recipe = copy recipe
```

Uma implementação de `Duplicable` é nonthrowing sob a policy normal de OOM. Ela
cria um valor semanticamente independente. COW só pode otimizar a operação
quando allocation, budget, deallocator, failure e cleanup não observam a
mudança. Mutar um resultado nunca altera o outro:

```w
let original = "Last Light"
var duplicate = copy original
duplicate.append("!")
expect original == "Last Light"
```

Partial move exige destructuring. O design vigente não permite mover um field e continuar a
usar o aggregate parcialmente inicializado.

### 7.4 Patterns de struct

O pattern de struct usa o nome do tipo e dos fields:

```w
let ref Order(guests, course, ...) = order
let Guest(id, name, ...) = take guest
let Order(id: orderId, guest: Guest(name, ...), ...) = take order
let inout Table(state, ...) = inout table
```

Um field sem `:` também cria um binding com o mesmo nome. `field: pattern`
renomeia, ignora com `_` ou aplica um pattern aninhado. Os fields listados
seguem a ordem da declaração. `...` ocorre no máximo uma vez e fica no fim.
Ele cobre os fields não listados.

Dentro de `Type(...)`, o token isolado `...` significa rest. Um range sempre
possui um operando. O parser distingue as duas formas sem consultar tipos.

O qualifier após `let` define um modo uniforme:

| Forma | Resultado |
|---|---|
| `let Type(...) = take value` | consome o aggregate e cria values owned |
| `let Type(...) = copy value` | copia o aggregate e cria values owned |
| `let ref Type(...) = value` | cria borrows compartilhados dos fields |
| `let inout Type(...) = inout value` | cria borrows exclusivos dos fields |

O modo owned consome ou copia o aggregate inteiro. Os fields cobertos por `...`
são destruídos com o restante do valor quando aplicável. O binding original não
fica parcialmente inicializado.

Um pattern emprestado aceita somente `let`. A mutation vem do modo `inout`, não
de `var`. Os borrows de fields distintos podem coexistir. O owner completo não
pode ser movido enquanto um desses borrows estiver vivo.

O design vigente não mistura `copy`, `ref`, `inout` e `take` dentro do mesmo pattern. O
programa usa projeções de field quando precisa de modos diferentes.

Somente stored fields visíveis no ponto de uso participam do pattern. Um struct
encapsulado aceita destructuring no módulo que controla seu storage. `object` e
`service` não aceitam destructuring. Essas categorias preservam identidade,
invariantes e cleanup atrás da API nominal.

Um struct com `deinit` customizado não aceita destructuring owned. O pattern
emprestado continua válido dentro do módulo. Essa regra impede que um pattern
ignore ou execute duas vezes o cleanup customizado.

**Alternativa:** usar `{field}` como record pattern. Outra alternativa usa
posições sem nomes. A forma vigente reutiliza `Type(...)`, mantém labels nominais
e evita reservar `{}` para um segundo modelo de record.

### 7.5 Valores callable e closures

O design vigente separa três formas. A separação torna capture, erasure e ABI observáveis:

| Tipo | Conteúdo | Uso |
|---|---|---|
| `fn(A): B` | ponteiro fino para função W sem capture | callback estático e call indireta |
| `some fn(A): B` | tipo concreto oculto, com ambiente conhecido pelo compiler | parâmetro ou retorno especializado |
| `any fn(A): B` | callable apagado, com owner, invoke e drop | storage ou seleção runtime |

Uma função singular e uma closure sem capture podem formar `fn(A): B`:

```w
fn double(value: Int): Int {
  return value * 2
}

let operation: fn(Int): Int = double
let result = operation(21)
```

Um parâmetro `some fn` é generic shorthand. Cada call preserva o tipo concreto e
permite specialization. Um retorno `some fn` precisa usar o mesmo tipo concreto
em todos os caminhos. Numa annotation local, o initializer fixa o tipo concreto:

```w
fn map<T, U>(
  values: ref Array<T>,
  using transform: some fn(ref T): U,
): Array<U> {
  // ...
}

fn makeEstimator(
  model: take Model,
): some fn(ref Observation): Score {
  return capture(take model) (observation) => model.score(observation)
}

let estimate: some fn(Int): Int = (value) => value * 2
```

`any fn` apaga a identidade do ambiente. O valor continua owned e move-first.
A representação contém uma operação de invoke e uma operação de drop. Ela pode
usar storage inline ou indireto sem mudar a semântica:

```w
struct Route {
  handler: any fn(Request): Response
}

fn chooseRoute(
  usePreview: Bool,
  stable: take any fn(Request): Response,
  preview: take any fn(Request): Response,
): Route {
  if usePreview {
    return Route(handler: take preview)
  }

  return Route(handler: take stable)
}
```

Erasure, escape ou capture pode exigir allocation. `w explain cost` mostra essa
decisão e o owner. O profile mantém a policy normal de allocation failure. A
otimização não pode mudar o momento observável do drop.

Function types não possuem labels, defaults ou nomes de parâmetros. Esses itens
pertencem à declaração. Uma call direta usa a forma declarada. Uma call por valor
usa todos os argumentos em ordem:

```w
fn energy(power: Power, during duration: Duration): Energy { ... }

let estimate: fn(Power, Duration): Energy =
  (power, duration) => energy(power, during: duration)

let result = estimate(2<si.W>, 3<si.s>)
```

Um overload set não converte para callable por expected type. O programa usa uma
closure explícita, como mostra a seção 7.2.1. Defaults também não acompanham o
valor callable.

O callable mode descreve como o corpo usa o ambiente capturado:

| Forma | Ambiente | Calls permitidas |
|---|---|---|
| `fn` | não sofre mutation nem move | repetidas por borrow compartilhado |
| `mut fn` | pode sofrer mutation | repetidas por acesso exclusivo |
| `take fn` | pode mover valores capturados | uma call que consome o callable |

`mut fn` e `take fn` exigem `some` ou `any`, pois um ponteiro fino não possui
ambiente. Um callable `fn` satisfaz um parâmetro `mut fn` ou `take fn`. Um
callable `mut fn` satisfaz um parâmetro `take fn`. A ordem inversa é inválida.

Um ponteiro fino `fn(...)` atende a `Copy`. Portanto, `take fn(...)` sempre
identifica callable mode. A transferência de um callable apagado usa
`take any fn(...)`.

```w
fn counter(
  initial: usize,
): some mut fn(): usize {
  var next = initial

  return capture(take next) () => {
    next += 1
    return next
  }
}

fn closingNotice(
  log: take ShiftLog,
): some take fn(): AuditRecord {
  return capture(take log) () => (take log).seal()
}

var nextTicket = counter(initial: 40)
let ticket = nextTicket()

let close = closingNotice(take shiftLog)
let audit = (take close)()
```

O compiler infere o mode menos restritivo. Uma annotation pode exigir um mode
mais restritivo. A call de `mut fn` exige um callable mutável e acesso exclusivo.
A call de `take fn` usa a mesma forma explícita dos receivers consuming.

Ownership dos parâmetros e effects fazem parte do function type:

```w
fn load(
  path: ref Path,
  using loader: some async fn(ref Path): Bytes throws IoError,
): Bytes throws IoError

struct Pipeline {
  transform: any mut fn(inout Buffer, take Command): Result
  finalize: any take fn(take Session): Receipt
}
```

A ordem canônica é qualifier, `unsafe`, callable mode, `async`, `fn`, parâmetros,
return e `throws`. A omissão do return type significa `()`.

Uma call preserva os markers da assinatura. `unsafe` exige bloco `unsafe`.
`async` exige `await`. `throws E` exige `try` ou propagação de `E`.

Function types são invariantes no design vigente. Parameter types, ownership, return,
error, `async`, `unsafe` e ABI precisam coincidir. Somente a relação entre
callable modes definida acima permite adaptação automática. Outra mudança usa
uma closure explícita:

```w
fn readByte(index: usize): u8 { ... }

let readWord: fn(usize): u16 =
  (index) => u16(readByte(index))
```

Captures são inferidos. `capture(...)` substitui a inferência nos casos
importantes. Os modos são `copy`, `ref`, `take` e `weak`:

```w
let task = capture(take model, ref cache) (input) => {
  return model.run(input, cache: cache)
}
```

Uma closure possui um tipo anônimo semelhante a uma struct de captures. A HIR
registra cada place capturado, seu modo, lifetime, owner e drop path. Uma closure
armazenada não captura `inout`. Um borrow `ref` só escapa quando o lifetime do
owner cobre todo o destino.

Um child estruturado pode manter um borrow exclusivo passado como `inout`. O
owner e o task frame precisam ficar estáveis. O parent não acessa o valor antes
do join. Um runtime owner ou `SharedTask` não recebe esse borrow.

Um ponteiro fino atende a `transferable` e `shareable`. Um callable concreto ou
apagado atende a esses predicates somente quando seu ambiente atende. `mut fn`
não atende a `shareable`, pois a call exige exclusividade. Ele pode atender a
`transferable`. `spawn` aplica essas regras ao callable e a cada capture.

Uma referência a instance method não captura `self` implicitamente. O programa
mostra o mode com uma closure:

```w
let bake = capture(ref oven) (order) => oven.bake(order)
```

Uma função generic precisa ter todos os argumentos de tipo resolvidos antes da
conversão. Igualdade, ordering e hash de callables não existem.

Uma fronteira C usa um ponteiro fino com ABI explícita:

```w
type SensorCallback =
  unsafe fn<abi: .c>(c.ptr<void>, c.int): ()
```

Esse tipo aceita somente carriers C. Ele não aceita capture, `async` ou
`throws`. Um callback C stateful usa um context pointer e um owner/deallocator
explícitos. O ABI de `fn(A): B` normal é interno ao build e não é uma promessa de
package ABI.

`(args) => body` é a única forma de closure do design vigente. `{ args in body }` e
`fn(args) { body }` ficam como alternativas de corpus.

A separação segue três precedentes. Swift removeu labels dos function types.
Rust separa function pointers e tipos anônimos de closure. Clang Blocks torna
invoke, ambiente, copy e dispose explícitos no ABI:

- [Swift SE-0111](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0111-remove-arg-label-type-significance.md)
- [Rust function pointer types](https://doc.rust-lang.org/reference/types/function-pointer.html)
- [Rust closure types](https://doc.rust-lang.org/reference/types/closure.html)
- [Clang Blocks ABI](https://clang.llvm.org/docs/Block-ABI-Apple.html)

**Alternativa:** um único `fn` apagado simplifica annotations, mas oculta capture,
dispatch e possível allocation. Outra alternativa usa protocols `Fn`, `FnMut` e
`FnOnce`. A forma vigente reutiliza `some`, `any`, `mut` e `take`.

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
| `some P` | tipo concreto preservado; a interface expõe somente `P` |

Herança de implementação não entra no design vigente. Composição, protocols e funções
livres são a baseline.

`some P` em um parâmetro é shorthand para um generic anônimo. `some P` em um
return type oculta um tipo concreto único:

```w
fn render(value: some Display): String
// Equivale a: fn render<T: Display>(value: T): String

fn activePolicy(): some PricingPolicy {
  return StandardPricing()
}
```

Essa regra também atende `some fn(...)`. Ela preserva specialization sem exigir
um nome generic usado uma única vez. A forma possui precedente nos
[opaque parameter declarations de Swift](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0341-opaque-parameters.md).

Extensions não adicionam storage:

```w
extension Dish: Display {
  fn write(to output: inout String): () { ... }
}
```

Tuples podem nomear seus elementos:

```w
let row: (id: i32, randomNumber: i32) = (
  id: 42,
  randomNumber: 7,
)

print(row.id)
```

Labels pertencem ao tipo. `(id: i32, value: i32)` não é o mesmo tipo que
`(value: i32, id: i32)` ou `(i32, i32)`. Uma conversão que remove labels é
explícita. Function arguments também não viram tuple implicitamente.

```w
let positional: (i32, i32) = (row.id, row.randomNumber)
```

Um tuple usa labels em todos os elementos ou em nenhum. `(id: i32, String)` é
erro. Um tuple sem labels usa destructuring. Os members posicionais `.0`, `.1`
e seguintes existem para composição genérica, mas uma API exportada deve
preferir labels ou um struct nominal. Um tuple de um elemento exige a vírgula:
`(value: i32,)` ou `(i32,)`.

Uma conformance pode ser declarada no módulo do tipo ou no módulo do protocol.
Essa regra evita conformances órfãs e conflitos dependentes da ordem de import.

### 8.2 Associated members, protocols e singleton

Um nome de tipo cria uma identidade compile-time e um namespace. Ele não cria
um objeto runtime. `struct`, `object` e `enum` podem declarar associated members:

```w
export struct Money {
  minorUnits: i128
  currency: Currency

  export const zeroCredits = Money(minorUnits: 0, currency: .cr)
}

export enum Course {
  broth
  cake

  export static fn fromOrdinal(value: usize): Course {
    return switch value {
      case 0: .broth
      case 1: .cake
      case _: panic("Course ordinal outside the closed enum")
    }
  }
}

let zero = Money.zeroCredits
let cake = Course.fromOrdinal(1)
```

`const` dentro do tipo declara um associated compile-time value. `static fn`
declara uma associated function. `static const fn` também permite avaliação
compile-time. O acesso usa `Type.member`. W usa lower camel case também para
constantes. Por exemplo, a biblioteca usa `u64.max`, não `Number.MAX_VALUE`.

Uma declaração direta não exige um `protocol`. O `protocol` é necessário quando
código generic precisa exigir o member:

```w
export protocol Sequence<Element> {
  const empty: Self
  static fn from(items: Array<Element>): Self
  fn first(): Element?
}

export struct Menu: Sequence {
  alias Element = Dish
  const empty = Menu(dishes: [])
  dishes: Array<Dish>

  static fn from(items: Array<Dish>): Menu { return Menu(dishes: items) }
  fn first(): Dish? { ... }
}
```

`Self` representa o tipo concreto que atende ao requisito. `Element` no head
declara um primary associated type. `alias Element = Dish` fornece o witness.
Um protocol pode exigir `const`, `static fn` e methods. Ele nunca cria storage.

O corpo que define um `struct` ou `object` pode declarar instance fields.
Protocol e extension não adicionam instance storage. Uma extension pode
adicionar `const`, `static fn` e `static const fn` quando coherence permite.

W não possui `static var` nem outro mutable type storage no design vigente. Esse storage
criaria estado global, ordem de inicialização, sincronização e destruction
ocultas. Um associated value runtime usa `static fn`. Estado compartilhado usa
um owner explícito criado por `entry` ou uma service instance com key explícita.

```w
entry {
  let catalog = Catalog(...)
  run(catalog: take catalog)
}
```

Um `object Catalog` pode ter várias instances. O binding acima expressa um
singleton do product, não da linguagem. Um módulo também não é singleton.

W não reifica tipos como `Type<T>` no design vigente. Associated member lookup continua
compile-time. `reflect.TypeId` oferece identidade runtime local. Uma conformance
a `reflect.Reflectable` solicita metadata estrutural. A seção 8.9 define os dois
contratos.

### 8.3 Construção e inicialização

`Type(...)` constrói uma instance. A forma não promete heap, stack ou região.
O optimizer escolhe o storage sem mudar ownership, identidade ou drop:

```w
let controller = try PidController(
  proportionalGain: 0.8,
  integralGain: 0.1,
  derivativeGain: 0.02,
)
```

O parser reconhece uma call expression. Name resolution transforma a chamada
de um tipo em `construct` na HIR. W não usa `new`. Essa keyword sugeriria uma
alocação que a semântica não exige.

Um `struct` ou `object` sem `init` explícito recebe um initializer sintetizado.
A assinatura segue estas regras:

1. cada field stored cria um parâmetro com label obrigatório;
2. um field sem initializer cria um parâmetro obrigatório;
3. um field com initializer cria um parâmetro que pode ser omitido;
4. computed properties não criam parâmetros;
5. os argumentos seguem a ordem de declaração dos fields;
6. cada valor é avaliado e instalado nessa mesma ordem;
7. um struct usa o nível menos visível entre tipo e fields;
8. um object mantém o initializer no módulo.

Um `export struct` transparente recebe um initializer `export` no caso comum.
Adicionar um field obrigatório quebra seus callers. Um field com default
preserva essas calls, mas altera a interface do record. `w interface` publica a
assinatura sintetizada e classifica a mudança.

Um tipo com invariantes declara um initializer:

```w
export struct PidController {
  proportionalGain: f64
  integralGain: f64
  derivativeGain: f64
  var accumulatedError: f64
  var previousError: f64

  export init(
    proportionalGain: f64,
    integralGain: f64,
    derivativeGain: f64,
  ) throws KitchenError {
    guard proportionalGain.isFinite && proportionalGain >= 0.0 else {
      throw .invalidControllerGain(kind: .proportional, value: proportionalGain)
    }

    self.proportionalGain = proportionalGain
    self.integralGain = integralGain
    self.derivativeGain = derivativeGain
    self.accumulatedError = 0.0
    self.previousError = 0.0
  }
}
```

`init` é contextual ao corpo de `struct` ou `object`. Ele não recebe `fn`,
`static`, `mut`, `async` ou return type. Ele pode receber visibilidade,
`unsafe` e `throws E`. Todos os parâmetros usam labels por default. `_` remove
um label.

Um tipo pode declarar vários initializers. Suas formas de call devem ser
disjuntas conforme a seção 7.2.1:

```w
export struct Money {
  export minorUnits: i128
  export currency: Currency

  export init(minorUnits: i128, currency: Currency) {
    self.minorUnits = minorUnits
    self.currency = currency
  }

  export init(majorUnits: i64, currency: Currency) throws DomainError {
    let minorUnits = try i128.checkedMultiply(i128(majorUnits), 100)
      .mapError((_) => .overflow)

    self = Money(minorUnits: minorUnits, currency: currency)
  }
}
```

As formas são `Money(minorUnits:, currency:)` e
`Money(majorUnits:, currency:)`. Os tipos não participam da seleção.

A presença de qualquer `init` remove o initializer sintetizado da interface
source. O compiler classifica cada initializer como direto ou delegante. Um
initializer direto inicializa todos os fields. Um initializer delegante chama
outro initializer do mesmo tipo. A forma de delegação é:

```w
self = Type(arguments)
```

Esse statement baixa para `delegate_init`. Ele não é uma assignment normal e
preserva a identidade do storage em construção. A expressão deve chamar
diretamente outro initializer do mesmo tipo.

Um initializer delegante segue estas regras:

1. cálculos locais e `guard` podem ocorrer antes da delegação;
2. nenhum field de `self` pode estar inicializado antes da delegação;
3. cada caminho normal delega exatamente uma vez;
4. depois da delegação, `self` é um valor completo;
5. ciclos no grafo de delegação são erros de compile time.

Uma declaração não pode misturar inicialização direta e delegação. Field
initializers executam somente no initializer direto final. Um frame delegante
não os executa novamente.

Uma construção com nome semântico usa `static fn`:

```w
export static async fn load(id: ControllerId): PidController throws LoadError
export static fn fromLegacy(value: LegacyController): PidController
```

Uma factory async torna a suspensão visível no call site. W não permite
`async init`. W também não possui `init?`. Uma falha usa `throws E`. Uma factory
pode retornar `Option<T>` quando ausência não é um erro.

Um `init` explícito usa seu próprio modifier. Ele não pode ser mais visível que
o tipo ou qualquer tipo de sua assinatura. Um object precisa de `export init`
ou de uma factory exportada para ser construído por outro módulo.

O verifier usa definite initialization em duas fases:

1. um initializer direto avalia field initializers na ordem de declaração;
2. `self` começa como um initialization place, não como um valor W;
3. `self.field = value` inicializa cada field restante uma vez;
4. todos os caminhos normais precisam inicializar os mesmos fields;
5. antes disso, o código não lê, empresta, chama método ou faz escape de `self`;
6. depois disso, `self` vira um valor completo e pode usar methods.

Um field imutável com default já está inicializado. O `init` não pode
reatribuí-lo. Um field `var` com default pode receber mutation. Essa mutation
não concede acesso a outro field ainda não inicializado.

Se a falha ocorre antes de `self` ficar completo, o runtime destrói os fields
completos em ordem inversa. Ele não executa `deinit`. Se a falha ocorre depois
de `self` ficar completo, o runtime executa `deinit` uma vez e depois destrói os
fields. W não possui zero initialization universal. Storage parcialmente
inicializado exige uma futura API `unsafe` própria.

Services usam o instance descriptor e o host. Enums usam seus cases. Um
refined type usa seu constructor fallible. Nenhuma dessas formas ganha `init`
por simetria.

A segurança em duas fases segue o objetivo da
[inicialização do Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/initialization/).
O risco de representar bytes ainda inválidos aparece no contrato de
[`MaybeUninit`](https://doc.rust-lang.org/core/mem/union.MaybeUninit.html).
O compact constructor de
[records Java](https://docs.oracle.com/en/java/javase/15/docs/specs/records-jls.html)
mostra o valor de validar antes de publicar o aggregate.

**Alternativa:** usar somente um initializer canônico e factories nomeadas.
Outra alternativa permite `init?`, `async init` ou delegação após inicialização
parcial. A forma vigente aceita vários initializers por forma, delegação total e
falha tipada.

### 8.4 Propriedades computadas

Uma computed property precisa declarar seus accessors. Ela continua diferente
de um stored field:

```w
export struct PaymentProof {
  paymentId: PaymentId
  amount: Money
  state: PaymentState

  export canServe: Bool {
    get => state == .captured
  }
}

object Cursor {
  var storedIndex: usize

  var index: usize {
    get => storedIndex
    set(value) => storedIndex = value

    modify {
      return inout storedIndex
    }
  }
}
```

Uma propriedade read-only não recebe `var`. Uma propriedade writable recebe
`var`, sempre declara `get` e declara `set`, `modify` ou ambos. W não possui
write-only property.

Os accessors usam estes receivers:

| Accessor | Receiver | Resultado |
|---|---|---|
| `get` | `ref self` | valor da propriedade |
| `set(value)` | `inout self` | substitui o valor lógico |
| `modify` | `inout self` | empresta um place com exclusividade |

`get => expression` e `set(value) => expression` são corpos curtos. Um bloco
permanece disponível. `modify` usa bloco. `return inout place` abre um borrow
escopado. O accessor retoma seus `defer` quando esse borrow termina.

Accessors são property-safe. O compiler aplica este teto de efeitos:

- são síncronos e não usam `throws`;
- não fazem I/O, bloqueio, service call ou device transfer;
- não criam tasks nem adquirem authority;
- não transferem ownership para fora de `self`;
- não fazem uma alocação geral oculta.

Reads, arithmetic curta e mutation do receiver são permitidos. Atomics
continuam com a semântica declarada pelo backing field. Uma operação que não
atende ao teto usa um método nomeado. Parentheses, `try` e `await` mostram o
custo no call site.

Um getter de receiver borrowed não move um valor move-only para fora de
`self`. Ele pode devolver um valor `Copy`, um novo valor owned ou uma view
permitida pelo borrow checker.

Um protocol pode exigir uma propriedade:

```w
protocol CompletionMetric {
  completionCount: u64 { get }
  var limit: usize { get set modify }
}
```

Um stored field compatível pode ser o witness. Uma extension pode adicionar
computed properties, mas não storage. Associated state continua ausente:
compile-time usa `const`, e cálculo associado usa `static fn` ou
`static const fn`.

Uma declaração não combina behavior e accessors explícitos. O behavior possui
o storage e gera os accessors. A inicialização do field chama `init` do
behavior. Ela não chama `set`. Um behavior publica seu contrato adicional de
custo conforme a seção 10.

Swift permite
[properties read-only com `async` e `throws`](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0310-effectful-readonly-properties.md).
W rejeita essa forma por enquanto. A rejeição preserva a expectativa de acesso
rápido e local. A separação entre stored e computed properties também segue as
[propriedades do Swift](https://docs.swift.org/swift-book/LanguageGuide/Properties.html).

**Alternativa:** permitir accessors com efeitos, observers e static computed
properties. Esses recursos precisam superar o corpus de previsibilidade antes
de entrar.

### 8.5 Option e ausência

#### 8.5.1 Tipo e significado

`T?` é `Option<T>`. O tipo possui somente `.some(T)` e `.none`:

```w
if let guest = findGuest(id) {
  greet(guest)
}

guard let guest = findGuest(id) else return .notFound
let name = guest?.name ?? "Anonymous"
```

`.none` significa somente ausência no domínio declarado. Ele não significa move,
falha, cancelamento ou storage sem inicialização:

```w
var selected: Course? = .none
selected = .some(.horizonCake)
```

W não possui `null`, `undefined`, `uninitialized` ou `empty` universais. Um
adapter preserva distinções externas com um tipo próprio:

```w
enum JsonField<T> {
  missing
  null
  value(T)
}
```

Definite initialization é estado do compiler. Move também é estado do compiler.
Nenhum dos dois grava `.none` no valor:

```w
let order = makeOrder()
submit(take order)
print(order.id) // error: order was moved
```

Storage não inicializado existe somente em `unsafe MaybeUninit<T>`. O tipo não
é um `T` até uma prova ou uma call `unsafe`:

```w
var slot = MaybeUninit<MenuToken>()
slot.write(.end(line: 1, column: 1))
let token = unsafe slot.assumeInitialized()
```

#### 8.5.2 Binding e ownership

Optional binding segue os modos de ownership comuns:

```w
if let ref recipe = recipes[course] { inspect(recipe) }
if let inout count = counts[course] { count += 1 }
if let copy title = cachedTitle { send(title) }
if let payment = gateway.poll() { archive(take payment) }
```

O binding sem modifier possui um valor owned. Ele move de um rvalue ou copia um
elemento `Copy`. Um lvalue non-Copy exige `ref`, `copy` ou `take()`:

```w
if let title = cachedTitle { print(title) }
// error: cachedTitle is a non-Copy lvalue

if let title = cachedTitle.take() { archive(take title) }
expect cachedTitle == .none
```

`Option.take()` exige acesso exclusivo. Ele move o payload e grava `.none`.
`Option<ref T>` e `Option<inout T>` preservam o borrow do payload.

#### 8.5.3 Chaining, fallback e propagação

`?.` faz leitura ou call condicional. O resultado possui um único nível de
Option, mesmo quando o member já retorna Option:

```w
let city: String? = guest?.address?.city
```

O flattening perde a causa da ausência de propósito. Um programa que precisa
distinguir as causas usa pattern matching:

```w
switch guest {
  case .none: record(.guestMissing)
  case .some(let value):
    if value.address == .none { record(.addressMissing) }
}
```

Optional chaining não faz mutation. Mutation condicional usa `if let inout`:

```w
guest?.visits += 1 // error: optional mutation can be skipped silently
if let inout value = guest { value.visits += 1 }
```

`??` avalia o lado direito somente quando o lado esquerdo é `.none`. O operador
associa à direita:

```w
let label = cachedLabel ?? storedLabel ?? computeLabel()
```

Postfix `?` propaga `.none` somente de uma função ou closure que retorna Option:

```w
fn firstOpen(orders: ref Array<Order>): ref Order? {
  let ref first = orders.first?
  return if first.isOpen { .some(first) } else { .none }
}
```

O operator preserva ownership. Um lvalue non-Copy precisa de borrow ou
`take()` antes da propagação. Ele nunca propaga `Result` ou `throws`:

```w
let owned = pending.take()? // move payload; pending fica .none
let value = result?         // error: Result uses try
```

W não possui postfix `!`, optional implicitamente unwrapped ou `try!`. Uma
invariante usa `expect` e informa a causa do panic. Ausência recuperável usa
`orThrow`:

```w
let config = loaded.expect("validated config disappeared")
let guest = try findGuest(id).orThrow(.unknownGuest(id))
```

`map`, `flatMap`, `filter`, `asRef` e `asInout` são APIs normais de Option. Elas
não criam control flow oculto além do contrato do método.

### 8.6 Newtype, alias e refinement

```w
type GuestId = u64
alias VisitorId = GuestId

type Ratio = f64<(0.0...1.0)>

type BoundedString<const min: usize, const max: usize> =
  String<(.scalars.count in min...max)>

type ShortLabel = BoundedString<min: 1, max: 40>
```

`<(...)>` aplica um refinement estático ao tipo completo. Um range no slot
primário inclui o subject e o operador `in`. Outros predicates usam `.member`
para acessar o mesmo subject. `value.member` continua disponível quando a forma
curta é ambígua.

Um literal válido pode ser provado em compile time. Um valor runtime usa um
construtor fallible:

```w
const full: Ratio = 1.0
let current = try Ratio(input)
```

Refined-to-base é implícito. Base-to-refined é fallible. O refinement não muda o
layout canônico em structs, ABI, FFI, persistência ou borrows. O optimizer pode
estreitar register, SIMD e storage interno não escapante e depois reestender.

Uma expressão pode inicializar um refinement sem `try` quando os fatos estáticos
provam o predicate. Isso não é uma conversão runtime:

```w
type Small = Int<(1...128)>
type Pair = Int<(2...256)>

fn pair(left: Small, right: Small): Pair {
  return left + right
}
```

Se a prova é `unknown`, o programa usa o constructor fallible. Uma annotation
ou build profile não substitui a prova.

A [Ada Reference Manual](https://docs.adacore.com/live/wave/arm22/pdf/arm22/arm-22.pdf)
também separa um subtype restringido por range do base type. O trabalho sobre
[Liquid Types](https://escholarship.org/uc/item/0vx7j8zc) trata refinements como
predicates verificáveis. W usa esses precedentes sem adotar seu source syntax.

As formas `T where (predicate)`, `T<where: (...)>` e `T(where: predicate)`
continuam como **Alternativa**. A forma vigente mantém o predicate dentro do
contrato estático sem criar um slot chamado `where`.

#### 8.6.1 Subconjuntos de cases de enum

Todo enum declara um slot estático implícito chamado `cases`. A forma posicional
é canônica:

```w
enum ServiceStage {
  accepted
  reserving
  preparing
  serving
  completed
  cancelled
}

alias WorkStage =
  ServiceStage<[.reserving, .preparing, .serving]>
```

`WorkStage` aceita somente os três cases listados. `alias` preserva a identidade
do refinement. Um `type` criaria outra identidade nominal.

O head define o significado do argumento estático. A mesma forma visual não
torna todos os contratos equivalentes:

```w
// `StagePath` declara uma StaticList. A ordem e a repetição são significativas.
let path: StagePath<[.accepted, .reserving, .preparing, .serving, .completed]>

// `ServiceStage` declara um case-set. A ordem não é significativa.
let stage: ServiceStage<[.preparing, .serving]> = .preparing
```

As listas `[.accepted, .reserving]` e `[.reserving, .accepted]` são valores
estáticos diferentes. `StagePath` também rejeita a segunda ordem pelo predicate.
`ServiceStage<[.preparing, .serving]>` e
`ServiceStage<[.serving, .preparing]>` são o mesmo tipo.

O subset preserva members e conformances do enum base. Por exemplo,
`ServiceFault<[.delayed]>` continua atendendo a `Error`. O subset restringe
valores possíveis. Ele não declara uma segunda conformance.

O payload `[...]` usa syntax de static list, mas o schema do enum interpreta os
itens como um conjunto de cases. O compiler:

1. rejeita duplicatas e cases de outro enum;
2. ordena o conjunto pela declaração do enum;
3. grava o conjunto normalizado na identidade do tipo;
4. apaga o contrato antes do runtime.

Estas formas identificam o mesmo tipo:

```w
alias A = ServiceStage<[.preparing, .serving]>
alias B = ServiceStage<[.serving, .preparing]>
```

O formatter preserva a ordem source durante a edição. `w interface` e o hash de
tipo usam a ordem canônica. Um conjunto com todos os cases normaliza para o enum
base. Um conjunto vazio é erro no source. Use `Never` para ausência de valores.

Uma função pode publicar um retorno mais preciso:

```w
fn nextWorkStage(inventoryReady: Bool): WorkStage {
  return switch inventoryReady {
    case true: .preparing
    case false: .reserving
  }
}
```

O contrato também pode descrever uma única transição:

```w
alias StageAfterAccepted =
  ServiceStage<[.reserving, .cancelled]>

fn routeAcceptedOrder(canReserve: Bool): StageAfterAccepted {
  return switch canReserve {
    case true: .reserving
    case false: .cancelled
  }
}
```

O código chamador não precisa defender contra `.accepted`, `.preparing`, `.serving` ou
`.completed`. Se o retorno ganhar `.preparing`, cada `switch` explícito sobre o
resultado precisa decidir como tratar esse case.

Cada `return` precisa pertencer ao conjunto. Este retorno falha:

```w
fn invalidStage(): WorkStage {
  return .cancelled
  // error[W-TYPE-ENUM-0001]: cancelled is outside WorkStage
}
```

Um `switch` usa o conjunto estático do subject:

```w
fn instruction(stage: WorkStage): String {
  return switch stage {
    case .reserving: "Reserve ingredients"
    case .preparing: "Prepare the course"
    case .serving: "Serve the guest"
  }
}
```

O switch acima é exaustivo. Um case `.cancelled` seria inalcançável. Se a API
adicionar `.cancelled` a `WorkStage`, o switch deixa de ser exaustivo.

Um wildcard ou pattern amplo continua cobrindo cases futuros do conjunto:

```w
fn isCooking(stage: WorkStage): Bool {
  return switch stage {
    case .preparing: true
    case _: false
  }
}
```

Nesse exemplo, `_` trata explicitamente qualquer ampliação. O compiler não exige
um novo braço.

O subset também restringe argumentos. Uma função pode aceitar somente estados
nos quais uma operação faz sentido:

```w
alias CancellableStage =
  ServiceStage<[.accepted, .reserving, .preparing]>

fn requestCancellation(stage: CancellableStage): CancelRequest {
  return CancelRequest(stage: stage)
}

requestCancellation(.preparing)
requestCancellation(.completed) // error: completed is outside CancellableStage
```

Ampliar o subset de um parâmetro é compatível com o código chamador existente.
Reduzi-lo é uma mudança major.

Cases com payload mantêm seu payload:

```w
enum KitchenOutcome<T> {
  ready(T)
  delayed(Duration)
  cancelled(CancelReason)
}

alias ContinuingOutcome<T> =
  KitchenOutcome<T><[.ready, .delayed]>

fn describe<T: Display>(outcome: ContinuingOutcome<T>): String {
  return switch outcome {
    case .ready(let value): value.display()
    case .delayed(let duration): "Delay: ${duration}"
  }
}
```

O primeiro envelope aplica `T`. O segundo restringe os cases. Essa regra também
se aplica a `Result<T, E><[.success]>`, embora uma API nonthrowing deva retornar
`T` diretamente.

Um enum com payload substitui estados paralelos que poderiam divergir:

```w
enum OvenReading {
  stable(Temperature)
  warming(current: Temperature, target: Temperature)
  failed(OvenFault)
}

alias UsableReading = OvenReading<[.stable, .warming]>

fn requestedHeat(reading: UsableReading): Temperature {
  return switch reading {
    case .stable(let temperature): temperature
    case .warming(_, let target): target
  }
}
```

Essa forma é preferível a `isFailed: Bool`, `temperature: Temperature?` e
`fault: OvenFault?`. Cada case constrói somente o payload que ele exige.

Um subset com um case preserva o payload sem criar um wrapper:

```w
alias ReadyOutcome<T> = KitchenOutcome<T><[.ready]>

fn preparedDish(outcome: ReadyOutcome<Dish>): Dish {
  return switch outcome {
    case .ready(let dish): dish
  }
}
```

O switch possui um braço porque `.ready` é o único valor possível. O tipo não
faz unwrap implícito. O pattern continua visível no source.

Um subset converte de forma implícita para o enum base ou para um superset:

```w
alias ActiveStage =
  ServiceStage<[.accepted, .reserving, .preparing, .serving]>

let work: WorkStage = .preparing
let active: ActiveStage = work
let base: ServiceStage = active
```

A conversão inversa exige validação:

```w
let work = try WorkStage(base)
let optionalWork = try? WorkStage(base)
```

Uma conversão válida preserva o valor e o payload. Uma falha retorna o mesmo
error estruturado usado por outro refinement fechado.

Flow analysis mantém um case-set fact. Um pattern reduz esse conjunto:

```w
fn prepare(stage: ServiceStage): String? {
  guard stage in (.reserving, .preparing, .serving) else return .none
  return instruction(stage)
}
```

Depois do guard, `stage` possui o fact de `WorkStage`. A HIR pode chamar
`instruction` sem check runtime. O binding source continua `ServiceStage`.

Cada braço de `switch` também recebe um fact mais preciso:

```w
fn prepareInstruction(stage: ServiceStage<[.preparing]>): String

fn continueService(stage: ServiceStage): String {
  return switch stage {
    case .accepted: "Reserve a timeline"
    case .reserving: "Reserve ingredients"
    case .preparing: prepareInstruction(stage)
    case .serving: "Serve the guest"
    case .completed: "Archive the order"
    case .cancelled: "Release the ingredients"
  }
}
```

No braço `.preparing`, `stage` possui o case-set
`ServiceStage<[.preparing]>`. O compiler pode passá-lo para
`prepareInstruction` sem conversão checked.

Um case curto exige um expected enum type:

```w
let stage: WorkStage = .preparing
let base = ServiceStage.preparing
let ambiguous = .preparing // error: no expected enum type
```

`EnumName.case` resolve uma colisão. `.case` nunca escolhe um enum por ordem de
imports ou por frequência.

Membership é a forma curta para testar mais de um case:

```w
if stage in (.preparing, .serving) {
  recordKitchenActivity()
}
```

W não adiciona `stage.is(.preparing, .serving)`. Para um enum comum, o valor
possui um case por vez. Um teste AND entre cases distintos seria sempre falso.
Um conjunto simultâneo usa `Set<Access>` ou um futuro tipo compacto de flags,
em vez de mudar a semântica de enum:

```w
enum Access {
  read
  write
  admin
}

let access = Set([Access.read, .write])
expect access.contains(.read)
expect !access.contains(.admin)
```

Uma variável declarada como subset nunca aceita outro case:

```w
var stage: WorkStage = .reserving
stage = .serving
stage = .cancelled // error: case is outside WorkStage
```

O subset usa o layout lógico do enum base. Ele não cria wrapper, nova tag ou
vtable. O optimizer pode eliminar tags em SSA ou storage interno quando o
resultado não for observável. Struct, ABI, FFI e persistência usam o layout
canônico do enum base.

Um schema exportado registra o conjunto permitido. Um decoder valida o case
antes de produzir o subset:

```w
fn decodeStage(source: ref Bytes): WorkStage throws DecodeError {
  let base = try ServiceStage.decode(source)
  return try WorkStage(base)
}
```

O mesmo contrato vale em effect position. Uma função pode publicar somente as
falhas que ela produz:

```w
enum ServiceFault: Error {
  ingredientsMissing(String)
  delayed(Duration)
  universeEnded
}

alias RecoverableServiceFault =
  ServiceFault<[.ingredientsMissing, .delayed]>

fn reserveCourse(): WorkStage throws RecoverableServiceFault
```

`throw .universeEnded` é erro dentro de `reserveCourse`. Em um contexto
nonthrowing, `catch` precisa tratar somente `.ingredientsMissing` e `.delayed`.
Adicionar `.universeEnded` ao effect publicado torna um catch explícito
nonexhaustive.

`w interface diff` considera a posição do subset:

| Mudança | Compatibilidade source |
|---|---|
| adicionar case a um retorno | major; callers podem perder exhaustividade |
| remover case de um retorno | minor; o valor fica mais preciso |
| adicionar case aceito por parâmetro | minor |
| remover case aceito por parâmetro | major |
| adicionar case a `throws` | major; callers precisam aceitar outra falha |
| remover case de `throws` | minor |
| alterar subset de field exportado | major |

Function types continuam invariantes. A tabela classifica evolução de
declarações, não variance automática.

A baseline não permite subsets de payloads. Use um enum menor, newtype ou
refinement do payload:

```w
alias SmallDelay = Duration<(0...30<s>)>
```

A baseline também não possui álgebra de case-set no source. Escreva e nomeie o
conjunto resultante:

```w
alias KitchenStage =
  ServiceStage<[.reserving, .preparing, .serving]>
```

Formas como `ActiveStage - [.accepted]` e
`CancellableStage & WorkStage` permanecem **Alternativa**. A HIR já usa união,
interseção e diferença de case-sets para flow analysis, mas a API pública não
precisa expor essa álgebra no design vigente.

O
[TypeScript Handbook](https://www.typescriptlang.org/docs/handbook/2/narrowing.html)
demonstra narrowing e exhaustividade sobre discriminated unions. A
[documentação de enums do Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/enumerations/)
exige switch exaustivo. W combina essas propriedades com um case-set explícito
na interface.

#### 8.6.2 Quando usar enum

Use enum quando o domínio contém uma escolha fechada. Cada case pode exigir um
payload diferente:

```w
enum PackageVerification {
  unverified
  reproducible(ArtifactDigest)
  audited(artifact: ArtifactDigest, report: SecurityReportId, grade: SecurityGrade)
  rejected(VerificationFault)
}
```

Esse valor não permite um pacote simultaneamente `unverified` e `audited`. O
payload de auditoria também não existe no case `unverified`.

Uma fronteira pode publicar um subset:

```w
alias TrustedVerification =
  PackageVerification<[.reproducible, .audited]>

fn selectForProduction(status: TrustedVerification): ArtifactDigest {
  return switch status {
    case .reproducible(let digest): digest
    case .audited(let digest, _, _): digest
  }
}
```

O exemplo acima também mostra um limite: dados necessários em mais de um case
devem estar em cada payload relevante ou em um member total.

Use outro contrato quando a estrutura do problema for diferente:

| Necessidade | Contrato | Exemplo |
|---|---|---|
| todos os fields coexistem | `struct` | `Receipt { orderId, total, traceId }` |
| implementações externas podem crescer | `protocol` | `DishFactory` |
| várias opções podem estar ativas | `Set<T>` ou flags | `Set([.read, .write])` |
| um scalar possui limites | refinement | `GuestCount = u16<(1...4096)>` |
| a ordem compile-time importa | `StaticList<T>` | `StagePath<[.accepted, .cancelled]>` |
| uma alternativa fechada está ativa | `enum` | `OvenReading.stable(180)` |
| uma API conhece menos alternativas | enum subset | `OvenReading<[.stable, .warming]>` |

Adicionar um case a um enum exportado pode invalidar switches exaustivos. Um
domínio que precisa aceitar variantes de terceiros deve usar protocol ou um
envelope versionado:

```w
protocol PaymentProvider {
  async fn authorize(request: PaymentRequest): PaymentAuthorization
}
```

Não use um case `.custom(any PaymentProvider)` para fingir que um enum fechado
é uma extensão aberta, a menos que o fechamento externo seja uma decisão
explícita do protocol de wire.

#### 8.6.3 Estado runtime, typestate e transições

W separa dois fatos:

| Fato | Representação | Exemplo |
|---|---|---|
| estado conhecido em runtime | enum em storage | `OrderState.stage: ServiceStage` |
| estado provado pelo código chamador | argumento `const` de enum | `OvenSession<.ready>` |

**Forma vigente:** use estado runtime para valores persistidos, compartilhados ou
escolhidos por entrada. Use typestate para um owner local cuja transição consome o
estado anterior.

Esses fatos não convertem de forma automática. Um enum runtime pode exigir uma
validação. Um typestate não cria um field runtime.

##### Transições e paths estáticos

Uma função `const` pode definir a mesma regra para compile time e runtime:

```w
export const fn canMove(from current: ServiceStage, to next: ServiceStage): Bool {
  return switch current {
    case .accepted: next in (.reserving, .cancelled)
    case .reserving: next in (.preparing, .cancelled)
    case .preparing: next in (.serving, .cancelled)
    case .serving: next in (.completed, .cancelled)
    case .completed: false
    case .cancelled: false
  }
}
```

Um refinement do parâmetro `const` valida uma sequência:

```w
export const fn isValidStagePath(stages: StaticList<ServiceStage>): Bool {
  guard stages.count > 0 else return false

  for index in 1..<stages.count {
    if !canMove(from: stages[index - 1], to: stages[index]) {
      return false
    }
  }

  return true
}

export struct StagePath<
  const _ stages: StaticList<ServiceStage><(isValidStagePath(.member))>,
> {
  orderId: OrderId
}
```

O tipo abaixo é válido:

```w
let standard: StagePath<
  [.accepted, .reserving, .preparing, .serving, .completed]
>
```

O tipo abaixo falha na transição `.accepted → .completed`:

```w
let skipped: StagePath<[.accepted, .completed]>
// error[W-CONST-STATE-0001]: invalid transition at stages[0] -> stages[1]
```

`StagePath` prova a sequência declarada. Ele não prova que uma service instance
executou essa sequência.

##### Typestate com enum e ownership

Um argumento `const` de enum pode identificar o estado estático:

```w
export type OvenId = u64

export enum OvenFault: Error {
  sensorUnavailable
}

export enum OvenSessionState {
  idle
  ready
  faulted
  closed
}

export struct OvenSession<const _ state: OvenSessionState> {
  id: OvenId

  init(id: OvenId) {
    self.id = id
  }
}

export enum ActivationOutcome {
  ready(OvenSession<.ready>)
  faulted(OvenSession<.faulted>, OvenFault)
}

export fn openOven(id: OvenId): OvenSession<.idle> {
  return OvenSession<.idle>(id: id)
}
```

O initializer sem `export` impede a construção externa de um estado arbitrário.
Uma extension publica somente as operações válidas para uma specialization:

```w
extension OvenSession<.idle> {
  export take fn activate(sensorWorks: Bool): ActivationOutcome {
    if sensorWorks {
      return .ready(OvenSession<.ready>(id: id))
    }

    return .faulted(
      OvenSession<.faulted>(id: id),
      .sensorUnavailable,
    )
  }
}

extension OvenSession<.ready> {
  export take fn close(): OvenSession<.closed> {
    return OvenSession<.closed>(id: id)
  }
}
```

`activate` consome `OvenSession<.idle>`. O binding antigo fica movido. Cada case
de `ActivationOutcome` devolve o novo owner e preserva o estado conhecido:

```w
let idle = openOven(id)
let outcome = (take idle).activate(sensorWorks: probe.isHealthy)

switch outcome {
  case .ready(let oven):
    bakeCake(using: take oven)
  case .faulted(let oven, let fault):
    report(fault)
    quarantine(take oven)
}
```

O compiler rejeita estas operações:

```w
(take idle).activate(sensorWorks: true)
// error: idle was already moved

let stillIdle = openOven(id)
(take stillIdle).close()
// error: close is not a member of OvenSession<.idle>
```

Uma transição não muda o tipo de um binding no lugar:

```w
var oven = openOven(id)
oven = OvenSession<.ready>(id: id)
// error: OvenSession<.idle> and OvenSession<.ready> are distinct types
```

Essa regra mantém o type checker local. Ela também impede que um alias observe
uma mudança de tipo.

Um borrow precisa terminar antes da transição consuming:

```w
let ref identifier = idle.id
let outcome = (take idle).activate(sensorWorks: true)
print(identifier)
// error: borrow of idle remains live across a consuming transition
```

Uma transição `throws` não restaura o owner de forma implícita. Se o código chamador
precisa recuperar um owner, a função retorna um enum como `ActivationOutcome`.
Cada saída transfere ou destrói o owner uma vez.

O argumento `state` participa da identidade de tipo e de `TypeId`. Ele não exige
tag, byte ou pointer adicional no runtime. O backend pode compartilhar machine
code entre specializations quando o estado não altera o body.

Typestate não é o default para collections heterogêneas:

```w
let ovens: Array<OvenSession<?>> = mixed
// error: W does not erase a const state with `?`
```

Use um enum runtime ou um envelope fechado quando vários estados precisam ocupar
a mesma collection:

```w
enum AnyOvenSession {
  idle(OvenSession<.idle>)
  ready(OvenSession<.ready>)
  faulted(OvenSession<.faulted>)
  closed(OvenSession<.closed>)
}
```

##### Services, snapshots e concorrência

Uma `ServiceRef` pode ter aliases. A instância também pode mudar entre duas
calls. Portanto, uma service não muda de protocol ou typestate depois de uma
call.

Uma API publica um snapshot quando o código chamador precisa observar o estado:

```w
struct StageSnapshot {
  stage: ServiceStage
  revision: u64
}

enum MoveOrderResult {
  applied(StageSnapshot)
  stale(current: StageSnapshot)
  rejected(from: ServiceStage, to: ServiceStage)
}

protocol RestaurantApi {
  async fn status(orderId: OrderId): StageSnapshot

  async fn move(
    orderId: OrderId,
    to next: ServiceStage,
    expectedRevision: u64,
  ): MoveOrderResult
}
```

O closed turn torna cada call serial na instância. O snapshot pode ficar antigo
depois do retorno. `expectedRevision` fornece uma precondição verificável para
a próxima transição.

Um enum subset ainda melhora cada resultado:

```w
alias AppliedOrStale =
  MoveOrderResult<[.applied, .stale]>
```

Ele não transforma o snapshot em authority sobre a instância.

##### Limites e alternativas

W não adiciona keywords `state` ou `transition` no design vigente. Const generics,
extensions, `take fn`, enums e refinements já expressam o protocolo.

Uma `StateGraph<E>` declarativa permanece **Pesquisa**:

```w
const serviceFlow = StateGraph<ServiceStage>([
  (.accepted, .reserving),
  (.accepted, .cancelled),
  (.reserving, .preparing),
  (.reserving, .cancelled),
])
```

Esse valor poderia gerar diagramas e analisar estados inalcançáveis. O protótipo
precisa provar que ele reduz duplicação sem limitar guards dinâmicos.

Typestate em `shared`, `service` ou outro owner com aliases fica **Rejeitado por
enquanto**. A transição exigiria invalidar aliases ou executar checks runtime.
Use enum em storage e uma fronteira serial.

O
[Embedded Rust Book](https://docs.rust-embedded.org/book/static-guarantees/typestate-programming.html)
mostra transições que consomem um estado e produzem outro. A seção sobre
[zero-cost abstractions](https://docs.rust-embedded.org/book/static-guarantees/zero-cost-abstractions.html)
mostra que markers de estado não precisam existir no runtime. O trabalho
[Typestates for Objects](https://www.cs.cmu.edu/~aldrich/courses/819/deline-typestates.pdf)
mostra o valor de preconditions por estado e o custo de aliases. As
[regras de Durable Objects](https://developers.cloudflare.com/durable-objects/best-practices/rules-of-durable-objects/)
mostram por que coordenação compartilhada exige estado runtime e serialização.

### 8.7 Generics

#### 8.7.1 Kinds, parâmetros e aplicação

W possui dois kinds de parâmetro generic:

| Kind | Declaração | Exemplo de argumento |
|---|---|---|
| tipo | `T` ou `T: P` | `String` |
| valor | `const count: usize` | `count: 64` |

Parâmetros são declarados antes do uso:

```w
fn get<T, const count: usize>(values: ref [T; count]): T
```

Um parâmetro de tipo é posicional. Um parâmetro `const` usa seu nome como label:

```w
struct Matrix<Element, const rows: usize, const columns: usize> { ... }

let weights: Matrix<f32, rows: 3, columns: 4>
```

`_` declara um único slot `const` posicional. Essa forma serve a um contrato
primário cujo significado já está no nome do head:

```w
struct StagePath<
  const _ stages: StaticList<ServiceStage><(isValidStagePath(.member))>,
> {
  orderId: OrderId
}

let path: StagePath<[.accepted, .reserving, .preparing, .serving, .completed]>
```

Um head pode misturar slots posicionais de tipo e slots `const` nomeados:

```w
struct Tensor<Element, const shape: StaticList<usize>> { ... }

let scores: Tensor<f32, shape: [8, 4]>
```

Cada argumento possui o kind declarado pelo head resolvido. Um modifier map
aberto não existe. Um label desconhecido, duplicado ou fora de ordem é erro.

Named type arguments permanecem **Alternativa**:

```w
Result<Success: Dish, Failure: KitchenError>
```

O design vigente usa `Result<Dish, KitchenError>`. Inlay hints e documentação mostram os
nomes `Success` e `Failure` sem duplicar tokens no source.

Um parâmetro entra em scope depois de sua declaração. Uma constraint pode usar
somente parâmetros anteriores:

```w
fn zip<Item, Left: Sequence<Item>, Right: Sequence<Item>>(
  left: ref Left,
  right: ref Right,
): Array<(Item, Item)>
```

Esta ordem é inválida:

```w
fn invalid<Left: Sequence<Item>, Item>(left: ref Left)
// error: Item is not declared at this point
```

A regra evita ciclos e torna a assinatura legível da esquerda para a direita.
Um parâmetro não pode ser redeclarado ou sombreado dentro da mesma declaração.

W não possui lifetime parameters no source. Borrow relations são inferidas,
verificadas na HIR e gravadas na interface.

W também não possui estes kinds no design vigente:

- type constructors de ordem superior, como `F<_>`;
- effects genéricos separados de tipos;
- parameter packs;
- parâmetros de layout ou allocator implícitos.

Essas formas permanecem **Pesquisa**. Protocols e associated types cobrem a
baseline.

#### 8.7.2 Constraints e composição

`T: P` exige uma conformance nominal. `&` combina requirements sem ordenar
protocols:

```w
fn label<T: Display & Equatable>(value: ref T): String {
  return value.display()
}
```

O type checker normaliza `Display & Equatable` por identidade de protocol.
Duplicatas são removidas. `&` em type position não executa bitwise AND.

Protocol inheritance usa a mesma composição:

```w
protocol StableKey: Hashable & Display {
  fn stableBytes(): Bytes
}
```

Uma declaração pode usar um protocol composto nomeado quando a combinação tem
significado de domínio. `T<[P, Q]>` e uma cláusula postfix `where` permanecem
**Alternativa**. A lista sugere ordem. `where` separa a constraint do parâmetro.

Same-type relationships usam um parâmetro comum:

```w
fn sameItems<Item: Equatable, Left: Sequence<Item>, Right: Sequence<Item>>(
  left: ref Left,
  right: ref Right,
): Bool {
  // ...
}
```

`Left` e `Right` possuem o mesmo `Item`. A assinatura não precisa de
`where Left.Item == Right.Item`.

Um `const` parameter pode usar refinement e parâmetros anteriores:

```w
struct Tile<
  const rows: usize<(1...4096)>,
  const columns: usize<(1...4096)>,
> {
  pixels: [Pixel; rows * columns]
}
```

O evaluator verifica a constraint quando instancia o head. A interface guarda o
predicate normalizado.

Generic defaults não entram no design vigente. Um alias nomeado oferece um default sem
mudar inference:

```w
alias Page<T> = Buffer<T, count: 4096>
```

#### 8.7.3 Primary associated types

Um protocol head declara primary associated types. Esses nomes pertencem a
`Self`. Eles não são parâmetros escolhidos por cada call:

```w
protocol Iterator<Item> {
  mut fn next(): Item?
}

protocol Sequence<Item> {
  fn iterator(): some Iterator<Item>
}
```

`Item` é shorthand de um requirement `type Item`. O body não repete
`type Item`.

Uma conformance fornece o witness explicitamente:

```w
struct Menu {
  dishes: Array<Dish>
}

extension Menu: Sequence {
  alias Item = Dish

  fn iterator(): some Iterator<Dish> {
    return dishes.iterator()
  }
}
```

`Sequence<Dish>` restringe o associated type primário:

```w
fn first<S: Sequence<Dish>>(source: ref S): Dish?
fn erase(source: take some Sequence<Dish>): any Sequence<Dish>
```

A aplicação não cria outra família de conformances. `Menu` possui uma única
conformance a `Sequence`, e essa conformance fixa `Item = Dish`.

Um protocol pode declarar mais de um primary associated type:

```w
protocol Mapping<Key, Value> {
  fn get(key: ref Key): ref Value?
}
```

A ordem pertence à interface do protocol. Alterá-la é major. Associated types
não primários continuam no body:

```w
protocol Parser<Input> {
  type State
  fn parse(input: ref Input): State
}
```

O conformer também declara `alias State = ...`. W não infere associated type
witnesses a partir de methods. A declaração explícita melhora diagnostics e
mantém a interface estável.

Generic associated types, defaults de associated type e constraints sobre uma
projection não primária permanecem **Pesquisa**. Um associated type que callers
precisam restringir deve ser primário no design vigente.

#### 8.7.4 Verificação e inference

O compiler verifica um body generic uma vez contra sua assinatura:

```w
fn contains<T: Equatable>(values: ref Array<T>, target: ref T): Bool {
  for ref value in values {
    if value == target { return true }
  }
  return false
}
```

Lookup usa somente os requirements de `Equatable` e os members conhecidos de
`Array<T>`. Uma conformance adicionada depois não muda o significado do body.
W não possui argument-dependent lookup ou lookup tardio de template.

Uma call generic segue estas etapas:

1. resolve owner e overload pela forma de call;
2. aplica argumentos generic explícitos;
3. cria equações com receiver e argumentos runtime;
4. usa o expected result type para parâmetros ainda abertos;
5. normaliza associated types pelos witnesses conhecidos;
6. exige uma solução única;
7. verifica constraints e ownership.

O compiler não retorna ao overload set quando inference falha.

Uma call comum omite todos os argumentos inferíveis:

```w
let found = contains(values, target: needle)
```

Um argumento explícito fixa uma parte da solução:

```w
let order = try request.json.decode<Order>()
let forecast = try forecast<tables: 2, courses: 4>(
  observations,
  weights: weights,
)
```

Type arguments explícitos formam um prefixo posicional. Labels `const` podem
omitir parâmetros inferíveis entre eles. `_` não é placeholder generic:

```w
decode<_>() // error: omit the argument or name a concrete type
```

Inference usa tipos, valores `const` e expected type. Ela não procura “algum
tipo que atende a P”. Constraints validam uma solução. Elas não inventam uma.

```w
fn make<T: Default>(): T

let value = make()
// error[W-TYPE-GENERIC-0002]: T has no argument or expected type
```

Literals permanecem exatos até a solução usar typed operands. Se somente
literals determinam um tipo numérico, a policy numérica escolhe o default.

Um generic function usado como valor precisa ficar totalmente instanciado:

```w
let compare: fn(ref Dish, ref Dish): Bool = equal
let unresolved = equal // error: generic function is not fully instantiated
```

Polymorphic function values e inference pelo body ficam fora do design vigente.

#### 8.7.5 Coherence e conformances condicionais

Conformance é nominal. Ela pode ser declarada no módulo do tipo ou no módulo do
protocol. A interface registra uma escolha global para cada par
`(type constructor, protocol)`.

Uma extension generic declara seus parâmetros antes do tipo:

```w
extension<T: Equatable> Array<T>: Equatable {
  fn equals(other: ref Self): Bool {
    return elementsEqual(self, other)
  }
}
```

Essa conformance existe quando `T: Equatable`. Cada parâmetro da extension deve
aparecer no tipo estendido. Uma extension de um type parameter nu é erro:

```w
extension<T: Display> T: Loggable { ... }
// error: a conformance extension needs a nominal type head
```

Duas conformances não podem se sobrepor para uma mesma instantiation. W não
possui:

- specialization de conformance;
- negative conformance;
- prioridade por módulo ou import;
- conformance escolhida pelo call site.

O compiler compara conditional heads por unification. Uma interseção possível
produz erro no módulo que declara a segunda conformance.

Um witness deve corresponder a nome, forma de call, receiver, ownership,
effects, tipo e generic signature do requirement. W não usa variance ou
conversão implícita para escolher um witness.

```w
protocol Store<Value> {
  fn get(key: ref String): Value throws StoreError
}

struct MenuStore: Store {
  alias Value = Dish
  fn get(key: ref String): Dish throws StoreError { ... }
}
```

Protocol methods podem ter implementação default. Somente o módulo do protocol
pode publicar um default witness:

```w
protocol Counted {
  fn count(): usize
  fn isEmpty(): Bool { return self.count() == 0 }
}
```

A conformance registra se usa o default ou um witness próprio. Imports
posteriores não mudam essa seleção. Uma extension externa pode adicionar methods
comuns, mas não pode criar um default witness oculto.

#### 8.7.6 `some`, `any` e composição

`some P` preserva um tipo concreto. Cada ocorrência em parâmetro cria um
parâmetro generic anônimo:

```w
fn compare(left: some Equatable, right: some Equatable)
```

`left` e `right` podem ter tipos diferentes. Se a relação exigir o mesmo tipo,
use um parâmetro nomeado:

```w
fn compare<T: Equatable>(left: ref T, right: ref T): Bool
```

Um retorno `some P` possui uma identidade opaca ligada à declaração e aos seus
argumentos generic. Todos os returns de uma instantiation usam o mesmo tipo:

```w
fn policy<T>(config: ref T): some PricingPolicy {
  if config.isTrial { return TrialPricing() }
  return StandardPricing() // error: two underlying opaque types
}
```

`some P & Q` exige as duas conformances sem apagar a identidade.

`any P` apaga a identidade concreta e usa dispatch por witness. Um protocol é
existential-compatible quando cada member chamado pelo valor:

- não possui parâmetros generic;
- não usa `Self` fora do receiver;
- não usa associated type sem binding;
- possui receiver e effects representáveis na witness table.

```w
protocol Factory {
  static fn make(): Self
}

let factory: any Factory
// error: make uses Self and has no value receiver
```

Associated consts e static functions continuam disponíveis por tipo concreto ou
generic. Eles não são chamados por um existential value.

Primary associated bindings tornam um existential utilizável:

```w
let dishes: any Sequence<Dish> = menu
```

`any P & Q` guarda witnesses para os dois protocols. A ordem source não altera o
tipo. Um owned existential pode usar inline storage ou box. `ref any P` é uma
fat borrow sem alocação.

`any P` não conforma automaticamente a `P`. O design vigente também não abre existentials
de forma implícita para uma função generic:

```w
fn inspect<T: PricingPolicy>(policy: ref T)
let erased: any PricingPolicy = StandardPricing()

inspect(erased) // error: existential is not a generic witness
```

A API aceita `ref any PricingPolicy` quando deseja dispatch dinâmico. Explicit
existential opening permanece **Pesquisa**. Essa decisão evita fresh types
ocultos e regras dependentes da posição do result.

#### 8.7.7 HIR, interface e lowering

HIR generic contém:

- parâmetros e kinds em ordem;
- constraints normalizadas;
- associated type projections;
- conformance IDs e witness selections;
- relações de borrow e effects;
- body tipado independente de instantiation.

Uma interface exportada inclui a assinatura, o digest do body generic e um blob
de HIR genérica no content-addressed storage (CAS). Um importer não reparseia o
source:

```text
generic signature + witness requirements + HIR digest + HIR body
```

O lowering possui liberdade de representação:

| Estratégia | Uso |
|---|---|
| monomorphization | layout, const arguments, inlining e hot paths |
| shared generic body | code size e separate compilation |
| direct call | witness conhecido e specialization |
| witness parameter | shared body ou existential |

Essas escolhas não mudam dispatch, error, ownership ou ordem de avaliação. Uma
profile pode mudar a estratégia e manter a mesma HIR verificada.

O compiler não expõe `@specialize` ou outra annotation. Tooling mostra a
decisão:

```text
w explain generic restaurant.forecast
  3 specialized instances
  1 shared body
  18.4 KiB estimated text
```

Um conditional witness usa uma witness factory. Por exemplo, a conformance
`Array<T>: Equatable` recebe o witness de `T: Equatable` e produz o witness de
Array.

#### 8.7.8 Instantiation, termination e cache

Uma instantiation é identificada por:

- declaration digest;
- type e `const` arguments normalizados;
- conformance e witness IDs;
- target, profile e edition;
- compiler e bundle versions.

Recursive calls com a mesma instantiation são normais:

```w
fn count<T>(node: ref Tree<T>): usize {
  return 1 + node.children.map(count<T>).sum()
}
```

Polymorphic recursion não é inferida. Uma call que muda a própria instantiation
precisa de argumentos explícitos e de um grafo finito. O compiler detecta uma
expansão sem limite:

```text
error[W-TYPE-GENERIC-0005]: instantiation does not converge
  expand<u8>
  expand<Array<u8>>
  expand<Array<Array<u8>>>
```

A recipe fixa limites determinísticos:

```w
build: {
  generics: {
    instances: 100_000
    depth: 128
  }
}
```

Os limites entram na chave de cache. Um budget de code size nunca muda a
semântica. Quando possível, o compiler escolhe um shared body em vez de falhar.
Uma expansão de tipos que não converge continua erro.

#### 8.7.9 Variance e fechamento W0

Generic types são invariantes por default:

```w
let work: Array<WorkStage> = [...]
let all: Array<ServiceStage> = work // error: Array is invariant
```

Use `map` para converter elementos. W não possui declaration-site variance na
design vigente. Borrow e function conversions seguem suas regras próprias.

`bootstrap.w0` inclui:

- type parameters;
- constraints de um protocol nominal ou composição;
- primary associated types;
- conformances diretas e condicionais sem overlap;
- monomorphization;
- inference por receiver, argumentos e expected result.

W0 não inclui `some`, `any`, shared generic bodies, generic associated types ou
polymorphic recursion. O compiler completo pode reconhecer essas formas sem
usá-las no próprio source.

O
[Rust Compiler Development Guide](https://rustc-dev-guide.rust-lang.org/backend/monomorph.html)
mostra os custos de compile time e tamanho causados por monomorphization. A
[especificação de Go](https://go.dev/ref/spec#Type_inference) mostra inference
por equações de tipos. O
[Swift Book](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/opaquetypes/)
separa generic, opaque e boxed protocol types. W usa esses precedentes com
lookup fechado e witnesses determinísticos.

### 8.8 Conversões

**Exemplo:** `u8` pode converter para `u16`. A conversão de `u16` para `u8`
exige uma operação checked explícita: `try u8(exactly: value)`.

Uma conversão implícita é permitida somente se:

1. é total para todos os valores do tipo de origem;
2. preserva o valor;
3. existe uma única rota canônica;
4. não muda ownership ou authority de forma oculta.

Narrowing, parsing, rounding, reinterpretation, ponteiro e conversão ambígua são
explícitos. Refinements podem provar que uma conversão adicional preserva valor.
O mesmo princípio permite `T` para `any P` quando `T: P`.

O checker não procura um terceiro tipo numérico comum. Ele aceita a identidade
ou uma única conversão segura de um operando para o tipo do outro. Assim,
`u8 + i16` produz `i16`, mas `i8 + u8` exige que o source escolha um tipo.

```w
let safe: i16 = 120_u8 + 8_i16
let chosen: i16 = i16(-1_i8) + i16(2_u8)
let narrow = try u8(exactly: chosen)
```

A seção 15.1.2 fecha as conversões entre integers e floats.

### 8.9 Reflection, síntese e parâmetros rest

#### 8.9.1 Dois planos de introspecção

W separa introspecção de tooling e reflection runtime.

Tooling lê interfaces e HIR versionadas. Esse plano contém nomes, tipos,
visibilidade, source spans, efeitos, ownership e documentação. Ele não exige
metadata no executável.

Runtime recebe somente metadata solicitada pelo programa. Uma conformance a
`reflect.Reflectable` cria essa solicitação. Debug symbols e source maps
continuam em sidecars removíveis.

```w
import std.reflect as reflect

export struct MenuCard: Hashable & reflect.Reflectable {
  title: String
  course: Course
}
```

A conformance fica na interface mesmo quando o linker remove o descriptor
runtime. O witness alcançável mantém o descriptor necessário.

Essa separação atende ferramentas e assistentes sem forçar reflection em todo
programa. Ela também impede que debug metadata vire uma API.

#### 8.9.2 `TypeId` sem metatype universal

`reflect.TypeId` identifica um tipo dentro de um build:

```w
let menuCardId = reflect.TypeId.of<MenuCard>()
let anotherId = reflect.TypeId.of<MenuCard>()
expect menuCardId == anotherId
```

A identidade inclui o tipo nominal e argumentos normalizados. Ela também inclui
refinements e subsets de enum:

```w
expect reflect.TypeId.of<ServiceStage>() !=
  reflect.TypeId.of<WorkStage>()
```

`TypeId` atende a `Copy`, `Equatable` e `Hashable`. Seu valor e hash podem mudar
entre builds, toolchains e processos. O programa não deve persistir, serializar
ou transmitir esse valor.

Um schema ID possui outro contrato. Ele usa nome, versão e codificação
canônicos. Um package digest também não usa `TypeId`.

O design vigente não possui `Type<T>`, `T.type` ou construção por metatype. Um `TypeId`:

- não constrói valores;
- não resolve associated members;
- não informa layout;
- não faz lookup por nome;
- não prova conformance.

Uma API estática usa um type argument:

```w
let order = try decoder.decode<Order>()
```

Uma escolha runtime usa um enum fechado, uma factory ou um existential:

```w
protocol DishFactory {
  fn make(): Dish
}

fn makeDish(using factory: ref any DishFactory): Dish {
  return factory.make()
}
```

`Type<T>` permanece **Alternativa** para uma futura API que precise transportar
um tipo preservado. Dynamic construction por nome fica **Rejeitado por
enquanto**. Ele exigiria argumentos apagados, initializers negociados e erros
runtime para relações que hoje são estáticas.

#### 8.9.3 `Reflectable` e metadata alcançável

`std.reflect` pertence a T0. Ele não depende do host. O protocol possui um
body vazio:

```w
protocol Reflectable {}
```

O compiler adiciona um descriptor ao conformance record. O marker continua
existential-compatible e não expõe um method especial. O programa usa duas
operações:

```w
let ref staticInfo = reflect.info<MenuCard>()

fn reflectedName(value: ref any reflect.Reflectable): view String {
  let ref dynamicInfo = reflect.info(of: value)
  return dynamicInfo.name
}
```

`reflect.info<T>()` usa o tipo estático. `reflect.info(of:)` usa o conformance
record do valor apagado. Ambas retornam um descriptor imutável com lifetime de
process.

O descriptor possui estes dados lógicos:

| Dado | Contrato |
|---|---|
| `id` | `TypeId` local |
| `name` | nome qualificado da interface |
| `kind` | scalar, struct, object, enum, refinement ou enum subset |
| `base` | `TypeId?` para refinement e enum subset |
| `properties` | propriedades instance exportadas, em ordem de declaração |
| `cases` | cases exportados e payload types, em ordem de declaração |

`PropertyInfo` contém nome, `TypeId`, mutabilidade e accessors disponíveis.
`CaseInfo` contém nome e payload types. O descriptor não contém:

- offsets ou tamanho físico;
- addresses de fields;
- getter ou setter universal;
- methods invocáveis por nome;
- valores de associated members;
- nomes privados;
- acesso por string a uma instance.

Property behavior aparece como uma propriedade lógica. Seu backing storage não
aparece. Uma computed property exportada pode aparecer com seus accessors. O
descriptor não executa o accessor.

Um enum subset preserva a conformance do enum base. Seu descriptor contém
somente os cases permitidos e aponta para o `TypeId` base:

```w
enum DispatchState: reflect.Reflectable {
  queued
  running
  completed
  cancelled
}

alias LiveState = DispatchState<[.queued, .running]>
let ref liveInfo = reflect.info<LiveState>()
expect liveInfo.cases.count == 2
```

Reflection respeita a interface exportada. Ela não revela um field privado nem
package por meio de um existential exportado. Código do mesmo módulo usa HIR
compile-time ou uma API nominal para acessar esses fields.

Generic specializations possuem `TypeId` distintos. O linker emite um
descriptor somente para uma specialization alcançável. Um registry global de
tipos não existe.

#### 8.9.4 Síntese por conformance

Uma conformance explícita pode solicitar witnesses conhecidos pelo compiler. W
não usa `@derive`, decorators ou macros:

```w
struct ReservationKey: Hashable & reflect.Reflectable {
  table: TableId
  sequence: u64
}
```

O compiler reconhece o protocol por identidade de módulo. Um protocol com o
mesmo nome não ativa síntese.

O design vigente sintetiza somente estas famílias:

| Protocol | Struct | Enum | Object |
|---|---:|---:|---:|
| `Equatable` | fields semânticos | tag e payloads | não |
| `Hashable` | fields semânticos | tag e payloads | não |
| `Duplicable` | fields semânticos | payload ativo | não |
| `reflect.Reflectable` | interface exportada | cases e payloads | interface exportada |

`Hashable` também fornece o witness requerido de `Equatable`. A ordem de
declaração governa equality, hash e duplication. O enum inclui o case antes dos
payloads.

Synthesis de `Hashable` inclui synthesis de `Equatable`. Um witness manual de
equality bloqueia hash estrutural. O author fornece os dois contratos.

Synthesis exige que todos os fields ou payloads necessários atendam ao
protocol. Um resource, capability ou owner singular bloqueia `Duplicable`.

Generic constraints ficam explícitas:

```w
struct Pair<Left: Hashable, Right: Hashable>: Hashable {
  left: Left
  right: Right
}
```

O compiler não adiciona `T: Hashable` de forma oculta. Uma declaração sem a
constraint recebe um diagnostic no ponto da conformance.

Synthesis ocorre somente na declaração primária do tipo. Uma extension pode
fornecer uma conformance manual. Ela não pede acesso estrutural implícito.

Quando o tipo fornece um witness do protocol, o compiler exige todos os
witnesses desse protocol. Ele não mistura uma implementação parcial com fields
sintetizados. Essa regra evita equality customizada com hash estrutural.

Fields privados participam de equality, hash e duplication. Eles não entram em
runtime reflection.

Computed properties e associated members não participam de synthesis
estrutural. Eles não fazem parte do stored value.

Um type com property behavior não recebe synthesis estrutural de `Equatable`,
`Hashable` ou `Duplicable` no design vigente. O author fornece witnesses manuais. Essa
regra evita confundir o valor lógico com cache ou backing storage.

`Reflectable` continua disponível. Ele descreve a propriedade lógica e ignora
o backing storage.

O compiler grava HIR normalizada para cada witness. Tooling mostra o resultado:

```text
w explain synthesis restaurant.ReservationKey: Hashable
  equality: table, sequence
  hash: case none; fields table, sequence
  source witnesses: synthesized
```

Uma mudança de field pode alterar um witness sintetizado. `w interface diff`
marca a mudança para revisão de comportamento, além da classificação estrutural
normal.

Síntese de `Display`, `Ordering`, codecs e schemas permanece **Pesquisa**. Essas
families precisam de escolhas humanas sobre formato, ordem e compatibilidade.
User-defined synthesis fica **Rejeitado por enquanto**. Ele exigiria macro,
reflection compile-time aberta ou outro gerador de declarations.

#### 8.9.5 Parâmetros rest homogêneos

`T...` declara zero ou mais argumentos do mesmo tipo:

```w
fn schedule(table: TableId, courses: Course...): usize {
  for course in courses {
    queue(table, course: course)
  }
  return courses.count
}

let count = schedule(
  table,
  courses: .nebulaBroth,
  .horizonCake,
)
```

O parâmetro rest deve ser o último. A declaração permite somente um. Ele não
aceita default.

O label aparece antes do primeiro argumento repetido. Os argumentos seguintes
não repetem o label. Um rest com label `_` recebe todos os itens sem label:

```w
fn byteCount(_ messages: ref String...): usize {
  var total: usize = 0
  for message in messages { total += message.bytes.count }
  return total
}

let bytes = byteCount("Kitchen ready", "Universe ending soon")
```

Zero itens não escreve o label:

```w
let emptyCount = schedule(table)
```

Uma call sem itens não infere o element type de um generic rest. Outro
argumento ou o expected result precisa fixar esse tipo.

O binding possui tipo intrínseco `Arguments<T>`. Um parâmetro `ref T...` produz
`Arguments<ref T>`. `Arguments` oferece `count`, indexação e iteração. Ele não
pode ser construído, armazenado, retornado ou capturado.

`for item in arguments` faz borrow de um pack owned. Ele preserva um element
que já é `ref T`. `for item in take arguments` consome um pack owned.

O function type preserva o rest marker:

```w
let scheduler: fn(TableId, Course...): usize = schedule
```

Callable conversion exige o mesmo prefixo, element type, label e ownership.

Uma declaração rest representa um conjunto de call shapes. O compiler rejeita
qualquer interseção com outra declaração:

```w
fn serve(table: TableId)
fn serve(table: TableId, _ courses: Course...)
// error: both declarations accept serve(_)
```

O compiler não prefere a forma fixa. Para exigir um item, declare o primeiro
item fora do rest:

```w
fn serve(table: TableId, first: Course, _ remaining: Course...)
```

Essa regra mantém overload resolution independente dos tipos.

#### 8.9.6 Expansão, ownership e lowering

`each` expande uma collection no último argumento rest:

```w
let planned = [.nebulaBroth, .horizonCake]
let count = schedule(table, courses: each planned)
```

`each` é a keyword de expansão em um argument. Ela não é um operador geral. A
forma evita colisão com o range unilateral `4...`.

Uma call aceita uma expansão final. Argumentos individuais podem aparecer antes
dela:

```w
schedule(table, courses: .welcomeDrink, each planned)
```

O source expandido deve ser `Arguments<T>`, `view Array<T>`, `[T; N]` ou
`Array<T>`. O element type e ownership precisam corresponder.

As regras normais de parâmetro valem para cada elemento:

| Rest | Efeito no caller |
|---|---|
| `T...` com `T: Copy` | copia cada elemento |
| `T...` owned | move cada último uso |
| `ref T...` | cria borrows compartilhados |
| `take T...` | exige `take` em cada valor owned |

Uma expansão owned usa `each take values`. Ela consome a collection. Uma
expansão borrowed usa somente `each values`.

```w
fn archive(_ records: take AuditRecord...) {
  for record in take records { persist(take record) }
}

archive(each take pendingRecords)
```

`inout T...` fica **Rejeitado por enquanto**. Um número dinâmico de borrows
exclusivos torna alias diagnostics e recovery pouco previsíveis. A API recebe
`inout view Array<T>`.

Argumentos individuais podem usar storage no call frame. Uma expansão borrowed
passa address e count. O lowering não exige heap. `Arguments<T>` mantém cleanup
dos elementos owned em todas as saídas.

Rest W não faz parte do ABI C. C varargs continuam `unsafe` e exigem um adapter
tipado ou `c.vaList`. Default argument promotions não entram no type checker W.

#### 8.9.7 Formas adiadas

Três famílias não entram no design vigente:

| Família | Estado | Baseline |
|---|---|---|
| typed property path | **Pesquisa** | closure ou função nominal |
| generic associated type | **Pesquisa** | primary associated type e método generic |
| type/value parameter pack | **Pesquisa** | rest homogêneo, tuple ou collection |

Uma typed property path precisa preservar place, borrow, accessors e
visibilidade. A forma vigente de pesquisa usa um construtor explícito:

```w
let guestName = path<Order>(.guest.name)
```

`\Order.guest.name` permanece **Alternativa**. Reflection por string fica
**Rejeitado por enquanto**. Até o contrato fechar, uma API recebe uma closure:

```w
let names = orders.map((order) => order.guest.name)
```

Generic associated types precisam de uma relação de borrow expressável e de
witness layout estável. O W0 não precisa dessa capacidade.

Packs heterogêneos evitariam overloads por aridade. Eles também adicionariam
outro kind, shape constraints e pack iteration. A sintaxe abaixo permanece
**Pesquisa**:

```text
fn format<each T: Display>(_ values: ref each T...)
```

O
[Swift SE-0161](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0161-key-paths.md)
mostra o valor de property paths tipadas. O
[Swift SE-0185](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0185-synthesize-equatable-hashable.md)
mostra síntese limitada a casos estruturalmente seguros. O
[Swift SE-0393](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0393-parameter-packs.md)
mostra a complexidade adicional de packs heterogêneos. W fecha primeiro a forma
homogênea.

## 9. Memória, layout e alocação

### 9.1 Quatro contratos separados

**Exemplo:** mover um `Buffer` encerra o binding antigo. Stack, arena ou heap não
mudam esse resultado.

W separa quatro contratos:

1. **Semântica:** owner, move, copy, borrow, shared, cleanup e erro.
2. **Lowering:** escape, placement, task frame, drop edge e specialization.
3. **Representação:** stack, register, heap, arena, niche, tag e allocator.
4. **Host:** quota, fault boundary, telemetry e policy de OOM.

Somente o primeiro contrato define o significado normal do programa. Os outros
podem mudar por target ou profile sem alterar o resultado observável.

Todo valor que exige cleanup possui um owner. O compilador controla
inicialização, move, borrow, escape e drop. A escada semântica é:

1. valor `Copy`;
2. owner único com drop determinístico;
3. borrow `ref` ou `inout`;
4. owner único `Pinned<T>` quando a API publica endereço estável;
5. região lexical quando muitos valores possuem o mesmo lifetime;
6. `shared T` quando existem múltiplos owners reais;
7. owner de service para estado serializado por instância;
8. ponteiro manual somente em `unsafe` ou FFI.

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
- partial move de field não existe no design vigente;
- destructuring owned move o aggregate inteiro e inicializa novos bindings;
- destructuring `ref` ou `inout` cria borrows de projections visíveis;
- reatribuição avalia o novo valor antes de destruir o valor anterior.

O frontend calcula lifetimes por uso e controle de fluxo. O source não contém
annotations de lifetime. Quando a prova falha, o diagnostic mostra o owner, o
borrow, o uso conflitante e a menor correção conhecida.

`ref` e `inout` também podem qualificar um resultado ou um argumento de tipo.
Eles continuam borrows; não criam reference counting:

```w
fn first(values: ref Array<Recipe>): ref Recipe? {
  return values.get(0)
}

let firstRecipe: ref Recipe? = first(recipes)
let views: Array<ref Recipe> = recipes.borrowed().collect()
```

`ref T?` significa `Option<ref T>`. `ref (T?)` significa borrow de um slot
optional. Um resultado borrowed deve vir do receiver ou de um parâmetro
borrowed explícito. A interface compilada registra essa relação de provenance:

```w
fn choose(primary: ref Recipe, fallback: ref Recipe): ref Recipe {
  return if primary.isReady { primary } else { fallback }
}
```

O source não escreve lifetime. O compiler rejeita um retorno que aponta para
local storage e mostra a origem inválida:

```w
fn invalid(): ref Recipe { // error: local Recipe ends at return
  let local = Recipe(...)
  return local
}
```

Um resultado `inout T` mantém o acesso exclusivo até o último uso do borrow. Ele
não pode ser guardado em um field owned comum nem sobreviver ao owner:

```w
guard let inout selected = inventory.getMutable(id) else {
  throw .missingIngredient(id)
}
selected.quantity -= 1
// inventory volta a aceitar acesso no último uso de selected.
```

Um borrow pode permanecer vivo após `await` somente quando o compiler prova:

1. que o owner permanece válido;
2. que owner e borrow ocupam storage estável antes da suspensão;
3. que o owner não é movido ou substituído;
4. que não existe acesso mutável conflitante;
5. que cancelamento também encerra o borrow.

O compiler pode colocar os valores no mesmo task frame. Se não puder provar
essas condições, ele exige ownership, copy ou uma API de pinning. Um raw pointer
não contorna essa regra.

#### 9.2.1 Placement e alocação inferida

Ownership não escolhe um endereço. O compiler escolhe register, stack, static
storage, task frame, arena ou allocator conforme escape, tamanho e target.
Source comum não recebe annotation de placement.

**Garantio design vigente:** uma função síncrona não causa alocação no allocator geral
somente para guardar um local de tamanho fixo que não escapa. Register pressure
pode criar um stack spill. Ela não autoriza boxing no heap.

```w
fn scale(sample: Sample): Sample {
  let adjusted = Sample(x: sample.x * 2.0, y: sample.y * 2.0)
  return adjusted // Nenhuma alocação geral é necessária.
}
```

`struct` e `object` também não significam heap. `object` define identity e
encapsulation. O owner pode ficar inline, no stack, num task frame, numa arena
ou numa allocation própria. Expor um endereço cria uma barreira de
representação, mas não exige heap. `pin` é necessário somente quando o endereço
precisa permanecer estável.

Containers dinâmicos, `shared`, pinning e task creation podem solicitar storage.
A interface compilada registra uma obrigação de alocação quando a operação pode
usar o allocator geral. Essa obrigação não aparece como annotation no source.
Ela alimenta diagnostics, budgets e `w explain memory`.

```text
w explain memory restaurant.app::run
  dynamic: String growth at app.w:39
  dynamic: task frame at app.w:33
  elided: temporary Order at app.w:38
```

O optimizer pode eliminar ou combinar allocations quando o programa não observa
identity, endereço, falha recuperável, budget, drop ou deallocator. Uma operação
fallible feita contra um allocator ou budget explícito é observável. O optimizer
não pode mover sua cobrança para fora dessa boundary.

Um product pode exigir que todo o grafo alcançável de um entry não use o
allocator geral:

```text
w check memory --require no-general-allocation restaurant.embedded
```

O gate usa HIR e link graph. Ele não depende de annotation em cada função. Uma
task frame, um `shared` control block ou growth dinâmico sem allocator fixo
produz diagnostic com a call chain. Essa prova não limita stack por si. Um
profile freestanding também declara stack e task-frame budgets.

### 9.3 Pinning e valores sensíveis ao endereço

A maioria dos valores W pode mudar de endereço. Pinning só existe quando uma API
depende de endereço estável, como uma callback C persistente, uma estrutura
self-referential ou um task frame que contém borrows internos.

Task frames gerados pelo compiler ficam estáveis enquanto uma suspensão exigir
isso. Essa escolha não aparece no source e não exige annotation.

**Forma vigente:** `pin` é uma operação unary de storage e ownership:

```w
let state = try pin take callbackState

unsafe { register_callback(state.asOpaqueCPtr()) }
```

A expressão é `try (pin (take callbackState))`. `take` move o owner para a
operação. `pin` coloca o valor em storage estável e produz
`Result<Pinned<T>, AllocationError>`. `try` propaga o error e entrega o handle.
Um temporary owned não precisa de `take`:

```w
let state = try pin BellState(closed: false)
```

O formatter usa a ordem `try pin take value`. `try take pin value` é erro:
`take` não configura outra operação. `take<.pin>` fica **Rejeitado por
enquanto** porque tornaria um move comum fallible e alocante. Ele também
misturaria uma policy de storage com a transferência de owner.

`pin let state = value` e `let pin state = value` ficam **Alternativa** para um
futuro pinned local lexical. Elas não substituem o handle owned necessário para
callback persistente, retorno ou field.

`pin` pode alocar ou adotar storage que já possui endereço estável. A operação
pode falhar antes de publicar o endereço. `Pinned<T>` pode mudar de endereço; o
`T` apontado por ele não pode. O raw pointer só é válido enquanto o owner
`Pinned<T>` permanece vivo. O handle é move-only; pinning não cria um segundo
owner.

O design vigente não possui keyword `unpin`. Consumir ou destruir `Pinned<T>` executa drop
no endereço estável. Mover `T` para fora depois que seu endereço foi publicado
exigiria provar que nenhum safe borrow, self-reference ou foreign pointer
permanece. Um consuming `intoValue` com proof token fica em **Pesquisa**.

**Pesquisa:** os nomes `PinnedRef<T>`, `PinnedMut<T>` e `withMut` para borrows
scoped ainda precisam de corpus. O contrato já é fixo:

- pinning não é ownership compartilhado;
- pinning não prova alias, validade ou thread safety;
- drop ocorre antes de o storage estável ser reutilizado;
- projection para um field pinned precisa de prova do compiler ou `unsafe`;
- safe projection mantém o parent pinned e não permite mover o field;
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

`share` cria o primeiro shared owner. A operação é uma função intrinsic T0,
sempre fallible, porque pode criar um control block:

```w
let root = try share(
  MenuSection(title: "Dinner", parent: .none, children: []),
  using: memory,
)
let observer = copy root
let parent = root.weak()
```

Um temporary não usa `take`. Promover um owner existente exige
`share(take value, using:)`. A forma sem `using` usa o allocator default. W não
converte um owner único para `shared T` somente por expected type ou parâmetro.

`shared T` e `weak T` são move-first. `copy handle` cria outro owner e torna o
retain visível no source. `upgrade()` é a única operação que cria um shared owner
a partir de `weak`. Uma função pode mover seu último shared handle sem retain.

O compiler pode co-alocar temporary e control block ou remover contagem quando
prova owner único. `share` ainda preserva failure, allocator e origem. Um valor
que depende de uma arena mais curta precisa de `rehome` antes da promoção.

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

Overflow de contador nunca faz wrap. Ele encerra a fault boundary antes de
perder um owner. O último release executa `deinit` uma vez. `weak` expira antes
de o storage ser reutilizado.

`ServiceRef<T>` não é `shared T`. O host controla o lifecycle da instance. Um
handle de service mantém identity e capability, não ownership direto do estado.

`w explain ownership` mostra retains, releases, possíveis ciclos e a razão de
uma contagem atômica. Isso é evidence de tooling, não prova global de ausência
de ciclo.

### 9.5 Regiões

Região agrupa lifetimes. Budget limita recursos. Eles são conceitos diferentes.
`Arena` é a capacidade de alocação. `region` é a boundary lexical que possui essa
capacidade.

**Forma vigente:** o bloco cria uma arena, liga seu lifetime ao nome e fecha a arena
em todas as saídas:

```w
region request(using: ctx.memory, limit: 64<MiB>) {
  let document = try parse(payload, using: request)
  respond(document)
}
```

O argumento `using` é opcional. Quando ele não existe, a região usa o allocator
default fixado pelo product. `limit` é obrigatório no design vigente. O nome `request`
aceita os mesmos lugares que esperam um `Allocator` borrowed. Ele não é uma
variável global nem um allocator thread-local.

Somente uma operação com `using: request` usa a região. O compiler não move
silenciosamente todos os locais do bloco para a arena:

```w
region request(limit: 64<MiB>) {
  let header = RequestHeader(...)                    // placement inferido
  let tree = try Json.parse(payload, using: request) // storage da região
}
```

Uma `Arena` é move-only e não é `shareable` por default. Um child paralelo usa
uma arena filha exclusiva:

```w
region request(limit: 64<MiB>) {
  let imageMemory = try request.child(limit: 16<MiB>)
  spawn let image = decodeImage(payload, using: take imageMemory)
  inspect(try await image)
}
```

O child precisa terminar antes do fim da região. Cancelamento faz join dos
children antes de liberar o storage. A arena default não recebe allocations
concorrentes. Um allocator sincronizado é outro tipo e declara esse custo.

Um valor que contém storage da região não escapa por return, field com lifetime
maior, `shared` owner ou task detached. Um valor independente e inline pode
escapar. A operação consuming `rehome` faz a transferência explícita:

```w
fn decodeMenu(payload: ref Bytes, memory: ref Allocator): Menu throws AllocationError {
  region scratch(using: memory, limit: 8<MiB>) {
    let parsed = try Menu.parse(payload, using: scratch)
    return try (take parsed).rehome(using: memory)
  }
}
```

`rehome` move fields independentes e realoca somente o storage que ainda depende
da região. Como todo receiver consuming, uma falha também consome o source. A
operação limpa o source e qualquer destino parcial antes de propagar o error. Uma
variante `attemptRehome` pode devolver o source num outcome quando o caller
precisa de retry. Adoção sem cópia só ocorre quando o allocator de destino
declara transferência de origem compatível.

Plain data usa bulk release. Um valor com `deinit` entra num drop ledger. O
cleanup usa a ordem inversa da construção concluída, não a ordem de conclusão de
tasks. Growth numa arena monotônica pode reservar um bloco novo e manter o bloco
antigo até o fim da região.

O budget lógico cobra tamanho solicitado após alignment, inclusive padding,
growth abandonado e drop metadata. Ele não inclui metadata privada do allocator
upstream. Isso torna `BudgetExceeded` reproduzível para a mesma execução. O host
pode medir e limitar bytes residentes separadamente.

`Arena` também existe como API T0. Ela permite bootstrap e código sem a syntax
`region`:

```w
var storage: [u8; 64<KiB>] = [0; 64<KiB>]
var scratch = Arena.fixed(inout storage)
let tokens = try lex(source, using: scratch)
scratch.clear()
```

`Arena.fixed` nunca solicita storage ao OS. `clear` executa os drops registrados
e reinicia a capacidade. Ele é erro enquanto um borrow ou valor dependente da
arena permanece vivo.

**Alternativas preservadas:** usar somente a API `Arena`, inferir uma região por
escape analysis ou marcar cada tipo com lifetime. A forma de bloco lidera porque
torna budget e cleanup visíveis sem annotations por valor. O W0 implementa a API
primeiro; a syntax pode baixar para a mesma API depois.

### 9.6 Allocator e origem

`Allocator` é uma capability opaca de T0. Código safe pode passá-la a APIs
allocating. Somente runtime, FFI e adapters `unsafe` implementam a operação raw
de allocate, resize e deallocate.

```w
fn decode(payload: ref Bytes, using memory: ref Allocator): Document throws AllocationError {
  var nodes = Array<Node>(using: memory)
  try nodes.tryReserve(minimumCapacity: 128)
  return try parseNodes(payload, into: nodes)
}
```

O owner criado com um allocator não pode sobreviver a ele. A HIR registra essa
relação de provenance. O source não escreve lifetime. Um container mantém a
origem necessária para resize e drop; ele não consulta um default novo depois.

O allocator default é fixado pelo product e pelo host adapter. Ele não muda
durante uma call, thread ou module import. Uma API sem `using` usa esse default:

```w
var names = Array<String>()              // allocator default do product
var local = Array<String>(using: memory) // allocator explícito
```

Alocação e growth normais podem causar panic `.outOfMemory`. A forma `try*`
retorna `AllocationError` e mantém o valor anterior quando falha. Uma API
explícita nunca converte `BudgetExceeded` em OOM.

O profile portátil começa com o allocator do sistema. O host pode selecionar
mimalloc ou outro allocator compatível. A seleção participa da recipe, do
artifact fingerprint e do profile de performance. Ela não altera ownership.

[mimalloc](https://github.com/microsoft/mimalloc) permanece uma opção forte para
benchmark. Sua portabilidade, heaps separados e modos de segurança justificam o
teste. Nenhum benchmark autoriza torná-lo universal sem matriz de target,
sanitizer, override, unload e cross-thread free.

W chama o allocator selecionado por sua API. Ele não depende de override global
de `malloc` quando código estrangeiro pode misturar origens. As regras de heap e
thread do mimalloc variam por versão. O profile fixa versão e configuration e
declara separadamente onde allocate e free podem ocorrer. O modo secure adiciona
mitigations; ele não promete memory safety.

| Profile de allocator | Uso |
|---|---|
| `system` | baseline portátil e integração estrangeira |
| `mimalloc` | candidato de performance após benchmark |
| `mimalloc-secure` | candidato de hardening com custo medido |
| `fixed` | buffer fornecido pelo host; nenhuma allocation do OS |

O modelo de arenas pre-reservadas do
[mimalloc](https://microsoft.github.io/mimalloc/group__arenas.html) pode ajudar
um host fixed. Os
[heaps do mimalloc](https://microsoft.github.io/mimalloc/group__heap.html)
mostram por que mobilidade de allocate e free precisa ser uma propriedade do
profile, não uma suposição pelo nome do allocator.

Cada allocation possui uma origem lógica:

```text
origin = allocator identity + instance + deallocator contract
```

A origem pode ficar no owner, num control block ou numa side table. Ela não
precisa ocupar bits do pointer. Move preserva a origem. FFI preserva o
deallocator estrangeiro. W nunca chama `free` num pointer de origem
desconhecida.

Zero-sized values não solicitam storage. Alignment precisa ser uma potência de
dois suportada pelo allocator. Soma ou multiplicação de tamanho que excede
`usize` falha antes da call raw. Uma allocation family sempre usa seu
deallocator correspondente; o optimizer não troca famílias através de uma
fronteira observável.

Safe W nunca lê bytes sem inicialização. Isso não exige zerar toda allocation.
Typed construction e definite initialization gravam cada valor antes da leitura.
Uma API que precisa de zero declara a operação:

```w
let bitmap = try Bytes(repeating: 0_u8, count: size, using: memory)
```

Somente `unsafe MaybeUninit<T>` expõe storage sem inicialização. Um profile pode
zerar storage ao alocar ou liberar por hardening, mas essa policy não muda o
valor de um programa safe. O mimalloc `zalloc` é uma implementação possível para
uma call que promete zero; ele não é o default semântico de toda allocation.

Alocações que precisam de recovery usam API fallible ou uma região com budget.
OOM geral encerra a fault boundary conforme a seção de panic. `w explain memory`
mostra allocator, origem, escape, stack estimate e motivo de cada allocation.

### 9.7 Provenance, pointer e address

**Exemplo:** converter um pointer para um endereço inteiro e voltar não restaura
authority sem uma API `unsafe` específica.

Um pointer não é somente um número. Ele carrega um endereço e a autorização para
acessar uma allocation durante um intervalo. Essa autorização inclui alcance,
lifetime e mutabilidade.

As regras de W são:

- `ref`, `inout` e `view` preservam provenance;
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

**Exemplo:** um wrapper `foreign c struct` fixa offsets. Um struct W comum pode
trocar packing entre builds.

Layout W comum é opaco entre builds. O compiler pode reorder fields não
exportados, usar niches, eliminar aggregates ou especializar storage interno.

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

#### 9.9.1 Valores válidos e niches

Um niche é uma representação física que o tipo lógico não usa. O compiler pode
usar esse espaço para representar um case adicional.

**Exemplo:** `ref Oven` não aceita null. `Option<ref Oven>` pode usar null para
`.none` e os pointers não null para `.some`. Essa escolha não adiciona um estado
ao pointer e não permite dereference de null.

A HIR registra `Validity<T>`, que descreve os bit patterns válidos conhecidos.
O layout de um enum segue esta ordem:

1. enumere os cases lógicos e os payloads;
2. calcule os estados físicos necessários;
3. encontre niches que nenhuma construção safe ou fronteira externa produz;
4. escolha um mapping determinístico;
5. use tag e payload explícitos quando a prova não for suficiente.

Um niche só é válido quando todas as origens do valor respeitam o mesmo
contrato. Um `c.ptr<T>` não null pode oferecer null como niche. Bytes vindos de
FFI, storage persistido ou uma union C não oferecem esse niche até que um
adapter valide a representação.

**Exemplo:** os três estados abaixo permanecem distintos:

```w
let unknown: Option<Option<ref Oven>> = .none
let knownMissing: Option<Option<ref Oven>> = .some(.none)
let knownOven: Option<Option<ref Oven>> = .some(.some(oven))
```

Um único null separa somente dois estados. O terceiro estado exige outro niche
provado ou uma tag explícita. O compiler nunca colapsa `unknown` e
`knownMissing`.

Enums usam a mesma regra:

```w
enum BellTarget {
  open(c.ptr<ll_bell>)
  unavailable
}
```

`BellTarget` pode usar null para `unavailable` num layout interno. Se o enum
ganhar `permissionDenied`, o compiler precisa encontrar outro niche ou expandir
o layout. A mudança de representação interna não muda o switch.

Um subset de enum reduz o conjunto semântico, mas não cria uma promessa pública
de tamanho:

```w
fn nextStage(): ServiceStage<[.preparing, .serving, .completed]>
```

O caller trata somente esses três cases. Se a assinatura passar a incluir
`.cancelled`, o diff de interface é incompatível e cada switch antes exaustivo
recebe um diagnostic. Um specialization interno pode remover cases e tags
impossíveis. Uma ABI pública mantém o layout publicado para o enum base ou usa
um schema próprio.

#### 9.9.2 Low bits

Um pointer para storage alinhado a `A` bytes possui `ctz(A)` bits inferiores
iguais a zero. O compiler pode usar esses bits somente quando prova o alignment
real da allocation e controla todo o storage do valor.

**Exemplo:** alignment provado de 16 bytes oferece quatro bits candidatos. Uma
allocation estrangeira com alignment de 4 bytes oferece somente dois, mesmo que
o tipo nominal tenha alignment maior.

O lowering mantém a provenance original. Ele mascara a tag antes de:

- dereference;
- call FFI;
- comparação que exige pointer canônico;
- operação atômica de pointer;
- entrega a debugger, sanitizer ou profiler.

**Exemplo:** um `shared MenuSection` interno pode guardar um flag imutável num
low bit. `ll_bell_subscribe` recebe sempre o `c.ptr` canônico, sem esse flag.

Low bits não guardam reference count, generation mutável, deallocator ou
allocator identity. Esses valores mudam, podem exceder os bits disponíveis ou
precisam sobreviver a uma representação canônica.

#### 9.9.3 Metadata mutável, atomics e ABA

Uma tag mutável só pode compartilhar uma palavra atômica com o pointer quando
todas as leituras e atualizações usam a palavra inteira. W não mistura uma view
atômica tagged com uma view não atômica do mesmo storage.

**Exemplo:** um slot que faz compare-and-swap de `{pointer, state}` precisa de
uma operação atômica para o par. Atomicidade de pointer não promete que uma
palavra maior seja lock-free.

Um contador pequeno não resolve ABA. Depois do wrap, o mesmo pointer e a mesma
tag podem reaparecer. Uma estrutura que precisa impedir ABA usa generation com
largura suficiente, epoch, hazard pointer ou outro algoritmo declarado.

**Exemplo:** uma fila não pode considerar `{node, generation: 3}` único para
sempre quando a generation possui somente dois bits.

#### 9.9.4 Headers, control blocks e handles

W não exige um header universal por object. Um valor com owner único pode ser
headerless. `shared T` cria um control block quando a implementação precisa de
strong count, weak count ou deallocator. Service identity fica no runtime do
host. Reflection mantém metadata por tipo alcançável, não por instance.

**Exemplo:** `BellHandle` possui somente seu handle e seu estado de drop.
`shared MenuSection` pode apontar para um control block. `ServiceRef<Kitchen>`
contém identity e capability do host. Os três valores não precisam do mesmo
header.

Pointer compression e handles indexados são uma classe diferente de tagging.
Eles só existem quando uma arena ou heap isolado fornece base e bounds
explícitos.

**Exemplo:** um target Wasm pode representar um handle de arena por `u32` e
expandir esse handle na façade C. Um pointer nativo fora da arena não participa
dessa representação.

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

NaN boxing não representa `f64` comum nem um valor W universal. W preserva NaN,
payload e signed zero. Um futuro container dinâmico interno pode pesquisar NaN
boxing, mas precisa de fallback e não pode alterar operações IEEE.

**Exemplo:** guardar `f64` em `Array<f64>` preserva os bits de um NaN. O
compiler não usa o payload desse NaN para representar `.none`.

### 9.10 Negociação, hardening e instrumentação

**Exemplo:** um profile com Memory Tagging Extension (MTE) pode desativar low-bit
tagging sem mudar a semântica do valor.

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

High-bit tagging não faz parte do profile portátil vigente. Um profile experimental
precisa confirmar, no mínimo:

1. CPU e modo de paginação;
2. enablement do processo e das threads;
3. ABI de kernel e system calls;
4. allocator e canonicalização;
5. linker, debugger e unwinder;
6. hardening e sanitizers ativos;
7. fronteiras FFI e de módulo binário.

**Exemplo:** suporte de CPU a top-byte-ignore não autoriza enviar um pointer
tagged a uma system call. O adapter remove a tag ou usa o fallback portátil.

Hardening vence compactação. MTE, pointer authentication, HWASan ou uma
capability architecture podem ocupar bits ou impor provenance que W não pode
reutilizar.

Testes diferenciais executam o mesmo corpus em profile portátil e compacto.
Sanitizers executam o fallback. Um resultado diferente bloqueia a otimização.

O fingerprint de representação inclui target data layout, schema de valores
válidos, ABI, allocator, hardening, sanitizer e versão do compiler. Dois objetos
binários só compartilham um layout W quando esses componentes compatíveis
produzem o mesmo fingerprint.

**Exemplo:** mudar o alignment garantido pelo allocator pode remover dois low
bits. O linker adapta, recompila ou rejeita; ele não interpreta o layout antigo
como novo.

### 9.11 Destruição e recuperação de storage

**Exemplo:** se o terceiro field falhar durante `init`, o runtime destrói o
segundo e o primeiro. Ele não chama `deinit` do aggregate incompleto.

- `deinit` recebe acesso exclusivo e não consuming ao valor completo;
- `deinit` pode mutar fields, mas não pode mover fields ou `self`;
- `deinit` não chama `take fn` no próprio receiver;
- um tipo com `deinit` customizado não atende a `Copy`;
- locals são destruídos na ordem inversa da inicialização;
- fields owned morrem em ordem inversa da inicialização completa;
- branches mantêm drop state explícito na HIR;
- `deinit` é síncrono e não usa `throws`;
- `defer` cobre todas as saídas estruturadas;
- cancelamento executa cleanup;
- panic encerra a fault boundary e não executa user cleanup;
- foreign callbacks registram owner, context e destroy function;
- o valor shared morre após o último owner; o control block morre após o último
  weak handle;
- pinned storage executa drop antes de perder estabilidade de endereço.

O compiler pode eliminar drop flags depois de provar definite initialization.
Ele não remove um cleanup observável.

Um `take fn` assume a obrigação de destruir ou transferir `self`. As saídas
seguem estas regras:

1. `return self` ou outro consumo inteiro transfere a obrigação;
2. fallthrough e `return ()` executam `deinit` uma vez;
3. `throw` e cancellation também executam `deinit` uma vez;
4. `defer` termina antes de `deinit`;
5. os fields executam drop depois de `deinit`.

Todo caminho que não transfere `self` precisa deixá-lo completo e válido para
`deinit`. O verifier aplica definite destruction antes de cada saída.

Código safe não possui `discard self`, `forget` ou call manual de `deinit`.
Uma operação que limpa um recurso antes do fim deixa o wrapper em estado válido.
Um handle opcional pode virar `.none`, por exemplo. O optimizer pode representar
esse estado com um niche.

Essa regra impede double-close em todos os caminhos. Ela também evita uma
análise de partial destruction na primeira implementação. Um futuro suporte a
extração de fields exige uma prova inversa de definite initialization.

A
[SE-0390](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0390-noncopyable-structs-and-enums.md)
mostra o risco de suprimir `deinit` em somente alguns caminhos. O
[`Drop` de Rust](https://doc.rust-lang.org/stable/core/ops/trait.Drop.html)
também mantém o destructor separado do consumo explícito. W usa um estado
válido e drop automático como baseline.

### 9.12 Explicação e medição

**Exemplo:** `w explain memory value` mostra owner, escape, allocation e drop
path para `value`.

`w explain memory` separa fatos, estimates e medições:

- owner, move, borrow e drop são fatos semânticos;
- stack, heap, region e tag são escolhas do artifact;
- tamanho importado e peak runtime são estimates;
- allocator calls, resident bytes e retain count são medições.

Uma lens de import pode estimar código, static data e peak memory. Ela não
publica um único número como previsão de runtime universal.

`w explain layout T` mostra:

- estados lógicos e payloads;
- layout portátil;
- niches e alignments provados;
- layout escolhido e fingerprint;
- fronteiras que exigem forma canônica;
- otimizações recusadas e o motivo.

**Exemplo:**

```text
$ w explain layout BellTarget
logical states: open(c.ptr<ll_bell>), unavailable
portable:       tag + payload
selected:       null niche
ffi form:       canonical c.ptr<ll_bell>
high-bit:       rejected; profile portable
```

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

Cada behavior publica seu custo na interface compilada. `Lazy` pode calcular,
alocar e guardar o valor uma vez. O nome `Lazy` torna essa diferença visível na
declaração. `w explain` mostra o initializer e o storage gerado.

Composição v0 usa um behavior composto nomeado. Lista por vírgula e nesting
arbitrário ficam como alternativas até que ordem, exclusivity e drop tenham uma
regra simples.

Range continua responsável por `contains` e `clamp`. Um behavior `Clamped` só
serve quando a propriedade precisa aplicar uma policy em toda atribuição. Ele
não substitui o refined type nem o `Range`.

## 11. Erros, panic, OOM e cleanup

### 11.1 Três canais distintos

W não usa um único mecanismo para ausência, falha recuperável e invariante
quebrada:

| Canal | Tipo ou efeito | Exemplo |
|---|---|---|
| ausência esperada | `Option<T>` | `menu.get(course)` |
| falha recuperável | `Result<T, E>` ou `throws E` | `try parse(source)` |
| invariante quebrada | panic | `orders[index]` fora do range |

Cancelamento também não é um error genérico. Uma task concluída usa
`TaskOutcome<T, E>` com `.success`, `.error` ou `.canceled`.

### 11.2 `Result`, `throws` e `try`

`Result<T, E>` exige `E: Error`. Ele é um enum T0 com `.success(T)` e
`.error(E)`. Ele armazena ou compõe um resultado sem criar control flow:

```w
let parsed: Result<Order, ParseError> = Order.parse(source)
let appResult = parsed.mapError((error) => AppError.parse(error))
```

`throws E` oferece direct style. Ele faz parte do function type:

```w
enum ParseError: Error {
  unexpectedToken(Token)
  incompleteDocument
}

fn parse(source: ref String): Document throws ParseError
let document = try parse(source)
```

`try` aceita uma call `throws E` ou um `Result<T, E>`. Ele produz `T` em success
e propaga `E` em error:

```w
let first = try parse(source)
let second = try Result.capture(() => try parse(backup))
```

`Result.capture` converte direct style em valor. `try result` faz a conversão
inversa. `map`, `mapError`, `andThen` e `asRef` são métodos normais de Result.
Eles não são syntax especial.

`try?` converte qualquer falha recuperável da expressão em `.none`:

```w
let candidate: GuestName? = try? GuestName(input)
```

Success produz `.some(T)`. Quando `T` já é Option, o resultado possui somente um
nível de Option. O operador não captura panic ou cancelamento. Uma expressão
nonthrowing com `try?` produz diagnostic:

```w
let course = try? Course.horizonCake // W-EFFECT-0009: expression cannot fail
```

`try?` declara perda intencional do error. Quando a causa importa, o programa usa
`try`, `Result` ou `do`/`catch`. `try!` não existe; uma invariante usa
`Result.expect("reason")`.

O [Error Handling do Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/errorhandling/)
é o precedente de ergonomia. W adiciona typed errors, Result e a regra explícita
de que panic e cancelamento não participam da conversão.

Postfix `?` não aceita Result. Ele continua exclusivo de Option:

```w
let document = result? // error: Result uses try
let document = try result
```

Uma expressão fallible precisa de `try` no escopo que propaga o error. Um `try`
externo cobre as subexpressões fallible da mesma expressão:

```w
let receipt = try store(load(source), audit: inspect(source))
```

Uma closure cria outro escopo de efeito. Ela precisa do próprio `try`:

```w
let probabilities = try demand.map((value) => try Probability(value))
```

Quando suspensão e error aparecem juntos, a ordem canônica é `try await`:

```w
let response = try await client.fetch(request)
```

Toda função fallible declara um error type concreto ou genérico. O design vigente não
possui `throws` sem tipo:

```w
fn map<U, E: Error>(
  transform: fn(ref T): U throws E,
): Array<U> throws E
```

Quando `E` é `Never`, o compiler especializa a função como nonthrowing. Como
bottom type, `Never` satisfaz a posição genérica `E: Error` sem criar um error
value ou uma conformance runtime. W não precisa de `rethrows`.

Um error concreto é um enum fechado que atende a `Error`. Seus cases carregam
dados estruturados. Uma mensagem de texto não é obrigatória:

```w
enum ParseError: Error {
  unexpectedToken(found: Token, expected: TokenKind)
  incompleteDocument(at: SourceSpan)
}
```

Se o error enum da função que chama possui exatamente um case que aceita `E`, o
compiler pode inserir essa conversão total:

```w
enum AppError: Error {
  parse(ParseError)
  storage(StorageError)
}
```

Duas rotas possíveis tornam a conversão ambígua. A função usa `mapError` ou
`do`/`catch` nesse caso.

Uma call por `ServiceRef` pode ter os effects `E` e `ServiceFailure`. Cada effect
precisa de uma rota total e única até o error enum da função:

```w
enum KitchenError: Error {
  oven(OvenError)
  service(ServiceFailure)
}

let ready = try await ovens.preheat()
```

`do`/`catch` testa os clauses em ordem lexical. Um guard torna a seleção
explícita:

```w
do {
  return try parse(source)
} catch .unexpectedToken(let found, _) if found.isRecoverable {
  return try parseFallback(source)
} catch error {
  throw .parse(error)
}
```

`catch` sem pattern aceita qualquer error. `catch error` liga o valor. Um error
sem match propaga quando a função declara `throws E`. Um contexto nonthrowing
exige catches exaustivos.

`throw expression` encerra o branch e possui tipo `Never` no IR:

```w
guard let ref recipe = recipes[course] else throw .missingRecipe(course)
```

### 11.3 Valores obrigatoriamente usados

Todo valor que não é `()` ou `Never` precisa ser consumido, ligado ou descartado
de forma explícita. A regra inclui `Result` e elimina uma annotation especial
como `must_use`:

```w
prices.add(.cake)         // error: unused Bool
let inserted = prices.add(.cake)
let _ = logger.flush()    // descarte intencional
```

Debug e release preservam a mesma semântica de error. Um profile pode adicionar
um error return trace sidecar. O trace contém somente spans de propagação e
conversão, build ID e source ID. Ele não é observável pelo programa:

```text
W-ERROR-TRACE parse.w:42 -> command.w:18 -> app.w:37
```

O lowering não exige exception unwind do host. MLIR representa success e error
com valores tagged e control-flow edges. Cada edge executa os drops e defers
aplicáveis.

Os precedentes principais são
[Swift typed throws](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0413-typed-throws.md),
[Rust Result](https://doc.rust-lang.org/std/result/index.html) e
[Zig error return traces](https://ziglang.org/documentation/master/). W mantém
direct style, valor armazenável e trace de propagação como contratos separados.

### 11.4 Panic e fault boundaries

Panic informa que o programa não pode continuar dentro de uma fault boundary.
Ele não é `Error`, `Result` ou `TaskOutcome`, e source W não pode capturá-lo:

```w
let course = courses[index] // panic quando index >= courses.count
```

Uma isolation boundary serializa ou protege estado lógico. Ela não contém uma
falha física. Uma fault boundary possui teardown próprio. Ela é um process, uma
instância Wasm ou um compartment nativo com registry de recursos:

```text
service isolado no mesmo process -> panic encerra o process
service numa instância Wasm      -> panic encerra a instância
```

Sem uma fault boundary interna, panic encerra o process. Reiniciar um service no
mesmo process só é válido quando ele está em um compartment que atende ao
contrato de fault boundary. Panic nunca atravessa FFI.

O runtime emite um payload allocation-free e limitado:

```w
PanicEvent(
  code: .bounds,
  build: buildId,
  module: "restaurant.collections",
  span: SourceSpan(file: "collections.w", start: 912, end: 926),
)
```

`PanicCode` inclui ao menos `.explicit`, `.bounds`, `.overflow`,
`.divisionByZero`, `.outOfMemory` e `.internalContract`. Uma mensagem literal é
opcional. Backtrace e symbols são sidecars removíveis. Debug e release preservam
as mesmas condições de panic.

O [Rust Reference](https://doc.rust-lang.org/stable/reference/panic.html)
também separa panic de error recuperável e permite abort ou unwind. W escolhe
teardown da fault boundary e não expõe unwind recuperável em source.

### 11.5 OOM e alocação fallible

Alocação normal, growth e `copy` podem causar panic `.outOfMemory`:

```w
let duplicate = copy largeMenu // panic quando o allocator geral falha
```

Uma operação que precisa de recovery usa uma API fallible:

```w
try buffer.tryReserve(minimumCapacity: packetSize)
let snapshot = try menu.tryDuplicate()
```

`tryReserve` retorna `Result<(), AllocationError>`. `tryDuplicate` retorna
`Result<T, AllocationError>`. Elas não alteram o valor quando falham.
As duas formas aceitam `using:` quando o caller precisa escolher o allocator.
`AllocationError` possui cases estáveis:

```w
enum AllocationError: Error {
  outOfMemory
  budgetExceeded(BudgetExceeded)
  sizeOverflow
  invalidLayout(size: usize, alignment: usize)
  unsupportedAlignment(usize)
}
```

O diagnostic pode anexar allocator e origem como evidence local. Essa evidence
não faz parte de equality, serialization ou resultado reproduzível. O erro
`outOfMemory` não promete a quantidade global de memória livre.

`BudgetExceeded` é diferente de OOM. Ele informa que uma quota conhecida foi
atingida:

```w
let frame = try Bytes(repeating: 0_u8, count: size, using: arena)
// A falha pode ser `.budgetExceeded(...)`.
```

Uma arena cobra o span alinhado antes de publicar storage. Uma falha de budget
não altera offset, drop ledger ou container. Um allocator upstream ainda pode
devolver `.outOfMemory` antes de o budget lógico terminar.

OOM durante emissão de error ou cleanup escala para panic. W não possui um
handler universal de emergência em source.

### 11.6 Cleanup

`defer` executa cleanup síncrono em ordem LIFO. Destruction segue a ordem inversa
da inicialização onde ela é observável. `return`, `throw`, `break`, `continue` e
cancelamento executam o cleanup dos scopes que encerram:

```w
let file = try open(path)
defer { file.close() }
return try decode(file)
```

Panic não garante `defer`, `deinit` ou outro código de usuário. O host libera os
recursos registrados na fault boundary. O sistema operacional libera os
recursos de um process encerrado.

O body de `defer` é síncrono e nonthrowing:

```w
defer { metrics.finish(span) }
```

Cleanup que precisa suspender usa uma forma distinta:

```w
defer async {
  await connection.close()
}
```

`defer async` só existe em função async. Ele executa em LIFO durante a saída do
scope. O body trata seus próprios errors. O runtime mascara cancelamento durante
uma janela limitada pelo profile. A janela não permite cleanup sem limite.

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

O error original continua primário. O cleanup pode registrar um error secundário
depois de capturá-lo, como no exemplo anterior. Um error de cleanup sem `catch`
produz diagnostic.

`defer<.error>` e `errdefer` permanecem em **Pesquisa**. Eles precisam distinguir
error, cancelamento e saída normal sem criar uma segunda regra de cleanup.

**Pesquisa:** um protocol padrão pode criar uma obrigação linear de close. Move
transfere a obrigação. Um `defer async` reconhecido a descarrega. Sair do scope
sem close produz diagnostic. O compilador não cria uma task detached no
destructor.

`take async fn` já expressa encerramento one-shot de um owner local. Ele não
torna `ServiceRef` linear. Esse handle é shareable e pode ter aliases. Uma lease
remota precisa de capability própria ou close idempotente antes de atender ao
protocol linear.

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
3. o contrato `<.domain>` define preference de placement;
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
spawn<.compute> let plan = optimize(take snapshot)
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

`await f()` executa `f` como parte da task atual. Ele não cria um child. A call
começa na ordem lexical e herda cancellation, deadline e preference. Um
suspension point pode devolver o executor ao runtime.

`async let` e `spawn let` criam um child depois que o parent avalia argumentos e
captures. O body do callee executa no child; ele não executa parcialmente no
parent.

**Exemplo:** se `prepare(take order)` precisa avaliar uma conversão que falha, a
falha ocorre antes da criação do child. Nenhuma task recebe uma parte de
`order`.

O runtime pode executar um `spawn` inline para limitar oversubscription. Ele deve
preservar os suspension points e as regras de alias. O programa não pode usar
simultaneidade como resultado sem synchronization explícita.

Paralelismo não é uma condição de liveness. Um programa não pode exigir que dois
`spawn` executem ao mesmo tempo para liberar um ao outro. Channels, async locks
ou service calls expressam a espera sem bloquear um worker.

**Exemplo:** dois cooks não usam spin loops em flags para iniciar juntos. Eles
recebem os ingredientes por `Channel<Ingredient>` ou são children de um group.

### 12.3 `Task` e ownership

`Task<T, E>` é lexical, linear e one-shot. O estado conceitual é:

```text
created → scheduled → running → success(T) | error(E) | canceled
```

As regras são:

1. cada child pertence ao scope criador;
2. o scope não termina antes do cleanup de todos os children;
3. `await` consome o handle e move um resultado owned;
4. `task.cancel()` solicita cancelamento, mas não consome o handle;
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

Children herdam somente contexto operacional:

- cancellation ancestry e menor deadline;
- causal trace e logical stack;
- budgets descendentes;
- executor preference conforme a seção 12.6.

Dados da aplicação, capabilities e contexto mutável usam argumentos ou captures
explícitos. W não copia um mapa task-local invisível.

**Exemplo:** um child herda o deadline da reserva. Ele só recebe
`PaymentCapability` quando o call site captura ou passa esse handle.

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
Panic não é um outcome recuperável. Ele encerra a fault boundary conforme a
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
export enum CancellationReason {
  userRequest
  shutdown
  superseded
  budgetExceeded
}

report.cancel(reason: .userRequest)
batch.cancel(reason: .shutdown)
```

Cancelamento é uma solicitação idempotente. Ele não usa `pthread_cancel` e não
faz unwind assíncrono de foreign frames.

`cancel` não é keyword nem statement. Ele é um método intrínseco do owner
`Task<T, E>`. O método retorna `()` e não consome o handle. Um `SharedTask`
observer não publica esse método. O type checker reconhece a operação para
preservar structured cancellation e trace.

`CancellationReason` é fechado. Deadline e ancestry ficam em campos próprios de
`Cancellation`; eles não fingem ser um motivo escolhido pelo caller.

Uma task observa o sinal:

- antes e depois de um suspension point;
- em I/O que aceita cancelamento;
- em uma boundary de task group;
- em `Task.checkCancellation()` para loops longos.

Uma completion committed vence a corrida com cancellation. O caller recebe o
valor owned. O sinal continua pendente para o próximo suspension point ou
`Task.checkCancellation()`. O runtime não injeta cancelamento entre statements:

```w
let payment = try await billing.capture(amount)
defer async { try await refundIfNeeded(take payment) }
```

Assim, o caller pode instalar cleanup antes de suspender novamente. Uma
operação que não consegue provar se o commit ocorreu retorna seu canal de
`unknownOutcome`; ela não informa um cancelamento pré-commit falso.

O cancelamento do parent propaga para descendants. Cancelar um child não cancela
siblings, salvo policy explícita do group. Um deadline cria o mesmo sinal com
metadata de causa.

O motivo serve a policy e observabilidade. O programa não deve usar a ordem de
dois motivos concorrentes como dado de domínio. O trace mantém todos os sinais e
sua causalidade.

Cancelamento não é rollback. Uma operação externa deve declarar um commit point
ou um outcome desconhecido. Antes do commit, o adapter pode garantir ausência de
efeito. Depois do commit, cleanup não desfaz o efeito sem compensação explícita.

**Forma vigente:** cancellation safety é uma propriedade da operação e de seu estado.
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
async<.network> let catalog = fetchCatalog()
spawn<.compute> let plan = optimize(take snapshot)
```

`<.domain>` seleciona uma preference estática do profile. Ele não promete
thread, affinity ou isolation. O profile define domínios, capacity e fallback.

A resolução usa esta ordem:

1. required isolation e host affinity precisam ser compatíveis;
2. a preference explícita substitui a herdada;
3. a preference herdada substitui o default do profile;
4. o callee isolado sempre executa em sua isolation boundary.

Uma call por `ServiceRef` não muda o placement do callee. O contrato do child
caller só muda seu trabalho não isolado. `async let` e groups concorrentes
herdam a preference. `spawn` e groups paralelos usam a regra de parallel default
abaixo. Um future owner runtime precisa declarar sua preference de novo.

`spawn` em um domínio estritamente serial é error. Trabalho UI deve chamar o
owner isolado:

```w
await renderer.show(plan)
```

`spawn<.ui>` confundiria affinity serial com paralelismo.

A [SE-0417](https://www.swift.org/swift-evolution/#SE-0417) também separa
executor preference de actor isolation. W mantém essa separação na HIR.

`spawn<.compute>` é **Forma vigente**. O slot `domain` é primário e fechado.
`spawn<domain: .compute>` fica como **Alternativa**. `spawn on .compute` fica
**Rejeitado por enquanto** porque duplica o contrato estático com uma frase
especial.

#### 12.6.1 Schema de domain

`ExecutionDomainId` é um kind estático do product profile. Ele não é um pointer
para thread pool. O enum fechado `StandardDomain` oferece estes IDs lógicos:

| ID | Uso | Capability mínima |
|---|---|---|
| `.default` | trabalho async geral | concorrente e non-blocking |
| `.io` | I/O async sem distinção de transporte | I/O non-blocking |
| `.network` | I/O de rede quando o host separa essa lane | I/O non-blocking |
| `.compute` | trabalho CPU parallel | intenção paralela e budget CPU |
| `.blocking` | adapter que pode bloquear uma thread | blocking bounded |

Um case contextual sem qualificação resolve em `StandardDomain`. Por isso,
`spawn<.compute>` é curto e não é ambíguo. O exemplo abaixo mostra a forma
qualificada para um domain customizado.

Um profile pode mapear `.io`, `.network` e `.default` ao mesmo executor. A
identity lógica continua no trace. Um target single-core pode dar capacity 1 a
`.compute`; o domain continua válido e o runtime executa os jobs em sequência.

`spawn` sem argumento usa o parallel default, que é `.compute` no profile
portátil. `async let` sem argumento herda a preference atual. `concurrentMap`
herda a preference; `parallelMap` sem argumento usa o parallel default.

**Exemplo:**

```w
async let menu = loadMenu()      // herda a preference atual
spawn let plan = optimize(snapshot)  // usa o parallel default
spawn<.compute> let bill = price(order)
```

Um product package pode declarar IDs customizados. Eles são members qualificados
registrados pelo profile, não keywords ou strings:

```w
export enum LastLightDomain: ExecutionDomain {
  thermal
}

spawn<domain: LastLightDomain.thermal> let profile = solveThermalModel(oven)
```

`LastLightDomain` é outro enum fechado. O product profile registra que
`.thermal` aceita parallel intent. `ExecutionDomain` é um marker protocol T1
restrito a enums sem payload e sem parâmetros genéricos. A conformance declara
nomes que precisam de binding. Ela não cria authority, queue ou executor.

Um valor runtime do enum continua sendo somente um ID. `spawn<...>` exige o case
compile-time; `any ExecutionDomain` não funciona como executor dinâmico. Esse
caso usa `ExecutionDomainRef`.

O product descriptor escolhe capacity, queue e fallback. Um package compilado
preserva a requirement; o link falha quando o product não fornece um binding
compatível.

O schema lógico de cada domain contém:

```text
identity
capabilities: StaticList<ExecutionCapability>
capacity: range constrained by host
fallback: compatible domain or reject
affinity: none or host requirement
instrumentation identity
```

`ExecutionCapability` é um enum. O schema valida a lista, rejeita duplicatas e a
normaliza como set. Ele não dá ao enum uma semântica OR oculta. `parallel`,
`nonBlockingIO`, `blocking`, `affine` e `device` são capabilities distintas.

**Exemplo normalizado:**

```text
StandardDomain.compute
  capabilities = [.parallel]
  capacity = 1...host.cpuQuota

LastLightDomain.thermal
  capabilities = [.parallel]
  fallback = .compute
```

O deployment pode reduzir capacity. Ele pode juntar domains quando o target
conserva capabilities. Ele não pode remover affinity, isolation, ordering,
mobility ou a capacidade necessária a `spawn`.

**Exemplo:** um deployment pode mapear `.network` em `.io`. Ele não pode mapear
`LastLightDomain.thermal` num strand UI serial quando o source usa `spawn`.

A declaração de um módulo não escolhe domain default. Importar um módulo nunca
cria executor, queue ou thread. Service, entry e product descriptor podem
declarar preference porque possuem instance e lifecycle.

Os antigos “thread groups” sobrevivem como domain IDs e bindings de profile.
Essa forma mantém a finalidade e remove a promessa de uma thread fixa. A
[documentação de Dispatch Queues da Apple](https://developer.apple.com/library/archive/documentation/General/Conceptual/ConcurrencyProgrammingGuide/OperationQueues/OperationQueues.html)
também separa queues de threads e permite que a capacidade varie com o sistema.

#### 12.6.2 Priority, deadline e seleção dinâmica

Priority e domain são contratos diferentes. `.background` não é um domain
standard no design vigente. Um profile pode oferecer QoS como policy, mas priority não muda
ownership, ordering, isolation ou resultado.

Deadline cria cancellation com causa. Priority continua uma preferência de
scheduling. Uma task urgente sem deadline não ganha uma garantia temporal.

**Exemplo:** `.compute` informa o tipo de trabalho. Uma policy
`.userInteractive` pode alterar sua precedência, mas não autoriza acessar state
UI.

Syntax de QoS no source fica em **Pesquisa**. O primeiro runtime aceita policy no
entry, service descriptor e API de task group. Isso evita modifier soup em
`spawn<...>`.

Seleção dinâmica permanece **Pesquisa**. O candidato usa um
`ExecutionDomainRef` fornecido pelo host:

```w
let task = Task.spawn(domain: domain, operation: work)
```

O resultado precisa continuar sendo child lexical. A API também precisa
representar admission failure sem criar uma task perdida. Implementar um
executor customizado exige a interface de runtime `unsafe`; uma library comum
não substitui o scheduler global por conformar um protocol.

### 12.7 Mobilidade e captures

W prova duas propriedades em uma fronteira concorrente:

| Propriedade | Pergunta |
|---|---|
| `transferable` | o owner ou acesso exclusivo pode mudar de domínio? |
| `shareable` | referências ao mesmo valor podem ser usadas por domínios paralelos? |

Esses fatos valem para domains de execução no mesmo address space. Eles não
prometem serialization, processo remoto, device transfer, stable address ou
real-time behavior. Cada contrato adicional permanece separado.

As propriedades são independentes. `transferable` exige:

1. um owner ou acesso exclusivo;
2. ausência de alias utilizável no domain de origem;
3. fields owned também transferíveis;
4. allocator, deallocator e `deinit` válidos no destino;
5. ausência de thread-local storage ou affinity incompatível.

O destino pode mutar um owner transferido. Isso não exige synchronization,
porque o domain de origem perdeu acesso. O parent recupera o valor somente por
join ou return.

`shareable` exige:

1. storage vivo e estável até o fim de todos os uses;
2. reads concorrentes sem data race;
3. interior mutation protegida por atomicidade, serialization ou outro
   mecanismo verificado;
4. cleanup posterior ao último use concorrente.

Imutabilidade profunda é suficiente para `shareable`, mas não é necessária.
`Atomic<T>` e `ServiceRef<P>` podem ser `shareable` mesmo quando state muda.
Um buffer owned mutável pode ser `transferable` e não `shareable`.

| Capture | Prova mínima |
|---|---|
| `take value` | `transferable(value)` |
| `copy value` independente | resultado owned `transferable` |
| `copy value` que mantém alias | storage `shareable` |
| `ref value` | `shareable(value)` e lifetime dentro do scope |
| `inout value` | `transferable(value)`; parent fica bloqueado até o join |
| `view value` | owner `shareable`, descriptor válido e lifetime dentro do scope |
| `inout view value` | owner e acesso exclusivo transferíveis; parent bloqueado |
| `ServiceRef<P>` | handle `shareable`; state não cruza |

Um child que continua na mesma isolation boundary pode acessar state isolado. Um
child que sai da boundary precisa de snapshot, copy ou move. `spawn` nunca
captura state mutável de uma service instance.

Uma view não possui mobilidade independente. O descriptor é `Copy`, mas copiar
pointer e count não torna o storage seguro. Um child estruturado pode receber
`view T` quando o owner é `shareable` e permanece vivo. Uma task detached não
recebe borrow.

O compiler deriva os fatos:

| Forma | Regra |
|---|---|
| scalar, enum, tuple e struct | composição dos fields e do `deinit` |
| String, Bytes e Array<T> owned | transferíveis quando element, allocator e cleanup são |
| `ref T` | cruza a boundary somente quando `T` é shareable |
| `inout T` | pode transferir exclusividade; nunca é shareable |
| `shared T` e `weak T` | cruzam somente quando `T` é shareable e a contagem é thread-safe |
| `Atomic<T>` | shareable somente para operações atômicas suportadas |
| `ServiceRef<P>` | transferable e shareable; state permanece na instance |
| function pointer | transferable e shareable |
| closure | deriva mode e fatos de cada capture |
| `Pinned<T>` e Arena | dependem de storage, allocator, cleanup e affinity |
| raw pointer, thread-local e foreign handle | locais por default |

Um `object` não ganha `shareable` somente por ter identity. O compiler analisa
seus fields e a interface que pode executar concorrentemente. Um wrapper de
synchronization reconhecido pode publicar o fato em sua interface compilada.
Imports sem esse fato continuam locais.

O [Rust Reference](https://doc.rust-lang.org/reference/special-types-and-traits.html)
separa `Send` de `Sync` e deriva ambos estruturalmente. O
[guia de concorrência de Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/concurrency/#Sendable-Types)
combina value transfer, immutable state e state serializado sob `Sendable`. W
mantém duas provas para não confundir move exclusivo com aliases concorrentes.

**Forma vigente:** `transferable` e `shareable` são predicates intrínsecos. Código
comum não declara annotations e tipos não conformam a marker protocols
`Send`/`Sync`.

O compiler infere o predicate exigido pelo body generic e o grava na interface
do módulo. Documentation gerada mostra o contrato. Adicionar um predicate
inferido a uma API publicada é uma mudança de compatibilidade.

Uma API genérica pode fixar o requisito com o refinement do parâmetro:

```w
protocol Inspectable {
  fn inspectionCode(): u64
}

protocol Consumable {
  take fn finish(): u64
}

async fn inspectElsewhere<T: Inspectable>(
  value: ref T<(.shareable)>,
): u64 {
  spawn<.compute> let code = value.inspectionCode()
  return await code
}

async fn consumeElsewhere<T: Consumable>(
  value: take T<(.transferable)>,
): u64 {
  spawn<.compute> let code = (take value).finish()
  return await code
}
```

`.shareable` e `.transferable` são Boolean facts do subject do contrato. Eles
são avaliados no compile time, não mudam layout e não executam um check runtime.
`T<(.transferable && .shareable)>` exige os dois. A forma explícita congela a
interface; a forma omitida continua inferida.

Uma prova manual não pertence a safe W. Um binding foreign ou primitive de
synchronization pode publicar um fato de mobilidade somente por uma interface
trusted que registra target, adapter e digest. Uma assertion escrita pelo
usuário precisa de uma boundary `unsafe`; sua forma source permanece
**Pesquisa** até o corpus de FFI provar diagnostics e negative facts.

**Alternativas:** `T<mobility: .transferable>` usa um static slot nomeado.
`T: Send`, `T: Sync` e `T: Sendable` usam marker protocols públicos. As formas
ficam rejeitadas no design vigente porque permitem conformance nominal para uma propriedade
que safe W deve derivar.

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

let mixtures = try await TaskGroup.parallelMap<.compute>(
  take jobs,
  limit: 8,
  ordering: .input,
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

#### 12.8.1 Paralelismo aninhado e capacity

`limit` e domain capacity medem coisas diferentes:

- `limit` limita children ativos e seus recursos lógicos;
- domain capacity limita jobs que executam ao mesmo tempo;
- host quota limita CPU física do product.

Groups aninhados no mesmo domain compartilham o mesmo budget. Eles não criam
pools independentes nem multiplicam `outerLimit * innerLimit` threads.

**Exemplo:** duas kitchens usam `parallelMap<.compute>(limit: 8)`. Se o domain
possui capacity 6, no máximo seis jobs CPU executam ao mesmo tempo. As duas
arrays ainda podem manter até 16 children ativos conforme seus limites.

Um parent que aguarda children não retém um worker necessário para esses
children. O runtime pode liberar o permit, executar um child inline ou ajudar a
fila do mesmo domain. Blocking code não usa o compute budget.

Essa regra impede deadlock por pool exhaustion e reduz oversubscription. Ela não
promete qual child executa em cada thread. O
[task scheduler do oneTBB](https://uxlfoundation.github.io/oneTBB/main/specification/source/task_scheduler.html)
também ajusta paralelismo real à capacidade disponível e não garante que todo
trabalho potencialmente paralelo execute em paralelo.

O effective parallelism é limitado por source, domain, product e host. O
runtime publica a medição; o programa não usa o número observado como resultado
de domínio.

```text
effective ≤ source limit
effective ≤ domain capacity
effective ≤ product CPU quota
```

NUMA, core type, work stealing e pinning são policies de profile expert. Eles
não mudam `spawn`, ownership ou ordering.

### 12.9 Streams e channels

`Stream` e `Channel` resolvem problemas diferentes:

- `Stream` descreve consumo pull, assíncrono e single-pass;
- `Channel` transfere ownership entre producers e um consumer;
- uma mailbox transporta calls de service, authority, deadline e outcomes.

Um tipo não substitui os outros. O runtime pode compartilhar uma primitive de
fila entre eles sem compartilhar a semântica pública.

#### 12.9.1 Contrato de `Stream`

**Forma vigente:** `Stream<Item, Failure>` é um protocol pull com um único cursor:

```w
export protocol Stream<Item, Failure: Error> {
  mut async fn next(): Item? throws Failure
}
```

`next()` suspende até produzir um item, terminar ou falhar. Cada item owned é
movido para o consumer. `.none` termina o stream. Depois de devolver `.none` ou
lançar `Failure`, toda chamada futura devolve `.none`. Um producer que precisa
continuar depois de um erro produz `Result<Item, Failure>` como item; ele não
usa o error effect terminal.

`Failure = Never` torna o stream nonthrowing. A especialização elimina `try`:

```w
async fn announce<S: Stream<Announcement, Never>>(source: take S) {
  var announcements = take source

  for await announcement in announcements {
    print(announcement.title)
  }
}
```

Um stream fallible usa a ordem canônica `for try await`:

```w
async fn audit<S: Stream<AuditEvent, AuditError>>(
  source: take S,
): () throws AuditError {
  var events = take source

  for try await event in events {
    inspect(event)
  }
}
```

O loop obtém acesso exclusivo ao cursor durante cada chamada a `next()`. A
forma explícita é equivalente:

```w
while let event = try await events.next() {
  inspect(event)
}
```

Um `break` deixa um stream nomeado parcialmente consumido. Destruir um stream
temporário ou deixar seu owner sair de escopo solicita cancelamento ao producer.
Não existe uma chamada implícita a `await` no destructor. Um producer
estruturado pertence ao scope que criou o stream; esse scope não termina antes
de o producer concluir seu cleanup. Um tipo que exige close assíncrono também
oferece uma operação consuming própria, usada com `defer async`.

Esta forma mantém uma única abstração pública. Adapters como `map`, `filter` e
`take` devolvem `some Stream<..., ...>`. Eles não exigem classes públicas como
`AsyncMapStream`. `any Stream<..., ...>` continua uma erasure explícita, com
indirection e possível allocation.

A proposta de
[AsyncSequence de Swift](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0298-asyncsequence.md)
também usa `next() async throws -> Element?`, torna o fim estável e preserva os
tipos concretos dos adapters para permitir otimização. W remove a separação
obrigatória entre sequence e iterator: `Stream` é sempre single-pass.

#### 12.9.2 Views em streams

`view` não é um tipo utilitário chamado `StringView`. Ele é o access mode
genérico de uma projeção borrowed. Por isso, um stream pode declarar um item
borrowed:

```w
async fn indexMenu<E: Error>(
  source: take some Stream<view String, E>,
): () throws E {
  var lines = take source

  for try await line in lines {
    index(line)
  }
}
```

Neste caso, a interface registra o stream como origem da view. A view pode viver
durante o body da iteração. Enquanto ela está viva, o programa não chama
`next()` de novo se essa chamada puder reutilizar ou alterar o mesmo storage.
Ela não escapa para uma task detached, um channel ou um owner com lifetime
maior.

Esta regra preserva três conceitos:

| Forma | Promessa |
|---|---|
| `ref T` | leitura do place completo |
| `view T` | leitura de uma projeção com provenance |
| `let value: T` profundamente imutável | fato inferido quando todo o grafo permite a prova |

W não adiciona `Readonly<T>`, `Immutable<T>`, `StringView` ou `ArrayView`.
Interior mutation, capabilities e aliases `shared` impedem que um wrapper
universal prometa imutabilidade profunda. A seção 16.2 fecha os descriptors,
lifetimes e limites das views.

#### 12.9.3 Endpoint e topologia de `Channel`

**Forma vigente:** o channel básico é MPSC: vários senders e um receiver. A criação
devolve endpoints distintos:

```w
let (ordersOut, ordersIn) = Channel<Order>.open(capacity: 64)
```

Os tipos inferidos são:

```w
ordersOut: Channel<Order><.send>
ordersIn: Channel<Order><.receive>
```

`Channel<T>` sem o contract de endpoint aparece somente como namespace de
`open`. Não existe um valor runtime bidirecional.

`Channel<T><.send>` é um handle shareable e move-first. `copy ordersOut` cria
outro producer e torna o retain visível. `Channel<T><.receive>` é move-only,
transferable e não shareable. Assim, duas tasks não podem receber o mesmo item.

`T` precisa atender a `T<(.transferable)>`. Um payload borrowed, inclusive
`view T`, não atende ao requisito:

```w
let line: view String = command.scalars[0..<4]
let _ = Channel<view String>.open(capacity: 1) // Erro: payload borrowed.
```

Um caller materializa a projeção ou envia um owner:

```w
let (textOut, _) = Channel<String>.open(capacity: 1)
try await textOut.send(line.materialize())
```

Separar endpoints fecha authority de close e evita que um valor bidirecional
seja copiado por acidente. A
[MPSC bounded de Tokio](https://docs.rs/tokio/latest/tokio/sync/mpsc/)
também usa handles separados, backpressure e um receiver único. W acrescenta
move explícito, enum subsets e facts de mobilidade.

#### 12.9.4 Envio, recebimento e recuperação do owner

Um envio normal suspende até obter admission. Ele move o item somente no ponto
de commit:

```w
try await ordersOut.send(take order)
let received: Order? = await ordersIn.receive()
```

O SDK declara um error fechado:

```w
export enum ChannelSendError<T>: Error {
  full(T)
  closed(T)
}
```

`send` pode produzir somente `.closed`. `trySend` pode produzir os dois cases:

```w
extension<T> Channel<T><.send> {
  async fn send(
    value: take T,
  ): () throws ChannelSendError<T><[.closed]>

  fn trySend(
    value: take T,
  ): () throws ChannelSendError<T>
}

extension<T> Channel<T><.receive> {
  async fn receive(): T?
  fn close()
}
```

O error devolve ownership ao caller:

```w
do {
  try await ordersOut.send(take order)
} catch .closed(let returnedOrder) {
  storeForTomorrow(take returnedOrder)
}
```

Se `send` é cancelado antes do commit, nenhum item entra no channel. O item
permanece no frame cancelado e executa seu cleanup uma vez. Se o commit ocorreu,
o receiver possui o direito de receber ou descartar o item; o sender não observa
um falso cancelamento.

`receive()` devolve `.none` somente quando o channel está fechado e todos os
itens aceitos foram drenados. `Channel<T><.receive>` também atende a
`Stream<T, Never>`, portanto o consumer comum usa `for await`.

```w
async fn serveAll(input: take Channel<Order><.receive>) {
  var orders = take input

  for await order in orders {
    serve(take order)
  }
}
```

Cancellation de um `receive` ainda não comprometido remove o waiter e deixa o
item na fila. Depois do commit, o task frame possui o item e faz seu cleanup se
a task terminar.

#### 12.9.5 Reserva de capacity

`reserve()` aguarda admission sem construir ou mover o item:

```w
let permit = try await ordersOut.reserve()
let order = prepareSynchronously()
try (take permit).send(take order)
```

As assinaturas são:

```w
export enum ChannelClosed: Error {
  closed
}

extension<T> Channel<T><.send> {
  async fn reserve(): ChannelPermit<T> throws ChannelClosed
}

extension<T> ChannelPermit<T> {
  take fn send(
    value: take T,
  ): () throws ChannelSendError<T><[.closed]>
}
```

O item ainda não existe durante `reserve()`. Por isso, `ChannelClosed` não
carrega payload e é separado de `ChannelSendError<T>`.

`ChannelPermit<T>` é move-only. Ele representa uma vaga ou, com capacity zero,
um receiver já pareado. Destruir um permit não usado devolve a vaga. Cancellation
antes do retorno de `reserve()` remove o waiter. Cancellation depois do retorno
executa o cleanup do permit.

Um close gracioso não revoga permits já emitidos. O receiver os drena ou espera
que sejam descartados. Destruir o receiver é abortivo; nesse caso,
`permit.send` devolve o item em `.closed`.

Manter um permit através de outro `await` reduz capacity e pode criar
head-of-line blocking. O resource lens registra a duração, e o lint avisa por
default. Uma restrição de tipo que proíba suspension com permit vivo permanece
**Pesquisa**; ela precisa tratar adapters foreign e false positives.

O mecanismo segue a função dos permits documentados por
[Tokio](https://docs.rs/tokio/latest/tokio/sync/mpsc/struct.Sender.html):
cancelar a espera perde a posição na fila, e destruir o permit libera a vaga.
W mantém o item fora da espera e preserva seu ownership.

#### 12.9.6 Capacity e backpressure

**Exemplo:** `Channel<Event>.open(capacity: 64)` aceita até 64 itens ou
permits. O item 65 aguarda uma vaga.

Capacity é obrigatória:

- `capacity: 0` cria rendezvous;
- `capacity: N`, com `N > 0`, mantém no máximo `N` itens ou permits aceitos;
- o item `N + 1` aguarda capacity ou cancellation;
- o design vigente não oferece channel unbounded.

Um channel limita a fila. Ele não limita o número de tasks suspensas que tentam
enviar. `TaskGroup.limit`, budgets do execution domain e a estrutura lexical
limitam esses frames.

Capacity conta itens, não o tamanho transitivo de cada grafo. O resource lens
mostra separadamente:

- storage da fila;
- itens retidos e tamanhos conhecidos;
- payload dinâmico não mensurável estaticamente;
- senders em espera;
- permits;
- high-water mark.

Uma mailbox de service continua a primitive para quotas simultâneas de itens,
bytes e trabalho em voo. Um `WeightedChannel<T>` com função de peso permanece
**Pesquisa**. Uma callback de peso não pode ocultar allocation, erro ou custo
não determinístico.

#### 12.9.7 Ordering, fairness e cancellation

O channel usa uma fila FIFO de admission:

1. sends sequenciais pelo mesmo endpoint preservam a ordem do source;
2. sends concorrentes não possuem ordem total sem outro edge;
3. depois da admission, o receiver observa a ordem dos tickets;
4. cancelar um waiter remove seu ticket;
5. `trySend` não ultrapassa waiters já enfileirados.

Fairness promete ausência de starvation quando o receiver progride e o
scheduler atende os tasks. Ela não promete a mesma ordem entre sends
concorrentes em targets diferentes. O trace registra tickets para replay e
diagnóstico; o resultado de domínio não depende desses números.

Os pontos lineares são:

| Operação | Commit |
|---|---|
| `send` | item ocupa uma vaga ou conclui o rendezvous |
| `trySend` | chamada aceita o item |
| `reserve` | permit recebe uma vaga ou receiver |
| `receive` | ownership do item entra no consumer |
| `close` | admission de novos sends e permits é proibida |

#### 12.9.8 Close e lifetime

O lifecycle é explícito e monotônico:

```text
open → closing → drained
  └────────────→ aborted
```

- destruir o último sender inicia `closing`;
- `receiver.close()` inicia `closing` e rejeita admission nova;
- o receiver ainda obtém itens e permits já aceitos;
- depois do drain, `receive()` devolve `.none` para sempre;
- destruir o receiver causa `aborted`, descarta o buffer e acorda waiters;
- senders não possuem uma operação que fecha globalmente um channel copiado.

`receiver.close()` é idempotente. W não possui channel `nil`, send em channel
não inicializado ou panic por close duplicado.

#### 12.9.9 Memory ordering

**Exemplo:** writes que preparam `order` antes de `send` são visíveis quando
`receive` devolve esse owner.

Um commit de `send` acontece antes de o `receive` correspondente devolver o
item. O receiver observa writes sequenciadas antes do envio.

Em rendezvous, o pareamento também acontece antes de `send` concluir. Em um
buffer de capacity `C`, liberar a vaga no receive `k` acontece antes do send
`k + C` que usa essa vaga concluir. O commit de `close` acontece antes de um
receive devolver `.none`.

Estas regras seguem a função de sincronização descrita no
[memory model de Go](https://go.dev/ref/mem), mas W ainda exige ownership,
atomic ou outra forma verificada para qualquer state compartilhado fora do
payload.

#### 12.9.10 Buffering de streams

Um stream não faz prefetch por default. `buffer(capacity:)` é um adapter
explícito e bounded:

```w
let buffered = telemetry.buffer(capacity: 8)
```

O adapter cria um producer estruturado e um channel interno. O fim, o primeiro
error terminal e cancellation fecham o mesmo scope. A ordem é preservada. Um
adapter paralelo declara `limit` e `ordering: .input | .completion`, como
`TaskGroup`.

Low e high watermarks são policies de wake-up e batching. Elas não mudam a
capacity nem o ownership. A ideia histórica `stream<watermark: ...>` permanece
registrada em `Y/WIP.MD`; ela não entra na assinatura antes de um benchmark
mostrar valor portátil.

`yield` e `yield*` permanecem **Pesquisa**. Antes dessa sugar, o verifier deve
representar:

- ownership do item;
- view borrowed do frame do producer;
- close e async cleanup;
- erro terminal;
- cancellation durante suspension;
- capacity e ordering.

Um producer implementa `next()` como state machine ou usa um channel enquanto
esses contratos não estiverem fechados no IR.

#### 12.9.11 Topologias distintas

Uma opção de mode em `Channel` não deve esconder semânticas incompatíveis:

| Necessidade | Tipo candidato | Regra |
|---|---|---|
| vários producers, um consumer | `Channel<T>` | baseline MPSC |
| vários workers dividem itens | `WorkQueue<T>` | cada item vai para um worker; **Pesquisa** |
| cada subscriber recebe uma cópia | `Broadcast<T>` | exige duplicação ou sharing e lag policy; **Pesquisa** |
| observar somente o valor mais novo | `Watch<T>` | intermediários podem sumir; **Pesquisa** |
| um resultado | `Task<T, E>` | já pertence à structured concurrency |
| call local ou remota | mailbox de service | schema, authority, deadline e boundary outcome |

MPMC, broadcast e watch não são contracts como `<.parallel>` sobre a mesma API.
Eles mudam loss, ordering, close, slow-consumer policy e mobilidade.

A implementação de `Channel` pode usar ring buffer, queue segmentada, mutex ou
atomics. `lockFree` não faz parte da promessa. O profile mede throughput,
latency, contention e allocation antes de selecionar uma implementação por
target.

#### 12.9.12 Oracle

**Exemplo:** dois balcões enviam pedidos para um maître único enquanto o
scheduler cancela cada operação antes e depois do commit.

O ensaio do restaurante verifica:

- dois producers e um consumer com capacity 0, 1 e 64;
- fechamento pelo último sender e close gracioso pelo receiver;
- receiver abortivo com itens e permits pendentes;
- recuperação do item em `.full` e `.closed`;
- cancellation antes e depois de cada commit;
- FIFO por sender e ausência de ordem presumida entre senders;
- `trySend` sem bypass;
- stream owned, stream de `view String` e erro terminal;
- adapter bounded sem producer órfão;
- uma, duas e quatro worker threads;
- TSan, leak sanitizer e allocation fault injection.

O scheduler virtual explora os interleavings pequenos. Invariantes verificam
que cada item termina exatamente em um receiver, um error que o devolve ou um
cleanup. Nenhum item some entre esses estados.

### 12.10 Memory model, atomics e locks

**Forma vigente:** safe W não permite data races. `atomic`, isolation e locks
continuam mecanismos explícitos.

#### 12.10.1 Data race e happens-before

Duas operações formam uma data race quando estas condições são verdadeiras:

1. elas acessam bytes sobrepostos;
2. ao menos uma operação escreve;
3. as operações podem executar concorrentemente;
4. nenhum edge de happens-before ordena as operações;
5. o storage não usa uma operação atômica compatível.

Um programa safe com data-race freedom observa sequential consistency para
acessos comuns. Uma order atômica mais fraca reduz somente as garantias
declaradas nessa operação. Race conditions de domínio ainda podem existir.

```w
var served: u64 = 0

spawn<.compute> let left = countLeft(inout served)  // Erro: write concorrente.
spawn<.compute> let right = countRight(inout served)
```

W cria edges de happens-before nestas operações:

| Origem | Destino |
|---|---|
| initialization e capture/transfer do parent | início do child |
| conclusão do child | retorno de `await` ou join |
| commit de `Channel.send` | `receive` que obtém o item |
| `Channel.receive` que libera uma vaga | próximo `send` que usa essa vaga |
| commit de `Channel.close` | `receive` que observa `.none` |
| envio de call por `ServiceRef` | início do turn que recebe o payload |
| unlock | próxima aquisição do mesmo lock |
| atomic release | atomic acquire que observa essa release |

Mover um valor pelo channel não exige atomic dentro do valor. O sender perde
ownership, e o receiver recebe o owner depois do edge.

```w
let (ordersOut, ordersIn) = Channel<Order>.open(capacity: 1)
try await ordersOut.send(take order)
let ownedOrder = await ordersIn.receive()
```

Cancelamento não publica user state por si só. Um programa usa join, channel,
service, lock ou atomic quando precisa publicar state.

Uma data race dentro de `unsafe` viola o contrato de safety. O optimizer não
preserva resultado para esse source. Um sanitizer profile deve detectar a race
quando o target oferece suporte.

W usa o modelo C++20 adotado pelo
[guia de atomics do LLVM](https://llvm.org/docs/Atomics.html) como base de
lowering. A linguagem remove orders inválidas da superfície safe. W também
separa `volatile`, atomics e synchronization.

#### 12.10.2 Storage `atomic`

`var atomic value: T` é o sugar comum para storage `Atomic<T>`:

```w
var atomic completed: u64 = 0

let before = completed // load sequential
completed = 1          // store sequential
completed += 1         // read-modify-write sequential e checked
```

`atomic` é um storage modifier contextual. Ele não é um property behavior.
Somente `var` aceita esse modifier. `var atomic Lazy value` e outras composições
são inválidas até existir um behavior composto com semântica própria.

O acesso comum usa sequential consistency. O compiler rejeita uma expressão que
parece atômica, mas separa load e store:

```w
completed = completed + 1
// Erro: esta forma contém load e store separados. Use `+=` ou uma operação
// nomeada.
```

`ref completed` produz `ref Atomic<u64>`. Ele nunca produz `ref u64`. O payload
não recebe `ref` ou `inout` enquanto aliases concorrentes podem existir.

Um caller com `inout Atomic<T>` prova exclusividade. Ele pode usar
`withExclusive` antes de publicar o valor:

```w
fn resetBeforePublication(counter: inout Atomic<u64>) {
  counter.withExclusive((value: inout u64) => value = 0)
}
```

A closure não pode devolver `ref` ou `view` do payload. `(take counter).intoValue()`
consome o wrapper e devolve o payload. `Atomic<T>` não é `Copy` nem `Duplicable`.

O compiler possui um fato intrínseco `atomicValue`. Ele não é um protocol que
user code pode implementar. A baseline aceita:

- `Bool`;
- integers com largura fixa;
- `usize` e `isize`;
- enums sem payload com representação canônica suportada.

Float, struct com padding, owner e pointer não entram na baseline. Atomics de
address, `shared T` e palavras duplas continuam **Pesquisa** em APIs próprias.
Essa separação evita bitwise equality, reclamation e deallocator ocultos.

```w
enum SignState {
  dark
  announcing
  closed
}

var atomic sign: SignState = .dark
```

#### 12.10.3 Orders como contratos estáticos

W oferece estas orders:

```w
enum MemoryOrder {
  relaxed
  acquire
  release
  acquireRelease
  sequential
}

alias LoadOrder = MemoryOrder<[.relaxed, .acquire, .sequential]>
alias StoreOrder = MemoryOrder<[.relaxed, .release, .sequential]>
alias UpdateOrder = MemoryOrder
```

A order pertence ao contrato estático da operação. O call usa `<...>`. A forma
sem contrato usa `.sequential`:

```w
let state = sign.load<.acquire>()
sign.store<.release>(.closed)
let ordinary = sign.load()
```

Essa forma evita dispatch runtime e torna uma order inválida não representável.
O uso de argumentos constantes segue o precedente das
[orders constantes do Swift Atomics](https://github.com/apple/swift-atomics/blob/main/Sources/Atomics/Types/UnsafeAtomic.swift).

| Order | Garantia |
|---|---|
| `.relaxed` | atomicidade e modification order somente para aquele storage |
| `.acquire` | operações seguintes observam dados publicados pela release lida |
| `.release` | operações anteriores são publicadas para um acquire correspondente |
| `.acquireRelease` | combina acquire e release numa read-modify-write |
| `.sequential` | adiciona a operação à ordem total dos atomics sequential |

`consume` não entra no design vigente. Memory scopes de GPU e device também não entram no
core. Eles pertencem aos contratos T2 de device.

Fences soltas permanecem **Pesquisa**. O programa deve preferir uma order na
operação que publica ou consome o valor. Isso mantém o edge visível no source.

#### 12.10.4 Exchange, comparação e aritmética

`exchange` troca o valor e devolve o valor anterior. `compareExchange` devolve
um enum, não um Boolean sem diagnóstico:

```w
enum AtomicExchange<T> {
  exchanged(previous: T)
  mismatch(actual: T)
}

let result = sign.compareExchange<
  success: .acquireRelease,
  failure: .acquire,
>(
  expected: .announcing,
  desired: .closed,
)
```

A failure order aceita somente `LoadOrder`. Ela não pode ser mais forte que a
success order. O compiler verifica a relação porque ambas são argumentos
estáticos.

`weakCompareExchange` pode devolver `.mismatch(actual: expected)` sem uma
mudança concorrente. Ele serve a loops que já repetem a operação.
`compareExchange` não falha de forma espúria.

Essas regras seguem a separação de success e failure usada pela
[API atômica de Rust](https://doc.rust-lang.org/std/sync/atomic/struct.Atomic.html).
W substitui combinações inválidas em runtime por diagnostics.

Compare-exchange não resolve ABA nem reclamation. Um algoritmo que recicla
endereços usa generation suficiente, epoch, hazard pointer ou outro protocolo
declarado.

```w
// `{address, generation: 3}` pode reaparecer depois do wrap.
// O CAS não prova que o node antigo continua vivo.
```

Aritmética atômica segue a policy numérica de W. `+=` usa aritmética checked e
não faz a write quando a própria operação detecta overflow. A implementação
pode usar um CAS loop.

As famílias nomeadas preservam a mesma distinção:

| Família | Resultado |
|---|---|
| `add` e `subtract` | checked; Unit |
| `checkedAdd` e `checkedSubtract` | `Result<T, ArithmeticError>` com o novo valor |
| `wrappingAdd` e `wrappingSubtract` | wrap explícito; Unit |
| `saturatingAdd` e `saturatingSubtract` | saturação explícita; Unit |
| prefixo `fetch` | devolve também o valor anterior |

```w
completed.saturatingAdd<.relaxed>(1)
let previous = completed.fetchWrappingAdd<.relaxed>(1)
```

Bitwise integers também oferecem `and`, `or` e `xor`. W não oferece uma closure
`update` na baseline. Uma closure repetida por CAS pode duplicar side effects.

#### 12.10.5 Lock-free, layout e ABI

`Atomic<T>` garante atomicidade. Ele não garante uma instrução lock-free. O
target pode usar uma operação nativa, um runtime portátil ou um lock interno
sem allocation por operação.

```w
let native: Bool = Atomic<u64>.isLockFree
let sequence = Atomic<u64, lockFree: true>(0)
```

`isLockFree` é um valor compile-time do target e profile. `lockFree: true`
rejeita o build quando a garantia não existe. Signal handlers e outros contexts
que não podem bloquear exigem esse contrato.

O layout de `Atomic<T>` é opaco. Ele não é ABI-compatible com `_Atomic(T)` de C.
Uma fronteira C usa um wrapper gerado e a metadata de atomic width, alignment e
lock-freedom.

Atomicidade cobre a palavra inteira escolhida. O programa não pode acessar os
mesmos bytes por uma view atômica e outra não atômica. Uma palavra maior também
não herda lock-freedom das partes.

#### 12.10.6 Locks síncronos e assíncronos

T1 oferece `Mutex<T>` para code síncrono e `AsyncMutex<T>` para tasks. Ambos
protegem o payload e expõem uma closure scoped:

```w
let snapshot = ledger.withLock(
  (value: ref ApologyLedgerState) => copy value,
)

let receipt = await asyncLedger.withLock((value: inout ApologyLedgerState) => {
  value.record(order)
  return copy value.lastReceipt
})
```

`Mutex.withLock` pode bloquear a thread. Um executor cooperativo exige um domain
que aceite blocking ou usa `AsyncMutex`. `tryWithLock` nunca bloqueia e devolve
`Option<R>`.

A interface compilada marca `Mutex.withLock` com o fato `blocking`. O verifier
rejeita uma call alcançável num domain non-blocking sem adapter:

```w
spawn<.blocking> let snapshot = ledger.withLock(
  (value: ref ApologyLedgerState) => copy value,
)
```

`AsyncMutex.withLock` pode suspender durante a aquisição. A closure protegida é
síncrona e não pode usar `await`. Ela também não pode devolver um borrow do
payload.

Cancelamento durante a espera remove o waiter. Depois da aquisição, a closure
termina e libera o lock antes de observar cancelamento. Return e `throw` também
liberam o lock.

W não publica um guard na baseline. A closure reduz escape, lock esquecido e
lock mantido durante suspension. Essa escolha aplica a intenção histórica de
`property.use`, mas torna o custo explícito no owner `Mutex`.

W não usa poisoning. Um panic termina a fault boundary física. Nenhum caller W
continua nessa boundary para observar state possivelmente parcial.

O [mutex assíncrono de Tokio](https://docs.rs/tokio/latest/tokio/sync/struct.Mutex.html)
mostra o custo adicional da aquisição async e o risco de guards através de
`await`. W aceita a aquisição async, mas fecha a critical section antes de outra
suspension.

`RwLock`, condition, once, barrier e atomic wait/notify permanecem T1 em
**Pesquisa**. Cada tipo precisa fechar fairness, cancellation e failure. RCU,
snapshot swap e striped counters também permanecem tipos especializados.

Services, channels e immutable snapshots lideram para state de domínio. Um
service serial não precisa de atomic ou lock para seu state interno:

```w
service DiningRoom {
  var served: u64 = 0 // O closed turn fornece isolation.
}
```

#### 12.10.7 Diagnostics e gate

O compiler rejeita:

- borrow comum do payload atômico;
- order incompatível com a operação;
- failure order mais forte que success;
- `await` numa closure protegida;
- retorno de borrow protegido;
- acesso atômico e não atômico ao mesmo storage.

`w explain synchronization` informa storage, order, happens-before edges,
lock-freedom, fallback e suspension. TSan profiles verificam o programa depois
do lowering.

```text
$ w explain synchronization restaurant.synchronization::EndOfUniverseSign.state
storage:   Atomic<SignState>
order:     release on publish; acquire on observe
lock-free: true for target x86_64
edge:      successful publish → observe that reads `.announcing`
```

Deadlock de locks dinâmicos não é um problema decidível em geral. O compiler
detecta reacquisition lexical do mesmo lock e cycles estáticos no call graph. O
runtime trace registra wait-for edges para os demais casos.

### 12.11 FFI, blocking calls e callbacks

**Exemplo:** um importer marca `read()` como blocking e exige um blocking adapter
quando a call ocorre em executor cooperativo.

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

**Exemplo:** `async let stock = reserve()` vira um child com parent, cancel edge,
join e drop registrados antes do lowering backend.

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

O profile portátil publica fairness condicional. Sob estas premissas:

1. o número de tasks runnable permanece bounded;
2. cada job retorna ao scheduler, suspende ou termina em tempo finito;
3. o executor e o host continuam saudáveis.

Uma task admitida e acordada executa novamente. O profile não promete um
intervalo máximo.

Nenhuma ordem relativa entre siblings é garantida. `Task.yield()` é um
suspension point e uma hint de fairness; não é barrier nem coloca a task no fim
de uma fila observável.

**Exemplo:** um loop async sem `await` ou `Task.yield()` pode impedir progresso
num executor cooperativo. O compiler avisa e sugere `spawn<.compute>` ou um
suspension point explícito.

A garantia segue as mesmas premissas bounded e non-blocking documentadas pelo
[runtime Tokio](https://docs.rs/tokio/latest/tokio/runtime/#detailed-runtime-behavior).
W grava as premissas no profile e testa starvation com scheduler virtual. Um
profile real-time precisa publicar bounds mais fortes e usar adapters próprios.

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

**Forma vigente:** cada instance usa um turn serial e fechado. Um handler externo
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

Um `entry` descreve como funções W ocupam slots de um host profile. Ele não
declara um executável, abre uma porta ou cria uma thread.

Forma curta:

```w
entry {
  print("Hello, final service window")
}
```

Ela cria um handler anônimo. O product precisa escolher um host profile com um
único slot default. O body ignora os parâmetros desse slot.

Uma função normal pode ocupar o slot default sem repetir seu nome:

```w
entry(run) {
  process.stdinLine = readCommand
  process.signal = handleSignal
}
```

Esse descriptor anônimo também fornece a base local para descriptors nomeados:

```w
entry(run) {
  process.signal = handleSignal
}

entry LastLightLineHost {
  process.stdinLine = readCommand
}

entry LastLightTui(runTui)
```

O resultado expandido é:

```text
.default       = { default slot: run,    process.signal: handleSignal }
LastLightLineHost
               = { default slot: run,    process.signal: handleSignal,
                   process.stdinLine: readCommand }
LastLightTui   = { default slot: runTui, process.signal: handleSignal }
```

`entry Name(handler)` substitui somente o binding do slot default. O body
adiciona ou substitui bindings qualificados. Dois bindings para o mesmo slot no
mesmo descriptor são erro.

Existe no máximo um descriptor anônimo por módulo. Somente descriptors nomeados
do mesmo módulo recebem sua base. Imports não combinam defaults. Um binding
herdado que não existe no host profile escolhido produz erro; o compiler não o
remove silenciosamente.

O product escolhe `.default` ou um nome. Quando ele omite `entry`, o module
precisa conter somente um descriptor resolvível. A interface compilada grava o
descriptor expandido. `w explain product` mostra cada binding e sua origem.

Bindings não usam vírgula. O header sem body é válido:

```w
entry LastLightSimulation(runSimulation)
```

Slots são símbolos tipados e versionados do profile. O build escolhe um
descriptor por product. Importar o módulo não registra nem executa o entry.

`Context` é uma capability tipada. Ele não é um mapa universal de environment.

#### 13.2.1 Entry, product e modo de execução

Um descriptor selecionado é uma escolha de link. Ele não é um modo escolhido
automaticamente no runtime.

Um único artifact pode servir CLI, TUI e HTTP quando o host profile contém os
slots necessários. Em um processo nativo, `process.main` normalmente interpreta
os argumentos e inicia os adapters selecionados:

```w
async fn run(args: ProcessArguments, ctx: ProcessContext): ExitCode throws AppError {
  return switch try LaunchMode.parse(args) {
    case .cli: try await runConsole(ctx, mode: .plain)
    case .tui: try await runConsole(ctx, mode: .ansi)
    case .serve(let address): try await serveHttp(address, ctx: ctx)
  }
}
```

O sistema operacional não chama `http.fetch`. Esse slot pertence a um host que
despacha requests, como um component host ou um adapter embutido. Um executável
nativo que abre seu próprio socket usa `process.main` e uma API de servidor.

O mesmo package pode gerar artifacts distintos:

```text
last-light-native  -> entry .default -> native-process@1
last-light-worker  -> entry LastLightWorker -> http-worker@1
last-light-sim     -> entry LastLightSimulation -> native-process@1
```

Esses products podem compartilhar todos os módulos de domínio. Cada artifact
mantém seu próprio grafo alcançável, target, recipe e digest.

**Alternativa:** exigir todos os bindings em cada descriptor elimina a base
anônima, mas repete shutdown, telemetry e lifecycle hooks.

**Alternativa:** selecionar vários descriptors no runtime torna o artifact mais
dinâmico. O design vigente prefere um descriptor expandido por product e um
handler explícito para modos runtime.

### 13.3 Unidade lógica e packing físico

**Exemplo:** duas instances de `DiningRoom` podem compartilhar um processo ou
usar processos distintos sem mudar `ServiceRef<DiningApi>`.

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

#### 13.3.1 Nanoservices

W usa “nanoservice” como nome de arquitetura, não como novo tipo da linguagem.
Uma service pequena continua sendo uma instance tipada. O build seleciona um
packing declarado. O runtime executa as units resultantes.

O objetivo é combinar:

- fronteiras lógicas finas;
- bindings por capability;
- calls locais próximas do custo de uma função;
- placement remoto sem trocar a interface;
- identity, quotas e traces por instance;
- deployment independente quando necessário.

O modelo segue uma ideia demonstrada pelo workerd: services podem ser
independentes no grafo e co-localizadas na mesma thread ou no mesmo processo.
Bindings explícitos também reduzem autoridade ambiente e risco de SSRF.

W não copia o runtime JavaScript nem torna toda call remota transparente. Uma
`ServiceRef` preserva `await`, failure boundary, cancellation, deadline,
admission e `unknownOutcome`. O local fast path pode remover serialização. Ele
não remove esses efeitos.

O grafo lógico pode ter um packing de processo único:

```text
single-process/main
  ├─ Restaurant
  ├─ Billing
  ├─ Oracle
  └─ DiningRoom
```

Outra recipe pode materializar o mesmo grafo como units:

```text
split-services/gateway  -> Restaurant
split-services/planning -> Oracle
split-services/finance  -> Billing
split-services/dining   -> DiningRoom
```

O deployment coloca essas units prebuilt em hosts. Ele não separa um executable
já ligado. O source muda somente quando authority, consistency ou effects
mudam. Trocar o packing ou o placement não pode converter uma call síncrona
comum em rede silenciosamente.

O workerd avisa que seu processo isolado não é, sozinho, um sandbox para código
malicioso. W mantém a mesma separação: packing fino e capability bindings
reduzem superfície; isolamento adversarial exige boundary física adequada.

Fonte primária:
[workerd — design e nanoservices](https://github.com/cloudflare/workerd).

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

Uma service keyed declara sua identity quando o body precisa da key:

```w
export service OrderCoordinator as OrderCoordinatorApi {
  identity: ServiceIdentity<OrderId>

  async fn submit(order: take Order): OrderAccepted throws OrderError {
    guard order.id == identity.key else {
      throw .wrongInstance(expected: identity.key, found: order.id)
    }

    // ...
  }

  async fn status(): StageSnapshot throws OrderError {
    // A ServiceRef keyed já selecionou identity.key.
    // ...
  }
}
```

`ServiceIdentity<K>` é um valor read-only injetado pelo instance manager. Ele
contém key, instance ID e generation. Ele não concede authority e não expõe
storage da instance.

O descriptor keyed precisa usar o mesmo `K`. Uma service `.process`, `.request`
ou `.deployment` pode declarar `ServiceIdentity<()>` quando precisa de instance
ID e generation sem key de domínio.

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

### 13.7 Trabalho runtime-owned e `SupervisorRef`

**Exemplo:** o handler aceita um pedido e retorna. O preparo continua sob um
owner runtime. Outro turn consulta o progresso ou solicita cancelamento.

**Forma vigente:** `Task<T, E>` permanece lexical. Ela nunca escapa por `return`,
drop, `spawn` ou `await`. Trabalho que ultrapassa o caller usa uma capability
explícita `SupervisorRef`.

| Unidade | Owner | Lifetime | Uso |
|---|---|---|---|
| `Task<T, E>` | scope lexical | termina antes do scope | subtarefa estruturada |
| trabalho supervisionado | supervisor runtime | pode ultrapassar a call | operação longa identificável |
| service instance | instance manager | depende do deployment | estado e calls serializadas |

Um supervisor não é um executor. Ele possui roots, admission, shutdown,
identity e outcomes. O execution domain continua responsável por placement.

`SupervisorRef` não é global. Um `Context`, service field ou argumento concede a
capability. Importar o tipo ou a operação não cria o supervisor.

#### 13.7.1 Identidade, estado e rights

Os tipos conceituais do runtime são:

```w
export enum WorkRight {
  observe
  cancel
  signal
}

export enum WorkState {
  queued
  running
  waiting
  succeeded
  failed
  canceled
  boundaryFailed
}

export struct WorkSnapshot<Progress> {
  id: WorkId
  revision: u64
  attempt: u32
  state: WorkState
  progress: Progress?
  cancellation: Cancellation?
  suspension: WorkSuspension?
}

export enum WorkOutcome<Output, Failure: Error> {
  success(Output)
  error(Failure)
  canceled(Cancellation)
  boundary(WorkBoundaryFailure)
}

export enum WorkBoundaryFailure {
  fault
  generationLost
  restartLimit
  operationUnavailable
  durability
  unknownOutcome(EffectId)
  historyMismatch(WorkflowPointId)
  historyLimit
}

export enum WorkCancelResult<Progress> {
  requested(WorkSnapshot<Progress>)
  alreadyRequested(WorkSnapshot<Progress>)
  terminal(WorkSnapshot<Progress>)
}

export enum WorkLookupError: Error {
  unknown
  outcomeExpired(WorkId)
}

export enum SupervisorFailure: Error {
  unavailable
  unauthorized
  incompatibleSchema
  unknownOutcome(EffectId)
}
```

`WorkId` é um identificador opaco. Ele não concede authority. Sua identidade
conceitual é:

```text
WorkId = (supervisor instance, logical key, incarnation)
Attempt = restart ordinal within the same incarnation
```

`EffectId` também é opaco. Ele identifica uma tentativa de efeito para
reconciliação e deduplication. Ele não concede authority. O trace mantém o
diagnóstico detalhado de cada `WorkBoundaryFailure`; o enum mantém somente a
classificação estável.

O runtime nunca usa um process hash como identity persistente. Um adapter local
pode indexar pelo hash, mas confirma o key completo. Um adapter durável usa o
codec canônico declarado pelo descriptor.

Authority possui três níveis:

| Capability | Alcance |
|---|---|
| `SupervisorRef<K, I, P, O, E>` | namespace de keys do supervisor |
| `WorkKeyRef<K, I, P, O, E>` | uma key lógica e suas incarnations |
| `WorkRef<P, O, E><[rights]>` | uma incarnation |

`SupervisorRef.at(key:)` atenua authority e cria `WorkKeyRef`. Um descriptor
keyed pode injetar essa forma com a key de `ServiceIdentity<K>`. O service não
recebe acesso às outras keys.

`WorkKeyRef.observe()` cria `WorkRef<P, O, E><[.observe]>`.
`WorkKeyRef.control()` cria `WorkRef<P, O, E><[.observe, .cancel]>`. As duas
formas expõem `snapshot` e `outcome`. Somente a segunda expõe `cancel`.
`WorkKeyRef.signals()` cria `WorkRef<P, O, E><[.observe, .signal]>`. Essa forma
pode enviar somente os event bindings declarados pelo supervisor.
`SupervisorRef` mantém authority administrativa mesmo quando nenhum `WorkRef`
foi delegado.

```w
let orderWork = fulfillment.at(key: orderId)
let observer: WorkRef<ServiceStage, Receipt, RestaurantError><[.observe]> =
  try await orderWork.observe()
let controller: WorkRef<ServiceStage, Receipt, RestaurantError><[.observe, .cancel]> =
  try await orderWork.control()
let signaler: WorkRef<ServiceStage, Receipt, RestaurantError><[.observe, .signal]> =
  try await orderWork.signals()
```

Handles são move-first e shareable. `copy` torna o retain visível. Copiar um
observer não copia o trabalho nem seu outcome.

`state`, `progress`, `cancellation` e `suspension` são eixos separados. O estado
`.waiting` informa que o root espera retry, timer ou evento sem manter um task
frame ativo. O último progress permanece no snapshot depois do estado terminal.
Snapshots são revisionados. Eles podem ficar antigos depois do retorno. O
caller usa `revision` quando uma operação depende do estado observado.

#### 13.7.2 Operação fechada e input explícito

Um supervisor possui um único contrato de operação. O alias reduz a forma no
source:

```w
export alias FulfillmentSupervisor = SupervisorRef<
  OrderId,
  FulfillmentInput,
  ServiceStage,
  Receipt,
  RestaurantError,
>

export alias FulfillmentKey = WorkKeyRef<
  OrderId,
  FulfillmentInput,
  ServiceStage,
  Receipt,
  RestaurantError,
>
```

Os parâmetros representam key, input, progress, output e failure. O descriptor
do product liga uma função com esta forma:

```w
package async fn fulfillOrder(
  input: take FulfillmentInput,
  work: WorkContext<ServiceStage>,
): Receipt throws RestaurantError {
  let orderId = input.order.id
  work.report(.accepted)
  Task.checkCancellation()

  let pantry = try await work.services.get(pantryService)
  let ovens = try await work.services.get(ovenService)
  let oracle = try await work.services.get(oracleService)
  let probe = try await work.services.get(aromaProbeService)
  let billing = try await work.services.get(billingService)
  let diningRoom = try await work.services.get(diningRoomService)

  work.report(.reserving)
  Task.checkCancellation()
  work.report(.preparing)
  let dish = try await prepareDish(
    take input.order,
    pantry: pantry,
    ovens: ovens,
    oracle: oracle,
    probe: probe,
  )

  work.report(.serving)
  let amount = try quote(loadPriceTable(), course: dish.course)
  let payment = try await billing.capture(amount, idempotencyKey: paymentKey(orderId))
  let proof = servingProof(payment)
  let refundIdempotencyKey = refundKey(payment.id)
  var completed = false

  defer async {
    if !completed {
      do {
        let _ = try await billing.refund(take payment, idempotencyKey: refundIdempotencyKey)
      } catch error {
        Trace.current.recordCleanupError(error)
      }
    }
  }

  let receipt = try await diningRoom.serve(take dish, payment: proof)
  completed = true
  work.report(.completed)
  return receipt
}
```

`start` não recebe uma closure arbitrária. O descriptor fixa a função e sua
versão. Input, key e capabilities ficam explícitos. O compiler rejeita captures
ocultos, borrows, state de service e valores não transferíveis.

A operação pode usar visibilidade `package`. O product do mesmo package liga o
símbolo, mas um consumer não recebe uma função pública para invocá-la sem o
supervisor.

`WorkContext` contém somente contexto operacional e bindings concedidos:

- cancellation, deadline, trace e budgets;
- report de progresso;
- bindings tipados de service e host;
- adapter de durability quando declarado.

Ele não contém um mapa mutável task-local. Uma capability da aplicação precisa
de binding ou input explícito.

A operação supervisionada é root de uma nova árvore. Dentro dela, `async let`,
`spawn let` e `TaskGroup` continuam estruturados. Esses children terminam antes
do root.

#### 13.7.3 Admission e transferência de ownership

`start` e `tryStart` retornam o primeiro `WorkSnapshot<Progress>`. O uso comum
fora de um closed turn é:

```w
let orderWork = fulfillment.at(key: order.id)
let started = try await orderWork.start(input: take input)
```

O call site move o input para um admission envelope. O commit point ocorre
quando o supervisor aceita key, input, quotas e operação. Antes do commit, o
envelope não representa um root. Uma rejeição tipada devolve o input. Depois do
commit, o supervisor possui o input e o root.

`start` espera capacity. `tryStart` não espera. As falhas anteriores ao commit
devolvem o input:

```w
export enum WorkStartError<Key, Input>: Error {
  duplicate(key: Key, current: WorkId, rejected: Input)
  full(Input)
  draining(Input)
  unavailable(Input)
  unauthorized(Input)
  incompatibleSchema(Input)
}

extension<K, I, P, O, E: Error> WorkKeyRef<K, I, P, O, E> {
  async fn start(
    input: take I,
  ): WorkSnapshot<P> throws WorkStartError<K, I><[
    .duplicate,
    .draining,
    .unavailable,
    .unauthorized,
    .incompatibleSchema,
  ]>

  async fn tryStart(input: take I): WorkSnapshot<P> throws WorkStartError<K, I>
}
```

`start` não produz `.full`. `tryStart` pode produzir todos os cases. O subset do
error torna essa diferença estática. Validações que recusam a admissão devolvem
o input por `WorkStartError`. Depois que a boundary não consegue mais provar se
o commit ocorreu, o único error é `SupervisorFailure.unknownOutcome`.

```w
do {
  let started = try await orderWork.tryStart(input: take input)
  recordAcceptance(started)
} catch .full(let rejected) {
  storeForRetry(take rejected)
}
```

Cancellation antes do commit encerra o admission envelope e executa o cleanup
normal do input. Ela não cria um root. `SupervisorFailure.unknownOutcome` indica
que a boundary não consegue provar se o commit ocorreu. Nesse caso, o caller não
recebe o input e precisa reconciliar pela key e pelo effect ID.

O commit vence uma corrida com cancellation. `start` retorna o snapshot e não
informa um cancelamento pré-commit falso. Depois do commit, o root possui
cancellation e deadline próprios. Ele não continua como child lexical do
caller. O caller usa a capability de controle para solicitar cancelamento.

Uma key duplicada nunca substitui input ou trabalho. O caller decide se descarta
o novo input, consulta o trabalho existente ou usa outra incarnation.

O supervisor mantém uma tombstone depois de expirar o outcome. A tombstone
preserva key, incarnation e estado terminal dentro de um budget separado.
Enquanto ela existe, uma nova admissão ainda retorna `.duplicate`.

Depois da tombstone, reutilizar a key cria outra incarnation. Um efeito que
exige deduplication mais longa usa um adapter durável ou uma key nova. Retention
de outcome não é uma promessa eterna de idempotência.

Cancellation depois do commit não recupera input. O trabalho continua
endereçável pela key determinística. Um retry de rede usa a mesma key e encontra
`.duplicate`.

Admission possui limites de:

1. roots não terminais, inclusive `.waiting`;
2. roots em execução;
3. admission envelopes aguardando antes do commit;
4. bytes retidos nesses envelopes;
5. outcomes terminais;
6. tombstones de deduplication.

`roots` conta `.queued`, `.running` e `.waiting`. `running` conta somente
execução ativa. Assim, uma espera durável libera execution capacity sem criar
roots ilimitados.

O product fixa máximos. O deployment pode reduzir os limites dentro do envelope
declarado.

#### 13.7.4 Progresso, cancelamento e outcome

O runtime incrementa `revision` em cada mudança observável de state, progress ou
cancellation. `work.report(take progress)` substitui o último progresso. Ele não
cria uma fila ilimitada. O trace pode registrar cada mudança dentro do próprio
budget.

`report` não é um commit durável por default. O adapter durável define quais
updates participam de uma transação ou step.

O caller observa sem entrar na isolation boundary da operação:

```w
let snapshot = try await orderWork.snapshot()
let result = try await orderWork.cancel(reason: .userRequest)
```

Cancelamento é idempotente e cooperativo. O retorno distingue `requested`,
`alreadyRequested` e `terminal`. Cada case contém o snapshot após a decisão. O
campo `cancellation` registra o sinal aceito. Ele não promete rollback.

`awaitOutcome()` espera o terminal e devolve `WorkOutcome`. `outcome()` consulta
sem esperar. Application error, cancellation e boundary failure permanecem
canais distintos.

`snapshot` continua disponível enquanto existe um root, outcome ou tombstone.
`outcome` retorna `none` para trabalho não terminal, o valor retido para trabalho
terminal e `outcomeExpired` quando resta somente a tombstone. Depois da
tombstone, as operações retornam `unknown`.

Progress, output e failure observáveis precisam ser transferíveis e
duplicáveis. Um resource singular retorna uma capability ou `ServiceRef`, não o
resource bruto.

Retention é limitada por itens e bytes. Depois da expiração, a lookup retorna
`outcomeExpired`; ela não inventa um outcome. Um `WorkRef` remoto não fixa
memória indefinidamente.

#### 13.7.5 Falha, restart e shutdown

Um error `E` conclui o trabalho com `.error(E)`. Cancellation conclui com
`.canceled`. Panic ou perda da fault boundary produz `.boundary(...)` quando o
supervisor sobrevive para registrar o evento.

**Forma vigente:** restart automático de uma operação arbitrária usa `.never`. O
runtime não executa novamente efeitos desconhecidos.

Uma policy de retry exige:

- limite de attempts;
- backoff e deadline;
- step ou operação idempotente explícita;
- effect ID estável;
- regra para `unknownOutcome`;
- restart intensity limitada.

Exceder o limite produz `restartLimit`. O supervisor não reinicia para sempre.
Essa regra segue o objetivo de evitar restart loops das supervision trees de
Erlang/OTP.

Shutdown usa:

```text
ready → draining → stopped
          ├─ reject new starts
          ├─ request cancellation
          └─ wait for cleanup until deadline
```

Depois do deadline, a fault boundary pode terminar. Nesse caso, W não promete
user cleanup de frames interrompidos. O trace registra cada root não concluído.

O trace do root mantém `originCallId`, supervisor, key, incarnation e operation
version. Ele não finge que o root ainda é child lexical da call encerrada.

#### 13.7.6 Memória, steps duráveis e scheduling

O primeiro adapter é process-local e em memória. Uma queda perde roots e
outcomes não persistidos. O supervisor reiniciado usa outra generation.

Durability não serializa stack, task frame, pointer, borrow, `ServiceRef` ou
capability. Um workflow durável usa points explícitos, schemas versionados e um
journal bounded.

**Forma vigente:** `WorkContext.step`, `sleep` e `wait` formam o contrato
portátil. O uso de uma dessas operações marca o root como workflow de steps. O
compiler verifica o call graph replayable. Não existe annotation `durable` nem
uma segunda forma de `async`.

Recovery inicia a mesma operation desde o começo. O journal devolve outcomes já
confirmados e só executa um point ainda não confirmado. O source entre points
pode executar novamente. Por isso, essa parte aceita somente:

- input imutável do root;
- outcomes confirmados de points;
- constants e funções sem effects;
- collections com ordem determinística;
- branches e loops derivados desses valores.

Essa recovery não é o restart arbitrário da seção 13.7.5. `restart: .never`
continua válido. O journal, a effect policy e os limits governam a retomada.

O compiler rejeita I/O, service call, clock, random, environment, mutable global,
FFI, `spawn`, cancellation observation e mutation externa fora de `step`. Ele
infere o fact replayable pela HIR. Uma função helper não precisa de annotation.

Um step recebe input owned e uma função nominal sem capture. O `StepContext`
concede services, cancellation, trace, attempt e `effectId` somente durante a
operação:

```w
export enum StepEffect {
  repeatable
  idempotent
  transactional
  atMostOnce
}

export enum StepBackoff {
  none
  fixed(Duration<(0...)>)
  linear(
    initial: Duration<(0...)>,
    increment: Duration<(0...)>,
    maximum: Duration<(0...)>,
  )
  exponential(
    initial: Duration<(0...)>,
    factor: u16<(2...)>,
    maximum: Duration<(0...)>,
  )
}

export struct StepRetry<Failure: Error> {
  maximumAttempts: u16<(1...)>
  backoff: StepBackoff
  attemptTimeout: Duration<(0...)>?
  retryWhen: fn(ref Failure): Bool
}

extension<P> WorkContext<P> {
  async fn step<Point, Input, Output, Failure: Error>(
    _ point: Point,
    progress: P,
    succeeded: P? = .none,
    input: take Input,
    effect: const StepEffect = .atMostOnce,
    retry: const StepRetry<Failure> = .never,
    using operation: async fn(take Input, StepContext): Output throws Failure,
  ): Output throws Failure
}
```

`Point`, `Input`, `Output` e `Failure` precisam de codec canônico, schema
versionado e ownership compatível com storage. Pointer, borrow, capability,
`ServiceRef`, task e storage foreign sem codec são rejeitados. O step persiste o
input antes de liberar a operation. O caller move o input. Se ele precisa do
valor depois, preserva outro owner explícito antes da call.

`progress` é confirmado quando a tentativa começa. `succeeded`, quando presente,
é confirmado junto com o outcome de sucesso. Um replay não reaplica essas
transições nem incrementa `revision` outra vez.

O resultado ou error da operation não fica visível para o workflow antes do
commit no journal. Depois do commit, todo replay devolve o mesmo output ou lança
o mesmo application error. Uma falha do adapter vira boundary failure; ela não
é convertida para `Failure`.

Identidade usa esta composição:

```text
WorkflowPointId = (WorkId, point kind, canonical Point value)
EffectId        = (WorkflowPointId, operation semantic fingerprint)
StepAttempt     = retry ordinal within the point
```

O mesmo `EffectId` acompanha todas as step attempts do mesmo efeito lógico.
`WorkSnapshot.attempt` continua sendo o restart ordinal do root. Um enum fechado
é a forma usual de `Point`. Um loop usa um payload estável no case:

```w
export enum FulfillmentPoint {
  prepareDish
  notifySatellite(SatelliteId)
  capturePayment
  serveDish
}
```

Dois calls com o mesmo point na mesma execução lógica são um
`historyMismatch`. Input, operation ou schema diferentes para um point já
registrado também falham na boundary. O runtime não usa ordem de source, nome
textual ou contador implícito como identity.

Cada `StepEffect` possui um contrato:

| Effect | Reexecução depois de outcome incerto | Obrigação |
|---|---|---|
| `.repeatable` | permitida | effect summary não contém mutation externa |
| `.idempotent` | permitida com o mesmo `EffectId` | destino deduplica ou reconcilia a key |
| `.transactional` | resolvida pelo adapter | efeito e journal usam a mesma transação |
| `.atMostOnce` | proibida | outcome incerto termina com `unknownOutcome` |

`.atMostOnce` é o default e exige `StepRetry.never`. `.repeatable` é verificado
pelo compiler.
`.transactional` só aceita capabilities do mesmo adapter e transaction.
`.idempotent` é uma afirmação de domínio. O compiler verifica que o
`StepContext.effectId` ou uma key de domínio estável alcança o parâmetro de
idempotência quando a interface o declara. Ele não prova o comportamento do
sistema externo. `w audit effects` inclui essa afirmação, a derivação da key, a
operation e o adapter.

W não promete exactly-once para um efeito externo arbitrário. Um adapter
transactional pode garantir uma única mudança dentro da própria transação. Um
serviço externo exige idempotência ou reconciliação. Um outcome at-most-once
incerto não dispara compensação automática, pois o runtime também não sabe se o
efeito original ocorreu.

`StepRetry.never` executa uma attempt. Uma policy bounded define total de
attempts, backoff, timeout por attempt e um predicate nominal sobre application
errors. Não existe jitter aleatório escondido. Um adapter pode derivar spread
determinístico de `WorkId`, point e attempt, e precisa gravar essa decisão.
Const validation rejeita `maximum < initial` e arithmetic overflow no schedule.
`retryWhen` precisa ser uma função nominal replayable.

Um error elegível confirma a attempt e o próximo wake instant na mesma mudança
de journal. O workflow não recebe esse error intermediário. O error final vira o
outcome do point e é lançado em todo replay.
Panic, schema failure e runtime fault não entram em `retryWhen`. Eles seguem a
boundary policy do supervisor.

Timeout e cancellation não provam que um serviço remoto deixou de aplicar o
efeito. Depois de uma queda ou completion incerta:

- `.repeatable` executa outra attempt;
- `.idempotent` executa outra attempt com o mesmo `EffectId`;
- `.transactional` consulta a decisão da transação;
- `.atMostOnce` termina em `.boundary(.unknownOutcome(effectId))`.

Um outcome de step confirmado vence cancellation. Sem outcome confirmado, um
efeito at-most-once incerto vence o estado `.canceled`, pois o runtime não pode
afirmar rollback. `step`, `sleep` e `wait` observam o cancel request durável.
Cleanup de uma attempt ativa ainda segue completion drain.

Steps são sequenciais na baseline. O body de um step pode usar tasks
estruturadas e paralelismo bounded. Scheduling durável concorrente, fan-out e
race de vários steps permanecem **Pesquisa**. Essa restrição evita uma ordem de
journal dependente do scheduler.

Timers também são points. Eles não mantêm um task frame ou worker:

```w
extension<P> WorkContext<P> {
  async fn sleep<Point>(_ point: Point, for duration: Duration<(0...)>)
  async fn sleep<Point>(_ point: Point, until deadline: Instant)
}

try await work.sleep(.settleProbability, for: 2<si.s>)
```

`sleep(for:)` confirma o deadline calculado pelo adapter antes de suspender.
Replay usa esse deadline. `sleep(until:)` usa o instant fornecido. Ler clock
diretamente no workflow continua proibido; uma observação de clock variável
precisa ser output de um step. Duração zero ou deadline passado confirma um
point imediato; duração negativa não atende ao tipo.

Eventos usam um binding tipado e versionado:

```w
export struct WorkEventBinding<Payload> {
  name: String
  version: u32<(1...)>
}

export enum WaitOutcome<Payload> {
  event(Payload)
  timeout
}

export enum WorkSuspension {
  retry(point: WorkflowPointId, attempt: u32, wakeAt: Instant)
  sleep(point: WorkflowPointId, wakeAt: Instant)
  event(point: WorkflowPointId, binding: WorkEventTypeId, deadline: Instant?)
}

export enum WorkEventSendResult {
  accepted(revision: u64)
  duplicate(revision: u64)
}

export enum WorkEventSendError<Payload>: Error {
  full(Payload)
  terminal(Payload)
  unavailable(Payload)
  unauthorized(Payload)
  incompatibleSchema(Payload)
  unknownOutcome(EventId)
}

extension<P> WorkContext<P> {
  async fn wait<Point, Payload>(
    _ point: Point,
    for event: const WorkEventBinding<Payload>,
  ): Payload

  async fn wait<Point, Payload>(
    _ point: Point,
    for event: const WorkEventBinding<Payload>,
    timeout: Duration<(0...)>,
  ): WaitOutcome<Payload>

  async fn wait<Point, Payload>(
    _ point: Point,
    for event: const WorkEventBinding<Payload>,
    until deadline: Instant,
  ): WaitOutcome<Payload>
}

extension<K, I, P, O, E: Error> WorkKeyRef<K, I, P, O, E> {
  async fn send<Payload>(
    _ event: const WorkEventBinding<Payload>,
    id: EventId,
    payload: take Payload,
  ): WorkEventSendResult throws WorkEventSendError<Payload><[
    .terminal,
    .unavailable,
    .unauthorized,
    .incompatibleSchema,
    .unknownOutcome,
  ]>

  async fn trySend<Payload>(
    _ event: const WorkEventBinding<Payload>,
    id: EventId,
    payload: take Payload,
  ): WorkEventSendResult throws WorkEventSendError<Payload>
}

export enum FulfillmentSignal {
  tableReady(TableId)
  restaurantClosing
}

export const fulfillmentSignals =
  WorkEventBinding<FulfillmentSignal>(name: "fulfillment", version: 1)

let signal = try await work.wait(
  .awaitTable,
  for: fulfillmentSignals,
  timeout: 1_800<si.s>,
)
```

Binding identity inclui package, module, exported symbol, `name` e `version`.
`name` não é um registry global. O linker rejeita identity duplicada ou payload
schema incompatível no mesmo supervisor. `Payload` segue as mesmas regras de
codec e ownership de step input. O const initializer aceita 1 a 64 caracteres
ASCII lowercase, digits e `-`; o primeiro caractere precisa ser uma letra.

Sem `timeout` ou `until`, `wait` devolve `Payload`. Com limite, ele devolve
`WaitOutcome<Payload>`. Timeout é um resultado esperado, não um application
error. Cancellation continua no canal de task.

`WorkKeyRef.send` espera inbox capacity. `trySend` devolve `.full(payload)` sem
esperar. As duas recebem binding, `EventId` e payload owned. O event commit
ocorre antes do acknowledgement. O mesmo `EventId` dentro da retention devolve
`.duplicate`; ele não entrega duas vezes. Uma falha anterior ao commit devolve o
payload. Um outcome incerto exige reconciliação por `EventId`. A revision do
resultado é a `WorkSnapshot.revision` depois do event commit.

O sender cria `EventId` antes do retry. Ele pode derivar o ID de uma key de
domínio canônica ou usar uma random capability. Recriar um ID aleatório em cada
attempt elimina deduplication. `derive` usa o codec canônico, a identity do
binding, domain separation e o digest completo do profile. Ele não usa o hash
de processo nem uma forma truncada para identity.

```w
let signalId = EventId.derive(
  fulfillmentSignals,
  key: (orderId, tableRevision),
)

let result = try await fulfillment.send(
  fulfillmentSignals,
  id: signalId,
  payload: .tableReady(42),
)

switch result {
  case .accepted(let revision): traceAcceptance(revision)
  case .duplicate(let revision): traceDuplicate(revision)
}
```

Um evento pode chegar antes do `wait`. O adapter o guarda dentro do inbox
bounded. Matching usa binding e ordem de commit, não timestamp do sender. O
commit decide uma corrida entre evento, timeout e cancellation. Se timeout
vence, um evento posterior permanece disponível para outro wait.

`WorkState.waiting` cobre retry, sleep e wait. `WorkSnapshot.suspension` expõe
point, kind, wake instant ou event binding sem expor payload. Uma espera não
consome uma execution slot, mas continua contando como root não terminal no
supervisor e usa bytes de journal, inbox e outcome.
`WorkflowPointId` e `WorkEventTypeId` são IDs opacos para observabilidade. Eles
não concedem authority.

O product fecha estes limites:

1. records e bytes de history;
2. bytes de input e output por step;
3. attempts e timers pendentes;
4. events e bytes no inbox;
5. tombstones de `EventId`;
6. tempo de retention.

O supervisor grava o envelope no artifact:

```w
durability: {
  recovery: .required
  confidentiality: .hostEncrypted
  points: "restaurant.workflow::FulfillmentPoint"
  events: ["restaurant.workflow::fulfillmentSignals"]
  adapters: ["w.std/sqlite-workflow@1"]
  history: {
    recordsPerRoot: 8_192
    bytesPerRoot: 64MiB
    retainedBytes: 512MiB
  }
  step: { inputBytes: 4MiB, outputBytes: 4MiB, attempts: 8 }
  inbox: {
    itemsPerRoot: 1_024
    bytesPerRoot: 8MiB
    retainedBytes: 64MiB
    tombstonesPerRoot: 4_096
  }
  retention: { terminal: 604_800<si.s> }
}
```

`PerRoot` limita uma instance. `retainedBytes` limita o supervisor inteiro.
Exceder history produz `historyLimit`. `trySend` com inbox cheio devolve o
payload. `continueAsNew`, child workflows e compaction definida pelo usuário
permanecem **Pesquisa**. A baseline é bounded.

Cada root fixa a operation version, o semantic fingerprint, os schemas de
points e events e o adapter ABI. Um deploy novo não altera um history ativo.
Workers novos precisam manter a versão antiga. Remover a versão produz
`operationUnavailable`. Migration de workflow permanece explícita e fora da
baseline.

SQLite é o primeiro adapter oficial do produto de referência. Uma transação
guarda input, attempt, outcome, progress, timer e consumo de evento. Um outbox na
mesma transação pode publicar uma mensagem depois do commit. Ele não torna um
efeito remoto exatamente uma vez.

O adapter profile fixa journal mode, `synchronous`, checkpoint policy e
filesystem assumptions. WAL exige processos no mesmo host e não funciona sobre
um network filesystem. Essas escolhas aparecem em `w explain workflow`; W não
as converte em uma garantia SQLite universal.

Journal pode conter dados pessoais ou comerciais. Capability, secret handle e
foreign resource continuam proibidos como payload. O artifact declara a
confidentiality mínima e a retention. Diagnostics mostram schema, digest e
tamanho, não payload. O adapter e o host precisam provar encryption at rest
quando o product exige `.hostEncrypted`.

O adapter em memória implementa o mesmo oracle para teste, mas não satisfaz um
product que exige recovery depois de process failure. O deployment seleciona
somente um adapter permitido pelo artifact contract.

`w explain workflow` mostra o contrato resolvido:

```text
$ w explain workflow last-light/fulfillment --key order:42
operation:       restaurant.workflow::fulfillOrderDurably
version:         sha256:...
state:           waiting
point:           FulfillmentPoint.awaitTable
effect:          event
step attempt:    1
journal:         w.std/sqlite-workflow@1
storage profile: wal / synchronous=full / local filesystem
history:         4 records / 18 KiB
inbox:           0 events / 0 B
recovery:        required
confidentiality: host-encrypted
```

O oracle derruba o adapter antes do dispatch, depois do dispatch, antes do
outcome commit e depois do commit. Ele também cobre input divergente, point
duplicado, retry exaurido, effect at-most-once incerto, evento antecipado e
duplicado, timeout concorrente, cancellation, history cheio, schema incompatível
e operation version ausente.

Alarmes e reminders acordam uma instance no futuro. Eles não são tasks mantidas
vivas. Delivery at-least-once exige handler idempotente.

`waitUntil`-like pode existir como adapter bounded de um host. Ele serve a
cleanup curto, logs ou cache. Ele não substitui `SupervisorRef` nem um workflow
durável.

Esta separação segue evidência externa:

- [JEP 525](https://openjdk.org/jeps/525) confina subtasks ao scope;
- [Cloudflare `waitUntil`](https://developers.cloudflare.com/workers/runtime-apis/context/)
  possui lifetime limitado e recomenda queues para trabalho confiável;
- [regras de Cloudflare Workflows](https://developers.cloudflare.com/workflows/build/rules-of-workflows/)
  exigem identidade determinística e isolam side effects em steps;
- [sleep e retry de Cloudflare Workflows](https://developers.cloudflare.com/workflows/build/sleeping-and-retrying/)
  usam waits persistentes e policies bounded;
- [eventos de Cloudflare Workflows](https://developers.cloudflare.com/workflows/build/events-and-parameters/)
  aceitam envio antes do wait e payload persistido;
- [constraints de Durable Task](https://learn.microsoft.com/en-us/azure/durable-task/common/durable-task-code-constraints)
  mostram por que replay exige clock, I/O e scheduling controlados;
- [versionamento de Durable Task](https://learn.microsoft.com/en-us/azure/durable-task/common/durable-orchestration-versioning)
  fixa uma versão por instance;
- [atomic commit do SQLite](https://www.sqlite.org/atomiccommit.html) e
  [transactions](https://www.sqlite.org/lang_transaction.html) sustentam o
  primeiro journal local;
- [WAL do SQLite](https://www.sqlite.org/wal.html) delimita host, concurrency e
  checkpoint;
- [Durable Objects](https://developers.cloudflare.com/durable-objects/best-practices/rules-of-durable-objects/)
  usa identity por entidade e recomenda trabalho curto na coordination boundary;
- [Durable Object alarms](https://developers.cloudflare.com/durable-objects/api/alarms/)
  usa delivery at-least-once e retry para wakeups persistentes;
- [Orleans timers e reminders](https://learn.microsoft.com/en-us/dotnet/orleans/grains/timers-and-reminders)
  separa timers transitórios de reminders persistentes;
- [Erlang supervisors](https://www.erlang.org/doc/system/sup_princ.html)
  limitam restart intensity e distinguem children dinâmicos de estado durável.

Alternativas:

| Forma | Estado |
|---|---|
| `WorkKeyRef.start` ou `tryStart` com operação fechada | **Forma vigente** |
| `work.step` com point fechado, função nominal e effect policy | **Forma vigente** |
| `work.sleep` e `work.wait` como records duráveis | **Forma vigente** |
| steps duráveis concorrentes e child workflows | **Pesquisa** |
| channel consumido por um entry root | implementação process-local possível |
| `ctx.waitUntil(task)` | adapter bounded, não owner geral |
| `spawn<owner: ...>` | **Rejeitado por enquanto**; muda lifetime pela syntax de paralelismo |
| drop de `Task` destaca o child | **Rejeitado por enquanto**; perde ownership e cleanup |
| call one-way de service | **Rejeitado por enquanto**; perde outcome e cria ambiguidade |
| service keyed com um handler longo | oracle adversarial; ainda bloqueia controle na mesma key |
| actor reentrant durante `await` | **Pesquisa**; exige invalidação e revalidação de state |
| persistir frame async automaticamente | **Rejeitado por enquanto**; pointers, effects e upgrades não têm contrato |

### 13.8 Bindings tipados, product e deployment

Um nome textual no source não deve escolher uma service sem type-check. O source
declara um binding const:

```w
export const restaurantService = ServiceBinding<RestaurantApi>(name: "restaurant")

export const orderCoordinators = ServiceFamily<OrderCoordinatorApi, OrderId>(name: "orders")

let restaurant = try await ctx.services.get(restaurantService)
let coordinator = try await ctx.services.get(orderCoordinators, key: orderId)
```

`ServiceBinding<P>` descreve uma requirement singular. `ServiceFamily<P, K>`
descreve instances keyed. Esses valores não concedem authority sozinhos. O
`Context` precisa conter o binding.

O compiler resolve protocol, key type, visibility e schema no link. A string é
somente o nome estável no product. Lookup dinâmica por string fica em
**Pesquisa** para hosts de plugins.

Fields sem initializer em uma service são pontos de injection:

```w
export service OrderCoordinator as OrderCoordinatorApi {
  identity: ServiceIdentity<OrderId>
  fulfillment: FulfillmentKey
}
```

O product descriptor liga cada field pelo nome e pelo tipo exato. Para um
`WorkKeyRef`, ele também liga a origem da key. Falta, duplicata, cycle estático
ou authority incompatível falha no build.

#### 13.8.1 Grafo lógico do product

**Exemplo:** o product nativo fornece `last-light` e importa os controladores
físicos da despensa e dos fornos.

`package.w` declara grafos nomeados. Um product seleciona no máximo um grafo.
Não existe herança ou overlay entre grafos na primeira edição.

```w
runtimeGraphs: [
  {
    name: "restaurant-core"
    providers: [
      {
        binding: "last-light"
        protocol: "restaurant.restaurant::RestaurantApi"
        implementation: "restaurant.restaurant::LastLightRestaurant"
        scope: .process
        mailbox: { items: 64, bytes: 8MiB, inFlight: 1 }
        inject: {
          pantry: .service("pantry")
          ovens: .service("ovens")
          oracle: .service("oracle")
          probe: .service("aroma-probe")
          billing: .service("billing")
          diningRoom: .service("dining-room")
        }
      },
      {
        binding: "orders"
        protocol: "restaurant.supervision::OrderCoordinatorApi"
        implementation: "restaurant.supervision::OrderCoordinator"
        scope: .keyed(keyType: "restaurant.domain::OrderId")
        mailbox: { items: 8, bytes: 1MiB, inFlight: 1 }
        inject: {
          fulfillment: .supervisor("fulfillment", key: .serviceIdentity)
        }
      },
    ]

    imports: [
      {
        binding: "pantry"
        protocol: "restaurant.kitchen::PantryApi"
        source: .deployment
      },
      {
        binding: "ovens"
        protocol: "restaurant.kitchen::OvenApi"
        source: .deployment
      },
      {
        binding: "aroma-device"
        capability: "restaurant.hardware::AromaProbeDevice"
        source: .host
      },
    ]

    exports: ["last-light", "orders"]
  },
]
```

Um `provider` seleciona implementation, scope, isolation, quotas e injections.
Um service import declara `protocol` e key type opcional. Um host import declara
um tipo de `capability`. Um `export` torna um provider visível para composição
externa.

O compiler deriva requirements a partir do entry, das funções alcançáveis e dos
fields de injection. O manifest precisa satisfazer cada requirement com um
provider, supervisor, host capability ou import declarado. Uma string não cria
uma requirement nova.

O linker resolve protocol, implementation, key type e schema por identity. Ele
rejeita estes casos:

- provider ausente ou duplicado;
- import sem uso ou requirement não declarado;
- injection com tipo, key ou rights incompatíveis;
- ciclo estático proibido;
- capability maior que o envelope;
- símbolo textual que não corresponde à interface compilada.

Um grafo fechado não possui imports. Um grafo aberto grava seus imports na
interface do artifact. O host profile precisa permitir composição. `w run`
exige um deployment local quando um executable possui imports abertos.

Os limites pertencem ao artifact contract. Um deployment pode reduzi-los. Ele
não pode trocar operation, protocol, key type, rights ou required isolation.

#### 13.8.2 Packing de build

**Exemplo:** o mesmo grafo gera um artifact único ou um index com quatro units.

`packing` é uma decisão de build. Ele divide providers e supervisors em artifact
units. O nome e a expansão do packing entram na recipe.

O product seleciona um packing default. `--packing` pode escolher outro packing
do mesmo grafo. Se o grafo possui uma opção, o build pode inferi-la.

```w
packings: [
  {
    name: "single-process"
    units: [
      {
        name: "main"
        entry: true
        providers: [
          "last-light",
          "orders",
          "oracle",
          "aroma-probe",
          "billing",
          "dining-room",
        ]
        supervisors: ["fulfillment"]
      },
    ]
  },
  {
    name: "split-services"
    units: [
      {
        name: "gateway"
        entry: true
        providers: ["last-light", "orders"]
        supervisors: ["fulfillment"]
      },
      { name: "planning", providers: ["oracle", "aroma-probe"] },
      { name: "finance", providers: ["billing"] },
      { name: "dining", providers: ["dining-room"] },
    ]
  },
]
```

Cada provider e supervisor aparece em uma unit. Um product com entry marca uma
unit com `entry: true`. Uma unit sem entry continua válida quando publica um
provider no artifact index. O instance manager inicia essa unit.

O packer deriva interfaces privadas entre units a partir de injections e
requirements do supervisor. Essas interfaces entram no artifact index. Elas não
tornam o provider um export público do runtime graph.

Cada edge privada fixa a unit de origem, a unit provedora, o binding, o protocol
e o key type. O deployment roteia essa edge entre os placements selecionados.
Ele não pode religá-la a outro provider. Somente um import aberto do runtime
graph recebe um provider no manifest de deployment.

Uma edge entre units usa a service ABI. Ela preserva `await`, schemas, quotas,
ordering, cancellation, failures e trace. Uma call normal, um borrow ou mutable
state compartilhado não pode cruzar essa edge.

`single-process` pode gerar um executable ligado. `split-services` gera um
artifact index e um payload por unit. Os dois resultados possuem digests
distintos. Um deployment não extrai services de um executable já ligado.

Static libraries, objects, MLIR e Wasm Components podem materializar uma unit.
Essa escolha não muda a semântica da linguagem. Um módulo não vira sandbox só
porque o build produziu uma static library intermediária.

#### 13.8.3 Manifest e lock de deployment

**Exemplo:** o plano local seleciona uma recipe. O lock grava os digests
resultantes.

```w
deployment {
  schema: "w.deployment/1"
  name: "last-light/local"

  artifacts: [
    {
      name: "restaurant"
      source: .product(
        "last-light-native",
        target: "x86_64-unknown-linux-gnu",
        profile: "debug",
        packing: "single-process",
      )
    },
  ]

  placement: [
    { unit: "restaurant/main", host: .local },
  ]

  bindings: [
    {
      import: "restaurant/pantry"
      provider: .adapter("last-light.dev/pantry@1")
    },
    {
      import: "restaurant/ovens"
      provider: .adapter("last-light.dev/ovens@1")
    },
  ]

  adapters: [
    {
      artifact: "restaurant"
      supervisor: "fulfillment"
      role: .workflowJournal
      provider: .adapter(
        "w.std/sqlite-workflow@1",
        storage: .capability("last-light/workflow-store"),
      )
    },
  ]

  limits: {
    supervisors: [
      {
        artifact: "restaurant"
        binding: "fulfillment"
        roots: 256
        running: 8
        admissionQueued: 32
      },
    ]
  }
}
```

`deployment.w` é um plano data-only. `.product(...)` referencia uma recipe
reproduzível. `w deploy resolve` grava cada artifact e unit por digest em
`deployment.lock`.

`.release(...)` referencia um artifact publicado por package, product, target e
version. O resolver fixa seu release index e seus payloads. Ele não recompila
essa release durante deploy.

`w deploy apply --locked` não executa build. Ele rejeita um source plan que
difere do lock. Production não aceita placeholder, tag mutável ou product sem
digest resolvido.

O deployment pode:

- colocar units prebuilt em hosts;
- rotear edges privadas já fixadas pelo packing;
- conectar imports abertos a exports compatíveis;
- selecionar adapters permitidos;
- reduzir quotas;
- referenciar secrets por capability.

Uma seleção por `role` satisfaz uma requirement já gravada no artifact. O
adapter precisa ter ABI, durability, confidentiality e target compatibility
iguais ou maiores. Um journal em memória não satisfaz `recovery: .required`.
Storage sem encryption at rest não satisfaz `.hostEncrypted`. O deployment não
pode reduzir essas garantias.

O deployment não pode:

- reagrupar providers;
- religar uma edge privada a outro provider;
- trocar código, protocol, operation ou target;
- aumentar o envelope;
- converter uma call normal em uma service call;
- conceder uma capability ausente no artifact.

Secrets não entram em `package.w`, artifact ou deployment lock. O plano
referencia uma capability do host. O runtime entrega um handle.

Artifact, packing, deployment e adapter digests aparecem em trace, audit e
crash report. Duas instalações podem usar os mesmos bytes e configurações
distintas sem perder observabilidade.

Fontes primárias:

- [Wasm Components e composição por imports/exports](https://component-model.bytecodealliance.org/design/components.html);
- [WIT worlds como contracts de imports e exports](https://component-model.bytecodealliance.org/design/worlds.html);
- [OCI manifests e indexes content-addressed](https://github.com/opencontainers/image-spec/blob/main/manifest.md).

#### 13.8.4 Versionamento e rolling update

**Exemplo:** um root iniciado com `fulfillOrder@v3` termina nessa versão. O
deploy de `v4` recebe somente starts novos.

Cada root fixa:

- operation identity e semantic fingerprint;
- input, progress, output e failure schemas;
- supervisor generation;
- deployment digest inicial.

Um deploy novo não troca o body de um root ativo. Ele drena a operation antiga,
inicia work novo na versão nova ou executa uma migration explícita.

Um adapter durável preserva a versão necessária para retomar steps. Remover essa
versão exige concluir, cancelar ou migrar cada root.

Service protocols usam compatibilidade de interface e wire schema. Co-location
não permite ignorar essa verificação durante rolling update.

### 13.9 Estado durável e gates

**Exemplo:** o adapter SQLite confirma a transação antes de liberar a resposta.
Uma falha descarta o output retido.

Durability é um adapter explícito. Um handler declara transação, commit point e
a relação entre state e outputs. A baseline não presume state durável.

SQLite é o primeiro adapter oficial provável. Ele oferece transações, operação
local e portabilidade. Ele não é a semântica universal. Memory, files, remote KV
e engines especializadas podem implementar o contrato.

**Forma vigente para workflows:** o adapter confirma input, outcome e progress
do step antes de liberar o resultado para o código replayable. Um outbox
transacional é a alternativa para mensagens emitidas depois desse commit.

Um supervisor durável usa o mesmo adapter somente em boundaries explícitas de
step. Ele não persiste um task frame arbitrário.

**Pesquisa:** um output gate geral pode reter responses ou writes fora de um
step. Se a write falhar, o runtime descarta os outputs. O protótipo precisa
provar causalidade, limites, backpressure e cancelamento. Os
[output gates de Durable Objects](https://blog.cloudflare.com/durable-objects-easy-fast-correct-choose-three/)
são uma referência, não uma decisão automática.

### 13.10 Capabilities e sandbox

**Exemplo:** um handler sem `FileSystem` não abre arquivos mesmo quando o processo
host possui essa permissão.

O contrato portátil usa capabilities tipadas para filesystem, network, clock,
random, process, environment, storage e devices. A enforcement boundary depende
do target:

- type system e HIR dentro do programa;
- processo/OS sandbox para código nativo não confiável;
- Wasm/component boundary quando compatível;
- seccomp, namespaces, sandbox-exec ou job objects como defense-in-depth.

Seccomp não protege “cada módulo importado”. Um módulo não é uma boundary
física. Um filtro de syscall não controla memory safety dentro do processo.

### 13.11 Wasm e playground

**Exemplo:** o playground executa `add(2, 3)` em Wasm. Network permanece ausente
sem um import tipado do host.

Wasm é um target e uma boundary possível. Ele não transforma W em substituto de
JavaScript. O playground compila um subset para Wasm e usa imports/exports
tipados do host. DOM, network e storage só existem quando o profile concede.

### 13.12 Observabilidade e teste

**Exemplo:** uma call de service registra queue time, execution time, instance ID
e causalidade no mesmo evento estruturado.

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

APIs públicas da standard library são escritas em W quando a linguagem consegue
expressá-las. Compiler intrinsics ficam atrás de declarations pequenas e
auditáveis. O source inicial está em [`std/`](std/).

Uma operação pertence ao tipo que possui seu estado ou contrato. A std não cria
uma classe utilitária apenas para agrupar nomes:

```w
var output = String()
output.reserve(minimumBytes: 4<KiB>)
output.append("Last Light")

let menu: view String = output.view
```

`StringBuilder` e `StringView` não são tipos core. Um tipo especializado pode
existir quando possui representação, complexity ou ownership próprios.

### 14.1 T0 — core independente do ambiente

**Exemplo:** `Array.map` e `String.scalars` funcionam em target freestanding sem
console, clock ou filesystem.

T0 contém:

- tipos primitivos, numeric modes/errors, `TotalFloat`, Option, Result e Error;
- String, Bytes, Array, Map, Set, Range e access mode `view`;
- `pin`, `Pinned<T>`, AllocationError e allocator hooks;
- protocols de igualdade, hash e iteração;
- `Arguments<T>`, `reflect.TypeId` e reflection opt-in;
- intrinsics de ownership e dos predicates `transferable`/`shareable`;
- `Atomic<T>`, `MemoryOrder` e operações atômicas sem espera;
- operações puras de texto, collection e matemática básica;
- intrinsics necessários para memória segura e compile time.

T0 pode usar o runtime/allocator do target. Ele não depende de console,
filesystem, rede, clock, locale ou OS API.

O runtime pode injetar uma hash seed inacessível como metadata de hardening.
Isso não cria uma random API nem altera output. Um target freestanding sem essa
metadata usa o fallback declarado e informa `.unavailable`:

```w
let map = Map<String, Token>() // funciona com ou sem hardening seed
```

### 14.2 T1 — systems e adapters comuns

**Exemplo:** `print("ready")` exige um host com `Console`. Uma library pura não
recebe essa capability implicitamente.

T1 contém:

- console e `print`;
- process, environment, filesystem e paths;
- clock, calendar, timezone e random;
- Task, TaskOutcome, TaskGroup, Stream e Channel;
- synchronization, executors e blocking adapters;
- build transforms com inputs, outputs e capabilities fechados;
- ServiceRef, ServiceBinding, ServiceFamily, ServiceIdentity e service host APIs;
- SupervisorRef, WorkKeyRef, WorkRef, WorkContext, WorkSnapshot e WorkOutcome;
- TCP, UDP, TLS e DNS;
- crypto, codecs, JSON e FFI C;
- storage e observabilidade básicas.

`print` é um nome normal da prelude T1. Ele só está disponível num scope de host
que concede Console. Uma função exportada fora desse scope recebe `Console`
como capability explícita.

#### 14.2.1 Contratos de bytes

**Forma vigente:** I/O comum usa dois protocols async-first:

```w
export enum ReadStep {
  data(usize<(1...)>)
  end
}

export enum WriteStep {
  complete
  partial(usize<(1...)>)
}

export protocol ByteSource<Failure: Error> {
  mut async fn read(
    appendTo destination: inout Bytes,
    maximum: usize<(1...)>,
  ): ReadStep throws Failure
}

export protocol ByteSink<Failure: Error> {
  mut async fn write(source: view Bytes): WriteStep throws Failure
  mut async fn writeMany(_ sources: view Bytes...): WriteStep throws Failure
}
```

Os nomes descrevem direção, unidade e ownership. Eles não criam classes
utilitárias. `ByteSource` possui um cursor lógico e acrescenta bytes ao owner do
caller. `ByteSink` empresta bytes e escreve um prefixo. `writeMany` trata os
segments como uma concatenação lógica sem materializá-la.

`async` permanece na assinatura do requirement. O call site usa `try await`.
Um tipo in-memory com `Failure = Never` ainda exige `await`, porque trocar a
implementação por socket, pipe ou arquivo não muda a assinatura.

`Reader`/`Writer`, `AsyncRead`/`AsyncWrite` e `Input`/`Output` permanecem
alternativas de nome. `ByteSource`/`ByteSink` vencem por não confundir texto,
messages, files e cursors, e por não repetir `Async` quando a função já declara
o efeito.

#### 14.2.2 Leitura, inicialização e EOF

**Exemplo:** uma leitura acrescenta de 1 a 4096 bytes ou devolve `.end`; zero
nunca significa duas coisas.

```w
var payload = Bytes()

switch try await input.read(
  appendTo: inout payload,
  maximum: 4096,
) {
  case .data(let count):
    let start = payload.count - count
    inspect(payload[start...])
  case .end: finish()
}
```

Antes de submeter I/O, o adapter garante uma reserva para `maximum` bytes. A
operação pode usar a parte ainda não inicializada do carrier de `Bytes`, mas
essa memória nunca aparece no source safe. O commit aumenta `destination.count`
exatamente pelo valor de `.data`.

As invariantes são:

1. `.data(count)` sempre possui `count > 0`;
2. `.end` não altera o destination;
3. depois de `.end`, as próximas leituras devolvem `.end`;
4. uma chamada não acrescenta mais que `maximum`;
5. error antes de progress não altera `count`;
6. o source e o destination ficam emprestados até completion ou cancel drain.

`read` segue a policy normal de allocation. Um caller que precisa recuperar OOM
reserva antes da call:

```w
let required = try usize.checkedAdd(payload.count, 4096)
try payload.tryReserve(minimumCapacity: required)
let step = try await input.read(appendTo: inout payload, maximum: 4096)
```

Depois de uma reserva suficiente, a call não aloca. `Bytes` mantém initialized
count e reserva privados; W não publica `ReadBuffer`, `MaybeUninit<u8>` ou uma
mutable view de bytes não inicializados.

O protocol também não oferece `read(into: inout view Bytes)`. Um backend pode
ter escrito no storage inicializado antes de cancellation vencer. Nesse caso,
manter o `count` não desfaz os bytes alterados. Uma API especializada só pode
expor um destino de extent fixo quando seu outcome informa o prefixo que o
backend pode ter modificado. A operação comum usa append e commit privado para
manter a garantia forte.

O [`ReadBuf` de Tokio](https://docs.rs/tokio/latest/tokio/io/struct.ReadBuf.html)
mostra por que initialized e filled são estados diferentes. W guarda os mesmos
fatos no owner `Bytes` e no frame da operação, sem tornar o wrapper parte da API
comum.

EOF é estado, não error. Se um backend observa bytes e EOF na mesma completion,
progress vence: a call devolve `.data`, grava EOF pendente e a próxima call
devolve `.end`. Isso evita o tuple ambíguo `(count, error)` e preserva todos os
bytes.

O [contrato POSIX de `read`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/read.html)
também permite uma leitura curta e devolve progress quando uma interrupção
ocorre depois de bytes transferidos. W normaliza o resultado antes de expor o
source.

#### 14.2.3 Escrita parcial e completa

**Exemplo:** `write` aceita o source inteiro ou informa um prefixo positivo.

```w
switch try await output.write(payload) {
  case .complete: done()
  case .partial(let count): retry(payload[count...])
}
```

Com source vazio, `write` devolve `.complete` sem suspender. Com source não
vazio, ele nunca devolve progress zero: aguarda capacidade, escreve ao menos um
byte ou lança error.

`.partial(count)` garante `0 < count < source.count`. O prefixo foi aceito pela
boundary concreta. Ele não garante que bytes chegaram a um peer, a mídia
persistente ou o display.

O adapter comum `writeAll` repete writes:

```w
try await output.writeAll(payload)
```

`writeAll` é uma conveniência não transacional. Um error depois de progress usa:

```w
export struct WriteAllError<Cause: Error>: Error {
  cause: Cause
  committed: usize
}
```

O caller ainda possui `payload`; `committed` identifica o prefixo que não deve
ser reenviado sem uma policy de protocolo. Cancellation pode ocorrer depois de
um prefixo confirmado. Código que precisa observar cada commit usa `write` em
vez de `writeAll`.

POSIX permite writes parciais e não promete atomicidade geral. O
[contrato de `write`](https://pubs.opengroup.org/onlinepubs/9699919799/functions/write.html)
é a razão para W não tratar uma call como entrega integral.

#### 14.2.4 Progress, error e cancellation

**Exemplo:** se completion de quatro bytes vence cancellation, a call confirma
quatro bytes. Cancellation é observada no próximo suspension point.

Cada operação possui uma disputa linear:

```text
pending ── completion ──→ committed(progress/end/error)
   └──── cancellation ──→ canceling ── completion drain ──→ canceled
```

Se cancellation vence antes de progress:

- a parte spare de `Bytes` não entra no initialized count;
- um read destination permanece logicamente inalterado;
- um write não confirma bytes;
- a task não termina antes de o backend liberar pointer, handle e callback.

Se completion vence, a operação devolve o progress. Uma solicitação de
cancellation pendente é observada no próximo ponto cancelável. O runtime não
transforma completion real em um falso “nenhum efeito”.

Um backend pode produzir progress e error na mesma completion. W devolve o
progress e armazena o error no source ou sink. A próxima operação lança esse
error antes de submeter outro I/O. Assim, cada call que lança error possui zero
progress próprio.

O error latched pertence à completion que já venceu a disputa. Por isso, ele é
observado antes de uma solicitação de cancellation posterior. `writeAll`,
`finish` e a próxima operação também consultam esse estado. Se o caller encerra
o owner sem observá-lo, `deinit` registra o fato no trace, mas não lança.

Solicitar cancellation não significa que o kernel interrompeu a operação. A
[documentação de `CancelIoEx`](https://learn.microsoft.com/en-us/windows/win32/fileio/cancelioex-func)
explica que uma operação pode concluir normalmente depois do pedido. A
[documentação de cancellation do `io_uring`](https://man7.org/linux/man-pages/man7/io_uring_cancelation.7.html)
também exige observar a completion final. O runtime W usa essa completion para
resolver a disputa e só então libera borrows.

Uma operação não cancelável aumenta cancellation latency. Ela não autoriza
use-after-free, detach do request ou liberação antecipada do buffer. O trace
separa `cancelRequested`, `cancelSubmitted`, `cancelConfirmed` e
`completedBeforeCancel`.

#### 14.2.5 I/O async e blocking

**Exemplo:** uma API foreign blocking entra num domain bounded explícito:

```w
spawn<.blocking> let step = legacy.read(
  appendTo: inout payload,
  maximum: 4096,
)
```

`ByteSource` e `ByteSink` prometem suspensão do caller. Eles não prometem qual
backend físico o target possui. O host registra uma destas estratégias:

| Estratégia | Consome worker bloqueado? | Cancellation física |
|---|---:|---|
| readiness | não durante a espera | remove interest; syscall final ainda pode ocorrer |
| completion | não durante a espera | request cancelável conforme o backend |
| blocking adapter | sim, dentro de quota | somente quando a API subjacente suporta |
| immediate/in-memory | não | completion síncrona |

Uma interface síncrona separada usa `BlockingByteSource` ou
`BlockingByteSink`. A interface compilada marca seus methods como `blocking`.
Ela não atende automaticamente ao protocol async.

Um adapter explícito pode produzir uma interface async:

```w
let input = legacy.adapt(using: context.executors.blocking)
let step = try await input.read(appendTo: inout payload, maximum: 4096)
```

O blocking executor é bounded. Cancellation da task pode abandonar o interesse
no resultado, mas não mata a thread nem libera o buffer antes de a call
terminar.

Um product pode exigir capability `nonBlockingIO`. Nesse caso, o build rejeita
um target cuja rota alcançável depende do blocking adapter. Essa gate permite
portabilidade sem esconder thread consumption.

#### 14.2.6 Filesystem, rights e offsets

**Forma vigente:** `FileSystem` é uma capability concedida pelo host. `File` é um
handle move-first com rights estáticos:

```w
let menu = try await files.open<[.read]>(menuPath)
let journal = try await files.open<[.write, .append]>(
  journalPath,
  creation: .create,
)
```

`File<[.read]>` não compila uma call de write. O static list é um subset fechado
de `FileRight`; ele não substitui a verificação dinâmica de path, sandbox,
quota ou permissão do sistema.

Arquivos seekable usam I/O posicional por default:

```w
var block = Bytes()
let step = try await menu.read(
  at: FileOffset(8192),
  appendTo: inout block,
  maximum: 4096,
)
```

`read(at:)` e `write(at:)` não alteram um cursor compartilhado. Essa regra
permite concorrência explícita e corresponde à função de
[`pread`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/read.html).
`FileOffset` é um newtype unsigned de 64 bits; conversions para limites do
target são fallible.

`File.read(at:)` não atende a `ByteSource`, pois não possui cursor. Seu
`.end` informa que não havia bytes naquele offset durante a operação. Outra call
pode observar dados se o arquivo crescer. O adapter sequencial abaixo transforma
essa observação num fim estável. Um caller que precisa de conteúdo estável abre
um snapshot ou valida a identidade e a versão do arquivo.

Uma operação sequencial cria um owner de cursor:

```w
var reader = (take menu).reader(from: FileOffset.zero)
let step = try await reader.read(appendTo: inout block, maximum: 4096)
```

O retorno é `some ByteSource<IoError>`. Não existe uma classe pública
`FileReader`. O adapter possui file e offset, e atualiza o offset somente pelo
progress confirmado.

Append não usa `metadata.size` seguido de `write(at:)`. O right `.append`
oferece uma operação própria que preserva a atomicidade de seleção do offset
fornecida pelo adapter. Isso não promete que um payload grande é indivisível.

`File` comum é move-only e transferable. Código que precisa de acesso paralelo
move o handle para `shared File` de forma explícita ou abre handles
independentes. Positional I/O é obrigatório no shared form; um cursor mutável
não se torna shareable.

#### 14.2.7 Sockets e message boundaries

**Exemplo:** uma conexão TCP pode separar leitura e escrita sem permitir duas
leituras concorrentes no mesmo cursor:

```w
let (input, output) = (take connection).split()

async let request = readRequest(take input)
async let response = writeResponse(take output)
```

Os halves atendem a `ByteSource<NetworkError>` e
`ByteSink<NetworkError>`. Cada half é move-only e transferable. O write half
oferece `take async fn finish()` para half-close; destruir a conexão é abortivo.

TCP não preserva messages. UDP não atende a `ByteSource`: ele usa
`DatagramSource<Message, Failure>` e devolve um datagram, endereço e truncation
status por receive. TLS e HTTP são adapters T2 sobre byte contracts; handshake,
record close e protocol errors não desaparecem dentro de `read`.

#### 14.2.8 Errors portáteis

**Exemplo:** código portátil testa `error.kind == .permissionDenied`; diagnostics
podem mostrar o código nativo.

```w
export struct IoError: Error {
  kind: IoErrorKind
  operation: IoOperation
  cause: IoCause?
}

export enum IoErrorKind {
  permissionDenied
  notFound
  alreadyExists
  invalidInput
  unsupported
  resourceExhausted
  timedOut
  connectionReset
  brokenPipe
  other
}
```

`IoCause` é opaca, target-specific e redigível. Ela não participa de igualdade,
serialization ou resultado de domínio. Um programa pode pedir o código nativo
somente depois de verificar o target.

`wouldBlock` não sai de uma API async: o executor registra interest e suspende.
Uma interrupção do sistema sem progress é repetida quando não representa
cancellation ou signal observável. EOF continua `ReadStep.end`.

Task deadline e `task.cancel()` produzem `TaskOutcome.canceled`, não
`IoError.timedOut`. `.timedOut` representa um timeout do protocolo, peer ou
adapter que não é o deadline da task.

`IoErrorKind` é edition-frozen. Um código novo ou desconhecido usa `.other`.
Retriability não é Boolean do error: ela depende de operation, idempotência,
progress e deadline.

#### 14.2.9 Finish, flush e durability

**Exemplo:** um arquivo de auditoria usa finish explícito; drop não promete
persistência.

```w
let journal = try await files.open<[.write]>(
  journalPath,
  creation: .replace,
)

defer async {
  try await (take journal).finish(durability: .data)
}
```

`ByteSource` e `ByteSink` não exigem `close` ou `flush`. Um memory sink não
possui handle; um TCP half-close e um file sync possuem contratos diferentes.

- `flush` envia buffers de user space ao adapter seguinte;
- `sync(.data)` solicita data durability;
- `sync(.all)` inclui metadata conforme o filesystem;
- `finish` executa a obrigação do tipo e consome o owner;
- `deinit` fecha o handle físico de forma síncrona e best-effort.

Um error de `deinit` entra no trace. Ele não pode ser lançado. Código que depende
da confirmação usa `finish` ou `sync`. Um handle compartilhado não oferece
`finish` até o programa recuperar ownership único.

#### 14.2.10 Streams, framing e limites

**Exemplo:** chunks borrowed reutilizam um único carrier; a view termina antes
da próxima leitura.

```w
var scratch = Bytes()
scratch.reserve(minimumCapacity: 4096)

var chunks = (take input).chunks(
  reusing: take scratch,
  maximum: 4096,
)

for try await chunk in chunks {
  decode(chunk)
}
```

O adapter devolve `some Stream<view Bytes, E>`. Ele possui source e scratch. Um
novo `next()` não ocorre enquanto `chunk` está vivo.

`chunks(maximum:using:) -> some Stream<Bytes, E>` devolve owners independentes e
pode alocar. A escolha entre owned e borrowed aparece no tipo.

`lines(maximumBytes:)` combina byte source, decoder incremental e framing:

```w
for try await line in input.lines(maximumBytes: 65_536) {
  handle(line)
}
```

Cada line é `String` owned. O limite é obrigatório e é verificado antes de
growth. EOF depois de texto sem delimitador produz a última line. Protocols que
exigem delimitador usam um framer explícito com `final: .requireDelimiter`.

`readToEnd` também exige `limit`. Sources contínuos, como terminal ou socket,
podem nunca produzir EOF. A API não oferece uma versão ilimitada por default.

#### 14.2.11 Backend, gather write e transferências especializadas

**Forma vigente:** `ByteSink.writeMany` recebe zero ou mais segments borrowed:

```w
let step = try await output.writeMany(responseHead, jsonBody, finalBoundary)

switch step {
  case .complete: finishResponse()
  case .partial(let count): rememberCommittedPrefix(count)
}
```

A operação observa os segments como se fossem uma sequência concatenada. Ela
não cria essa concatenação. Segments vazios não mudam o resultado.

O resultado segue estas regras:

1. `.complete` confirma todos os bytes de todos os segments.
2. `.partial(count)` confirma um prefixo positivo da concatenação lógica.
3. O prefixo termina dentro de um segment ou entre dois segments.
4. Um error lançado confirma zero bytes nessa call.
5. Progress e error na mesma completion seguem a regra de error latched.
6. A call não promete entrega ao peer, atomicidade, durability ou message
   boundary.

O default de protocol chama `write` somente para o primeiro segment não vazio.
Se outros segments possuem bytes, um `.complete` dessa call vira
`.partial(source.count)`. Esse fallback não aloca, não copia e não faz uma
segunda operação física.

Um adapter pode substituir o default por uma operação gather nativa. Ele
processa os segments em ordem. Um limite nativo de quantidade ou tamanho não
vira error público. O adapter envia o maior prefixo válido e devolve
`.partial(count)` quando ainda existe input.

O adapter não concatena segments para ultrapassar `IOV_MAX` ou outro limite. Ele
pode usar um array bounded de descriptors no task frame. A call mantém cada
owner e descriptor válido até completion ou cancel drain.

O compiler impede mutation, resize ou destruição dos owners durante a call.
`view Bytes` não exige `Pinned<T>` no source. O lowering pode fixar o task frame
ou impedir o move do carrier durante a operação.

W não expõe `isWriteVectored`. O source usa a mesma call com qualquer adapter.
`w explain io` informa native gather, quantidade submetida, fallback e
allocation.

O adapter seleciona uma estratégia pelo target, product contract e benchmark:

- IOCP e `WSASend` no Windows;
- `io_uring`, `writev`, epoll ou poll no Linux;
- `writev`, kqueue ou poll nos BSDs e macOS;
- WASI, browser host ou HAL no target correspondente;
- blocking pool bounded quando não existe backend melhor.

POSIX processa os buffers de `writev` em ordem e permite short progress. O
número de segments possui limite específico do host. `WSASend` também preserva
a ordem e exige buffers válidos até completion. W mantém essas propriedades sem
publicar `iovec` ou `WSABUF`.

Atomicidade de `writev` em um arquivo não vira garantia de `ByteSink`. Sockets,
filesystems e adapters possuem boundaries diferentes. Uma API que exige
atomicidade usa um contrato específico.

**Pesquisa:** scatter read não entra no protocol comum. A forma exigiria vários
borrows exclusivos, initialized counts e rollback parcial. W também rejeita
`inout T...` por causa de alias diagnostics. A baseline continua
`read(appendTo:maximum:)` com um único owner.

Um futuro scatter read pode usar um owner `ReadBatch` ou um tuple fixo de
destinos. A pesquisa deve provar alias checking, cancellation e progress em
POSIX, Windows e um host component.

**Pesquisa:** transferência file-to-sink continua uma operação explícita.
`sendfile`, `TransmitFile`, `splice`, memory mapping e device buffers mudam
provenance, offset, mutation lifetime e fallback.

Esta forma permanece **Alternativa**:

```w
let step = try await output.transfer(
  from: archive,
  range: requestedRange,
  fallback: inout scratch,
)
```

O scratch explícito evita allocation escondida quando o target não possui uma
rota especializada. A operação só entra na forma vigente depois de oracles em
Windows, Linux e um terceiro host.

Fontes primárias:

- [`readv` e `writev`](https://man7.org/linux/man-pages/man2/writev.2.html);
- [`WSASend`](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasend);
- [`sendfile`](https://man7.org/linux/man-pages/man2/sendfile.2.html);
- [`TransmitFile`](https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-transmitfile).

#### 14.2.12 Observabilidade e oracle

**Exemplo:** o explanation record mostra por que o mesmo source usa IOCP num
product e blocking pool em outro.

```text
$ w explain io restaurant.io::readRecipeBlock
operation:      file.read(at:)
backend:        iocp
request:        <= 4096 B
buffer:         Bytes spare; pinned until completion
blocking worker: no
cancel:         request + completion drain
allocation:     none after caller reservation
```

```text
$ w explain io restaurant.io::writeExtinctRecipeFrame
operation:        bytes.writeMany
segments:         3 borrowed
backend:          wsasend
native submitted: 3
fallback:         available; one write
allocation:       none
```

O resource lens registra bytes solicitados, confirmados e descartados, short
operations, queue time, backend, blocking workers, pinned bytes, syscall count,
cancel latency e native cause redigida.

O oracle do restaurante executa:

- vazio, EOF depois de data e todas as partições de 1 a 4096 bytes;
- short read e short write em cada posição;
- error antes e depois de progress;
- cancellation antes da submissão, durante espera e depois da completion;
- destination sem mutation quando cancellation vence;
- buffer vivo até cancel drain;
- positional reads fora de ordem com resultado ordenado pelo caller;
- cursor sequencial sem offset perdido;
- blocking, readiness e completion backends com o mesmo resultado;
- gather com zero, empty e mais segments que o limite nativo;
- partial gather dentro e entre segments;
- fallback, `writev` e `WSASend` com o mesmo byte stream;
- `Stream<view Bytes>` sem allocation depois da reserva;
- limits de line e read-to-end antes de growth;
- leak sanitizer, TSan e fault injection.

### 14.3 T2 — domínios oficiais

**Exemplo:** `std.http` e `std.si` são bundled, mas só entram no payload quando
o programa os alcança.

T2 contém módulos bundled e reachability-linked:

- HTTP client/server, CLI/TUI e adapters de UI;
- database, SQL descriptors e cache local com limite;
- SI, quantities, análise numérica e constants versionadas;
- BigInt, Rational, FixedDecimal, Complex e quantization;
- tensor, linear algebra e ML experimental;
- SQLite/durable adapter;
- formatos e protocols de domínio com suporte oficial.

Álgebra simbólica completa, CAS, dataframe e stacks de vendor continuam packages
first-party experimentais. Tier não significa import implícito, linking ou
disponibilidade universal em todo target.

#### 14.3.1 Mensagem HTTP e ownership

**Exemplo:** o handler consome o body uma vez e exige limite antes de decodificar:

```w
async fn fetch(
  request: take http.Request,
  ctx: http.Context,
): http.Response throws ApiError {
  let command = try await request.decodeJson<Command>(
    maximumBytes: 64<KiB>,
  )
  return try http.Response.json(command)
}
```

`std.http` representa a semântica HTTP. Ele não expõe frames de HTTP/1.1,
HTTP/2 ou HTTP/3 no handler comum. Um adapter pode informar versão e transport
em metadata observável. O resultado do programa não depende desses dados.

`Request` é um owner move-only. Ele contém method, target, headers, body e
metadata atenuada pelo host. Ele não atende a `Copy` ou `Duplicable`. Uma
service call que transfere um request também transfere o body.

O body atende a `ByteSource<HttpBodyError>` e só pode ser consumido uma vez.
`decodeJson`, `bytes` e `text` são métodos async do request. Cada método exige
um limite. O programa usa o body como stream quando não deseja materializá-lo.

W não oferece `request.clone()` implícito. Um caller que precisa de duas leituras
materializa bytes com limite ou cria um tee bounded. Um tee mantém backpressure
entre os consumers e declara o buffer máximo.

Headers preservam entradas repetidas. Comparação de nome é ASCII
case-insensitive. O valor continua `Bytes`, pois um peer pode enviar octets que
não formam texto Unicode. A conversão para `String` exige uma policy explícita.
`HeaderName` e method tokens rejeitam um token vazio ou um byte fora da grammar
HTTP. `HeaderField` rejeita NUL, CR, LF e outros control bytes não permitidos.
Assim, um adapter nunca recebe um field inválido por uma API safe.

`Method` possui os methods registrados e `.other(MethodToken)`. O token
customizado é validado. `StatusCode` aceita `100..<600`; associated constants
como `http.Status.ok` evitam números soltos.

O modelo segue a mensagem abstrata do
[RFC 9110](https://datatracker.ietf.org/doc/rfc9110/) e a separação de
request, response, fields e streams do
[WASI HTTP](https://github.com/WebAssembly/wasi-http).

#### 14.3.2 Response, streaming e commit

**Exemplo:** bytes owned geram tamanho conhecido. Um stream mantém sua própria
boundary:

```w
let fixed = http.Response.bytes(
  take encoded,
  status: .ok,
  contentType: "application/json",
)

let streamed = http.Response.stream(
  take body,
  status: .ok,
  headers: headers,
)
```

`Response` é um owner. Retorná-lo transfere headers e body para o host. Um body
fixo usa `Bytes` ou `String`. Um body incremental usa um
`ByteSource<HttpBodyError>` owned.

O host cria um response-pump runtime-owned para o stream. Esse pump é parte do
request root. Ele não é uma task solta. Disconnect, deadline ou erro de
transport cancelam o pump, drenam a completion física e destroem o body uma
vez.

Retornar `Response` confirma somente a aceitação pelo host. Não confirma entrega
ao client. Código que precisa de efeito confiável depois da resposta usa
`SupervisorRef`, queue ou workflow. Ele não depende de callback de socket.

O adapter valida status, headers, trailers e framing antes do primeiro commit.
Depois do commit, um error de stream fecha a resposta e entra no trace. Ele não
se converte num segundo status HTTP.

Responses `1xx`, `204` e `304` não aceitam body. Uma resposta a `HEAD` envia os
headers da representação e não consome o body. O linter informa quando o handler
constrói um body que será descartado por essa regra.

`Response.text`, `.bytes`, `.json` e `.html` são constructors do próprio tipo.
Eles definem media type e encoding. `Response.json` exige `JsonEncodable`; ele
não usa reflection universal.

#### 14.3.3 Host HTTP, limits e admission

**Exemplo:** um processo nativo fornece network e limites. Um worker recebe o
mesmo envelope do host:

```w
const serverLimits = http.ServerLimits(
  activeRequests: 1_024,
  queuedRequests: 2_048,
  queuedBytes: 64<MiB>,
  connections: 8_192,
  message: http.MessageLimits(
    targetBytes: 16<KiB>,
    headerBytes: 64<KiB>,
    headerFields: 128,
    bodyBytes: 1<MiB>,
  ),
)

try await http.serve(
  at: .loopback(port: 8_080),
  using: ctx.network,
  limits: serverLimits,
  handler: fetch,
)
```

`http-worker@1` possui este slot default:

```text
http.fetch:
  async fn(take http.Request, http.Context)
    -> http.Response throws E
```

O profile grava como `E` vira uma resposta de boundary. Um error de domínio
precisa de mapping explícito. Panic, quota e fault continuam outcomes distintos.

Cada host profile possui um schema fechado de `hostConfiguration`. O compiler
rejeita fields desconhecidos e grava a configuração na recipe. A configuração
seleciona somente policies que o profile declara. Ela não concede capability,
slot ou authority.

**Exemplo:** o profile de benchmark fixa headers e remove trabalho que o
harness proíbe:

```w
hostConfiguration: {
  responseHeaders: {
    server: .literal("W")
    date: .cached(maximumAge: 1<s>)
  }
  compression: .deny
  logging: { requests: .deny, disk: .deny, console: .deny }
}
```

O cache de `Date` usa clock autorizado e não pode servir um instante mais velho
que o limite. Um profile sem clock não oferece essa policy. O deployment pode
reduzir logging e compression. Ele não pode habilitar uma policy negada na
recipe.

Um processo nativo usa `http.serve`. Um component recebe requests pelo slot
`http.fetch`. Os dois caminhos usam os mesmos `Request`, `Response`, handler e
oracles. O native adapter possui accept loop; o worker host possui admission
externa.

Limits são obrigatórios no product ou na call de `serve`. O menor envelope
vence. O adapter rejeita target, headers e body antes de growth acima do limite.
Ele também limita requests ativos, fila, bytes enfileirados e conexões.

Overload antes do handler não cria uma task W. O host responde conforme o
profile e registra o motivo. Rate limit de negócio continua uma capability
separada.

Cada request aceito cria um root estruturado. Children normais terminam antes do
handler devolver. Somente owners transferidos no `Response` e adapters
runtime-owned declarados podem sobreviver ao frame do handler.

`http.Context` não é um mapa ambiental. Ele contém registries tipados para as
capabilities declaradas pelo product. Database, cache, secret e service usam
bindings const que o linker verifica.

O host projeta duas formas de authority:

- uma capability padrão e singular usa um member tipado, como `ctx.random`;
- um resource nomeado usa um registry e um binding const, como
  `ctx.databases.get(benchmarkDatabase)`.

O product autoriza a família de capabilities no envelope máximo. Uma
capability singular definida pelo host profile não precisa de outro nome. O
runtime graph declara cada resource nomeado como host import. Assim, database e
cache exigem a família no product e o binding exato no graph. Nos dois casos, o
compiler deriva o requirement do source e rejeita authority ausente no link.
O parâmetro `binding` de cada `get` é `const`; um nome calculado em runtime não
abre outro resource.

#### 14.3.4 SQL estático e rows tipadas

**Exemplo:** parâmetros e resultado pertencem ao tipo do descriptor:

```w
type WorldKey = (id: i32,)
type WorldRow = (id: i32, randomNumber: i32)

const worldById: database.Query<WorldKey, WorldRow> = database.Query(
  text: #"""
    SELECT id, randomNumber
    FROM World
    WHERE id = :id
    """#,
)

let row = try await store.one(
  worldById,
  parameters: (id: 42,),
)
```

`Query<Parameters, Row>` e `Command<Parameters>` são const descriptors. Os
methods de `Database` recebem o descriptor em um parâmetro de chamada `const`.
Portanto, um query construído com input runtime não entra nessa API. SQL usa
parâmetros nomeados. Interpolação de String não cria parâmetro.

O const evaluator verifica que cada placeholder corresponde a um field de
`Parameters`. Ele rejeita nome ausente, duplicação incompatível, statement
múltiplo e text mutável. O artifact grava o SQL normalizado, dialect, tipos e
query identity.

O descriptor não repete um `name` textual. O compiler usa o path do binding
`const` como label de diagnostics e telemetry. A identity preparada deriva do
descriptor normalizado, dos tipos, do dialect e do schema bundle. Um rename não
muda o statement; uma mudança semântica muda a identity. Um descriptor inline
usa source span como label local. Uma interface reutilizável deve preferir um
binding nomeado.

`Row` é um tuple ou tipo aceito por um decoder explícito. A baseline não
preenche uma struct por reflection. O adapter verifica número, ordem, nullability
e representação de cada column antes de construir o row.

Um schema bundle opcional permite validar table, column e tipo no build. Sem
bundle, essas verificações permanecem runtime. O artifact e o resultado de
`w explain database` informam qual nível foi provado.

`.portable` aceita o subset SQL versionado pelo W. `.sqlite` e `.postgresql`
permitem extensions específicas. Um query de um dialect não executa em outro
adapter por fallback silencioso.

SQL dinâmico exige uma API separada, capability própria e resultado apagado.
Ele permanece fora da baseline. Essa regra mantém query identity, prepared
statement cache e auditoria estáveis.

SQLite também transforma SQL em prepared statement antes de bind e execução.
As fases públicas de
[prepare, bind, step, reset e finalize](https://sqlite.org/c3ref/stmt.html)
fundamentam o descriptor W.

#### 14.3.5 Database, pipeline e transaction

**Exemplo:** cada item continua um statement independente, mesmo quando o
adapter usa pipeline:

```w
let rows = try await store.queryMany(
  worldById,
  parameters: take keys,
  maximumInFlight: 20,
)

let updated = try await store.transaction(
  take worlds,
  isolation: .readCommitted,
  using: persistWorlds,
)
```

`database.Binding` seleciona um pool por nome e dialect. O deployment fornece o
adapter e referencia credentials por secret capability. Source, artifact e lock
não contêm connection string secreta.

O pool limita connections, operations queued e bytes retidos. Admission cheia
produz `DatabaseError.overloaded`. A fila não cresce até OOM.

`one` exige um row. `optional` aceita zero ou um. `all` exige limites de rows e
bytes. `queryMany` executa uma vez por item, preserva a ordem de input e exige
`maximumInFlight`.

`executeMany` também aplica `Command` uma vez por item e preserva a ordem
observável de errors. O adapter pode usar uma batch protocol ou uma transmission
única. Ele não converte os parâmetros em um statement que altera vários rows,
salvo quando o próprio `Command` declara esse statement.

O adapter pode preparar, reutilizar statements, agrupar syscalls e pipeline de
transport. Ele não pode combinar SELECTs, remover queries repetidas ou mudar
transactionality. A
[pipeline do PostgreSQL 18](https://www.postgresql.org/docs/18/libpq-pipeline-mode.html)
mostra o ganho e também exige associar cada result à query original.

`transaction` cria um borrow de `Transaction` que não escapa da operação. Em
success, o adapter confirma commit antes de devolver o output. Em error ou
cancellation, ele executa rollback e drena o connection antes de devolvê-lo ao
pool.

Uma perda de conexão depois de enviar commit produz
`TransactionFailure.unknownCommit`. W não repete a transaction
automaticamente. Query read-only também não recebe retry implícito; a policy
precisa considerar deadline, idempotência e progress.

Buffers de driver podem alimentar o decoder por view. Nenhuma view de row escapa
da call. Um stream de rows futuro precisa possuir seu pool lease e declarar
limites, cancellation e cleanup antes de entrar na std.

#### 14.3.6 Cache local com limite

**Exemplo:** o binding fixa a autoridade local e o limite de entries:

```w
const cachedWorlds = cache.LocalBinding<i32, CachedWorld>(
  name: "cached-worlds",
  maximumEntries: 10_000,
  maximumActiveLoads: 256,
  maximumQueuedLoads: 4_096,
  expiration: .none,
)

let worlds = try ctx.caches.get(cachedWorlds)
let value = try await worlds.getOrLoad(id, using: loadCachedWorld)
```

`LocalCache<K, V>` é uma capability process-local. `K` atende a igualdade,
hash e duplicação. `V` atende a `Duplicable`. `get` devolve um value owned; uma
eviction nunca invalida borrow do caller.

O cache possui limite obrigatório por entries. Weight e limite por bytes
permanecem próximos candidatos. Expiration usa monotonic clock e pode ocorrer
depois de access ou write. O adapter pode remover qualquer entry antes do prazo
para respeitar memória.

O binding também limita loaders ativos e calls enfileiradas. Uma call que não
entra nesses limites falha com `.overloaded` antes de iniciar o loader. Calls
para a mesma key contam como waiters enfileirados. O limite por entries não
promete um limite transitivo de bytes; o product mantém seu envelope de memória
até existir um contrato de weight comprovável.

Hit ou miss não altera o resultado correto. Cache não é source of truth,
durability ou lock distribuído. A baseline não possui write-back.

`getOrLoad` mantém no máximo um loader ativo por key. Calls concorrentes aguardam
esse loader. O loader continua child estruturado do caller líder. Se ele falha
ou é cancelado, nenhum value entra no cache; um waiter posterior pode tentar de
novo. Errors não são negative-cached por default.

Cancelar um waiter remove somente esse waiter. Cancelar o líder solicita
cancelamento do loader e acorda os waiters com cancellation. O commit do value é
o ponto linear: se ele vence a cancellation, o cache e os waiters recebem o
value; se a cancellation vence, a entry não existe. O runtime aplica a mesma
regra de completion committed usada por tasks e I/O.

O algoritmo de eviction pode combinar frequência e recência. A ordem exata não
faz parte do resultado do programa. Um test host injeta clock e policy
determinísticos. O [Caffeine](https://github.com/ben-manes/caffeine/wiki/Eviction)
mostra por que capacity e expiration são eixos separados.

O comportamento de loader compartilhado corresponde ao problema isolado pelo
[`singleflight` do Go](https://pkg.go.dev/golang.org/x/sync/singleflight).
W acrescenta ownership, error type e cancellation estruturada ao contrato.

Um deployment pode reduzir capacidade. Ele não pode converter `LocalCache` em
rede. Um cache remoto usa `ServiceRef<CacheApi>` e methods async. Assim, latency,
failure boundary e placement continuam visíveis na assinatura.

## 15. Números, ranges e unidades

### 15.1 Modelo numérico

#### 15.1.1 Tipos e literais

**Exemplo:** `0o755_u16` e `6.022_140_76e23_f64` chegam ao type checker sem
truncamento no lexer.

`Int` é a identidade pública de `i64`. `UInt` é a identidade pública de `u64`.
`isize` e `usize` são tipos distintos com a largura de address do target. Os
integers de largura fixa são `i8`, `i16`, `i32`, `i64`, `i128` e seus pares
unsigned. Signed integers usam representação two's complement. `Bool` não é
integer. Os target profiles do design vigente possuem address width de 32 ou 64 bits.

```w
let guests: Int = 42       // i64 semântico
let byteOffset: usize = 42 // largura do target
let mask = 0b1111_0000_u8
let mode = 0o755_u16
let color = 0xff_40_00_u32
```

`Int` fixo mantém overflow, serialização e refinements iguais em targets de 32
e 64 bits. O optimizer pode estreitar uma operação interna e reestender o valor
na boundary. Um target de 32 bits ainda implementa a semântica de 64 bits.
`Int` com largura do target, literal default `i32` e `BigInt` como default
permanecem **Alternativa**.

O lexer aceita:

- decimal, `0b` binary, `0o` octal e `0x` hexadecimal;
- `_` somente entre digits;
- fraction decimal com digits nos dois lados de `.`;
- exponent decimal `e` ou `E`, com sinal opcional;
- suffix separado por `_`, como `_i16`, `_u8`, `_f32` ou `_f64`.

`2.` é erro; use `2.0`. Um sinal é um operador, não parte do literal numérico.
O checker trata `-128_i8` como o limite representável, mas rejeita `128_i8` e
`-(129_i8)`. Hexadecimal float não entra no design vigente. Controle de bits usa
`f32.fromBits` ou `f64.fromBits`.

Antes do expected type, um literal integer guarda magnitude arbitrária. Um
literal decimal guarda um rational decimal exato. A materialização ocorre uma
vez:

- integer sem contexto vira `Int`;
- um literal com fraction ou exponent sem contexto vira `f64`;
- um integer literal esperado como integer exige representabilidade exata;
- um integer literal esperado como float precisa ser exato, salvo suffix float;
- um real literal esperado como binary float arredonda uma vez com
  nearest, ties-to-even;
- um expected decimal ou rational não passa por binary float.

Um literal com fraction ou exponent não materializa como integer, mesmo quando
seu valor matemático é integral. Use `1_000`, não `1e3`, para pedir um integer.
Um suffix float, uma fraction ou um exponent torna explícita a intenção de
rounding. Um real literal com suffix integer é um diagnostic.

Overflow ou infinity durante a materialização é diagnostic. Underflow para zero
ou subnormal é aceito com o mesmo rounding de runtime e pode produzir warning
configurável. `f32` e `f64` fornecem os valores associados `infinity` e `nan`;
`inf` e `nan` não são tokens literais. `fromBits` cria um payload NaN específico.

#### 15.1.2 Tipagem e conversões

**Exemplo:** `let total: i16 = 250_u8 + 2_i16` é seguro. `-1_i8 + 2_u8` não
escolhe sozinho entre signed e unsigned.

Um operador binário exige o mesmo tipo depois de no máximo uma conversão
implícita de um operando para o tipo do outro. O checker não procura um terceiro
tipo comum. Um literal ainda não materializado usa o tipo do outro operando
quando o valor cabe.

```w
let bytes: u16 = 250_u8 + 2_u16
let ratio: f64 = 3_i32 + 0.5_f64
let explicit: i16 = i16(-1_i8) + i16(2_u8)
```

Não existem integer promotions de C. `u8 + u8` produz `u8` e mantém overflow
verificado. Uma conversão integer é implícita somente quando todos os valores
da origem cabem no destino. Um refinement pode fornecer a mesma prova para um
valor mais restrito.

As conversões float implícitas são:

- `f32` para `f64`;
- `i8`, `u8`, `i16` e `u16` para `f32`;
- todos os integers de até 32 bits para `f64`;
- outra conversão cuja exatidão esteja provada por refinement.

Signed para unsigned, narrowing, integer de 64 bits para float e float para
integer são explícitos. As formas canônicas são:

```w
let port = try u16(exactly: configuredPort)
let count = try i32(rounding: sample, mode: .towardZero)
let clipped = u8(saturating: signal, nan: .zero)
let lowBits = u16(truncatingBits: word)
let short = try f32(rounding: precise, mode: .nearestEven)
let bits = short.toBits()
```

`exactly:` rejeita out-of-range, fraction, non-finite e perda de precisão.
`rounding:` exige uma `RoundingMode`. Um destination integer rejeita NaN,
infinity e out-of-range. Um destination float preserva infinity, rejeita NaN e
rejeita overflow finito na forma `try`. `saturating:` exige uma policy para NaN.
`truncatingBits:` existe somente entre integers. `toBits` e `fromBits` preservam
a representação, não convertem valor.
`T(value)` também pode tornar explícita uma conversão que já é total e exata.
Ele nunca esconde uma operação fallible.

```w
export enum RoundingMode {
  nearestEven
  nearestAwayFromZero
  towardZero
  towardPositive
  towardNegative
}

export enum NaNConversion {
  zero
  minimum
  maximum
}

export enum NumericConversionError: Error {
  outOfRange
  fractional
  nonFinite
  inexact
}
```

`nearestEven` e `nearestAwayFromZero` definem o desempate exato. `towardPositive`
e `towardNegative` apontam para infinity, não para maior ou menor magnitude.

#### 15.1.3 Aritmética inteira e bits

**Exemplo:** `u8.max + 1` causa panic em debug e release.
`u8.wrappingAdd(u8.max, 1)` produz zero de forma explícita.

`+`, `-`, `*`, unary `-` e integer `**` usam o resultado matemático e causam
panic quando ele não cabe no tipo. Em const evaluation, o mesmo caso é um
diagnostic. Nenhum profile troca essa regra por wrap.

```w
let next = try u16.checkedAdd(current, 1)
let wrapped = u16.wrappingAdd(current, 1)
let clipped = u16.saturatingAdd(current, 1)
let (sum, overflowed) = u16.overflowingAdd(current, 1)
```

`checkedAdd`, `checkedSubtract`, `checkedMultiply`, `checkedNegate`,
`checkedDivide`, `checkedPower`, `checkedShiftLeft` e `checkedShiftRight` retornam
`Result<T, ArithmeticError>`.
As famílias `wrapping`, `saturating` e `overflowing` cobrem as operações nas
quais a policy tem significado. `carryingAdd`, `borrowingSubtract` e
`fullMultiply` servem multiprecision e crypto sem depender de flags da CPU.

```w
export enum ArithmeticError: Error {
  overflow
  divisionByZero
  invalidShift(count: UInt, width: UInt)
}
```

Integer division causa panic em divisor zero e em `signed.min / -1`. O quotient
trunca em direção a zero. O remainder possui o sinal do dividendo. APIs
`euclideanDivide` e `euclideanRemainder` tornam a alternativa não negativa
explícita. `0 ** 0` produz `1`; o exponent de integer `**` é `UInt`.
`float ** Int` usa exponentiation integer strict, inclusive exponent negativo.
Potência com exponent float usa `math.pow`.

`&`, `|`, `^` e `~` operam nos bits do mesmo tipo integer. O operando direito de
shift é `UInt`. Um count igual ou maior que a largura causa panic. `value << n`
equivale à multiplicação matemática por `2 ** n` e causa panic se perder bits.
`>>` é lógico para unsigned e arithmetic para signed. `wrappingShiftLeft`,
`logicalShiftRight`, `rotatedLeft` e `rotatedRight` oferecem intenções de bits
sem sobrecarregar os operadores. `wrappingShiftLeft` ainda valida o count;
`maskedShiftLeft` e `maskedShiftRight` aplicam count módulo width.

Endianness não altera o valor numérico. A memória nativa segue o target e a ABI.
Serialização escolhe uma ordem:

```w
let wire = 0x0102_0304_u32.toBytes(order: .big)
let restored = u32.fromBytes(wire, order: .big)
```

`.little` e `.big` são estáveis. `.native` é target-dependent e não serve como
formato persistente ou de rede. Float serializa sua representação com `toBits`
e as mesmas operações integer.

#### 15.1.4 Floating point e ordem

**Exemplo:** `0.0_f64 / 0.0_f64` produz NaN. Ele não causa panic nem ativa
fast-math.

`f32` e `f64` seguem IEEE binary32 e binary64. Operações básicas usam
round-to-nearest, ties-to-even, preservam subnormals e não reassociam. Divisão
por zero, overflow e operação inválida produzem os valores IEEE. `a * b + c`
não vira FMA no mode strict; use `math.fma(a, b, c)`.

```w
let unordered = f64.nan.partialCompare(1.0) // none
let ordered = f64.totalOrder(-0.0, 0.0)     // .less
let key = TotalFloat(0.0)
```

Store, copy, serialization por bits e `toBits` preservam signed zero e payload
de NaN. O resultado NaN de uma operação continua NaN, mas seu sign e payload não
são portáveis. `==`, `<`, `<=`, `>` e `>=` seguem comparação IEEE: NaN não é
igual a si e `-0.0 == 0.0`.

Por isso, floats não conformam a `Equatable`, `Hashable` ou uma ordem total.
`partialCompare` retorna `Ordering?`. `f32.totalOrder` e `f64.totalOrder`
implementam a ordem total IEEE. `TotalFloat<T>` fornece equality, hash e ordem
compatíveis para keys. `minimum` propaga NaN; `minimumNumber` seleciona o
número quando somente um operando é NaN.

Safe W não expõe um floating-point environment global. Source comum não muda
rounding mode, flush-to-zero ou exception flags. Uma foreign call que altera
esse estado precisa declará-lo e restaurá-lo na boundary. APIs checked retornam
estado como valor quando o programa precisa observá-lo.

`mode: .fast` permite flags fast-math declaradas pela API.
`mode: .reproducible` usa algoritmo, ordem de reduction e accuracy profile
versionados. Uma flag de release não ativa nenhum desses modes. Transcendentals
ficam em `std.math` T2 e publicam domínio, tratamento de casos especiais e erro
máximo em ULP.

#### 15.1.5 Tipos numéricos T2

**Exemplo:** `FixedDecimal<i128, scale: 2>` representa dinheiro decimal sem
passar por `f64`.

T2 adiciona tipos com custo e domínio explícitos:

```w
let tax: FixedDecimal<i128, scale: 2> = 12.30
let exact = try Rational<BigInt>(22, denominator: 7)
let phase = Complex<f64>(real: 0.0, imaginary: 1.0)

type FlavorQ =
  Quantized<
    i8,
    expressed: f32,
    scale: StaticRatio<1, 128>,
    zeroPoint: 0,
  >
```

`std.math` fornece `BigInt`, `BigUInt`, `Rational` e `Complex`. `std.decimal`
fornece `FixedDecimal`. `std.quant` fornece quantization. Esses módulos são T2
e não entram na prelude.

`BigInt` e `BigUInt` possuem precisão arbitrária, são owned e seguem a policy de
OOM da seção 11.5. `FixedDecimal<Storage, scale:>` usa uma escala integer não
negativa e guarda um coefficient integer vezes `10 ** -scale`. Add e subtract
exigem a mesma escala. Multiply soma as escalas e verifica overflow do storage.
Divide exige result scale e rounding nomeados. Uma conversão de escala também
declara rounding. `Money` continua a adicionar currency e não vira alias
universal de decimal.

`Rational<BigInt>` normaliza sign e greatest common divisor. `Complex<T>` usa
constructors nomeados; W não reserva pontuação para literal complexo.

`f16` e `bf16` são formatos T2 de storage e operand. Seus valores convertem
exatamente para um expected `f32`. Eles não definem scalar arithmetic. Source
escalar usa `.toF32()`; tensor e ML APIs declaram accumulator. Formatos float
de 8 bits não entram no core.
`Quantized<Storage, expressed:, scale:, zeroPoint:>` representa:

```text
expressed = (stored - zeroPoint) * scale
```

`StaticRatio<Numerator, Denominator>` exige denominator positivo e normaliza
sign e greatest common divisor. Assim, a escala exata entra na identidade do
tipo. Scale e zero point são contratos estáticos. Per-axis e
per-block quantization adicionam um eixo e uma lista de parâmetros versionada.
A conversão, o accumulator e a saturation policy ficam explícitos na API.

Os contratos seguem as operações básicas de
[LLVM](https://llvm.org/docs/LangRef.html), os rounding modes de
[MLIR Arith](https://mlir.llvm.org/docs/Dialects/ArithOps/) e a separação entre
storage e expressed type de
[MLIR Quant](https://mlir.llvm.org/docs/Dialects/QuantDialect/).

Posit, Unum, IEEE decimal float e arbitrary-precision real ficam em
**Pesquisa** como tipos T2. Cada candidato precisa definir rounding, special
values, serialization, FFI, vector fallback e differential oracle. Nenhum deles
substitui `f32` ou `f64` no design vigente. Tipos `fast8` ou `fast16` dependentes do target
ficam **Rejeitado por enquanto**; ProofFacts e o optimizer escolhem a largura
física sem mudar o tipo source.

### 15.2 Ranges

**Exemplo:** `1>..<5` contém `2`, `3` e `4`. Ele não aloca nem cria um
iterator.

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
`hasAll`. Um `case` usa `if` para adicionar um guard. O guard não substitui
`&&` em outra expressão Boolean.

Somente tipos discretos/strideable podem iterar um Range. Outros usam
`stride`. `clamp` exige um intervalo fechado.

Os endpoints possuem o mesmo tipo depois das conversões da seção 15.1.2. A
ordem não muda a direção:

- lower menor que upper cria o intervalo indicado;
- endpoints iguais criam um singleton somente em `a...a`;
- lower maior que upper cria um range vazio;
- um bound unordered, como NaN, causa panic na forma de operador;
- `try Range.closed(lower, upper)` retorna error para bounds unordered.

Membership com um valor NaN é false. `clamp` preserva um input NaN. Um Range
float permite comparação e clamp, mas não iteration direta. Iteração direta
exige successor discreto. Uma progressão descendente ou com step usa `stride`:

```w
for countdown in stride(from: 5, through: 1, by: -1) {
  print(countdown)
}
```

O step não pode ser zero e precisa avançar em direção ao endpoint. A forma
direta causa panic quando esse contrato falha; `try Stride(...)` oferece a
construção fallible. `to:` exclui o endpoint e `through:` o inclui. Overflow ao
calcular o próximo elemento encerra somente quando o endpoint já foi alcançado;
caso contrário, causa panic. `range.count()` retorna
`Result<usize, ArithmeticError>`.

Um range unilateral pode aparecer como argumento ou pattern:

```w
let tail: view Array<Order> = orders[4...]
```

O parser distingue essa forma pelo fim do argumento. `each values` não reutiliza
o mesmo token.

### 15.3 Delimitador de unidade

O design vigente compara quatro formas:

| Forma | Estado | Motivo |
|---|---|---|
| `9.81<m/s^2>` | **Forma vigente** | preserva `[]`, comunica aplicação estática e possui precedente no F# |
| `9.81[m/s^2]` | **Reserva DB1** | parse simples, mas parece indexação e sobrecarrega `[]` |
| `9.81{m/s^2}` | **Rejeitado por enquanto** | chaves devem continuar a indicar body/scope |
| `9.81 m/s^2` | **Pesquisa** | aproxima SI, mas não mostra onde a unit expression termina |

O [F#](https://learn.microsoft.com/en-us/dotnet/fsharp/language-reference/units-of-measure)
usa angle brackets em quantidades e apaga units no runtime. Isso é um precedente,
não uma prova de preferência.

No design vigente, o literal exige adjacência:

```w
let gravity = 9.80665<m/s^2>
let setpoint = -40<degC>
let memory = 64<KiB>
```

A produção aceita somente um literal numérico adjacente a
`<unit-expression>`. O sinal continua um operador unary aplicado à quantity.
Uma expressão runtime não usa essa forma. Ela usa `Quantity(value, unit: m)`.

Dentro da unit expression, o design vigente aceita nomes de units, `*`, `/`, `^`,
parênteses e expoentes inteiros. `^` continua XOR fora desse contexto. Nomes
qualificados são permitidos. O literal `1` pode ocupar o numerator
dimensionless, como em `1/mol`; outros coefficients são rejeitados. O resolver
aceita somente símbolos de kind `Unit`.

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

## 16. Texto, bytes e collections

### 16.1 `String` e unidades de texto

**Forma vigente:** `String` é um valor owned, contíguo e UTF-8 válido. Uma mutação
exige acesso exclusivo e preserva UTF-8 válido.

```w
var title = "Last Light"
title.append(" Restaurant")
let bytes = title.bytes.count
let scalars = title.scalars.count
let graphemes = title.graphemes.count
```

`String` pode conter U+0000. Ele não possui terminador NUL obrigatório. SSO,
layout e estratégia de crescimento não fazem parte da API. Cópia e move seguem
a seção 9.

Esse contrato segue a experiência de
[Swift com armazenamento UTF-8](https://www.swift.org/blog/utf8-string/) sem
tornar a representação curta ou COW parte da linguagem.

Contiguidade é uma garantia dos bytes durante um borrow scoped. Ela não promete
um endereço estável depois de move ou mutation. Uma implementação pode usar
estas formas internas:

| Forma | Storage | Allocation |
|---|---|---:|
| vazia | descriptor canônico | não |
| literal | bytes static read-only | não |
| pequena | bytes inline no próprio valor | não, quando o profile oferece SSO |
| dinâmica | buffer UTF-8 único com count, reserva e origem | sim |

O bootstrap W0 começa com literal/static e buffer dinâmico único. Ele não
precisa de SSO para compilar W. A representação dinâmica mínima contém pointer,
byte count, reserva física e allocator origin. Esses fields são internos e não
formam ABI pública.

`copy text` possui custo semântico O(bytes) e produz outro owner. A baseline
duplica storage dinâmico durante o `copy`. COW não entra na baseline, pois pode
mover allocation failure, budget e deallocator para uma mutation posterior. O
optimizer só pode compartilhar storage quando prova que allocation, identity,
endereço, budget, failure e cleanup não observam a mudança.

Um literal pode continuar em storage static depois de `copy`, pois não possui
owner dinâmico nem reference count. A primeira mutation materializa storage
único quando necessário. `w explain memory` informa essa transição.

`String` não possui `length`, `count` ou subscript direto. O programa escolhe
uma destas unidades:

| Forma | Elemento | `count` | Percurso |
|---|---|---:|---:|
| `text.bytes` | `u8` da codificação UTF-8 | O(1) | O(bytes) |
| `text.scalars` | `UnicodeScalar` | O(bytes) | O(bytes percorridos) |
| `text.graphemes` | grapheme cluster estendido | O(bytes) | O(bytes percorridos) |

```w
let sign = "A🇧🇷e\u{301}"
expect sign.bytes.count == 12
expect sign.scalars.count == 5
expect sign.graphemes.count == 3
```

`text.bytes` é uma view read-only, inclusive quando o owner possui acesso
exclusivo. Nenhuma API safe altera um byte isolado e cria UTF-8 inválido.

```w
var sign = "A🇧🇷"
sign.bytes[0] = 0xff_u8 // Erro: a view de bytes é read-only.
sign.append("!")        // Válido: a operação preserva UTF-8.
```

`UnicodeScalar` é `Copy`. Ele contém um scalar Unicode válido e nunca contém
surrogate. W não define um tipo universal chamado `Char` ou `Character`.
Um elemento de `graphemes` é um
`view String<(.graphemes.count == 1)>`. Ele empresta um cluster contíguo.

```w
for scalar in sign.scalars {
  print("U+${scalar.hex}")
}

for grapheme in sign.graphemes {
  print(grapheme)
}
```

Um valor owned que exige um único grapheme usa `String` refinado. O refinement
não cria um tipo `Character` escondido.

```w
type MenuGlyph = String<(.graphemes.count == 1)>

let planet: MenuGlyph = "🪐"
let invalid = try MenuGlyph("ab") // Falha: dois graphemes.
```

Um único grapheme não possui limite universal de bytes ou scalars. Por isso,
`MenuGlyph` não autoriza storage inline por si só. Um limite de bytes precisa
estar no contrato quando a capacidade física importa.

```w
type CompactGlyph =
  String<(.graphemes.count == 1 && .bytes.count <= 32)>
```

### 16.2 Views, índices e slices

W separa quatro conceitos que outras APIs chamam de “imutável”:

| Forma | Garantia |
|---|---|
| `let T` | o binding não recebe outro valor |
| `ref T` | acesso read-only a um place completo |
| `view T` | descriptor read-only de uma projeção borrowed |
| `inout view T` | acesso exclusivo a uma projeção de extent fixo |
| tipo sem operação mutating | a interface pública não oferece mutation |

Nenhuma forma promete imutabilidade transitiva de um grafo com atomics,
capabilities ou aliases `shared`. W não adiciona um wrapper universal
`Readonly<T>`. Essa forma esconderia interior mutation e não resolveria
ownership.

Read-only é uma permissão de acesso, não uma segunda cópia do type system. Um
generic lê um valor completo por `ref T`:

```w
fn checksum<T: Hashable>(value: ref T): u64
```

O compiler também calcula um fato interno de imutabilidade profunda. Um valor
owned em binding `let` recebe esse fato somente quando todo storage alcançável é
value storage e não contém `shared`, atomic, object identity, capability,
pointer externo ou outro canal de mutation. O fato permite constant folding,
sharing seguro e diagnostics. Ele não cria syntax, conformance pública nem
conversão implícita. `w explain type value` mostra a primeira razão que impede a
prova.

```w
let courses = ["broth", "cake"]       // profundamente imutável
let counter = try share(Atomic(0_u64)) // não: atomic permite mutation observável
```

Uma API que precisa prometer um snapshot recebe ou devolve um valor owned.
`ref T` impede escrita por aquele acesso, mas outro alias autorizado ainda pode
mudar o owner. `view T` tem a mesma limitação temporal.

**Forma vigente:** `view` é um access mode genérico. W não publica uma família
`StringView`, `Slice<T>`, `MutableSlice<T>` ou `CStringView`:

```w
fn firstWord(line: ref String): view String?
fn serve(orders: view Array<Order>)
fn reprioritize(orders: inout view Array<Order>)
```

`ref T` empresta o place completo e preserva identity e metadata do owner.
`view T` empresta uma projeção lógica. Ela não possui capacity, allocator,
identity ou authority para mudar o extent. `inout view T` permite mutation
dentro do extent, mas não permite append, resize ou substituição do owner.

Esta separação evita dois erros. `view` não é um sinônimo menor para `ref`.
Também não é um wrapper que torna qualquer grafo profundamente imutável.
O [`Ref` de Swift](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0519-borrow-inout-types.md)
representa uma referência a uma instância. O
[`Span` de Swift](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0447-span-access-to-contiguous-storage.md)
e os [slices de Rust](https://doc.rust-lang.org/reference/types/slice.html)
representam sequências borrowed. W mantém a mesma distinção de função, mas usa
um access mode comum para as famílias que possuem uma projeção verificável.

```w
fn inspectOwner(values: ref Array<Order>) {
  print(values.capacity)
}

fn inspectWindow(values: view Array<Order>) {
  print(values.count)
  print(values.capacity) // Erro: uma view não possui capacity.
}
```

Uma view read-only é `Copy`; a cópia duplica somente seu descriptor e mantém a
mesma provenance. Uma view exclusiva é move-only. Nenhuma delas mantém o owner
vivo, cria reference counting ou estende seu lifetime.

O source não escreve lifetimes. Um retorno `view T` deve apontar para o receiver
ou para um parâmetro borrowed explícito. A interface compilada registra cada
origem possível. O compiler rejeita uma view de local storage.

Uma view pode atravessar `await` somente quando o owner permanece estável no
task frame. Ela pode entrar em um child estruturado somente quando o join
precede o fim do owner e as regras de exclusividade continuam válidas. `view`
não torna um owner shareable e não autoriza escape por task detached.

O descriptor depende da família lógica:

| Forma | Projeção e representação portátil |
|---|---|
| `view String` | bytes UTF-8 contíguos em boundaries válidas |
| `view Bytes` | bytes contíguos |
| `view Array<T>` | elementos `T` contíguos |
| `view CString` | bytes contíguos com NUL validado |
| `view Tensor<T, ...>` | base, shape e strides; pode não ser contígua |

`view` vence `slice` e `span` porque uma projeção pode ser textual ou strided,
e não somente uma sequência contígua. `borrow` duplicaria `ref`. `readonly`
descreveria a permissão, mas não a perda intencional de owner metadata. Esses
nomes permanecem alternativas documentadas; não ficam reservados como
keywords.

Criar uma view não aloca. A interface registra owner lógico, element type, index
model, contiguidade, shape/strides, mutabilidade e provenance. Esses fatos não
exigem o mesmo layout runtime para todas as famílias.

Somente `Array`, fixed array, `Bytes`, `String`, `CString` e `Tensor` publicam
views no design vigente. Um tipo de usuário expõe uma view dessas sobre seu storage ou um
borrow nominal próprio. Uma extensão futura para views customizadas precisa de
um modelo de descriptor verificável; um protocol comum não pode inventar
provenance.

`inout view` existe para `Array`, fixed array, `Bytes` e `Tensor` quando a
projeção permite escrita. O design vigente não oferece `inout view String` nem
`inout view CString`. Escrita arbitrária poderia invalidar UTF-8 ou o
terminador. Uma mutação de texto usa `inout String`; um buffer C mutável usa
`inout view Bytes` e valida o contrato sentinela quando volta à fronteira.

Views expõem somente a superfície lógica que o tipo define para essa forma.
Methods que exigem owner, identity, allocator ou capacity não participam.
Generic algorithms usam `Iterable` e os outros protocols de collection; eles não
dependem de um protocol universal `View`.

Materialização é explícita e produz o owner lógico:

```w
let prompt: view String = "End of service"
let owned: String = prompt.materialize() // Usa a policy normal de OOM.
let recovered = try prompt.tryMaterialize(using: memory)
```

`materialize()` segue a policy normal de allocation. `tryMaterialize(using:)`
retorna `AllocationError`. Ela copia conteúdo; uma view borrowed não pode adotar
o storage do owner.

`view String` empresta uma subsequência UTF-8 contígua. `String` e suas
projeções oferecem somente views que preservam boundaries válidas.

```w
fn firstWord(line: ref String): view String? {
  return line.scalars.split(where: (scalar) => scalar.isWhitespace).first
}
```

`text.bytes[usize]` tem acesso aleatório O(1). As views de scalar e grapheme não
aceitam um ordinal em subscript. Isso evita esconder uma busca O(n).

```w
let firstByte: u8 = text.bytes[0]
let secondScalar = text.scalars.element(at: 1) // busca explícita O(n)
```

As operações de índice publicam o custo:

| Operação | Bytes | Scalars ou graphemes |
|---|---:|---:|
| `start` e `end` | O(1) | O(1) |
| `index(after:)` | O(1) | O(bytes do elemento) |
| `index(i, offsetBy: n)` | O(1) | O(bytes atravessados) |
| `distance(from:to:)` | O(1) | O(bytes atravessados) |
| slice com índices válidos | O(1) | O(1) |

```w
let start = text.graphemes.start
let third = text.graphemes.index(start, offsetBy: 2)
let traversed = text.graphemes.distance(from: start, to: third)
```

`ScalarIndex` e `GraphemeIndex` pertencem ao source que os criou. O type checker
rastreia essa origem como um borrow sem annotation pública. O programa não pode
usar o índice em outro source nem mutar o owner enquanto o índice está vivo.

```w
let start: ScalarIndex = text.scalars.start
let end = text.scalars.index(start, offsetBy: 4)
let prefix: view String = text.scalars[start..<end]

// Erro: `start` empresta `text`, não `other`.
let invalid = other.scalars[start..<end]
```

Um slice de bytes retorna `view Bytes`. Ele pode cortar a codificação de um
scalar. A conversão de byte range para `view String` valida os dois limites.

```w
let raw: view Bytes = text.bytes[1..<4]
let word: view String = try text.view(bytes: 1..<4)
let owned: String = word.materialize()
```

Uma falha de boundary informa qual extremidade é inválida. O offset é contado
em bytes desde o início do mesmo source.

```w
enum Utf8BoundaryError: Error {
  outOfBounds(offset: usize)
  startInsideScalar(offset: usize)
  endInsideScalar(offset: usize)
}

let flag = "🇧🇷"
let invalid = try flag.view(bytes: 1..<flag.bytes.count)
// Falha: `.startInsideScalar(offset: 1)`.
```

Uma mutação invalida views e índices. O borrow normalmente rejeita a mutação.
Um adapter unsafe deve restabelecer a mesma regra.

```w
let segment = text.scalars[start..<end]
text.append("!") // Erro: `segment` mantém um borrow ativo.
print(segment)
```

Índices do mesmo owner podem terminar seu borrow na própria operação de edição.
Isso permite localizar e substituir sem converter offsets para integers. A
mutação começa depois da avaliação do range. O range não pode ser usado depois.

```w
var title = "Last Light"
let start = title.scalars.index(title.scalars.start, offsetBy: 5)
let end = title.scalars.end
title.replace(scalars: start..<end, with: "Course")
expect title == "Last Course"

print(start) // Erro: a edição invalidou o índice.
```

Uma replacement view que empresta o mesmo owner mantém um alias ativo. O
programa materializa o replacement ou usa outro source.

```w
let suffix = title.scalars[start..<end]
title.replace(scalars: start..<end, with: suffix) // Erro: alias ativo.
```

### 16.3 `Bytes` e conversão UTF-8

`Bytes` é um valor owned, contíguo e move-first para dados binários. Ele pode
conter qualquer byte. `Bytes` e `Array<u8>` são tipos distintos.

```w
var packet: Bytes = b"\x89PNG\r\n\x1a\n"
packet.append(0xff_u8)
let byte: u8 = packet[0]
```

`Array<u8>` expressa uma collection numérica genérica. `Bytes` expressa um
payload binário, um digest ou uma operação de I/O. Uma conversão que copia,
move ou muda a interpretação é explícita.

```w
let encoded = Bytes(copying: text.bytes)
let text = try String.fromUtf8(encoded)
let repaired = String.replacingInvalidUtf8(encoded)
```

As três conversões principais tornam ownership e allocation visíveis:

| Forma | Resultado | Custo e owner |
|---|---|---|
| `String.viewFromUtf8(bytes)` | `view String` validada | O(bytes), sem cópia |
| `String.fromUtf8(bytes)` | `String` | O(bytes), copia |
| `String.adoptingUtf8(take bytes)` | outcome owned | O(bytes), transfere o carrier |
| `(take text).intoBytes()` | `Bytes` | sem validação e sem allocation geral |

As formas borrowed aceitam `Bytes` e `view Bytes`. A forma adopting exige
`Bytes`, pois uma view não possui sua allocation.

```w
let borrowed = try String.viewFromUtf8(payload)
let copied = try String.fromUtf8(payload)

let adopted = String.adoptingUtf8(take ownedPayload)
let text = switch adopted {
  case .text(let value): value
  case .invalid(let bytes, let error): recover(bytes, after: error)
}
```

`String` e `Bytes` usam um carrier owned compatível em T0. Isso não torna seus
tipos ou layouts públicos iguais. A compatibilidade garante que
`adoptingUtf8` valida e transfere o carrier sem copiar o payload nem pedir uma
allocation geral. Uma forma inline pode mover seus bytes limitados dentro do
valor. Uma falha devolve o mesmo owner de `Bytes`:

```w
enum Utf8Adoption {
  text(String)
  invalid(Bytes, Utf8Error)
}
```

A conversão inversa consome `String`, remove a invariant UTF-8 e preserva
allocator origin:

```w
let packet: Bytes = (take text).intoBytes()
```

Ela não valida nem adiciona terminador. Um borrow continua usando `text.bytes`;
`intoBytes()` existe somente quando o caller precisa do owner binário.

`Utf8Error.offset` aponta para o início da primeira maximal subpart inválida.
`length` informa os bytes dessa subpart. `reason` não depende do decoder usado.

```w
enum Utf8Reason {
  invalidLeadingByte
  invalidContinuation
  overlongEncoding
  surrogate
  outOfRange
  unexpectedEnd
}

struct Utf8Error: Error {
  offset: usize
  length: usize
  reason: Utf8Reason
}
```

A classificação usa esta precedência no primeiro offset que não pode ser
convertido:

| Prefixo | Motivo |
|---|---|
| `C0..C1`, `E0 80..9F` ou `F0 80..8F` | `.overlongEncoding` |
| `ED A0..BF` | `.surrogate` |
| `F4 90..BF` ou `F5..FF` | `.outOfRange` |
| `80..BF` quando um leader era esperado | `.invalidLeadingByte` |
| leader válido seguido por byte incompatível | `.invalidContinuation` |
| prefixo válido ainda incompleto em `finish` | `.unexpectedEnd` |

```w
let overlong = String.fromUtf8(b"\xC0\xAF")
// error: offset 0, length 1, reason `.overlongEncoding`
```

`String.fromUtf8` para no primeiro erro. A forma
`replacingInvalidUtf8` substitui cada maximal subpart inválida por um U+FFFD.
Ela nunca consome uma subsequência UTF-8 válida adjacente. W não repara texto de
forma implícita.

```w
let invalid = b"\x66\x6f\x80"

do {
  let _ = try String.fromUtf8(invalid)
  panic("invalid UTF-8 was accepted")
} catch error {
  expect error.offset == 2
  expect error.length == 1
  expect error.reason == .invalidLeadingByte
}

let repaired = String.replacingInvalidUtf8(b"\xF0\x80\x80A")
expect repaired == "���A"
```

O decoder incremental preserva uma sequência parcial entre chunks. Ele mantém
no máximo três bytes pendentes. Somente `finish()` classifica uma sequência
pendente como `.unexpectedEnd`. O offset continua relativo ao início do stream.

```w
var decoder = Utf8Decoder()
try decoder.push(b"table \xF0\x9F")
try decoder.push(b"\xAA\x90")
let label = try (take decoder).finish()
expect label == "table 🪐"
```

`Utf8RepairDecoder` aplica a mesma regra de maximal subpart sem lançar
`Utf8Error`.

```w
var decoder = Utf8RepairDecoder()
decoder.push(b"\xE1\x80")
let repaired = (take decoder).finish()
expect repaired == "�"
```

UTF-8 não possui byte order. As APIs core preservam um U+FEFF inicial como
conteúdo. Um adapter de arquivo ou protocolo só remove a assinatura quando sua
policy nomeada permite.

```w
expect try String.fromUtf8(b"\xEF\xBB\xBFmenu") == "\u{FEFF}menu"

let source = try TextFile.decode(
  payload,
  encoding: .utf8,
  signature: .consumeIfPresent,
)
```

Essas regras seguem a
[definição de maximal subpart do Unicode 17](https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-3/)
e a [orientação de BOM do Unicode](https://www.unicode.org/faq/utf_bom.html).

### 16.4 Construção e concatenação

Interpolação é a forma canônica para construir texto com valores de tipos
diferentes. Cada valor atende ao protocol `Display`.

```w
let message = "Order ${order.id} has ${order.guests} guests"
```

`Display.write` grava no `String` de destino. O default `display()` cria esse
destino e o retorna. A interpolação baixa para uma única operação de construção
e chama `write` para cada segmento. Ela não exige um `String` intermediário por
campo.

```w
protocol Display {
  fn write(to output: inout String): ()
}

extension Display {
  fn display(): String {
    var output = String()
    write(to: inout output)
    return output
  }
}
```

Uma implementação escreve diretamente:

```w
extension Course: Display {
  fn write(to output: inout String): () {
    output.append(switch self {
      case .nebulaBroth: "Nebula broth"
      case .photonSouffle: "Photon soufflé"
      case .quietSalad: "Quiet salad"
      case .horizonCake: "Horizon cake"
    })
  }
}
```

`+` produz um novo owner, mas consome o operand esquerdo e empresta o direito.
Ele pode reutilizar a reserva esquerda. `+=` e `append` mutam um `String`
exclusivo. Semanticamente, o operator recebe `take String` à esquerda e
`view String` à direita:

```w
let joined = (take left) + right
```

Last-use inference move o operand esquerdo. Se ele ainda for usado depois, o
source precisa escrever `copy left + right`. `append` também aceita
`view String`; um `String`, literal ou grapheme view fornece esse borrow sem
materialização. Uma view que precisa virar owner usa `materialize()`. W não
converte números ou objetos nesses operadores.

```w
var greeting = "Hello"
greeting += ", universe"
let question = greeting + "?"
```

Uma cadeia de `+` move o intermediate e pode reutilizar seu buffer. Construção
em loop continua preferindo reserve/append, pois cada nova parcela ainda precisa
ser copiada e growth pode ocorrer.

`String` mantém uma reserva interna. `String(reservingBytes:)` torna o mínimo
inicial explícito em loops ou construções grandes. O formatter pode sugerir
reserva e `append` para uma cadeia de `+`.

```w
var output = String(reservingBytes: rows.count * 32)

for row in rows {
  output.append(row.name)
  output.append("\n")
}

let report = output
```

O valor exato da reserva não é uma property pública. Isso permite SSO e
estratégias de crescimento diferentes sem mudar a API. As operações públicas
são:

| Operação | Contrato |
|---|---|
| `String()` | vazio sem allocation |
| `String(using:)` | vazio ligado ao allocator fornecido |
| `String(reservingBytes:, using:)` | solicita uma reserva mínima |
| `reserve(minimumBytes:)` | usa a policy normal de OOM |
| `tryReserve(minimumBytes:)` | retorna `AllocationError` e mantém o valor na falha |
| `append(view String)` | copia bytes UTF-8 válidos para o fim |
| `append(UnicodeScalar)` | codifica e anexa um scalar |
| `replace(scalars:, with:)` | substitui um range com índices do mesmo owner |
| `clear()` | esvazia e mantém a reserva |
| `reset()` | esvazia e libera storage dinâmico |
| `takeAll()` | devolve o conteúdo owned e deixa o receiver vazio |
| `(take text).intoBytes()` | consome texto válido e devolve seus bytes |

`reserve` e `tryReserve` recebem o total mínimo, não bytes adicionais. Depois de
reservar `n`, mutations não alocam enquanto o resultado possui no máximo `n`
bytes. Uma implementação pode reservar mais. A recipe fixa a growth policy
quando budget ou failure fazem o tamanho físico observável.

```w
var line = String(using: memory)
try line.tryReserve(minimumBytes: expectedBytes)
line.append(prefix)
line.append('🪐')
```

Overflow de tamanho falha antes de alterar o valor. `tryReserve` oferece strong
failure guarantee. Código que precisa recuperar de OOM calcula o tamanho final,
executa `tryReserve` e depois faz as mutations.

`clear()` serve para reutilização em loop. `reset()` devolve storage ao allocator
e preserva a allocator origin para growth futuro. `takeAll()` transfere o
conteúdo sem uma cópia dinâmica; uma forma inline pode copiar somente seu limite
fixo dentro do novo valor.

```w
var frame = String(reservingBytes: 4096)
frame.append(chunk)
let complete = frame.takeAll()
expect frame.bytes.count == 0
```

Uma mutation não aceita uma source view do mesmo owner. Essa regra evita um
temporary oculto e mantém o borrow model uniforme:

```w
let suffix = line.scalars[start..<end]
line.append(suffix) // Erro: source e destination possuem o mesmo owner.
```

O programa materializa `suffix` ou usa uma operação futura que declare
explicitamente self-copy. O design vigente não cria esse temporary de forma implícita.

W não concatena literais adjacentes. W também não concatena valores separados
somente por whitespace.

```w
let invalid = "Last" "Light" // Erro: falta `+` ou interpolação.
```

### 16.5 Literais de texto e bytes

Uma string normal aceita escapes e interpolação `${expression}`. Uma raw string
usa o par `#"` e `"#`. Ela desativa escapes e interpolação.

```w
let normal = "line\norder ${order.id}\u{2026}"
let raw = #"C:\orders\${notInterpolation}"#
```

Uma string multiline normal permite interpolação. Uma raw multiline combina
o delimitador raw com as regras de multiline.

```w
let card = """
  guest: ${guest.name}
  course: ${order.course}
  """

let template = #"""
  ${thisStaysLiteral}
  C:\last-light
  """#
```

O newline após o delimitador inicial não entra no valor. O newline antes do
delimitador final também não entra. A coluna do delimitador final define o
dedent. Cada linha não vazia deve ter pelo menos essa indentação. O compiler
normaliza CRLF e CR para LF antes dessa regra.

```w
let menu = """
  broth
    horizon-cake
  """

expect menu == "broth\n  horizon-cake"
```

Escapes normais de `String` são `\\`, `\"`, `\n`, `\r`, `\t`, `\0` e
`\u{scalar}`. `\xNN` não entra em `String`, pois ele descreve bytes.

```w
let bell = "\u{1F514}"
let zero = "\0"
let invalid = "\x80" // Erro: use `b"\x80"`.
```

Um literal `'λ'` contém exatamente um `UnicodeScalar`. Um literal `b'A'`
contém um `u8`. Um byte literal aceita ASCII direto ou `\xNN`.

```w
let scalar: UnicodeScalar = 'λ'
let ascii: u8 = b'A'
let high: u8 = b'\xFF'
let header: Bytes = b"WPKG\x00\x01"
```

Uma byte string aceita ASCII e escapes de byte. Ela não aceita interpolação.
Texto Unicode vira bytes somente por uma conversão UTF-8 explícita.

### 16.6 Igualdade, normalização e segmentação

Equality, hash e ordenação estável comparam a sequência UTF-8 exata. Em UTF-8
válido, essa ordem também corresponde à ordem de scalars. Ela não é collation
linguística.

```w
let composed = "é"
let decomposed = "e\u{301}"

expect composed != decomposed
expect composed.normalized(.nfc) == decomposed.normalized(.nfc)
```

`String`, `view String` e literais comparam texto sem materializar outro
`String`. O hash da mesma sequência é igual em todos esses access modes.

```w
let verb: view String = command.scalars[start..<end]
expect verb == "place"
```

Normalização exige `.nfc`, `.nfd`, `.nfkc` ou `.nfkd`. Busca caseless, locale,
collation e transliteração também usam APIs nomeadas. Elas não alteram `==`.

```w
let key = input.normalized(.nfc)
let ordered = names.collated(using: portugueseRules)
```

A edição fixa a versão do bundle Unicode. Graphemes seguem os extended
grapheme clusters default da UAX #29. Tailoring de locale pertence a T2 e
declara seu profile.

```text
$ w explain unicode
Unicode version: 17.0.0
Grapheme profile: UAX29-C1-1
```

O semantic fingerprint contém a edição Unicode, os digests das tabelas e os
profiles usados. Um refinement avaliado com essas tabelas usa o mesmo
fingerprint.

```w
type OneGlyph = String<(.graphemes.count == 1)>

$ w explain unicode OneGlyph
Unicode version: 17.0.0
Tables digest: sha256:...
Refinement profile: UAX29-C1-1
```

Uma atualização de Unicode pode mudar boundaries de grapheme para texto ainda
não segmentado. Ela não muda os bytes persistidos. Índices não podem ser
persistidos porque emprestam um source e uma edição.

```w
store.put("name", value: copy guest.name)
store.put("index", value: graphemeIndex) // Erro: índice emprestado.
```

Normalização possui estabilidade própria, mas case folding, spoof detection e
segmentação continuam versionados. Nenhuma dessas operações é aplicada a uma
`String` por padrão.

```w
let folded = input.caseFolded(profile: .default)
let report = unicode.security.checkIdentifier(input)
```

O bundle também fixa normalização e regras de identificadores. A baseline usa
[UAX #15](https://www.unicode.org/reports/tr15/),
[UAX #29](https://www.unicode.org/reports/tr29/),
[UAX #31](https://www.unicode.org/reports/tr31/) e
[UTS #39](https://www.unicode.org/reports/tr39/). Tabelas geradas e testadas
evitam depender da versão Unicode do sistema.

### 16.7 Texto nativo do host

`OsString` preserva argumentos e nomes nativos sem perda. Em Unix, ele pode
conter bytes que não são UTF-8. Em Windows, ele pode conter unidades UTF-16
sem pareamento.

```w
let native: OsString = context.process.arguments[0]
let text: String = try native.toString()
let label: String = native.displayLossy()
```

`Path` preserva a representação nativa. `Utf8Path` exige UTF-8 válido, mas
mantém as regras de path do target. Filesystem, argv e environment não usam
`String` como substituto de `OsString`.

```w
let nativePath = Path(native)
let utf8Path = try Utf8Path(nativePath)
let restored = Path.fromUtf8(utf8Path)
```

Conversão nativa para `String` ou `Utf8Path` é fallible. `displayLossy` serve
somente para UI e diagnostics. APIs do host rejeitam NUL quando o sistema não
consegue representá-lo.

`PackagePath` é portátil. Ele usa UTF-8 NFC, `/`, componentes relativos e
nenhum componente `.` ou `..`.

```w
let source = try PackagePath("src/restaurant/menu.w")
```

Dois nomes que normalizam para o mesmo `PackagePath` são uma colisão. O package
reader rejeita o segundo antes de extrair ou compilar arquivos.

```w
let first = try PackagePath("menu/é.w")
let second = try PackagePath("menu/e\u{301}.w")
expect first == second
// Um archive que contém ambos falha com `PackagePathCollision`.
```

Codecs T1 convertem formatos externos. Locale do processo nunca muda a
interpretação de `String`.

```w
let legacy = try TextCodec.windows1252.decode(payload)
```

O [`OsString` de Rust](https://doc.rust-lang.org/std/ffi/struct.OsString.html)
é um precedente para preservar texto nativo sem forçar UTF-8.

### 16.8 C strings e buffers sentinela

`CString` é um buffer owned com terminador NUL. Sua construção rejeita NUL
interno. `view CString` empresta o mesmo contrato.

```w
let name = try CString.from("last-light")

name.withPointer((pointer) => unsafe {
  c_register_restaurant(pointer)
})
```

Um pointer C recebido precisa de limite máximo. O wrapper procura o terminador
dentro desse limite e valida a codificação separadamente.

```w
let bytes: view CString = unsafe {
  try CString.view(pointer, maxBytes: 4096)
}
let text = try bytes.decodeUtf8()
```

`WideCString` cobre APIs Windows que exigem UTF-16 terminado em zero. Arrays e
strings W não recebem terminação sentinela como semântica geral.

```w
let wide = try WideCString.from("Última Luz")
```

O [`CString` de Rust](https://doc.rust-lang.org/std/ffi/struct.CString.html)
é um precedente para separar ownership, NUL e lifetime do pointer.

### 16.9 Representações especializadas

Um refinement limita valores. Ele não promete layout ou capacidade.

```w
type ShortLabel = String<(.scalars.count <= 40)>
type GuestName = String<(.graphemes.count in 1...80)>

let buffer = String(reservingBytes: 4096)
```

`InlineString<capacity: N>` continua **Pesquisa** como tipo de storage. Rope,
piece table, interning e a tree string histórica também ficam em tipos ou
packages especializados.

```w
let label: InlineString<capacity: 64> = try InlineString("Last Light")
let document = Rope.from(largeText)
```

O compiler pode escolher storage especializado para um refinement quando a
escolha for invisível. Um ABI que exige capacidade inline usa um tipo físico.
A hipótese de tree string permanece em `Y/W` porque favorece compartilhamento,
mas aumenta metadata e indireções no caso comum.

SSO invisível e `InlineString` público resolvem problemas diferentes. SSO reduz
allocations sem prometer threshold, tamanho ou layout. `InlineString` promete
que um limite físico faz parte do tipo e precisa definir overflow, conversão,
ABI e tamanho por target.

O primeiro protótipo de `String` usa a forma flat semelhante ao
[`String` de Rust](https://doc.rust-lang.org/stable/alloc/string/struct.String.html):
UTF-8 válido sobre um buffer growable. O protótipo seguinte compara SSO com a
[representação UTF-8 pequena de Swift](https://www.swift.org/blog/utf8-string/)
e com storage inline explícito como
[`SmallString` do LLVM](https://llvm.org/doxygen/classllvm_1_1SmallString.html).
Nenhum threshold entra no contrato antes dos benchmarks.

Literal/static, inline e dinâmica precisam produzir os mesmos resultados,
errors, índices e bytes. Sanitizers, debug e ABI C usam o fallback flat quando a
forma compacta não preserva tooling ou provenance.

Raw access permanece scoped:

```w
unsafe text.bytes.withPointer((pointer, count) => consume(pointer, count))
```

O borrow bloqueia move e mutation até o fim da closure. O pointer não escapa.
Uma API C persistente usa `CString` quando precisa de NUL ou consome e fixa um
buffer:

```w
let bytes = (take text).intoBytes()
let stable = try pin take bytes
```

O compiler W0 precisa somente deste subset:

- literal UTF-8 e `String()` vazio;
- validação UTF-8 nas fronteiras;
- `bytes`, equality, hashing e lexicographic comparison;
- `reserve`, `tryReserve`, `append`, `clear` e `takeAll`;
- `view String`, `view Bytes` e índices de byte;
- `fromUtf8`, `adoptingUtf8` e `intoBytes`;
- move, `copy`, drop e allocator origin.

Grapheme segmentation, normalization, locale, SSO, COW e texto indexado não são
pré-requisitos do self-host. O compiler pode usar scalars somente onde a syntax
Unicode exigir. Assim, o primeiro compiler W não depende da camada Unicode
completa que ele próprio ajudará a gerar.

O tipo de contagem determina a prova disponível. Um limite de bytes limita
storage diretamente. Um limite de scalars fornece um limite conservador de
quatro bytes por scalar. Um limite de graphemes não fornece limite de bytes.

```w
type PacketLabel = String<(.bytes.count <= 64)>      // até 64 bytes
type ScalarLabel = String<(.scalars.count <= 64)>   // até 256 bytes
type VisualLabel = String<(.graphemes.count <= 64)> // bytes sem limite finito
```

Os sketches históricos `min`, `max`, `expected`, `mask` e `inputType` não viram
knobs universais de `String`. Refinement define invariants. Reserva define uma
estimativa. Property behavior ou newtype define transformação.

```w
type TaxId = String<(.scalars.count == 11 && TaxIdRules.isValid(value))>
let likelyLarge = String(reservingBytes: expectedBytes)
let taxId = try TaxId.fromMasked(input)
```

### 16.10 Collections

#### 16.10.1 Formas e inferência

`Array<T>`, `Map<K, V>` e `Set<T>` possuem storage dinâmico owned. `[T; count]`
possui count estático e storage inline:

```w
let courses: Array<Course> = [.broth, .cake]
let prices: Map<Course, Money> = [.broth: 12[cr], .cake: 42[cr]]
let capabilities = Set([.network, .clock])
let digest: [u8; 32] = [0; 32]
```

`[a, b]` cria `Array<T>` quando não existe um tipo esperado. `[key: value]`
cria `Map<K, V>`. W não reserva chaves para collections e não possui literal de
set. Records anônimos continuam em `()`:

```w
let queue = [orderA, orderB]
let lookup = ["tea": 1, "cake": 2]
let point = (x: 10, y: 20)
let unique = Set(queue)
```

`[]` é válido quando o contexto determina o elemento. Sem contexto, o compiler
solicita um tipo. Os constructors explícitos cobrem o outro caso:

```w
var orders: Array<Order> = []
let pending = Array<Order>()
let prices = Map<Course, Money>()
let seen = Set<OrderId>()

let unknown = [] // error: element type is not known
```

`[value; count]` cria um array fixo, avalia `value` uma vez e exige `Copy`.
Construção independente de valores move-only usa uma closure:

```w
let zeroDigest: [u8; 32] = [0; 32]
let trays = Array.generate(count: 8, using: (index) => Tray(id: index))
let invalid = [Tray(); 8] // error: Tray is not Copy
```

`[T]` como tipo dinâmico e `[:]` como empty map permanecem rejeitados. Eles
confundem type, literal e shape ou criam uma exceção sem ganho de capacidade.

#### 16.10.2 `Array` e arrays fixos

`Array<T>` mantém elementos contíguos e expõe `count` e `capacity` em O(1).
Criar um array vazio não aloca. `append` tem custo amortizado O(1);
insert/remove no meio têm O(n):

```w
var courses = Array<Course>()
var staged = Array<Course>(using: request)
courses.reserve(minimumCapacity: 16)
courses.append(.broth)
courses.insert(.cake, at: 0)
expect courses == [.cake, .broth]
```

A estratégia de crescimento e o valor exato de `capacity` não fazem parte da
semântica, da igualdade ou da ABI. `reserve` segue a policy normal de OOM.
`tryReserve` retorna falha recuperável e oferece a strong failure guarantee:

```w
let required = try usize.checkedAdd(buffer.count, packetSize)
try buffer.tryReserve(minimumCapacity: required)
buffer.append(take packet) // não cresce antes de `required`
```

Após uma reserva suficiente, `append` não aloca até atingir essa capacidade.
Isso mantém o owner do elemento no caller quando a reserva falha. Um array
criado com `using` preserva esse allocator em todo growth e drop.

O subscript por índice faz bounds check e panic quando o contrato local foi
violado. `get` representa input fallible:

```w
let first = courses[0]
let optional = courses.get(userIndex) // ref Course?
```

Um subscript de array é um place. Ler um elemento `Copy` copia o valor; um
elemento move-only é borrowed conforme o contexto. `take array[index]` é erro
porque criaria um buraco. Remoção owned usa uma operação que preserva ou declara
a mudança de ordem:

```w
let recipe: ref Recipe = recipes[0]
let removed = recipes.remove(at: 0)       // O(n), preserva ordem
let quick = recipes.swapRemove(at: 0)     // O(1), pode mudar ordem
let hole = take recipes[0]                // error
```

`[T; count]` possui count no tipo. Ele aceita indexação e views, mas não muda de
count:

```w
var window: [f32; 4] = [0.0; 4]
window[2] = 1.0
let dynamic = Array(window)
```

`Array<T>` atende a `Duplicable` quando `T` atende. A operação preserva count e
ordem, mas não promete a mesma capacity:

```w
let snapshot = copy courses
expect snapshot == courses
```

A baseline contígua e growable possui precedente em
[`Vec<T>`](https://doc.rust-lang.org/std/vec/struct.Vec.html). W fixa também a
ordem de cleanup e separa `reserve` de `tryReserve`; portanto o precedente não
é a especificação.

#### 16.10.3 `view` e mutation

Uma faixa de `Array<T>` produz `view Array<T>`. Uma faixa de `Bytes` produz
`view Bytes`. Um binding `inout` solicita a forma exclusiva:

```w
let middle: view Array<Order> = orders[1..<4]
let inout tail: view Array<Order> = orders[4...]
tail[0].priority += 1
```

Range inválido em subscript produz panic. `get(range)` retorna uma view
optional para input externo. Uma view não muda o count do owner:

```w
let payload = bytes.get(packetRange) // view Bytes?
let invalid = bytes[0...bytes.count] // panic: closed upper bound is outside
```

O owner não pode mover, desalocar ou alterar a estrutura enquanto uma view está
viva. Alterar elementos por uma `inout view` continua válido:

```w
let window = orders[0..<2]
orders.append(next) // error: append can relocate storage borrowed by window
print(window[0])
```

Uma pointer C existe somente por um adapter scoped `unsafe`. Ela não estende o
lifetime da view:

```w
unsafe bytes.withPointer((pointer, count) => c_write(pointer, count))
```

#### 16.10.4 Iteração

O protocolo mínimo separa produção de elementos, borrow e consumo:

```w
protocol Iterator<Item> {
  mut fn next(): Item?
}

protocol Iterable<Item> {
  fn iterator(): some Iterator<Item>
}

protocol MutableIterable<Item, MutableItem>: Iterable<Item> {
  mut fn mutableIterator(): some Iterator<MutableItem>
}

protocol ConsumableIterable<Item, OwnedItem>: Iterable<Item> {
  take fn intoIterator(): some Iterator<OwnedItem>
}
```

Um iterator é single-pass. `next()` avança seu estado. `for` avalia a fonte uma
vez e faz o lowering para um dos três métodos:

```w
for course in menu { print(course) }             // borrow
for ref course in menu { inspect(course) }       // borrow explícito
for inout dish in dishes { dish.plate() }         // acesso exclusivo
for copy code in statusCodes { send(code) }       // exige Copy
for dish in take dishes { serve(take dish) }      // consumo owned
```

Array usa `Item = ref T`, `MutableItem = inout T` e `OwnedItem = T`. Map usa
entry types diferentes para borrow, mutation e consumo. Set não atende a
`MutableIterable`, pois seu elemento também é sua key. O primeiro exemplo é o
sugar comum de `for ref`. A forma explícita é útil em API, documentação e
diagnostics. Um producer, como `0..<10`, pode usar `Item = usize` e produzir
valores owned sem uma collection intermediária:

```w
for index in 0..<10 { visit(index) }
```

O source fica borrowed até o fim do iterator. Mutation estrutural durante o
loop é erro, mas mutation do elemento por `inout` é válida:

```w
for item in orders {
  orders.append(item) // error: orders is borrowed by its iterator
}
```

Operations em `Array` são eager. Operations após `.lazy` e operations em
`Iterator` são lazy. `collect()` materializa o resultado:

```w
let names: Array<String> = guests.map((guest) => copy guest.name)
let firstThree: Array<OrderId> = orders.lazy
  .filter((order) => order.isOpen)
  .map((order) => order.id)
  .take(3)
  .collect()
```

Side effects usam `for`; descartar um pipeline lazy produz warning:

```w
orders.lazy.map((order) => audit(order)) // warning: iterator is never consumed
for order in orders { audit(order) }
```

Iteração não cria paralelismo implícito. Trabalho paralelo usa `TaskGroup` e
declara bound e ordem:

```w
let results = try await TaskGroup.parallelMap<.compute>(
  take jobs,
  maxParallelism: cooks,
  order: .input,
  operation: cook,
)
```

#### 16.10.5 `Map`

`Map<K, V>` preserva ordem de inserção e guarda chaves e valores completos. O
lookup usa hashing keyed com seed aleatório, mas a seed e o layout não mudam a
ordem:

```w
var menu: Map<String, Money> = ["broth": 12[cr], "cake": 42[cr]]
menu["broth"] = 14[cr] // mantém a primeira posição
expect menu.keys.collect() == ["broth", "cake"]
menu.remove("broth")
menu["broth"] = 15[cr] // reinsere no final
expect menu.keys.collect() == ["cake", "broth"]
```

Lookup e inserção têm O(1) esperado e O(n) no pior caso. Iteração tem O(count)
e segue a ordem de inserção. `Map` compara full keys após colisão; armazenar
somente o hash é incorreto:

```w
let price: ref Money? = menu["broth"]
expect CollisionKey(1) != CollisionKey(2)
expect mapWithForcedCollision.count == 2
```

A seed é metadata de hardening fornecida pelo runtime e não pode ser lida pelo
programa. Ela não altera valor, ordem, cleanup ou bytes serializados. Um host
sem entropy anuncia que hash-flood hardening está indisponível:

```w
let policy = Runtime.current.hardening.hashFlood
// .seeded em hosts normais; .unavailable em um seed freestanding mínimo.
```

Isso não concede uma random capability. Um service que recebe keys não
confiáveis pode exigir `.seeded` no manifest ou escolher `SortedMap`.

O subscript é um optional place. Optional binding escolhe borrow, mutation ou
copy:

```w
if let ref price = menu["cake"] { print(price) }
if let inout price = menu["cake"] { price += 1[cr] }
if let copy price = menu["cake"] { send(price) }
menu["tea"] = 9[cr]
```

Assignment substitui e destrói o valor anterior. `insert` devolve o valor
anterior quando o caller precisa dele. `entry` evita dois lookups:

```w
let old = menu.insert(10[cr], for: "tea") // Money?
let inout count = ordersByGuest.entry(take guestId).orInsert(0)
count += 1
```

`remove` devolve a key original e o valor. `removeValue` descarta a key e
devolve somente o valor. Ambos preservam a ordem restante. `swapRemove` declara
que pode trocar a posição do último elemento:

```w
let removed: MapEntry<String, Money>? = menu.remove("tea")
let value: Money? = menu.removeValue(for: "cake")
let fast: MapEntry<String, Money>? = menu.swapRemove("broth")
```

Um literal avalia entries da esquerda para a direita. Duplicata conhecida é
diagnostic. Duas keys que se tornam iguais em runtime mantêm a posição e a key
da primeira entry, mas usam o último valor:

```w
let invalid = ["cake": 1, "cake": 2] // error: duplicate literal key
let merged = [normalize(a): 1, normalize(b): 2]
```

Igualdade de `Map` compara pares, não ordem de inserção. `Map` não atende a
`Hashable` por default:

```w
expect ["a": 1, "b": 2] == ["b": 2, "a": 1]
```

`Map<K, V>` atende a `Duplicable` quando key e value atendem. A cópia preserva
ordem de inserção, mas recebe storage e hash seed próprios:

```w
let menuSnapshot = copy menu
expect menuSnapshot.keys.collect() == menu.keys.collect()
```

Uma key armazenada só aparece como `ref K`; safe W não permite mudar equality
ou hash no lugar. Um lookup borrowed pode usar uma view equivalente sem alocar:

```w
let name: view String = request.pathSegment(0)
let dish: ref Dish? = dishes.get(name) // Map<String, Dish>
```

Iteração borrowed produz uma entry com `ref K` e `ref V`. Iteração `inout`
mantém a key read-only e entrega `inout V`. Iteração consuming entrega key e
value owned:

```w
for entry in menu { print("${entry.key}: ${entry.value}") }
for inout entry in menu { entry.value.applyDiscount() }
for entry in take menu { archive(take entry.key, take entry.value) }

for inout entry in menu {
  entry.key.normalize() // error: a stored key is never mutable
}
```

`EquivalentKey<K>` exige que a view produza o mesmo hash feed e a mesma
igualdade da key owned. `view String` atende a `EquivalentKey<String>`. Uma
conformance que viola a lei é erro de contrato em teste e pode causar
diagnostic dinâmico em builds instrumentados.

Ordem de inserção possui precedentes no
[dictionary de Python](https://docs.python.org/3/reference/datamodel.html#dictionaries)
e em
[`LinkedHashMap`](https://docs.oracle.com/en/java/javase/25/docs/api/java.base/java/util/LinkedHashMap.html).
Hashing keyed possui precedente em
[`HashMap`](https://doc.rust-lang.org/std/collections/struct.HashMap.html).
W combina as propriedades e não herda a ordem arbitrária do último.

Um map inteiramente compile-time pode baixar para comparação, switch, tabela
ordenada ou perfect hash. A escolha não muda a ordem lógica:

```w
const opcodeNames: Map<String, u8> = ["heat": 0x02_u8, "serve": 0xff_u8]
```

**Pesquisa:** uma variante sem ordem só entra se benchmarks mostrarem ganho
material. `UnorderedMap` é um nome mais honesto que `HashMap`, porque `Map` já
usa hashing. A variante precisa declarar iteration order, hash-flood policy,
cleanup e serialization antes de promoção.

Map não possui uma representação binária universal. Um codec declara se usa
insertion order ou canonical key order:

```w
let displayBytes = codec.encode(menu, mapOrder: .insertion)
let signedBytes = codec.encode(menu, mapOrder: .canonicalByKey)
```

#### 16.10.6 `Set`

`Set<T>` possui a mesma ordem e política de hash de `Map`. A primeira inserção
define a posição; repetir um elemento não muda a ordem:

```w
var courses = Set([.cake, .broth, .cake])
expect courses.collect() == [.cake, .broth]
expect !courses.add(.cake)
expect courses.add(.salad)
```

`contains` não aloca. `remove` devolve o elemento owned. Igualdade ignora a
ordem, e `Set` não atende a `Hashable` por default:

```w
expect courses.contains(.broth)
let removed: Course? = courses.remove(.broth)
expect Set([.cake, .salad]) == Set([.salad, .cake])
```

Um elemento de Set é uma key. Iteração nunca entrega `inout T`; o programa
remove, altera e reinsere:

```w
for inout course in courses { course.rename() } // error
guard let course = courses.remove(.cake) else panic("cake fixture is missing")
let _ = courses.add(course.withLabel("final cake"))
```

`Set<T>` atende a `Duplicable` quando `T` atende. A cópia preserva a ordem
lógica e não compartilha mutation:

```w
var snapshot = copy courses
let _ = snapshot.add(.broth)
expect !courses.contains(.broth)
```

#### 16.10.7 Hashing e keys

`Hashable` exige `Equatable`. Valores iguais alimentam um `Hasher` com a mesma
sequência lógica:

```w
protocol Hashable: Equatable {
  fn hash(into hasher: inout Hasher)
}

struct GuestId: Hashable {
  raw: u64

  fn hash(into hasher: inout Hasher) {
    hasher.append(raw)
  }
}
```

Hash e equality são puros, totais e nonthrowing. O compiler pode sintetizar a
implementação quando todos os fields semânticos atendem aos contratos:

```w
struct Coordinate: Hashable {
  x: i32
  y: i32
} // síntese: x e depois y
```

O algoritmo de `Hasher`, a seed e o resultado não são source, ABI ou storage
contracts. Um hash de processo nunca vira ID persistente, digest de package ou
nome de symbol:

```w
let localBucket = Hasher.processLocal.hash(key)
let artifact = Sha256.tagged("w-artifact-v1", bytes) // digest persistente
```

Map e Set sempre guardam o valor completo e confirmam equality após o hash.
HH32, XXH64 e algoritmos futuros podem ser candidatos de implementação, não
identidades públicas.

#### 16.10.8 Ordenação e busca

`sort()` é stable e exige uma total order. Ele mantém a ordem relativa de
elementos equivalentes e possui O(n log n) no pior caso:

```w
var tickets = [
  Ticket(priority: 1, id: 7),
  Ticket(priority: 1, id: 9),
]
tickets.sort(by: (left, right) => left.priority.compare(right.priority))
expect tickets.map((ticket) => ticket.id) == [7, 9]
```

`sortUnstable()` declara que equivalentes podem mudar de posição e não exige
buffer auxiliar proporcional a n:

```w
numbers.sortUnstable()
```

`sorted()` cria outro `Array`. O comparator retorna `Ordering`; ele é puro,
nonthrowing e não altera a collection:

```w
let ranked = tickets.sorted(by: (a, b) => a.score.compare(b.score).reversed())
```

Floating-point usa uma total order nomeada. Usar `<` diretamente em sort é erro
porque NaN quebra a ordem:

```w
samples.sort(by: f64.totalOrder)
samples.sort(by: (a, b) => a < b) // error: Bool comparator is not Ordering
```

`binarySearch` distingue match de insertion point:

```w
switch sortedIds.binarySearch(42) {
  case .found(let index): use(sortedIds[index])
  case .insertion(let index): sortedIds.insert(42, at: index)
}
```

O algoritmo exato pode mudar por tipo, target e edição se estabilidade,
complexidade, memória e resultado permanecerem iguais. `std.algorithm` pode
oferecer `radixSort` e outros algoritmos com preconditions explícitas:

```w
std.algorithm.radixSort(inout packetIds, radix: 8)
```

Timsort, fluxsort, blitsort e outros candidatos só entram após licença,
provenance, fuzzing e benchmarks reproduzíveis. O nome de um algoritmo não vira
o contrato de `sort()`.

A separação entre sort stable e unstable possui precedente nas
[operations de slice de Rust](https://doc.rust-lang.org/std/primitive.slice.html).
W escolhe stable como default para reduzir surpresa; benchmark ainda decide o
lowering.

#### 16.10.9 Collections especializadas e cleanup

`SortedMap<K, V>` ordena por key e suporta range queries. Ele exige total order,
não `Hashable`:

```w
let arrivals = SortedMap<Date, Guest>()
for entry in arrivals[opening..<closing] { admit(entry.value) }
```

`Deque`, `PriorityQueue` e `BitSet` ficam em `std.collections`. `LinkedList` e
concurrent collections não entram em T0:

```w
import { Deque, PriorityQueue } from std.collections
```

O T0 contém `Array`, arrays fixos, `view Array<T>`, `inout view Array<T>`, `Map`, `Set`,
`Range`, `Iterator`, `Iterable`, `MutableIterable`, `ConsumableIterable`,
`Hashable` e `Hasher`. Isso fecha lexer, parser, symbol table, worklist e
diagnostics do compiler W0.

Destruição é observável e possui ordem definida. Array e array fixo destroem em
ordem inversa de índice. Map e Set destroem em ordem inversa de inserção.
`clear()` usa a mesma ordem:

```w
var resources = [Resource(id: 1), Resource(id: 2)]
resources.clear()
expect DropLog.ids == [2, 1]
```

Capacity, buckets e tree nodes continuam detalhes de implementação. Uma
collection concorrente futura precisa declarar atomicidade, progress guarantee,
snapshot e iteration order; ela não é um `Map` com atomics escondidos.

## 17. Matrizes, tensors e ML

Nested arrays são a forma canônica:

```w
let transform: Matrix<f32, rows: 2, columns: 3> = [
  [1.0, 0.0, 10.0],
  [0.0, 1.0, 20.0],
]
```

`[1 2; 3 4]` fica preservado como alternativa. Ele perde no design vigente porque usa
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

`@` possui uma família fechada para ranks 1 e 2:

| Operandos | Resultado | Operação |
|---|---|---|
| `[K] @ [K]` | scalar | produto interno |
| `[M, K] @ [K]` | `[M]` | matrix-vector |
| `[K] @ [K, N]` | `[N]` | vector-matrix |
| `[M, K] @ [K, N]` | `[M, N]` | matrix-matrix |

Um exemplo completo:

```w
let visits: Matrix<f32, rows: 2, columns: 3> = [
  [1.0, 2.0, 3.0],
  [4.0, 5.0, 6.0],
]

let weights: Matrix<f32, rows: 3, columns: 2> = [
  [1.0, 0.0],
  [0.0, 1.0],
  [1.0, 1.0],
]

let scores = visits @ weights
expect scores == [[4.0, 5.0], [10.0, 11.0]]
```

Uma `Matrix<f32, rows: 2, columns: 3>` não multiplica uma
`Matrix<f32, rows: 4, columns: 2>`. O diagnostic mostra as dimensões internas
`3` e `4`. Scalar expansion e broadcast não participam de `@`.

Rank maior usa `tensor.batchMatmul` com batch shape explícito. Contração geral
usa `tensor.contract` com eixos nomeados. Essa divisão evita inferir eixos,
broadcast ou permutation pelo operador.

Indexação multi-rank usa uma lista de índices:

```w
let score = forecast[table, course]
let row: view Tensor<f32, shape: [courses]> = forecast[table]
```

Fornecer todos os índices produz um elemento. Fornecer um prefixo produz uma
view da rank restante. `forecast[table][course]` é válido, mas materializa a
operação intermediária no source e pode formar uma view temporária.

Regras:

- `*`, `/`, `+`, `-` são elementwise para shape igual;
- scalar expansion é total;
- `@` usa somente a família rank-1/rank-2 definida acima;
- broadcast entre shapes diferentes é explícito;
- slicing retorna `view Tensor<T, ...>`;
- `materialize()` produz um Tensor owned;
- host/device transfer é explícita;
- random recebe generator/seed;
- reduction declara a policy numérica;
- autodiff é transformação tipada de biblioteca/IR, não annotation.

Um element refinement não é preservado automaticamente por `@`. O resultado
usa a tabela abaixo, salvo quando `tensor.matmul<R>` declara outro result type.

```w
type Signal = Int<(1...128)>

let samples: Matrix<Signal, rows: 16, columns: 64>
let weights: Matrix<Signal, rows: 64, columns: 8>
let scores: Matrix<Int, rows: 16, columns: 8> = samples @ weights
```

Integer `@` preserva overflow verificado. Float `@` usa o mode `.strict`.
`tensor.matmul<R>` expõe o tipo lógico de redução/resultado e outro mode quando
o programa precisa mudar esses contratos. A seção 18 define as otimizações
permitidas.

O element type define o resultado de `@`:

| Elemento | Resultado de `@` | Accumulator strict |
|---|---|---|
| signed integer até 64 bits | `Int` | `Int` ou largura menor provada |
| unsigned integer até 64 bits | `UInt` | `UInt` ou largura menor provada |
| `i128` ou `u128` | mesmo tipo | mesmo tipo ou largura menor provada |
| `f16` ou `bf16` | `f32` | `f32` |
| `f32` | `f32` | `f32` |
| `f64` | `f64` | `f64` |
| `Quantized` | sem operator `@` | API quantized explícita |

Para integer, `@` é uma operação de domínio com widening fixo; ele não cria uma
promoção para `+`, `*` ou outros operators. Cada prefixo da redução mantém o
overflow do result type. Uma prova pode escolher um accumulator físico menor
somente quando preserva o mesmo valor e o mesmo ponto de panic.

`tensor.matmul<R>` converte os inputs para `R` pelas regras explícitas da API e
verifica cada prefixo da redução em `R`. Ele retorna `Tensor<R, ...>`. Assim,
`tensor.matmul<i32>` não é somente uma optimization hint.

```w
let accumulated =
  tensor.matmul<i32>(samples, weights: weights, mode: .strict)
let output = try quant.requantize(
  accumulated,
  as: FlavorQ,
  rounding: .nearestEven,
  saturation: .clamp,
)
```

Inputs com element types diferentes não promovem silenciosamente. A API nomeia
dequantization, cast, result ou accumulator. Requantization declara destination
scale, rounding e saturation. Calibration e seleção de scale são tooling; elas
não ocorrem durante uma call normal.

StableHLO e ONNX são adapters. Eles não definem a semântica completa de W.

## 18. Performance e custo

### 18.1 Contrato

Performance não muda a semântica da linguagem. Um profile otimizado precisa
preservar:

- valor e tipo lógico;
- panic, error, cancellation e cleanup observáveis;
- ordem de effects;
- ownership, aliasing e provenance;
- policy numérica;
- ABI, persistência e FFI declaradas.

**Exemplo:** `-O3` não transforma overflow verificado em wrap. Ele pode remover
o check somente quando prova que o overflow é impossível.

```w
type SmallCount = Int<(1...128)>

fn doubled(value: SmallCount): Int {
  return value + value // fato: resultado em 2...256
}
```

O resultado de `doubled` é o mesmo em todos os profiles. Um profile pode usar
uma operação estreita e reestender o valor antes de uma boundary. Outro pode
usar a largura de `Int`.

O source declara invariants e intenção. Ele não escolhe instruções. SIMD,
unrolling, inlining, fusion, storage compression e library dispatch são
decisões do artifact. Uma API que precisa de layout, device ou policy numérica
os declara com tipos ou argumentos próprios.

```w
let exact = left @ right
let approximate = tensor.matmul(left, right, mode: .fast)
```

Uma recipe idêntica produz os mesmos bytes do artifact. Outra versão do
compiler ou outro target pode escolher instruções diferentes. A recipe inclui
compiler, target, CPU features, profile, PGO input e semantic bundles.

### 18.2 Fatos de prova

A HIR mantém `ProofFacts` separados do tipo lógico e do layout. O conjunto
inicial inclui:

| Fato | Origem típica | Uso |
|---|---|---|
| intervalo inteiro | refinement, comparison, range ou `switch` | checks, largura e SIMD |
| nonzero | refinement ou guard | remover check de divisão |
| enum case-set | subset ou flow narrowing | exhaustividade e branch elimination |
| comprimento | array, string refinement ou const parameter | bounds e reserva |
| shape e stride | Tensor type ou view | fusion, tiling e bounds |
| alignment | allocation e layout interno | vector load e low bits |
| alias e escape | owner, `ref`, `inout` e capture | vectorização e stack/region |
| float class | guard explícito | remover branches; nunca ativa fast math |
| unit e scale | tipo de unit | eliminar conversões estáticas |

```w
fn ratio(total: Int, count: Int): Int {
  guard count != 0 else return 0
  return total / count // o caminho possui o fato `count != 0`
}
```

Facts surgem de tipos, refinements, controle de fluxo, enum subsets,
const values, ownership e target. O programador não escreve annotations de
otimização.

```w
fn label(stage: ServiceStage): String {
  guard let active = try? WorkStage(stage) else return "terminal"

  return switch active {
    case .reserving: "reserve"
    case .preparing: "prepare"
    case .serving: "serve"
  }
}
```

Um predicate arbitrário continua válido como invariant, mas o optimizer usa
somente fatos que consegue extrair e verificar. Não entender um predicate
reduz performance; não altera correção.

```w
type TaxId = String<(
  .scalars.count == 11 && TaxIdRules.isValid(value)
)>

// O compiler conhece `scalars.count == 11`.
// Ele não deduz fatos internos de `TaxIdRules` sem uma regra verificada.
```

O passe inicial normaliza intervalos, case-sets, equalities, congruences,
comprimentos, shapes e relações de alias. Passes posteriores podem descartar um
fato somente quando também descartam toda transformação que depende dele.

### 18.3 Refinements, largura e storage

Um refinement não muda o carrier público. Ele pode mudar quatro escolhas
internas independentes:

1. largura da operação;
2. largura do accumulator;
3. largura da lane SIMD;
4. largura do storage não escapante.

**Exemplo:** dois valores `Int<(1...128)>` exigem oito bits unsigned para cada
operando. O produto exige 14 bits. A soma de 64 produtos exige 21 bits.

| Expressão | Intervalo provado | Bits mínimos sem sinal |
|---|---:|---:|
| `value` | 1...128 | 8 |
| `left * right` | 1...16_384 | 14 |
| soma de 64 produtos | 64...1_048_576 | 21 |

```w
type FlavorSignal = Int<(1...128)>

let samples: Matrix<FlavorSignal, rows: 16, columns: 64>
let weights: Matrix<FlavorSignal, rows: 64, columns: 8>
let score: Matrix<Int, rows: 16, columns: 8> = samples @ weights
```

O lowering pode carregar lanes de oito bits, multiplicar em 16 bits e acumular
em 32 bits. O resultado continua `Int`. Se uma dimensão ou um intervalo não
provar o limite, o compiler usa a largura base ou mantém checks.

A aritmética de intervalos inclui overflow na própria análise. Para
`[a, b] + [c, d]`, o fato candidato é `[a+c, b+d]`. Para multiplicação, o
compiler verifica os quatro produtos dos extremos. Se um cálculo de prova
excede a precisão ou a quota, o resultado vira `unknown`.

```w
fn combine(left: SmallCount, right: SmallCount): Int<(2...256)> {
  return left + right // a conversão para o refinement é provada.
}
```

Um valor runtime sem essa prova continua a usar construção fallible:

```w
let bounded = try Int<(2...256)>(input)
```

Storage especializado só pode aparecer quando não cruza uma boundary de
layout. Fields de structs com layout canônico, ABI, FFI, persistência,
reflection física e address exposure usam o carrier declarado.

```w
struct PublicReading {
  value: Int<(1...128)> // layout de Int no struct
}

fn local(values: take Array<Int<(1...128)>>): Int {
  // O buffer pode usar storage estreito se não escapar e o cost model aprovar.
  return values.sum()
}
```

Uma compressão interna precisa considerar unpack, alignment, vector width,
cache e code size. Menos bytes não é sempre mais rápido. `w explain
performance` informa quando o compiler recusa a compressão.

### 18.4 Texto e collections

Texto possui custos que fazem parte da API:

| Operação | Custo sem cache obrigatório | Allocation |
|---|---:|---:|
| `bytes.count` | O(1) | não |
| byte access | O(1) | não |
| scalar ou grapheme ordinal | O(bytes atravessados) | não |
| equality diferente cedo | O(prefixo comum) | não |
| normalização | O(bytes) | resultado owned |
| `copy text` dinâmico | O(bytes) | outro owner |
| `append(source)` | O(bytes de source), amortizado | somente se a reserva não basta |
| `replace` | O(bytes movidos + replacement) | somente se a reserva não basta |
| `clear()` | O(1) | não; mantém storage |
| `reset()` | O(1) + deallocation | não |
| `takeAll()` | O(1) ou cópia inline limitada | não |
| `String.fromUtf8` | O(bytes) | copia |
| `String.viewFromUtf8` | O(bytes) | não copia |
| `String.adoptingUtf8` | O(bytes) | não; transfere carrier |
| `(take text).intoBytes()` | O(1) ou cópia inline limitada | não |
| `left + right` | O(bytes de right) amortizado se left é reutilizado; O(total) no fallback | somente se a reserva esquerda não basta |

**Exemplo:** um parser usa bytes para localizar ASCII estrutural e cria views
somente depois de validar boundaries.

```w
fn commandName(line: ref String): view String throws Utf8BoundaryError {
  let separator = line.bytes.firstIndex(of: b' ') ?? line.bytes.count
  return try line.view(bytes: 0..<separator)
}
```

A implementação pode validar UTF-8 com SIMD, manter um fast path ASCII e criar
summaries atualizados durante construction ou mutation. Reads de `String` não
alocam e não alteram o owner. Portanto uma cache lazy por owner não pode aparecer
na baseline. Uma String não volta a ser validada a cada iteração porque sua
construção já prova UTF-8 válido.

Um profile pode guardar bits ou contagens eager, como `isAscii`, desde que toda
mutation os atualize. Index checkpoints, segment trees e caches alocantes
pertencem a um tipo especializado, como um futuro `IndexedText`, e aparecem em
`w explain performance`.

Refinements de texto oferecem provas diferentes:

```w
type WireName = String<(.bytes.count <= 64)>
type ScalarName = String<(.scalars.count <= 64)>
type DisplayName = String<(.graphemes.count <= 64)>
```

`WireName` prova até 64 bytes. `ScalarName` prova até 256 bytes.
`DisplayName` não prova um limite estático finito de bytes. O optimizer não converte
contagem visual em capacidade física.

`String(reservingBytes:)` e `append` formam o caminho público de custo previsível
para construção. Interpolation escreve num único `String` de destino. A
implementação pode usar um buffer interno, mas esse buffer não é outro tipo
público. Uma sequência longa de `+` pode receber um diagnostic de performance,
mas mantém o mesmo significado.

```w
var report = String(reservingBytes: orders.count * 48)
for ref order in orders {
  report.append("Order ${order.id}\n")
}
let text = report
```

### 18.5 Matrizes, SIMD e devices

Shape estático elimina validações de dimensão e fornece trip counts para tiling
e vectorização.

```w
fn transform<const rows: usize>(
  input: ref Tensor<f32, shape: [rows, 64]>,
  weights: ref Tensor<f32, shape: [64, 8]>,
): Tensor<f32, shape: [rows, 8]> {
  return input @ weights
}
```

O borrow model fornece alias facts. Dois inputs `ref` não podem ser tratados
como disjuntos sem prova adicional. Um output novo é disjunto. Um `inout`
exclusivo não possui outro alias acessível durante a call.

```w
fn scale(values: inout Tensor<f32, shape: [128]>, factor: f32): () {
  values *= factor // a exclusividade permite vectorização in-place.
}
```

Para integers, `@` usa a semântica de operações verificadas. O compiler pode
usar um accumulator mais largo. Ele remove checks intermediários somente quando
prova que cada prefixo lógico cabe no carrier. Um check final basta para uma
redução monotônica com esse fato. Nos demais casos, o lowering preserva os
checks por etapa. Uma API explícita escolhe outro tipo lógico de
redução/resultado:

```w
let score: Matrix<i32, rows: 16, columns: 8> =
  tensor.matmul<i32>(samples, weights: weights, mode: .strict)
```

O `@` reduz a dimensão de contração por índice crescente, de zero a `K - 1`.
Para float, `.strict` arredonda multiply e add separadamente nessa ordem. Ele
não reassocia nem contrai as duas operações. `mode: .fast` autoriza
reassociation, FMA e kernels aproximados conforme um profile publicado.
`mode: .reproducible` pode usar outra árvore, mas fixa algoritmo, chunks e
rounding numa versão para obter o mesmo resultado nos targets que declaram
suporte.

```w
let strict = observations @ weights
let fast = tensor.matmul(observations, weights: weights, mode: .fast)
let stable = tensor.matmul(observations, weights: weights, mode: .reproducible)
```

Um modo numérico não é uma build flag global. O tipo de resultado não esconde a
policy usada. Trace e `w explain performance` registram kernel, layout,
accumulator, vector width e device.

Transferência de device permanece explícita. Fusion pode eliminar um
intermediate lógico, mas não pode inserir uma transferência oculta.

```w
let deviceWeights = try weights.to(device)
let deviceInput = try input.to(device)
let result = try tensor.matmul(deviceInput, weights: deviceWeights)
let hostResult = try result.to(.host)
```

MLIR `linalg`, `vector`, `shape`, `tensor` e `gpu` preservam estrutura útil para
essas transformações. LLVM recebe range e alias facts somente depois que a HIR
W verifica sua validade.

### 18.6 Observação, PGO e budgets

`w explain performance` separa fato, decisão, estimativa e medição:

```text
$ w explain performance restaurant.oracle::forecast
fact: shape [16, 64] @ [64, 8]
fact: element range 1...128
decision: 8-bit loads, 16-bit multiply, 32-bit accumulator
decision: vector width 256 bits
missed: tensor fusion crosses an observable allocation budget
estimate: 8.5 KiB read, 512 B written
measurement: absent
```

Uma optimization record possui IDs estáveis, source spans e motivos para
aplicar ou recusar um passe. O editor pode mostrar esse record sem prometer uma
instrução específica.

PGO é um build input declarado por digest. Ele pode orientar branch layout,
inlining, specialization e code placement. Ele não pode mudar overload,
const evaluation, refinement, public interface ou resultado.

```w
profiles: [
  {
    name: "release"
    optimize: .speed
    targetCpu: "x86-64-v3"
    pgoUse: "sha256:..."
  },
]
```

O proof engine possui quotas determinísticas para tempo, memória e tamanho de
facts. Intervalos, case-sets, shapes e aliasing formam a baseline. SMT geral e
autotuning que executa código durante o build permanecem **Pesquisa**.

Benchmarks de promoção registram:

- target, CPU, OS, compiler e profile;
- cold e warm execution;
- latency distribution e throughput;
- allocations, peak memory, code size e compile time;
- dataset e digest;
- baseline e intervalo de ruído.

Uma otimização entra no default somente quando melhora sua matriz alvo sem
alterar oracles semânticos. Fallback e differential tests continuam
obrigatórios.

### 18.7 Atomics, locks e contenção

Atomicidade não mede custo. Uma operação pode usar uma instrução, um CAS loop ou
um lock do runtime. `w explain performance` mostra o lowering:

```text
$ w explain performance restaurant.execution::BrigadeMetrics.completed
storage:  Atomic<u64>
order:    relaxed
lowering: atomicrmw add i64
lockFree: true
sharing:  one writer group, four worker threads
warning:  measured cache-line contention at 16 workers
```

Uma order mais fraca não elimina contenção na cache line. Um contador global
pode escalar menos que counters locais combinados no join. O compiler não faz
essa transformação sozinho, pois overflow e snapshots observáveis podem mudar.

Fields atômicos independentes podem causar false sharing. O layout W pode
separá-los quando a mudança é invisível e o profile possui evidência. Um
contrato explícito `Atomic<T, cache: .isolated>` permanece **Pesquisa** porque
altera tamanho, alignment e cache footprint por target.

```w
// Pesquisa: solicita um interference granule exclusivo, não um tamanho fixo.
let visits = Atomic<u64, cache: .isolated>(0)
```

Locks registram wait time, hold time, contention e owner causal no profile de
observabilidade. O runtime não inclui endereço bruto ou thread ID no resultado
do programa.

RCU favorece reads, mas exige publication e reclamation corretas. Trocar um
pointer atomicamente não mantém o objeto anterior vivo. Uma API de snapshot só
avança depois de comparar epochs, hazard pointers e `shared T`.

### 18.8 Perfis de benchmark

**Exemplo:** `last-light-benchmark` executa o mesmo handler em um processo
monolítico e em nanoservices co-localizados.

Um ranking externo não define a semântica W. Ele oferece um workload
reproduzível para encontrar custos em HTTP, JSON, database access, scheduling,
allocation e backpressure.

O produto Última Luz mantém um profile compatível com as famílias públicas do
TechEmpower:

1. JSON serialization;
2. single database query;
3. multiple queries;
4. cached queries;
5. fortunes;
6. data updates;
7. plaintext.

O source oracle implementa as sete rotas. O runtime graph e o deployment
declaram PostgreSQL, cache local, admission e quotas. Esse estado não constitui
um resultado. Ainda faltam runtime HTTP, codec JSON, template adapter, database
adapter, cache, harness e medição.

Antes de throughput, o harness valida:

- method, path, payload, media type, `Server` e `Date`;
- `queries` ou `count` com clamp em `1...500`;
- uma query database distinta por item quando o workload exige;
- uma boundary `Sync` por query no extended protocol do PostgreSQL;
- read-modify-write concluído antes da resposta de updates;
- escaping UTF-8 das fortunes;
- read-through e replacement real na rota de cache;
- ausência de gzip, resposta pré-renderizada e disk log por request;
- limites de admission, pool, pipeline, cache e retained bytes.

**Exemplo:** validação e medição são passos distintos:

```text
w benchmark validate last-light-benchmark \
  --deployment deployments/benchmark.w \
  --harness github:TechEmpower/FrameworkBenchmarks@57d92fbec6f8fd7431bc77326dd0484e60c96e20

w benchmark run last-light-benchmark \
  --deployment deployments/benchmark.w \
  --harness github:TechEmpower/FrameworkBenchmarks@57d92fbec6f8fd7431bc77326dd0484e60c96e20 \
  --evidence results/last-light.wbench
```

`validate` não produz ranking. `run` recusa um artifact, deployment ou harness
que não corresponde ao validation record.

Cada resultado registra:

- versão e configuração do benchmark;
- hardware, kernel, database e network topology;
- source, lock, compiler, runtime e artifact digests;
- concorrência, warmup, duração e número de repetições;
- throughput, latency distribution, errors e resource use;
- todas as diferenças da configuração oficial.

“Primeiro lugar” não é um gate de correção. O gate inicial é completar o corpus
sem bypass semântico, manter os oracles e explicar o custo por request. Depois,
profiles separados medem:

- um processo monolítico;
- nanoservices co-localizados;
- services separados por processo;
- worker/component host;
- allocators e storage adapters permitidos.

As rotas compartilham business logic. Uma variante não pode retornar constants,
remover validação ou usar SQL diferente somente para ganhar o ranking.

O repositório público histórico do FrameworkBenchmarks foi arquivado em 24 de
março de 2026. A versão, a origem do harness e o canal vigente de submissão
precisam ser fixados antes de publicar um resultado.

Fontes primárias:

- [TechEmpower Framework Benchmarks](https://www.techempower.com/benchmarks/);
- [visão dos testes](https://github.com/TechEmpower/FrameworkBenchmarks/wiki/Project-Information-Framework-Tests-Overview);
- [repositório FrameworkBenchmarks](https://github.com/TechEmpower/FrameworkBenchmarks).

Outras fontes primárias da camada de desempenho:

- [LLVM `range` metadata](https://llvm.org/docs/LangRef.html#range-metadata)
  representa intervalos de integer e vectors;
- [MLIR Vector](https://mlir.llvm.org/docs/Dialects/Vector/) preserva operações
  n-dimensionais para lowering retargetable;
- [MLIR Linalg](https://mlir.llvm.org/docs/Dialects/Linalg/) preserva estrutura
  de loops para tiling e library dispatch;
- o [guia de atomics do LLVM](https://llvm.org/docs/Atomics.html) separa
  atomicidade, order e lock-freedom no lowering;
- o [modelo UTF-8 de Swift](https://www.swift.org/blog/utf8-string/) demonstra
  validation na criação, fast paths e storage unificado.

## 19. FFI, unsafe e ilhas de linguagem

### 19.1 Fronteira C

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
| pointer + length | `view Array<T>`, `view Bytes` ou owner correspondente |
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

### 19.2 `fn<Language>`

```w
unsafe fn<C> legacyChecksum(data: c.ptr<const c.uchar>, size: c.size): c.uint {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < size; ++i) {
    hash = (hash ^ data[i]) * 16777619u;
  }
  return hash;
}
```

`fn<Language>` contém source da própria aplicação. Ele não importa uma library
externa. A intenção é igual à de inline assembly, mas o body pertence a uma
linguagem completa.

`C` é o adapter de bootstrap reservado. Outro nome resolve por um adapter
declarado no manifest e fixado no lock. O mesmo alias não pode resolver para
adapters diferentes dentro do product.

O parser W lê a assinatura e delimita o body. Ele preserva os bytes internos como
source opaco. O adapter registrado faz lex, parse, type-check e code generation.
O parser W nunca interpreta C, Rust ou outra linguagem como um subset W.

O adapter também fornece um body scanner. O scanner encontra o delimitador final
segundo as regras lexicais da linguagem externa. Ele não entrega sua AST ao
parser W. Sem o adapter, a ferramenta preserva bytes, mas informa que não pode
validar o fechamento da ilha.

**Alternativa:** um raw body com fence hash permite recovery sem adapter. O
corpus deve comparar a forma braced com `#{...}#` antes do design freeze.

O builder agrupa as funções por:

- adapter e versão de toolchain;
- target, ABI e profile;
- runtime e dependency graph;
- compilation unit declarada.

O adapter gera uma declaration equivalente na linguagem de destino. Ele também
gera uma façade com C ABI para cada símbolo W. O artefato preferido é uma static
system library ou um conjunto de object files. O conteúdo não vira “C”. A façade
somente usa a ABI C do target.

Essa diferença é importante para Rust. Um `staticlib` Rust inclui suas
dependencies e partes do runtime. Várias `staticlib` Rust podem colidir. O
builder agrupa ilhas Rust compatíveis em uma crate e uma unidade de link. A
[Rust Reference](https://doc.rust-lang.org/reference/linkage.html) documenta
essas propriedades.

Somente os símbolos da façade ficam visíveis. O builder gera nomes com package,
module, function e contract digest. A link recipe usa export list ou mecanismo
equivalente do target.

A fronteira aceita somente carriers com layout e ownership definidos:

| Valor W | Carrier da façade |
|---|---|
| scalar C compatível | valor C correspondente |
| view contígua de Array, Bytes ou String | pointer, length e lifetime scoped |
| owned buffer | pointer, length, capacity e função de drop |
| enum fechado | tag e payload com layout declarado |
| `throws E` | status e out value, ou result struct ABI |
| callback | function pointer, context e destroy function |

Rich W types não atravessam diretamente. O compiler gera wrappers W antes e
depois da façade. Um borrow não vira owner. Um panic ou exception não atravessa
a boundary sem um ABI e uma policy explícitos.

Um refinement usa o carrier do base type. Inputs já possuem a prova W. Outputs
são validados antes de recuperar o refined type. Uma falha segue o error contract
da wrapper.

Cada adapter recebe:

- body inline ou source separado da aplicação;
- assinatura W já reduzida a carriers suportados;
- target triple, data layout, sysroot e capabilities;
- imports, flags e dependencies fixados;
- regra de panic, exception, blocking e callback;
- symbol names e source map solicitados.

O [guia de cross-compilation do Clang](https://clang.llvm.org/docs/CrossCompilation.html)
mostra que target triple, sysroot, include paths e library paths não podem vir
do host por acidente.

O adapter devolve:

- object files, static archive ou outro artefato permitido pelo target;
- symbol manifest e libraries nativas exigidas;
- diagnostics mapeados para o source W;
- dependencies descobertas e seus digests;
- metadata de effects, ownership e concurrency;
- provenance completa da invocação.

O recipe fixa todos esses inputs. O builder não executa um comando livre
fornecido pelo package. Um adapter é uma tool target hermética e versionada.

A linguagem externa não precisa usar LLVM ou MLIR. Ela precisa produzir um
artefato compatível com o linker e a façade do target. Compartilhar LLVM pode
habilitar link-time optimization. Isso não prova ABI, layout, runtime ou
ownership. A
[MLIR Dialect Conversion](https://mlir.llvm.org/docs/DialectConversion/)
também exige conversões e regras de legalidade explícitas.

O primeiro adapter é C. Rust, Zig, C++ e Fortran são candidatos naturais quando
o toolchain gera objects para o mesmo target. JS, TypeScript e outras linguagens
com runtime entram somente se um adapter AOT fornecer runtime e artefato
herméticos.

`fn<C>` é **Forma vigente**. `fn<lang: .c>` permanece **Alternativa**. Um source
separado com `from` e compilation units nomeadas permanecem **Pesquisa**.

## 20. Compilador e bootstrap

### 20.1 Planos do sistema

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

### 20.2 Pipeline

```text
UTF-8 source
  → lexer lossless
  → CST com recovery e spans
  → AST
  → HIR tipada com ownership, effects e tasks
  → grafo const, ConstIR e evaluator
  → HIR verificada com ConstValue
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
- contexts const, ConstIR, target facts e materialização;
- tipo, conversion e numeric policy;
- initialization, ownership, borrow e drop edges;
- pointer provenance, address capture, pin e allocation origin;
- errors, cancellation, panic e cleanup scopes;
- task parent/child, mobilidade e execution preference;
- effects/capabilities;
- layout/ABI boundaries;
- source map, diagnostic origin e expansion de sugars.

Um verifier rejeita HIR incompleta antes do lowering.

### 20.3 Dialeto W/MLIR

**Exemplo:** um move validado vira uma operação W de ownership antes de qualquer
bufferization ou lowering para LLVM.

O dialeto W mantém as invariantes que LLVM e os dialetos genéricos não conhecem.
Passes só apagam uma distinção depois de prová-la. O lowering pode usar:

- `func`, `arith`, `math`, `cf` e `scf`;
- `async` como ferramenta de state machine;
- `memref`, `bufferization` e LLVM para memória;
- `tensor`, `shape`, `linalg` e `vector` para ciência;
- `gpu`/SPIR-V/vendor adapters quando o profile permite;
- EmitC para auditoria ou bootstrap limitado.

MLIR bytecode é cache do toolchain, não formato público eterno.

### 20.4 ABI e runtime

**Exemplo:** `fn(Int): Int` pode mudar calling convention entre builds. Somente
`unsafe fn<abi: .c>` promete a ABI C.

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

### 20.5 Bootstrap

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

#### 20.5.1 Fechamento mínimo de `bootstrap.w0`

O subset precisa expressar um lexer, parser, type checker, HIR, diagnostics,
serializer e driver. Ele também precisa chamar o backend pelo adapter C.

| Família | Capacidade mínima |
|---|---|
| source | UTF-8, comentários, módulos, imports e visibility |
| bindings | `const`, `let`, `var`, assignment e definite initialization |
| controle | `if`, `guard`, `switch`, loops, break, continue e return |
| funções | funções livres, `const fn`, `const init`, methods, `static fn`, labels, recursion e calls diretas/por `fn(...)` |
| tipos | scalars, tuples, structs, objects, enums, Option, typed Error e newtypes |
| protocols | primary associated types, composition e dispatch estático; sem existential |
| números | widths fixas, `usize`, checked arithmetic, bit operations e endian explícito |
| dados | String, Bytes, `view`, Array, Map, Set e Range de T0 |
| generics | type parameters, constraints, inference fechada, coherence e monomorphization |
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

O ponteiro fino `fn(...)` pertence ao fechamento. `some fn`, `any fn` e closures
com capture não pertencem ao primeiro seed. Elas podem entrar quando reduzirem o
compiler sem ampliar muito o seed. O primeiro source W0 usa funções nomeadas e
closures sem capture quando necessário.

O profile impõe três regras de determinismo:

1. `Map` e `Set` iteram em ordem de inserção, sem depender da hash seed.
2. Output por key usa `SortedMap` ou sort explícito.
3. Clock, random, locale e environment não entram sem input declarado.

O evaluator CE0 segue a seção 3.6. Ele executa somente ConstIR e usa quotas
fixadas na recipe. O seed C e o core self-hosted comparam ConstValue normalizado.

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

#### 20.5.2 Camadas do compiler

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

#### 20.5.3 Estágios

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

#### 20.5.4 Evolução e recovery

**Exemplo:** se stage C falhar, a recipe recompila `compiler/core-w0` com o seed
C fixado e compara a HIR normalizada.

Uma release W `N` compila o core de `N + 1` dentro de uma janela publicada. Uma
mudança que quebra essa janela exige uma ponte source ou um novo seed versionado.
O seed antigo não precisa aceitar toda edição futura.

O seed é validado com Clang, GCC e MSVC quando o target permitir. Pelo menos uma
rota usa warnings máximos e sanitizers disponíveis. Dependable C informa a
matriz; ele não é um dialect e não autoriza undefined behavior.

O comando futuro `w bootstrap explain` lista stages, compiler parents, source
digests, adapters, environment e pontos de convergência. Um artifact sem essa
recipe pode funcionar, mas não recebe o estado “bootstrap reproduzido”.

### 20.6 Incrementalidade

**Exemplo:** alterar o corpo privado de `parseLine` não recompila importers
quando a interface serializada permanece igual.

Cache é content-addressed por source normalizado, interface das dependências,
edition, target, profile, toolchain e flags semânticas. Type checking ocorre por
módulo. Instâncias generics e outputs de passes possuem chaves próprias.

Um cache miss afeta performance, não resultado. A ferramenta registra o motivo
do miss. Ela não usa timestamps como identidade.

### 20.7 Diagnostics e debug

**Exemplo:** um use-after-move informa o move original, o uso inválido e um
fix-it que propõe `copy` somente quando o tipo atende a `Copy`.

Diagnostics possuem código estável, spans, related spans, fix-its e saída
estruturada. O compilador preserva a regra violada até produzir o diagnóstico.

Debug symbols ficam em sidecar removível. Logical task stacks e source maps de
`fn<Language>` preservam a origem. Remover debug não remove reflection solicitada
nem muda o payload executável além das sections declaradas.

### 20.8 Targets e profiles

W separa quatro identidades:

| Identidade | Pergunta |
|---|---|
| target | Para qual architecture, vendor, system e ABI o código é emitido? |
| host profile | Quais slots, capabilities e regras de lifecycle o host oferece? |
| product | Qual entry, module graph, runtime envelope e artifact kind serão ligados? |
| deployment | Onde os artifacts e instances serão colocados? |

Misturar essas identidades faria `linux`, `CLI`, `server` e `GPU` parecerem
variações da mesma propriedade. Elas não são.

#### 20.8.1 Target identity

O target possui um record canônico:

```text
TargetId = {
  architecture,
  vendor,
  system,
  abiOrEnvironment,
}
```

A forma textual segue triples usuais e pode omitir o componente final quando
ele é `none`. O parser não infere campos pela máquina de build.

Exemplos:

```text
x86_64-unknown-linux-gnu
aarch64-apple-darwin
aarch64-unknown-linux-android
wasm32-wasip3
thumbv7em-none-eabihf
nvptx64-nvidia-cuda
amdgcn-amd-amdhsa
spirv64-unknown-vulkan
```

O schema normaliza aliases antes de calcular a recipe. Um target também fixa
data layout, endianness, pointer width, object format e calling conventions.
CPU, features, sysroot, SDK e linker são campos separados e entram na chave do
artifact.

Um nome aceito pelo LLVM não constitui suporte W. O target W precisa de:

1. backend funcional;
2. runtime subset;
3. host adapter;
4. SDK profile;
5. linker, sysroot e packaging;
6. testes e evidence publicados.

Targets de device, como NVPTX, AMDGPU e SPIR-V, não prometem um processo
standalone. Eles geram objects ou kernels consumidos por um host product.

#### 20.8.2 Host profiles

Um host profile versionado declara:

- slots e suas assinaturas;
- slot default, quando existe;
- capabilities de I/O, clock, random, storage e network;
- lifecycle, shutdown, deadlines e fault boundaries;
- execution domains disponíveis;
- limites de threads, memória, stack e file descriptors;
- regras de dynamic loading e sandbox.

Perfis iniciais:

| Profile | Slot default | Uso |
|---|---|---|
| `native-process@1` | `process.main` | CLI, TUI, daemon e servidor próprio |
| `http-worker@1` | `http.fetch` | request host e nanoservice |
| `mobile-app@1` | `app.start` | Android e plataformas Apple |
| `firmware@1` | `device.reset` | bare metal e RTOS |
| `audio-device@1` | `audio.render` | callback com deadline e sem allocation |
| `accelerator-module@1` | nenhum | kernels chamados por um host |
| `test-harness@1` | `test.run` | testes determinísticos |
| `build-transform@1` | `build.transform` | geração hermética com inputs e outputs tipados |

Um profile pode incluir slots opcionais. O product precisa ligar todos os slots
required. Um `entry` não inventa um slot e um target não concede capability.

#### 20.8.3 Matriz de suporte

Uma linha de suporte declara target, host profile, artifact kind, SDK
capabilities, compiler/runtime version, status e test evidence.

| Tier | Garantia |
|---|---|
| experimental | subset declarado, sem garantia de upgrade |
| tier 3 | build conhecido e manutenção comunitária |
| tier 2 | CI compila e executa o corpus relevante |
| tier 1 | CI obrigatória e releases oficiais |
| long-term | janela de suporte e policy de segurança/depreciação |

Tier não mede a segurança de um programa. Um target estreito pode ser correto
sem oferecer rede, threads, dynamic linking ou Unicode completo.

O plano inicial, ainda sem implementação, usa esta ordem:

| Grupo | Targets candidatos | Primeiro gate |
|---|---|---|
| desktop/server | Linux x86-64 e AArch64; Windows x86-64; macOS AArch64 | process, files, TCP, TLS, tasks e debugger |
| mobile | Android AArch64/x86-64; iOS AArch64 e simulator | lifecycle, package, signing e platform SDK |
| WebAssembly | `wasm32-wasip3` | native async component, capabilities e deterministic host tests |
| embedded | ARM Cortex-M e RISC-V bare metal | no-heap profile, interrupts, MMIO e linker script |
| accelerator | NVIDIA, AMD e SPIR-V devices | kernel subset, address spaces, transfer e launch |
| research | BPF, FPGA/HDL e ASIC descriptions | verifier ou synthesis pipeline específico |

Esta tabela define candidatos, não suporte entregue. A primeira release publica
somente linhas que passaram o gate correspondente.

O backend LLVM fornece muitas architectures. A política de LLVM inicia targets
novos como experimentais. W aplica a mesma prudência e adiciona seus próprios
gates de runtime e SDK.

O Android NDK atual expõe `arm64-v8a`, `armeabi-v7a`, `x86` e `x86_64`. O plano
W começa por AArch64 e x86-64. Outros ABIs entram após evidence de demanda e CI.

WASI 0.3 é a baseline de Component Model. Ela possui `async func`, `stream<T>` e
`future<T>` na Canonical ABI. Esses contratos correspondem melhor ao runtime W
que os adapters de polling do WASI 0.2.

O target `wasm32-wasip3` permanece experimental para W até existir toolchain e
corpus próprios. `wasm32-wasip2` continua como target de compatibilidade. Um
adapter pode satisfazer imports 0.2. Ele não muda a interface W para polling.

O dialeto GPU do MLIR oferece uma abstração intermediária para launch e separa
os address spaces `global`, `workgroup`, `private` e `constant`. Ele não
paraleliza um algoritmo por conta própria. O frontend W precisa provar ou pedir
o mapeamento antes do lowering para NVVM, ROCDL ou SPIR-V.

Fontes primárias:

- [targets configuráveis do LLVM](https://llvm.org/docs/CMake.html);
- [política de targets experimentais do LLVM](https://llvm.org/docs/DeveloperPolicy.html);
- [lançamento do WASI 0.3](https://bytecodealliance.org/articles/WASI-0.3);
- [WIT e seus tipos async](https://component-model.bytecodealliance.org/design/wit.html);
- [dialeto GPU do MLIR](https://mlir.llvm.org/docs/Dialects/GPU/);
- [dialeto NVVM](https://mlir.llvm.org/docs/Dialects/NVVMDialect/),
  [dialeto ROCDL](https://mlir.llvm.org/docs/Dialects/ROCDLDialect/) e
  [dialeto SPIR-V](https://mlir.llvm.org/docs/Dialects/SPIR-V/);
- [ABIs do Android NDK](https://developer.android.com/ndk/guides/abis).

## 21. Packages, builds e releases

### 21.1 Manifest e resolução

`package.w` usa um subset data-only. Ele aceita records, lists, strings,
numbers, size literals, booleans e enum values. Ele não executa imports, loops,
funções ou I/O.

O manifest ocupa o arquivo inteiro. Ele não pode coexistir com import, função,
type ou outro manifest.

```w
package {
  schema: "w.package/1"
  authority: .registry("w")
  name: "last-light/restaurant"
  version: "0.1.0"
  edition: "2026"
  namespace: "restaurant"

  runtimeGraphs: [
    {
      name: "restaurant-edge"
      providers: []
      imports: [
        {
          binding: "last-light"
          protocol: "restaurant.restaurant::RestaurantApi"
          source: .deployment
        },
      ]
      exports: []
      packings: [
        {
          name: "entry-only"
          units: [{ name: "main", entry: true, providers: [] }]
        },
      ]
    },
  ]

  products: [
    {
      name: "last-light-native"
      kind: .executable
      module: "restaurant.app"
      entry: ".default"
      host: "w.host/native-process@1"
      targets: ["desktop"]
      runtime: "restaurant-edge"
      packing: "entry-only"
    },
    {
      name: "last-light-worker"
      kind: .component
      module: "restaurant.worker_app"
      entry: "LastLightWorker"
      host: "w.host/http-worker@1"
      targets: ["wasi"]
      runtime: "restaurant-edge"
      packing: "entry-only"
      limits: {
        http: {
          activeRequests: 1_024
          queuedRequests: 2_048
          queuedBytes: 64MiB
          connections: 8_192
          targetBytes: 16KiB
          headerBytes: 64KiB
          headerFields: 128
          bodyBytes: 1MiB
        }
      }
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
      use: .product
      source: .registry("w")
    },
  ]

  targetSets: [
    {
      name: "desktop"
      targets: [
        "x86_64-unknown-linux-gnu",
        "aarch64-apple-darwin",
        "x86_64-pc-windows-msvc",
      ]
    },
    {
      name: "wasi"
      targets: ["wasm32-wasip3"]
    },
    {
      name: "wasi-compat"
      targets: ["wasm32-wasip2"]
    },
  ]

  build: {
    network: .deny
    environment: []
    profiles: [
      {
        name: "release"
        optimize: .speed
        targetCpu: "portable"
      },
    ]
    constEval: {
      steps: 1_000_000
      heap: 64MiB
      callDepth: 256
      result: 8MiB
    }
  }
}
```

Unknown fields são erro. Extensões usam namespace. O parser do manifest é menor
e independente do parser W completo.

- `package.w` é um formato data-only;
- `package.lock` é obrigatório para build reprodutível;
- o resolver é determinístico e registra sua versão;
- o design vigente usa uma versão por package identity em cada resolution
  realm;
- o resolver escolhe a maior versão compatível no snapshot assinado;
- pre-release exige opt-in;
- aliases são locais e não mudam identity;
- múltiplas versões ficam fora da v0;
- features são aditivas, explícitas e entram na chave do artefato.

#### 21.1.1 Workspace

**Exemplo:** o Última Luz desenvolve o restaurante e o compiler de cardápio no
mesmo checkout, mas cada package mantém identity e release próprias:

```w
workspace {
  schema: "w.workspace/1"
  members: [
    ".",
    "packages/menu-compiler",
  ]
  defaultMembers: ["."]
  patches: []
}
```

`workspace.w` usa o mesmo codec data-only dos outros manifests. Ele é uma
fronteira de desenvolvimento e resolução. Ele não é package, module, product ou
release. O arquivo não é publicado como parte da identidade de um member.

`members` contém `PackagePath` relativos e exatos. A v0 não aceita glob, path
absoluto, `..` ou symlink que saia da raiz. Cada path precisa conter um
`package.w`, e duas entries não podem resolver para a mesma árvore ou identity.
Um workspace com um único member continua válido.

Todos os members compartilham um `package.lock` na raiz e o mesmo CAS. O lock
mantém contexts separados por product, target e usage de dependência. Outputs
continuam imutáveis; packages não escrevem no diretório de outro member.

Uma dependência usa automaticamente um member quando package identity e version
constraint conferem. O field `authority` do member participa dessa prova.
Version incompatível produz error. O resolver não usa uma release do registry
no lugar do member local sem informar o usuário. O lock grava o manifest digest,
o source-set digest e a razão da seleção. A recipe grava o content tree digest.

O discovery local procura o `workspace.w` ancestral mais próximo que liste o
package atual. `w context` mostra manifest, workspace, lock e roots antes de
qualquer mutation. CI e release usam `--workspace <path>` ou `--standalone`;
eles não dependem de discovery ambiental.

`defaultMembers` afeta somente comandos sem seleção, como `w check --workspace`.
Ele não muda dependências ou artifacts. `w publish check` resolve cada member
como package externo, sem substituição automática por workspace. Assim, um
workspace verde não esconde uma dependência que ainda não pode ser publicada.

**Alternativa:** manter configuração de workspace apenas fora do repository
facilita overlays pessoais. Ela torna CI, lock e seleção de members menos
observáveis.

#### 21.1.2 Usages de dependência

**Exemplo:** o restaurante usa o compiler de cardápio durante o build. O
compiler não entra no executable:

```w
dependencies: [
  {
    alias: "menuCompiler"
    package: "last-light/menu-compiler"
    version: "^0.1.0"
    use: .build
    source: .registry("w")
  },
]
```

`use` é obrigatório e possui quatro cases na v0:

| Use | Grafo habilitado | Configuração |
|---|---|---|
| `.product` | modules e artifacts alcançáveis pelo product | target |
| `.build` | tools executadas para produzir inputs | execution |
| `.test` | tests e seus fixtures | test target |
| `.benchmark` | harness e medição | benchmark target |

Uma `.build` dependency não fica importável por um module `.product`. Uma
`.test` ou `.benchmark` dependency não entra em library interface, runtime
graph, release payload ou SBOM do product. Ela aparece na provenance do test ou
benchmark correspondente.

Dependências transitivas preservam seu usage. Um build tool pode ter suas
próprias `.product` dependencies; elas pertencem ao artifact do tool na
configuração de execution. Elas não se misturam com uma versão da mesma library
compilada para o target final.

O resolver deriva o grafo alcançável por root. Uma dependency que product,
action, test, benchmark ou feature não referencia é error. Uma dependency
referenciada somente por feature permanece inativa até um root selecionar essa
feature; isso não é erro nem causa download preventivo.

Cada package declara um `namespace` de module. Todos os modules públicos do
package ficam nessa raiz. O `alias` da dependency substitui essa raiz no source
consumer:

```w
// Package acme/telemetry declares namespace "acme.telemetry".
// The dependency alias is "telemetry".
import telemetry.codec
import { Frame } from telemetry.model
```

O alias é um W identifier e não participa de type identity, ABI ou wire schema.
O compiler resolve `telemetry.codec` para `acme.telemetry.codec` antes de name
lookup. Um module local, std module e dependency alias não podem ocupar a mesma
raiz no mesmo package.

O namespace canônico contém um ou mais W identifiers separados por `.`. Um
module público precisa ser o namespace ou seu descendant. A raiz `std` é
reservada ao SDK. O alias usa um único identifier e pode seguir o vocabulário
local do consumer.

Modules do próprio package usam o namespace canônico. Interfaces e diagnostics
mostram alias e identity resolvida. URL, version e digest não entram no import.

#### 21.1.3 Features sem defaults ocultos

**Exemplo:** uma feature ativa uma dependency inativa e um module set adicional.
O product escolhe a feature:

```w
features: [
  {
    name: "compressed-telemetry"
    enables: [
      .dependency("zstd")
      .moduleSet("telemetry-zstd")
    ]
  },
]

products: [
  {
    name: "observatory"
    features: ["compressed-telemetry"]
  },
]
```

Uma feature é uma seleção aditiva do grafo. Ela pode ativar dependency,
module set, resource ou action declarados. Na v0, source W não possui
`if feature`, annotation ou macro de feature. Código opcional ocupa um module
set próprio e mantém imports normais.

W não cria uma feature implícita para dependency opcional. W também não ativa
um conjunto `default` de outro package. Cada product e cada dependency edge
lista as features que solicita. Lista ausente significa lista vazia.

Features não removem API, não trocam implementation existente e não são
mutuamente exclusivas. Casos incompatíveis usam products, packages ou runtime
configuration distintos. Essa regra permite unir pedidos de features sem
alterar a semântica de um pedido anterior.

O fechamento ocorre dentro de:

```text
ResolutionRealm = root selection + dependency use + target role + target identity
```

`dependency use` é a configuração da seleção raiz. Dependências `.product` de
um build tool continuam no realm `.build` desse tool. Elas não abrem um realm
do payload final.

Dentro do mesmo realm, pedidos para a mesma package identity e version formam a
união aditiva. Product, build tool, test, benchmark e targets diferentes mantêm
realms distintos. O lock e a artifact key gravam o conjunto final e a origem de
cada feature.

`w explain feature <package>::<feature>` mostra os roots e edges que ativaram a
feature. `w diff-lock` separa mudança de version, source e feature.

**Alternativa:** features condicionais dentro de qualquer statement reduzem o
número de modules. Elas aumentam o número de programas possíveis por arquivo e
dificultam interface diff, testes e leitura por ferramentas.

#### 21.1.4 Sources e patches

**Exemplo:** uma dependency pública usa registry. Um root privado pode fixar um
commit Git. O workspace pode testar uma correção local com a mesma identity:

```w
dependencies: [
  {
    alias: "http"
    package: "w/http"
    version: "^1.0"
    use: .product
    source: .registry("w")
  },
  {
    alias: "telemetry"
    package: "acme/telemetry"
    version: "0.8.2"
    use: .product
    source: .git(
      "https://github.com/acme/telemetry.git",
      revision: "9b6d4a1f4bb8f4a8d6935e4b2c1a28cfac70f334",
    )
  },
]

patches: [
  {
    package: "acme/telemetry"
    version: "0.8.2"
    source: .path("patches/telemetry")
  },
]
```

Uma source localiza metadata e source. O manifest encontrado declara a
authority esperada; a source não concede outra package identity. Para uma
release externa, o lock grava tree digest, origin imutável e metadata snapshot.
Para source local editável, a recipe grava o content tree digest.

Package identity é:

```text
PackageIdentity = declared authority + scoped package name
```

Todo `package.w` declara um field `authority`:

```w
authority: .registry("w")
authority: .git("https://github.com/acme/telemetry.git")
authority: .local
```

Um registry authority usa um ID estável ancorado na linhagem de root metadata.
URL, alias local `"w"` e chave atual não são a identidade. Uma rotação
autorizada preserva a linhagem; trocar para uma root sem essa delegação cria
outra authority. Uma Git authority usa a canonical repository identity. A
revision identifica uma source tree, não uma package identity. Mudar registry
authority ou repository cria outra identity, mesmo quando o texto `owner/name`
coincide.

`.local` identifica somente um root não publicável. Ele não pode satisfazer uma
dependency, receber patch ou entrar em release metadata. Um package que precisa
ser compartilhado declara registry ou Git authority desde o início.

O scoped name possui duas partes ASCII lowercase separadas por `/`. Cada parte
começa por letra e contém letras, digits ou `-`, com 1 a 63 caracteres. Registry
metadata controla delegation, transfer e revocation do owner. Unicode fica no
display name, não no identificador usado por filesystem, URL e type identity.

- `.registry(name)` exige a mesma registry authority no package encontrado;
- `.git(url, revision:)` exige a mesma Git authority e um commit completo na
  dependency;
- dependency source `.path(path)` existe somente em `workspace.w` e aponta
  para um `package.w`;
- um member compatível é uma source local implícita e registrada no lock;
- binary artifacts são candidatos de uma release resolvida, não outra source
  declarada na dependency.

Branch, tag, `latest` e URL de archive mutável não entram em `package.w`. Um
comando de conveniência pode resolver uma referência humana, mas grava o commit
imutável antes do build.

`patches` pertence somente à raiz do workspace. O package encontrado precisa
declarar a mesma identity e uma version compatível. Trocar por um fork com outra
identity exige alterar a dependency. Isso impede que um override local mude
silenciosamente type identity ou authority.

Patch ativo entra no lock, recipe, provenance e diagnostics. `w publish check`
rejeita patches. Para publicar a correção, o autor publica uma release da mesma
identity ou usa uma identity nova.

O registry público inicial aceita somente dependencies de release por registry.
Git continua disponível para roots privados e experimentos. Essa policy pode
evoluir sem mudar o formato do manifest.

`--locked` rejeita uma configuração local que aponte o alias de registry para
outra linhagem de authority. Mirrors podem mudar sem trocar a authority, porque
servem objetos já identificados por digest.

#### 21.1.5 Contexts de resolução e lock

**Exemplo:** o mesmo source pode exigir três resolutions sem misturar bytes:

```text
last-light-native / linux-aarch64 / product
menu-compiler     / windows-x86_64 / build
last-light-tests  / linux-aarch64 / test
```

O design vigente mantém no máximo uma version de cada package identity dentro
de um resolution realm. Um conflito de constraints falha com paths mínimos do
grafo. Realms diferentes podem escolher versões ou features diferentes e
produzem artifact keys diferentes.

`package.lock` reutiliza o codec data-only e possui top-level `lock`. O resolver
gera o arquivo em UTF-8, LF e ordem canônica:

```w
lock {
  schema: "w.package-lock/1"
  resolver: "w.resolver/1"
  workspace: "sha256:..."
  contexts: [
    {
      root: .product("last-light-native")
      use: .product
      targetRole: .target
      target: "x86_64-unknown-linux-gnu"
      features: []
      nodes: []
    },
    {
      root: .tool("menu-compiler")
      use: .build
      targetRole: .execution
      target: "x86_64-pc-windows-msvc"
      features: []
      nodes: ["sha256:package-node..."]
    },
  ]
  packages: [
    {
      id: "sha256:package-node..."
      authority: "w:sha256:..."
      name: "last-light/menu-compiler"
      version: "0.1.0"
      source: .member(
        path: "packages/menu-compiler",
        manifest: "sha256:...",
        sourceSet: "sha256:...",
      )
      dependencies: []
    },
  ]
}
```

`id` é uma referência interna ao lock. Ele é o digest do package identity,
version, source descriptor e dependency edges normalizados. Um member usa
manifest e source-set digests; o content tree local fica na recipe. Um package
externo usa metadata snapshot e content tree digest. Realms ou feature sets
distintos podem produzir nodes distintos para a mesma identity e version. O
node ID não participa de type identity.

O lock de workspace registra:

- schema e resolver version;
- digest de cada manifest e do workspace;
- roots, usages, features e target roles;
- versões, sources, external tree digests e edges transitivos;
- member e patch paths, manifest digests e source-set digests;
- build-tool packages e metadata snapshots;
- razão de cada seleção e exceção de policy.

Um source-set digest cobre a lista ordenada de `PackagePath`, module identity e
role. Ele muda quando um arquivo entra, sai ou muda de role. Ele não muda quando
o conteúdo de um arquivo existente muda. Assim, edição local normal não exige
nova resolução, mas um arquivo novo não entra em `--locked` por discovery.

O lock não contém payload digest, action output, profile, compiler, sysroot ou
provenance do build. Esses facts pertencem à recipe e ao artifact record. `w
resolve` grava o lock de forma atômica. `w diff-lock` mostra mudanças semânticas.
Um lock modificado não recebe confiança especial; o resolver valida todos os
digests e invariants antes do uso.

O lock de uma library publicado com sua release preserva a resolução usada nos
próprios tests e artifacts. A recipe correspondente conclui a reprodução. O
lock não força a resolution dos consumers. O consumer usa as constraints do
`package.w` e grava o resultado no próprio lock.

Fontes primárias usadas para os invariantes:

- [Cargo workspaces](https://doc.rust-lang.org/cargo/reference/workspaces.html);
- [Cargo features](https://doc.rust-lang.org/cargo/reference/features.html);
- [Cargo dependency sources](https://doc.rust-lang.org/cargo/reference/specifying-dependencies.html);
- [Go workspaces e replacements](https://go.dev/ref/mod#workspaces).

#### 21.1.6 Source snapshot publicável

**Exemplo:** o package principal publica somente os arquivos necessários para
rebuild, documentação e licença:

```w
license: {
  expression: "MIT"
  files: ["LICENSE"]
}

publish: {
  source: .required
  files: [
    .modules,
    .path("deployments/local.w"),
    .path("deployments/distributed.w"),
    .path("menus/final.menu"),
    .path("README.md"),
    .path("BUILD.md"),
    .path("LICENSE"),
  ]
}
```

`publish.files` é uma allowlist obrigatória. A serialização canônica do
`package.w` atual é metadata obrigatória e participa do snapshot digest.
`.modules` inclui os arquivos declarados por `modules` e `moduleSets`; ele não
inclui `package.w`, `workspace.w` ou manifest de subpackage. `.path` usa
`PackagePath` exato, não aceita glob, não segue symlink e não sai da raiz. A
lista normalizada entra no release recipe. Um arquivo novo fora de `.modules`
não é publicado até a allowlist mudar.

`.gitignore`, excludes globais do editor e estado do VCS não alteram o
snapshot. Eles servem ao checkout, não à supply chain. Subpackages também não
entram por traversal; cada member publica sua própria árvore.

`w package list` mostra path, size, digest e razão de inclusão. `w package
check` cria o snapshot sem publicar, verifica que todos os modules, resources,
license files e build inputs necessários estão presentes e reconstrói o
package usando somente esse snapshot.

`license.expression` usa uma
[SPDX License Expression](https://spdx.github.io/spdx-spec/v3.0.1/annexes/spdx-license-expressions/).
Um package sem licença open source usa `.proprietary`; ausência de informação
usa `.noAssertion`. Esses cases são facts distintos. Cada `LicenseRef` exige o
texto correspondente em `license.files`.

Repository, homepage, display name e descrição são metadata. Eles não mudam
package identity ou o field `authority`. O registry pode aplicar limites de
tamanho e policy, mas não acrescenta arquivos ao snapshot enviado pelo
maintainer.

**Alternativa:** publicar todos os arquivos não ignorados reduz configuração.
Ela transforma uma policy local e mutável em boundary de distribuição.

`kind` seleciona um schema fechado. O manifest não usa um record com fields
opcionais sem relação.

Um product iniciado por host liga exatamente um descriptor expandido. Uma
library usa symbols exportados como roots. Uma service-only unit usa providers
publicados no artifact index. Todo product e toda unit precisam de ao menos um
root alcançável.

Vários products podem usar os mesmos módulos. Escolher outro entry, host,
target, runtime graph ou packing cria outra recipe.

`limits` usa um schema fechado pelo host profile e pelas capabilities. Ele
define o maior envelope do artifact. Um limite exigido por um slot precisa
estar no product ou numa const call alcançável, como `http.serve`. Deployment
pode reduzir o envelope. Ele não pode aumentá-lo.

Product kinds iniciais:

| Kind | Resultado |
|---|---|
| `.executable` | payload iniciado por um process ou application host |
| `.staticLibrary` | archive e interface para link |
| `.dynamicLibrary` | library com ABI declarada |
| `.component` | component com entry, imports ou providers exportados |
| `.firmware` | imagem e metadata de device |
| `.deviceBundle` | kernels/objects para um accelerator e manifest de launch |
| `.test` | harness e corpus selecionado |
| `.benchmark` | harness, workload e evidence schema |
| `.tool` | executável hermético usado pelo build |

`.dynamicLibrary` não estabiliza a ABI W. A superfície exportada precisa escolher
uma ABI, como C, Wasm Component ou schema W versionado.

`targetSets` servem à matriz de CI e release. Eles não produzem um payload
universal por inferência. Cada combinação de product, target e profile possui
uma chave própria.

O algoritmo inicial deve ser PubGrub ou outro solver que produza explicações
equivalentes e determinísticas. O resultado, não o nome do algoritmo, é o
contrato.

O lock registra a resolução. A recipe registra uma build concreta. O artifact
record registra os outputs dessa recipe. Os três schemas não se fundem:

| Record | Inputs principais | Não contém |
|---|---|---|
| `package.lock` | versões, sources, features, contexts e metadata | payloads e resultados de actions |
| recipe | source trees, lock digest, product, target, profile e toolchain | payload digest autorreferente |
| artifact record | recipe digest, payloads, resources e sidecars | inputs ambientais não declarados |

### 21.2 Build

**Exemplo:** o build escolhe product, target e packing. O product escolhe entry,
host e runtime graph:

```text
w build last-light-native \
  --target x86_64-unknown-linux-gnu \
  --packing single-process \
  --profile release \
  --locked

w build last-light-worker \
  --target wasm32-wasip3 \
  --packing entry-only \
  --profile release \
  --locked

w build --matrix desktop --product last-light-native --locked
```

`--matrix` agenda recipes independentes. Ele não muda a identidade dos
payloads. O resultado inclui um index que aponta para cada digest.

- source é o fallback normativo;
- binaries são otimização sob uma chave ABI completa;
- static linkage é preferido quando compatível;
- build scripts não recebem rede ou filesystem irrestrito;
- code generation é uma tool target hermética;
- adapters `fn<Language>` são tool targets fixadas, não shell commands livres;
- cada foreign unit possui source digest, toolchain, target, ABI e symbol manifest;
- cache é content-addressed;
- recipe fixa toolchain, target, profile, inputs e environment permitido;
- recipe fixa source tree digests, source sets e lock digest;
- recipe fixa quotas e evaluator version de compile-time;
- CBOR determinístico é a representação canônica inicial;
- SHA-256 tagged é o digest inicial e possui algorithm agility.

`w build --locked` falha se manifest, resolution context ou source-set
membership divergir do lock. Editar o conteúdo de um source local existente é
permitido e gera outra recipe. CI/release usa esse modo. `w update package`
mostra o diff mínimo do grafo. Offline não acessa a rede; frozen pode buscar
somente objetos já fixados.

`w reproduce <recipe>` exige os content tree digests exatos. A mesma recipe deve
produzir o mesmo payload bit a bit. Data, commit, paths, locale, timezone, seeds
e environment são inputs explícitos ou são removidos.

#### 21.2.1 Artifact identity

A chave mínima inclui:

```text
package graph + product + expanded entry + host profile + runtime graph + packing
+ target + CPU/features + sysroot/SDK + profile
+ compiler/runtime + adapters + build inputs + lock digest
```

O artifact record separa:

- payload primário;
- interface e symbol manifest;
- resources;
- device payloads;
- debug sidecars;
- provenance e attestations;
- envelope de plataforma.

Um macOS universal binary, um Android App Bundle e um firmware bundle são
artifacts compostos. Seus componentes continuam identificados por digest.
Assinatura, notarization ou timestamp não alteram o digest do payload interno.

#### 21.2.2 Multimode e múltiplos artifacts

Um executável nativo pode oferecer `--cli`, `--tui` e `--serve`. Seu único
`process.main` escolhe o modo e mantém um só descriptor:

```text
w run last-light-native --deployment deployments/local.w -- --tui
w run last-light-native --deployment deployments/local.w \
  -- --serve 127.0.0.1:8080
```

Um worker HTTP usa outro host lifecycle. Ele recebe outro product e, em geral,
outro artifact. Compartilhar source não exige compartilhar entry, runtime ou
bytes finais.

O build pode incluir vários handlers alcançáveis, mas dead stripping remove os
que nenhum binding ou call referencia. `w explain product` mostra por que um
símbolo permaneceu:

```text
$ w explain product last-light-native
entry: restaurant.app::.default
default slot: process.main -> restaurant.app::run
binding: process.signal -> restaurant.app::shutdown
reachable adapter: http.Server (selected by LaunchMode.serve)
excluded entry: restaurant.worker_app::LastLightWorker
```

#### 21.2.3 Build transforms tipadas

**Exemplo:** o compiler de cardápio recebe um input nomeado e confirma um output
nomeado. Ele não recebe argv ou filesystem:

```w
import std.build
import { MenuCompileError, compileMenu } from last_light.menu.compiler

const menuSource = build.Input<String>(name: "menu")
const menuBytecode = build.Output<Bytes>(name: "bytecode")

enum MenuTransformError: Error {
  build(build.Error)
  compile(MenuCompileError)
}

async fn transform(ctx: build.Context): () throws MenuTransformError {
  let source = try await ctx.read(menuSource, maximumBytes: 64<KiB>)
  let compiled = try compileMenu(source)
  let MenuBytecode(bytes, _) = take compiled
  try await ctx.write(menuBytecode, take bytes)
}

entry(transform)
```

O package root liga o tool a paths e budgets:

```w
actions: [
  {
    name: "compile-final-menu"
    tool: .dependency("menuCompiler", product: "menu-compiler")
    inputs: [
      {
        binding: "menu"
        source: .file("menus/final.menu")
        maximumBytes: 64KiB
      },
    ]
    outputs: [
      {
        binding: "bytecode"
        kind: .resource
        maximumBytes: 1MiB
      },
    ]
  },
]
```

Um product consome o output por identity lógica:

```w
resources: [
  .action("compile-final-menu", output: "bytecode"),
]
```

Declarar a action não a executa por si só. Um product, module set, test,
benchmark ou outra action precisa alcançar um output. Action ou dependency sem
consumer é error. Vários consumers podem compartilhar o mesmo objeto do CAS.

Um build transform é um `.tool` product com host
`w.host/build-transform@1`. O slot default é:

```text
build.transform:
  async fn(build.Context) -> () throws E
```

`build.Context` concede somente bindings declarados. `build.Input<T>` e
`build.Output<T>` usam nomes const, tipos e codecs fechados. A v0 oferece
`String`, `Bytes`, source tree, artifact e metadata target tipada. Um transform
não enumera diretórios, abre path arbitrário ou consulta environment.

Um binding possui de 1 a 64 caracteres ASCII lowercase, digits e `-`; o
primeiro caractere é uma letra. Input e output compartilham o namespace da
action. Duplicata ou binding declarado somente de um lado falha antes de
executar o tool.

Network, clock, random e secrets são negados. Uma evolução pode conceder uma
capability específica, mas ela entra no action schema, lock, recipe e policy.
Install scripts, shell fragments e callbacks de package não são transforms.

O runtime cria um diretório de trabalho descartável ou uma sandbox equivalente.
Inputs são read-only. Outputs começam privados e entram no CAS somente quando o
handler termina com success, todos os outputs obrigatórios existem e cada budget
confere. Error, panic ou cancellation descartam a tentativa. O tool nunca
escreve no source tree.

Tool e payload usam configurações distintas:

```text
execution target -> artifact executável do menu-compiler
product target   -> artifact last-light-native
```

Uma cross-build em Windows para Cortex-M ainda executa o tool em Windows. Se um
transform precisa de facts do target final, o action declara
`.targetMetadata(...)` como input. O host não fica observável por acidente.

Action identity inclui:

```text
tool artifact + expanded entry + execution target + typed inputs
+ target metadata declarada + output schema + budgets + allowed capabilities
```

Essa identidade é a action recipe key. Ela não contém os output digests. Após a
execução, um action result liga a key aos output digests. A product recipe usa
esses digests como build inputs. Assim, nenhum record inclui o próprio resultado
na chave:

```text
action recipe -> action result -> generated input -> product recipe
               output digests                    -> artifact record
```

Dois executores que produzem outputs diferentes para a mesma action recipe
revelam uma violação de determinismo. O cache não escolhe um resultado em
silêncio: ele preserva a evidência conflitante, rejeita publicação automática e
exige nova tool identity ou correção.

O scheduler pode executar a mesma action local ou remotamente. O resultado
correto não depende do executor. Cache hit não executa o tool e mantém a mesma
provenance de input e output.

`w explain action compile-final-menu` mostra tool, execution target, inputs,
budgets, capabilities, cache key e consumers. `w run tool` não concede mais
authority que a action.

Esse contrato segue o princípio de hermeticidade do
[Bazel](https://bazel.build/concepts/hermeticity): toolchains e inputs fazem
parte do grafo, e influência externa precisa ser eliminada ou declarada. W usa
um host profile tipado no lugar de uma linguagem geral de build.

### 21.3 Verificação

**Exemplo:** dois builders reproduzem o mesmo payload e publicam recipes,
toolchains e sidecars de provenance separados.

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

### 21.4 Registry, mirrors e estado de segurança

**Exemplo:** uma versão pode ser `reproduced` e ainda possuir um advisory de
segurança aberto. Os estados não se substituem.

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

### 21.5 Scripts e supply chain

**Exemplo:** um build script sem capability de rede não baixa um binary durante
CI.

Install scripts arbitrários são rejeitados. Tool targets de geração declaram
inputs, outputs e capabilities. Network é denied por default. Outputs entram no
CAS e na provenance.

`build-transform@1` é a rota W normal para geração. Um adapter de toolchain
foreign precisa declarar executable, argv canônico, inputs, outputs, execution
target e sandbox. Ele não transforma uma string em shell command.

Dependências transitivas não recebem capabilities do app. Build tools e product
dependencies aparecem como relações distintas na provenance e no SBOM.

### 21.6 CLI

```text
w context
w workspace check
w resolve
w update <package>
w fetch --locked
w package list [package]
w package check [package]
w build <product> --target <target> [--packing <packing>] --locked
w build --matrix <set> --product <product> --locked
w run <product> [--deployment <plan>] -- <arguments>
w test [product] --locked
w explain dependency <package>
w explain feature <package>::<feature>
w explain action <action>
w explain product <product>
w explain artifact <digest>
w explain workflow <supervisor> --key <key>
w audit effects <product>
w diff-lock
w verify <artifact>
w reproduce <release>
w deploy resolve <plan>
w deploy check <plan> --locked
w deploy apply <plan> --locked
w bundle offline
w cache import <bundle>
w publish check [package]
```

Saída humana é curta. `--json` fornece o grafo, diagnostics e evidências
completos.

#### 21.6.1 Fluxo local

```text
$ w resolve
resolved 14 packages; wrote package.lock

$ w build last-light-native --target x86_64-unknown-linux-gnu --locked
built last-light-native
payload sha256:7e...
recipe  sha256:21...

$ w run last-light-native --deployment deployments/local.w -- --cli
```

O CLI não imprime download, compile unit ou cache hit por default. `--verbose`
mostra fases. `--json` emite eventos estáveis.

#### 21.6.2 Publicação e reprodução

```text
w package assemble last-light-native --target x86_64-unknown-linux-gnu --locked
w publish --release 0.1.0 --artifacts dist/release.windex
w verify registry:last-light/restaurant@0.1.0
w reproduce registry:last-light/restaurant@0.1.0 \
  --target x86_64-unknown-linux-gnu
```

`publish` envia metadata autorizada e objetos por digest. Ele não executa um
build oculto. A CI de reprodução baixa source e recipe, usa um builder
independente e publica uma attestation separada.

### 21.7 Evolução e governança

**Exemplo:** o registry pode marcar uma versão como yanked. Ele não troca os
bytes associados ao mesmo digest.

Cada package declara uma edition. Editions podem alterar grammar, prelude e
lints com migração automatizada. Elas não mudam resultado ou effects
silenciosamente.

Schemas de interface, lock, package, diagnostics e provenance têm readers
versionados. Deprecation informa replacement e janela de remoção. O projeto
publica suporte de targets e security policy.

Identidade do nome W, domínio, executable e trademark precisa de validação antes
do lançamento público. A frase “A última linguagem que você vai precisar
aprender” fica como alternativa de marca; não é promessa técnica.

## 22. Tooling e interface para máquinas

### 22.1 Tooling humano

- `w fmt` produz a forma canônica de 120 colunas;
- `w check` não gera artefato final;
- `w interface` mostra a interface normalizada em texto ou JSON;
- `w interface diff` classifica compatibilidade de source e casos para revisão;
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

**Exemplo:** a página de `spawn` inclui o snippet de `execution.w` e falha no
build quando essa região deixa de parsear.

O portal atual é um **protótipo congelado**. Ele demonstra direção visual, tema,
navegação e playground lexical. Ele não é uma segunda documentação e não precisa
acompanhar cada mudança durante o endurecimento da linguagem.

Depois do design freeze, um gerador usa:

1. a estrutura Markdown e o registro W deste arquivo;
2. snippets extraídos dos arquivos `.w` canônicos;
3. resultados estruturados da grammar, formatter e testes;
4. metadata de versão e provenance do build.

O source do portal não duplica contratos nem snippets. Cada bloco gerado aponta
para arquivo, região e revisão de origem. CI falha quando um include desaparece,
um snippet não parseia ou um ID W não existe.

Astro é uma opção de renderização, não uma decisão de arquitetura. Ele só será
comparado depois do freeze com um gerador mínimo. A escolha depende de build
hermético, output estático, acessibilidade, dependências, tempo de build e
facilidade de remover o framework.

### 22.2 Documentação e testes

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

### 22.3 Lens de recursos

**Exemplo:** o editor mostra `+0 B static, <=4 KiB peak` ao lado de um import
quando a análise possui bounds provados.

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

### 22.4 Interface para modelos

**Exemplo:** um modelo recebe diagnostic JSON com move original, uso inválido,
tipo e fix-it. Ele não precisa inferir esses fatos de uma mensagem livre.

W não possui uma sintaxe curta exclusiva para IA. Em vez disso, oferece:

- formatter determinístico;
- grammar e schema versionados;
- diagnostics estruturados;
- interface compilada com tipos, effects e ownership;
- `w explain --json`;
- exemplos positivos e negativos;
- testes de compilação e comportamento;
- source maps para código gerado e ilhas de linguagem.

Contagem de tokens depende do tokenizer. O design vigente mede vários modelos antes de
trocar uma keyword por pontuação. Compile success, testes e edit distance têm
mais peso que token count isolado.

### 22.5 Diagnostics estruturados

Um diagnostic possui identidade estável e dados antes de possuir prosa. A saída
JSONL canônica contém:

```json
{
  "schemaVersion": 1,
  "code": "W-MOVE-0001",
  "phase": "typecheck",
  "severity": "error",
  "primary": {"source": "order.w", "startByte": 418, "endByte": 423},
  "labels": [
    {"source": "order.w", "startByte": 351, "endByte": 361, "role": "move"}
  ],
  "facts": {"binding": "order", "type": "Order"},
  "help": ["borrow the value or move it only once"],
  "fixes": [],
  "root": null
}
```

Byte offsets são canônicos. O renderer calcula line, Unicode scalar column e
display column. Um diagnostic gerado por macro ou ilha registra os spans de
origem e expansão:

```text
order.w:18:9: error[W-MOVE-0001]: order was already moved
  note: move occurred at order.w:15:10
```

Codes usam uma família e quatro digits, como `W-PARSE-0001`,
`W-TYPE-0042`, `W-MOVE-0001`, `W-EFFECT-0008`, `W-FFI-0012` e
`W-BUILD-0003`. Um code removido nunca recebe outro significado. A mensagem
humana pode melhorar sem alterar o code.

Um fix contém edits, applicability e precondition. Applicability é `.machine`,
`.review` ou `.placeholder`. A precondition inclui o source digest:

```json
{
  "title": "borrow order",
  "applicability": "machine",
  "sourceDigest": "sha256:...",
  "edits": [{"source": "order.w", "startByte": 418, "endByte": 418, "text": "ref "}]
}
```

Uma ferramenta só aplica `.machine` quando o digest ainda corresponde. `.review`
exige confirmação humana. `.placeholder` contém uma região que ainda precisa de
um valor.

Diagnostics usam ordem determinística por logical path, byte inicial, code e
ocorrência. Um error secundário aponta para o diagnostic raiz. O renderer limita
cascades por raiz:

```text
W-TYPE-0042 root
  W-TYPE-0119 caused-by W-TYPE-0042
```

O compiler pode usar poison types para continuar a análise. Ele nunca gera um
executable quando existe um diagnostic `error`.

Errors não podem ser suprimidos. Warnings são configuradas por code e path no
manifest ou CLI:

```w
diagnostics {
  deny = ["W-FFI-*"]
  allow = [{ code: "W-DOC-0017", path: "generated/**" }]
}
```

Source annotations de suppressão não entram no design vigente. Elas esconderiam policy no
programa e adicionariam syntax permanente.

O schema JSON não é localizado. Um renderer pode localizar a mensagem. LSP e
SARIF são adapters do mesmo diagnostic; eles não definem a semântica interna.
Eventos runtime `ErrorEvent` e `PanicEvent` usam schemas separados.

`w explain diagnostic CODE` mostra significado, causas, exemplos e fixes:

```text
w explain diagnostic W-MOVE-0001
```

O schema toma como precedentes a saída
[JSON do rustc](https://doc.rust-lang.org/beta/rustc/json.html), o
[Language Server Protocol](https://microsoft.github.io/language-server-protocol/)
e o [SARIF 2.1.0](https://docs.oasis-open.org/sarif/sarif/v2.1.0/sarif-v2.1.0.pdf).
W mantém um schema interno menor e produz LSP ou SARIF por adapter.

## 23. Protocolos e pesquisas de ecossistema

Nenhum item desta seção reserva keyword. Cada hipótese usa os contratos do core
e pode evoluir como package separado.

### 23.1 Contrato tipado e wRPC

**Exemplo:** `ServiceRef<MenuApi>.lookup(id)` preserva request, response, error,
deadline e cancellation no schema de transporte.

Um protocol de service pode gerar um contrato independente de transporte. O
contrato registra operações, inputs, outputs, error sets, idempotência, limits e
schema dos tipos alcançáveis.

wRPC é um possível envelope de call, não query language nem codec. O mínimo
possui protocol version, message kind, call ID, service ID, operation ID, codec,
metadata, payload e tamanho validado antes da alocação.

Retry mutante não é implícito. Deadline/cancelamento não promete rollback.
Falhas de aplicação, transporte, codec, protocol e autorização são distintas.

### 23.2 JSON, WLO e wStruct

**Exemplo:** um adapter JSON rejeita field desconhecido quando o schema usa modo
strict. WLO não muda essa regra por syntax.

- JSON é o primeiro codec de interoperabilidade e debug.
- WLO/WLON é pesquisa de formato data-only canônico para valores W.
- wStruct pesquisa IPC sob target, ABI e layout idênticos.

WLO precisa de grammar menor que W, canonical bytes, limits e fuzzing. wStruct
não serializa pointers, padding ou handles crus. Ambos precisam de fallback.

### 23.3 wQL e RestPC

**Exemplo:** uma query de pedidos precisa declarar paginação, limites, auth,
cache e error mapping antes de virar API oficial.

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

### 23.4 V6 e Computer Units

**Exemplo:** uma Computer Unit pode virar uma service instance. Ela não recebe
wire format ou deployment implícitos.

V6 continua pesquisa de runtime/host serverless. Computer Unit é uma instância
com entrypoints, limits e capabilities. Esses conceitos podem hospedar services
W, mas não definem a linguagem, wRPC ou o package manager.

### 23.5 Tree strings

**Exemplo:** um protótipo compara árvore compacta com `String` + parser em tamanho,
lookup, edição e interoperabilidade.

Tree strings continuam uma estrutura especializada para interning, índices ou
edição. `String` público permanece UTF-8 contíguo. Codec e ABI observam o valor
lógico, não a representação experimental.

### 23.6 GPU, HDL, PGO e geração assistida

**Exemplo:** um kernel tensor pode baixar para GPU quando o device atende ao
contrato. O mesmo source mantém fallback CPU correto.

GPU começa por um kernel puro com baseline CPU, device transfer explícita e
comparação de resultado/custo. HDL exige um modelo próprio de timing e
verificação; não é lowering automático de código CPU.

PGO, snapshots e casos gerados por IA são artefatos de tooling. Seed, workload,
provenance e oracle são explícitos. Nenhum deles muda a semântica source.

### 23.7 Gate de promoção

**Exemplo:** `fn<Rust>` não entra na linguagem até reproduzir archive, façade C,
diagnostics e cleanup em dois targets.

Uma pesquisa só avança quando possui:

1. problema e métrica;
2. implementação pequena e fallback;
3. alternativa mais simples;
4. erro, cancelamento, FFI e dois targets;
5. impacto em parser, formatter, metadata e packages;
6. decisão registrada por diff e teste.

## 24. Classificação de viabilidade

| Família | Classe vigente | Motivo |
|---|---|---|
| owner único, borrow e whole-value move | **Possível agora** | análise e lowering conhecidos |
| provenance separada de address | **Possível agora** | HIR e LLVM preservam a distinção |
| pinning interno de task frame | **Provável** | lowering conhecido; drop e projection exigem corpus |
| `pin` e `Pinned<T>` públicos | **Provável** | contrato claro; FFI persistente precisa de protótipo |
| placement local sem annotation | **Possível agora** | escape e frame analysis conservadores fornecem fallback stack |
| gate sem allocator geral | **Provável** | call graph e allocation facts são conhecidos; FFI exige summary |
| `Arena` T0 e bloco `region` | **Provável** | lifetime lexical e bulk release são conhecidos; async e rehome exigem corpus |
| allocator explícito por `using` | **Possível agora** | origem e deallocator acompanham o owner |
| `shared` + `weak` sem cycle collector | **Provável** | RC é conhecido; tooling de ciclos precisa de avaliação |
| `async let`/`spawn let` estruturados | **Possível agora** | state machine e runtime mínimo delimitados |
| modules sem lifecycle e imports herméticos | **Possível agora** | contrato estático simples |
| UTF-8 owned e views | **Possível agora** | representação portátil com fallback |
| String flat com owner único no W0 | **Possível agora** | pointer/count/reserva/origin e validação UTF-8 são conhecidos |
| carrier comum de String/Bytes consuming | **Possível agora** | ambos são buffers T0 owned; type safety permanece nas conversões |
| SSO invisível | **Provável** | Swift e SmallString provam viabilidade; threshold e target exigem benchmark |
| COW como baseline de String | **Rejeitado** | desloca allocation, budget, failure e deallocator para mutation futura |
| cache lazy por String | **Rejeitado na baseline** | read não deve alocar, mutar owner ou exigir synchronization |
| `view T` genérica para projeções core | **Possível agora** | provenance e descriptor são definidos por família; `ref` cobre o place completo |
| fato de imutabilidade profunda | **Provável** | owner único e fields fechados são verificáveis; capabilities e foreign storage exigem fallback conservador |
| UTF-8 incremental e maximal subpart | **Possível agora** | estado máximo de três bytes e algoritmo Unicode versionado |
| adoção de `Bytes` por `String` | **Provável** | owner transfer é claro; reuse depende do allocator/layout |
| Bytes, paths nativos e C strings distintos | **Possível agora** | fronteiras conhecidas; conversões preservam perda e terminador |
| graphemes default e normalização versionados | **Possível agora** | tabelas Unicode geradas; custo linear permanece visível |
| `InlineString` com layout público | **Pesquisa** | benefício depende de target, ABI e benchmark contra SSO invisível |
| strict numerics e overflow verificado | **Possível agora** | backend oferece operações adequadas |
| literal exato até materialização | **Possível agora** | big integer e rational decimal ficam no frontend |
| conversões pelo domínio completo | **Possível agora** | tabela fechada e facts de refinement decidem sem heurística |
| float strict e total-order wrapper | **Possível agora** | IEEE e backend fornecem as operações necessárias |
| ranges com quatro closures | **Possível agora** | representação, membership e iteration discreta são separáveis |
| BigInt, Rational e FixedDecimal T2 | **Provável T2** | algoritmos conhecidos; API, OOM e limites exigem corpus |
| `f16`, `bf16` e quantization T2 | **Provável T2** | MLIR preserva storage/expressed type; targets exigem fallback |
| Posit, Unum e decimal float | **Pesquisa** | interoperability, rounding e hardware ainda não justificam baseline |
| schema fechado de contrato estático | **Possível agora** | AST/HIR simples; corpus angular já existe |
| referências `.member` contextuais | **Possível agora** | expected type e refinement subject fecham a resolução |
| associated constants, functions e types | **Possível agora** | lookup estático e witnesses nominais são conhecidos |
| generics com primary associated types | **Possível agora** | inference fechada, coherence nominal e lowering híbrido definidos |
| subsets fechados de enum | **Possível agora** | case-set normalizado, flow narrowing e layout base definidos |
| typestate por `const` enum + `take fn` | **Provável** | lookup e ownership são conhecidos; diagnostics e code sharing exigem corpus |
| `TypeId` e reflection opt-in | **Possível agora** | descriptor alcançável não expõe layout nem storage privado |
| synthesis de protocols core | **Possível agora** | families fechadas e witnesses normalizados |
| parâmetros rest homogêneos | **Provável** | call shape é fechado; ownership e lowering exigem corpus |
| packs heterogêneos e GAT | **Pesquisa** | kinds, shape constraints, borrow e witness layout ficam abertos |
| metatype e dynamic construction | **Rejeitado por enquanto** | generics, factory e enum preservam relações estáticas |
| visibilidade efetiva por tipo de membro | **Possível agora** | interface e HIR usam normalização determinística |
| destructuring nominal de struct | **Possível agora** | pattern e modos de borrow fechados |
| switch exaustivo e tuple scrutinee | **Possível agora** | ordem, guards e patterns fechados possuem análise conhecida |
| diff de interface e SemVer | **Provável** | regras básicas fechadas; conflitos de resolução exigem corpus |
| receiver consuming `take fn` | **Possível agora** | whole-value move e drop state já são necessários |
| retorno fluente `: self` | **Provável** | reborrow é conhecido; borrow suspenso exige corpus |
| overload por forma de call | **Possível agora** | seleção por labels ocorre antes do type-check |
| vários initializers e delegação total | **Possível agora** | flow analysis e grafo de delegação são conhecidos |
| computed property property-safe | **Possível agora** | accessors e borrow do receiver possuem lowering direto |
| static record e static list | **Possível agora** | payload const; cada head ainda precisa de schema |
| `fn`, `some fn` e `any fn` | **Provável** | tipos e drop são conhecidos; escape e erasure exigem corpus de custo |
| services serial-turn e `ServiceRef` async | **Provável** | exige protótipo de mailbox, deadlock e trace |
| `SupervisorRef` process-local | **Provável** | owner, admission, cancellation e outcome estão fechados; restart exige oracle |
| bindings tipados e runtime graph data-only | **Possível agora** | requirements, providers, imports e exports fecham por interface no link |
| packing de service graph | **Provável** | partição e index são simples; ABI entre units e fast path exigem protótipo |
| deployment plan e lock por digest | **Provável** | resolução é direta; placement, adapters e rolling update exigem runtime |
| workflow durável por steps | **Provável T2** | superfície, replay e effect policy estão fechados; journal, crash oracle e migration exigem protótipo |
| `<unit>` e units customizadas | **Provável** | type/lowering coerentes; ergonomia precisa de corpus |
| refinements e value parameters | **Provável** | exige evaluator, proof budget e ABI identity |
| interval, case-set, shape e alias facts na HIR | **Possível agora** | análises conhecidas; fallback conserva checks e largura |
| remoção de checks por prova verificada | **Possível agora** | range e control-flow facts possuem lowering direto |
| largura de operação e SIMD por refinement | **Provável** | precisa preservar overflow, accumulator e cost model por target |
| storage estreito não escapante | **Provável** | exige boundary analysis, repacking e benchmark de cache/code size |
| optimization record e `w explain performance` | **Possível agora** | facts e decisões já existem nos passes; schema precisa ser estável |
| theorem prover ou SMT geral no build | **Pesquisa** | custo, diagnostics e reproducibility não fecham a baseline |
| property behaviors | **Provável** | expansão HIR é viável; composição ainda precisa de teste |
| obrigação linear de async close | **Pesquisa** | evita leak oculto; receiver e cancellation precisam de protótipo |
| entries e host profiles | **Provável** | binding é claro; adapters precisam de schemas |
| descriptor anônimo e overlay local | **Possível agora** | expansão é estática; diagnostics precisam mostrar origem |
| package manifest data-only | **Possível agora** | grammar separada, schema fechado e evaluator ausente |
| workspace data-only com lock compartilhado | **Possível agora** | members exatos, identity e contexts são verificações estáticas |
| usages separados de dependência | **Possível agora** | reachability e target role fecham product, build, test e benchmark |
| features somente no grafo | **Possível agora** | união aditiva evita conditional source e defaults ocultos |
| source snapshot por allowlist | **Possível agora** | module expansion, PackagePath e digest produzem uma árvore fechada |
| build transform tipada | **Provável** | host profile e CAS são diretos; sandbox cross-platform exige protótipo |
| parâmetro de chamada `const` | **Possível agora** | evaluator e call checking já existem; ABI pode apagar o requisito |
| mensagem HTTP, ownership e admission | **Possível agora** | types, stream e limits estão fechados; adapters ainda precisam de corpus |
| adapter HTTP nativo e worker | **Provável** | sockets e WASI existem; parity, cancel drain e headers exigem implementação |
| SQL estático e rows tipadas | **Possível agora** | descriptors e bind são diretos; schema completo depende de bundle |
| pool, pipeline e transaction database | **Provável** | protocolos existem; admission, cleanup e unknown commit exigem fault tests |
| cache local com limite e read-through | **Provável** | algoritmos são conhecidos; eviction, cancellation e custo exigem protótipo |
| target identity e matrix build | **Possível agora** | recipes independentes evitam falsa identidade entre payloads |
| WASI 0.3 native async component | **Provável** | standard estável; target e guest toolchains ainda amadurecem |
| desktop/server LLVM targets | **Provável** | backends existem; runtime, SDK e CI ainda são trabalho W |
| Android e Apple mobile | **Provável** | ABI e SDK existem; lifecycle, packaging e signing exigem adapters |
| Cortex-M e RISC-V firmware | **Provável** | backends existem; freestanding runtime e device descriptions exigem corpus |
| NVVM, ROCDL e SPIR-V device bundle | **Provável T2** | MLIR oferece lowerings; kernel subset e transfer precisam de protótipo |
| ASIC/FPGA como target geral | **Pesquisa** | timing, synthesis e verification não seguem o runtime CPU |
| nanoservices co-localizados | **Provável** | service graph permite fast path; equivalência física exige trace e fault tests |
| profile TechEmpower | **Provável** | sete source oracles existem; adapters, harness fixado e medição ainda faltam |
| tensors ranked, `@` e views | **Provável T2** | MLIR ajuda; API e device model precisam de protótipo |
| integer tensor com accumulator inferido por range | **Provável T2** | prova é conhecida; panic, widening e kernel dispatch exigem corpus |
| float matrix modes strict/fast/reproducible | **Provável T2** | cada mode precisa de oracle numérico e matriz de targets |
| niches de null e bit pattern inválido | **Possível agora** | validity facts e fallback explícito fecham a semântica |
| low-bit interno por alignment provado | **Provável** | exige lowering de provenance e corpus de FFI, atomics e sanitizer |
| high-bit addresses e NaN boxing | **Pesquisa** | dependem de target, processo e tooling; não integram o profile portátil |
| universal tagged pointer ou object header | **Rejeitado** | conflita com ABI, hardening, capability pointers e valores sem metadata |
| schema portátil de execution domains | **Possível agora** | IDs, capabilities e regras de binding são estáticos |
| capacity compartilhada em paralelismo aninhado | **Provável** | runtime inicial precisa provar liveness e ausência de oversubscription |
| `Stream<Item, Failure>` single-pass | **Possível agora** | cursor mutável, Optional terminal e error effect possuem lowering direto |
| `for try await` | **Possível agora** | sugar local para `next()`; borrow do cursor e effects permanecem visíveis |
| `Channel<T>` MPSC bounded | **Provável** | ownership e estados estão fechados; fairness, cancellation e custo exigem protótipo |
| permits de channel | **Provável** | capability linear fecha capacity; close e suspension longa exigem oracle |
| MPMC, broadcast, watch e weighted channel | **Pesquisa** | loss, fan-out, lag e accounting não cabem no contrato MPSC |
| `ByteSource`/`ByteSink` async-first | **Possível agora** | short progress, EOF e errors possuem resultados fechados |
| read por append em reserva privada de `Bytes` | **Possível agora** | initialized count e commit ocultam storage ainda não inicializado |
| cancellation de I/O com completion drain | **Provável** | backends possuem completion; runtime e borrow checker precisam de oracle |
| filesystem com rights estáticos e offset posicional | **Provável** | handles e syscalls existem; profiles e diagnostics exigem protótipo |
| adapters blocking com quota | **Provável** | pool bounded preserva semântica; cancellation física depende da API |
| backends readiness/completion equivalentes | **Provável** | contrato comum está fechado; matriz de targets deve provar os mesmos traces |
| gather write com segments borrowed | **Possível agora** | rest homogêneo, prefix progress e fallback sem allocation fecham a superfície |
| scatter read | **Pesquisa** | múltiplos borrows exclusivos, initialized counts e rollback ainda não estão fechados |
| file/device zero-copy | **Pesquisa** | provenance, offset, mutation lifetime e fallback variam por host |
| `transferable`/`shareable` estruturais | **Possível agora** | fields, captures, borrows, cleanup e interface compilada fornecem facts fechados |
| data-race freedom e happens-before | **Possível agora** | ownership, tasks, channels, services, locks e atomics fornecem edges fechados |
| `var atomic` e orders estáticas | **Possível agora** | superfície baixa diretamente para atomic load/store/RMW/cmpxchg |
| fallback atomic não lock-free | **Provável** | runtime striped lock preserva semântica; signals e freestanding exigem profile |
| `Atomic<T, lockFree: true>` | **Possível agora** | target e alignment resolvem o contrato em compile time |
| `Mutex.withLock` scoped | **Possível agora** | closure não escapa e cleanup síncrono fecha unlock |
| `AsyncMutex.withLock` sem suspension interna | **Provável** | fila cancel-safe e scheduler precisam de protótipo |
| RCU e snapshot cell | **Pesquisa** | publication é simples; reclamation, ABA e leitura longa não estão fechados |
| facts trusted para FFI e synchronization customizada | **Pesquisa** | segurança exige negative facts, target/digest e uma boundary `unsafe` auditável |
| domain default por módulo | **Rejeitado** | import não possui instance, lifecycle ou executor |
| QoS na syntax de `spawn` | **Pesquisa** | policy não pode parecer garantia de ordering ou deadline |
| `bootstrap.w0` e self-host antes de tasks | **Provável** | subset fechado; seed C e adapter MLIR precisam de prova |
| mimalloc como profile | **Provável** | API e build são conhecidos; versão, targets e foreign mix exigem benchmark |
| mimalloc universal | **Rejeitado por enquanto** | origem estrangeira, versão e targets impedem um default sem evidência |
| SQLite como durability universal | **Rejeitado** | adapter oficial é útil; semântica universal não é portátil |
| seccomp por módulo importado | **Rejeitado** | import não é uma security boundary |
| sandbox portátil por process/Wasm | **Provável** | depende do host, mas preserva o contrato |
| `fn<C>` com static archive | **Provável** | depende primeiro da façade C e do build hermético |
| `fn<Rust>`/`fn<Swift>` | **Pesquisa** | toolchain, runtime, ABI e agrupamento são maiores que C |
| álgebra simbólica completa no core | **Rejeitado** | package T2 experimental preserva evolução |
| custom operators e precedência do usuário | **Rejeitado** | piora parser, tooling e previsibilidade |
| macros/annotations universais | **Rejeitado** | cria uma segunda linguagem e hidden behavior |

## 25. Produto de referência Última Luz

O Restaurante Última Luz homenageia o absurdo cósmico popularizado por Douglas
Adams. Os personagens, diálogos, pratos e eventos do corpus são originais. O
humor vem de situações técnicas: reservas em fusos relativísticos, cozinha
térmica, estoque por telemetria, previsão tensorial, cobrança idempotente e
burocracia de encerramento.

Última Luz não é um exemplo descartável. Ele é o alvo de especificação
executável do W. O projeto usa o produto para:

- experimentar a superfície da linguagem;
- fechar contratos de std, runtime e build;
- testar parser, formatter e type checker;
- executar regressão semântica e de performance;
- validar targets e host profiles;
- produzir material do Book e de treinamento.

O primeiro product é um oracle determinístico e não exige deployment:

```text
last-light-simulation / LastLightSimulation
  → profile fechado
  → simulação por ticks
  → energia + receita + filas
  → relatório estável
```

O process product usa um protocolo de aplicação compartilhado:

```text
last-light-native / entry .default
  → process.main
  → CLI / TUI / servidor local
  → Command
  → dispatch
  → RestaurantApi
  → AppResponse
  → texto plain / ANSI / JSON
```

O component product usa outro lifecycle:

```text
last-light-worker / LastLightWorker
  → http.fetch
  → Command
  → RestaurantApi
  → JSON
```

O grafo lógico possui duas materializações:

```text
restaurant-core
  ├─ single-process → main
  └─ split-services → gateway + planning + finance + dining

deployment local       → uma host placement
deployment distributed → native units + WASI 0.3 + device releases
```

Os dois packings preservam providers, imports, supervisor e service effects.
O deployment seleciona somente units prebuilt.

A rota de trabalho longo testa o owner runtime:

```text
ServiceFamily<OrderCoordinatorApi, OrderId>
  → turn curto
  → WorkKeyRef.tryStart
  → root de fulfillment
  → prepare / wait / capture / serve points
  → signal / snapshot / cancel / outcome
```

Na segunda rota, o mesmo `Command` e o mesmo `AppResponse` impedem que cada
adapter de host crie semântica de negócio própria. O modo ANSI é uma
apresentação de texto. Ele não introduz uma UI library no core ou na std.

O adapter escolhe uma autoridade antes do dispatch. CLI e line host usam
`localOperator`. HTTP usa `remoteClient` e não aceita `Command.shutdown`.

O `place()` atual mantém um closed turn durante todo o atendimento. Ele preserva
invariantes, mas bloqueia `status()` e `cancel()` na mesma instance. O corpus
mantém esse handler como oracle adversarial.

O gate operacional exige um turn curto de aceitação. Um owner supervisionado
continua o workflow por pedido. Status e cancelamento usam turns curtos. A key
do pedido seleciona a instance. `SupervisorRef` e deployment data-only são
**Forma vigente**. Points, retry, timer e evento duráveis também possuem forma
vigente. O journal e o adapter ainda precisam de implementação.

Products adicionais aumentam a superfície sem criar linguagens paralelas:

```text
mobile      → app lifecycle e shell de UI nativa
controller  → sensors, interrupts, radio e MMIO
audio       → callback com deadline e fixed buffer
wifi        → HTTP, secrets, rate limit e durable sessions
orbit       → satellite services, units e telemetry
horizon     → event time, sensor fusion e tensors
accelerator → kernels e device bundles
benchmark   → HTTP/database workloads e performance evidence
```

O repository do produto é um workspace:

```text
last-light/restaurant      → products e runtime do restaurante
last-light/menu-compiler   → build tool bootstrap.w0
compile-final-menu         → menu source -> resource no CAS
```

O segundo package é uma `.build` dependency. Ele executa no execution target e
não entra no payload nativo. O workspace usa o member local; `w publish check`
prova que a release também resolve fora do workspace.

O gate final é o **Turno do Horizonte Violeta**:

```text
RestaurantApi
  → parser streaming de comanda
  → build transform do cardápio restrito a bootstrap.w0
  → Restaurant service
  → keyed coordinator + fulfillment supervisor
  → workflow journal + typed event inbox
  → async calls com payloads owned
  → parallelMap bounded da brigada
  → controle PID com `init`, computed property, ranges e units
  → tensor de previsão
  → sensor C + fn<C>
  → grafo shared/weak + callback pinned
  → pricing + billing idempotente
  → DiningRoom service
  → mailbox, ServiceFailure e cycle oracle
  → AppResponse
  → cleanup, trace e provenance
```

Uma injeção de falha em cada seta não pode deixar task, lease, buffer, mailbox
item, event, history record, shared owner, callback ou pagamento sem estado
observável. O compiler de cardápio precisa continuar dentro do fechamento W0.

Os gates são cumulativos:

1. `LastLightSimulation` passa parser, type-check, replay e runtime;
2. os adapters de host preservam `Command` e `AppResponse`;
3. o turno completo passa deployment, FFI, fault injection e provenance;
4. package, lock e build reproduzem cada target;
5. mobile, firmware, áudio e devices passam seus resource gates;
6. benchmarks preservam semântica e publicam evidence suficiente.

Enquanto o compiler não existe, o documento deve informar quais gates são
oracles e quais possuem evidência executável. Um parse do Tree-sitter não prova
type-check, lowering ou comportamento runtime.

O produto detalhado está em
[Restaurante Última Luz](reference/last-light/README.md). Products, targets e
comandos estão em [BUILD.md](reference/last-light/BUILD.md). Os planos ficam em
[deployments/](reference/last-light/deployments/).

## 26. Protocolo de revisão

**Exemplo:** pessoas e modelos corrigem o mesmo erro de ownership no restaurante
antes de informar preferência pela syntax.

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
- `spawn<.compute>` contra `spawn<domain: .compute>` e `spawn on .compute`;
- `T<(.member predicate)>` contra `value.member`, `where` e constructor;
- `Array<u8><(.count <= 64)>` contra uma static list no mesmo envelope;
- `: self` explícito contra retorno implícito do receiver e retorno `()`;
- `(take value).method()` contra consumo implícito e free function;
- error em `take fn` contra restauração implícita do owner;
- associated member direto contra protocol requirement e mutable type storage;
- struct transparente contra `export` em cada field e export total do tipo;
- pattern nominal `Type(field, ...)` contra record pattern com `{}`;
- tuple scrutinee contra syntax especial para múltiplos valores;
- patterns fechados contra handler customizado pelo usuário;
- `...` externo obrigatório contra exaustividade aberta implícita;
- object encapsulado contra storage público e constructor herdado;
- overload por forma contra ranking por tipos e nomes distintos;
- vários initializers contra initializer único e factories nomeadas;
- computed property property-safe contra method com `try` ou `await`;
- static record/list contra interpretações universais de extension e constraints;
- ponteiro `fn`, `some fn` e `any fn` contra um único callable apagado;
- call posicional por valor contra labels e defaults preservados no function type;
- `fn`, `mut fn` e `take fn` contra protocols callable separados;
- signature invariável contra variance e effect widening implícitos;
- `fn<C>` contra `fn<lang: .c>`;
- slot angular nomeado contra case enum posicional em erro e evolução de schema;
- closure `=>` contra `fn(...)`;
- nested matrix contra `;`;
- import de namespace compacto contra a forma DB1 com `from`.

## 27. Plano de implementação

Cada fase entrega um corte vertical, tests e uma demonstração reproduzível no
corpus ou na CLI. O portal gerado começa somente depois do design freeze.

### 27.1 Fase -1 — design e corpus

**Exemplo:** cada forma vigente possui um caso positivo, um negativo e uma
alternativa preservada.

- consolidar este documento;
- criar corpus W positivo, negativo e comparativo;
- completar o restaurante cósmico;
- fixar diagnostic IDs e formatter examples.

Saída: toda forma implementada possui contrato, alternativa e teste.

### 27.2 Fase 0 — lexer, parser e formatter

**Exemplo:** `parse → format → parse` de `callables.w` produz árvores
equivalentes e nenhum error node.

- lexer lossless;
- tokens numéricos exatos, radix, exponent e suffix sem sign incorporado;
- recursive-descent/Pratt;
- EBNF;
- CST/recovery;
- modifiers `const fn` e `const init`;
- parâmetros de chamada `name: const T` em declarations e function types;
- contratos estáticos com expression, record e list payloads;
- referência `.member` contextual sem perda no CST;
- patterns nominais de struct e marker `...`;
- formatter idempotente;
- Tree-sitter e semantic highlight projetados do corpus.

Saída: parse/format/parse estável e diagnostics preparados.

### 27.3 Fase 1 — AST, nomes e tipos

**Exemplo:** `w check` rejeita um overload por tipo antes de existir backend.

- AST e module graph;
- imports, visibilidade efetiva e interface normalizada;
- primitives, `()`, `Never`, structs, enums, functions, Option e error sets;
- literais exatos, widths numéricas, conversões seguras e ranges;
- function pointers, callable modes, opaque callables e erased callables;
- evolução de structs, diff de interface e classificação SemVer;
- associated constants, functions, types e `: self`;
- overload sets por forma de call e interface normalizada;
- initializer sintetizado, vários `init` e definite initialization;
- computed properties e property requirements;
- generic signatures, primary associated types, witnesses e inference local;
- refinements e enum case subsets;
- argumentos `const` de enum, extensions especializadas e paths refinados;
- rest signatures, call-shape intersection e `each` expansion;
- synthesis core, `TypeId` e interfaces de reflection;
- grafo const, ConstIR, quotas e ConstValue;
- verificação de parâmetros de chamada `const` no call site;
- `ProofFacts` para intervalos, case-sets, comprimentos, shapes e flow;
- interface compilada inicial.

Saída: `w check` verifica o subset síncrono do restaurante.

### 27.4 Fase 2 — HIR, MLIR e executável nativo

**Exemplo:** o mesmo programa aritmético gera HIR equivalente pelo seed C e pelo
frontend self-hosted.

- HIR tipada;
- propagation de facts, eliminação de checks e optimization record;
- witnesses sintetizados e descriptors alcançáveis;
- dialeto W/MLIR e verifiers;
- arithmetic/control lowering;
- integer checked, float strict, total order e numeric conversion lowering;
- String UTF-8, views, decoder incremental e maximal-subpart tests;
- LLVM/native e runtime core;
- seed C aceita o primeiro `bootstrap.w0` e emite C11;
- corpus diferencial entre o caminho seed-C e W/MLIR.

Saída: payload determinístico para programas síncronos nos dois caminhos.

### 27.5 Fase 3 — memória, errors e C

**Exemplo:** um callback C com context executa cleanup uma vez em success, error
e cancelamento.

- initialization e whole-value move;
- operação `pin`, `Pinned<T>`, projections e callback storage estável;
- receiver `take fn`, deinit e saídas com consumo;
- transições typestate consuming e outcomes que devolvem o novo owner;
- borrows, drop e defer;
- typed errors e panic boundary;
- allocator hooks;
- `foreign c`, unsafe e wrappers;
- primeiro adapter `fn<C>` com body opaco e static archive.

Saída: sanitizers e corpus negativo não encontram dangling/double drop.

### 27.6 Fase 4 — bootstrap e self-host

- fechar o profile `bootstrap.w0`;
- escrever `compiler/core-w0` em W;
- fixar o adapter C para MLIR;
- gerar stages A, B e C;
- comparar HIR, interfaces, payloads e diagnostics;
- publicar a recipe de recovery pelo seed.

Saída: o core W compila o próprio source sem tasks, services ou packages.

#### 27.6.1 Gates internos do self-host

O self-host não começa no primeiro build completo. Cada gate adiciona somente a
capacidade necessária para o gate seguinte.

| Gate | Capacidade mínima de W | Prova |
|---|---|---|
| SH0 | bytes, UTF-8, source locations, lexer e diagnostics | tokeniza o próprio source |
| SH1 | parser, recovery, AST, static contracts, ConstIR, modules, imports e names | cria AST e tabela const de forma estável |
| SH2 | scalars, aggregates, enums, generics, associated members e type checking | verifica os módulos do core |
| SH3 | initialization, move, borrow, drop, errors e collections | constrói HIR sem GC |
| SH4 | HIR tipada, verifier, serialization e deterministic order | round-trip preserva a HIR |
| SH5 | C ABI, foreign units, filesystem, argv, path, environment e backend adapter | gera um compiler executável |
| SH6 | recipes herméticas, stages A/B/C e comparação normalizada | recompila o mesmo source |
| SH7 | seed C por toolchains diversos e recipe de recovery | reproduz a rota auditável |

Todos os gates usam allocation fallible. Nenhum gate depende de clock, random,
locale, ordem de `Map` ou variável de environment não declarada. SH5 é o menor
ponto que permite dizer “W compila W”. SH6 prova convergência. SH7 reduz a
confiança necessária no seed.

Em SH5, o parser self-hosted reconhece toda a grammar congelada do design vigente. Ele pode
emitir um diagnostic de profile para semantics que ainda não possuem lowering.
As fases seguintes adicionam esses verificadores e lowerings ao compiler
self-hosted. O source de `compiler/core-w0` continua restrito a W0. Assim, tasks,
services, units, tensors e packages não ampliam a base de recovery.

### 27.7 Fase 5 — tasks

**Exemplo:** `mixPair` cancela o sibling após erro e aguarda ambos os children
antes de sair do scope.

- async state machine;
- `async<.domain> let`, `spawn<.domain> let` e inheritance de preference;
- linear Task, `TaskOutcome` e cancellation;
- `concurrentMap`/`parallelMap` bounded;
- blocking adapter e callback scheduling;
- HIR verificada antes do lowering async;
- executor cooperativo e pool paralelo bounded;
- schema de domain e bindings de profile;
- budget compartilhado para groups paralelos aninhados;
- deterministic test executor.

Saída: restaurante executa I/O concorrente e lotes paralelos com ordering,
backpressure e cleanup reproduzíveis.

### 27.8 Fase 6 — services e host entries

**Exemplo:** `.default` valida `runNative` contra `process.main`.
`LastLightWorker` valida `fetch` contra `http.fetch` em outro product.

- `entry` e host profiles;
- service instance manager;
- closed turn, generation e drain;
- mailbox com três quotas;
- `ServiceFailure`, cycle detection e `ServiceRef`;
- `ServiceBinding`, `ServiceFamily` e validation do runtime graph;
- imports, exports e provider injection por interface;
- `SupervisorRef` em memória, admission bounded e `WorkOutcome`;
- cancelamento, retention, tombstones e drain de roots;
- verificação replayable e journal de steps em memória;
- `work.step`, sleep, event inbox e effect IDs estáveis;
- packing em service-only units e artifact index;
- validation de deployment plan e lock contra o product envelope;
- tracing e local fast path;
- process e `wasm32-wasip3` boundary experimentais;
- `Request`, `Response`, `http.Context` e admission HTTP;
- adapter HTTP nativo e adapter `http-worker@1` com o mesmo oracle;
- `ByteSink.writeMany` com fallback, short progress e cancel drain;

Saída: CLI e HTTP exibem hops, queues, overload, cycle, trabalho supervisionado
e restart de instance.

### 27.9 Fase 7 — packages e SDK

**Exemplo:** CI recompila um package público pela recipe e compara o digest antes
de marcar a versão como reproduced.

- package parser, resolver, lock e CAS;
- workspace parser, members exatos e resolução standalone;
- usages `.product`, `.build`, `.test` e `.benchmark`;
- feature closure por root, target role e usage;
- separação entre lock, recipe e artifact record;
- builds `--locked`/offline;
- host `build-transform@1`, action graph e outputs no CAS;
- T0/T1 mínimos;
- descriptors SQL, codecs de row e adapters database com pool limitado;
- cache local com limite, replacement, expiration e loader compartilhado;
- provenance, SBOM e reprodução local;
- lens por import;
- SQLite adapter para steps, timers, events e outcomes supervisionados;
- recovery por operation version, effect ID e schema;
- crash injection antes e depois de cada attempt e journal commit.

Saída: uma máquina limpa reconstrói o mesmo payload sem rede durante o build.

### 27.10 Fase 8 — ciência e extração

**Exemplo:** `Matrix<2, 3> @ Matrix<3, 4>` baixa para CPU e mantém shape
`Matrix<2, 4>`.

- units/refinements completos;
- BigInt, FixedDecimal, Rational e math accuracy profiles;
- tensor CPU, `@`, accumulators e modes numéricos;
- `f16`, `bf16`, quantization e requantization explícitas;
- SIMD por range, storage estreito experimental e `w explain performance`;
- rebuild em estágios;
- suíte de conformidade;
- decisão sobre mover W para repository próprio.

Saída: design W demonstrado de ponta a ponta e pronto para revisão pública.

### 27.11 Gates

| Gate | Pergunta | Evidência mínima |
|---|---|---|
| memória | `shared`, arena e allocator compõem sem surpresa? | benchmarks, cycles, FFI e cancellation |
| tasks | lowering preserva join, cancelamento e mobilidade? | testes diferenciais e scheduler reproduzível |
| services | closed turn, admission e cycle são previsíveis? | três workloads, failure injection e trace |
| units | `<>` supera `[]` em uso real? | estudo humano e modelo |
| ML | shape/operator reduzem erros sem esconder cost? | corpus CPU/SIMD/device |
| performance | facts aceleram sem mudar semântica ou layout público? | differential oracles, optimization records e benchmarks |
| packages | resolver e evidence model são operáveis? | projeto real offline/reproduzido |
| self-host | SH0–SH7 fecham e convergem? | mini compiler, builds diversos e diff de outputs |

### 27.12 Checkpoint por fase

**Exemplo:** uma fase não fecha quando seus testes passam, mas `git diff --check`
ou o corpus negativo falha.

Cada checkpoint executa:

1. testes afetados e corpus negativo;
2. formatter idempotente;
3. comparação debug/release;
4. benchmark proporcional ao risco;
5. `git diff --check`;
6. atualização deste documento;
7. commit pequeno com resultado e limitações.

## 28. Relação com a tentativa DB1

A “DB1” foi uma consolidação intermediária. Ela não foi um design concluído.
Esta tabela existe somente para explicar mudanças rastreáveis.

| Tema | Tentativa DB1 | Forma vigente |
|---|---|---|
| unit literal | `9.81[m/s^2]` | `9.81<m/s^2>` |
| namespace import | `import name as alias from path` | `import path [as alias]` |
| refinement | `T where predicate`, com alternativas | `T<(predicate)>` |
| execution preference | superfície aberta | `async/spawn<.domain>` |
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
| static contract | aplicações pontuais | envelope `<...>` fechado por head |
| ilha multilíngue | `fn<lang>` em pesquisa | adapter externo, façade C e static archive |
| callable | `CallbackType` e capture dispersos | `fn`, `some fn` e `any fn` separam pointer, ambiente, owner e drop |

Estas mudanças são experimentais. A fotografia da tentativa DB1 continua
acessível no
[arquivo histórico](../Y/W/archive/db1-2026-07-27/README.md) e no histórico do
Git.

## 29. Registro de decisões e alternativas

Esta tabela é o checklist de revisão humana. **Forma vigente** significa
“integrar e experimentar”, não “decisão irreversível”.

| ID | Tema | Forma vigente | Alternativas preservadas |
|---|---|---|---|
| W-001 | função | `fn name(...): Return` | `func`; retorno `->`; sem keyword |
| W-002 | bindings | `const`/`let`/`var` | `let mut`; uma única keyword |
| W-003 | modifiers | ordem fixa antes de `fn` | ordem livre; effects após retorno |
| W-004 | labels | primeiro posicional, demais nomeados | todos nomeados; todos posicionais |
| W-005 | closure | `(args) => body` | `fn(args) {}`; `{ args in }` |
| W-006 | capture | inferência + `capture(...)` | `[capture]`; somente inferência |
| W-007 | visibility | módulo default, `package`, `export`; sem `private` | public universal; `public/private`; block export |
| W-008 | import seletivo | `{X} from path` | `path.{X}`; imports livres |
| W-009 | import namespace | `import path as alias` | forma DB1 `name as alias from path` |
| W-010 | módulos | manifest multi-file, DAG | declaração `module`; cycles de interface |
| W-011 | runtime top-level | declarations/const somente | init global; ordem de inicializadores |
| W-012 | tipos nominais | `type X = T` | wrapper struct; `newtype` |
| W-013 | alias | `alias X = T` | `typealias`; context-dependent `type` |
| W-014 | refinement | `T<(.member predicate)>`; range como sugar | `value.member`; `T where (...)`; `T(where:)` |
| W-015 | value generics | `const` parameters e labels | positional only; contrato universal aberto |
| W-016 | existential | `any P` | `P` sozinho; `dyn P`; `Any` universal |
| W-017 | opaque type | `some P` em local, retorno e parâmetro generic anônimo | existential; generic nomeado |
| W-018 | reflection | `reflect.Reflectable` opt-in e alcançável | metadata universal; annotations |
| W-019 | Option | `T?` com some/none | null; sentinel; result-like |
| W-020 | conversão | total, única e sem perda | tudo explícito; promotions amplas |
| W-021 | owner | único/move-first | ARC universal; GC |
| W-022 | borrow | `ref` e `inout` | lifetime annotations públicas; pointers |
| W-023 | transfer | last-use + `take` obrigatório na API | move sempre explícito; move implícito amplo |
| W-024 | copy | implícito só para `Copy`; `copy value` explícito usa `Duplicable` | `.clone()` universal; COW como contrato |
| W-025 | shared | `try share(value, using:)`, `copy` para novo owner e `weak()` | ARC implícito; promotion por expected type; region-only |
| W-026 | region | `region name(using:, limit:)` lidera e baixa para `Arena`; W0 implementa API primeiro | lifetime annotations; heap por módulo; API sem bloco |
| W-027 | allocator | capability explícita, default fixado pelo product, system portátil e profile substituível | mimalloc universal; allocator por import; default thread-local mutável |
| W-028 | OOM | fallible explícito; geral aborta boundary | throws universal; abort de process sempre |
| W-029 | layout | W opaco; C/schema explícitos | layout W estável universal |
| W-030 | tagged values | otimização invisível com fallback | tagged address obrigatório; annotation |
| W-031 | property behavior | `var Behavior name` | prefix before var; `by`; wrapper type |
| W-032 | behavior composition | composite nomeado | lista ordenada; nesting arbitrário |
| W-033 | erro | `throws E` + `try` | exceptions abertas; Result em toda assinatura |
| W-034 | error widening | case único compatível | mapping sempre explícito; `From` livre |
| W-035 | panic | encerra a fault boundary física mais próxima | unwind recuperável; tratar toda isolation como fault boundary |
| W-036 | async cleanup | `defer async` | RAII sync only; `using`; cleanup solto |
| W-037 | concorrência | `async let` | Future/Promise; task API somente |
| W-038 | paralelismo | `spawn let` | mesma keyword de async; parallel loop apenas |
| W-039 | execution domain | `async/spawn<.domain>` | `<domain: .name>`; `on .name` (**Rejeitado por enquanto**); descriptor-only |
| W-040 | Task | linear, lexical, one-await | Future clonável; detached default |
| W-041 | grupos | lexical e bounded | queue ilimitada; thread pool exposto |
| W-042 | solicitação de cancelamento | `task.cancel(reason:)` intrínseco e `CancellationReason` fechado | statement `cancel` (**Rejeitado por enquanto**); async thread cancellation |
| W-043 | erro concorrente | primário lexical + anexos | primeiro a concluir; aggregate always |
| W-044 | atomics | `var atomic`, seq-cst comum e contratos estáticos de order; detalhes W-440–453 | C-like default; wrapper obrigatório; lock oculto em `var` comum |
| W-045 | nomes de mobilidade | `transferable`/`shareable` derivados; detalhes em W-424–429 | `Send`/`Sync` públicos; runtime checks |
| W-046 | service | keyword + protocol + closed turn | object+metadata; actor reentrant |
| W-047 | service call | ServiceRef sempre async | local sync/remoto async; RPC explícito |
| W-048 | mailbox | bounded por itens, bytes e trabalho em voo; detalhes em W-458–472 não mudam a call boundary | drop; unbounded; tratar como channel local |
| W-049 | entry curto | `entry { ... }` usa o default slot único | main mágico; manifest-only |
| W-050 | entry composto | descriptor de slots tipados; anonymous base e header handler | repetição total; `entry defaults`; conformance |
| W-051 | units | `9.81<m/s^2>` | `[]`; `{}`; whitespace SI |
| W-052 | custom unit | `dimension`/`unit` declarations | wrapper types; runtime registry |
| W-053 | affine/log units | metaconstrutores distintos | scale universal; runtime-only |
| W-054 | range | quatro closures; unilateral em argumento/pattern; intervalo | dois ranges; producer universal |
| W-055 | membership | `value in (a, b)` | `.isOneOf`; equality chain |
| W-056 | exponent | `**`; `^` somente em unit grammar | `^` universal; `pow` only |
| W-057 | integer safety | checked, panic; APIs alternatives | wrapping default; Result operators |
| W-058 | float | IEEE strict default | fast default; build-mode semantics |
| W-059 | String | owned UTF-8 contíguo | tree/rope default; COW contract |
| W-060 | String indexing | access mode `view`, sem `string[i]` | scalar index; grapheme index default |
| W-061 | raw string | `#"..."#` | `r"..."`; backtick |
| W-062 | scalar/byte | `'x'` e `b'x'` | constructor only; char=grapheme |
| W-063 | arrays/maps | `[]` e `[key: value]` | braces para map/set |
| W-064 | matrix literal | nested arrays | semicolon/whitespace; constructor only |
| W-065 | matrix multiply | `@` | `*` + `.*`; `matmul` only |
| W-066 | broadcast | diferente shape explícito | Array API implicit; dotted operators |
| W-067 | device | transfer explícita | automatic placement |
| W-068 | SDK | T0/T1/T2 | uma stdlib plana; packages somente |
| W-069 | prelude | pequena, edition-frozen | toda std implícita; nada implícito |
| W-070 | print | T1 contextual ao host | T0 intrinsic; `io.print` obrigatório |
| W-071 | C | `foreign c` + unsafe wrapper | C superset; generated bridge only |
| W-072 | inline language | `fn<C>` com adapter externo | `fn<lang: .c>`; library import; multi-language v0 |
| W-073 | parser | recursive-descent/Pratt + EBNF | generated parser; Tree-sitter compiler |
| W-074 | editor parser | Tree-sitter projection | compiler CST compartilhada |
| W-075 | IR | W/MLIR antes de lowering | C IR público; LLVM direto |
| W-076 | bootstrap | C11 seed, self-host cedo | TypeScript/Bun; C++ compiler inteiro |
| W-077 | build tool | CMake/Ninja no seed | xmake; custom builder antes do self-host |
| W-078 | packages | manifests de package/workspace data-only + lock compartilhado | executable manifest; lock opcional |
| W-079 | resolver | determinístico, uma versão por identity em cada resolution realm | múltiplas versões no mesmo realm |
| W-080 | artifact | source-first, static preferred | binary-only; dynamic-only |
| W-081 | canonical bytes | deterministic CBOR | WLO imediato; JSON assinada |
| W-082 | digest | tagged SHA-256 inicial | hash fixo eterno; hash recebido sem metadata |
| W-083 | registry | metadata authority; mirrors por digest | registry hospeda tudo e define trust |
| W-084 | evidence | eixos separados | selo único; estrelas |
| W-085 | resource lens | facts/estimates/measurements | número exato universal; nada no import |
| W-086 | formatter | 120 colunas e uma forma | user-configurable style amplo |
| W-087 | tests | runner único com modos | ferramentas sem grafo comum |
| W-088 | AI | schemas/diagnostics comuns | dialeto AI; token count como objetivo único |
| W-089 | SQLite | durable adapter T2 | storage universal |
| W-090 | sandbox | capability + process/OS/Wasm | seccomp por módulo |
| W-091 | wRPC/wQL | packages após core | keywords W; protocolo universal |
| W-092 | WLO/tree strings | pesquisa com fallback | formato/representação default |
| W-093 | GPU/HDL | lowerings posteriores | requisito da v0 |
| W-094 | custom operators | rejeitado | precedência e operators do usuário |
| W-095 | annotations/macros | rejeitado na v0 | `@annotations`; macro AST universal |
| W-096 | portal | gerar após design freeze; protótipo congelado | páginas manuais; escolher Astro agora |
| W-097 | aplicação `<...>` | contrato fechado por head e payload tipado | slots universais; mapa aberto |
| W-098 | campos | imutável sem prefixo; `var` para mutation | `let` obrigatório; `let` opcional |
| W-099 | collection dinâmica | `Array<T>`, `Map<K, V>` e `Set<T>` | `[T]`; braces para map/set |
| W-100 | tensor indexing | `tensor[i, j]`; prefixo retorna view | nesting obrigatório; método `at` |
| W-101 | recurso async | `defer async` + `take async fn`; obrigação linear em pesquisa | async destructor; `using await`; lint |
| W-102 | receiver | `fn` borrow, `mut fn` exclusivo, `take fn` owned, `static fn` sem receiver | `self`; inferir static; função livre |
| W-103 | camadas de memória | semântica separada de lowering, representação e host | tag ou allocator como semântica |
| W-104 | borrow suspenso | permitido somente com owner, frame e alias provados | proibir sempre; lifetime annotation |
| W-105 | pinning | interno sem annotation; `pin` explícito produz `Pinned<T>` público | annotation universal; raw pointer |
| W-106 | ciclos shared | `weak`, close, região ou lifecycle owner; sem collector default | cycle collector universal |
| W-107 | pointer provenance | address separado; round-trip não restaura authority | pointer como integer |
| W-108 | origem de allocation | owner/control block/side table preserva deallocator | bits do pointer obrigatórios |
| W-109 | compactação | portátil → niche → low-bit; high-bit em pesquisa | tagged address obrigatório |
| W-110 | hardening | sanitizer, PAC, MTE e capability têm precedência | compactação vence o profile |
| W-111 | subset self-host | profile `bootstrap.w0` fechado | compiler exige a linguagem inteira |
| W-112 | seed output | W0 para C11, backend normal W/MLIR | MLIR completo no seed; C como backend público |
| W-113 | momento do self-host | depois de memória/FFI e antes de tasks | somente após o design completo |
| W-114 | cláusula estática | `<...>` no source e record tipado na HIR | `where`/`on`; modifier map |
| W-115 | slots angulares | schema declara posição, labels e slot primário | inferir slot pelo nome do enum case |
| W-116 | evolução self-host | gates SH0–SH7; W0 fechado e core separado | marco único; compiler usa toda o design vigente |
| W-117 | eixos de execução | lifetime, intent, preference, isolation e affinity separados | thread group único |
| W-118 | início de child | `async let`/`spawn let` iniciam na declaração | lazy no primeiro await |
| W-119 | task longa | owner runtime explícito; sem detached sem owner | drop destaca; task global |
| W-120 | outcome de task | success/error/canceled; panic encerra fault boundary | cancel em `E`; panic como Result |
| W-121 | seleção de error | ordem lexical declarada | primeira completion sempre vence |
| W-122 | cancelamento | cooperativo, idempotente e sem rollback implícito | matar thread; transação implícita |
| W-123 | resolução de domain | isolation/affinity vencem preference | contrato do caller substitui isolation |
| W-124 | grupos dinâmicos | concurrent/parallel map bounded e ordering explícito | queue ilimitada; intent oculto |
| W-125 | stream/channel | pull single-pass e MPSC bounded; detalhes em W-454–472 | generator unbounded; channel bidirecional universal |
| W-126 | memory model | safe W data-race-free; edges fechados e DRF-SC salvo orders explícitas | race definida em safe code; somente “thread-safe” nominal |
| W-127 | FFI concorrente | metadata conservadora e callback em executor conhecido | assumir non-blocking |
| W-128 | async lowering | invariantes W antes de MLIR Async/LLVM coroutine | backend define semantics |
| W-129 | lifecycle de instance | identity + generation; restart invalida state anterior | reuse de pointers/frames |
| W-130 | admission | quotas de itens, bytes e in-flight | unbounded; limite só por item |
| W-131 | falha de call | `E` e `ServiceFailure` são effects separados | transporte dentro de todo `E` |
| W-132 | call cycle | ancestry causal rejeita ciclo closed-turn conhecido | esperar somente deadline |
| W-133 | output durável | outcome de step só aparece depois do commit; outbox para mensagem; gate geral em pesquisa | gate geral inferido na v0 |
| W-134 | scheduler de teste | clock/I/O/schedule injetáveis e replay | teste somente por timing real |
| W-135 | payload de service | value/`take`/capability; sem `ref`/`inout` do caller | borrow no fast path local |
| W-136 | paralelismo de service | instances keyed; mesma key serial | singleton longo; reentrância implícita |
| W-137 | RPC encadeado | `CallPipeline` explícito em pesquisa | toda `ServiceRef` vira Promise lazy |
| W-138 | payload angular | `()`, `{}` e `[]` são expression, record e list | três operadores universais |
| W-139 | extensão de tipo | refinement, extension, struct, enum e C union separados | `T<{...}>` universal |
| W-140 | foreign artifact | unit agrupada, archive/object e façade C | archive por função; C source obrigatório |
| W-141 | foreign parser | body opaco entregue ao adapter da linguagem | parser W interpreta subset externo |
| W-142 | foreign delimiter | body braced com scanner do adapter | raw fence hash; parser W conhece strings externas |
| W-143 | language tag | `LanguageAdapterId` fixada no lock | enum eterno no compiler; string ou command livre |
| W-144 | referência contextual | `.member` usa subject ou enum esperado; HIR qualificada | somente `value.member`; `.case` apenas |
| W-145 | generic refinado | `Array<T><(predicate)>` separa aplicação e refinement | `Array<[T, predicate]>`; slot misto |
| W-146 | unit e bottom | `()` e `Never` | `Void`; `!`; retorno omitido dependente do contexto |
| W-147 | retorno fluente | `: self` explícito como reborrow | retorno `self` implícito; `Self` owned; builder externo |
| W-148 | associated member | `const`, `static fn` e `type` requerido | companion object; metatype runtime obrigatório |
| W-149 | associated type witness | `type Name` exige `alias Name = T` | `associatedtype`; `type Name = T` contextual |
| W-150 | mutable type storage | ausente; owner de `entry` ou service explícito | `static var`; módulo singleton |
| W-151 | object singleton | `object` permite várias instances; singleton é composição | object declaration singleton; module singleton |
| W-152 | construção | `Type(...)` baixa para `construct`; sem promessa de placement | `new Type`; literal `Type {...}` |
| W-153 | initializer sintetizado | struct usa menor nível; object fica no módulo | visibilidade do tipo sempre; sempre privado |
| W-154 | initializer customizado | vários `init` com formas disjuntas; `throws E`; factory nomeada | initializer único; `init?`; `async init` |
| W-155 | definite initialization | duas fases; sem uso de `self` parcial; cleanup por field | zero universal; runtime check; partial safe value |
| W-156 | computed property | `name: T { get }`; `var` exige write accessor | getter implícito; method obrigatório |
| W-157 | efeitos de property | property-safe, síncrona, local e sem `throws` | `async`/`throws` property; custo irrestrito |
| W-158 | mutation de property | `set(value)` e `modify` com `return inout` escopado | get-modify-set implícito; observers |
| W-159 | property requirement | `{ get [set] [modify] }`; stored field pode ser witness | protocol exige storage; reflection estrutural |
| W-160 | struct transparente | sem `init`: stored fields herdam visibilidade do tipo | `export` por field; todos os members herdam |
| W-161 | struct encapsulado | `init` explícito restaura default de módulo nos fields | keyword `opaque`; field sempre público |
| W-162 | object | storage e initializer sintetizado ficam no módulo | herdar visibilidade do object; constructor público |
| W-163 | enum e protocol | cases e requirements herdam; witness não repete modifier | `export` repetido; todos os members públicos |
| W-164 | service | storage nunca cruza módulo; API usa protocol async | field público; computed property remota |
| W-165 | interface exportada | signature não expõe tipo menos visível; HIR normaliza | lint apenas; defaults preservados na HIR |
| W-166 | pattern de struct | `Type(field, field: pattern, ...)`; nominal e ordenado | `{field}`; tuple posicional |
| W-167 | evolução de pattern | `...` obrigatório fora do package | exaustivo externo; modifier no tipo |
| W-168 | ownership de pattern | modo uniforme owned, `ref` ou `inout` | qualifier por field; partial move |
| W-169 | limite de destructuring | struct visível; object e service rejeitados | destructuring estrutural universal |
| W-170 | evolução de struct | field com default é minor se a resolução não muda; field obrigatório é major | todo field novo é major |
| W-171 | evolução de enum | enum fechado; case novo é major | `nonexhaustive`; default case obrigatório |
| W-172 | source contra schema | source, ABI e wire evoluem por contratos separados | derivar schema do struct |
| W-173 | verificação SemVer | `w interface diff` classifica e sinaliza revisão | revisão manual; só major/minor binário |
| W-174 | consuming receiver | `take fn`; call usa `(take value).method()` | consumo implícito; `consuming fn`; free function |
| W-175 | saída consuming | success, error e cancellation consomem; owner pode ser retornado | restaurar no error; abortar sem drop |
| W-176 | authority de `deinit` | exclusivo e não consuming; mutation sem move | borrow read-only; consumir fields |
| W-177 | supressão de drop | ausente em safe W; wrapper mantém estado válido | `discard self`; `forget` geral |
| W-178 | limite de receiver | protocol exige mode exato; service e handles aliases não usam `take fn` | adaptação com copy; service consuming |
| W-179 | `deinit` e copy | tipo com cleanup customizado não atende a `Copy` | copiar e contar drops; lint |
| W-180 | identidade de overload | owner, nome e forma de call | tipos, return type ou constraints |
| W-181 | resolução de overload | forma antes do type-check; sem backtracking | ranking de melhor candidato |
| W-182 | defaults e overload | famílias de formas devem ser disjuntas | preferência por menos defaults |
| W-183 | ownership do overload set | um owner; imports não fundem sets | overload set aberto entre módulos |
| W-184 | overload como valor | closure explícita seleciona a forma | expected type; seletor de forma |
| W-185 | vários initializers | labels e formas disjuntas | ranking por tipos; initializer único |
| W-186 | delegação de initializer | `self = Type(...)` antes de qualquer field | `self.init`; delegação parcial |
| W-187 | falha de initializer | cleanup parcial; `deinit` após self completo | zero universal; leak parcial |
| W-188 | efeitos de initializer | síncrono; `throws E`; sem `init?` | `async init`; initializer failable |
| W-189 | evolução de overload | set existente: minor; primeiro overload: major; forma alterada: major | classificação somente por nome |
| W-190 | ordem de argumentos | ordem da declaração; labels não reordenam | named arguments livres |
| W-191 | parâmetros rest | `T...` homogêneo e final; `each` expande collection | somente collection; type pack; C varargs |
| W-192 | function type | source usa `fn(A): B`; labels e defaults ficam na declaração | labels no tipo; somente inference |
| W-193 | callable concreto | `some fn(A): B` preserva tipo, captures e specialization | generic nomeado; `fn` sempre apagado |
| W-194 | callable apagado | `any fn(A): B` guarda owner, invoke e drop | `CallbackType`; box manual |
| W-195 | callable mode | `fn`, `mut fn` e `take fn` descrevem uso do ambiente | `Fn`/`FnMut`/`FnOnce`; inferência sem annotation |
| W-196 | call por valor | posicional, aridade completa e sem defaults | labels cosméticos; labels significativos |
| W-197 | capture e escape | HIR registra place, modo, lifetime, owner e drop | capture sempre weak; heap por default |
| W-198 | method reference | closure explícita mostra receiver e ownership | bound method implícito |
| W-199 | callback C | `unsafe fn<abi: .c>` fino + context/owner explícitos | converter closure W; callback universal |
| W-200 | static list | `StaticList<T>` compile-time, ordenada e apagada | named index runtime; set implícito |
| W-201 | operador `@` | família rank-1/rank-2 sem broadcast; APIs nomeadas para rank maior | contração geral implícita; `*` linalg |
| W-202 | exemplo normativo | cada contrato aponta para exemplo válido, erro ou cenário canônico | afirmação sem evidência local |
| W-203 | opaque parameter | `some P` é generic anônimo e especializado | exigir generic nomeado; existential |
| W-204 | switch | expressão exaustiva, sem fallthrough ou `break` | switch statement; fallthrough explícito |
| W-205 | ordem de case | ordem lexical, first-match e diagnostic de inalcançável | exigir patterns disjuntos; ranking |
| W-206 | múltiplos scrutinees | tuple subject e tuple pattern | `switch a, b`; matching relacional implícito |
| W-207 | custom pattern | pesquisa; conversão nomeada ou guard no design vigente | handler arbitrário; protocol de pattern na v0 |
| W-208 | callable transfer | `fn` é transferível/compartilhável; closure deriva predicates do ambiente | `Send`/`Sync` nominais; confiar no pointer |
| W-209 | compatibilidade callable | signature invariável; somente callable-mode possui lattice | variance; effect widening; ranking |
| W-210 | semântica de String | owner único, bytes UTF-8 contíguos e mutation exclusiva; static/SSO ficam internos | tree/rope default; UTF-16; COW baseline |
| W-211 | unidades e custos | sem `length`; bytes O(1), scalars/graphemes podem ser O(n) | grapheme default; cache obrigatório |
| W-212 | elementos de texto | `UnicodeScalar` Copy e grapheme como `view String` refinada; owned usa String refinado | Character/Grapheme nominal; scalar chamado Char |
| W-213 | índices de texto | origem borrowed, custo visível e uso terminal em edição | ordinal em subscript; índice universal |
| W-214 | slices de texto | byte slice é `view Bytes`; byte range para `view String` é fallible | arredondar boundary; slice sempre String |
| W-215 | Bytes | tipo binário owned distinto de `String` e `Array<u8>` | alias de Array; String aceita UTF-8 inválido |
| W-216 | conversão UTF-8 | strict, repair, borrow, copy e adoption explícitos; detalhes W-358–362 | replacement implícito; locale codec default |
| W-217 | construção de String | interpolation e Display escrevem num `String`; `+` consome left; reserve/append lideram loops | builder público; concat adjacente; String intermediário por campo |
| W-218 | raw/multiline | `#"..."#`, `${}`, multiline com dedent determinístico | hashes arbitrários; `r` prefix; três delimitadores equivalentes |
| W-219 | byte string | `b"..."` produz Bytes ASCII/escapes, sem interpolation | Unicode direto; Array literal somente |
| W-220 | igualdade Unicode | sequência exata; normalização e collation nomeadas | equivalência canônica em `==`; locale global |
| W-221 | bundle Unicode | edição, tabelas e digests fixos para UAX #15/#29/#31 e UTS #39 | versão do host; ICU obrigatório |
| W-222 | texto do host | `OsString`, `Path`, `Utf8Path` e `PackagePath` distintos; colisão NFC rejeitada | paths sempre String; bytes portáveis do OS |
| W-223 | C strings | `CString`/view separados, NUL verificado e inbound bounded | String sempre NUL; scan C ilimitado |
| W-224 | storage textual | refinement não fixa layout; reserva mínima é operação; capacity/SSO exatos não são properties | capacity pública; SSO observável |
| W-225 | estruturas textuais | rope, piece table, interning e tree string são especializadas | tree string geral; representation ABI única |
| W-226 | ordem de avaliação | esquerda para direita e sequenciada; formas condicionais short-circuit | ordem não especificada; optimizer escolhe |
| W-227 | resultados borrowed | `ref`/`inout` em tipos e retorno, provenance inferida e interface registrada | lifetime no source; lookup owned |
| W-228 | array dinâmico | `Array<T>` owned, contíguo, count/capacity O(1) e append amortizado O(1) | linked chunks default; `[T]` |
| W-229 | literais de array | `[a, b]`, `[]` contextual e `[value; count]` fixo com Copy | `[:]`; repeat clona move-only |
| W-230 | views de array | `view Array<T>` read-only Copy e `inout view Array<T>` exclusiva move-only | tipos Slice públicos; pointer público; resize pela view |
| W-231 | iteração | single-pass; borrow default, `ref`/`inout`/`copy` explícitos e `take` consome | copiar sempre; mutation estrutural durante loop |
| W-232 | pipelines | Array eager; `.lazy` e Iterator lazy; `collect()` materializa | tudo lazy; tudo eager |
| W-233 | `Map` | hashing keyed e ordem de inserção estável; full key confirma colisão | ordem de bucket; guardar somente hash |
| W-234 | `Set` | ordem de inserção; equality ignora ordem; sem literal próprio | set não ordenado; literal com chaves |
| W-235 | hashing | `Hashable: Equatable`; algoritmo/seed process-local e não persistente | XXH como ABI; hash como identity |
| W-236 | lookup borrowed | `EquivalentKey<K>` permite view com a mesma equality e hash feed | alocar key em todo lookup; equivalência ad hoc |
| W-237 | ordenação | `sort` stable por default; `sortUnstable` explícito; comparator `Ordering` | algoritmo fixo no contrato; Bool comparator |
| W-238 | maps ordenados | `SortedMap` por total order para range e key order | tornar todo Map tree; B-tree no ABI |
| W-239 | cleanup de collections | ordem inversa de índice/inserção; capacity e buckets invisíveis | drop order não especificada |
| W-240 | escopo da std | core em T0; Deque/PriorityQueue/BitSet em `std.collections`; concorrentes fora de T0 | todas as estruturas no prelude |
| W-241 | duplicação owned | `Copy` barato e implícito; `Duplicable` explícito via `copy value` | clone method; copiar owned implicitamente |
| W-242 | ausência | `Option<T>` com some/none; sem null/undefined universal | sentinela universal; pointer null por default |
| W-243 | estado de memória | definite init e move no compiler; `MaybeUninit<T>` unsafe | gravar none após move; uninitialized como valor comum |
| W-244 | controle Option | `?.`, lazy/right-associative `??` e postfix `?` só para none | force unwrap; postfix `?` para Result |
| W-245 | ownership Option | binding owned por default; `ref`/`inout`/`copy`; `take()` esvazia | copiar payload owned; mutation por optional chain |
| W-246 | Result | enum T0 success/error para storage e composição | Result implícito só em debug; exceptions abertas |
| W-247 | `try` | propaga `throws E` ou `Result<T,E>`; cada closure é outro effect scope | postfix `?` para ambos; propagação implícita |
| W-248 | error type | enum fechado e estruturado; `throws E` sempre tipado | throws sem tipo; string obrigatória |
| W-249 | effect polymorphism | generic `E: Error`; bottom `Never` é aceito e especializa como nonthrowing | keyword `rethrows`; erasure universal |
| W-250 | catch | ordem lexical, guard e exaustividade no contexto nonthrowing | ranking de catches; catch implícito |
| W-251 | uso de valores | todo valor non-unit/non-Never deve ser usado ou descartado com `let _` | annotation must-use; ignorar Result |
| W-252 | lowering de error | tagged result e cleanup edges; trace sidecar não observável | host exception unwind; sem trace estruturado |
| W-253 | fault boundary | process, Wasm instance ou compartment com teardown próprio | service lógico sempre recuperável; panic capturável |
| W-254 | panic | payload limitado, code estável e sem user cleanup garantido | payload alocável obrigatório; user recovery |
| W-255 | OOM | alocação normal pode panic; APIs `try*` retornam AllocationError | toda alocação fallible; emergency handler universal |
| W-256 | cleanup | saídas estruturadas e cancel executam LIFO; panic não garante user cleanup | panic unwind; defer que propaga error |
| W-257 | diagnostic | code estável, spans em bytes, facts e relação root/cascade | texto livre como API; reutilizar code |
| W-258 | fix e policy | edits com applicability/digest; ordem estável; error não suprimível | fix sem precondition; source suppression no design vigente |
| W-259 | `try?` | converte falha recuperável em Option e flatten; não captura panic/cancel | excluir o sugar; `try!`; preservar error oculto |
| W-260 | const context | `const`, value argument, contract, fixed size, unit e refinement exigem avaliação | confiar no optimizer; executar tudo em compile time |
| W-261 | const callable | `const fn` e `const init` explícitos; mesma semântica runtime | inferir API pelo body; função exclusiva da fase |
| W-262 | modifier const | depois de `static`; incompatível com unsafe/async; combina com mut/take | annotation; `comptime fn`; combinação irrestrita |
| W-263 | const-safe | local mutation, loops, recursion, dados e typed errors; sem capabilities/FFI | subset expression-only; executar host code |
| W-264 | fase | sem `isComptime`; mesmo input produz o mesmo valor nas duas fases | branch por fase; implementação separada |
| W-265 | const failure | error não tratado, panic e quota viram diagnostics W-CONST | fault boundary no compiler; AllocationError catchable |
| W-266 | ConstRepresentable | predicate derivado para valores estruturais sem identity/authority | protocol implementável; qualquer tipo serializável |
| W-267 | materialização | const sem owner; uso owned cria valor independente; borrow não escapa | singleton mutable; endereço estável público |
| W-268 | target | evaluator usa target e módulo `w.target`; nunca a máquina host | host semantics; target facts implícitos |
| W-269 | build input | módulo gerado e recipe declarada; sem env/file/clock no evaluator | `#define`; env intrinsic; acesso sandboxed ad hoc |
| W-270 | quotas | steps, heap, depth e result na recipe; wall clock não semântico | quota por source; sem limite; timeout como semântica |
| W-271 | cache const | chave inclui ConstIR, args, target, bundles, evaluator, quotas e generated modules | cache por source text; omitir target |
| W-272 | type builder | identidade declarada + const parse/refinement; sem função que retorna Type | `type(regex)`; type function arbitrária |
| W-273 | geração | ConstIR para ConstValue; codegen em tool target; WLO continua codec | stringify/reparse; macro AST universal |
| W-274 | feedback | PGO declarado só orienta otimização; nunca altera const/tipo/interface | substituir const com execução anterior |
| W-275 | implementação const | evaluator HIR antes de MLIR; folding MLIR não define correção | JIT host; canonicalizer como evaluator semântico |
| W-276 | bootstrap const | CE0 no seed C e core W0; ConstValue normalizado deve coincidir | excluir const fn do seed; evaluator só no compiler final |
| W-277 | force expression | sem `comptime expr` na baseline; binding const nomeia o resultado | keyword obrigatória; const block na v0 |
| W-278 | static argument | predicate estrutural sem float/dynamic collection; serialização canônica na identidade | qualquer ConstValue; somente integer |
| W-279 | const e overload | const não distingue call shape; elegibilidade não promete termination/quota | overload por fase; inferir const por call |
| W-280 | generic kinds | type e `const`; sem lifetime/effect/HKT/pack no source | kinds extensíveis; template sem kind |
| W-281 | generic labels | type positional; `const` nomeado; `const _` cria slot primário posicional | todos posicionais; named type args |
| W-282 | generic scope | parâmetros entram em scope da esquerda para a direita | lista inteira em scope; forward reference |
| W-283 | protocol composition | `P & Q`, sem ordem e com normalização | `P, Q`; `T<[P, Q]>`; composite sempre nomeado |
| W-284 | generic body | verificado uma vez contra constraints; lookup fechado | template com lookup tardio; verificar só após instantiation |
| W-285 | generic inference | depois da forma de call; argumentos, receiver e expected result; solução única | ranking; busca por tipo conforme; body inference |
| W-286 | explicit generic args | type prefix e `const` labeled podem compor com inference; sem `_` | placeholders; lista completa obrigatória |
| W-287 | primary associated type | protocol head declara projection de `Self`; aplicação restringe o witness | generic protocol por conformance; somente body |
| W-288 | associated witness | `alias` explícito; sem inference/default/GAT no design vigente | inferir por method; associated type default |
| W-289 | coherence | conformance no módulo do type ou protocol; escolha única por par | orphan livre; seleção por import |
| W-290 | conditional conformance | `extension<T: P> Nominal<T>: Q`; sem overlap ou specialization | blanket conformance; prioridade |
| W-291 | default witness | somente o módulo do protocol publica; seleção gravada na conformance | extension importada muda witness |
| W-292 | existential compatibility | sem generic method, Self externo ou associated type não ligado | aceitar tudo com traps; banir existential |
| W-293 | existential opening | `any P` não conforma a P e não abre implicitamente | self-conformance; implicit opening |
| W-294 | opaque identity | `some P` preserva um tipo por instantiation; occurrence de parâmetro é independente | existential; união de returns |
| W-295 | generic lowering | monomorphization, shared body e witness são escolhas equivalentes | monomorphization universal; erasure universal |
| W-296 | generic interface | signature, witness requirements e HIR generic por digest/CAS | reparse de source; somente machine code |
| W-297 | generic termination | grafo finito, quotas de instance/depth e cache completo | expansão sem limite; timeout semântico |
| W-298 | generic variance | type constructors invariantes por default | variance inferida; covariance de Array |
| W-299 | bootstrap generics | constraints, primary associated types, coherence e monomorphization; sem any/some | seed sem protocols; runtime dictionaries |
| W-300 | enum subset | enum possui slot primário `cases`; `Enum<[.a, .b]>` | enum base + guard; anonymous union |
| W-301 | subset normalization | conjunto por ordem de declaração; duplicata/empty rejeitados; all vira base | StaticList ordenada na identity |
| W-302 | subset conversion | subset→superset/base implícito; base→subset checked | cast implícito nos dois sentidos |
| W-303 | subset flow | switch usa case-set e flow narrowing elimina checks | exhaustividade sempre pelo enum base |
| W-304 | subset payload/layout | payload preservado; layout público do enum base; tag interno pode sumir | wrapper/tag novo; payload subset |
| W-305 | subset evolution | retorno widening e parâmetro narrowing são major | qualquer mudança minor; variance automática |
| W-306 | subset de error | `throws Enum<[...]>`; throw e catch usam o case-set publicado | error enum inteiro; effect union separado |
| W-307 | planos de introspecção | interface/HIR para tooling; descriptor opt-in no runtime | runtime metadata universal; debug como API |
| W-308 | type identity | `reflect.TypeId` local ao build; sem persistência ou layout | ID estável global; nome como identidade |
| W-309 | metatype | sem `Type<T>`/`T.type`; generic, factory ou enum | metatype universal; dynamic construction |
| W-310 | reflection trigger | conformance explícita a `reflect.Reflectable`; sem annotation | inferir por uso; decorator; registro manual |
| W-311 | reflection visibility | somente interface exportada e properties lógicas | fields privados; backing storage; getter por string |
| W-312 | reflection reachability | witness alcançável mantém descriptor; sem registry global | todos os conformers como roots |
| W-313 | synthesis trigger | conformance no type head; protocol reconhecido por identidade | `@derive`; macro; nome textual |
| W-314 | synthesis scope | Equatable, Hashable, Duplicable e Reflectable em struct/enum; Reflectable em object | qualquer protocol; Display/codec automáticos |
| W-315 | synthesis witness | all-or-none por protocol; constraints explícitas | completar witness parcial; inferir constraints |
| W-316 | rest syntax | último `T...`; zero ou mais; um label inicial | `params`; `*args`; overloads por aridade |
| W-317 | rest shape | conjunto infinito deve ser disjunto de todo overload | fixed vence rest; ranking por tipos |
| W-318 | rest binding | `Arguments<T>` não escapante; mode por elemento | Array alocado obrigatório; tuple runtime |
| W-319 | rest expansion | `each collection` somente no argumento final | `values...`; spread universal; expansão implícita |
| W-320 | rest ownership | value/ref/take; sem `inout`; cleanup por elemento | ownership apagado; inout dinâmico |
| W-321 | C varargs | adapter unsafe tipado ou `c.vaList`; rest W não cruza ABI | mapear rest diretamente; promotions implícitas |
| W-322 | formas type-level adiadas | property path, GAT e heterogeneous packs continuam Pesquisa | incluir no W0; reflection por string |
| W-323 | resolução de enum case | `.case` exige expected enum; `Enum.case` resolve colisão | escolher por import, frequência ou ranking |
| W-324 | sequência e case-set | o head decide: `StagePath<[...]>` preserva ordem; `Enum<[...]>` normaliza conjunto | tratar toda static list como conjunto |
| W-325 | enum e flags | enum representa uma alternativa; simultaneidade usa Set ou tipo de flags separado | enum com semântica AND/OR contextual |
| W-326 | álgebra de case-set | somente na HIR; source nomeia a lista resultante | operadores públicos de union/intersection/difference no design vigente |
| W-327 | dois estados | enum em storage para runtime; argumento `const` de enum para typestate local | typestate universal; enum runtime universal |
| W-328 | argumento const enum | slot primário aceita `.case`; slot normal usa `label: .case` | marker type vazio; string; annotation |
| W-329 | transição typestate | extension especializada + `take fn`; novo tipo no retorno | mudar tipo do binding no lugar; pre/post annotations |
| W-330 | falha consuming | outcome enum devolve cada novo owner; `throws` não restaura owner | rollback implícito; owner escondido no error |
| W-331 | path estático | `StaticList<Enum>` refinada por `const fn`; primeiro edge inválido vira diagnostic | lista sem validação; DSL obrigatória |
| W-332 | estado de service | enum persistido + snapshot revisionado; closed turn por call | `ServiceRef<State>` muda depois da call |
| W-333 | erasure de typestate | envelope enum explícito para collections mistas | `T<?>`; existential implícito; tag escondida |
| W-334 | DSL de transição | sem keywords novas; `StateGraph<E>` declarativa em Pesquisa | `state`/`transition` no design vigente; annotations |
| W-335 | validity e niche | HIR registra bit patterns válidos; niche só representa estados impossíveis | sentinel sem contrato; colapsar estados aninhados |
| W-336 | layout de enum | mapping determinístico; tag explícita é fallback; subset não promete tamanho público | niche obrigatório; wrapper por subset |
| W-337 | low-bit | somente storage interno com alignment real provado e canonicalização nas fronteiras | annotation de source; alignment nominal |
| W-338 | high-bit | profile experimental após negociação completa; ausente do portátil | inferir por CPU; requisito de linguagem |
| W-339 | metadata mutável | count, generation, allocator e deallocator ficam em owner/control block/side table | esconder tudo no pointer |
| W-340 | atomics tagged | operação cobre a palavra inteira; lock-free e ABA exigem provas separadas | atomicidade por associação; generation curta universal |
| W-341 | object header | nenhum header universal; cada ownership/runtime usa metadata necessária | header W em toda allocation |
| W-342 | NaN boxing | rejeitado para `f64` e valor universal; somente pesquisa para container interno | reduzir payload ou range do float |
| W-343 | boundary de layout | FFI, persistência, address exposure e ABI usam forma canônica ou schema | tag interna cruza a fronteira |
| W-344 | fingerprint de representação | inclui validity, target, ABI, allocator, hardening, sanitizer e compiler | fingerprint só por target triple |
| W-345 | pointer compression | handle de arena/heap isolado é classe própria com base e bounds | tratar índice como pointer tagged |
| W-346 | início de async | `await` usa a task atual; `async/spawn let` avaliam captures no parent e executam body no child | Promise implícita; body parcial no parent |
| W-347 | contexto de child | cancellation, deadline, trace, budget e preference; user data/capability são explícitos | task-local map mutável herdado |
| W-348 | domains portáteis | `StandardDomain` fecha defaults; enum payload-free conforme a `ExecutionDomain` declara IDs customizados | toda finalidade vira keyword; string |
| W-349 | domain schema | capabilities, capacity, fallback, affinity e trace identity | thread/pool como identidade semântica |
| W-350 | defaults de execução | `async` herda; `spawn` e parallel group usam parallel default | herdar domain serial e degradar `spawn` |
| W-351 | domain de módulo | nenhum default por módulo; instance/entry/product possui binding | import cria queue/thread |
| W-352 | capacity aninhada | groups no mesmo domain compartilham budget; parent aguardando não retém permit | pool por group; produto dos limits |
| W-353 | liveness paralela | simultaneidade nunca é necessária para correção | spin wait entre children; thread por child |
| W-354 | fairness | eventual sob tasks bounded e jobs non-blocking; sem ordem entre siblings | FIFO scheduler como semântica |
| W-355 | priority e deadline | priority é policy; deadline vira cancellation; syntax local em Pesquisa | `.background` como domain; priority garante prazo |
| W-356 | executor dinâmico | `ExecutionDomainRef` lexical em Pesquisa; admission failure precisa ser explícita; executor custom é runtime unsafe | detached escondida; protocol comum substitui scheduler |
| W-357 | bytes de String | view read-only; mutação somente por operação que preserva UTF-8 | byte mutation com validação posterior; storage exposto |
| W-358 | conversão UTF-8 | view valida; String copia; adoption transfere carrier sem allocation e devolve o mesmo owner no erro | cópia implícita em todas; reuse opcional |
| W-359 | erro UTF-8 | offset, maximal-subpart length e reason estáveis | byte inválido apenas; mensagem livre; decoder-dependent |
| W-360 | reparo UTF-8 | um U+FFFD por maximal subpart; nunca implícito | um por byte; descartar bytes; replacement configurável global |
| W-361 | UTF-8 incremental | até três bytes pendentes; `finish` decide incomplete; offset do stream | validar cada chunk isolado; buffer sem limite |
| W-362 | BOM UTF-8 | core preserva U+FEFF; adapter nomeado aplica policy | remover sempre; preservar sempre em todo protocolo |
| W-363 | índices de texto | origem emprestada, custo visível e uso terminal em edição | integer offset universal; índice persistível |
| W-364 | grapheme owned | `String<(.graphemes.count == 1)>`; sem `Character` | tipo Character universal; Grapheme owned implícito |
| W-365 | interpolação | um `String` de destino + `Display.write`; `display()` é conveniência | builder público; String intermediário por campo; concatenação implícita |
| W-366 | edição Unicode | bundle e digests no semantic fingerprint; índices não persistem | versão do sistema; boundary congelada no valor |
| W-367 | PackagePath | NFC e colisão normalizada rejeitada | nomes distintos por bytes; escolher o primeiro |
| W-368 | semântica de performance | profiles preservam valor, panic, effects, ownership e numeric policy | release muda overflow/float; optimizer como semântica |
| W-369 | facts de otimização | `ProofFacts` na HIR para interval, case-set, length, shape, alignment e alias | annotations de usuário; confiar só no backend |
| W-370 | predicate opaco | invariant válido; optimizer usa somente fatos extraídos e verificados | SMT obrigatório; ignorar todo predicate |
| W-371 | largura interna | operation, accumulator, SIMD lane e storage são escolhas separadas | menor tipo único para tudo; carrier sempre obrigatório |
| W-372 | resultado refinado | expressão provada satisfaz expected refinement sem check; caso geral é fallible | `try` mesmo com prova; narrowing runtime implícito |
| W-373 | storage estreito | somente não escapante e após cost model; boundaries usam carrier | layout menor público por refinement; nunca comprimir |
| W-374 | custo de texto | complexidade por bytes e unidade explícita; caches/ASCII/SIMD invisíveis | `length` O(1) universal; cache obrigatório no layout |
| W-375 | integer `@` | checked semantics; widening fixo; `matmul<R>` muda redução/resultado | wrap; accumulator sempre igual ao elemento |
| W-376 | float `@` | strict default; fast e reproducible por mode explícito | fast global em release; operator dependente do target |
| W-377 | device e fusion | transfer explícita; fusion pode apagar intermediário, não mover device | auto-transfer; toda operação materializa |
| W-378 | PGO | input por digest só orienta optimization | muda const/interface; profile implícito da máquina |
| W-379 | explicação de performance | facts, decisions, estimates, measurements e missed reasons separados | assembly como única explicação; número exato universal |
| W-380 | proof budget | quotas determinísticas; interval/case-set/shape/alias baseline; SMT em Pesquisa | solver sem limite; timeout como resultado semântico |
| W-381 | gate de otimização | benchmark reproduzível + differential oracle + fallback | microbenchmark único; otimização sem profile portátil |
| W-382 | largura de `Int` | `Int`/`UInt` têm 64 bits; `isize`/`usize` seguem address width | Int segue target; literal default `i32`; BigInt default |
| W-383 | representação integer | widths fixas; signed two's complement; Bool distinto | signed dependente do target; Bool como integer |
| W-384 | token numérico | decimal/binário/octal/hex; exponent decimal; suffix após `_` | suffix colado; trailing dot; hex float |
| W-385 | valor do literal | magnitude/rational exato até expected type; uma materialização | truncar no lexer; converter decimal por f64 intermediário |
| W-386 | defaults de literal | integer `Int`; decimal `f64`; expected type prevalece quando válido | i32 default; BigInt/Decimal default |
| W-387 | tipagem binária | identidade ou uma conversão segura para um tipo operando; sem terceiro tipo | promoções C; ranking de common type |
| W-388 | conversão implícita | total, exata, única e sem authority oculta; refinement pode provar | cast implícito narrowing; exigir todo cast |
| W-389 | conversão explícita | `exactly`, `rounding`, `saturating`, `truncatingBits` e bits nomeados | um cast com policy dependente do par |
| W-390 | overflow integer | operators checked em todo profile; const vira diagnostic | wrap em release; undefined behavior |
| W-391 | divisão integer | zero e min/-1 causam panic; quotient toward zero; Euclidean nomeado | floor universal; resultado Option implícito |
| W-392 | shift | count `UInt`; bound e perda à esquerda causam panic; bit policies nomeadas | mask do count; regras C; wrap silencioso |
| W-393 | float baseline | f32/f64 IEEE strict, nearest-even, subnormal e sem FMA implícito | fast-math em release; flush-to-zero default |
| W-394 | float equality | comparação IEEE parcial; `TotalFloat` para key e ordem total | float conforma aos protocols totais; bit equality como `==` |
| W-395 | modes float | strict default; fast e reproducible explícitos e versionados | flag global muda semântica; reproducible sem algoritmo |
| W-396 | numeric T2 | BigInt/UInt, FixedDecimal, Rational e Complex com custo explícito | número universal; Decimal como Money |
| W-397 | ML storage | f16/bf16 sem scalar operators e com tensor accumulator f32; Quantized separa storage/expressed | aritmética f16 implícita; float8 core |
| W-398 | range | intervalo; quatro closures; reversed vazio; stride para direção/step | range como collection; range descendente implícito |
| W-399 | superfície de pinning | `try pin take value`; `pin` é fallible e separado de `take` | `Pinned.make`; `take<.pin>`; modifier no binding |
| W-400 | saída de pinning | sem `unpin` no design vigente; drop in-place; `intoValue` com proof token em Pesquisa | unpin seguro irrestrito; unpin keyword unsafe |
| W-401 | endian numérico | valor independe de endian; bytes exigem `.little`, `.big` ou `.native` | ordem implícita de persistência; reinterpret seguro |
| W-402 | reals alternativos | Posit, Unum e decimal float como Pesquisa T2; f32/f64 ficam baseline | número universal novo; trocar IEEE sem oracle/hardware |
| W-403 | construção de String | reserva e mutation pertencem a `String`; sem `StringBuilder` público | builder obrigatório; concatenação repetida |
| W-404 | view genérica | `view T` é access mode vigente; sem família pública `XView` | `Slice<T>`/`Span<T>`; `Readonly<T>` profundo; usar somente `ref` |
| W-405 | placement | sem annotation; local síncrono fixo que não escapa não usa allocator geral | annotation stack/heap; boxing por register pressure |
| W-406 | fato de alocação | HIR/interface registram obrigação; `w explain` e gate usam call graph | effect escrito em cada função; allocation invisível ao tooling |
| W-407 | alocação em region | somente call com `using: region`; bloco não captura todos os locais | placement lexical implícito; allocator global da região |
| W-408 | escape de arena | inline independente pode sair; storage dependente exige consuming `rehome` | copiar sempre no return; escape unchecked; adoção presumida |
| W-409 | arena e tasks | Arena move-only e não shareable; child paralelo recebe arena filha exclusiva | arena monotônica concorrente default; proibir todo child |
| W-410 | budget de arena | cobra span alinhado, padding, growth retido e drop metadata; host mede resident separado | cobrar somente live payload; usar resident bytes como semântica |
| W-411 | origem | owner preserva allocator instance e deallocator; zero-size não aloca; family não mistura | `free` universal; origem em low bits |
| W-412 | allocator profiles | system baseline; mimalloc e secure por recipe/benchmark; fixed sem OS allocation | override global obrigatório; allocator escolhido por import |
| W-413 | allocation failure | cases estáveis, strong guarantee em `try*` e budget distinto de OOM | tamanho livre global; falha parcial; uma exception universal |
| W-414 | inicialização de storage | safe typed allocation nunca expõe uninitialized; zero é operação/policy explícita | calloc semântico universal; bytes residuais legíveis |
| W-415 | criação shared | intrinsic fallible `share`, allocator explícito opcional e sem promotion implícita | constructor wrapper; expected type aloca; shared universal |
| W-416 | cópia shared | handles são move-first; `copy` torna retain visível; optimizer pode elidir | shared atende a Copy implícito; retain escondido em assignment |
| W-417 | `ref` versus `view` | `ref` preserva place completo; `view` descreve projeção sem owner/capacity | tratar ambos como pointer + count; view nominal por tipo |
| W-418 | mutation de view | binding/parameter `inout view T`; extent fixo e sem resize; String/CString permanecem read-only | `MutableXView`; mutation por view read-only; copy-on-write |
| W-419 | materialização | `materialize()` normal e `tryMaterialize(using:)` fallible produzem `T` | constructor por família; adoção borrowed; conversão implícita |
| W-420 | escopo de view | core families e Tensor; custom type expõe core view ou borrow nominal | protocol inventa provenance; view automática de todo tipo |
| W-421 | ABI de view | descriptor W por família; C usa pointer + count somente quando contígua | descriptor universal; layout W cruza FFI |
| W-422 | lifetime de view | provenance inferida; `await` exige owner estável; child estruturado termina antes do owner | lifetime annotation; view mantém owner vivo; escape detached |
| W-423 | read-only e imutabilidade | `ref` é acesso read-only; `view` é projeção; imutabilidade profunda é fato inferido sem syntax | `Readonly<T>` universal; modifier `immutable`; `let` promete grafo congelado |
| W-424 | mobilidade pública | facts intrínsecos `transferable`/`shareable`; sem marker protocols | `Send`/`Sync`; `Sendable`; check runtime |
| W-425 | transferência | owner/acesso exclusivo, fields, allocator, cleanup e affinity; origem perde acesso | exigir synchronization para move único; copiar sempre |
| W-426 | sharing | storage vivo e reads sem race; interior mutation precisa de mecanismo verificado | exigir imutabilidade profunda; aceitar todo `ref` |
| W-427 | constraint de mobilidade | `T<(.transferable)>` e `T<(.shareable)>`; omitida é inferida | `T: Send`; `<mobility: ...>`; annotation na declaração |
| W-428 | views e mobilidade | descriptor não prova nada; owner, provenance e lifetime satisfazem o capture | view é Send/Sync por pointer + count; proibir toda view |
| W-429 | FFI mobility | local por default; fato trusted exige adapter/digest e boundary unsafe ainda em Pesquisa | raw pointer deriva facts; assertion segura do usuário |
| W-430 | representação W0 de String | literal/static + buffer flat único com pointer/count/reserva/origin | SSO e COW no bootstrap; rope; runtime Unicode obrigatório |
| W-431 | COW de String | fora da baseline; optimizer exige efeitos de allocation e cleanup não observáveis | refcount em toda String; COW como contrato; proibir otimização |
| W-432 | reserva de String | mínimo total por bytes; exact capacity não é pública; `tryReserve` tem strong guarantee | bytes adicionais; growth fixo na linguagem; capacity property |
| W-433 | mutation de String | append/replace recebem view válida; source do mesmo owner é erro; índices são invalidados | mutable byte view; temporary de alias implícito; byte offsets unchecked |
| W-434 | esvaziar String | `clear` mantém storage; `reset` libera; `takeAll` transfere conteúdo | Boolean `keepingCapacity`; um método ambíguo; builder separado |
| W-435 | String e Bytes | carrier T0 compatível; adoption e `intoBytes` consomem sem allocation geral | layout público igual; cópia obrigatória; cast implícito |
| W-436 | caches de texto | reads não alocam nem mutam; summaries eager permitidos; índice alocante usa tipo próprio | cache lazy invisível; owner muta por read; grapheme ordinal O(1) |
| W-437 | String especializada | SSO invisível medido; `InlineString`, Rope, IndexedText e tree string são tipos próprios | threshold público de SSO; uma String universal adaptativa |
| W-438 | ponteiro textual | somente borrow scoped; move/mutation bloqueados; persistência usa CString/Bytes/Pinned adapter | pointer estável de String; NUL obrigatório; raw pointer safe |
| W-439 | String no self-host | flat UTF-8, bytes, append/reserve, views, conversions e ownership; Unicode avançado não bloqueia SH0 | grapheme/locale antes do parser; C runtime de String permanente |
| W-440 | data race | bytes sobrepostos, concorrência, write e ausência de happens-before; safe W rejeita | race com resultado definido; check somente em runtime |
| W-441 | happens-before | task start/join, channel em W-467, service turn, unlock/lock e release/acquire | thread start/join somente; cancel publica user state |
| W-442 | storage atomic | `var atomic value: T` baixa para `Atomic<T>`; acesso comum seq-cst | `Atomic<T>` sempre explícito; behavior Atomic; todo var atomic |
| W-443 | atomic value | fato intrínseco fechado para Bool, integers e enum sem payload | protocol user-defined; qualquer Copy; floats e structs na baseline |
| W-444 | order | `<.order>` estática; load/store/update usam enum subsets; default `.sequential` | argumento runtime; suffix por método; relaxed default |
| W-445 | compare-exchange | result enum; success/failure estáticas e válidas; weak é explícita | Boolean; expected inout; combinação inválida em runtime |
| W-446 | aritmética atômica | policy checked normal; wrapping/saturating/fetch nomeados | wrap do hardware implícito; closure update com retries ocultos |
| W-447 | borrow atômico | `ref` obtém Atomic; acesso ao payload somente com exclusividade ou consumo | `ref T` comum; misturar views atômicas e não atômicas |
| W-448 | lock-free | não é implícito; const `isLockFree` e contrato `lockFree: true` | garantir toda largura; runtime query sem target fixo |
| W-449 | ABI atômica | layout W opaco; C usa wrapper e metadata | layout igual a C `_Atomic`; layout estável universal |
| W-450 | mutex síncrono | `Mutex.withLock` scoped e marcado blocking; sem guard público na baseline | lock/unlock manual; behavior Locked; poisoning |
| W-451 | mutex assíncrono | aquisição suspende; closure protegida é sync e cancel-safe | guard cruza await; mutex síncrono no worker cooperativo |
| W-452 | RwLock e RCU | tipos T1 especializados em Pesquisa; service/channel/snapshot lideram state maior | policy automática por property; RCU default universal |
| W-453 | contenção | explanation record mostra lowering, lock-free e waits; cache isolation em Pesquisa | prometer performance por `atomic`; padding universal |
| W-454 | stream assíncrono | `Stream<Item, Failure>` é protocol pull, single-pass e com cursor mutável | sequence + iterator obrigatórios; push callback; generator como semântica |
| W-455 | término de stream | `.none` ou primeiro error são terminais; `Failure = Never` remove `try` | continuar depois de throw; sentinel; close como item |
| W-456 | iteração assíncrona | `for try await` baixa para `next()`; `for await` quando nonthrowing | `await stream` lê tudo; callback; loop especial por tipo |
| W-457 | item borrowed de stream | `Stream<view T, E>` registra o stream como origem e impede outro `next` conflitante | família `TView`; view transferable; proibir todo item borrowed |
| W-458 | topologia de channel | MPSC bounded com endpoints separados | bidirecional copiável; MPMC default; unbounded |
| W-459 | endpoint de channel | `Channel<T><.send>` shareable move-first e `<.receive>` único move-only | `Sender<T>`/`Receiver<T>` nominais; direção dinâmica |
| W-460 | payload de channel | `T<(.transferable)>` owned; borrow e `view` são rejeitados | cópia implícita; raw pointer; lifetime runtime |
| W-461 | falha de envio | `ChannelSendError<T>` devolve owner; `send` usa subset `.closed` | panic; Boolean; perder item em erro |
| W-462 | capacity | obrigatória; zero é rendezvous; positiva limita itens + permits; sem unbounded no design vigente | default zero; hint elástico; fila ilimitada |
| W-463 | permit | reserva linear sem item; drop libera; close gracioso honra permit aceito | construir item antes de esperar sempre; reservation invisível |
| W-464 | cancellation de channel | commit linear; antes dele não envia, depois dele receiver possui; waiter sai da fila | resultado ambíguo; rollback do item recebido |
| W-465 | ordering de channel | FIFO de admission, ordem por sender e sem total order concorrente; `trySend` não ultrapassa | ordem global determinística; fairness não especificada |
| W-466 | close de channel | último sender ou receiver.close faz drain; drop do receiver aborta; sem close global no sender | close por qualquer producer; sentinel; panic em close duplicado |
| W-467 | happens-before de channel | send→receive, slot liberado→send admitido e close→fim observado | somente ownership; fence manual pelo usuário |
| W-468 | buffering de stream | nenhum prefetch default; adapter bounded com scope estruturado | watermark na assinatura; buffer ilimitado; producer detached |
| W-469 | `yield` | adiado até IR provar borrow, cleanup, erro, cancellation e capacity | generator define semântica; callback oculto |
| W-470 | outras topologias | `WorkQueue`, `Broadcast`, `Watch` e weighted channel permanecem tipos pesquisados | um `Channel<mode: ...>` muda loss e fan-out |
| W-471 | implementation de channel | target escolhe ring, segmentos, mutex ou atomics; lock-free não é contrato | algoritmo único no ABI; tagged pointer obrigatório |
| W-472 | accounting de channel | lens separa storage, itens, payload desconhecido, waiters, permits e watermark medido | capacity promete bytes transitivos; número único exato |
| W-473 | byte I/O | `ByteSource<Failure>` e `ByteSink<Failure>` async-first; cursor lógico no source | Reader/Writer nominal por backend; prefixo `Async`; interface sync única |
| W-474 | destino de read | append em `inout Bytes`; initialized count e spare privados; sem `ReadBuffer` público | `MaybeUninit` safe; `read(into: inout view Bytes)` genérico; allocation escondida inevitável |
| W-475 | resultado de read | `.data(positive)` ou `.end`; EOF é estável e progress vence EOF simultâneo | zero significa EOF; tuple count/error; EOF como error |
| W-476 | progress e error | progress retorna agora e error simultâneo fica latched para a próxima call | lançar depois de mutar sem informar; perder progress; outcome com estados impossíveis |
| W-477 | resultado de write | `.complete` ou `.partial(positive)`; `writeAll` informa prefixo já committed | Boolean; assumir write integral; rollback fictício |
| W-478 | cancellation de I/O | cancellation disputa com completion e só libera borrow depois do drain | liberar buffer no pedido; fingir zero progress; matar worker thread |
| W-479 | blocking I/O | interfaces separadas e adapter explícito em executor bounded | blocking invisível no worker cooperativo; pool ilimitado; uma interface condicional |
| W-480 | rights de arquivo | `File<[.read, ...]>` usa static list fechada e mantém checks dinâmicos do host | flags somente runtime; capability implica permissão de path; annotations |
| W-481 | offsets de arquivo | I/O seekable é posicional por default; `.end` observa o offset; shared File exige offset explícito | cursor compartilhado default; EOF latched no handle posicional; metadata.size + write |
| W-482 | cursor sequencial | adapter opaque `some ByteSource<IoError>` possui File + offset; sem classe utilitária pública | `FileReader` público; cursor dentro de todo File; offset global |
| W-483 | sockets | TCP pode virar halves únicos; UDP preserva datagrams em protocol separado | duas reads concorrentes; UDP como byte stream; message boundary implícita |
| W-484 | error de I/O | kind e operation portáteis; cause nativa opaca; task cancellation não é IoError | errno universal; wouldBlock em async; retriable Boolean |
| W-485 | finish e durability | protocols base não exigem close/flush; tipos concretos nomeiam finish, sync e half-close | async destructor; drop durável; flush universal |
| W-486 | adapters de stream | chunks borrowed/owned, lines e read-to-end exigem limites explícitos | buffer ilimitado; item borrowed transferable; framing invisível |
| W-487 | backend de I/O | target escolhe readiness, completion, blocking bounded ou immediate sem mudar source | backend na syntax; um algoritmo universal; thread por operação |
| W-488 | lifetime de buffer I/O | pinning é interno; handles, callbacks e borrows vivem até completion drain | `pin` obrigatório no caller; raw pointer escapa; cancellation encerra lifetime cedo |
| W-489 | famílias de I/O especializado | gather, scatter e transfer possuem decisões separadas; nenhuma otimização muda bytes ou progress | `readv`/`sendfile` invisível; mapa mutável universal; promessa sem target |
| W-490 | observabilidade de I/O | explanation record e trace mostram backend, progress, waits e cancellation race | backend opaco sem diagnóstico; log muda semântica; timestamps como ordering |
| W-491 | trabalho runtime-owned | `SupervisorRef` é owner explícito; `Task` permanece lexical | drop destaca; `spawn<owner: ...>`; task global |
| W-492 | operação supervisionada | descriptor fixa função e versão; key, input e bindings explícitos | closure arbitrária; capture de state; body trocado em work ativo |
| W-493 | admission de work | roots, running, admission queue e bytes são bounded; commit transfere input; unknown outcome reconcilia por key | fila ilimitada; input perdido; start fire-and-forget |
| W-494 | identity de work | supervisor + key completa + incarnation; attempt separado; hash nunca é identity | PID/pointer; hash persistente; nome solto |
| W-495 | observação de work | state, progress, cancellation e suspension separados; snapshot revisionado; retention bounded | event list ilimitada; ref para frame; polling sem revision |
| W-496 | rights de work | SupervisorRef → WorkKeyRef → WorkRef; observe, cancel e signal atenuam authority | Boolean runtime; todo observer controla; ID concede authority |
| W-497 | outcome de work | success, `E`, canceled e boundary separados | cancel em `E`; panic capturável como application error; ausência vira success |
| W-498 | restart de work | `.never` default; retry bounded exige step/effect ID/idempotência | retry eterno; reiniciar todo async body; retry mutante implícito |
| W-499 | workflow durável | replay desde o começo usa points e outcomes explícitos; sem persistir frame, pointer, borrow ou capability | serializar stack automaticamente; Durable Object universal |
| W-500 | binding de service | `ServiceBinding<P>` e `ServiceFamily<P, K>` const e link-checked | lookup normal por string; import cria instance; registry global |
| W-501 | product runtime graph | `runtimeGraphs` fixa providers, imports, exports, injection e envelope; compiler deriva requirements | manifest executável; reflection encontra implementação; limite só no host |
| W-502 | deployment | plano e lock data-only ligam units prebuilt por digest; só placement, binding permitido e redução | rebuild por ambiente; config invisível; deployment troca packing ou semântica |
| W-503 | rolling work | root fixa operation/schema; drain ou migration explícita | hot-swap do body ativo; retomar com versão ausente |
| W-504 | after-response | adapter host bounded para cleanup curto; trabalho confiável usa supervisor/queue/workflow | `waitUntil` sem prazo; Promise solta; resposta mantém process vivo |
| W-505 | identity keyed de service | `ServiceIdentity<K>` read-only e injetada; descriptor exige o mesmo key type | Context global; string key; inferir pelo primeiro argumento |
| W-506 | dedup de work | outcome e tombstone têm budgets separados; key só é reutilizada em nova incarnation | outcome eterno; expiração permite duplicação silenciosa; key global única |
| W-507 | completion versus cancellation | completion committed entrega o valor; cancellation fica pendente; unknown outcome permanece distinto | descartar valor committed; injetar cancel entre statements; rollback presumido |
| W-508 | entry anônimo | `entry(handler)` fornece descriptor default e base local para entries nomeados | repetir bindings; `entry defaults`; herança entre módulos |
| W-509 | shorthand de entry | `entry Name(handler)` liga o slot default único do host profile | escrever `process.main`; inferir pelo nome da função |
| W-510 | seleção de entry | product iniciado por host escolhe descriptor; library usa export e service-only unit usa provider no index | entry obrigatório para todo artifact; seleção runtime por nome; registry global |
| W-511 | aplicação multimodo | um `process.main` escolhe CLI/TUI/server; slots de host continuam distintos | vários mains no mesmo payload; OS chama `http.fetch` |
| W-512 | identidade de target | architecture-vendor-system-ABI + CPU/features/sysroot separados | string livre; target igual a OS; backend implica suporte |
| W-513 | host profile | slots, capabilities e lifecycle versionados, separados do target | APIs condicionais por `#ifdef`; target concede capabilities |
| W-514 | product kind | `kind` seleciona schema fechado; executable, libraries, component, firmware, device bundle, test, benchmark e tool tipada | record de fields opcionais; executable universal; kind inferido pelo entry |
| W-515 | matriz de build | cada product/target/profile gera recipe e digest próprios; index agrega resultados | hash único entre architectures; matrix muda payload |
| W-516 | produto de referência | Última Luz é especificação executável, regressão e benchmark do W | exemplo descartável; snippets independentes como oracle principal |
| W-517 | nanoservice | service é fronteira lógica; runtime pode co-localizar sem apagar effects | processo por service; call remota transparente |
| W-518 | accelerator | device target gera kernels/objects ligados por host product | device como processo geral; offload implícito |
| W-519 | benchmark externo | sete source oracles, profile versionado e validators precedem medição; ranking não é semântica | otimizar para placar sem oracle; chamar source de resultado; prometer posição |
| W-520 | module set | `.fileStem` expande paths de forma determinística e grava a lista no lock | descoberta livre do diretório; `module` em cada source |
| W-521 | std em W | contratos públicos são source W; handles e operações intrínsecas têm fronteira explícita | std toda no compiler; wrappers utilitários por operação |
| W-522 | fechamento do runtime graph | cada requirement recebe provider, supervisor, host capability ou import declarado | lookup por string; import implícito; provider descoberto por reflection |
| W-523 | interface de graph aberto | imports e exports tipados entram na interface do artifact; executable aberto exige deployment | esconder import no Context; rede global; executable presume provider |
| W-524 | packing | partição de providers em units ocorre no build e entra na recipe | extrair service de executable durante deploy; processo por módulo |
| W-525 | crossing de unit | somente service ABI cruza unit; preserva async, schema, quotas, failures e trace | call normal remota; borrow ou mutable state entre units |
| W-526 | deployment lock | source plan resolve products e releases para artifact, unit e adapter digests | tag mutável em production; build durante apply; secret dentro do lock |
| W-527 | WASI baseline | `wasm32-wasip3` usa Component Model native async; `wasm32-wasip2` é compatibilidade | polling 0.2 como semântica W; Wasm implica DOM |
| W-528 | unit root | entry unit é explícita; service-only unit publica provider no artifact index; toda unit possui root | unit vazia; initializer implícito; entry sintético pelo nome |
| W-529 | interface privada de unit | packer deriva endpoints privados e fixos; deployment só roteia a edge | tornar provider público; religar edge interna no deployment |
| W-530 | tuple com labels | todos os elementos têm label ou nenhum; labels pertencem ao tipo; unitário exige vírgula | mistura de labels; labels decorativos; function arguments viram tuple; struct anônimo universal |
| W-531 | modelo HTTP | `std.http` representa semântica de mensagem, independente de HTTP/1.1, HTTP/2 ou HTTP/3 | frames no handler; Fetch API copiada integralmente; version decide domínio |
| W-532 | ownership de request | `Request` move-only transfere body e só permite um consumer | clone implícito; body copiável; stream ambiental |
| W-533 | materialização HTTP | decode, bytes e text são async e exigem limite; stream preserva backpressure | ler body inteiro sem limite; decode síncrono; buffer oculto |
| W-534 | headers HTTP | tokens e controls são validados; nomes são ASCII case-insensitive; Bytes, repetição e ordem são preservados | Map<String, String>; normalizar value Unicode; juntar Set-Cookie |
| W-535 | response streaming | retorno transfere owner ao pump runtime-owned; entrega ao client não é confirmada | task solta; retorno confirma socket; segundo status após commit |
| W-536 | admission HTTP | product/call fixa requests, fila, bytes, connections e message limits | defaults ilimitados; task antes de admission; rate limit de negócio implícito |
| W-537 | host HTTP | native `serve` e worker slot usam o mesmo handler e oracle | API por protocolo de transporte; OS chama fetch; handler especial de benchmark |
| W-538 | `http.Context` | registries tipados expõem somente bindings e capabilities declaradas | mapa de env; singleton global; lookup livre por string |
| W-539 | SQL estático | descriptors const usam bind nomeado e identity derivada; sem `name` textual repetido | interpolação; SQL mutável; query sem dialect; identity manual |
| W-540 | row database | tuple tipado ou decoder explícito; schema bundle aumenta prova | preencher struct por reflection; `Any` row; column conversion silenciosa |
| W-541 | pool e pipeline | admission bounded; query/execute-many preservam um statement, outcome e ordem por input | pool ilimitado; combinar SELECTs; deduplicar queries; ordem de result variável |
| W-542 | transaction | closure recebe borrow não escapante; output sai após commit confirmado; unknown commit é distinto | transaction escapa; retry automático; perda de conexão significa rollback |
| W-543 | cache local | limita entries, loaders e fila; devolve owned duplicate; bytes ficam no envelope do product | Map global; fila livre; view sobre entry; cache como source of truth |
| W-544 | cache load | um loader por key; waiter cancela só a espera; admission e commit são explícitos | task detached; waiter cancela loader; errors cached por default; loaders iguais |
| W-545 | cache remoto | usa `ServiceRef` async separado; deployment não converte cache local em rede | API sync location-transparent; timeout invisível; remote fallback |
| W-546 | limits de product | schema do host/capability fixa envelope máximo; deployment somente reduz | config aumenta authority; limite só operacional; defaults sem artifact contract |
| W-547 | configuração de host | schema fechado do profile entra na recipe; deployment somente reduz policies permitidas | mapa de opções livre; config concede capability; proxy oculto corrige semântica |
| W-548 | parâmetro de chamada `const` | `name: const T` exige ConstRepresentable conhecido no call site; ABI pode apagar | `comptime` em cada call; runtime descriptor na API estática; monomorphization obrigatória |
| W-549 | gather write | `writeMany(_ sources: view Bytes...)` confirma prefixo da concatenação lógica; default usa um `write` | `IoSlice` público; concatenação; exigir backend vetorizado; erro por excesso de segments |
| W-550 | scatter read | permanece em Pesquisa; baseline acrescenta a um único `Bytes` owner | `inout view Bytes...`; `ReadBatch`; mutable buffer universal |
| W-551 | transferência zero-copy | permanece em Pesquisa e exige operação, fallback e oracle por host explícitos | lowering invisível de `write`; ausência de fallback; promessa universal de zero-copy |
| W-552 | ativação de workflow | usar `work.step`, `sleep` ou `wait` ativa análise replayable; sem annotation `durable` | keyword nova; replay sem análise; persistência automática de frame |
| W-553 | identidade de point | `WorkId` + kind + valor canônico fechado; duplicata ou input divergente é history mismatch | string livre; ordem de source; contador implícito |
| W-554 | identidade de efeito | `EffectId` é estável por point e operation; step attempt e root attempt são ordinais separados | key nova por retry; hash de payload como identity |
| W-555 | effect policy | `.repeatable`, `.idempotent`, `.transactional` ou `.atMostOnce`; at-most-once é default | exactly-once universal; retry automático de efeito desconhecido |
| W-556 | retry de step | policy const e bounded seleciona application errors, backoff e timeout | defaults infinitos; jitter oculto; Boolean retriable no error |
| W-557 | commit de step | input precede dispatch; outcome e progress precedem visibilidade ao workflow | devolver output antes do journal; converter falha de storage para application error |
| W-558 | timer durável | `sleep` registra deadline e não mantém task frame; clock direto fora de step é rejeitado | `Task.sleep` persistido; recalcular deadline em replay |
| W-559 | evento de workflow | binding tipado, `EventId`, inbox bounded e `send`/`trySend` com deduplication | event string sem schema; payload global; fila ilimitada |
| W-560 | corrida de wait | commit escolhe evento, timeout ou cancelamento; evento posterior permanece disponível | timestamp do sender decide; timeout descarta evento |
| W-561 | scheduling durável | points sequenciais; paralelismo estruturado pode ocorrer dentro do step | journal dependente do scheduler; fan-out implícito |
| W-562 | versão de workflow | root fixa operation, point/event schemas e adapter ABI; migration é explícita | hot-swap do history; worker antigo executa versão nova |
| W-563 | adapter de workflow | contrato portátil; SQLite profile é explícito e memory serve ao oracle volátil | SQLite universal; deployment reduz garantia; storage oculto |
| W-564 | confidencialidade de journal | artifact fixa mínimo e retention; payload não entra em diagnostics; adapter prova storage | plaintext implícito; secret handle serializado; deployment reduz proteção |
| W-565 | workspace | `workspace.w` data-only lista members exatos; não cria identity publicável | discovery recursivo; workspace executável; package e workspace fundidos |
| W-566 | lock de workspace | um lock compartilhado com contexts por root, usage e target | lock por member sem visão global; um grafo único para todos os targets |
| W-567 | member local | identity + version compatíveis selecionam member e tree digest; mismatch falha | fallback silencioso para registry; import por path |
| W-568 | usage de dependência | `.product`, `.build`, `.test` e `.benchmark` fecham reachability e target role | uma lista universal; dev dependency entra no payload |
| W-569 | feature | seleção aditiva de grafo, sem default implícito ou conditional source na v0 | `if feature` livre; optional dependency cria feature; negation |
| W-570 | união de feature | união por resolution realm; realms distintos não vazam | união global do workspace; uma build por edge |
| W-571 | source de dependency | registry ou Git por commit; path somente em workspace; lock fixa tree externo e source set local | branch/tag no build; URL no import; binary URL como identity |
| W-572 | patch | somente workspace root, mesma identity/version, sempre visível e não publicável | dependency troca por fork invisível; patch transitivo; release patched |
| W-573 | build tool W | `.tool` usa `build-transform@1` e bindings tipados | install script; shell fragment; process com filesystem ambiental |
| W-574 | action hermética | tool, inputs, outputs, execution target, budgets e capabilities formam a key; commit vai ao CAS | output no source tree; host implícito; cache por path/mtime |
| W-575 | namespace de dependency | package declara raiz canônica; alias local substitui a raiz no import sem mudar identity | package name dentro do import; URL import; namespace global sem alias |
| W-576 | package identity | `authority` declarada + scoped ASCII name; revision, mirror e alias local não mudam identity | nome global sem authority; source concede identity; Unicode no path canônico |
| W-577 | source snapshot | `publish.files` allowlist usa modules e PackagePath; VCS ignore não altera release | publicar tudo; usar `.gitignore`; registry acrescenta arquivos |
| W-578 | licença de package | SPDX expression + files; proprietary e no-assertion são states distintos | string livre; ausência implica licença; metadata externa substitui texto |
| W-579 | lock, recipe e artifact | lock fixa resolução; recipe fixa inputs; artifact record liga outputs | lock contém resultados; recipe autorreferente; provenance decide resolução |

Uma revisão pode responder por ID. Uma mudança deve atualizar o exemplo, a
grammar, o formatter, o corpus e a seção semântica correspondente.
