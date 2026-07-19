# Um passeio pela linguagem W

> **Status:** sintaxe e semântica candidatas para prototipação
> **Data:** 18 de julho de 2026
> **Importante:** exemplos dourados de design, ainda não código executável.

Este passeio começa pela experiência que queremos e desce gradualmente até memória, tasks e C. Quando uma escolha ainda não está madura, o texto diz isso em vez de fingir que já existe uma especificação.

## 1. Primeiro programa

```w
fn main() {
  print("Hello, W!")
}
```

`print` exercita a prelude T0 curada proposta para a DB1. Ele não é keyword e
não esconde I/O: compiler/LSP ainda mostram origem, effect, capability e
reachability. Todo export std único, somente namespaces implícitos e import
explícito permanecem alternativas em [W-O026](STATUS.md).

Arquivos usam UTF-8. Quebra de linha encerra a maior parte das declarações; `;` é aceito apenas onde for necessário interoperar ou colocar statements na mesma linha — a decisão de aceitá-lo como estilo geral ainda será testada pelo formatter.

Um package é dividido em módulos. O caminho do arquivo e o manifest determinam o módulo; uma declaração nominal no source pode ser adicionada depois se resolver um problema real. Imports sempre apontam para nomes lógicos resolvidos pelo manifest e pelo lockfile:

```w
import { http } from std.net
import { User, UserId } from app.models
import geometry as geo from acme.geometry
```

URLs, mirrors e hashes pertencem à resolução do package, não a cada linha de source. Isso mantém o código legível e permite trocar transporte sem trocar identidade.

## 2. Bindings

```w
const maxConnections = 512 // calculado e embutido na compilação
let host = "127.0.0.1"      // binding imutável em runtime
var served = 0              // binding mutável em runtime

served += 1
```

`let` não significa necessariamente “memória const”: significa que o binding não pode ser reatribuído. A mutabilidade interna depende do tipo e do contrato do método. `const` só aceita uma expressão que o ambiente de compilação autorizado consiga avaliar de forma determinística.

Tipos são inferidos quando a informação é inequívoca e podem ser escritos quando fazem parte do contrato:

```w
let retryLimit: u8 = 3
let timeout: Duration = .seconds(5)
var total: Int = 0
```

Tipos inteiros de largura fixa são `i8`, `i16`, `i32`, `i64`, `i128` e as versões `u`. `Int` e `UInt` têm largura natural do target. Layout público, serialização e FFI devem preferir larguras fixas.

Overflow não muda de comportamento entre debug e release:

```w
let checked = a + b  // falha de forma definida se transbordar
let wrapped = a +% b // wrapping deliberado
```

A forma exata da falha verificada — typed error, trap ou operação que exige prova — ainda precisa de protótipo. O requisito já é direção: nunca virar undefined behavior silencioso.

## 3. Funções e argumentos nomeados

```w
fn clamp(value: i32, min: i32, max: i32): i32 {
  guard min <= max else panic("invalid bounds")
  return value.max(min).min(max)
}

let opacity = clamp(value: input, min: 0, max: 255)
```

A baseline usa uma única keyword (`fn`) e retorno depois de `:`. Argumentos nomeados fazem parte da assinatura pública. Uma futura convenção pode permitir omitir o label em APIs matemáticas ou locais, mas o formatter e a API metadata continuarão tendo uma forma canônica.

Defaults são avaliados no caller:

```w
fn connect(
  host: String,
  port: u16 = 443,
  timeout: Duration = .seconds(10),
): Connection throws ConnectError {
  // ...
}

let api = try connect(host: "api.example.com")
```

Funções genéricas usam parâmetros de tipo comuns, não uma tag de linguagem embutida:

```w
fn first<T>(items: ref Slice<T>): T? {
  if items.isEmpty { return .none }
  return .some(items[0])
}
```

## 4. Tipos de dados que dizem como se comportam

W não trata tudo como uma representação universal. A categoria do tipo comunica semântica; o compilador ainda pode especializar layout e eliminar cópias.

### Struct: valor

```w
struct Point {
  x: f64
  y: f64
}

let origin = Point(x: 0, y: 0)
let alsoOrigin = origin // valor independente; cópia pode ser eliminada
```

### Object: identidade e lifetime

```w
object Connection {
  let socket: Socket
  var state: State = .open

  mut fn close() throws IOError {
    guard state == .open else return
    try socket.close()
    state = .closed
  }

  deinit {
    socket.closeIgnoringErrors()
  }
}
```

Um `object` tem owner e identidade. Ele não ganha compartilhamento global implícito só por ser uma referência. `shared`/ARC é uma das questões abertas; o modelo inicial deve funcionar com ownership único, borrows e transferência.

### Property behaviors: storage tipado, ainda em pesquisa

W-O097 estuda generalizar patterns como lazy, observers, COW e inicialização
tardia sem tornar o wrapper o tipo público da propriedade:

```w
// Hipótese visual; ainda não pertence à grammar.
var Lazy heatProfile = deriveHeatProfile(model)
```

O compiler enxergaria storage, init, accessors, ownership e efeitos em HIR. O
caller continuaria vendo `target: Celsius`. A forma `with`, o `var [behavior]` do
Swift original, wrapper nominal e bloco próprio permanecem alternativas em
[property behaviors](research/property-behaviors.md); nenhuma reabre annotations
genéricas ou autoriza I/O oculto em field access.

### Enum: alternativas exaustivas

```w
enum Message {
  text(String)
  image(url: Url, caption: String?)
  progress(percent: u8)
}
```

Cases podem ser abreviados quando o tipo esperado já é conhecido:

```w
let state: State = .open
let message: Message = .progress(percent: 42)
```

### Protocol: contrato sem storage escondido

```w
protocol Writable {
  fn write(bytes: ref Slice<u8>): UInt throws IOError
}
```

Protocolos descrevem operações e associated types. Adicionar storage invisível por protocolo não faz parte da baseline, pois compromete layout e interop.

### Tipos refinados

```w
type Port = u16 where value in 1...65535
type UserId = u64 where value > 0
type ShortName = String where value.graphemeCount <= 80

const http: Port = 80       // validado na compilação
let port = try Port(input)  // validado em runtime
```

O refinamento é uma restrição lógica do valor. Política de alocação (`inline`, arena, capacity) não deve ser misturada no mesmo parâmetro de tipo.

Na proposta de layout da DB1, `type UserId = u64` cria identidade nominal e
preserva o storage de `u64`; `alias NativeSize = c.size` cria somente um segundo
nome. Isso substitui `transparent struct` no caso comum de newtype, sem afirmar
que qualquer struct W possui ABI C.

## 5. Ausência não é um conjunto de sentinelas

```w
let nickname: String? = .none

if let name = nickname {
  print(name)
}

guard let token = request.token else {
  return .unauthorized
}
```

`T?` é açúcar para `Option<T>` e tem dois estados: `.some(T)` e `.none`.

- **uninitialized** é estado do analisador; ler antes de inicializar é erro de compilação;
- **empty** é um valor do domínio, como `""` ou `[]`;
- **null** só aparece onde uma ABI estrangeira exige um ponteiro nulo;
- **undefined** pertence a formatos/runtimes dinâmicos e deve ser convertido explicitamente.

Essa separação elimina sentinelas por tipo e torna narrowing simples para o compilador e para quem lê.

## 6. Erros recuperáveis são tipados

```w
enum ParseError: Error {
  unexpected(token: String, at: SourceLocation)
  missing(field: String)
  invalidUtf8(offset: UInt)
}

fn parseUser(source: ref String): User throws ParseError {
  let document = try parseDocument(source)
  guard let name = document["name"] else throw .missing(field: "name")
  return User(name: try name.asString())
}
```

`throws ParseError` faz parte do tipo da função. `try` propaga; `catch` trata:

```w
do {
  let user = try parseUser(payload)
  save(user)
} catch .missing(let field) {
  io.error("Missing ${field}")
} catch let error {
  io.error(error.description)
}
```

A implementação deve baixar isso para controle de fluxo explícito, tagged result ou status/out parameter — sem exigir exceptions do sistema ou stack unwinding oculto. `panic` é reservado para invariantes quebradas ou situações declaradas como irrecuperáveis.

`defer` executa cleanup ao sair por retorno ou erro:

```w
fn readConfig(path: Path): Config throws IOError {
  let file = try File.open(path)
  defer file.close()
  return try Config.decode(file.readAll())
}
```

O comportamento de `defer` durante cancelamento faz parte do contrato de tasks e precisa ser garantido pelo lowering.

## 7. Controle de fluxo e patterns

```w
if temperature > 30 {
  fan.start()
} else if temperature < 10 {
  heater.start()
}

for user in users {
  print(user.name)
}

while queue.hasItems {
  process(queue.pop())
}
```

`switch` é exaustivo para enums e pode comparar múltiplos valores:

```w
switch message {
  case .text(let body):
    renderText(body)

  case .image(let url, caption: let caption):
    renderImage(url, caption: caption ?? "")

  case .progress(let percent) where percent < 100:
    renderProgress(percent)

  case .progress(100):
    renderDone()
}
```

```w
switch (connection.state, retryCount) {
  case (.ready, _): start(connection)
  case (.offline, 0): showOffline()
  case (.offline, let count): retry(after: backoff(count))
}
```

Não há fallthrough implícito. Regex, globs e handlers arbitrários devem entrar via protocolo de pattern ou biblioteca; reservar comportamento mágico no compilador antes desse protocolo existir seria prematuro.

Ranges candidatos seguem duas formas ortogonais:

```w
for index in 0..<items.count { } // inclui 0, exclui count
for day in 1...31 { }            // inclui os dois limites
```

Steps, ranges multidimensionais e extremos abertos à esquerda ficam como APIs/experimentos até justificarem nova pontuação.

## 8. Strings deixam a unidade explícita

```w
let greeting = "Hello, ${user.name}!"
let path = r"C:\work\${notInterpolation}"
let message = """
  First line
  Second line
  """
```

`String` contém UTF-8 válido. Operações que poderiam esconder custo ou semântica exigem a view:

```w
text.byteCount
text.bytes
text.scalars
text.graphemes

for grapheme in text.graphemes {
  render(grapheme)
}
```

Não existe um `length` universal ambíguo. Index de `String` não é um inteiro arbitrário; slicing por bytes, scalar ou grapheme usa a view correspondente. Um buffer UTF-8 contíguo é a representação baseline; small-string optimization, rope e interning não mudam essa semântica.

Coleções também deixam ownership/layout consultáveis:

```w
let digest: [u8; 32]       // array fixo
let names: Array<String>   // buffer dinâmico owned
let window: Slice<u8>      // view borrowed
let users: Map<UserId, User>
```

## 9. Ownership aparece apenas onde muda a história

Os quatro termos da baseline são:

| Termo | Significado |
|---|---|
| `ref T` | borrow imutável, não escapa do lifetime permitido |
| `inout T` | borrow mutável exclusivo |
| `take T` | transfere ownership e invalida a origem |
| `copy T` | cria deliberadamente outro valor/owner |

```w
fn render(user: ref User) {
  print(user.name)
}

fn rename(user: inout User, to name: String) {
  user.name = name
}

fn enqueue(job: take Job) {
  queue.push(job)
}

render(user)
rename(inout user, to: "Ada")
enqueue(take job)

let template = copy original
```

O compiler pode inferir borrows e moves óbvios. Quando a origem ainda será usada, quando existe alias mutável ou quando um valor atravessa uma task paralela, ele exige que a intenção seja inequívoca.

Retornar um valor transfere o resultado ao caller semanticamente; não obriga toda ABI a usar a mesma mecânica física. Stack promotion, return-value optimization, caller-allocated buffers e arenas são lowerings possíveis.

Destruição é determinística. O compilador pode escolher stack, heap ou região quando isso não altera o programa. Regiões explícitas e `shared`/ARC continuam questões abertas, pois precisam coexistir com FFI, ciclos e concorrência sem poluir o código comum.

## 10. Concorrência estruturada

Uma função suspensível declara `async` no contrato:

```w
fn fetchProfile(id: UserId): Profile async throws NetworkError {
  let response = try await http.get("/profiles/${id}")
  return try response.json(as: Profile)
}
```

`await` suspende a task atual; não bloqueia necessariamente uma thread. `async let` cria um filho que começa imediatamente dentro do mesmo escopo:

```w
fn home(id: UserId): Home async throws NetworkError {
  async let profile = fetchProfile(id)
  async let feed = fetchFeed(id)

  return Home(
    profile: try await profile,
    feed: try await feed,
  )
}
```

Regras candidatas:

- o escopo não termina enquanto seus filhos não terminarem;
- cada handle deve ser aguardado, cancelado ou transferido para outro escopo estruturado;
- se um filho falhar e o erro sair do escopo, irmãos ainda ativos são cancelados e todos são joined;
- cancelar o pai propaga cancelamento aos filhos;
- cancelamento é cooperativo em suspension/checkpoints e sempre executa cleanup;
- uma task destacada, se existir, exige API longa e um owner runtime explícito; não será o default conveniente.

O type checker registra sendability, captures e efeitos antes que o lowering transforme a função em frames/continuations.

## 11. Paralelismo explícito

`spawn let` cria outro filho estruturado, mas autoriza execução simultânea em um executor de CPU:

```w
fn thumbnails(images: Array<Image>): Array<Thumbnail> async {
  return await TaskGroup<Thumbnail>.parallel { group in
    for image in images {
      group.add(take image, using: resize)
    }

    return await group.collect()
  }
}
```

Dados enviados a `spawn` precisam ser owned/movidos, imutavelmente compartilháveis ou explicitamente sincronizados. Um `inout` não pode atravessar a fronteira enquanto permanecer acessível no caller.

Executors, QoS, afinidade, número de threads e IO backend são configuração/runtime. A linguagem não promete que `spawn` cria uma pthread nem que `async` vive em um event loop específico.

```w
async let response = socket.read() // concorrência e suspensão
spawn let result = compress(data)  // paralelismo de CPU
```

Essa distinção é uma das poucas coisas que W quer tornar mais explícitas que a maioria das linguagens.

## 12. Fronteira C

Declarações C ficam numa região que anuncia a mudança de garantias:

```w
foreign c from "math.h" {
  fn cos(value: f64): f64
}

fn cosine(value: f64): f64 {
  return cos(value)
}
```

A proposta atual de W-O044 coloca layout compatível dentro da fronteira:

```w
foreign c {
  struct Header {
    kind: c.uint
    size: c.size
  }
}
```

Essa forma ainda está **Em aberto**. O default proposto para a DB1 separa layout
W nativo opaco, layout C dentro de `foreign c`, `type` nominal versus `alias` e
`packed`/`aligned` seguros. Ordem física por default, `transparent struct` e uma
ABI W universal fixa continuam preservados como alternativas.

O compilador deve gerar/importar metadata suficiente para documentar:

- target ABI e calling convention;
- ownership de buffers e callbacks;
- nullable pointers;
- errno/status/exception boundary;
- thread safety e possibilidade de suspensão;
- headers, symbols e libraries usados.

Interop sem overhead é possível quando representações e ownership coincidem; não é uma promessa universal. C++ exige adapter/ABI específico. Blocos implementados em JS, Rust, Zig ou outras linguagens ficam para plugins futuros depois desse contrato funcionar bem para C.

A ideia histórica `fn<lang>` continua visível como pesquisa, não como promessa:

```w
fn<C> readProbeRaw(_ handle: c.ptr<Equipment>, _ probe: c.int): c.double
  from "native/equipment.c"
```

Source externo, body inline, namespace de compilation unit e adapter declarado
são comparados no [experimento multilíngue do restaurante](examples/restaurant/multilingual.md).
Em qualquer spelling, o call site recebe tipos W e a receita fixa adapter,
toolchain, flags, source digests e artefatos.

## 13. O package também é um contrato

No source, o import é estável. O manifest declara intenção, e o lock registra a resolução exata:

```w
package {
  schema: "w.package/1"
  name: "acme/image-service"
  version: "0.1.0"

  dependencies: [
    {
      alias: "http"
      package: "w/http"
      version: "^2.3"
      source: {
        kind: .registry
        registry: "w"
      }
    },
    {
      alias: "images"
      package: "acme/images"
      version: "2.1.3"
      source: {
        kind: .registry
        registry: "acme"
      }
    }
  ]
}
```

Uma build registra source digest, dependências transitivas, target, compiler/toolchain, flags semânticas, artefatos, SBOM e provenance. Binários compatíveis podem vir do cache; a política pode exigir reprodução independente antes de aceitá-los.

Mirrors transportam bytes; não recebem confiança implícita. Identidade, signatures, expiração/revogação e transparency pertencem à metadata verificada. Veja [design/packages.md](design/packages.md).

## 14. O que o runtime realmente precisa fazer

O modelo acima implica um runtime pequeno, mas não trivial:

- alocação/destruição e suporte opcional a regiões/shared values;
- scheduler de tasks, timers, cancelamento e executors;
- adapters de I/O por plataforma;
- panic/diagnostics e stack/source mapping;
- metadata de módulos, ABI e capabilities;
- primitives de sincronização usadas por bibliotecas.

Não implica:

- um heap por import;
- um GC global;
- uma thread por task;
- atomics em toda propriedade;
- ausência completa de locks;
- SQLite em toda aplicação;
- uma representação tagged para todo valor.

Esses podem ser lowerings ou políticas onde medição justifique.

## 15. Antes de chamar isso de linguagem

Cada snippet deste tour deve virar um arquivo em `examples/`, um caso do parser, uma árvore esperada e um teste do formatter. Depois, as invariantes de tipo/ownership/tasks devem ser testadas no HIR e apenas então baixadas para MLIR.

Até isso acontecer, este documento é a maquete navegável do W: concreta o suficiente para discutir, honesta o suficiente para mudar.
