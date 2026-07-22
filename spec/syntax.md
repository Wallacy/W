# Sintaxe de trabalho

> **Status:** candidata; destinada a corpus, parser e formatter
> **Data:** 21 de julho de 2026

Este documento reduz as alternativas de sintaxe a uma baseline coerente. Ele
ainda não é uma gramática normativa. Toda forma abaixo precisa virar exemplo
dourado, teste positivo/negativo e regra de formatter antes de ser aceita.

## Objetivos

- parecer familiar para quem conhece C, Swift ou TypeScript;
- permitir ler o custo e os efeitos importantes sem excesso de pontuação;
- ter uma única forma canônica para declarações comuns;
- produzir uma CST estável mesmo em arquivos parcialmente inválidos;
- evitar ambiguidades que dependam de whitespace invisível;
- deixar extensões, compile time e foreign code delimitados.

## Source e tokens

- Encoding: UTF-8, sem BOM na forma canônica.
- Quebra de linha: LF na forma formatada; o lexer aceita CRLF.
- Identificadores ASCII são a baseline do primeiro parser.
- Unicode normalizado em identificadores é uma questão aberta; nunca deve depender de hash “sem colisão”.
- Keywords são case-sensitive e lowercase.
- Tipos nominais usam PascalCase por convenção; funções, bindings e módulos usam lowerCamelCase; módulos/packages podem adotar segmentos lowercase.

Comentários:

```w
// uma linha

/* bloco
   possivelmente multilinha */

/// documentação de uma declaração pública
```

Comentários de bloco aninhados são desejáveis, mas só entram se o lexer e o formatter os tratarem sem casos especiais frágeis.
`///` se anexa à próxima declaração e contém Markdown. Fences `w test` viram
doctests; `///` não cria uma annotation geral nem executa durante import. O
contrato completo está em
[documentação e testes](../design/documentation-and-tests.md).

## Separação de statements

O formatter não emite `;`. O parser usa a completude sintática da expressão e os delimitadores do bloco; uma quebra de linha pode separar statements quando o token anterior poderia encerrá-lo e o próximo não pode continuá-lo.

```w
let x = calculate()
let y = transform(x)
```

`semicolon` pode ser aceito como separador explícito para interop/migração, mas não cria uma segunda forma formatada. Expressões adjacentes nunca são concatenadas implicitamente.

## Literais

### Booleanos e ausência

```w
true
false
.none
```

### Números

```w
42
1_000_000
0xff_u8
0b1010
3.141_592_f64
```

Separadores `_` não alteram o valor. Uma unit expression canônica segue o número:

```w
9.81[m/s^2]
90[degC]
64[KiB]
```

Dentro de `[...]`, `^` é potência dimensional. Açúcares contíguos como `90C`,
`90°F`, `5km` e `64KiB` pertencem ao mapa versionado da edição e sempre podem ser
expandidos pela tooling; a forma delimitada permanece estável. Regras de prefixos,
temperatura e informação estão em
[numéricos e quantidades](../design/numerics-and-quantities.md).

### Strings

```w
"plain UTF-8"
"Hello, ${name}"
#"C:\path\${literal}"#
#"raw can contain "quotes" and ${noInterpolation}"#
"""
  multiline text
  dedented by the closing delimiter
  """
```

Há uma única string interpolada comum, uma raw hash-delimited e uma multiline.
Raw não usa prefixo `r`; a mesma quantidade de `#` abre e fecha o literal, e
hashes adicionais permitem incluir a sequência de fechamento. Raw não processa
escapes nem `${}`. A forma multiline raw usa os mesmos hashes em torno de
`"""..."""`. Aspas simples e crase não são delimitadores equivalentes.
Concatenação exige `+`, interpolation ou builder explícito.

### Coleções e records

```w
[1, 2, 3]
["red": 0xff0000, "blue": 0x0000ff]
(x: 10, y: 20)
```

O literal nomeado pode construir uma tupla/record anônimo; conversão para um tipo nominal exige contexto ou construtor explícito.

## Bindings

```w
const pageSize = 4096
let name = "W"
let port: u16 = 8080
var requests = 0
var Lazy heatProfile = deriveHeatProfile(model)
var atomic completedOrders: u64 = 0
```

- `const`: valor disponível no compile time hermético;
- `let`: binding runtime não reatribuível;
- `var`: binding runtime reatribuível.

Uma lista de behaviors pode existir entre `var` e o nome. `var` ancora a
declaração; `Lazy var value`, `by` e `with` não são formas canônicas. Behaviors
de biblioteca preservam o tipo lógico e expandem storage/accessors em HIR.
`atomic` é um modifier contextual verifier-backed, não um behavior comum:
`var atomic completedOrders: u64` possui storage `Atomic<u64>`, proíbe borrow
normal do payload e dá semântica atômica às operações suportadas.

Uma declaração sem initializer só é aceita quando o analisador prova inicialização em todos os caminhos antes da leitura.

## Funções

Forma canônica:

```w
fn name(label: Type, other: Other = default): Return {
  return value
}
```

Exemplos com efeitos:

```w
fn update(user: inout User) {
  user.revision += 1
}

async fn load(id: UserId): User throws LoadError {
  // ...
}
```

Escolhas candidatas:

- uma única keyword: `fn`, não `func`;
- tipo de retorno com `:`, não `->`;
- `async` imediatamente antes de `fn`; combinado com receiver mutável, a ordem é
  `mut async fn`;
- `mut` antes de `fn` quando um método muta seu receiver implícito; uma free
  function já anuncia mutação externa por `inout`;
- `throws E` após o retorno; `async` não reaparece como sufixo;
- `Void` pode ser omitido;
- trailing comma é aceita e emitida em listas multilinha.

A posição de `async` ainda é questão aberta em [STATUS.md](../STATUS.md). A forma acima existe para que o corpus tenha uma baseline.

### Labels

O primeiro argumento é posicional por default; os seguintes usam o nome como
label. Isso acompanha o corpus e permanece candidato em [W-O040](../STATUS.md):

```w
fn move(source: Path, to destination: Path)
move(source, to: destination)
```

Um label explícito antes do nome substitui o default, inclusive no primeiro
argumento. O label `_` permite argumento posicional adicional onde isso realmente
melhora a API:

```w
fn dot(_ lhs: Point, _ rhs: Point): f64
let value = dot(a, b)
```

Labels fazem parte do overload e da API metadata. O uso em funções privadas pode ser relaxado pelo linter, não por uma segunda gramática.

### Generics

```w
fn identity<T>(_ value: T): T {
  return value
}

fn decode<T: Decodable>(bytes: ref Slice<u8>, as _: T.Type): T throws DecodeError
```

`<...>` depois do nome é reservado a parâmetros de tipo/const. A forma histórica
`fn<C> name` seria lexicalmente distinta porque a tag vem depois de `fn`, não do
nome. Ela permanece pesquisa por causa de toolchain, body opaco, source maps e
provenance — não por uma ambiguidade inexistente com generics.

### Closures

Forma mínima candidata:

```w
let double = (value: Int) => value * 2

items.map((item) => {
  return transform(item)
})
```

Captures que mudam lifetime ou atravessam `spawn` precisam ser visíveis no type checker. A sintaxe de capture list (`copy`, `ref`, `take`, `weak`) permanece aberta e não deve ser inferida da pontuação de C++ por acidente.

## Declarações de tipo

```w
struct Point {
  x: f64
  y: f64
}

object Session {
  id: SessionId
  var state: State
}

enum Result<T, E> {
  ok(T)
  error(E)
}

protocol Drawable {
  fn draw(on canvas: inout Canvas)
}

type Port = u16 where value in 1...65535
```

- `struct`: semântica de valor;
- `object`: identidade e ownership;
- `enum`: sum type fechado;
- `protocol`: contrato sem storage implícito;
- `type ... where`: tipo refinado/nominal.

Herança de implementação não faz parte da baseline. Composição, protocols e extensions devem ser testados primeiro.

## Visibilidade, módulos e imports

```w
import { http, Url } from std.net
import models as domain from app.models

export struct User {
  id: UserId
  name: String
}

export fn findUser(id: UserId): User? {
  // ...
}
```

`export` é a keyword candidata para API de módulo/package. Exportação estrangeira
usa uma declaração/wrapper dentro da fronteira correspondente; nem todo `export`
precisa virar symbol público C.

O top-level v0 deve aceitar imports, declarations e `const`. Inicialização runtime e `var` global exigem uma decisão explícita sobre order, errors e concurrency; a baseline recomenda encapsular estado em `object`/serviço.

O source não importa URLs. O manifest resolve nomes de package e o lockfile fixa a versão/artefato.

## Opcionais e narrowing

```w
let value: User? = findUser(id)

if let user = value {
  render(user)
}

guard let user = value else return .notFound

let label = value?.name ?? "Anonymous"
```

`T?` é `Option<T>`. Optional chaining `?.` e coalescing `??` são candidatos porque têm semântica conhecida e compacta.

## Erros

```w
enum ParseError: Error {
  unexpectedToken(Token)
  incompleteDocument
}

fn parse(source: ref String): Document throws ParseError

let document = try parse(source)

do {
  try save(document)
} catch .permissionDenied {
  recover()
} catch let error {
  report(error)
}
```

`Error` é um marker protocol do core/superfície implícita versionada, não uma classe-base com storage ou
unwind ocultos. A forma candidata `enum E: Error` declara um error set fechado;
`throws E` aceita esse conjunto no tipo da função.

`try` não é uma promessa de exception/unwind; marca propagação de um erro tipado.
Na baseline, ele só propaga diretamente quando caller e callee usam o mesmo
error set. Se uma função `throws MenuError` chama outra que `throws OrderError`,
ela converte explicitamente em `do`/`catch` e `throw .order(error)`. Conversões
declaradas ou uma injeção única inferida continuam em [W-O033](../STATUS.md).
`panic` é uma chamada/efeito separado.

## Controle de fluxo

```w
if condition { } else { }
while condition { }
for item in collection { }

guard condition else {
  return fallback
}
```

Switch candidato:

```w
switch value {
  case .some(let item): use(item)
  case .none: return
}
```

Regras desejadas:

- enum switch exaustivo;
- sem fallthrough implícito;
- bindings nos patterns;
- guard `where`;
- tuples/múltiplos valores;
- decisão posterior sobre switch como expression.

Ranges podem atuar como patterns, e `where` refina o pattern já selecionado:

```w
switch value {
  case 0.0...1.0: useInRange(value)
  case ..<0.0 where recovery > 0.0: recoverLow(value)
  case 1.0>.. where recovery < 0.0: recoverHigh(value)
  case _: reject(value)
}
```

Esse uso é candidato. `where` não se torna spelling alternativo de `&&` em todo
`if`; sua função permanece guardar patterns, constraints e futuras queries.

Ranges:

```w
1...5     // [1, 5]   — ambos inclusivos
1..<5     // [1, 5)   — upper exclusivo
1>..5     // (1, 5]   — lower exclusivo
1>..<5    // (1, 5)   — ambos exclusivos
```

As quatro closures entram na grammar como **Candidato**, não como semântica
fechada. `value in 0.0...1.0` testa bounds sem alocar. Para iteração, a direção
preferida é permitir `for index in 0..<count` quando o tipo possui sucessor e
exigir step/`stride` explícito para floats e outros domínios contínuos.

A alternativa histórica de tratar todo range como producer/lazy array, além da
opção de separar `Interval<T>` de uma progressão, permanece em
[W-O041](../STATUS.md). Patterns one-sided `..<upper`, `...upper`, `lower>..` e
`lower...` entram no experimento W-C028. Transformá-los em first-class values,
multirange ou ranges infinitamente iteráveis continua em pesquisa até definir
membership, iteration, `count` e bounds.

## Testes no módulo

```w
test "clamp preserva o interior" for clampRatio {
  expect clampRatio(0.5) == 0.5
}
```

`test` é contextual, o nome é obrigatório e `for symbol` é opcional. A declaração
não é exportada e é removida de builds release. Testes maiores usam `*.test.w` sem
mudar de linguagem ou runner. Veja
[documentação e testes](../design/documentation-and-tests.md).

## Ownership no source

Assinaturas:

```w
fn inspect(value: ref Value)
fn edit(value: inout Value)
fn store(value: take Value)
```

Call sites que mudam ownership/mutabilidade:

```w
inspect(value)
edit(inout value)
store(take value)
let duplicate = copy value
```

`ref` pode ser inferido na chamada porque não altera a disponibilidade do argumento. `inout`, `take` e `copy` aparecem no ponto de uso.

## Concorrência e paralelismo

```w
async let response = fetch(url)
let body = try await response

spawn let digest = hash(bytes)
let value = await digest

cancel response
cancel digest, reason: .shutdown
```

Uma chamada de função `async` só é válida sob `await` ou como initializer de uma construção de child task. Não há Future/Promise silenciosa criada por esquecer uma keyword.

`async let` e `spawn let` são bindings especiais cujo valor é um handle lexical.
`cancel` é keyword contextual aceita somente para `Task`/`TaskGroup` ou outro
tipo explicitamente `Cancellable`; ela solicita cancelamento cooperativo e não
interrompe uma stack de forma assíncrona. A semântica completa está em
[concurrency.md](concurrency.md).

## FFI e fronteiras de representação

O bloco abaixo combina a baseline `foreign c` com o layout C candidato de
[W-C039](../STATUS.md). Uma declaração dentro do bloco segue a ABI C do target;
uma struct W normal continua com layout opaco entre builds.

```w
foreign c from "sqlite3.h" {
  type sqlite3
  fn sqlite3_close(handle: c.ptr<sqlite3>): c.int

  struct Header {
    kind: c.uint
    size: c.size
  }
}
```

A v0 não reserva `@name(...)`. Semântica usa keywords/blocos próprios e metadata
de build/deploy usa manifest. Macros arbitrárias e decorators que executam
código não entram no parser mínimo.

Foreign bodies inline, se existirem, precisam de delimitador que preserve o
source original e de um adapter/frontend próprio. A primeira versão aceita
declarações `foreign c`; depois, [W-O042](../STATUS.md) compara body inline da
aplicação, `from` para source separado, namespace de compilation unit e adapter
declarado. O parser W delimita a ilha, mas não interpreta seus
statements.

## Operadores

Precedência inicial, da maior para a menor:

1. member/call/index: `.`, `()`, `[]`, `?.`;
2. exponenciação: `**`, associativa à direita;
3. prefix: `!`, `~`, unary `-`, `copy`, `take`, `await`;
4. multiplicativos: `*`, `/`, `%` e variantes explícitas;
5. aditivos: `+`, `-` e variantes explícitas;
6. shifts: `<<`, `>>`;
7. ranges: `...`, `..<`, `>..`, `>..<`;
8. relações: `<`, `<=`, `>`, `>=`, `is`, `in`;
9. igualdade: `==`, `!=`;
10. bitwise: `&`, `^`, `|`;
11. boolean: `&&`, `||`;
12. coalescing: `??`;
13. assignment: `=`, `+=`, `-=`, etc.

Assim, `-x ** 2` significa `-(x ** 2)`. Dentro de `[unit expression]`, uma
subgramática separada usa `^` para expoentes científicos, como `m/s^2`; fora
dela, `^` continua XOR.

Operadores customizados e precedência declarada pelo usuário ficam fora do parser v0; prejudicam tooling e podem tornar o runtime pouco evidente.

`value in (a, b)` é membership finito intrinsic e não aloca uma coleção
observável. Para sets/flags, `hasAny` e `hasAll` são operações diferentes; um
enum simples nunca pode ser dois cases ao mesmo tempo.

## Keywords candidatas

```text
as async atomic await break cancel case catch const continue copy defer do else enum
export false fn for foreign from guard if import in inout is let mut object panic
protocol ref return service spawn struct switch take test throw throws true try type var where while
```

`atomic`, `cancel` e `test` são contextuais: continuam disponíveis como identifiers
fora das posições de modifier/statement/declaration em que a gramática os reconhece.

Os sketches recuperados pela auditoria não ampliam esta lista. Em particular,
`entry`, `on`, argumentos genéricos rotulados e o separador `;` em literals de
matriz pertencem a W-O100–W-O103 e continuam **Em aberto**. O portal pode mostrá-los
em blocos marcados como experimento, mas parser, formatter, Tree-sitter e extensão
VS Code não devem tratá-los como sintaxe W antes da ratificação do
[adendo da DB1](../DB1_ADDENDUM.md).

`self` e `this` ainda precisam de uma única regra. A preferência é `self` para receiver de valor/objeto e o nome do módulo para namespace, evitando dois pronomes contextuais.

## O que não é gramática normativa

- sketches, snippets e exemplos exploratórios não substituem esta baseline;
- alternativas como `func`, ausência de keyword ou outro separador de retorno
  permanecem fora do parser até uma decisão explícita;
- o portal visual deriva desta baseline, mas não substitui testes do parser.

## Próximo artefato obrigatório

Uma gramática EBNF pequena e um corpus em `examples/` devem ser criados juntos. Para cada construct:

- exemplo formatado;
- CST esperada;
- AST/HIR esperada quando aplicável;
- casos de recuperação de erro;
- caso negativo com diagnóstico esperado;
- round-trip parse → format → parse.

Só então esta sintaxe pode deixar de ser “de trabalho”.
