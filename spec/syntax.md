# Sintaxe de trabalho

> **Status:** candidata; destinada a corpus, parser e formatter
> **Data:** 18 de julho de 2026

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

O sufixo exato e as regras de inferência ainda serão definidos. Separadores `_` não alteram o valor.

### Strings

```w
"plain UTF-8"
"Hello, ${name}"
r"C:\path\${literal}"
"""
  multiline text
  dedented by the closing delimiter
  """
```

Há uma única string interpolada comum, uma raw e uma multiline. Aspas simples e crase não são delimitadores equivalentes. Concatenação exige `+`, interpolation ou builder explícito.

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
```

- `const`: valor disponível no compile time hermético;
- `let`: binding runtime não reatribuível;
- `var`: binding runtime reatribuível.

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

fn load(id: UserId): User async throws LoadError {
  // ...
}
```

Escolhas candidatas:

- uma única keyword: `fn`, não `func`;
- tipo de retorno com `:`, não `->`;
- `mut` antes de `fn` quando um método muta seu receiver implícito; uma free
  function já anuncia mutação externa por `inout`;
- efeitos após o retorno, em ordem canônica `async throws E`;
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

`export` é a keyword candidata para API de módulo/package. Uma annotation de ABI define exportação estrangeira; nem todo `export` precisa virar symbol público C.

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
```

Uma chamada de função `async` só é válida sob `await` ou como initializer de uma construção de child task. Não há Future/Promise silenciosa criada por esquecer uma keyword.

`async let` e `spawn let` são bindings especiais cujo valor é um handle lexical. A semântica completa está em [concurrency.md](concurrency.md).

## FFI e annotations

```w
foreign c from "sqlite3.h" {
  type sqlite3
  fn sqlite3_close(handle: c.ptr<sqlite3>): c.int
}

@repr(c)
struct Header {
  kind: u32
  size: u64
}
```

`@name(...)` é reservado a annotations verificadas pelo compilador/tooling. Macros arbitrárias e decorators que executam código não entram no parser mínimo.

Foreign bodies inline, se existirem, precisam de delimitador que preserve o
source original e de um adapter/frontend próprio. A primeira versão aceita
declarações `foreign c`; depois, [W-O042](../STATUS.md) compara body inline da
aplicação, `from` para source separado, namespace de compilation unit e
annotation/adapter. O parser W delimita a ilha, mas não interpreta seus
statements.

## Operadores

Precedência inicial, da maior para a menor:

1. member/call/index: `.`, `()`, `[]`, `?.`;
2. prefix: `!`, `~`, unary `-`, `copy`, `take`, `await`;
3. multiplicativos: `*`, `/`, `%` e variantes explícitas;
4. aditivos: `+`, `-` e variantes explícitas;
5. shifts: `<<`, `>>`;
6. ranges: `...`, `..<`, `>..`, `>..<`;
7. relações: `<`, `<=`, `>`, `>=`, `is`, `in`;
8. igualdade: `==`, `!=`;
9. bitwise: `&`, `^`, `|`;
10. boolean: `&&`, `||`;
11. coalescing: `??`;
12. assignment: `=`, `+=`, `-=`, etc.

Operadores customizados e precedência declarada pelo usuário ficam fora do parser v0; prejudicam tooling e podem tornar o runtime pouco evidente.

Testar um value contra várias alternativas merece uma forma compacta, mas não
precisa virar novo operador antes de provar seu lowering. O corpus compara
`value.isOneOf(a, b)` com `value in (a, b)` em [W-O038](../STATUS.md). A operação
é OR/membership e não aloca uma coleção observável. Para sets/flags, `hasAny` e
`hasAll` são operações diferentes; um enum simples nunca pode ser dois cases ao
mesmo tempo.

## Keywords candidatas

```text
as async await break case catch const continue copy defer do else enum
export false fn for foreign from guard if import in inout is let mut object panic
protocol ref return spawn struct switch take throw throws true try type var where while
```

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
