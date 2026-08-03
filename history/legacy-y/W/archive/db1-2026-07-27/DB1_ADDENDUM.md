# Addendum de proveniência à Baseline de Design 1

> **Status:** **Em aberto** · W-O100–W-O103
> **Data:** 22 de julho de 2026

A revisão H01–H14 fechou coerentemente as 97 questões então catalogadas, mas a
auditoria integral de [`Y/WIP.MD`](../../WIP-audit.md) encontrou quatro famílias
que não haviam sido comparadas com a mesma profundidade. Este addendum corrige a
alegação de completude; ele não desfaz silenciosamente nenhuma decisão ratificada.

As quatro famílias são:

1. domínios/executors de execução e sua relação com modules, services e tasks;
2. entrypoints orientados a eventos e profiles de host;
3. a superfície de matrizes/tensors/ML, além de apenas listar `Tensor` no T2;
4. aplicação de tipos com `<...>`, parâmetros compile-time e sua relação com
   newtypes, refinements e storage.

Nenhum sketch neste documento já faz parte da grammar. As recomendações são a
posição técnica inicial; as alternativas permanecem visíveis para a próxima
ratificação humana em lote.

## Resumo das recomendações

| Questão | Recomendação técnica inicial | O que não deve ser confundido |
|---|---|---|
| W-O100 · execução | executor é recurso lógico; isolation pertence a service/entry; tasks podem herdar ou selecionar placement | executor ≠ thread; task group ≠ executor; affinity ≠ serialização |
| W-O101 · entries | `entry` liga funções comuns a slots versionados de um host profile; build escolhe o start principal | `export` W ≠ entry de host; signal handler ≠ código executado no contexto do signal |
| W-O102 · tensor/ML | arrays/views no T0, tensor/linalg no T2; shape preservado; nested literal primeiro; transfer, broadcast e modo numérico observáveis | notação matricial ≠ stack ML; tensor lógico ≠ buffer/layout/device |
| W-O103 · parâmetros/refinements | `<...>` recebe somente argumentos declarados de tipo/valor; um predicado de refinement restringe valores; `type` cria identidade; storage usa tipo próprio | `String<...>` não é newtype, hint, validator e layout ao mesmo tempo |

## A01 · Domínios de execução

### A pergunta recuperada

O WIP experimentava `.main`, `.background`, `.network`, `.io`, `.UX`, queues por
módulo, afinidade de CPU e `spawn<.background>`. A intenção é boa: declarar onde
o trabalho pertence e obter serialização ou paralelismo previsíveis sem gerenciar
pthreads. O nome histórico **thread group**, porém, congelaria detalhes que GCD já
evita congelar: uma queue serial pode migrar entre threads e uma queue concorrente
usa uma quantidade dinâmica delas.

### Vocabulário que precisa permanecer separado

| Conceito | Contrato observável | Não promete |
|---|---|---|
| task group | ownership lexical, join, erro, cancelamento e backpressure de children | thread, fila ou isolamento de estado |
| executor | aceita jobs e decide quando/onde executá-los | identidade de thread ou exclusão, salvo contrato serial explícito |
| domínio de isolamento | garante que estado protegido não execute simultaneamente fora das regras | afinidade física permanente |
| executor preference | placement herdável para trabalho não isolado | proteção de memória ou prioridade garantida |
| affinity | requisito raro de host/device para uma thread/CPU/event loop | fairness, throughput ou isolamento por si só |
| priority/QoS | informação de scheduling ou latency class | deadline ou ordem total garantida |

> **Decisão parcial de 22 de julho:** a decomposição acima foi aceita como direção
> semântica. W-O100 continua aberta para decidir a superfície `on`, angular ou
> descriptor e como services/entries declaram isolation.

Essa separação é compatível com a lição de Swift: um serial executor fornece
isolamento; uma task executor preference fornece threads/placement e é herdada
pela árvore estruturada, mas não substitui o executor exigido por um actor.

### Invariantes recomendadas

1. Um módulo estático nunca ganha thread, queue ou executor ao ser importado.
2. Uma instância de `service` ou um `entry` pode ser ligada a um domínio de
   isolamento pelo descriptor/host.
3. Cruzar um domínio isolado é uma call suspensível; exige `await` e valores
   transferíveis, mesmo quando o runtime usa fast path na mesma thread.
4. Trabalho não isolado herda a preference da task pai. Um override vale para a
   subtree estruturada, não cria uma task detached escondida.
5. `async` continua significando concorrência/suspensão. `spawn` continua
   autorizando paralelismo; selecionar um executor serial não pode fazer `spawn`
   mudar silenciosamente de significado.
6. Um executor serial garante exclusão lógica. Só um profile como UI/main pode
   acrescentar afinidade à thread do host.
7. Nomes `network` e `io` descrevem finalidade/capability, não “uma thread de
   rede”. O backend pode usar IOCP, epoll/kqueue, `io_uring` ou outro adapter.
8. O deployment pode reduzir limites ou remapear uma preference, mas não remover
   isolamento, `Send`, ordering ou efeitos observáveis exigidos pelo programa.

### Superfície a comparar

O caminho comum não menciona executors:

```w
async let menu = loadMenu()
spawn let forecast = buildForecast(take history)
```

Quando placement realmente importa, duas grafias compactas merecem o corpus:

```w
// Relação lida como frase.
async on .network let menu = loadMenu()
spawn on .compute let forecast = buildForecast(take history)

// Aplicação contextual ao head, recuperando a ideia `spawn<group>` do WIP.
async<.network> let menu = loadMenu()
spawn<.compute> let forecast = buildForecast(take history)
```

`on` seria contextual. Na forma angular, o head `async`/`spawn` declara que o
argumento é uma `ExecutorPreference`; `.network`/`.compute` são members do profile,
não keywords globais. Isso não torna o construct generic nem aceita um modifier
map livre. A forma angular é compacta e coesa com `fn<C>`, mas pode incentivar
modifier soup; `on` é semanticamente legível, mas acrescenta a relação infixa que
causou desconforto humano. Placement dinâmico continua sendo API explícita.

As duas formas preservam o mesmo contrato: `async` cria um child concorrente com
preference e `spawn` pede capacidade paralela. Um call a estado UI não precisa de
um spawn artificial:

```w
await renderer.show(forecast)
```

O tipo/descriptor de `renderer` já exige o domínio UI. Se o caller estiver nele,
o runtime elimina o hop quando isso preserva ordering; se não estiver, agenda e
suspende.

As declarações abaixo são sketches de alternativas, não duas sintaxes aceitas:

```w
// A. descriptor/manifest; menor core e melhor override de deployment
ui      = serial(host: .ui)
network = concurrent(kind: .io, max: 256)
compute = parallel(max: .availableCores)

// B. açúcar source; melhor leitura quando isolation faz parte da API
service Renderer as RenderApi on ui {
  // ...
}
```

### Alternativas preservadas

| Alternativa | Vantagem | Problema | Posição inicial |
|---|---|---|---|
| somente manifest/descriptor | source portátil e deployment controla recursos | requirement pode ficar distante da API | **Candidato** para definição física |
| `service/entry ... on domain` | isolation/locality visível perto do owner | acrescenta sintaxe e nome precisa ser resolvido | **Candidato** para açúcar |
| `async/spawn on executor` | override local curto e herdável | pode ser abusado como micro-scheduling | **Candidato** somente em ponto de custo real |
| `async/spawn<.executor>` | evita `on`, head declara o kind e recupera o WIP | pontuação extra e risco de transformar `<...>` em canal universal de knobs | **Em aberto** |
| default por módulo | poucas palavras e recupera o WIP literalmente | import passa a sugerir lifecycle/placement; tests e múltiplas instâncias divergem | **Rejeitado por enquanto** como isolation |
| preference default do package/profile | tuning sem mudar API | comportamento indireto e risco de performance cliff | **Pesquisa** com tooling obrigatório |
| afinidade de CPU no source comum | controle low-level | não portátil, piora scheduling e vira promessa ABI/runtime | **Rejeitado por enquanto** fora de profile expert |
| presets globais `main/background/network/io/UX` | familiaridade GCD | nomes não significam a mesma coisa em server, CLI, embedded e GPU | **Pesquisa** como catálogo de host, não keywords |

### Lowering e diagnóstico

HIR precisa distinguir `requiredIsolation`, `executorPreference`, `parallelIntent`
e `hostAffinity`. Um hop registra source/target domain, capture transfer, queue
time e cancellation edge. O compiler deve diagnosticar:

- `spawn on ui` quando `ui` só fornece serial execution;
- capture não-`Send` ao cruzar domínio;
- blocking num executor cooperativo;
- ciclo de calls isoladas conhecido estaticamente;
- override que não satisfaz requirement de affinity/device;
- nome de executor inexistente no profile selecionado.

O runtime inicial ainda pode ter apenas um executor cooperativo e um pool CPU
limitado. O descriptor mantém a abstração para que IOCP/GCD/work stealing sejam
implementações substituíveis, não semântica da linguagem.

## A02 · Entrypoints e profiles de host

### Quatro coisas que o WIP misturava

| Camada | Função |
|---|---|
| `export fn` | torna um símbolo parte da interface W de um módulo/package |
| handler comum | função tipada e testável, sem nome mágico |
| entry descriptor | liga slots de um profile de host aos handlers |
| product/deployment | escolhe descriptor, principal start, capabilities e adapters |

Uma biblioteca pode exportar cem funções sem ser executável. Um handler pode ser
privado ao módulo e ainda ser alcançável pelo descriptor. `main`, `fetch` ou
`mouse` não devem ser procurados por nome em runtime.

### Forma recomendada para o ensaio

```w
async fn run(context: ref process.Context): ExitCode throws AppError {
  // ...
}

async fn handleRequest(
  request: take http.Request,
  context: ref http.Context,
): http.Response throws HttpAppError {
  // ...
}

fn handleSignal(event: process.Signal, context: ref process.SignalContext) {
  // ...
}

entry Restaurant {
  process.main = run
  http.fetch = handleRequest
  process.signal = handleSignal
}
```

### Entry anônimo e programa mínimo

O binding explícito é adequado quando existem vários slots, mas seria um custo
alto demais para o primeiro programa. Esta forma é uma alternativa legítima:

```w
entry {
  print("Hello World")
}
```

Ela não significa “execute statements durante import” nem procura uma função
`main`. O product/profile precisa oferecer exatamente um slot default — em um
programa CLI, normalmente `process.main` — e o compiler sintetiza um handler
privado com a assinatura completa daquele slot. Se não houver default único, o
diagnóstico exige `profile.slot = handler`. O bloco anônimo não pode misturar
statements e bindings.

A ausência do nome é o discriminador gramatical: `entry { ... }` contém somente
o body do handler default; `entry Nome { ... }` contém somente bindings de slots.
Assim, um assignment comum dentro do body não precisa ser distinguido de um
binding de host por resolução semântica ou heurística do parser.

Essa regra mantém lifecycle explícito pela palavra `entry`, mas ainda deixa duas
políticas abertas: se um body `Void` implica saída bem-sucedida e como argumentos,
context e errors ficam disponíveis sem criar variáveis ambientes mágicas.

O exemplo com closure torna outra pergunta visível. A baseline atual de função
anônima é `(args) => body`, portanto a forma coerente seria:

```w
entry {
  process.main = (context: ref process.Context) => {
    print("Hello World")
    return .success
  }
}
```

`(): { ... }` não é candidata inicial: `:` já introduz return type e labels, e
`{...}` depois dela poderia ser lido como type/record ou body. `fn(...) { ... }`,
Swift-like `{ args in ... }` e um block contextual continuam em [W-O052](STATUS.md).
O açúcar `entry { statements }` não depende de escolher uma segunda closure syntax.
Do ponto de vista do frontend, `(args) => body` permanece a recomendação inicial:
é uma expressão delimitada, separa parâmetros do body e não exige decidir pelo
contexto se qualquer `{ ... }` é block ou closure.

O bloco é uma lista de bindings declarativos, não um record literal; portanto não
precisa de vírgulas. Cada lado esquerdo resolve um slot versionado com assinatura,
efeitos, lifecycle, executor/isolation e capabilities definidos. O lado direito
é uma função comum sobre a qual doctests e unit tests continuam funcionando.

Um slot não é uma string global. Ele possui identidade de símbolo no SDK/package,
versão e fingerprint de contrato fixados pelo lockfile; aliases apenas encurtam a
grafia no source. Dois providers podem publicar um `http.fetch` textual sem colidir,
e o adapter do produto precisa implementar exatamente o profile resolvido ou uma
conversão declarada. Isso impede que atualizar um host mude silenciosamente a ABI
ou as capabilities de um handler.

Slots adicionais não exigem keywords novas:

```w
entry RestaurantDesktop {
  process.main = runDesktop
  ui.pointer = handlePointer
  ui.keyboard = handleKeyboard
  device.hid = handleHid
}
```

`cli(stdin)` não define implicitamente “uma call por linha”. Um profile CLI pode
fornecer `stdin` como `Stream<Bytes>` ou `Stream<String>`; um host que entregue
linhas é outro slot com framing explícito. Da mesma forma, um adapter de signal
executa apenas a captura async-signal-safe e depois agenda um `process.Signal`
normal; nunca chama código W arbitrário dentro do handler nativo do SO.

### Context é capability tipada

O `ctx` histórico não deve ser um dicionário universal. Profiles compõem tipos
pequenos e testáveis:

- argumentos, exit e stdio para `process.main`;
- request, response/body streams, deadline e cancellation para `http.fetch`;
- bindings/environment explicitamente concedidos;
- task scope do entry;
- clocks, random, filesystem, network ou storage somente quando importados pelo
  profile/deployment.

O host pode oferecer uma facade conveniente, mas o HIR e a recipe conhecem cada
capability alcançável.

### Seleção do principal

Um package pode conter vários descriptors e gerar vários products. O build
seleciona sem exigir um `main` C dentro de cada compilation unit:

```text
product restaurant-cli
  entry = Restaurant
  start = process.main

product restaurant-worker
  entry = Restaurant
  start = http.fetch
```

Essa é pseudoconfiguração, não o manifest W escolhido. Para native, o backend
gera o `main`/`WinMain`/adapter necessário. Para WASI Component Model, pode gerar
um world export. Para testes, um host in-process chama o mesmo descriptor. Isso
não exige transformar fisicamente toda compilation unit em `.a`, `.so` ou DLL.

### Alternativas preservadas

| Forma | Leitura | Evolução/typing | Posição inicial |
|---|---|---|---|
| `export { fn fetch... }` | compacta e semelhante a Worker JS | mistura visibility, body e lifecycle; vírgulas ficam ambíguas | **Rejeitado por enquanto** como entry |
| `entry { fn fetch... }` | tudo perto e responde à ideia histórica | handlers deixam de parecer funções normais/reutilizáveis | **Em aberto** |
| `entry { statements }` para um único slot default | hello world pequeno e lifecycle ainda explícito | depende do product/profile e precisa definir context, exit e errors | **Em aberto**; candidato forte para script/CLI |
| `entry { profile.slot = closure }` | handler local sem nome auxiliar | assinatura/captures podem ficar densas e não há reutilização | **Em aberto** com W-O052 |
| `entry { profile.slot = handler }` | separa contrato, handler e host; slots extensíveis | acrescenta descriptor e qualified names | **Candidato**; recomendação inicial |
| `entry App as ProcessMain, HttpFetch` | conformance familiar e type-safe | pode exigir object/namespace artificial | **Candidato** alternativo |
| manifest-only | nenhuma syntax nova | refactor/diagnostic fica distante do código | **Candidato** para principal/placement, não único binding |
| nomes mágicos `main/fetch/cli/mouse/...` | mínimo de tokens | lista fechada, colisões e comportamento dependente do target | **Rejeitado por enquanto** |

WIT worlds e WASI reforçam a separação: um component declara exports e imports
de um world; o host os fornece/invoca. Cloudflare Workers demonstra a ergonomia
de handlers conhecidos. W pode adotar a ideia sem transformar os nomes atuais de
um fornecedor em keywords eternas.

## A03 · Matrices, tensors e ML

### O diferencial não é apenas uma notação

Uma linguagem não se torna boa para ML por aceitar `[1, 2; 3, 4]`. Para competir
com Python/Julia no authoring e com C++/CUDA no deployment, W precisa manter no
mesmo modelo:

- rank e shape estáticos, simbólicos e dinâmicos;
- ownership, aliasing, views, strides e layout;
- dtype, promotion, rounding, overflow e quantization;
- broadcasting, reductions e ordem numérica;
- dense, sparse e sharded tensors;
- device/address space e custo de transferência;
- kernels, fusion, autodiff e random reproduzível;
- interchange com StableHLO/ONNX sem fazer deles a semântica source de W.

### Camadas propostas

| Camada | Escopo recomendado |
|---|---|
| T0 | `Array<T>`, fixed/inline array a decidir, `Slice<T>`/borrowed view, iterators e bounds safety |
| T2 `tensor` | `Tensor`, `TensorView`, shapes, slicing, reductions, linalg, CPU/SIMD e device protocol |
| T2 experimental `ml` | autodiff, optimizers, model graph, quantization, sparse/sharding e import/export |
| compiler | shape/refinement facts, fusion/vectorization/bufferization e target lowerings |

ML não precisa morar no core para ser first-party. O core precisa oferecer value
parameters/refinements, ownership, operators e compiler hooks suficientes para o
T2 não parecer uma FFI Python.

### Tipo lógico e shapes

O candidato de trabalho é um tensor ranked cujo shape possa misturar extents
estáticos, símbolos e dimensões validadas em runtime:

```w
fn classify<const batch: usize>(
  input: ref Tensor<f32, [batch, 784]>,
  weights: ref Tensor<f32, [784, 128]>,
): Tensor<f32, [batch, 128]> {
  // ...
}
```

Essa forma depende de W-O103; ainda não promove `const` generics nem a gramática
de shape. A alternativa menos dependente é `Tensor<f32, rank: 2>` com extents
refinados no body. A recomendação é **rank conhecido** para kernels compilados,
extents tão precisos quanto os inputs permitem e uma conversão fallible que
estabelece o proof quando os dados chegam dinamicamente. Após a prova, `@` não
deve falhar por shape.

`Tensor` é um valor lógico, não uma promessa de layout. `TensorView` carrega
shape, strides, offset, address space e borrow; uma boundary C/BLAS/device escolhe
um representation contract explícito.

### Literals

A baseline não precisa de pontuação nova:

```w
let transform: Matrix<f32, 2, 3> = [
  [1.0, 0.0, 10.0],
  [0.0, 1.0, 20.0],
]
```

O expected type converte nested arrays retangulares em matrix/tensor e rejeita
linhas de tamanhos diferentes em compile time. Alternativas que ficam no Book:

```w
// Sugar MATLAB/Julia-like; Em aberto
let transform = [1.0, 0.0, 10.0; 0.0, 1.0, 20.0]

// Constructor explícito; sempre pode existir na biblioteca
let transform = try Matrix.from(rows: [[1.0, 0.0, 10.0], [0.0, 1.0, 20.0]])
```

O sugar com `;` é legível para matrix pequena, mas não escala naturalmente a
rank N, reutiliza o terminador opcional de statements e conflita com o antigo
`MultiRange`. Deve ser medido num corpus científico antes de reservar semântica.

### Operadores

| Operação | Candidato | Alternativas preservadas |
|---|---|---|
| elemento a elemento | `+ - * / **` com shape igual; scalar expansion total | operadores pontuados `.*`; métodos nomeados |
| matrix/tensor contraction | `a @ b` como sugar de `matmul(a, b)` | `a * b` linear-algebra + `a .* b`; somente `matmul` |
| broadcasting de rank/extent | `broadcast(to:)` explícito no primeiro corte | regra trailing-dimension implícita da Array API/NumPy |
| transpose/permutation | `transpose`/`permuted` nomeado | `.T`, postfix futuro |
| Einstein summation | `einsum`/`contract` T2 com spec compile-time | DSL de índices na linguagem |

A recomendação de segurança é: scalar expansion pode ser implícita; shapes
diferentes exigem broadcast explícito inicialmente. Broadcasting bem definido
normalmente não copia, mas ainda pode mascarar um eixo errado e multiplicar
trabalho. O corpus ML dirá se a regra Array API merece sugar posterior.

### Ownership, device e execução

- `Tensor` owned é move-first; uma mutation exige exclusividade.
- slicing retorna `TensorView` borrowed por default; `copy()` materializa.
- compartilhar storage mutável exige um tipo/policy explícito, não COW oculto.
- captures de `spawn` obedecem `Send`; large tensors devem mover, borrow sob join
  provado ou usar shared immutable storage.
- host↔device transfer nunca acontece por causa de um `+` aparentemente local.
- um device pode fornecer executor, allocator e compiler target relacionados,
  mas os três são capabilities distintas.

Sketch:

```w
let deviceModel = try await model.to(device)
let deviceBatch = try await batch.to(device)
spawn on device let logits = deviceModel(deviceBatch)
let result = await logits
```

Se `device` não oferece paralelismo no sentido de `spawn`, uma API de submit
assíncrono pode ser mais correta. W-O100 e W-O102 precisam decidir juntos; o
sketch não autoriza esconder transfer ou compilation latency.

### Autodiff, random e reproducibilidade

Autodiff deve começar como transformação explícita de função pura/tipada, não
annotation universal:

```w
let (loss, gradients) = valueAndGrad(lossFor)(parameters, batch)
```

O transform precisa declarar operações diferenciáveis, mutation/alias rules,
controle de nondifferentiable branches e error de unsupported op. Random recebe
um generator/capability com seed observável. `strict`, `reproducible` e `fast`
governam reductions e kernels como já exige W-O037; escolher GPU não concede
`fast-math` silencioso.

### Lowering recomendado

```text
W Tensor HIR
  ├─ shape/refinement verification
  ├─ ownership + alias analysis
  ├─ numeric/reproducibility mode
  ├─ MLIR tensor/shape/linalg/vector
  ├─ bufferization → memref/library ABI
  ├─ CPU/SIMD/BLAS | GPU/SPIR-V/vendor adapter
  └─ StableHLO/ONNX import/export adapters
```

MLIR já separa tensor lógico, shape, structured linear algebra, vector, buffers,
GPU, sparse, quantization e sharding. W deve preservar sua semântica num dialeto
próprio até poder selecionar esses lowerings; mapear todo source diretamente a
StableHLO limitaria systems code e operações fora do seu conjunto.

### Gate de credibilidade ML

A feature só avança quando o corpus contém, no mínimo:

1. matrix multiply com shape error estático e dinâmico;
2. batched inference com dimensão simbólica;
3. view/slice sem cópia e um caso que exige materialização explícita;
4. reduction serial/paralela em modos strict/reproducible/fast;
5. autodiff de um modelo pequeno com gradient check;
6. CPU escalar, SIMD e um device com mesmos tolerances declarados;
7. import/export StableHLO ou ONNX com unsupported-op diagnostic;
8. benchmark que separa compile, transfer, allocation e kernel time.

## A04 · `<...>`, value parameters, refinements e newtypes

### A decomposição necessária

O sketch `String<length, min: 123, max: 4321>` parece pequeno, mas pode tentar
representar cinco contratos diferentes:

| Intenção | Mecanismo recomendado | Exemplo |
|---|---|---|
| identidade nominal | `type` | `type CPF = ...` |
| restrição de valores | `where` | `String where value.scalars.count == 11` |
| família parametrizada | parâmetro compile-time declarado | `BoundedString<min: 1, max: 100>` |
| layout/capacity observável | tipo de storage próprio | `InlineString<capacity: 100>` |
| parsing/UI/format | codec/validator/behavior | `CPF.parse(input, using: codec)` |
| expectativa de uso | profile/resource hint | não participa da identidade do tipo |

Misturar essas dimensões num “modifier registry” por tipo tornaria overload,
ABI, equality, generic specialization, serialization e diagnostics dependentes
de opções que parecem metadata.

### Regra comum às superfícies candidatas

`Type<...>` é aplicação de um declaration generic. Cada posição tem kind conhecido
pelo símbolo resolvido: tipo, valor compile-time ou eventualmente effect/shape
dedicado. Se W adotar `where:` dentro de `<...>`, ele será um único operador de
tipo reservado pelo compiler, não um mapa aberto que cada tipo interpreta.

Sketch de declaração:

```w
type BoundedString<const min: usize, const max: usize> =
  String where value.scalars.count in min...max

type RestaurantName = BoundedString<min: 1, max: 100>
type CPF = String where value.scalars.count == 11 && cpf.isValid(value)
```

O desconforto com o `where` postfix revela quatro superfícies diferentes para a
mesma semântica, todas ainda **Em aberto**:

```w
type ShortA = String where value.scalars.count <= 40
type ShortB = String(where: value.scalars.count <= 40)
type ShortC = String<where: (value.scalars.count <= 40)>
type ShortD = String<where(value.scalars.count <= 40)>
```

`ShortA` tem a grammar menor, mas o scope visual de `where` piora em tipos
aninhados. `ShortB` usa a leitura familiar de constructor e delimitadores claros,
mas parece uma call runtime. `ShortC` localiza a constraint e preserva a família
angular desejada. O parser contextual consegue distinguir o operador relacional
`>` quando ainda há um operando à direita, mas parênteses produzem uma fronteira
mais simples para formatter, diagnostics e futuros operadores angulares. A forma
menos carregada `String<where(value.scalars.count <= 40)>` também entra no corpus.
Entre as angulares, essa última é a preferência técnica provisória: a fase de tipo
fica explícita, o predicate possui delimitadores próprios e `where` é um único
operador reservado que baixa para um tipo refinado no HIR. Isso não autoriza
`String<qualquerModifier: ...>`; argumentos comuns continuam dependentes da
declaração do tipo.

O uso com labels é candidato porque acompanha calls W e torna dois `usize`
legíveis. A forma posicional `BoundedString<1, 100>` continua alternativa. Uma
lista usa vírgula; `String<size;1000>` e grupos separados por `;` ficam
**Rejeitados por enquanto** porque não há semântica clara para o separador.

`length` sozinho também é insuficiente para `String`. A constraint precisa nomear
bytes, Unicode scalars ou grapheme clusters. Um limite lógico não muda o layout
canônico: o optimizer ainda pode compactar storage não observável sob W-C032.

### Definição, prova e validação são fases diferentes

Nenhuma dessas grafias diz que todo valor existe em compile time. A declaração
define o predicate enquanto o compiler constrói o tipo; a aplicação pode ser
provada estaticamente ou validada em runtime:

```w
const title: ShortLabel = "W"       // prova durante a compilação
let title = try ShortLabel(input)   // validação runtime na boundary
```

`comptime` é uma questão ortogonal de [W-O051](STATUS.md): ele serviria para
exigir que uma expressão seja avaliada durante a compilação, como
`let table = comptime buildTable()`. Aplicá-lo ao refinement inteiro seria errado,
porque valores vindos de rede, arquivo ou usuário ainda precisam ser validados.
O predicate usa um subset puro, hermético e total; o ponto de construção decide
se a prova foi estática ou se um check fallible permanece.

### Por que `String` não deve receber todos os knobs

```w
// valor UTF-8 owned normal, com invariant lógico
type ShortLabel = String where value.scalars.count <= 40

// representação cujo tamanho/capacity faz parte do contrato
let buffer: InlineString<capacity: 64>

// otimização de uma instância, sem criar outra identidade de String
let text = String(reserving: 4096)
```

`InlineString<64>` pode ser um tipo T0/T1 com layout documentado; `ShortLabel`
continua semanticamente String nominal refinada e permite storage especializado
apenas onde não observável. `expected: 1000` pertence ao constructor, profile ou
lens de recursos, nunca à igualdade de tipos.

Máscaras de CPF, teclado numérico e apresentação pertencem a codecs/UI. O tipo
`CPF` protege o dado; um formatter decide `123.456.789-00`; um input adapter pede
teclado numérico. Assim, usar CPF num servidor não arrasta uma policy de UI.

### Relação com newtype e `fn<lang>`

Esta recuperação **não reabre** a escolha ratificada:

```w
type UserId = u64   // identidade nominal, mesmo storage lógico
alias NativeId = u64 // sinônimo
```

`BoundedString<...>` é uma aplicação generic; somente `type RestaurantName = ...`
cria a nova identidade nominal. Os mecanismos se compõem em vez de competir.

`fn<C>` também não transforma `<...>` num modifier registry. Após a keyword `fn`,
essa posição é uma ilha de implementação com grammar própria e provenance; após
um nome de tipo, é uma application generic resolvida. O parser consegue separar
os contextos sem lhes dar a mesma semântica.

### Alternativas preservadas

| Alternativa | Vantagem | Custo | Posição inicial |
|---|---|---|---|
| value parameters declarados + `where` | geral, tipado e otimizável | exige const evaluator/generics | **Candidato**; recomendação inicial |
| `T(where: predicate)` reservado em type position | delimitador claro e leitura de constructor | parece runtime call e mistura parenteses de tipo/valor | **Em aberto** |
| `T<where: (predicate)>` ou `T<where(predicate)>` como operador universal fechado | coeso com `<...>` contextual e scope local | interação com `>` exige parsing contextual ou delimiter; adiciona uma forma compiler-reserved | **Em aberto** |
| modifier map livre em todo `Type<...>` | muito compacto | semântica ad hoc, ABI/overload e diagnostics frágeis | **Rejeitado por enquanto** |
| apenas aliases nominais concretos | frontend menor | duplica tipos bounded e limita shapes/fixed arrays | **Candidato** mínimo de bootstrap |
| dedicated types (`InlineString`, `Matrix`) | custos físicos visíveis | mais nomes na biblioteca | **Candidato** recomendado para layout observável |
| wrapper/property behavior | composição de validation/access | pode esconder fallibility/storage | **Candidato** quando access semantics, não type invariant |
| labels em generic args | leitura e refactor melhores | grammar e canonicalization adicionais | **Em aberto** |

## Perguntas para ratificação em lote

### W-O100 · execução

1. **Respondida: sim.** Um módulo nunca tem isolation/thread implícita; o
   equivalente moderno da ideia histórica vive em `service`, `entry` e task.
2. Quer nomes lógicos no source com `async/spawn on .compute`, com
   `async/spawn<.compute>`, ou prefere placement não isolado apenas no
   descriptor/manifest?
3. `service/entry ... on ui` é um açúcar desejável, ou o tipo do handle/host já
   torna a exigência suficientemente clara?

### W-O101 · entries

4. `entry { statements }` deve desugar para o único slot default do product/profile
   em programas pequenos, sem permitir mistura com bindings?
5. Entre as formas completas, `entry { profile.slot = handler }` deve ser a baseline?
6. Profiles devem ser extensíveis/versionados por packages (`http.fetch`,
   `ui.keyboard`) em vez de uma lista de nomes reservados pela linguagem?
7. Um product pode expor vários profiles simultaneamente, com apenas um `start`
   principal por adapter nativo?

### W-O102 · tensor/ML

8. Nested arrays contextuais bastam como baseline e `[a, b; c, d]` fica como
   sugar a medir, ou a notação matricial deve entrar desde a v0 pública?
9. Você prefere `@` + `matmul` para produto matricial e `*` elementwise, ou a
   família MATLAB/Julia `*`/`.*`?
10. Broadcasting entre shapes diferentes e host↔device transfer devem começar
   explícitos, mesmo com mais tokens?
11. Faz sentido separar T2 `tensor/linalg` estável de T2 `ml/autodiff/models`
    experimental, ambos distribuídos oficialmente?

### W-O103 · tipos parametrizados

12. Parâmetros compile-time de valor são centrais o bastante para a v0, cobrindo
    fixed arrays, bounded strings e tensor shapes?
13. Generic arguments de valor devem aceitar labels canônicos, como
    `BoundedString<min: 1, max: 100>`?
14. Qual refinement surface merece o corpus principal: `T where predicate`,
    `T(where: predicate)`, `T<where: (predicate)>`, `T<where(predicate)>` ou outra
    forma?
15. W precisa de `comptime` em expression position na v0, além de `const` e value
    parameters, ou isso pode esperar pelo experimento W-O051?

### W-O052 · closures, por dependência

16. A baseline `(args) => expression/block` deve permanecer, ou `fn(args) {}`,
    `{ args in ... }` ou block contextual é mais coerente com entries e trailing
    closures?

Responder em lote permite atualizar `STATUS.md` sem uma sequência de decisões
locais que se contradigam.

## Fontes primárias de comparação

### Execução

- [Apple — Dispatch Queues](https://developer.apple.com/library/archive/documentation/General/Conceptual/ConcurrencyProgrammingGuide/OperationQueues/OperationQueues.html)
- [Swift SE-0392 — Custom Actor Executors](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0392-custom-actor-executors.md)
- [Swift SE-0417 — Task Executor Preference](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0417-task-executor-preference.md)

### Entrypoints e hosts

- [WebAssembly Component Model — WIT Worlds](https://component-model.bytecodealliance.org/design/worlds.html)
- [WASI 0.3 — worlds `command`, `service` e `middleware`](https://wasi.dev/releases/wasi-p3)
- [Cloudflare Workers — Handlers](https://developers.cloudflare.com/workers/runtime-apis/handlers/)

### Tensors e ML

- [MLIR — `tensor`](https://mlir.llvm.org/docs/Dialects/TensorOps/),
  [`shape`](https://mlir.llvm.org/docs/Dialects/ShapeDialect/),
  [`linalg`](https://mlir.llvm.org/docs/Dialects/Linalg/),
  [`vector`](https://mlir.llvm.org/docs/Dialects/Vector/),
  [`gpu`](https://mlir.llvm.org/docs/Dialects/GPU/),
  [`sparse_tensor`](https://mlir.llvm.org/docs/Dialects/SparseTensorOps/),
  [`quant`](https://mlir.llvm.org/docs/Dialects/QuantDialect/) e
  [`shard`](https://mlir.llvm.org/docs/Dialects/Shard/)
- [OpenXLA — StableHLO specification](https://openxla.org/stablehlo/spec)
- [Python Array API — `matmul`](https://data-apis.org/array-api/2023.12/API_specification/generated/array_api.matmul.html)
- [ONNX — IR specification](https://onnx.ai/onnx/repo-docs/IR.html)
- [MATLAB — `;` em commands e linhas de arrays](https://www.mathworks.com/help/matlab/ref/semicolon.html)
- [Julia — array literals e concatenação multidimensional](https://docs.julialang.org/en/v1/manual/arrays/)
- [Julia — operadores pontuados e broadcasting](https://docs.julialang.org/en/v1/manual/mathematical-operations/)

### Tipos parametrizados e constraints

- [Rust Reference — generic/const parameters](https://doc.rust-lang.org/reference/items/generics.html)
- [Ada 2022 Reference Manual — scalar constraints e static subtypes](https://docs.adacore.com/live/wave/arm22/html/arm22/arm22-4-9.html)
- [Zig Language Reference — `comptime`](https://ziglang.org/documentation/master/#comptime)

### Closures

- [The Swift Programming Language — Closures](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/closures/)
