# Cheatsheet da Linguagem W

## Sintaxe Básica

**Comentários:**

```typescript
// Comentário de linha única

/*
 * Comentário
 * de múltiplas
 * linhas
 */
```

**Declaração de Variáveis:**

```typescript
let nomeImutavel = "W Lang" // Variável imutável (constante)
const nomeConstante = "W Lang" // Variável constante, escopo de bloco
var nomeMutavel = "W Lang"   // Variável mutável, escopo de bloco
global const PI = 3.14159         // Constante global (nível de módulo)
```
`const` declara constantes com escopo de bloco, enquanto `global const` declara constantes no escopo do módulo. `let` declara variáveis imutáveis com escopo de bloco, e `var` declara variáveis mutáveis com escopo de bloco.

**Tipos de Dados:**

```typescript
Int         // Inteiro (tamanho dependente da arquitetura, padrão i32/i64)
Int<size>   // Inteiro com tamanho específico (e.g., Int<16>, Int<32>, Int<64>)
Int<bitSize: size> // Inteiro com tamanho específico em bits (e.g., Int<bitSize: 16>)
Float       // Ponto flutuante (padrão double)
Float<size> // Ponto flutuante com tamanho específico (e.g., Float<32>, Float<64>)
String      // String UTF-8
Char        // Caractere Unicode
Bool        // Booleano (true ou false)
Void        // Tipo vazio (sem retorno)
```
`Int`, `Float`, `String`, `Char`, `Bool`, e `Void` são tipos primitivos. `Int` e `Float` podem ter tamanhos especificados.

**Tipos Opcionais:**

```typescript
let usuario: String? = nil  // Variável String que pode ser nula
let idade: Int?          // Variável Int opcional, valor inicial nil
```
Tipos opcionais são declarados com `?` e podem conter `nil`.

**Tipos Restritos:**

```typescript
type NomeUsuario = String<maxLength: 20, pattern: /^[a-zA-Z0-9_]+$/>
type IdadeValida = Int<range: 0...120>
type CPF = String<maxLength: 12; mask:CPF, inputType:Number>; // Tipo String com máscara de CPF
type CPFType = String<maxLength: 12; mask:CPF, inputType:Number>; // Type alias para CPF
type Email = String<pattern: /^[\w-\.]+@([\w-]+\.)+[\w-]{2,4}$/>
type Senha = String<minLength: 8>
type HexColor = String<pattern: /^#([0-9A-F]{3}){1,2}$/i> // Cores Hexadecimal
type TelefoneBR = String<mask: '(99) 99999-9999', inputType: Number> // Telefone BR
type Porcentagem = Float<range: 0.0...1.0> // Porcentagens entre 0 e 1
type UUID = String<pattern: /^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$/> // UUIDs
type DataISO = String<pattern: /^\d{4}-\d{2}-\d{2}$/> // Datas no formato ISO 8601 (YYYY-MM-DD)
type Hora24h = String<pattern: /^(?:[01]\d|2[0-3]):[0-5]\d$/ > // Horas no formato 24h (HH:MM)
type ValorMonetario = Float<range: 0...> // Valores monetários não negativos
type CodigoPostalBR = String<mask: '99999-999', inputType: Number> // Código postal brasileiro
type PlacaVeiculoBR = String<maxLength: 8, pattern: /^[A-Z]{3}\d[A-Z0-9]\d{2}$/> // Placas de veículo no formato BR

```
Tipos podem ser restringidos com `maxLength`, `pattern`, `range`, `minLength`, `mask`, e `inputType`. `type` keyword creates type aliases.

**Operadores:**

*   **Aritméticos:** `+`, `-`, `*`, `/`, `%`
    ```typescript
    10 + 5  // 15
    20 - 3  // 17
    7 * 6   // 42
    50 / 5  // 10
    10 % 3  // 1
    ```
*   **Comparação:** `==`, `!=`, `>`, `<`, `>=`, `<=`
    ```typescript
    10 == 10 // true
    5 != 3   // true
    8 > 2    // true
    1 < 0    // false
    5 >= 5   // true
    2 <= 1   // false
    ```
*   **Lógicos:** `&&`, `||`, `!`
    ```typescript
    true && false // false
    true || false // true
    !true        // false
    ```
*   **Atribuição:** `=`, `+=`, `-=`, `*=`, `/=`, `%=`
    ```typescript
    var x = 10
    x += 5 // x é agora 15
    x -= 3 // x é agora 12
    x *= 2 // x é agora 24
    x /= 4 // x é agora 6
    x %= 5 // x é agora 1
    ```
*   **Coalescência Nula:** `??` (ex: `nomeUsuario ?? "Anônimo"`)
    ```typescript
    let nome: String? = nil
    let nomeExibicao = nome ?? "Visitante" // nomeExibicao é "Visitante"
    ```
*   **Opcional Chaining:** `?.` (ex: `usuario?.nome`)
    ```typescript
    class Usuario {
        let nome: String?
    }
    let usuario: Usuario? = nil
    let nomeUsuario = usuario?.nome // nomeUsuario é nil
    ```
*   **Range Operators:** `..` (inclusivo), `..<` (exclusivo superior), `>..` (exclusivo inferior), `>..<` (exclusivo ambos)
    ```typescript
    for (i in [1..3]) { print(i) }      // 1 2 3
    for (i in [1..<3]) { print(i) }     // 1 2
    for (i in [2>..5]) { print(i) }     // 3 4 5
    for (i in [2>..<5]) { print(i) }    // 3 4
    for (i in [1..7, 2]) { print(i) }   // 1 3 5 7
    ```
*   **Optional Operators:** `?+`, `?-`, `?*`, `?/`, `?%`, `?>`, `?<`, `?>=`, `?<=`, `?+=`, `?-=` (conditional operators for optionals)
    ```typescript
    let a: Int? = 10
    let b: Int? = nil
    print(a ?+ 5) // Optional(15)
    print(b ?+ 5) // nil
    ```

**Strings e String Interpolation:**

Strings podem ser declaradas com `"` , `'` ou `` ` ``.

```typescript
print("Hello, ${name}! Today is \"${date.DayOfWeek}\", it's ${date:HH:mm} now.")
print('Hello, ${name}! Today is "${date.DayOfWeek}", it\'s ${date:HH:mm} now.')
print(`Hello, ${name}! Today is "${date.DayOfWeek}", it's ${date:HH:mm} now.`)

print($`Hello, {name}! Today is {date.DayOfWeek}, it's {date:HH:mm} now.`) // Positional interpolation

print(@"Hello, ${name}! Today is ${date.DayOfWeek}, it's ${date:HH:mm} now."); // Literal string, no interpolation

print(#"Hello, ${name}!
      Today is ${date.DayOfWeek}
      it's ${date:HH:mm} now.") // Multiline string with identation

print(#$`Hello, {name}!
      Today is {date.DayOfWeek}
      it's {date:HH:mm} now.`) // Multiline and positional interpolation

print("Hello, Joe! "
      "Today is friday, it's 12/05/198814h25 "
      "now its a good time") // Implicit string concatenation

```
Strings suportam interpolação com `${}`.  `$`` enables positional interpolation. `@` disables interpolation for literal strings. `#"` enables multiline strings with indentation based on `#` position. `#$`` enables multiline and positional interpolation. Strings can be implicitly concatenated by placing them side-by-side.

**Whitespace Handling:**

Whitespace is generally ignored outside of strings.

## Estruturas de Controle

**Condicional `if`:**

```typescript
let x = 10
if (x > 5) {
  print("Maior que 5")
} else if (x == 5) {
  print("Igual a 5")
} else {
  print("Menor que 5")
}
```

**Condicional `guard`:**

```typescript
func processarValor(valor: Int?) -> String? {
  guard let val = valor else {
    return nil // Early return se valor for nil
  }
  // Continua o processamento se valor não for nil
  return "Valor processado: ${val}"
}
```

```typescript
func example() {
  guard cond return "reason1" // Early return com valor
  guard cond2 return "reason2"
  guard cond3 return "reason3"

  ...thing...
  return ...
}
```
`guard` statements ensure conditions are met before proceeding, providing early exits. `guard <bool_expression> return <...>` provides a shorthand for early return with a value.

**Condicional `if let`:**

```typescript
const provider = getProvider() // Provider | undefined
if let prov = provider {
  prov.doIt() // prov is now type Provider and not undefined
}
```
`if let` provides a way to safely unwrap optionals and check for non-null values.

**Condicional `if ~= condition`:**

```typescript
var bb = [...]

if bb ~= 'someElementMatch' { // Checks if 'someElementMatch' is present in bb
  // ...
}

if bb ~= someFunc { // Checks if someFunc returns true for any element in bb
 // ...
}

if bb ~= (b) => { // Checks if the lambda returns true for any element in bb
 // ...
}
```
`if ~= condition` provides pattern matching capabilities for collections, checking for element presence or conditions met by elements.

**Switch Statement:**

```typescript
let valor = 3
switch (valor) {
  case 1: print("Um")
  case 2, 3: print("Dois ou Três")  // Múltiplos casos
  default: print("Outro")
}
```

**Loop `for`:**

```typescript
for (var i = 0; i < 5; i++) {
  print(i)        // Imprime 0, 1, 2, 3, 4
}

for (item in colecao) { // for-in para coleções (arrays, etc - a ser definido)
  print(item)     // Itera sobre array
}

for (valor in [1..5]) {  // Range inclusivo: 1, 2, 3, 4, 5
  print(valor)
}

for (valor in [1..<5]) { // Range exclusivo superior: 1, 2, 3, 4
  print(valor)
}

for (valor in [1>..5]) { // Range exclusivo inferior: 2, 3, 4, 5
  print(valor)
}

for (valor in [1>..<5, 2]) { // Range com step: 3
  print(valor)
}
```

**Loop `while`:**

```typescript
var count = 0
while (count < 5) {
  print(count)
  count += 1
}
```

**Loop `do-while`:**

```typescript
var count = 0
do {
  print(count)
  count += 1
} while (count < 5)
```

## Funções, Closures e Lambdas

Em W, funções e closures compartilham a mesma base sintática. Funções são, essencialmente, closures nomeadas. Lambdas são funções anônimas *inline*.

**Declaração de Função Nomeada:**

```typescript
func nomeDaFuncao(parametro1: Tipo1, parametro2: Tipo2): TipoRetorno {
  // Corpo da função
  return valorDeRetorno
}
// Funções sem tipo de retorno (retornam Void implicitamente)
func funcaoSemRetorno(parametro: Tipo) {
  // Corpo da função
}

//Funções com nome de parametro
func cumprimentar(nome pessoa: String, idade: Int) {
    print("Olá, ${pessoa}! Você tem ${idade} anos.")
}
cumprimentar(nome: "Ana", idade: 30)

//Funções com argumentos variádicos
func somar(numeros: Int...) -> Int {
    var total = 0
    for (numero in numeros) {
        total += numero
    }
    return total
}

print(somar(1, 2, 3, 4, 5)) // Imprime 15

// Funções com código em outras linguagens
func<C> funcaoC(parametro: Int): Int {
    // Código C inline
    return parametro * 2;
}

func<asm ("MYFUNC")> funcaoAsm() Int // Declaração de função assembly externa

```
Funções são declaradas using `func` or `fn` keyword.  Functions can have named parameters and variadic arguments. Functions can also contain inline code in other languages like C, using `func<C>`. External assembly functions can be declared using `func<asm ("MYFUNC")>`.

**Funções Anônimas (Closures):**

```typescript
let minhaFuncaoAnonima = (parametro: Tipo): TipoRetorno {
  // Corpo da função anônima
  return valorDeRetorno
}

// Shorthand para funções anônimas com corpo de uma linha (Lambdas)
let funcaoCurta = (parametro: Tipo) => valorDeRetorno // Quando não usa o par `{ }`, deve-se usar `=>`

//Exemplos
let dobrar = (x: Int) => x * 2
print(dobrar(5)) // Imprime 10

let saudacao = (nome: String) => {
    return "Olá, ${nome}!"
}
print(saudacao("Carlos")) // Imprime "Olá, Carlos!"
```

**Funções como Closures com Captura Explícita:**

```typescript
func contador(init: Int) -> (Int) -> Int { // Retorna uma função (closure)
  var count = init // Variável local à função contador
  return (incremento: Int) { |let c = count| // Captura 'count' por cópia
    return (c + incremento)
  }
}

let meuContador = contador(init: 10)
print(meuContador(5)) // 15
print(meuContador(3)) // 18

// Captura por referência (weak)
func observador(valor: Int) -> () -> Int? {
    var valorObservado: Int? = valor // Usando opcional para simular referência fraca
    return () { |weak valorObservado|
        return valorObservado
    }
}

var valorInicial = 10
let meuObservador = observador(valor: valorInicial)
print(meuObservador()) // Imprime Optional(10)
valorInicial = 20 // Modificar valorInicial não afeta valorObservado
print(meuObservador()) // Imprime Optional(10)

func contador(init: Int) {
  let count = init
  return (add: Int) => { |let c = count, self| // Captura 'count' por cópia e 'self' por strong reference
    return (c + add)
  }
}

func contador(init: Int) {
  let count = init
  return (add: Int) => { |weak object| // Captura 'object' por weak reference
    return (count ?+ add)
  }
}

func contador(init: Int) {
  let count = init
  return somador(add: Int) { |count| // Named closure, 'somador' name is lost in return type
    return (count + add)
  }
}
```
Closures can capture variables explicitly using `|capture_list|`. Capture modes include copy (`let`), strong reference (`self`), and weak reference (`weak`). Captures can be named using `|let captureName = variable, ...|`.

**Funções Assíncronas (`async`/`await`) com Modificadores:**

```typescript
async func funcaoAssincrona(): String {
  // Código assíncrono
  let resultado = await algumaOperacaoAssincrona()
  return resultado
}

func chamarFuncaoAsync() async {
  let resultado = await funcaoAssincrona()
  print(resultado)
}
```

**Funções Paralelas (`spawn`/`await`) com Modificadores:**

```typescript
async func tarefaPesada(): String { // Spawn só pode ser feito em funções do tipo async.
  // Operação computacionalmente intensiva
  return "Tarefa pesada concluída"
}

func executarParalelamente() async {
  let tarefa = spawn tarefaPesada()
  let resultado = await tarefa
  print(resultado)
}

func executarParalelamenteEmBackground() async {
  let tarefa = spawn<.background> tarefaPesada()
  let resultado = await tarefa
  print(resultado)
}

async<.max(8)> func makeDinner() throws: Meal { // Max threads for async function
  let veggies = try chopVegetables()
  let meat = marinateMeat()
  let oven = try preheatOven(temperature: 350)

  let dish = Dish(ingredients: [veggies, meat])
  return try oven.cook(dish, duration: .hours(3))
}
```
`async` and `await` keywords are used for asynchronous operations. `spawn` creates parallel tasks. Modifiers like `<.background>` and `<.max(threads)>` can be used to configure execution context.

**Modificadores de Parâmetros:**

```typescript
func processarDados(dados: String<.ref>, arquivo: String<.storage>, cache: String<.cow>) {
  // ref: Referência mutável (in-out)
  // storage: Transferência de posse
  // cow: Copy-on-write
}
```
Parameter modifiers control ownership and mutability of arguments passed to functions.

**CallbackType:**

```typescript
func greet(callback fptr: (nome: String) -> Void) {
    fptr("World");
}

func sayHello(nome: String) {
    print("Hello, ${nome}!\n")
}

func main() {
    greet(fptr: sayHello)
}

func some(a: Int, callback b: (value: Int) -> Void){ // Callback with type and argument name
  ...
  b(2);
}

func some(a: Int, callback x: add){ // Callback with function signature
  ...
  x(2);
}

type CallbackTypeAlias = (value: Int) -> Void // Callback type alias

func some(a: Int, callback x: CallbackTypeAlias){ // Callback with type alias
  ...
  x(2);
}
```
`callback` modifier defines function pointer types, enabling interoperability with C-style callbacks and event systems. `CallbackType` is a special type for function pointers.

**Funções com Side Effects:**

```typescript
let count = 0

mut action(){ // 'mut' keyword marks function with side effects
  count++
}

return mut () => { // Anonymous function with side effects
  count++
}
```
`mut` keyword marks functions that have side effects, modifying variables outside their local scope.

**Function Configuration:**

```typescript
declare configX = <W, .gpu, .heap> // Declare a function configuration type

fn<configX> call() {} // Apply configuration type to a function
```
Function configurations can be declared using `declare configX = <...>` and applied to functions using `fn<configX>`.

**Asm Functions:**

```typescript
fn<asm ("MYFUNC")> funcaoAsm() Int // Declaração de função assembly externa
```
External assembly functions can be declared using `fn<asm ("MYFUNC")>`.

## Módulos

**Declaração de Módulo:**

```typescript
module NomeDoModulo { // Module declaration
  // Funções, constantes, tipos, etc.
  export func funcaoDoModulo() { ... }
  export const CONSTANTE_MODULO = 123

  export { // Exportando múltiplos itens em um bloco
    funcaoDoModulo,
    CONSTANTE_MODULO,
  }

  export default { // Export default para valor padrão do módulo
    versao: "1.0.0"
  }
}
```
Modules are declared using the `module` keyword and act as singletons. Module names are case-sensitive and lowercase ASCII.

**Import de Módulos:**

```typescript
import { funcaoDoModulo, CONSTANTE_MODULO } from "NomeDoModulo" // Import seletivo
import NomeDoModulo from "NomeDoModulo" // Importa módulo como namespace
import * as ModuloAlias from "NomeDoModulo" // Importa com alias
import { a,b,c } as lili from 'lulu' // Import with alias and selection
include 'bababa' // Include module content in current module namespace
include 'bababa' as nanana // Include module with namespace renaming
include ninini from 'hahaha' // Include specific export from module
import * from 'bababa' // Include all exports, same as include 'bababa'
import lilili from lololo // Module rename, module names are global variables
import math from 'https://libs.w.org/supermath@1.2.3/math.w' // Import from URL with version
import math from 'supermath' // Import using defined name
import { coisa } from someModule(args) // Import with module function call
import { coisa } from someModule --arg like -cli=true // Import with module CLI arguments
import 'supermath' from 'https://libs.w.org/supermath@1.2.4/math' // Import with extension type preference (.a, .dyn, .w)
import a from "a" // Import module 'a' as 'a' variable
import type a from "a" // Import only type definitions from module 'a'
import type { someClass } from "a" // Import specific type definitions from module 'a'
import fork { func as func_alt } from 'X' // Import forked module with alias
import { X , Y , Z } from 'alphabet' with { config, ....} // Import with configuration override
```
Modules are imported using `import` and `include` keywords. `import` makes the module available as a variable, while `include` merges the module's content into the current namespace. Variations include selective imports, aliases, URL imports, and conditional imports. `fork import` creates a new instance of a module. `import ... with { config, ... }` allows overriding module configurations during import.

**Configurações de Módulo:**

```typescript
module MeuModulo {
  #config { // Configurações inline
    threads = [.background, .network] // Threads dedicadas
    memory = .rcu                   // Gerenciamento de memória RCU
    dynamic.maxSize = 1G        // Limite de memória dinâmica
  }

  #config threads = [.background, .network] // Shorthand para config única
  #config memory = .rcu
  #config dynamic.maxSize = 1G
}

module Rede {
    #config {
        dynamic.maxSize = 1G        // Limite de memória dinâmica
        threads = [.background, .network]
        callPolice = .rcu           // Read-Copy-Update para concorrência
        #threads: .module // Module dedicated threads
        #dynamicThreads: 100 // Dynamic threads limit for module
    }
}
```
Module configurations are defined using `#config` blocks, controlling threads, memory management, and other resources. Configurations can be inline or shorthand.

**Entry Points de Módulo:**

```typescript
module Principal {
  export func main(args: String[]) { // Entry point principal para executáveis
    print("Executando módulo principal com argumentos: ${args}")
  }

  export func fetch(request: Request, context: Context) -> Response { // Entry point similar ao Cloudflare Workers
    return new Response("Resposta do módulo W!")
  }

  export default func() { // Default export como entry point alternativo
    print("Entry point default do módulo")
  }

  export entry { // Entry block for multiple entry points
    main(){
      // ...
    }
    cli(){
      // ...
    }
    ipc(req, conn){
      // ...
    }
    ...
  }
}
```
Modules can define multiple entry points, including `main` for executables, `fetch` for service workers, and `default` for general module entry. `export entry { ... }` block allows defining multiple named entry points.

**Module Lifecycle:**

```typescript
module SomeModule {
  module.init = initFunctionName // Custom module initialization function name
  module.deinit = deinitFunctionName // Custom module deinitialization function name

  export func init() { // Default module initialization function
    // ...
  }

  export func deinit() { // Default module deinitialization function
    // ...
  }

  export func release(moduleName: String) // Unload a module by name
  export func release(moduleVar: ModuleType) // Unload a module by variable
}
```
Modules have a lifecycle with `init` and `deinit` functions. `module.init` and `module.deinit` can be used to customize the names of these functions. `release()` function unloads a module.

**Module Naming:**

Module names should be ASCII, lowercase, and case-sensitive. Real name of variables and functions in WC is `moduleName_variableOrFunc`.

**Module Compilation:**

Modules are compiled as static libraries. The main module is linked with a template main function that calls the module's entry point and constructor.

**Module Export:**

```typescript
module SomeModule {
  export callThing // Export function
  export callThing as thing // Export function with alias
  export { callThing as thing } // Export function with alias in block
  export hide_export { callThing_SF } // Hide export from module interface
}
```
`export` keyword is used to make functions, constants, and types available outside the module. `hide_export` block hides exports from the module's public interface.

## Objetos (Classes e Structs)

Em W, `class` e `struct` são keywords usadas para declarar tipos complexos. `class` declares reference types, and `struct` declares value types. `object` keyword is used to define protocols or interfaces, and for generic object references that can be either class or struct instances.

**Declaração de `class` (Tipo por Referência):**

```typescript
class NomeDaClasse : SuperClasse, Protocolo1, Protocolo2 { // Class declaration with inheritance and protocols
  public let propriedadeImutavel: Tipo // Public immutable property
  private var propriedadeMutavel: Tipo // Private mutable property

  init(parametro1: Tipo, parametro2: Tipo) { // Constructor
    this.propriedadeImutavel = parametro1
    this.propriedadeMutavel = parametro2
  }

    init.completo(param1: Tipo, param2: Tipo, param3: Tipo) { // Named constructor
        this.propriedadeImutavel = param1
        this.propriedadeMutavel = param2
        // ... outras inicializações
  }

  public func metodoDaClasse() { // Public method
    // ...
  }

  mut public func metodoMutavel() { // Public mutable method
      this.propriedadeMutavel = novoValor //permitido apenas em func mut
  }

  static constants->{ // Static constants block
    undefined = 'undefined'
    null = 'null'
    unitialized = 'undefined' // e.g
  }

  Property1:: SomeType // Default property for type association
  Property2: SomeType // Regular property
  Property3:2: AnotherType // Property with index 2 for layout purposes
  Property1:-: SomeType // Property with default layout

  _id: .pointer // ID property with pointer type
  _state: .state(name: string, age:number) // State property for persistence

}

// Instanciação
let objeto = NomeDaClasse(parametro1: valor1, parametro2: valor2)
let objetoCompleto = NomeDaClasse.completo(param1: val1, param2: val2, param3: val3)
```
`class` keyword declares reference types. Classes support inheritance, protocols, constructors (`init` and named `init.name`), methods (public and private, mutable with `mut`), static constants, and property configurations. Properties can be declared as `public` or `private`. Property configurations include default properties (`::`), indexed properties (`:index:`), and default layout properties (`:-:`). `_id` and `_state` properties are special properties for object identity and persistence.

**Declaração de `struct` (Tipo por Valor):**

```typescript
struct NomeDaStruct {  // Struct declaration
  let propriedadeImutavel: Tipo
  var propriedadeMutavel: Tipo

//Construtores não são obrigatórios em Structs
//Se todas as propriedades tiverem valores padrão, você pode usar a struct sem um construtor explícito.

  // Métodos (structs também podem ter métodos)
  func metodoDaStruct() {
    // ...
  }

  // Métodos mutáveis em structs (retornam uma nova cópia)
  mut func metodoMutavel() -> NomeDaStruct {
      var copia = this //cópia implicita
      copia.propriedadeMutavel = novoValor
      return copia
  }
}

// Instanciação
let minhaStruct = NomeDaStruct(propriedadeImutavel: valor1, propriedadeMutavel: valor2)
let structModificada = minhaStruct.metodoMutavel() // Retorna uma nova instância
```
`struct` keyword declares value types. Structs are similar to classes but are value types, copied on assignment. Structs can have methods, including mutable methods that return a new copy of the struct. Constructors are optional for structs.

**Declaração de `enum` (Tipo Enumerado):**

```typescript
enum DayOfWeek(var dayNumber: Int) { // Enum declaration with associated value
  MONDAY(1), TUESDAY(2), WEDNESDAY(3), THURSDAY(4),
  FRIDAY(5), SATURDAY(6), SUNDAY(7)
}

let day: DayOfWeek = .MONDAY // Enum case assignment

enum TargetType { // Enum without associated value
    /// A target that contains code for the Swift package’s functionality.
    case regular
    /// A target that contains code for an executable's main module.
    case executable
    /// A target that contains tests for the Swift package’s other targets.
    case test
    /// A target that adapts a library on the system to work with Swift packages.
    case system
    /// A target that references a binary artifact.
    case binary
    /// A target that provides a package plugin.
    case plugin
}

let target: Target = .executable // Enum case assignment

enum number() { // Enum as type with cases as constructors
  float(var f: float),
  int(var f: int),
  long(var f: long),
  ...
  bitInt(var f: long[]))
}

var coisa: number = .float(2) // Enum case constructor usage
var coisa: number = 2.0 // Shorthand for enum case constructor

enum PackageDependencies { // Enum for package dependencies
  .package(url: String, from: String)
}

let packageDep: PackageDependencies = .package(url: "https://github.com/gringoireDM/EnumKit.git", from: "1.1.0") // Enum case with associated values
```
`enum` keyword declares enumeration types. Enums can have associated values, and cases can be used as constructors. Enums can also be used to define types with enumerable constructors.

**Declaração de `object` (Protocolo/Interface):**

```typescript
protocol Desenhavel { // Protocol declaration using 'protocol' keyword
    func desenhar()
    var cor: String { get } // Propriedade apenas para leitura
    //var tamanho: Int { get set } //erro, protocolos não podem ter propriedades armazenadas
}

// Conformidade com protocolo
class Circulo : Desenhavel { //":" indicates inheritance and/or protocol conformance
    let raio: Float
    var cor: String = "azul" //precisa ter o var aqui

    init(raio: Float) {
        this.raio = raio
    }

    func desenhar() {
        print("Desenhando um círculo de raio ${this.raio} e cor ${this.cor}")
    }
}

let meuCirculo: Desenhavel = Circulo(raio: 5.0) // Protocol type usage
meuCirculo.desenhar()
print(meuCirculo.cor)
```
`protocol` keyword declares interfaces. `object` keyword can be used as a synonym for `protocol` when defining interfaces. Protocols define contracts that classes and structs can conform to. Protocols cannot have stored properties, only computed properties and methods.

**Métodos de Objetos:**

```typescript
object SomeObject {
  func someMethod() {
    // ...
  }
}

let obj = SomeObject()
obj.someMethod() // Method call

// Object Call Syntax Sugar
// In C backend: object_someMethod(obj);

```
Methods are functions associated with classes and structs. Object call syntax `obj.method()` is syntactic sugar for `object_method(obj)`.

## Interoperabilidade com C

**Import de Bibliotecas C:**

```typescript
import { funcao_c } from "c:biblioteca_c.h" // Importa função C
import { tipo_c } from "c:biblioteca_c.h"   // Importa tipo C
```

**Chamada de Funções C:**

```typescript
func usarFuncaoC() {
  let resultadoC: Int = funcao_c(argumentoW)
  // ...
}
```

**C Module:**

```typescript
module CModule {
  #C::MyNamespace { // C code block with namespace
    #include <stdio.h>
    int c_function(int arg) {
      printf("Hello from C! Arg: %d\n", arg);
      return arg * 2;
    }
  }

  export func callCFunction(val: Int) -> Int {
    return #C::MyNamespace.c_function(val) // Call C function with namespace
  }
}
```
`#C::NamespaceName { ... }` blocks allow embedding C code directly within W modules, with optional namespace specification.

## Sistema de Build

**Arquivo `build.w`:**

```typescript
module Build {
  func main() {
    let fontes = glob("src/*.w")
    let objetos = fontes.map(f => compile(f, "c"))
    link(objetos, "programa_w")
  }
}
```

**Comandos `w build`:**

*   `w build` - Compila o projeto.
*   `w run` - Compila e executa.
*   `w test` - Executa testes (a serem definidos).
*   `w package` - Empacota para distribuição.
*   `w release` - Empacota para release, using git tags for versioning.

## Testes e Debug

**Test Files:**

Test files are named with `.test.w` extension (e.g., `file.test.w`). They are treated as libraries imported in debug mode.

**Debug Files:**

Debug files are named with `.debug.w` extension (e.g., `file.debug.w`). They are similar to test files but allow side effects and are used for debugging and demonstrations.

**In-Place Tests:**

```typescript
//@(2,1) == 3 // In-place test comment
fn someFunc(Int a, Int b){
 return a + b
}

fn someFunc(Int a, Int b){
 @(2,1) == 3 // In-place test assertion
 return a + b
}

// Some Docs about that func
// @a: Int, bla bla bla
// @b: Int, bla bla bla
// @@: Int, sum of bla bla bla
// @(2,2) == 4 // In-place test with JSDocs like arguments and result

// Some Docs about that func
// @a: String, bla bla bla
// @@: SomeComplexObject, bla bla bla
// @a = "Very Complex sample"
// @@ = SerializedForm from SomeComplexObject
// @(a) == @return // In-place test with complex arguments and results

@Test("Continents mentioned in videos", arguments: [ // Test function with arguments
  "A Beach",
  "By the Lake",
  "Camping in the Woods"
])
func mentionedContinents(videoName: String) async throws {
  let videoLibrary = try await VideoLibrary()
  let video = try #require(await videoLibrary.video(named: videoName))
  #expect(video.mentionedContinents.count <= 3)
}

@debug("variavel que na verdade vai mudar em debug a primeira vez que rodar esse aquivo") // Debug variable annotation
mut someFunc(string a){
 @debug("posso me chamar também sempre que executa a função em debug") // Debug function call annotation
 @debug someOtherFunc() // Debug function call annotation
 #debug ("a") == Result.OK // Debug assertion with Result type
 return new SomeComplexObject()
}

var a = someFunc()
if (a == Result.OK){ // Check Result.OK
  a.value == "blabla" // Access value from Result.OK
}

guard let b = someFunc, where b == Result.err(504){ // Guard with Result.err check
  //....
  // outside guard b is Result<T>.value
}

if let c = someFunc() { // if let with Result.OK check
  //.... c is value T
  // if let only enters if result is okay, so b is always value
}
```
In-place tests can be added as comments or code annotations using `@(...) == ...` syntax. `@Test` annotation defines test functions with arguments. `@debug` annotation marks code for debug-only execution. `Result<T>` type is implicitly used in debug mode to handle function results, with `Result.OK` and `Result.err` cases.

## Comptime

**Comptime Functions:**

```typescript
var someValue = #func() // #func is called at compile time

var someValue = #func("static string") // comptime function with static string argument
var value = someInput() using(max_value:30000) // using clause helps compiler find bounds
var value = someInput() using(max_value:30000, boundChecking: true) // boundChecking forces runtime test
declare type myInt = int using(max_value:10000) // declare type with using clause
declare type myInt = int32 using(boundChecking: true) // declare type with boundChecking
declare type age = int using(max_value:1000) // declare type with using clause

guard someNumber { // guard block for runtime overflow checking
  // overflow in runtime, can be handled here.
}

var obj = SomeObject { literalPro1 = value, literalProp2 = 42, ...} // Wlon literal object initialization

var someArray = [{someLabel, 33}, 44, 55, {lolo, 66}] // Wlon array with labels

```
`#func()` executes a function at compile time. Comptime functions can be used for code generation and static computations. `using` clause helps the compiler infer type bounds. `declare type ... using(...)` declares types with bounds and runtime checks. `guard someNumber { ... }` block handles runtime overflow checks. Wlon (W Literal Object Notation) is used for literal object and array initialization, and for comptime function return values.

## Serviços

**Service Declaration:**

```typescript
service NameOfService { // Service declaration
  name: idLikeName // default service name
  context: Network | Process | Thread // default service context
  protocol: Http | TCP | WS | SharedMemory | Custom // default service protocol
  onStart(){ // default onStart handler
    //
  }

  anyFunction(anyArg){ // Service function

  }

  handler(genericProtocolArg){ // default message handler
    genericProtocolArg.type
    genericProtocolArg.data

    return data
  }
  async call(.methodToCall(args)) // RPC/ITC call
  send(genericSendArg) // Send generic message
  send(.context(ctx)) // Send context message
  async receive(.context(ctx)) // Receive context message

  onFinish(){ // default onFinish handler
    //
  }
}
```
Services are declared using the `service` keyword and provide a unified way to handle IPC/ITC, RPC, and other communication methods. Services define lifecycle handlers (`onStart`, `onFinish`), message handlers (`handler`), and functions for communication (`call`, `send`, `receive`).
