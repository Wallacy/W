# Tipos, valores e memória

> **Status:** modelo candidato; precisa de implementação e provas por exemplo
> **Data:** 18 de julho de 2026

O objetivo não é inventar um único truque de memória que resolva todos os programas. É oferecer uma semântica pequena e previsível que permita ao compilador escolher stack, ownership, regiões, ARC ou chamadas C sem mudar o que o source significa. A estratégia de implementação e seus critérios estão em [Estratégia híbrida de memória](../design/memory-strategy.md).

## Invariantes

Em Safe W:

1. nenhuma leitura observa valor não inicializado;
2. nenhuma referência sobrevive ao valor referenciado;
3. no máximo um acesso mutável existe enquanto aliases podem observar o mesmo storage;
4. liberar um valor ocorre uma vez e em ponto determinístico;
5. uma data race não é um resultado válido do programa;
6. layout e overflow não mudam silenciosamente entre debug/release;
7. uma otimização target-specific sempre tem representação semanticamente equivalente de fallback;
8. cruzar uma fronteira que não preserva essas garantias é explícito.

Essas são propriedades da linguagem. Tagged pointers, ARC, mimalloc, caller allocation e arenas são estratégias de implementação.

## Categorias de tipo

### Primitivos

```text
Bool
i8 i16 i32 i64 i128
u8 u16 u32 u64 u128
f16 f32 f64
Int UInt
Rune? (aberto; provavelmente UnicodeScalar na stdlib)
```

Inteiros de largura fixa têm layout previsível. `Int`/`UInt` usam a largura natural do target e não devem aparecer em wire formats ou ABI estável sem conversão.

Floating point segue formatos IEEE suportados pelo target. Operações aproximadas, fast-math e contração FMA precisam ser policies explícitas; um build de release não pode ativá-las silenciosamente.

`c.int`, `c.long`, `c.size`, `c.char` e outros tipos específicos de ABI vivem no namespace da FFI. Eles não são aliases portáveis de `i32`/`i64`.

### Structs

`struct` é um product type com semântica de valor:

```w
struct Coordinate {
  latitude: f64
  longitude: f64
}
```

Copiar um struct produz valor logicamente independente. O optimizer pode usar move, scalar replacement, registers ou shared immutable backing quando isso é indistinguível.

Layout interno é escolhido por target. A sintaxe de fronteira permanece aberta
em [W-O044](../STATUS.md); W-C036 elimina `@repr`/annotations genéricas em favor
de uma construção/modifier próprio da fronteira.

### Enums

`enum` é um sum type fechado:

```w
enum Packet {
  ping
  text(String)
  data(kind: u16, body: Bytes)
}
```

A semântica é tag + payload, mas a representação pode usar niches, pointer tagging ou layout expandido. A escolha nunca altera cases, precisão numérica ou reflection metadata.

### Objects

`object` tem identidade, storage e lifetime:

```w
object Cache {
  var entries: Map<Key, Value>
}
```

Baseline: owner único. Atribuição/call pode mover ownership; borrows dão acesso temporário. Compartilhamento prolongado exige uma construção explícita ainda aberta (`shared T`, região ou owner de serviço).

“Reference type” não significa automaticamente “ARC em toda atribuição”. ARC é uma implementação candidata apenas para valores semanticamente shared.

### Property behaviors

Storage sintetizado por uma propriedade é uma questão distinta de protocol
existential. [W-O097](../STATUS.md) pesquisa um `behavior` tipado que declare
init/get/set/modify e seja expandido para storage verificável pelo compiler,
mantendo o tipo lógico da propriedade. A proposta, alternativas e limites de
efeitos estão em [property-behaviors.md](../research/property-behaviors.md). Ela
ainda não pertence à grammar nem decide layout, ownership ou concorrência.

### Protocols e existentials

`protocol` descreve operações e associated types. Usar um protocol como tipo dinâmico pode exigir existential box/vtable; usar como generic constraint permite especialização estática.

```w
protocol Hashable {
  fn hash(into hasher: inout Hasher)
}

fn index<T: Hashable>(_ value: ref T): Hash
```

O call site e metadata devem permitir descobrir quando dispatch é dinâmico. Protocols não injetam storage invisível.

#### Existential, erasure e reflection

Estes conceitos não devem ser fundidos:

| Forma | Tipo concreto conhecido por | Dispatch/custo esperado |
|---|---|---|
| `T: P` | caller e compiler | generic, normalmente especializável |
| `some P` | implementação e compiler | identidade preservada, escondida do caller |
| `any P` | somente runtime | existential com witness table e inline/box |

W-C033 adota `any P`. Escrever somente `P` seria menor e não confundiria o
parser em posição de tipo, mas esconderia na leitura humana que a identidade
concreta foi apagada. O token aparece em storage/assinaturas, não no call site,
e não implica que haverá heap allocation: inline storage e devirtualization
continuam livres. As duas grafias não coexistem como sinônimos.

W-C030 retira `Any` da superfície v0. Casos comuns têm formas mais precisas:

- heterogeneidade fechada usa `enum`;
- comportamento heterogêneo usa `any P`;
- algoritmos reutilizáveis usam generics;
- JSON e bridges dinâmicas definem `JsonValue`/`DynamicValue` próprios;
- `void*` e handles opacos pertencem a `foreign c`;
- compiler/runtime podem usar um erased container interno sem torná-lo tipo W.

Conversão `T → any P` é implícita quando `T: P` e ownership permite a passagem.
Ela pode usar payload inline ou box, e o tooling reporta o custo. Nenhuma
conversão inversa implícita existe.

[W-O043](../STATUS.md) agora decide somente o mínimo de metadata e operações:

- witness de dispatch, layout, move e drop alcançáveis;
- `copy`, equality, hash, serialization e `Send` apenas quando a constraint os
  exige;
- associated types que precisam estar bound para formar o existential;
- reflection opt-in sem atravessar encapsulamento;
- `TypeId` local ao universo toolchain/artefato, separado de schema ID estável.

Representação candidata W-C034:

| Forma | Representação lógica mínima | Pode alocar? |
|---|---|---:|
| `ref any P` | data address + protocol witness | não |
| owned `any P` inline | payload + value witness + protocol witness | não |
| owned `any P` boxed | box owner + value witness + protocol witness | sim |

O value witness contém somente o necessário para layout, move e drop. `copy`,
equality, hash ou sendability entram apenas se `P` os exige. A identidade do
descriptor pode servir como `TypeId` dentro do processo/artefato, sem gravar nome
ou fields. Um downcast seguro pode comparar essa identidade e retornar option;
ela nunca vira identidade de wire/package.

Nomes, fields e acesso estrutural só são emitidos por reachability quando o tipo
declara conformance a `Reflectable`. O compiler sintetiza os requisitos sem
annotation; uma implementação manual futura continua sendo uma conformance
normal. Reflection respeita visibility e não abre fields privados ao caller.
Debug symbols/source maps são artefatos separados, podem conter mais detalhes e
podem ser removidos sem alterar reflection solicitada pelo programa.

Associated types precisam estar bound quando um método existentialmente chamado
depende deles. A sintaxe exata pertence a W-O050; métodos genéricos ou que usam
`Self` de forma impossível pelo witness não ficam silenciosamente disponíveis.

### Tuplas, funções e collections

```w
(Int, String)
(x: f64, y: f64)
fn(Int): String throws ParseError
[u8; 32]
Array<T>
Slice<T>
Map<K, V>
```

- `[T; N]` é array fixed-size;
- `Array<T>` owns um buffer redimensionável;
- `Slice<T>` é uma view borrowed;
- `Map` é coleção de stdlib, não sintaxe de layout;
- function types incluem effects relevantes.

## Inicialização e estados especiais

### Inicialização

```w
let value: Config

if production {
  value = productionConfig()
} else {
  value = developmentConfig()
}

use(value)
```

O exemplo é válido apenas se todos os caminhos que chegam a `use` inicializam `value` exatamente como permitido. `uninitialized` não é um valor que possa ser comparado ou serializado.

### Option

```w
let user: User? = .none
```

`T?` é `Option<T>`:

```w
enum Option<T> {
  some(T)
  none
}
```

O compiler pode representar `Option<ref T>` com um niche nulo e `Option<Bool>` com bits adicionais; isso não cria `.null`, `.undefined` ou `.empty` como cases extras.

### Empty, null e undefined

- `String()` e `Array()` podem estar vazios: são valores válidos.
- `c.ptr<T>` pode ser nullable na fronteira C e deve ser convertido para `T?`/wrapper.
- dados JS/JSON que distinguem missing/null usam um tipo do adapter, por exemplo `JsonValue.missing`/`.null`.
- nenhum desses substitui definite initialization.

## Tipos refinados

```w
type Percentage = u8 where value <= 100
type Port = u16 where value in 1...65535
type Username = String where Username.isValid(value)
```

Construção a partir de literal pode ser provada em compile time. Construção dinâmica é fallible:

```w
const full: Percentage = 100
let current = try Percentage(input)
```

Regras necessárias:

- a constraint é pura, total e terminante dentro dos limites do compile time;
- o tipo define um erro de validação ou usa `RefinementError` parametrizado;
- operações preservam, enfraquecem ou perdem a prova de forma explícita;
- FFI/deserialize sempre revalida valores não confiáveis;
- o tipo lógico, overflow e aritmética continuam sendo os do tipo base.

Refinements complexos por regex ou função arbitrária ficam atrás de um protótipo do evaluator hermético.

### Refinement como informação de otimização

Um refinement também fornece facts ao optimizer. Em:

```w
type SmallCount = u16 where value in 1...10
```

`SmallCount` continua semanticamente `u16`: conversões, overflow e resultados
não passam a obedecer aritmética `u8`. O range provado pode, porém:

- usar padrões inválidos como niches de `Option<SmallCount>`/enums;
- eliminar checks redundantes;
- escolher instruções/larguras menores e mais lanes SIMD quando equivalentes;
- especializar storage interno para `u8` e reestender para `u16` ao operar;
- escolher buffers GPU compactos quando target e interface concordarem.

O compilador separa dois layouts:

| Contexto | Regra candidata |
|---|---|
| valor SSA/register | largura livre, desde que operações preservem semântica `u16` |
| field de struct materializado | storage/alignment canônicos de `u16` |
| aggregate eliminado por scalar replacement | fields podem estreitar depois que o layout deixa de existir |
| storage interno não escapante | pode compactar se nenhum endereço/layout for observado |
| `Array<SmallCount>` | pode compactar somente quando análise prova que não haverá borrow/raw view/layout boundary |
| `ref`/`inout` ou endereço do elemento | barreira: precisa de storage canônico; não cria proxy temporário escondido |
| export/ABI/FFI/shared memory/persistência | layout canônico ou schema de fronteira explícito |

Assim, `sizeOf<SmallCount>` continua reportando o layout materializável
canônico de `u16`; uma variável eliminada ou um buffer especializado não muda o
resultado. `w explain layout` pode mostrar que um allocation concreto usa bytes
compactos e por que a otimização foi aceita ou bloqueada.

Para SIMD/GPU, estreitar storage não autoriza aritmética diferente. Cada
operação usa range analysis para provar que a lane estreita preserva o resultado
ou estende antes de calcular. Capabilities de storage/alinhamento do target e a
ABI host↔device fazem parte do representation profile.

A infraestrutura existe sem precisar codificar a regra de W no LLVM: MLIR possui
[análise de ranges inteiros](https://mlir.llvm.org/doxygen/IntegerRangeAnalysis_8h.html),
LLVM aceita [range metadata](https://llvm.org/docs/LangRef.html#range-metadata) em
loads/calls e o passe [SROA](https://llvm.org/doxygen/SROA_8cpp.html) consegue
eliminar/promover aggregates. A política de quando storage pode estreitar
continua sendo verificada no dialeto W antes desses lowerings.

## Layout, ABI e resilience

> **Status:** **Em aberto** em [W-O044](../STATUS.md).

Layout de memória, identidade de wire e visibility são contratos diferentes.
`export` não congela offsets; serialization não copia bytes de uma struct; e um
layout compatível com C continua específico do target ABI.

### Proposta de máquina

#### W nativo por default

Uma `struct`/`enum` W comum tem layout canônico consultável para a receita atual,
mas opaco como compromisso entre versões independentes:

- ordem de declaração governa init, drop, reflection e documentação, não exige a
  mesma ordem física;
- compiler pode inserir padding, reordenar storage, usar niches ou scalar
  replacement dentro de uma build compatível;
- `sizeOf<T>`/`alignOf<T>` retornam facts do target/profile atual, não um wire
  format nem promessa para outra versão do compiler;
- exported fields e resilience de API continuam separados em W-O035;
- cruzar módulos nativos por valor exige representation fingerprint compatível,
  a ser fechado em W-O045.

#### Fronteira C sem annotation

Uma declaração dentro de `foreign c` adota automaticamente o layout C do target:

```w
foreign c from "time.h" {
  struct Timespec {
    seconds: c.time
    nanoseconds: c.long
  }

  fn clock_gettime(clock: c.int, time: c.ptr<Timespec>): c.int
}
```

Somente tipos compatíveis com a ABI C podem compor esse layout. A fronteira não
promete bytes iguais entre Windows/POSIX, 32/64-bit ou ABIs diferentes e nunca é
usada como serialization implícita.

#### Newtype transparente

Para preservar identidade W sem custo de ABI, a forma candidata é uma keyword:

```w
transparent struct UserId {
  raw: u64
}
```

Ela exige exatamente um field armazenado e possui layout/calling convention
iguais aos desse field. Métodos e conformances não mudam a representação.

#### O que não entra ainda

- layout W nativo congelado entre versões fica depois de W-O085;
- `packed struct` não entra em safe W v0: fields desalinhados não podem produzir
  `ref` normal e codecs/loads unaligned são mais honestos;
- endianness, offsets de protocolo e persistência usam encode/decode ou schemas,
  não memória transmutada;
- alinhamento especial deve nascer de um caso SIMD/hardware concreto, não de uma
  annotation genérica.

Um modelo resilient semelhante ao de Swift permitiria mudar fields sem recompilar
o cliente por meio de metadata/accessors, mas custa indireção. W começa
source-first e com ABI nativa não estável; não deve pagar nem prometer esse modo
antes de um caso real de biblioteca dinâmica.

### Alternativas preservadas

| Alternativa | Vantagem | Custo/armadilha |
|---|---|---|
| default em ordem de declaração | previsível para low-level | congela oportunidades de packing e vira ABI acidental |
| default opaco + tooling | otimização e evolução | exige `w explain layout` para inspeção física |
| `fixed struct` W já na v0 | plugins/native libs mais diretos | congela uma ABI antes de generics, enums e ownership |
| `packed struct` geral | formatos densos | unaligned borrows, endianness e atomics perigosos |

### Perguntas humanas

1. Você aceita que a ordem física de `struct` W comum seja opaca, embora
   `sizeOf`/`alignOf` e `w explain layout` mostrem o resultado da build?
2. `foreign c { struct ... }` e `transparent struct` parecem construções claras
   o bastante sem `@repr`?
3. Podemos retirar `fixed`/`packed` da v0 e exigir codecs/adapters até aparecer um
   caso de hardware/ABI que realmente não caiba em `foreign c`?

Referências de comparação: [Rust type layout](https://doc.rust-lang.org/stable/reference/type-layout.html),
[Swift library evolution](https://www.swift.org/blog/library-evolution/) e
[Zig `extern struct`](https://ziglang.org/documentation/master/#extern-struct).

## Aritmética

A linguagem precisa separar quatro perguntas:

1. largura/layout do tipo;
2. inferência do literal;
3. promoção/coerção entre operandos;
4. comportamento de overflow.

Baseline:

- sem conversão implícita signed ↔ unsigned que possa perder valores;
- sem redução de largura implícita;
- literais sem tipo são escolhidos pelo contexto ou por default documentado;
- `+`, `-`, `*` têm overflow definido e verificado;
- variantes wrapping são explícitas (`+%`, `-%`, `*%`);
- saturating e overflowing-result podem ser métodos (`addingSaturating`, `addingReportingOverflow`) até justificar operadores.

A alternativa de promover toda soma/multiplicação para uma largura maior deve
ser comparada: reduz overflow intermediário, mas muda ABI, vetorização e custo.
Não é baseline sem benchmark e regra completa para generics.

## Strings e texto

`String` armazena UTF-8 válido. A API diferencia unidades:

```w
text.byteCount
text.scalarCount
text.graphemeCount

text.bytes
text.scalars
text.graphemes
```

Propriedades candidatas:

- buffer contíguo e terminador zero disponível apenas quando pedido pela FFI;
- small-string optimization permitida, não observável;
- slicing por uma view retorna range/slice na mesma unidade;
- normalização Unicode não ocorre silenciosamente;
- comparar canonically equivalent text é operação explícita;
- ICU/utf8proc podem implementar graphemes/normalização via stdlib/target bundle;
- `Bytes` não promete UTF-8 e converte de forma fallible para `String`.

`tree_string`/DAG, rope e piece table são estruturas especializadas. Elas não substituem o contrato geral de `String`.

## Ownership

### Owner único

Todo valor que requer cleanup tem um owner semântico. Quando o owner termina, o valor é destruído, salvo se ownership foi transferido.

```w
let connection = try Connection.open(address)
use(connection)
// cleanup determinístico no fim do scope
```

O compiler pode inferir um move no último uso:

```w
let packet = Packet.data(bytes)
send(packet) // pode mover se packet não for usado novamente
```

Quando a intenção não é óbvia ou faz parte do contrato:

```w
send(take packet)
```

A regra exata de last-use move é [W-O002](../STATUS.md); os exemplos usam `take` nos pontos pedagogicamente importantes.

### Borrow imutável

```w
fn checksum(data: ref Slice<u8>): u64
```

`ref` não owns e não pode escapar do lifetime declarado/inferido. Múltiplos refs imutáveis podem coexistir enquanto não há borrow mutável.

No call site, `ref` é normalmente inferido:

```w
let sum = checksum(data)
```

### Borrow mutável exclusivo

```w
fn normalize(image: inout Image)
normalize(inout image)
```

`inout` é visível nos dois lados. Enquanto a função usa o borrow, nenhum alias pode ler/escrever o mesmo storage de forma conflitante.

### Transferência

```w
fn submit(job: take Job)
submit(take job)
// job indisponível aqui
```

`take` concentra a intenção de transferência em uma operação simples. Um callee
que quer guardar um argumento precisa recebê-lo owned (`take`) ou produzir sua
própria `copy`.

### Cópia

```w
let duplicate = copy original
```

Para values pequenos, `copy` pode ser implícito segundo traits/custo. Para objects/buffers, cópia deliberada é visível. Deep/shallow não deve ser um booleano misterioso: o tipo define o que seu `copy` significa ou oferece uma operação nomeada.

## Shared ownership

Existem casos reais — graphs, caches, callbacks long-lived — em que owner único não basta. Opções a prototipar:

### `shared T` com ARC

- semântica clara de múltiplos owners;
- `weak T?` para cycles;
- retain/release podem ser otimizados pela análise;
- atravessar `spawn` exige thread-safe reference counts e `Send`.

### Região/arena

- vários valores pertencem a um owner comum;
- deallocation em lote;
- referências não escapam da região;
- excelente para request/compilation frames, menos natural para graphs independentes.

### Owner de serviço/isolate

- estado vive enquanto um serviço vive;
- acesso serializado por mensagens/tasks;
- combina lifecycle e concorrência, mas não deve transformar todo módulo em singleton.

Essas opções podem coexistir como mecanismos explícitos. O que não pode coexistir é uma semântica ambígua em que o mesmo assignment às vezes move, às vezes incrementa ARC e às vezes prende o objeto ao heap do módulo sem aparecer no tipo.

## Regiões e budgets

A pesquisa de `module.memory.max`, heaps por módulo e `flush` contém duas ideias diferentes:

1. **budget/capability:** limitar ou medir recursos;
2. **lifetime region:** liberar allocations em conjunto.

Elas devem ser separadas. Um módulo estático pode ter metadata de custo sem possuir um heap. Uma região runtime pode ter budget sem ser um módulo.

Possível direção de API, ainda não sintaxe:

```w
region request(limit: 64 MiB) {
  let model = try parse(payload, in: request)
  respond(model)
}
```

Questões a provar:

- retorno/move para fora da região;
- destructors e foreign allocations;
- async child que ainda usa a região;
- recursão e containers dinâmicos;
- erro por limite e recovery;
- medição estática vs runtime.

## Destruição e cleanup

- locals são destruídos em ordem reversa da inicialização onde observável;
- campos owned são destruídos com seu owner;
- `deinit` não pode falhar por `throws`; errors devem ser tratados/registrados internamente;
- `defer` roda em todas as saídas estruturadas;
- cancelamento coopera com scopes para executar cleanup;
- `panic` pode escolher abort por profile, mas não deixa invariantes de memória parcialmente executadas e depois continuar;
- one-shot process pode deixar o SO recuperar páginas apenas como otimização depois de respeitar efeitos externos necessários (flush, locks, temp files).

## Closures e callbacks

Uma closure precisa de environment explícito na IR. Captures recebem modos:

- copy/value;
- immutable borrow (`ref`);
- exclusive borrow (`inout`, apenas lexical/síncrono);
- ownership transfer (`take`);
- weak/shared, se esses tipos existirem.

Uma closure que escapa não pode capturar borrow local. Uma closure enviada a `spawn` precisa de captures sendable e sem aliases mutáveis.

Callbacks C exigem function pointer + context + destroy callback quando necessário. O wrapper registra qual lado owns o context e em qual thread pode chamar de volta.

## FFI e memória manual

Raw pointers pertencem ao namespace/boundary C:

```w
foreign c {
  type FILE
  fn fclose(file: c.ptr<FILE>): c.int
}
```

Dentro da camada segura, wrappers convertem:

- nullable pointer → `T?`;
- `(ptr, len)` → `Slice<T>` borrow ou `Array<T>` copy/take;
- status/errno → typed error;
- callback/context → closure owned;
- foreign allocator → owner com deallocator correto.

Nunca se chama `free` em memória de allocator desconhecido. Nunca se presume que um C pointer é válido apenas porque é non-null.

Um bloco foreign/manual pode quebrar garantias; isso não contamina toda a linguagem, mas sua API pública precisa restabelecê-las.

## Representação e MLIR

A HIR tipada deve registrar antes do lowering:

- categoria e layout constraints do tipo;
- owner/move/borrow edges;
- initialization state por control-flow path;
- destructor/defer scopes;
- error e cancellation edges;
- sendability/shared state;
- ABI/repr requirements.

O dialeto W/MLIR preserva operações como ownership transfer, borrow scope, construction/destruction e structured task. Só depois das análises elas baixam para stack allocations, memrefs/pointers, calls runtime, async ops e LLVM dialect.

Emitir C cedo demais perderia distinções necessárias e transformaria regras de linguagem em convenções frágeis de comments/names.

## Tagged values não definem a linguagem

Pointer tagging pode compactar `Option<ref T>`, existentials/erasure interna, small integers ou runtime metadata em targets compatíveis. Não deve:

- reduzir bits de `f64` silenciosamente;
- mudar range/overflow de `Int`;
- depender de bits de ponteiro não garantidos sem runtime capability check;
- misturar bitfield non-atomic com acesso atomic;
- vazar para ABI C;
- deixar targets com pointer authentication, sanitizers ou capability pointers sem fallback.

Pela política candidata W-C029:

- não existe annotation de compactação no source;
- boxing de existentials/erasure interna é permitido e precisa ser reportável;
- `Option<ref T>` não aloca para representar `.some`/`.none`, mas seu tamanho e
  alinhamento permanecem propriedades publicadas do target/profile.

Veja [research/tagged-values.md](../research/tagged-values.md).

## Questões que o primeiro protótipo deve responder

1. Quantos marcadores `take`/`copy` aparecem em programas reais com last-use inference?
2. O diagnóstico de alias/inout consegue sugerir correções locais?
3. Typed throws e ownership podem compartilhar uma calling convention eficiente?
4. Como task frames guardam owned values e executam destruction no cancelamento?
5. `shared T` é necessário no primeiro slice ou regiões/owners cobrem os exemplos?
6. Qual subset de refinement é decidível e útil sem evaluator complexo?
7. Qual é o custo real de UTF-8 views e quais operações ficam na stdlib?
8. Quais wrappers C conseguem ser gerados de headers e quais exigem adapter declarations/overrides humanos?

O resultado pode mudar a sintaxe candidata. Não pode mudar as invariantes sem uma decisão explícita de produto.
