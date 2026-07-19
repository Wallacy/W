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
em [W-O044](../STATUS.md): `@repr(c)` é apenas uma ilustração antiga, não uma
decisão. A preferência atual é uma construção/modifier próprio da fronteira,
caso consiga evitar annotations genéricas sem perder composição.

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

### Protocols e existentials

`protocol` descreve operações e associated types. Usar um protocol como tipo dinâmico pode exigir existential box/vtable; usar como generic constraint permite especialização estática.

```w
protocol Hashable {
  fn hash(into hasher: inout Hasher)
}

fn index<T: Hashable>(_ value: ref T): Hash
```

O call site e metadata devem permitir descobrir quando dispatch é dinâmico. Protocols não injetam storage invisível.

#### `Any`, existential e reflection

Estes conceitos não devem ser fundidos:

| Forma | Tipo concreto conhecido por | Dispatch/custo esperado |
|---|---|---|
| `T: P` | caller e compiler | generic, normalmente especializável |
| `some P` | implementação e compiler | identidade preservada, escondida do caller |
| `any P` | somente runtime | existential com witness table e inline/box |
| `Any` | somente runtime | type erasure total; exige downcast antes de operar |

`some P` e `any P` são sintaxe de trabalho, não decisão. A recomendação de
máquina para [W-O043](../STATUS.md) é manter `Any` como escape hatch raro, não
como base dinâmica da linguagem:

- `Any` owns o valor erased por default; borrows não viram owners;
- erasure pode usar payload inline ou box sem mudar o tipo lógico;
- `TypeId` identifica tipos apenas dentro do mesmo universo de toolchain/
  artefato; não é schema ID, nome público nem identidade serializável;
- IPC, persistência e packages usam uma identidade de schema versionada,
  separada de `TypeId`;
- metadata runtime é emitida por reachability para operações realmente usadas:
  type check, move/drop e witness tables aplicáveis;
- `Any` não ganha `copy`, equality, hash, serialization ou `Send` universais;
  cada operação exige uma constraint/capability comprovada;
- conversão para `Any` pode ser contextual; downcast e extração são sempre
  explícitos e nunca fazem cópia escondida.

API ilustrativa, ainda **Em aberto**:

```w
let payload: Any = take order

if let order = payload.ref(as: Order) {
  print(order.id)
}

let order = payload.take(as: Order) // Order?; consome somente no sucesso
```

Alternativas preservadas:

| Alternativa | Ganho | Armadilha principal |
|---|---|---|
| somente generics/protocols na v0 | core menor e mais estático | empurra type erasure incompatível para cada biblioteca/FFI |
| separar `T`, `some P`, `any P` e `Any` | custo e autoridade ficam distinguíveis | quatro conceitos precisam de ensino e diagnostics bons |
| tornar todo protocol automaticamente existential | source menor | dispatch/boxing ficam invisíveis e generics se tornam ambíguos |
| reflection completa em todo tipo | plugins/serializers muito flexíveis | metadata, encapsulamento, stripping e ABI viram custo universal |

Permanecem três decisões humanas para W-O043:

1. `Any` deve ser um escape hatch deliberadamente raro, mantendo generics e
   protocols como caminho normal?
2. A conversão contextual `T → Any` pode ser implícita quando a assinatura já
   declara `Any`, com boxing mostrado pelo tooling, ou deve exigir uma palavra no
   call site?
3. O custo de dispatch merece `any P` explícito, distinguindo-o de `T: P` e
   `some P`, ou o nome `P` sozinho deve poder significar existential?

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
- constraints não são usadas para esconder allocation/storage policy.

Refinements complexos por regex ou função arbitrária ficam atrás de um protótipo do evaluator hermético.

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

Pointer tagging pode compactar `Option<ref T>`, `Any`, small integers ou runtime metadata em targets compatíveis. Não deve:

- reduzir bits de `f64` silenciosamente;
- mudar range/overflow de `Int`;
- depender de bits de ponteiro não garantidos sem runtime capability check;
- misturar bitfield non-atomic com acesso atomic;
- vazar para ABI C;
- deixar targets com pointer authentication, sanitizers ou capability pointers sem fallback.

Pela política candidata W-C029:

- não existe annotation de compactação no source;
- boxing de `Any` é permitido e precisa ser reportável;
- `Option<ref T>` não aloca para representar `.some`/`.none`, mas seu tamanho e
  alinhamento permanecem propriedades publicadas do target/profile.

Veja [research/tagged-values.md](../research/tagged-values.md).

## Questões que o primeiro protótipo deve responder

1. Quantas anotações `take`/`copy` aparecem em programas reais com last-use inference?
2. O diagnóstico de alias/inout consegue sugerir correções locais?
3. Typed throws e ownership podem compartilhar uma calling convention eficiente?
4. Como task frames guardam owned values e executam destruction no cancelamento?
5. `shared T` é necessário no primeiro slice ou regiões/owners cobrem os exemplos?
6. Qual subset de refinement é decidível e útil sem evaluator complexo?
7. Qual é o custo real de UTF-8 views e quais operações ficam na stdlib?
8. Quais wrappers C conseguem ser gerados de headers e quais exigem annotations humanas?

O resultado pode mudar a sintaxe candidata. Não pode mudar as invariantes sem uma decisão explícita de produto.
