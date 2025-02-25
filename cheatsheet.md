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
var nomeMutavel = "W Lang"   // Variável mutável
const PI = 3.14159         // Constante global (nível de módulo)
```

**Tipos de Dados:**

```typescript
Int         // Inteiro (tamanho dependente da arquitetura, padrão i32/i64)
Int<size>   // Inteiro com tamanho específico (e.g., Int<16>, Int<32>)
Float       // Ponto flutuante (padrão double)
Float<size> // Ponto flutuante com tamanho específico (e.g., Float<32>, Float<64>)
String      // String UTF-8
Char        // Caractere Unicode
Bool        // Booleano (true ou false)
Void        // Tipo vazio (sem retorno)
Any         // Tipo dinâmico (inferência em tempo de execução)
```

**Tipos Opcionais:**

```typescript
let usuario: String? = nil  // Variável String que pode ser nula
let idade: Int?          // Variável Int opcional, valor inicial nil
```

**Tipos Restritos:**

```typescript
type NomeUsuario = String<maxLength: 20, pattern: /^[a-zA-Z0-9_]+$/>
type IdadeValida = Int<range: 0...120>
```

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
    object Usuario { //"object" é usado para declarar classes e structs
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

```

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
```

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
```

**Modificadores de Parâmetros:**

```typescript
func processarDados(dados: String<.ref>, arquivo: String<.storage>, cache: String<.cow>) {
  // ref: Referência mutável (in-out)
  // storage: Transferência de posse
  // cow: Copy-on-write
}
```

**Chamando código em outras linguagens:**

```typescript
// Importando uma função C da biblioteca padrão
import { printf } from "c:stdio.h"

func dizerOiC(nome: String) {
  printf("Olá, %s!\n", nome) // Chamando printf diretamente
}

// Chamando código JavaScript (exemplo hipotético)
import { jsFunction } from "js:modulo_js"

func usarJs() {
  let resultado = jsFunction("Olá do W!")
  print(resultado)
}

// Chamando código Rust (exemplo hipotético)
import { rustFunction } from "rust:modulo_rust"

func usarRust() {
  let resultado = rustFunction(10, 20)
  print("Resultado Rust: ${resultado}")
}
```

## Módulos

**Declaração de Módulo:**

```typescript
module NomeDoModulo {
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

**Import de Módulos:**

```typescript
import { funcaoDoModulo, CONSTANTE_MODULO } from "NomeDoModulo" // Import seletivo
import NomeDoModulo from "NomeDoModulo" // Importa módulo como namespace
import * as ModuloAlias from "NomeDoModulo" // Importa com alias
```

**Configurações de Módulo:**

```typescript
module MeuModulo {
  #config { // Configurações inline
    threads = [.background, .network] // Threads dedicadas
    memory = .rcu                   // Gerenciamento de memória RCU
  }

  #config threads = [.background, .network] // Shorthand para config única
  #config memory = .rcu
}

module Rede {
    #config {
        dynamic.maxSize = 1G        // Limite de memória dinâmica
        threads = [.background, .network]
        callPolice = .rcu           // Read-Copy-Update para concorrência
    }
}
```

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
}
```

## Objetos (Classes e Structs)

Em W, `object` é a palavra-chave usada para declarar tanto classes (tipos por referência) quanto structs (tipos por valor). A distinção principal é o comportamento de cópia e mutabilidade.

**Declaração de `object` (Classe - Tipo por Referência):**

```typescript
object NomeDaClasse { // Por padrão, é um tipo por referência (classe)
  let propriedadeImutavel: Tipo
  var propriedadeMutavel: Tipo

  // Construtor (init)
  init(parametro1: Tipo, parametro2: Tipo) {
    this.propriedadeImutavel = parametro1
    this.propriedadeMutavel = parametro2
  }

    // Construtor nomeado
  init.completo(param1: Tipo, param2: Tipo, param3: Tipo) {
        this.propriedadeImutavel = param1
        this.propriedadeMutavel = param2
        // ... outras inicializações
  }

  // Métodos
  func metodoDaClasse() {
    // ...
  }

  mut func metodoMutavel() {
      this.propriedadeMutavel = novoValor //permitido apenas em func mut
  }
}

// Instanciação
let objeto = NomeDaClasse(parametro1: valor1, parametro2: valor2)
let objetoCompleto = NomeDaClasse.completo(param1: val1, param2: val2, param3: val3)

```

**Declaração de `object` (Struct - Tipo por Valor):**

```typescript
object NomeDaStruct {  // Comportamento de struct (tipo por valor)
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

**Protocolos (Interfaces):**

```typescript
protocol Desenhavel {
    func desenhar()
    var cor: String { get } // Propriedade apenas para leitura
    //var tamanho: Int { get set } //erro, protocolos não podem ter propriedades armazenadas
}

// Conformidade com protocolo
object Circulo : Desenhavel { //":" indica herança e/ou conformidade
    let raio: Float
    var cor: String = "azul" //precisa ter o var aqui

    init(raio: Float) {
        this.raio = raio
    }

    func desenhar() {
        print("Desenhando um círculo de raio ${this.raio} e cor ${this.cor}")
    }
}

let meuCirculo: Desenhavel = Circulo(raio: 5.0)
meuCirculo.desenhar()
print(meuCirculo.cor)
```

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

---

**Nota:** Este cheatsheet está em desenvolvimento e pode mudar. Consulte a documentação completa para informações detalhadas.
