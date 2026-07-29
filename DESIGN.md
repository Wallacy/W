# Design integral da linguagem W

> **Status:** **Candidato experimental** · 29 de julho de 2026

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
7.5. O ensaio correspondente fica em `examples/restaurant`.

## 1. Limite da alegação

**Exemplo:** a comparação entre `<unit>` e `[unit]` precisa medir correção antes
de medir preferência.

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

**Exemplo:** um build release não pode aceitar overflow que o mesmo programa
rejeita em debug.

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

## 3. Contratos estáticos e orçamento de símbolos

Cada delimitador mantém uma função mental principal:

| Forma | Função principal na DB2 | Exemplos |
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

`where` e `on` não são keywords da DB2. `where` permanece em comparação com
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

Exemplos da forma líder:

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

A forma curta é **Líder DB2**. `value` continua disponível para
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

`ExecutionDomain` e outros cases de profile são enums fechados. `C` e `Rust`
resolvem para uma `LanguageAdapterId` fixada no lock. A HIR guarda identity,
version e digest do adapter. Ela não guarda uma string livre.

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
forma líder. `spawn<domain: .compute>` permanece **Alternativa** para corpus e
diagnostics.

### 3.3 Refinement, composição e layout

As operações abaixo permanecem distintas:

| Intenção | Forma líder | Muda storage? |
|---|---|---:|
| restringir valores | `T<(predicate)>` | não |
| criar identidade nominal | `type X = T` | não por default |
| criar sinônimo | `alias X = T` | não |
| adicionar methods ou conformance | `extension X { ... }` | não |
| adicionar fields | `struct X { value: T ... }` | sim |
| representar um de vários cases | `enum X { a(A) b(B) }` | conforme layout do enum |
| sobrepor storage C | `foreign c union` ou wrapper `unsafe` | sim e explícito |

Uma extension nunca adiciona storage. Herança de implementação não entra na
DB2. Um safe sum usa `enum`. Uma C union sobrepõe bytes e pertence à fronteira
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

**Líder DB2:** usar `T<(...)>`, `async/spawn<.domain>`, `fn<Language>` e unit
literal sem label.

**Alternativa:** preservar `where` e receiver implícito no corpus comparativo.
Slots primários nomeados continuam aceitos para comparação e diagnostics. O
formatter emite a forma curta quando o schema não é ambíguo.

**Rejeitado por enquanto:** `spawn on .domain`. O corpus preserva a forma para
medir leitura e migração. O parser DB2 não a aceita.

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

O exemplo abaixo mostra a forma líder. Ele não tenta mostrar toda a biblioteca.

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
  var atomic completed: u64 = 0

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

entry LastLight {
  process.main = run
  http.fetch = fetch
}
```

O formatter mantém uma assinatura em uma linha quando ela cabe em 120 colunas.
O exemplo `score` quebra porque a forma completa ultrapassa esse limite.

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
Semicolon não separa linhas de matriz na DB2.

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
precisa fechar pureza, custo, exhaustividade, captures e diagnostics. A DB2 usa
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
Uma atualização recompila os dependentes. A DB2 não promete substituir uma
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

Enums da DB2 são fechados. Um `switch` exaustivo recebe um diagnostic quando a
versão adiciona um case. W não inclui uma forma `nonexhaustive` na DB2.

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

A HIR registra o function type de todo callable. A DB2 infere o tipo de closures
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
alternativa proíbe todo overload e exige nomes distintos. A forma líder permite
APIs naturais e mantém a seleção local, finita e reproduzível.

Parâmetros rest homogêneos entram na DB2. A forma `T...` aceita zero ou mais
argumentos do mesmo tipo. A seção 8.9.5 define labels, ownership, overlap e
lowering. Type packs heterogêneos continuam em **Pesquisa**.

### 7.3 Parâmetros e ownership

```w
fn inspect(value: ref Value)
fn edit(value: inout Value)
fn store(value: take Value)
fn transform(value: Value): Result
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

Quando o operand é `ref T`, `copy` materializa um `T` owned. Ele não copia o
borrow:

```w
guard let ref recipe = recipes[course] else panic("recipe invariant failed")
let ownedRecipe: Recipe = copy recipe
```

Uma implementação de `Duplicable` é nonthrowing sob a policy normal de OOM. Ela
cria um valor semanticamente independente. COW pode otimizar a operação, mas
mutar um resultado nunca altera o outro:

```w
let original = "Last Light"
var duplicate = copy original
duplicate.append("!")
expect original == "Last Light"
```

Partial move exige destructuring. A DB2 não permite mover um field e continuar a
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

A DB2 não mistura `copy`, `ref`, `inout` e `take` dentro do mesmo pattern. O
programa usa projeções de field quando precisa de modos diferentes.

Somente stored fields visíveis no ponto de uso participam do pattern. Um struct
encapsulado aceita destructuring no módulo que controla seu storage. `object` e
`service` não aceitam destructuring. Essas categorias preservam identidade,
invariantes e cleanup atrás da API nominal.

Um struct com `deinit` customizado não aceita destructuring owned. O pattern
emprestado continua válido dentro do módulo. Essa regra impede que um pattern
ignore ou execute duas vezes o cleanup customizado.

**Alternativa:** usar `{field}` como record pattern. Outra alternativa usa
posições sem nomes. A forma líder reutiliza `Type(...)`, mantém labels nominais
e evita reservar `{}` para um segundo modelo de record.

### 7.5 Valores callable e closures

A DB2 separa três formas. A separação torna capture, erasure e ABI observáveis:

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

Function types são invariantes na DB2. Parameter types, ownership, return,
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

`(args) => body` é a única forma de closure da DB2. `{ args in body }` e
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
`FnOnce`. A forma líder reutiliza `some`, `any`, `mut` e `take`.

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

Herança de implementação não entra na DB2. Composição, protocols e funções
livres são a baseline.

`some P` em um parâmetro é shorthand para um generic anônimo. `some P` em um
return type oculta um tipo concreto único:

```w
fn render(value: some Displayable): String
// Equivale a: fn render<T: Displayable>(value: T): String

fn activePolicy(): some PricingPolicy {
  return StandardPricing()
}
```

Essa regra também atende `some fn(...)`. Ela preserva specialization sem exigir
um nome generic usado uma única vez. A forma possui precedente nos
[opaque parameter declarations de Swift](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0341-opaque-parameters.md).

Extensions não adicionam storage:

```w
extension Dish: Displayable {
  fn display(): String { ... }
}
```

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

W não possui `static var` nem outro mutable type storage na DB2. Esse storage
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

W não reifica tipos como `Type<T>` na DB2. Associated member lookup continua
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
parcial. A forma líder aceita vários initializers por forma, delegação total e
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

A [Ada Reference Manual](https://docs.adacore.com/live/wave/arm22/pdf/arm22/arm-22.pdf)
também separa um subtype restringido por range do base type. O trabalho sobre
[Liquid Types](https://escholarship.org/uc/item/0vx7j8zc) trata refinements como
predicates verificáveis. W usa esses precedentes sem adotar seu source syntax.

As formas `T where (predicate)`, `T<where: (...)>` e `T(where: predicate)`
continuam como **Alternativa**. A forma líder mantém o predicate dentro do
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
precisa expor essa álgebra na DB2.

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

**Líder DB2:** use estado runtime para valores persistidos, compartilhados ou
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

W não adiciona keywords `state` ou `transition` na DB2. Const generics,
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

A DB2 usa `Result<Dish, KitchenError>`. Inlay hints e documentação mostram os
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

W também não possui estes kinds na DB2:

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

Generic defaults não entram na DB2. Um alias nomeado oferece um default sem
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
precisam restringir deve ser primário na DB2.

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

Polymorphic function values e inference pelo body ficam fora da DB2.

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

`any P` não conforma automaticamente a `P`. A DB2 também não abre existentials
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
DB2. Borrow e function conversions seguem suas regras próprias.

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
exige uma operação checked explícita.

Uma conversão implícita é permitida somente se:

1. é total para todos os valores do tipo de origem;
2. preserva o valor;
3. existe uma única rota canônica;
4. não muda ownership ou authority de forma oculta.

Narrowing, parsing, rounding, reinterpretation, ponteiro e conversão ambígua são
explícitos. O mesmo princípio permite `T` para `any P` quando `T: P`.

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

DB2 não possui `Type<T>`, `T.type` ou construção por metatype. Um `TypeId`:

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

fn reflectedName(value: ref any reflect.Reflectable): StringView {
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

DB2 sintetiza somente estas famílias:

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
`Hashable` ou `Duplicable` na DB2. O author fornece witnesses manuais. Essa
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

O source expandido deve ser `Arguments<T>`, `Slice<T>`, `[T; N]` ou
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
`MutableSlice<T>`.

Argumentos individuais podem usar storage no call frame. Uma expansão borrowed
passa address e count. O lowering não exige heap. `Arguments<T>` mantém cleanup
dos elementos owned em todas as saídas.

Rest W não faz parte do ABI C. C varargs continuam `unsafe` e exigem um adapter
tipado ou `c.vaList`. Default argument promotions não entram no type checker W.

#### 8.9.7 Formas adiadas

Três famílias não entram na DB2:

| Família | Estado | Baseline |
|---|---|---|
| typed property path | **Pesquisa** | closure ou função nominal |
| generic associated type | **Pesquisa** | primary associated type e método generic |
| type/value parameter pack | **Pesquisa** | rest homogêneo, tuple ou collection |

Uma typed property path precisa preservar place, borrow, accessors e
visibilidade. A forma líder de pesquisa usa um construtor explícito:

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
OOM geral encerra a fault boundary conforme a seção de panic.

### 9.7 Provenance, pointer e address

**Exemplo:** converter um pointer para um endereço inteiro e voltar não restaura
authority sem uma API `unsafe` específica.

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

Testes diferenciais executam o mesmo corpus em profile portátil e compacto.
Sanitizers executam o fallback. Um resultado diferente bloqueia a otimização.

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

Toda função fallible declara um error type concreto ou genérico. A DB2 não
possui `throws` sem tipo:

```w
fn map<U, E: Error>(
  transform: fn(ref T): U throws E,
): Array<U> throws E
```

Quando `E` é `Never`, o compiler especializa a função como nonthrowing. W não
precisa de `rethrows`.

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
`AllocationError` informa a classe e o allocator, mas não promete a quantidade
global de memória livre.

`BudgetExceeded` é diferente de OOM. Ele informa que uma quota conhecida foi
atingida:

```w
let frame = try arena.allocate(bytes: size) // pode devolver BudgetExceeded
```

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
report.cancel()
batch.cancel(reason: .shutdown)
```

Cancelamento é uma solicitação idempotente. Ele não usa `pthread_cancel` e não
faz unwind assíncrono de foreign frames.

`cancel` não é keyword nem statement. Ele é um método intrínseco do owner
`Task<T, E>`. O método retorna `()` e não consome o handle. Um `SharedTask`
observer não publica esse método. O type checker reconhece a operação para
preservar structured cancellation e trace.

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
caller só muda seu trabalho não isolado. `async let` e task groups herdam a
preference. Um future owner runtime precisa declará-la de novo.

`spawn` em um domínio estritamente serial é error. Trabalho UI deve chamar o
owner isolado:

```w
await renderer.show(plan)
```

`spawn<.ui>` confundiria affinity serial com paralelismo.

A [SE-0417](https://www.swift.org/swift-evolution/#SE-0417) também separa
executor preference de actor isolation. W mantém essa separação na HIR.

Seleção dinâmica usa API:

```w
let task = Task.spawn(executor: executor, operation: work)
```

`spawn<.compute>` é **Líder DB2**. O slot `domain` é primário e fechado.
`spawn<domain: .compute>` fica como **Alternativa**. `spawn on .compute` fica
**Rejeitado por enquanto** porque duplica o contrato estático com uma frase
especial.

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
fixar o contrato no source com um predicate de tipo explícito. A forma dessa
constraint continua em **Pesquisa**. Ela não cria traits para conformar.

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

### 12.9 Streams e channels

**Exemplo:** `Channel<Event>(capacity: 64)` aplica backpressure no item 65 até
existir espaço ou cancelamento.

**Direção:** `Stream<T, E>` usa pull. `next()` é async e move um elemento para o
consumer. Cancelar ou destruir o consumer fecha o producer scope.

Prefetch é explícito e bounded. Ordering, watermark e ownership fazem parte do
constructor. `yield` não entra na grammar antes de o verifier representar esses
contratos.

`Channel<T>` é um tipo separado. `send` move `T`. `receive` devolve ownership.
Capacity zero cria rendezvous. Capacity positiva é bounded. Ordering é FIFO por
sender, salvo policy mais forte.

### 12.10 Memory model, atomics e locks

**Exemplo:** `var atomic completed: u64` aceita incremento concorrente. Um `var`
comum não pode participar de data race em safe W.

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

**Exemplo:** o adapter SQLite confirma a transação antes de liberar a resposta.
Uma falha descarta o output retido.

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

### 13.9 Wasm e playground

**Exemplo:** o playground executa `add(2, 3)` em Wasm. Network permanece ausente
sem um import tipado do host.

Wasm é um target e uma boundary possível. Ele não transforma W em substituto de
JavaScript. O playground compila um subset para Wasm e usa imports/exports
tipados do host. DOM, network e storage só existem quando o profile concede.

### 13.10 Observabilidade e teste

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

### 14.1 T0 — core independente do ambiente

**Exemplo:** `Array.map` e `String.scalars` funcionam em target freestanding sem
console, clock ou filesystem.

T0 contém:

- tipos primitivos, Option, Result e Error;
- String, StringView, Bytes, StringBuilder, Array, Map, Set, Range e views;
- Slice, `Pinned<T>`, AllocationError e allocator hooks;
- protocols de igualdade, hash e iteração;
- `Arguments<T>`, `reflect.TypeId` e reflection opt-in;
- intrinsics de ownership e dos predicates `transferable`/`shareable`;
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
- ServiceRef, ServiceFailure e service host APIs;
- TCP, UDP, TLS e DNS;
- crypto, codecs, JSON e FFI C;
- storage e observabilidade básicas.

`print` é um nome normal da prelude T1. Ele só está disponível num scope de host
que concede Console. Uma função exportada fora desse scope recebe `Console`
como capability explícita.

### 14.3 T2 — domínios oficiais

**Exemplo:** `std.http` e `std.si` são bundled, mas só entram no payload quando
o programa os alcança.

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

**Exemplo:** `u8.max + 1` causa panic. `u8.max.wrappingAdd(1)` produz zero de
forma explícita.

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
`hasAll`. Um `case` usa `if` para adicionar um guard. O guard não substitui
`&&` em outra expressão Boolean.

Somente tipos discretos/strideable podem iterar um Range. Outros usam
`stride`. `clamp` exige um intervalo fechado.

Um range unilateral pode aparecer como argumento ou pattern:

```w
let tail = orders.slice(4...)
```

O parser distingue essa forma pelo fim do argumento. `each values` não reutiliza
o mesmo token.

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

## 16. Texto, bytes e collections

### 16.1 `String` e unidades de texto

**Líder DB2:** `String` é um valor owned, contíguo e UTF-8 válido. Uma mutação
exige acesso exclusivo e preserva UTF-8 válido.

```w
var title = "Last Light"
title.append(" Restaurant")
let bytes = title.bytes.count
let scalars = title.scalars.count
let graphemes = title.graphemes.count
```

`String` pode conter U+0000. Ele não possui terminador NUL obrigatório. SSO,
COW e a estratégia de crescimento são otimizações invisíveis. Cópia e move
continuam com a semântica definida na seção 9.

Esse contrato segue a experiência de
[Swift com armazenamento UTF-8](https://www.swift.org/blog/utf8-string/) sem
tornar a representação curta ou COW parte da linguagem.

`String` não possui `length`, `count` ou subscript direto. O programa escolhe
uma destas unidades:

| Forma | Elemento | Custo de `count` |
|---|---|---|
| `text.bytes` | `u8` da codificação UTF-8 | O(1) |
| `text.scalars` | `UnicodeScalar` | O(n), salvo cache invisível |
| `text.graphemes` | grapheme cluster estendido | O(n), salvo cache invisível |

```w
let sign = "A🇧🇷e\u{301}"
expect sign.bytes.count == 12
expect sign.scalars.count == 5
expect sign.graphemes.count == 3
```

`UnicodeScalar` é `Copy`. Ele contém um scalar Unicode válido e nunca contém
surrogate. W não define um tipo universal chamado `Char` ou `Character`.
Um elemento de `graphemes` é um `Grapheme`, que empresta um cluster contíguo.

```w
for scalar in sign.scalars {
  print("U+${scalar.hex}")
}

for grapheme in sign.graphemes {
  print(grapheme)
}
```

### 16.2 Views, índices e slices

`StringView` empresta uma subsequência UTF-8 contígua. `String`, `StringView` e
`Grapheme` oferecem as views válidas para seu conteúdo.

```w
fn firstWord(line: ref String): StringView? {
  return line.scalars.split(where: (scalar) => scalar.isWhitespace).first
}
```

`text.bytes[usize]` tem acesso aleatório O(1). As views de scalar e grapheme não
aceitam um ordinal em subscript. Isso evita esconder uma busca O(n).

```w
let firstByte: u8 = text.bytes[0]
let secondScalar = text.scalars.element(at: 1) // busca explícita O(n)
```

`ScalarIndex` e `GraphemeIndex` pertencem ao source que os criou. O type checker
rastreia essa origem como um borrow sem annotation pública. O programa não pode
usar o índice em outro source nem mutar o owner enquanto o índice está vivo.

```w
let start: ScalarIndex = text.scalars.start
let end = text.scalars.index(start, offsetBy: 4)
let prefix: StringView = text.scalars[start..<end]

// Erro: `start` empresta `text`, não `other`.
let invalid = other.scalars[start..<end]
```

Um slice de bytes retorna `Slice<u8>`. Ele pode cortar a codificação de um
scalar. A conversão de byte range para `StringView` valida os dois limites.

```w
let raw: Slice<u8> = text.bytes[1..<4]
let view: StringView = try text.view(bytes: 1..<4)
let owned: String = view.toString()
```

Uma mutação invalida views e índices. O borrow normalmente rejeita a mutação.
Um adapter unsafe deve restabelecer a mesma regra.

```w
let view = text.scalars[start..<end]
text.append("!") // Erro: `view` mantém um borrow ativo.
print(view)
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

`String.fromUtf8` empresta `Bytes` ou `Slice<u8>`. Ele informa o primeiro byte
inválido. A forma `replacingInvalidUtf8` substitui sequências inválidas por
U+FFFD. W não faz essa substituição de forma implícita.

```w
let invalid = b"\x66\x6f\x80"

do {
  let _ = try String.fromUtf8(invalid)
  panic("invalid UTF-8 was accepted")
} catch .invalidByte(let offset) {
  expect offset == 2
}
```

### 16.4 Construção e concatenação

Interpolação é a forma canônica para construir texto com valores de tipos
diferentes. Cada valor atende ao protocol `Display`.

```w
let message = "Order ${order.id} has ${order.guests} guests"
```

`+` cria um novo `String`. `+=` e `append` mutam um `String` exclusivo.
Os operadores aceitam somente `String`. Uma view usa builder, interpolation
ou `toString()`. W não converte números ou objetos nesses operadores.

```w
var greeting = "Hello"
greeting += ", universe"
let question = greeting + "?"
```

`StringBuilder` torna alocação e custo explícitos em loops ou construções
grandes. O formatter pode sugerir o builder para uma cadeia de `+`.

```w
var output = StringBuilder(reservingBytes: rows.count * 32)

for row in rows {
  output.append(row.name)
  output.append("\n")
}

let report = (take output).finish()
```

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

`String`, `StringView`, `Grapheme` e literais comparam texto sem materializar
outro `String`. O hash da mesma sequência é igual em todas essas views.

```w
let verb: StringView = command.scalars[start..<end]
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

Codecs T1 convertem formatos externos. Locale do processo nunca muda a
interpretação de `String`.

```w
let legacy = try TextCodec.windows1252.decode(payload)
```

O [`OsString` de Rust](https://doc.rust-lang.org/std/ffi/struct.OsString.html)
é um precedente para preservar texto nativo sem forçar UTF-8.

### 16.8 C strings e buffers sentinela

`CString` é um buffer owned com terminador NUL. Sua construção rejeita NUL
interno. `CStringView` empresta o mesmo contrato.

```w
let name = try CString.from("last-light")

name.withPointer((pointer) => unsafe {
  c_register_restaurant(pointer)
})
```

Um pointer C recebido precisa de limite máximo. O wrapper procura o terminador
dentro desse limite e valida a codificação separadamente.

```w
let bytes = unsafe {
  try CStringView.from(pointer, maxBytes: 4096)
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
courses.reserve(minimumCapacity: 16)
courses.append(.broth)
courses.insert(.cake, at: 0)
expect courses == [.cake, .broth]
```

A estratégia de crescimento e o valor exato de `capacity` não fazem parte da
semântica, da igualdade ou da ABI. `reserve` segue a policy normal de OOM.
`tryReserve` retorna falha recuperável:

```w
try buffer.tryReserve(minimumCapacity: packetSize)
expect buffer.count == oldCount // quando a reserva falha
```

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

#### 16.10.3 `Slice` e mutation

`Slice<T>` é uma view contígua shared e `Copy`. `MutableSlice<T>` é uma view
contígua exclusiva e move-only:

```w
let middle: Slice<Order> = orders[1..<4]
let tail: MutableSlice<Order> = orders.mutableSlice(4...)
tail[0].priority += 1
```

Range inválido em subscript produz panic. `get(range)` retorna uma view
optional para input externo. Uma view não muda o count do owner:

```w
let payload = bytes.get(packetRange) // Slice<u8>?
let invalid = bytes[0...bytes.count] // panic: closed upper bound is outside
```

O owner não pode mover, desalocar ou alterar a estrutura enquanto uma view está
viva. Alterar elementos por `MutableSlice` continua válido:

```w
let view = orders[0..<2]
orders.append(next) // error: append can relocate storage borrowed by view
print(view[0])
```

Uma pointer C existe somente por um adapter scoped `unsafe`. Ela não estende o
lifetime do slice:

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
let names: Array<String> = guests.map((guest) => guest.name.toString())
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
let name: StringView = request.pathSegment(0)
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
igualdade da key owned. `StringView` atende a `EquivalentKey<String>`. Uma
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

O T0 contém `Array`, arrays fixos, `Slice`, `MutableSlice`, `Map`, `Set`,
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
let row = forecast[table]
```

Fornecer todos os índices produz um elemento. Fornecer um prefixo produz uma
view da rank restante. `forecast[table][course]` é válido, mas materializa a
operação intermediária no source e pode formar uma view temporária.

Regras:

- `*`, `/`, `+`, `-` são elementwise para shape igual;
- scalar expansion é total;
- `@` usa somente a família rank-1/rank-2 definida acima;
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
| slice ou String view | pointer, length e lifetime scoped |
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

`fn<C>` é **Líder DB2**. `fn<lang: .c>` permanece **Alternativa**. Um source
separado com `from` e compilation units nomeadas permanecem **Pesquisa**.

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

### 19.3 Dialeto W/MLIR

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

### 19.4 ABI e runtime

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
| funções | funções livres, `const fn`, `const init`, methods, `static fn`, labels, recursion e calls diretas/por `fn(...)` |
| tipos | scalars, tuples, structs, objects, enums, Option, typed Error e newtypes |
| protocols | primary associated types, composition e dispatch estático; sem existential |
| números | widths fixas, `usize`, checked arithmetic, bit operations e endian explícito |
| dados | String, StringView, Bytes, StringBuilder, Slice, Array, Map, Set e Range de T0 |
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

### 19.6 Incrementalidade

**Exemplo:** alterar o corpo privado de `parseLine` não recompila importers
quando a interface serializada permanece igual.

Cache é content-addressed por source normalizado, interface das dependências,
edition, target, profile, toolchain e flags semânticas. Type checking ocorre por
módulo. Instâncias generics e outputs de passes possuem chaves próprias.

Um cache miss afeta performance, não resultado. A ferramenta registra o motivo
do miss. Ela não usa timestamps como identidade.

### 19.7 Diagnostics e debug

**Exemplo:** um use-after-move informa o move original, o uso inválido e um
fix-it que propõe `copy` somente quando o tipo atende a `Copy`.

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
numbers, size literals, booleans e enum values. Ele não executa imports, loops,
funções ou I/O.

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

**Exemplo:** `w build --locked` recebe target, profile, flags, environment
declarado e lockfile como inputs da recipe.

- source é o fallback normativo;
- binaries são otimização sob uma chave ABI completa;
- static linkage é preferido quando compatível;
- build scripts não recebem rede ou filesystem irrestrito;
- code generation é uma tool target hermética;
- adapters `fn<Language>` são tool targets fixadas, não shell commands livres;
- cada foreign unit possui source digest, toolchain, target, ABI e symbol manifest;
- cache é content-addressed;
- recipe fixa toolchain, target, profile, inputs e environment permitido;
- recipe fixa quotas e evaluator version de compile-time;
- CBOR determinístico é a representação canônica inicial;
- SHA-256 tagged é o digest inicial e possui algorithm agility.

`w build --locked` falha se manifest, context ou lock divergirem. CI/release usa
esse modo. `w update package` mostra o diff mínimo do grafo. Offline não acessa a
rede; frozen pode buscar somente objetos já fixados.

Mesma fonte, recipe e ambiente fixado devem produzir o mesmo payload bit a bit.
Data, commit, paths, locale, timezone, seeds e environment são inputs explícitos
ou são removidos.

### 20.3 Verificação

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

### 20.4 Registry, mirrors e estado de segurança

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

### 20.5 Scripts e supply chain

**Exemplo:** um build script sem capability de rede não baixa um binary durante
CI.

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

## 21. Tooling e interface para máquinas

### 21.1 Tooling humano

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

### 21.4 Interface para modelos

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

Contagem de tokens depende do tokenizer. A DB2 mede vários modelos antes de
trocar uma keyword por pontuação. Compile success, testes e edit distance têm
mais peso que token count isolado.

### 21.5 Diagnostics estruturados

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

Source annotations de suppressão não entram na DB2. Elas esconderiam policy no
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

## 22. Protocolos e pesquisas de ecossistema

Nenhum item desta seção reserva keyword. Cada hipótese usa os contratos do core
e pode evoluir como package separado.

### 22.1 Contrato tipado e wRPC

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

### 22.2 JSON, WLO e wStruct

**Exemplo:** um adapter JSON rejeita field desconhecido quando o schema usa modo
strict. WLO não muda essa regra por syntax.

- JSON é o primeiro codec de interoperabilidade e debug.
- WLO/WLON é pesquisa de formato data-only canônico para valores W.
- wStruct pesquisa IPC sob target, ABI e layout idênticos.

WLO precisa de grammar menor que W, canonical bytes, limits e fuzzing. wStruct
não serializa pointers, padding ou handles crus. Ambos precisam de fallback.

### 22.3 wQL e RestPC

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

### 22.4 V6 e Computer Units

**Exemplo:** uma Computer Unit pode virar uma service instance. Ela não recebe
wire format ou deployment implícitos.

V6 continua pesquisa de runtime/host serverless. Computer Unit é uma instância
com entrypoints, limits e capabilities. Esses conceitos podem hospedar services
W, mas não definem a linguagem, wRPC ou o package manager.

### 22.5 Tree strings

**Exemplo:** um protótipo compara árvore compacta com `String` + parser em tamanho,
lookup, edição e interoperabilidade.

Tree strings continuam uma estrutura especializada para interning, índices ou
edição. `String` público permanece UTF-8 contíguo. Codec e ABI observam o valor
lógico, não a representação experimental.

### 22.6 GPU, HDL, PGO e geração assistida

**Exemplo:** um kernel tensor pode baixar para GPU quando o device atende ao
contrato. O mesmo source mantém fallback CPU correto.

GPU começa por um kernel puro com baseline CPU, device transfer explícita e
comparação de resultado/custo. HDL exige um modelo próprio de timing e
verificação; não é lowering automático de código CPU.

PGO, snapshots e casos gerados por IA são artefatos de tooling. Seed, workload,
provenance e oracle são explícitos. Nenhum deles muda a semântica source.

### 22.7 Gate de promoção

**Exemplo:** `fn<Rust>` não entra na linguagem até reproduzir archive, façade C,
diagnostics e cleanup em dois targets.

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
| Bytes, paths nativos e C strings distintos | **Possível agora** | fronteiras conhecidas; conversões preservam perda e terminador |
| graphemes default e normalização versionados | **Possível agora** | tabelas Unicode geradas; custo linear permanece visível |
| `InlineString` com layout público | **Pesquisa** | benefício depende de target, ABI e benchmark contra SSO invisível |
| strict numerics e overflow verificado | **Possível agora** | backend oferece operações adequadas |
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
| `fn<C>` com static archive | **Provável** | depende primeiro da façade C e do build hermético |
| `fn<Rust>`/`fn<Swift>` | **Pesquisa** | toolchain, runtime, ABI e agrupamento são maiores que C |
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
  → controle PID com `init`, computed property, ranges e units
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

## 26. Plano de implementação

Cada fase entrega um corte vertical, tests e uma demonstração reproduzível no
corpus ou na CLI. O portal gerado começa somente depois do design freeze.

### 26.1 Fase -1 — design e corpus

**Exemplo:** cada forma líder possui um caso positivo, um negativo e uma
alternativa preservada.

- consolidar este documento;
- criar corpus DB2 positivo, negativo e comparativo;
- completar o restaurante cósmico;
- fixar diagnostic IDs e formatter examples.

Saída: toda forma implementada possui contrato, alternativa e teste.

### 26.2 Fase 0 — lexer, parser e formatter

**Exemplo:** `parse → format → parse` de `callables.w` produz árvores
equivalentes e nenhum error node.

- lexer lossless;
- recursive-descent/Pratt;
- EBNF;
- CST/recovery;
- modifiers `const fn` e `const init`;
- contratos estáticos com expression, record e list payloads;
- referência `.member` contextual sem perda no CST;
- patterns nominais de struct e marker `...`;
- formatter idempotente;
- Tree-sitter e semantic highlight projetados do corpus.

Saída: parse/format/parse estável e diagnostics preparados.

### 26.3 Fase 1 — AST, nomes e tipos

**Exemplo:** `w check` rejeita um overload por tipo antes de existir backend.

- AST e module graph;
- imports, visibilidade efetiva e interface normalizada;
- primitives, `()`, `Never`, structs, enums, functions, Option e error sets;
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
- interface compilada inicial.

Saída: `w check` verifica o subset síncrono do restaurante.

### 26.4 Fase 2 — HIR, MLIR e executável nativo

**Exemplo:** o mesmo programa aritmético gera HIR equivalente pelo seed C e pelo
frontend self-hosted.

- HIR tipada;
- witnesses sintetizados e descriptors alcançáveis;
- dialeto W/MLIR e verifiers;
- arithmetic/control lowering;
- LLVM/native e runtime core;
- seed C aceita o primeiro `bootstrap.w0` e emite C11;
- corpus diferencial entre o caminho seed-C e W/MLIR.

Saída: payload determinístico para programas síncronos nos dois caminhos.

### 26.5 Fase 3 — memória, errors e C

**Exemplo:** um callback C com context executa cleanup uma vez em success, error
e cancelamento.

- initialization e whole-value move;
- receiver `take fn`, deinit e saídas com consumo;
- transições typestate consuming e outcomes que devolvem o novo owner;
- borrows, drop e defer;
- typed errors e panic boundary;
- allocator hooks;
- `foreign c`, unsafe e wrappers;
- primeiro adapter `fn<C>` com body opaco e static archive.

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

Em SH5, o parser self-hosted reconhece toda a grammar congelada da DB2. Ele pode
emitir um diagnostic de profile para semantics que ainda não possuem lowering.
As fases seguintes adicionam esses verificadores e lowerings ao compiler
self-hosted. O source de `compiler/core-w0` continua restrito a W0. Assim, tasks,
services, units, tensors e packages não ampliam a base de recovery.

### 26.7 Fase 5 — tasks

**Exemplo:** `mixPair` cancela o sibling após erro e aguarda ambos os children
antes de sair do scope.

- async state machine;
- `async<.domain> let`, `spawn<.domain> let` e inheritance de preference;
- linear Task, `TaskOutcome` e cancellation;
- `concurrentMap`/`parallelMap` bounded;
- blocking adapter e callback scheduling;
- HIR verificada antes do lowering async;
- executor cooperativo e pool paralelo bounded;
- deterministic test executor.

Saída: restaurante executa I/O concorrente e lotes paralelos com ordering,
backpressure e cleanup reproduzíveis.

### 26.8 Fase 6 — services e host entries

**Exemplo:** `entry LastLight` valida `process.main` e `http.fetch` contra o
profile do host.

- `entry` e host profiles;
- service instance manager;
- closed turn, generation e drain;
- mailbox com três quotas;
- `ServiceFailure`, cycle detection e `ServiceRef`;
- tracing e local fast path;
- process/Wasm boundary experimental.

Saída: CLI e HTTP exibem hops, queues, overload, cycle e restart.

### 26.9 Fase 7 — packages e SDK

**Exemplo:** CI recompila um package público pela recipe e compara o digest antes
de marcar a versão como reproduced.

- package parser, resolver, lock e CAS;
- builds `--locked`/offline;
- T0/T1 mínimos;
- provenance, SBOM e reprodução local;
- lens por import.

Saída: uma máquina limpa reconstrói o mesmo payload sem rede durante o build.

### 26.10 Fase 8 — ciência e extração

**Exemplo:** `Matrix<2, 3> @ Matrix<3, 4>` baixa para CPU e mantém shape
`Matrix<2, 4>`.

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

## 27. Mudanças da DB1 para a DB2

| Tema | DB1 | Líder DB2 |
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
| D2-007 | visibility | módulo default, `package`, `export`; sem `private` | public universal; `public/private`; block export |
| D2-008 | import seletivo | `{X} from path` | `path.{X}`; imports livres |
| D2-009 | import namespace | `import path as alias` | forma DB1 `name as alias from path` |
| D2-010 | módulos | manifest multi-file, DAG | declaração `module`; cycles de interface |
| D2-011 | runtime top-level | declarations/const somente | init global; ordem de inicializadores |
| D2-012 | tipos nominais | `type X = T` | wrapper struct; `newtype` |
| D2-013 | alias | `alias X = T` | `typealias`; context-dependent `type` |
| D2-014 | refinement | `T<(.member predicate)>`; range como sugar | `value.member`; `T where (...)`; `T(where:)` |
| D2-015 | value generics | `const` parameters e labels | positional only; contrato universal aberto |
| D2-016 | existential | `any P` | `P` sozinho; `dyn P`; `Any` universal |
| D2-017 | opaque type | `some P` em local, retorno e parâmetro generic anônimo | existential; generic nomeado |
| D2-018 | reflection | `reflect.Reflectable` opt-in e alcançável | metadata universal; annotations |
| D2-019 | Option | `T?` com some/none | null; sentinel; result-like |
| D2-020 | conversão | total, única e sem perda | tudo explícito; promotions amplas |
| D2-021 | owner | único/move-first | ARC universal; GC |
| D2-022 | borrow | `ref` e `inout` | lifetime annotations públicas; pointers |
| D2-023 | transfer | last-use + `take` obrigatório na API | move sempre explícito; move implícito amplo |
| D2-024 | copy | implícito só para `Copy`; `copy value` explícito usa `Duplicable` | `.clone()` universal; COW como contrato |
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
| D2-035 | panic | encerra a fault boundary física mais próxima | unwind recuperável; tratar toda isolation como fault boundary |
| D2-036 | async cleanup | `defer async` | RAII sync only; `using`; cleanup solto |
| D2-037 | concorrência | `async let` | Future/Promise; task API somente |
| D2-038 | paralelismo | `spawn let` | mesma keyword de async; parallel loop apenas |
| D2-039 | execution domain | `async/spawn<.domain>` | `<domain: .name>`; `on .name` (**Rejeitado por enquanto**); descriptor-only |
| D2-040 | Task | linear, lexical, one-await | Future clonável; detached default |
| D2-041 | grupos | lexical e bounded | queue ilimitada; thread pool exposto |
| D2-042 | solicitação de cancelamento | `task.cancel(reason:)` intrínseco | statement `cancel` (**Rejeitado por enquanto**); async thread cancellation |
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
| D2-054 | range | quatro closures; unilateral em argumento/pattern; intervalo | dois ranges; producer universal |
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
| D2-072 | inline language | `fn<C>` com adapter externo | `fn<lang: .c>`; library import; multi-language v0 |
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
| D2-097 | aplicação `<...>` | contrato fechado por head e payload tipado | slots universais; mapa aberto |
| D2-098 | campos | imutável sem prefixo; `var` para mutation | `let` obrigatório; `let` opcional |
| D2-099 | collection dinâmica | `Array<T>`, `Map<K, V>` e `Set<T>` | `[T]`; braces para map/set |
| D2-100 | tensor indexing | `tensor[i, j]`; prefixo retorna view | nesting obrigatório; método `at` |
| D2-101 | recurso async | `defer async` + `take async fn`; obrigação linear em pesquisa | async destructor; `using await`; lint |
| D2-102 | receiver | `fn` borrow, `mut fn` exclusivo, `take fn` owned, `static fn` sem receiver | `self`; inferir static; função livre |
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
| D2-114 | cláusula estática | `<...>` no source e record tipado na HIR | `where`/`on`; modifier map |
| D2-115 | slots angulares | schema declara posição, labels e slot primário | inferir slot pelo nome do enum case |
| D2-116 | evolução self-host | gates SH0–SH7; W0 fechado e core separado | marco único; compiler usa toda a DB2 |
| D2-117 | eixos de execução | lifetime, intent, preference, isolation e affinity separados | thread group único |
| D2-118 | início de child | `async let`/`spawn let` iniciam na declaração | lazy no primeiro await |
| D2-119 | task longa | owner runtime explícito; sem detached sem owner | drop destaca; task global |
| D2-120 | outcome de task | success/error/canceled; panic encerra fault boundary | cancel em `E`; panic como Result |
| D2-121 | seleção de error | ordem lexical declarada | primeira completion sempre vence |
| D2-122 | cancelamento | cooperativo, idempotente e sem rollback implícito | matar thread; transação implícita |
| D2-123 | resolução de domain | isolation/affinity vencem preference | contrato do caller substitui isolation |
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
| D2-138 | payload angular | `()`, `{}` e `[]` são expression, record e list | três operadores universais |
| D2-139 | extensão de tipo | refinement, extension, struct, enum e C union separados | `T<{...}>` universal |
| D2-140 | foreign artifact | unit agrupada, archive/object e façade C | archive por função; C source obrigatório |
| D2-141 | foreign parser | body opaco entregue ao adapter da linguagem | parser W interpreta subset externo |
| D2-142 | foreign delimiter | body braced com scanner do adapter | raw fence hash; parser W conhece strings externas |
| D2-143 | language tag | `LanguageAdapterId` fixada no lock | enum eterno no compiler; string ou command livre |
| D2-144 | referência contextual | `.member` usa subject ou enum esperado; HIR qualificada | somente `value.member`; `.case` apenas |
| D2-145 | generic refinado | `Array<T><(predicate)>` separa aplicação e refinement | `Array<[T, predicate]>`; slot misto |
| D2-146 | unit e bottom | `()` e `Never` | `Void`; `!`; retorno omitido dependente do contexto |
| D2-147 | retorno fluente | `: self` explícito como reborrow | retorno `self` implícito; `Self` owned; builder externo |
| D2-148 | associated member | `const`, `static fn` e `type` requerido | companion object; metatype runtime obrigatório |
| D2-149 | associated type witness | `type Name` exige `alias Name = T` | `associatedtype`; `type Name = T` contextual |
| D2-150 | mutable type storage | ausente; owner de `entry` ou service explícito | `static var`; módulo singleton |
| D2-151 | object singleton | `object` permite várias instances; singleton é composição | object declaration singleton; module singleton |
| D2-152 | construção | `Type(...)` baixa para `construct`; sem promessa de placement | `new Type`; literal `Type {...}` |
| D2-153 | initializer sintetizado | struct usa menor nível; object fica no módulo | visibilidade do tipo sempre; sempre privado |
| D2-154 | initializer customizado | vários `init` com formas disjuntas; `throws E`; factory nomeada | initializer único; `init?`; `async init` |
| D2-155 | definite initialization | duas fases; sem uso de `self` parcial; cleanup por field | zero universal; runtime check; partial safe value |
| D2-156 | computed property | `name: T { get }`; `var` exige write accessor | getter implícito; method obrigatório |
| D2-157 | efeitos de property | property-safe, síncrona, local e sem `throws` | `async`/`throws` property; custo irrestrito |
| D2-158 | mutation de property | `set(value)` e `modify` com `return inout` escopado | get-modify-set implícito; observers |
| D2-159 | property requirement | `{ get [set] [modify] }`; stored field pode ser witness | protocol exige storage; reflection estrutural |
| D2-160 | struct transparente | sem `init`: stored fields herdam visibilidade do tipo | `export` por field; todos os members herdam |
| D2-161 | struct encapsulado | `init` explícito restaura default de módulo nos fields | keyword `opaque`; field sempre público |
| D2-162 | object | storage e initializer sintetizado ficam no módulo | herdar visibilidade do object; constructor público |
| D2-163 | enum e protocol | cases e requirements herdam; witness não repete modifier | `export` repetido; todos os members públicos |
| D2-164 | service | storage nunca cruza módulo; API usa protocol async | field público; computed property remota |
| D2-165 | interface exportada | signature não expõe tipo menos visível; HIR normaliza | lint apenas; defaults preservados na HIR |
| D2-166 | pattern de struct | `Type(field, field: pattern, ...)`; nominal e ordenado | `{field}`; tuple posicional |
| D2-167 | evolução de pattern | `...` obrigatório fora do package | exaustivo externo; modifier no tipo |
| D2-168 | ownership de pattern | modo uniforme owned, `ref` ou `inout` | qualifier por field; partial move |
| D2-169 | limite de destructuring | struct visível; object e service rejeitados | destructuring estrutural universal |
| D2-170 | evolução de struct | field com default é minor se a resolução não muda; field obrigatório é major | todo field novo é major |
| D2-171 | evolução de enum | enum fechado; case novo é major | `nonexhaustive`; default case obrigatório |
| D2-172 | source contra schema | source, ABI e wire evoluem por contratos separados | derivar schema do struct |
| D2-173 | verificação SemVer | `w interface diff` classifica e sinaliza revisão | revisão manual; só major/minor binário |
| D2-174 | consuming receiver | `take fn`; call usa `(take value).method()` | consumo implícito; `consuming fn`; free function |
| D2-175 | saída consuming | success, error e cancellation consomem; owner pode ser retornado | restaurar no error; abortar sem drop |
| D2-176 | authority de `deinit` | exclusivo e não consuming; mutation sem move | borrow read-only; consumir fields |
| D2-177 | supressão de drop | ausente em safe W; wrapper mantém estado válido | `discard self`; `forget` geral |
| D2-178 | limite de receiver | protocol exige mode exato; service e handles aliases não usam `take fn` | adaptação com copy; service consuming |
| D2-179 | `deinit` e copy | tipo com cleanup customizado não atende a `Copy` | copiar e contar drops; lint |
| D2-180 | identidade de overload | owner, nome e forma de call | tipos, return type ou constraints |
| D2-181 | resolução de overload | forma antes do type-check; sem backtracking | ranking de melhor candidato |
| D2-182 | defaults e overload | famílias de formas devem ser disjuntas | preferência por menos defaults |
| D2-183 | ownership do overload set | um owner; imports não fundem sets | overload set aberto entre módulos |
| D2-184 | overload como valor | closure explícita seleciona a forma | expected type; seletor de forma |
| D2-185 | vários initializers | labels e formas disjuntas | ranking por tipos; initializer único |
| D2-186 | delegação de initializer | `self = Type(...)` antes de qualquer field | `self.init`; delegação parcial |
| D2-187 | falha de initializer | cleanup parcial; `deinit` após self completo | zero universal; leak parcial |
| D2-188 | efeitos de initializer | síncrono; `throws E`; sem `init?` | `async init`; initializer failable |
| D2-189 | evolução de overload | set existente: minor; primeiro overload: major; forma alterada: major | classificação somente por nome |
| D2-190 | ordem de argumentos | ordem da declaração; labels não reordenam | named arguments livres |
| D2-191 | parâmetros rest | `T...` homogêneo e final; `each` expande collection | somente collection; type pack; C varargs |
| D2-192 | function type | source usa `fn(A): B`; labels e defaults ficam na declaração | labels no tipo; somente inference |
| D2-193 | callable concreto | `some fn(A): B` preserva tipo, captures e specialization | generic nomeado; `fn` sempre apagado |
| D2-194 | callable apagado | `any fn(A): B` guarda owner, invoke e drop | `CallbackType`; box manual |
| D2-195 | callable mode | `fn`, `mut fn` e `take fn` descrevem uso do ambiente | `Fn`/`FnMut`/`FnOnce`; inferência sem annotation |
| D2-196 | call por valor | posicional, aridade completa e sem defaults | labels cosméticos; labels significativos |
| D2-197 | capture e escape | HIR registra place, modo, lifetime, owner e drop | capture sempre weak; heap por default |
| D2-198 | method reference | closure explícita mostra receiver e ownership | bound method implícito |
| D2-199 | callback C | `unsafe fn<abi: .c>` fino + context/owner explícitos | converter closure W; callback universal |
| D2-200 | static list | `StaticList<T>` compile-time, ordenada e apagada | named index runtime; set implícito |
| D2-201 | operador `@` | família rank-1/rank-2 sem broadcast; APIs nomeadas para rank maior | contração geral implícita; `*` linalg |
| D2-202 | exemplo normativo | cada contrato aponta para exemplo válido, erro ou cenário canônico | afirmação sem evidência local |
| D2-203 | opaque parameter | `some P` é generic anônimo e especializado | exigir generic nomeado; existential |
| D2-204 | switch | expressão exaustiva, sem fallthrough ou `break` | switch statement; fallthrough explícito |
| D2-205 | ordem de case | ordem lexical, first-match e diagnostic de inalcançável | exigir patterns disjuntos; ranking |
| D2-206 | múltiplos scrutinees | tuple subject e tuple pattern | `switch a, b`; matching relacional implícito |
| D2-207 | custom pattern | pesquisa; conversão nomeada ou guard na DB2 | handler arbitrário; protocol de pattern na v0 |
| D2-208 | callable transfer | `fn` é transferível/compartilhável; closure deriva predicates do ambiente | `Send`/`Sync` nominais; confiar no pointer |
| D2-209 | compatibilidade callable | signature invariável; somente callable-mode possui lattice | variance; effect widening; ranking |
| D2-210 | semântica de String | owned, contíguo, UTF-8 válido e mutable por acesso exclusivo | tree/rope default; UTF-16; COW contract |
| D2-211 | unidades e custos | sem `length`; bytes O(1), scalars/graphemes podem ser O(n) | grapheme default; cache obrigatório |
| D2-212 | elementos de texto | `UnicodeScalar` Copy e `Grapheme` borrowed; sem `Character` universal | Character owned; scalar chamado Char |
| D2-213 | índices de texto | byte usa `usize`; scalar/grapheme usam índices borrowed do source | ordinal em subscript; índice universal |
| D2-214 | slices de texto | byte slice é `Slice<u8>`; byte range para StringView é fallible | arredondar boundary; slice sempre String |
| D2-215 | Bytes | tipo binário owned distinto de `String` e `Array<u8>` | alias de Array; String aceita UTF-8 inválido |
| D2-216 | conversão UTF-8 | validação e reparo explícitos; erro informa byte offset | replacement implícito; locale codec default |
| D2-217 | construção de String | interpolation canônica; `+` aloca; `+=` muta; builder para volume | concat adjacente; conversão universal implícita |
| D2-218 | raw/multiline | `#"..."#`, `${}`, multiline com dedent determinístico | hashes arbitrários; `r` prefix; três delimitadores equivalentes |
| D2-219 | byte string | `b"..."` produz Bytes ASCII/escapes, sem interpolation | Unicode direto; Array literal somente |
| D2-220 | igualdade Unicode | sequência exata; normalização e collation nomeadas | equivalência canônica em `==`; locale global |
| D2-221 | bundle Unicode | edição fixa UAX #15/#29/#31 e UTS #39 em tabelas testadas | versão do host; ICU obrigatório |
| D2-222 | texto do host | `OsString`, `Path`, `Utf8Path` e `PackagePath` distintos | paths sempre String; bytes portáveis do OS |
| D2-223 | C strings | `CString`/view separados, NUL verificado e inbound bounded | String sempre NUL; scan C ilimitado |
| D2-224 | storage textual | refinement não fixa layout; `InlineString` permanece Pesquisa | capacity em String; SSO observável |
| D2-225 | estruturas textuais | rope, piece table, interning e tree string são especializadas | tree string geral; representation ABI única |
| D2-226 | ordem de avaliação | esquerda para direita e sequenciada; formas condicionais short-circuit | ordem não especificada; optimizer escolhe |
| D2-227 | resultados borrowed | `ref`/`inout` em tipos e retorno, provenance inferida e interface registrada | lifetime no source; lookup owned |
| D2-228 | array dinâmico | `Array<T>` owned, contíguo, count/capacity O(1) e append amortizado O(1) | linked chunks default; `[T]` |
| D2-229 | literais de array | `[a, b]`, `[]` contextual e `[value; count]` fixo com Copy | `[:]`; repeat clona move-only |
| D2-230 | views de array | `Slice<T>` shared Copy e `MutableSlice<T>` exclusiva move-only | pointer público; resize pela view |
| D2-231 | iteração | single-pass; borrow default, `ref`/`inout`/`copy` explícitos e `take` consome | copiar sempre; mutation estrutural durante loop |
| D2-232 | pipelines | Array eager; `.lazy` e Iterator lazy; `collect()` materializa | tudo lazy; tudo eager |
| D2-233 | `Map` | hashing keyed e ordem de inserção estável; full key confirma colisão | ordem de bucket; guardar somente hash |
| D2-234 | `Set` | ordem de inserção; equality ignora ordem; sem literal próprio | set não ordenado; literal com chaves |
| D2-235 | hashing | `Hashable: Equatable`; algoritmo/seed process-local e não persistente | XXH como ABI; hash como identity |
| D2-236 | lookup borrowed | `EquivalentKey<K>` permite view com a mesma equality e hash feed | alocar key em todo lookup; equivalência ad hoc |
| D2-237 | ordenação | `sort` stable por default; `sortUnstable` explícito; comparator `Ordering` | algoritmo fixo no contrato; Bool comparator |
| D2-238 | maps ordenados | `SortedMap` por total order para range e key order | tornar todo Map tree; B-tree no ABI |
| D2-239 | cleanup de collections | ordem inversa de índice/inserção; capacity e buckets invisíveis | drop order não especificada |
| D2-240 | escopo da std | core em T0; Deque/PriorityQueue/BitSet em `std.collections`; concorrentes fora de T0 | todas as estruturas no prelude |
| D2-241 | duplicação owned | `Copy` barato e implícito; `Duplicable` explícito via `copy value` | clone method; copiar owned implicitamente |
| D2-242 | ausência | `Option<T>` com some/none; sem null/undefined universal | sentinela universal; pointer null por default |
| D2-243 | estado de memória | definite init e move no compiler; `MaybeUninit<T>` unsafe | gravar none após move; uninitialized como valor comum |
| D2-244 | controle Option | `?.`, lazy/right-associative `??` e postfix `?` só para none | force unwrap; postfix `?` para Result |
| D2-245 | ownership Option | binding owned por default; `ref`/`inout`/`copy`; `take()` esvazia | copiar payload owned; mutation por optional chain |
| D2-246 | Result | enum T0 success/error para storage e composição | Result implícito só em debug; exceptions abertas |
| D2-247 | `try` | propaga `throws E` ou `Result<T,E>`; cada closure é outro effect scope | postfix `?` para ambos; propagação implícita |
| D2-248 | error type | enum fechado e estruturado; `throws E` sempre tipado | throws sem tipo; string obrigatória |
| D2-249 | effect polymorphism | generic `E: Error`; `Never` especializa como nonthrowing | keyword `rethrows`; erasure universal |
| D2-250 | catch | ordem lexical, guard e exaustividade no contexto nonthrowing | ranking de catches; catch implícito |
| D2-251 | uso de valores | todo valor non-unit/non-Never deve ser usado ou descartado com `let _` | annotation must-use; ignorar Result |
| D2-252 | lowering de error | tagged result e cleanup edges; trace sidecar não observável | host exception unwind; sem trace estruturado |
| D2-253 | fault boundary | process, Wasm instance ou compartment com teardown próprio | service lógico sempre recuperável; panic capturável |
| D2-254 | panic | payload limitado, code estável e sem user cleanup garantido | payload alocável obrigatório; user recovery |
| D2-255 | OOM | alocação normal pode panic; APIs `try*` retornam AllocationError | toda alocação fallible; emergency handler universal |
| D2-256 | cleanup | saídas estruturadas e cancel executam LIFO; panic não garante user cleanup | panic unwind; defer que propaga error |
| D2-257 | diagnostic | code estável, spans em bytes, facts e relação root/cascade | texto livre como API; reutilizar code |
| D2-258 | fix e policy | edits com applicability/digest; ordem estável; error não suprimível | fix sem precondition; source suppression na DB2 |
| D2-259 | `try?` | converte falha recuperável em Option e flatten; não captura panic/cancel | excluir o sugar; `try!`; preservar error oculto |
| D2-260 | const context | `const`, value argument, contract, fixed size, unit e refinement exigem avaliação | confiar no optimizer; executar tudo em compile time |
| D2-261 | const callable | `const fn` e `const init` explícitos; mesma semântica runtime | inferir API pelo body; função exclusiva da fase |
| D2-262 | modifier const | depois de `static`; incompatível com unsafe/async; combina com mut/take | annotation; `comptime fn`; combinação irrestrita |
| D2-263 | const-safe | local mutation, loops, recursion, dados e typed errors; sem capabilities/FFI | subset expression-only; executar host code |
| D2-264 | fase | sem `isComptime`; mesmo input produz o mesmo valor nas duas fases | branch por fase; implementação separada |
| D2-265 | const failure | error não tratado, panic e quota viram diagnostics W-CONST | fault boundary no compiler; AllocationError catchable |
| D2-266 | ConstRepresentable | predicate derivado para valores estruturais sem identity/authority | protocol implementável; qualquer tipo serializável |
| D2-267 | materialização | const sem owner; uso owned cria valor independente; borrow não escapa | singleton mutable; endereço estável público |
| D2-268 | target | evaluator usa target e módulo `w.target`; nunca a máquina host | host semantics; target facts implícitos |
| D2-269 | build input | módulo gerado e recipe declarada; sem env/file/clock no evaluator | `#define`; env intrinsic; acesso sandboxed ad hoc |
| D2-270 | quotas | steps, heap, depth e result na recipe; wall clock não semântico | quota por source; sem limite; timeout como semântica |
| D2-271 | cache const | chave inclui ConstIR, args, target, bundles, evaluator, quotas e generated modules | cache por source text; omitir target |
| D2-272 | type builder | identidade declarada + const parse/refinement; sem função que retorna Type | `type(regex)`; type function arbitrária |
| D2-273 | geração | ConstIR para ConstValue; codegen em tool target; WLO continua codec | stringify/reparse; macro AST universal |
| D2-274 | feedback | PGO declarado só orienta otimização; nunca altera const/tipo/interface | substituir const com execução anterior |
| D2-275 | implementação const | evaluator HIR antes de MLIR; folding MLIR não define correção | JIT host; canonicalizer como evaluator semântico |
| D2-276 | bootstrap const | CE0 no seed C e core W0; ConstValue normalizado deve coincidir | excluir const fn do seed; evaluator só no compiler final |
| D2-277 | force expression | sem `comptime expr` na baseline; binding const nomeia o resultado | keyword obrigatória; const block na v0 |
| D2-278 | static argument | predicate estrutural sem float/dynamic collection; serialização canônica na identidade | qualquer ConstValue; somente integer |
| D2-279 | const e overload | const não distingue call shape; elegibilidade não promete termination/quota | overload por fase; inferir const por call |
| D2-280 | generic kinds | type e `const`; sem lifetime/effect/HKT/pack no source | kinds extensíveis; template sem kind |
| D2-281 | generic labels | type positional; `const` nomeado; `const _` cria slot primário posicional | todos posicionais; named type args |
| D2-282 | generic scope | parâmetros entram em scope da esquerda para a direita | lista inteira em scope; forward reference |
| D2-283 | protocol composition | `P & Q`, sem ordem e com normalização | `P, Q`; `T<[P, Q]>`; composite sempre nomeado |
| D2-284 | generic body | verificado uma vez contra constraints; lookup fechado | template com lookup tardio; verificar só após instantiation |
| D2-285 | generic inference | depois da forma de call; argumentos, receiver e expected result; solução única | ranking; busca por tipo conforme; body inference |
| D2-286 | explicit generic args | type prefix e `const` labeled podem compor com inference; sem `_` | placeholders; lista completa obrigatória |
| D2-287 | primary associated type | protocol head declara projection de `Self`; aplicação restringe o witness | generic protocol por conformance; somente body |
| D2-288 | associated witness | `alias` explícito; sem inference/default/GAT na DB2 | inferir por method; associated type default |
| D2-289 | coherence | conformance no módulo do type ou protocol; escolha única por par | orphan livre; seleção por import |
| D2-290 | conditional conformance | `extension<T: P> Nominal<T>: Q`; sem overlap ou specialization | blanket conformance; prioridade |
| D2-291 | default witness | somente o módulo do protocol publica; seleção gravada na conformance | extension importada muda witness |
| D2-292 | existential compatibility | sem generic method, Self externo ou associated type não ligado | aceitar tudo com traps; banir existential |
| D2-293 | existential opening | `any P` não conforma a P e não abre implicitamente | self-conformance; implicit opening |
| D2-294 | opaque identity | `some P` preserva um tipo por instantiation; occurrence de parâmetro é independente | existential; união de returns |
| D2-295 | generic lowering | monomorphization, shared body e witness são escolhas equivalentes | monomorphization universal; erasure universal |
| D2-296 | generic interface | signature, witness requirements e HIR generic por digest/CAS | reparse de source; somente machine code |
| D2-297 | generic termination | grafo finito, quotas de instance/depth e cache completo | expansão sem limite; timeout semântico |
| D2-298 | generic variance | type constructors invariantes por default | variance inferida; covariance de Array |
| D2-299 | bootstrap generics | constraints, primary associated types, coherence e monomorphization; sem any/some | seed sem protocols; runtime dictionaries |
| D2-300 | enum subset | enum possui slot primário `cases`; `Enum<[.a, .b]>` | enum base + guard; anonymous union |
| D2-301 | subset normalization | conjunto por ordem de declaração; duplicata/empty rejeitados; all vira base | StaticList ordenada na identity |
| D2-302 | subset conversion | subset→superset/base implícito; base→subset checked | cast implícito nos dois sentidos |
| D2-303 | subset flow | switch usa case-set e flow narrowing elimina checks | exhaustividade sempre pelo enum base |
| D2-304 | subset payload/layout | payload preservado; layout público do enum base; tag interno pode sumir | wrapper/tag novo; payload subset |
| D2-305 | subset evolution | retorno widening e parâmetro narrowing são major | qualquer mudança minor; variance automática |
| D2-306 | subset de error | `throws Enum<[...]>`; throw e catch usam o case-set publicado | error enum inteiro; effect union separado |
| D2-307 | planos de introspecção | interface/HIR para tooling; descriptor opt-in no runtime | runtime metadata universal; debug como API |
| D2-308 | type identity | `reflect.TypeId` local ao build; sem persistência ou layout | ID estável global; nome como identidade |
| D2-309 | metatype | sem `Type<T>`/`T.type`; generic, factory ou enum | metatype universal; dynamic construction |
| D2-310 | reflection trigger | conformance explícita a `reflect.Reflectable`; sem annotation | inferir por uso; decorator; registro manual |
| D2-311 | reflection visibility | somente interface exportada e properties lógicas | fields privados; backing storage; getter por string |
| D2-312 | reflection reachability | witness alcançável mantém descriptor; sem registry global | todos os conformers como roots |
| D2-313 | synthesis trigger | conformance no type head; protocol reconhecido por identidade | `@derive`; macro; nome textual |
| D2-314 | synthesis scope | Equatable, Hashable, Duplicable e Reflectable em struct/enum; Reflectable em object | qualquer protocol; Display/codec automáticos |
| D2-315 | synthesis witness | all-or-none por protocol; constraints explícitas | completar witness parcial; inferir constraints |
| D2-316 | rest syntax | último `T...`; zero ou mais; um label inicial | `params`; `*args`; overloads por aridade |
| D2-317 | rest shape | conjunto infinito deve ser disjunto de todo overload | fixed vence rest; ranking por tipos |
| D2-318 | rest binding | `Arguments<T>` não escapante; mode por elemento | Array alocado obrigatório; tuple runtime |
| D2-319 | rest expansion | `each collection` somente no argumento final | `values...`; spread universal; expansão implícita |
| D2-320 | rest ownership | value/ref/take; sem `inout`; cleanup por elemento | ownership apagado; inout dinâmico |
| D2-321 | C varargs | adapter unsafe tipado ou `c.vaList`; rest W não cruza ABI | mapear rest diretamente; promotions implícitas |
| D2-322 | formas type-level adiadas | property path, GAT e heterogeneous packs continuam Pesquisa | incluir no W0; reflection por string |
| D2-323 | resolução de enum case | `.case` exige expected enum; `Enum.case` resolve colisão | escolher por import, frequência ou ranking |
| D2-324 | sequência e case-set | o head decide: `StagePath<[...]>` preserva ordem; `Enum<[...]>` normaliza conjunto | tratar toda static list como conjunto |
| D2-325 | enum e flags | enum representa uma alternativa; simultaneidade usa Set ou tipo de flags separado | enum com semântica AND/OR contextual |
| D2-326 | álgebra de case-set | somente na HIR; source nomeia a lista resultante | operadores públicos de union/intersection/difference na DB2 |
| D2-327 | dois estados | enum em storage para runtime; argumento `const` de enum para typestate local | typestate universal; enum runtime universal |
| D2-328 | argumento const enum | slot primário aceita `.case`; slot normal usa `label: .case` | marker type vazio; string; annotation |
| D2-329 | transição typestate | extension especializada + `take fn`; novo tipo no retorno | mudar tipo do binding no lugar; pre/post annotations |
| D2-330 | falha consuming | outcome enum devolve cada novo owner; `throws` não restaura owner | rollback implícito; owner escondido no error |
| D2-331 | path estático | `StaticList<Enum>` refinada por `const fn`; primeiro edge inválido vira diagnostic | lista sem validação; DSL obrigatória |
| D2-332 | estado de service | enum persistido + snapshot revisionado; closed turn por call | `ServiceRef<State>` muda depois da call |
| D2-333 | erasure de typestate | envelope enum explícito para collections mistas | `T<?>`; existential implícito; tag escondida |
| D2-334 | DSL de transição | sem keywords novas; `StateGraph<E>` declarativa em Pesquisa | `state`/`transition` na DB2; annotations |

Uma revisão pode responder por ID. Uma mudança deve atualizar o exemplo, a
grammar, o formatter, o corpus e a seção semântica correspondente.
