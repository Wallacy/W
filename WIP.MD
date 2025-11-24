```typescript

fn({ a: string, b: number}){ // implicit args property and variable destructor
  
  return 0
}

const x = "x"

fn(args:SomeType){
  let { a, b } = args
  return false
}

fn(variable:SomeType){
  let { a, b } = variable
  return false
}

let count = 0
fn(SomeType){
  let ${ a, b}  
  return { count+b, `${x}-${a}` }
}

type FType = { a: string, b: number} 

fn(FType): string {
  let ${ a, b}
  return `${x}-${a}`
}

type RType = { c: number, d: string} 
fName(FType):RType{
  let ${ a, b}
  
  return false // error...
}

type FType2 = { a: string?, b = 0 } 

fn(FType2){
  let ${ a ?? 'default', b==0 ? 2, c > 1 ?: 0 }
  return `${a}-${b}-${c}`
}

// FuncReturnType
// FuncArgs
int func(*FuncReturnType,FuncArgs){

}
// return an error code as a return value and everything else as a parameter in the function,

fName(FType){
  ${ a, b}
  import let { w, x } from('coisa')
  
  return w+a * b*x
}

//
int fn(int* a){
    *a=33;
    return 2;
}

int main()
{
    int* a = alloca(sizeof *a);
    int b = fn(a);
    printf("Hello World %d %d",b,*a);

    return 0;
}
//

const == c const
let = variavel imutavel igual swift
var = variavel mutavel.

//

import { coisa } from 'outraCoisa'
import lalala from 'lololo'
import * as lili from 'lulu' // same as import lili from 'lulu
import { a,b,c } as lili from 'lulu' // apenas { a,b,c } vão fazer parte de lili
include 'bababa' // semelhante include do C, usando bababa namespace
include 'bababa' as nanana // mudando module/namespace
include ninini from 'hahaha' // vai incluir só ninini e derivados
import * from 'bababa' // same as [ include 'bababa' ]
import lilili from lololo // modules/namespace são variaveis globais, aqui é apenas um rename.

// namespaces/modulos são globais e podem ser referenciados por seu nome canonico em string nos imports. [ps: provavelmente não faz sentido usar string]

//

export x
export let a = 'a'  // const, var
export default {

} // ou main, não os dois
// export default tem que ser um objeto ou uma função. Ela substitui o main no no caso dos modulos.

process.args // parseado / yargs/cli like
process.rawArgs
process.modules // todos os modulos ativos em memoria
process.memory // cpu etc.

*globalVar* // Variaveis que referenciam bibliotecas dinamicas, nome dos modulos ets. Tudo que é importado é considerado um modulo, inclusive libs.

moduleName.variable // variavel publica em algum modulo, use o import { variable } como forma de trazer injetar só a variavel no contexto local
// importar um modulo permite renomear o nome da biblioteca ou modulo definido pelo compilador.


file1.w
module name

// impl

file2.3
module name

//impl

// Modulos são tratados como o mesmo arquivo. Não podem conter top level funções/variaveis etc iguais.

// Só se importa modulos, não arquivos.
// É possivel refereciar o nome de qualquer modulo como se fosse uma variavel global sem importar nada.
// Namespace é apenas uma questão de organização, mas no final é um conjunto de modulos.

namespace > module > arquivo

// Um aquivo pode ter varios modulos
// deve-se importar sempre o de maior grau se um modulo faz parte de um namespace deve-se importar o namespace primeiro

// um executavel por ter varios entry points, cada um pode ser um modulo separado.
// quando se compilado um arquivo, o entry point deverá ser apontado (um arquivo/modulo/namespace) ele será chamdo por padrão,
// porém se chamar o main/default de cada modulo depois

// Sistema de pacotese gstreamer like, onde todo os pacotes da comunidade são separado em niveis
std // modulos de nivel 1 - suporte direto da linguagem [suporte garantido]
library // modulos de nivel 2 - suporte suporte compartilhado da linguagem e comunidade [suporte garantido]
modules // modulos de nivel 3 - suporte da comunidade [suporte da comunidade esperado]
packages // modulos de nivel 4 - pacotes da comunidade sem garantia de suporte

// nivel 0 é a linguagem em sí.

import { http } from std // mesmo que std.http
include http from std // vai tornar o conteudo de std no modulo corrente, regras de modulo continuam se aplicando, ou seja, pode haver colisão

import { ffmpeg } from library
import { gst } from modules

// std/library/modules/packages possui uma lista de pacotes a serem referenciados a qualquer momento offline. Quando compilado só o que foi usado vai ser importado.

// O sistema vai manter apenas o tracker do hash do modulo, se qualquer coisa mudar no modulo ele deve alertar. SemVer é obrigatorio.

Qualquer outro modulo deverá ser referenciado na build e acessado no codigo em um bind nomeado no processo de build. O bind será uma variavel global ou uma string com mesmo nome.


modulos init => init/main/default // todos são palavras reservadas a não ser que definidos de outra forma em module.init = nome ou 
modulos deinit => deinit/dealoc // todos são palavras reservadas a não ser que definidos de outra forma em module.deinit = nome

modulos podem ter quantidade de memoria maxima definida // module.memory = 128M

// process.memory é igual a soma de todos os module.memory
// process.cpu tem que acessar todas as cpus referenciadas em module.cpu // default = auto, usar cpuafinity like [ 1,2,4 ]

module.memory.min
module.memory.max
module.memory.base
module.memory.current
module.memory.flush() // flush heap memory
module.memory = 64M  // === module.memory.base = 64M


module.memory.max = system.memory

// module.memory considera heap;
// modulos e objetos possuem propriedades padrões no get/set, por exemplo module.memory.get == module.memory.base.get (mesmo para o set)

{
  get: a.get //func
  set: a.set
  a,
  b,
  c
}
//
{
 [get, set]: a[get. set] // aqui usando sitaxe de array para atribuição mulplica, a.get poderia ser a.someFn
}
//
{
 [get, set]: a // object, nesse caso ele vai pedir get e set de a se houver. o mesmo poderia ser feito para mapear qualquer função
}


memoria e outros recursos são gerenciados por modulo,
modulos são executados como scripts, liha por linha. init/deinit são chamados após executar/ler todo o modulo, modulos são singletons. Se não exportar nada não vão executar nada novamente;

process.flush(module) // força o deinit do modulo e aplica o flush na memoria

proxima vez que o modulo for chamado usando a variavel ele vai ser carregado novamente

process.flush é chamado automaticamente após a ultima referencia de um modulo (mesmo dentro de outro modulo)
process.flush não é capaz de deinicializar um modulo usado em paralelo.

//
Uma vez um modulo inicializado a quantidade de memoria maxima é definida. Só na proxima incialização que da para trocar.
Todas as variaveis fora da stack só podem consumir esse maximo de memoria, na pratica:

var a = {}
// =
module->{
  a: {}
}

Tudo é na verdade referenciado na struct pre alocada do modulo
c max

//

Objetos são sempre flat, ou seja, como structs. Funções do super são renomeadas na hora de compilar, e ou é feito um marshal para a função quando não for externo como o caso de uma lib.
O sistema deve manter o track de quanto de memoria cada objeto pode ocupar no maximo. e alertar se passar o maximo do modulo em tempo de compilação.

objetos/strucs/enums podem ser extendidas, fazer herança e podem ter construtor; Caso não tenha construitor, nesse caso não precisa usar `new`;

// Penso em tudo ser como o enum do swift

memorias no heap em funções são na verdade parte da estrutura do modulo // ??

fn(SomeType){
  ${ a, b}  
  let someBigVar = new SomeTypeToHeap(a,b)
  // something with someBigVar
  return someBigVar
}

module.fn.someBigVar = new SomeTypeToHeap(a,b)

// Pensei em ter em paralelo um HeapStack padrão para extender a Stack normal para não ter que mexer nela.
// Poderia usar os tipos para ter controle dos objetos, paginar isso de acordo com os modulos/namespace

...
//

Explorar a possiblidade de usar o xxhash para gerar os nomes das propriedadesm, funções (possivelmetne variaveis)
Com isso poderia um enum se for atualizado entre versões não vai ter colisão. Não precisaria guardar os nomes das propriedades nos objetos nem nas estruturas.
//XXH3 is bijective for len==8. It guarantees no collision.
// XXH128 is bijective for len==16, porém len==256 no collision by test
// variaveis maiores podem gerar uma combinação de hash de 16 ou de 8, basta ter uma forma de declarar isso no C depois.

// A vantagem de usar um hash é que podemos aceitar qualquer unicode como simbolo/nome/identificador só precisamos manter o maximo de 255 caracteres.
// Uma flag poderia dizer se é melhor usar o 64/32 bits e qual o padrão da quebra.

//

No linux poderia se manipular a stack como parte do module.memory, isso pode gerar flexbilidade. Pois é interessante do sistema criar a propria stack/ringbuffer para o resto da heap.

//

Outra ideia é usar auto e porcentagens para alocar coisas na stack/memoria

// A ideia é aproveita o maximo da stack nesse caso
fn(){
  let a = [auto] // compilador tenta adivinhar um bom valor
  let b = [.u] // compilador usa pesos nos valores
  let c = [.5u]
  let d = [2u]
  let e = 55 // e is type is number e o compilador vai tentar usar o menor tipo automagicamente
}
//

number.float
number.int
number.long
..etc

// Dentro do number ele pode usar o tipo que foi usado para associar para colocar urmazenar, tipo um enum com payload onde cada tipo é uma das possibilidades do number

enum number() {
  float(var f: float),
  int(var f: int),
  long(var f: long),
  ...
  bitInt(var f: long[])) // ou seja lá qual vai ser implementação do bigint
}

var coisa: number = .float(2)
===
var coisa: number = 2.0

//

Variaveis exportadas poderiam estar no data segment e serem consideradas globais como no C intermediario. 

Pensar se vale a pena representar o modulo como uma grande função para poder compatilhar facilmente dados da stack sem fazer copia
Blocos podem ser usados como forma de isolar o body das funções, as fariaveis re retorno e chamada das funções poderiam ficar fora do body, dessa forma não precisaria de ponteiros dentro do modulo
Tudo que for usado no heap poderia ser desalocado por padrão quando sair do contexto igual a stack, a não ser que seja apontado um contexto de desalocação, algo como "de free nessa quando essa outra morrer" e por aí sucessivamente. Nos objetos seria no dealoc e eles seriam no contexto apontado, como por exemplo o modulo, que poderia ser o padrão. Tudo que não foi exportado no modulo morre com o ele.

Uma alternativa é fazer com que funções usem apenas o stack e actions/`services` consigam usar o heap
services serial a contrapartida heap de objetos/stucts, e actions a contrapartida das funções.

Verificar também se faz sentido split-stack nessa linguagem.

//

SomeType {
  //
}

SomeType.constants->{
  undefined = 'undefined'
  null = 'null'
  unitialized = 'undefined' // e.g
}

if let aa {
  // test for undefined/null/unitialized => exists with actual info
}

if aa ~= 'condtion/regex/glob/pattern' {

}

var bb = [...]

if bb ~= 'someElementMatch' {

}

fomeFunc(b){
  //
}

if bb ~= someFunc {
 // exit on first true
}

if bb ~= (b) => {
 // exit on first true
}

let someVar = bb ~= (b) => {
 // exit first non undefined return
}

valueOf someVar == 'undefined'
typeOf someVar == ActualType?
ActualType.contants.null == 'undefined'

if (someVar == null){
  //okay for 'undefined'
}

'unitialized' is only for lifecycle, during the new ActualType(...)

// ===
bb.forEach(b =>{
  if (true){
    return;
  }
})


int/float/string

string.constants.unitialized == ""
string.constants.undefined == 'null'
string.constants.null == 'null'
string.constants.empty == ""

int.constants.unitialized == 'unitialized'
int.constants.undefined == 0
int.constants.null == 0
int.constants.empty == 0

number.constants.unitialized == 'unitialized'
...

*(number.constants.unitialized) = &constUnitialized // `C`

//
if (someVar == .undefined){
  // seria bacana tratar todos os tipos como se fossem enuns com valores
}

if (someVar == .empty){
}

if (someVar == "valor" && someOther == 0){
  // qualquer coisa que não use o . é implicito o campo "value" / "valueOf"
} 

// Se tudo for enum, o ponteiro da variavel pode ser usado em um map/hashMap para encontrar o valor. Semelhante ao associate object. Algo que também poderia ser feito nas extenções.

//

// Pensar um pouco aqui, mas esse unitialized pode resolver o problema do force-unwrap

//

enum DayOfWeek(var dayNumber: Int) {
  MONDAY(1), TUESDAY(2), WEDNESDAY(3), THURSDAY(4),
  FRIDAY(5), SATURDAY(6), SUNDAY(7)
} // fazer como no Kotlin / dayNumber will be a property with get/set; Isso se eu não encontrar uma forma melhor que encapsular. Como Property Behaviors / Property Wrappers

object Text(var body: String)
obj Image(let url: String, var caption: String)
// cada propriedade vai ter o construtor implicito get/set

// object / obj / enum / class / struct ?  Penso em deixar tudo como obj e deixar tudo como o enum com payload, adicionar funções etc.
// Fico pensando em como ligar com a questão da referencia vs valor, mas acho que da pra ser algo mais automatico.

// a function can either modify its inputs or create an output, not both

////

Structured concurrency // ver swift

fn makeDinner() async throws: Meal {
  let veggies = try await chopVegetables()
  let meat = await marinateMeat()
  let oven = try await preheatOven(temperature: 350)

  let dish = Dish(ingredients: [veggies, meat])
  return try await oven.cook(dish, duration: .hours(3))
}
// É concorrente mas não paralelo ==>
fn makeDinner() async throws: Meal {
  async let veggies = chopVegetables()
  async let meat = marinateMeat()
  async let oven = preheatOven(temperature: 350)

  let dish = Dish(ingredients: await [try veggies, meat])
  return await oven.cook(dish, duration: .hours(3))
}
// <== É concorrente e paralelo 

Com o lançe dos tipos, veggies por exemplo pode ser do tipe `async Vegetable` (ao invés de uma Promise<Vegetable>) e você pode pedir para a variavel veggies coisas do tipo:

async seria uma propriedade reservada na linguagem:

async<.cancel> veggies // seria noOp em variaveis não assync.

async let coisa = nonAsyncFunc() // Iria rodar async mesmo sem a marcação async

// async no nome da func seria apenas uma indicação de boa pratica por ser uma função potencialmente demorada. Já que da pra forçar o async de qualquer forma.

fn makeDinner() throws: Meal {
  let veggies = try chopVegetables()
  let meat = marinateMeat()
  let oven = try preheatOven(temperature: 350)

  let dish = Dish(ingredients: [veggies, meat])
  return try oven.cook(dish, duration: .hours(3))
}
// <== Forçaria ser sincrono! Mesmo veggies podendo ser async.

// Pensei que se uma task for cacenlada e tentar ler sem conferir, poderia dar throw automatico.
// Checar o unitialized poderia ser o suficiente para evitar throw nesse contexto e continuar o codigo
...
valueOf dish == .unitialized
console.log(throwOf dish) // dish is unitialized because meat is unitialized
...

// Acho que por padrão todos os init/contrutories poderiam fazer isso automatico com as variaveis que recebe

async<.cpu(1,3)> let veggies = chopVegetables() // boa forma de fazer afinidade
async<.cpus(2)> let veggies = chopVegetables()

// resumo
async let variable = someFunc()
async let variable = new Coisa()
async let variable = 1+2
// mesma coisa:
let variable = async someFunc()
let variable = async new Coisa()
let variable = async 1+2
//
let variable = await someFunc()
let variable = await new Coisa()
let variable = await 1+2
// Não é a mesma coisa
let variable = sync someAsyncFunc()
let variable = sync new Coisa()
let variable = sync 1+2
//

// sync tem o mesmo efeito de apenas chamar apenas a função, porém a vantagem de ser mais claro que você está serializando na mesma thread intencionamente uma função async; Isso pode ser interessante dependendo das regras do linter;

// Chamar someAsyncFunc() sem sync ou await gera um warning, já chamar someFunc() (consdierando que não é marcado como async) com sync é um noOp e pode gerar um warning também.

//

Faz um certo sentido em começar usando o Tcc (Tiny C Compiler) via libtcc; Já começar com uma linguagem que consegue fazer boostrap de sí mesmo como é uma grande vantagem. Assim como é o caso do VLang;

//Eu até me basearia no vlang, e dos dos motivos que estou fazendo o W é porque não gosto de varias decisões do V como linguagem, muitas promessas e varios testes que fiz peguei varios problemas.
// Mas ainda pode ser interessante fazer um bootstrap inicial em V (half W !!) até ter a primeira versão capaz de compilar a sí.
// Porem o ideal seria usar varias coisas, para ir o mais rapido possivel incluindo o fasm (flat assembler)

// Começar com o Clang/LLVM gera mais performance btw;

// Baseado nisso: https://blog.cloudflare.com/branch-predictor/

// Em Intel/AMD é interessante manter hot-code abaixo de 16KiB e ARM como o M1 abaixo de 4KiB.

//
// Caso eu armazena os objetos como o exemplo do enum_pay;
// No caso das libs eu poderia usar o xxhash (32 > 63 > 128 de acordo com as colisões) para gerar mais um nivel de indireção mas garantir que uma call entre enums/objetos diferentes;
// No caso dos executaveis finais não precisa, basta usar os indexes inteiros do enum mesmo.
//
// Tudo provavelmente pode ser enum, mas provavelmente terei que usar structs para cada tipo guardar os ponteiros;
//
// Outra coisa que pensei é que cada modulo poderia guardar as informações do que ele, o modulo coloca na HEAP;
// Por padrão o processo teria uma Stack de 2-8M o que deve caber em varias caches L2; Penso em haver algum tipo de config para targets dai o programador poderia ajustar as contrains baseano nisso. (só no pthread consigo alterar o __stacksize dinamico no linux/mac)
// Seria legal o linter e o proprio processador ajustar a organizar o que vai na stack e o que vai no hot code; Se tudo for "enum" exceto obviamente os ponteiros das funções, deve caber muita coisa
// 
// Esses blocos seriam compartilhados entre os modulos. A principio cada modulo pega um, mas com o uso eles podem ser comapatilhados.
// Existe a possbilidade de cada bloco ter sua "Heap" fixa, o que facilita o desenvolvimento também. Dai cada um poderia especificar quanto precisa de Heap min/max;
// A compatição disso via processo poderia ser um merge/analise da call tree pra saber quantos blocos de Heap vão estar disponiveis no maximo em dado momento.
// Considerando que vamos usar concorrencia estruturada acho que é bem tranquilo descobrir o ponto maximo de paralelismo;
// Claro que com o dinamismo teoricamente ele vai perceber que não pode chamar recurisvamente um função async que chame infinitas threads, mas isso também pode gerar um warning em dev time, o que é legal!
// Em casos de recursão o programador deveria "provar" ou "prometer" ao compilador que está dentro dos bounds
//
// Ainda sobre memoria, meus pensamentos parecem estar querendo reimplementar o malloc... Nesse caso poderia apenas usar o mimalloc que parece ser muito bom e da para criar varios Heaps. Um para cada modulo parece resolver meu problema. Além do mais parece ter uma boa implementação do calloc, que é mais seguro no geral.
// O mais importante é o recurso de crair novas heaps e deletar tudo junto. Isso vai bem de encontro com o que preciso para os modulos.
// https://github.com/microsoft/mimalloc


fn makeDinner() async<.max(8)> throws: Meal {
  let veggies = try chopVegetables()
  let meat = marinateMeat()
  let oven = try preheatOven(temperature: 350)

  let dish = Dish(ingredients: [veggies, meat])
  return try oven.cook(dish, duration: .hours(3))
}

// Nesse caso o pool de thread implicito da função makeDinner pode chegar até 8 threads no maximo, desse ponto, funções que chamam ( await makeDinner() ) teriam que aguadar na fila

// No caso do `fn makeDinner() async(.max(8)) throws` Isso poderia ser considerado `function composition` e na pratica ser:

throws<throwCtx,async<asyncConfig,fn makeDinner>>

//Dessa forma qualquer função que tiver um overload capaz de aceitar uma função de 'handler' poderia ser composta:

async<asyncConfig>
async<asyncConfig, handler>
throws<throwCtx>
throws<throwCtx, handler>

anyFunc()
anyFunc(handler)
anyFunc2(variable)
anyFunc2(variable,handler)

anyFunc() anyFunc2 async throws

// Par consumir

let dinner = try await makeDinner()

// Basicamente na ordem inversa do decladado

// Pensei em outras alternativas

fn makeDinner(): Meal async throws 
fn makeDinner(): Meal | async | throws 
fn makeDinner(): Meal | async<.max(8)> | throws
{
  
} 
//
// module.operator.async.max = 8;
// Dai não precisava aplicar isso em async(.max(8))
//

Provavelmente a ordem importa, o ideal era sempre ser  `async throws` ao invés de `throws async`, poderia ter um mecanismo de precedencia, mas acho que é uma complicação boba.
// Isso poderia ficar só como em boas praticas
//

// Apenas explorando o conceito:

// Poderia ser feito da mesma forma que é feito os operadores de procedencia. Até o momento pensei em deixar os operadores fixos, mas acho que para compor legal poderia criar essa regra;

`async` e `throws` também poderia ser vistos como operadores, dessa forma poderia garantir o `async throws`, operadores seriam funções que aceitam coisas opcionalmente de ambos os lados

// Generalizando funções compostas são operadores onde funções/operadores no RHS entra como handler;

operator √ prefix
operator ◉ infix
operator throws sufix 
operator ? ternary : // Poderia apenas criar também uma tabela de relacionamento. O que o operador ? faz quando o : está a direita ou ao contrario.

operator async infix(requires: .One)

// operator async(lhs: Function, rhs?: Function) // 
// operator async(lhs?: Function, rhs: Function) //

.operator->throws->{
  position: sufix
}

.operator.async.precedence->{
  lowerThan: throws
}

.operator.async->{
  handler<T>(lhs: Function, rhs?: Function): Function<T>{
    //
  }
  position: infix
  lowerThan: throws
}
//

// Se consideramos `module` o top level principal, tudo que vier depois pode ser chamado apenas .something como se o objeto modulo fosse `this`/`self`
// Funções fariam parte da constante 'module' (module.somefunc()) mas não precisaria do . pois seria implicito, operadores também não precisariam `operator` seria um operador também
// E o arquivo não precisaria estar sob {} pois o escopo é implicito // todo o arquivo.

//
//top-level keywords:
process
module
import
export
//

Pensando sobre o ->

Fica legal para o response na sintaxe do swift. Porém no geral prefiro digitar `: response` Quando compoe com async / throws fica bem melhor na resposta.
porem pensando no caso do `.operator->throws->` a setinha poderia indicar que ao invés de `operators` em operator você pode fazer um "select" em algum dos valores (sem interar)
//algumas tipos poderia ter um default outros não

stdout(`Operator: ${module.operator}`) // output ==> Operator: undefined
stdout(`Operator: ${module.memory}`) // output ==> Operator: 128M 

// module.memory por exemplo poderia ter o default para .memory.current, parecido com o que falei do get/set acima.
//
Considerando que module.operator não teria um `set` o compilador poderia gerar um erro quando tentar usa-lo diretamente sem o `select` já que seu valor sempre vai ser undefined

// Conceito base: https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
// Mais um projeto legal: http://dunkels.com/adam/pt/index.html
// https://github.com/LarryRuane/protothread-multicore
// https://github.com/nanomsg/nng
// https://github.com/iqiyi/libfiber || <= fazer teste contra libmill | Unico problema é a LICENSE

//
// Ter suporte a ao if verificador de contexto do typescript:
// const provider = getProvider() // Provider | undefined
// if (!provider) return
// provider.doIt() // provider: Provider
//
// throw sem stack rewind é uma coisa interessante na questão da gerencia de erro. Pois é bom ter uma mensagem clara dos erros sem ter que interferir nos tipos. E sem ter que explicitamente voltar undefined;
//
//

import { coisa } from someModule(args)
import { coisa } from someModule --arg like -cli=true

// Não sei se daria pra fazer isso com um executavel sem dar um spawn. Seria interessante ter como exportar a lib de um exe;
// Possivelmente na hora de compilar poderia escollher entre as tres opções, lib, exe, hibrido. Que na verdade seria um exe onde algum argumento especifico iria fazer o extract da lib.
// Poderia fazer um executavel base para cada plataforma e apenas da um append da lib, esse executavel base poderia ter seu hash já disponivel previamente na lib, dai esse "executor" poderia ser validado na lib, e esse executor iria receber a hash da lib pelo executavel real (que seria capaz de fazer parser da cli)...wip

// Sob o ponto de vista do problema do async:

Penso que a abordagem do Simon é muito boa, simples e direta usa só C puro e da pra ir para todas as plataformas. Pode-se expandir o mesmo conceito para algo mais completo gerando uma especie de event loop do main para as folhas da arvore permitindo que todo mundo processe um pouco algo assim no pseudo codigo

main(
  code = 0
  moduleHeap = new Heap()
  do {
    code += await someAsync(moduleHeap)
    code += await someAsync2(moduleHeap)
    // se ambos derem await o codigo vai ser 200
  } while (code > 0 && code % 100 == 0)
  cleanHeap(moduleHeap)
)
someAsync(
  var code = &(moduleHeap->someAsync->code) = 0
  do {
    code += await someAsync3(moduleHeap)
    code += await someAsync4(moduleHeap)
    // se ambos derem await o codigo vai ser 200
  } while (code > 0 && code % 100 == 0)
)
someAsync4(
  for/while/etc
  tudo terá um 
  yield() implicito
  yield também usado em algums pontos aleatorios quando o bloco é grande.
)

// Dai o codigo por ir indo e voltando em todas as funções, toda função chamada por await é responsabilidade de quem chama mandar um heap e todas as variaveis dessa ao invés da stack fica na heap que é enviada como contexto. Isso evita problemas de thread etc.
// Da para tratar uns erros, ver se é melhor olhar o codigo de retorno em todas as linhas ou só no while e fazer alguma operação pra saber em qual deu erro. O ideal é evitar olhar toda hora, poderia verificar os erros em um switch, com codigos simples é facil descobrir qual linha deu erro, 199, etc...
// Comunicação entre threads poderia ser apenas a questão dos seriviços e alguma area de meoria compartilhada. Se ficar bem definido quem escreve quem lê, quando voltar do yield para a mesma area da pra saber se a outra thread retornou finalizou ou não a execução.

// Funções com asinatura de async pode usar sempre a heap por padrão exceto se for usado o modificador sync do lado de chamada dai o compilador poderia gerar a mesma função sync.
// Funções sem assinatura async usa a stack por padrão mas o compilador tenta criar a versão com heap dedicada.

//C

int originalFunc(args){
  //...
}

int async_originalFunc(args){
  // things
  return originalFunc(args)
}

// Parece que boa parte disso já é feito na Protothread, verificar se vai dar o resultado esperado.

// Outra coisa dificilmente o threadpool será maior que o numero de cores x2; O sistema pode avaliar em tempo de compilação o max parallel level e em runtime ele escolher o tamanho do pool e criar logo no start da aplicação. Ver se é possivel criar uma thread secundaria para backaground e usar ela pra criar as outras thread e coisas no sistema.
// Mesmo no caso do single thread ter uma thread secundaria pode ser interessante para algumas tasks; Não sei o como vai ser o caso do epool, mas o ideal é que mesmo um simples sleep não seja blocante.

// Uma implementação basica com semaphoro parece adequada já que o await meio que bloqueia, mas isso só se tudo fosse thread. No caso das corrotinas acho melhor algo mais no que diz respeito ao lock-free usando apeans operadores atomicos. Ver melhor como está o suporte ao _Atomic co C11
// some ref: https://lumian2015.github.io/lockFreeProgramming/c11-features-in-currency.html // pessoal não curte tanto esse cara mas da para ter uma ideia inicial.
// also: https://github.com/concurrencykit/ck

// Uma coisa que facilitaria decidir o que iria para uma nova thread é a notação explicita na func

fn makeDinner() async<.IO>: Meal {
 //
}

// Muito do que falei aqui é similar a protothread_create_maxpt
// async poderia ter property
// reason .IO / .Parallel / .Heavy / .Soft / .None

// No caso da notação async(.IO) não seria necessario chamar async(reason: .IO) muito menos no caso das cpus async(.max(8)) ao invés async(cpus: .max(8)) pois é a unica propriedade que aceita essa property. Para o caso das duas propriedades poderia usar: async(.IO | .max(8)) ou async(reason: .IO, cpus: .max(8))
// Esse sistema só funcina se não houve propriedades com o mesmo tipo. Isso é resolvido em compiler time e é expandido nas libs para evitar problemas de compatibilidade entre versões caso uma nova propriedade seja interida. Ao atualizar uma lib ou um objeto pode ser que seja necessario corrigir esse tipo de erro. A IDE pode ajudar no se baseano nos artefatos intermediarios previos.
// Uma forma de evitar isso é usar a notação :: que seleciona uma propriedade para ser a padrão desse tipo de associação. Pode ser usado mesmo sem ter outra do mesmo tipo.

object SomeName {
  Property1:: SomeType
  Property2: SomeType
  Property3:: AnotherType
}

// Para efeitos de serialização pensei em sempre usar a hash, porém essa não é forma mais compacta. já que temos 32 bits por property.

object SomeName{
  Property1:-: SomeType
  Property2: SomeType
  Property3:2: AnotherType
}
// Alternativamente poderia usar um hash de 16 bits ou cortar o xxhash no meio como já citado no site. Ficaria limitado a xxh16 65536 propriedades o que é okay.
// Dessa forma eu poderia enviar apenas hash+valor diretamente. Ainda seria maior que o flatbuffers mas é um start para eu pensar.

// Ver também #pragma pack
// Procurar um modelo onde eu possa calcular do outro lado a ordem. Por exemplo, se algo for inserido entre duas outras propriedades (usando o hash por exemplo como base) 
// Eu pensei em umas coisas de layout, de forma que eu possa por o layout no module

module.sometype.layout
module.sometype.size

// Dessa forma na hora de importar os tipos é facil colocar todas as infos de forma estatica e não precisar compilar. Isso também pode ser usada via IPC. (ITC não deve precisar)
// Tem que ver se é melhor isso ou deixar no c struct default. Posso fazer isso de inicio, analisar se vai dar muito trabalho e se realmente tem vantagens.
// Uma das vantagens que vejo é que é possivel ter melhor compatibilidade entre versões da mesma estrutura/enum.
// Provavelmente os tipos em W vão ser compostos de um enum+struct; Possivelmente tudo da para fazer via struct, mas avaliar se dividir pode ajudar na compatibilidade com libs em C.

// Serviços

// Mesma coisa dos objetos porém com ciclo de vida e capacidade de comunicação [ via threads, tcp, etc]

A ideia aqui é ter um ponto unico para comunicação IPC/ITC, RPC e outras formas.
Usando o proprio serviço como broker-like moleculer. Seja possivelmente facilmente chamar objetos remotos, transferir dados para handlers, etc.
// A ideia é substituir os channels para o caso de ITC para o caso do async para algo via serviço também.

// object NameOfService: Service // Tipo especial de define as propriedades padrões.
service NameOfService {
  name: idLikeName // default
  context: Network | Process | Thread // default
  protocol: Http | TCP | WS | SharedMemory | Custom // default
  onStart(){ // default
    //
  }
  
  anyFunction(anyArg){

  }

  handler(genericProtocolArg){ // default
    genericProtocolArg.type
    genericProtocolArg.data
    
    return data
  }
  async call(.methodToCall(args)) // RPC/ITC
  send(genericSendArg)
  send(.context(ctx)) // util em ITC
  async receive(.context(ctx)) // util em ITC

  onFinish(){ // default
    //
  }
}

// No caso do async ele vai chamar um chamar um receive(.context(ctx)) no caso de multithread que vai aguardar a resposta da outra thread.
// Já a função pode ser chamada via ITC como se fosse um RPC; Analogo ao broker call.

...
module.service // default module service, é criado na primeira vez que é chamado.
let resp = await .service.call(.someFuncByName(args))
// .someFuncByName é o shorthand, mas pode ser feita a chamada completa como module.someFuncByName ou module.someType(ref).someFuncByName; Mesma ideia para o .service;
...

// Controle de Acesso

public
private
object

//

// Memoria:
// O tipo callback, é resposabildiade do target dar free... Não é possivel enviar funções normais como argumento. Para conveter uma para a outra tem que chamar o modificador callback.
// A ideia é que se eu prover a heap na hora de chamar, na hora de retornar eu posso limpar o sem problema, claro que para isso não pode haver side efect.
// Para prevenir isso, duas coisas: o callback tem que ser tipo o react onde você declara a captura que vai ser copiada para essa memoria.
// E quem do outro lado o callback não pode guardar nada para só sem copiar.
// Para guardar um callback tem que ser feito uma especie de await infinito. (libdill tem um exemplo disso)
// Tentar manter cada modulo com seu proprio heap. Entre modulos existe apenas copia, exceto nos exports onde quem importa que fica com a memoria.
// Unica forma de exportar funções é usar a assinatura de callback, via modificador ou como funções anonimas

const callback = ()=>{
  //
} // é uma func ou callback, tanto faz. Se acessar context implicitamente é func.

const callback = (arg)=>{
  |capture1, capture2, etc|
}
//

// package-manager

// Apenas libs são importadas.
// Hash pode ser validado em runtime se assim for definido.
// Qualquer arquivo dentro de um modulo pode indicar uma dependencia

module.someModulename.{
  path: 'somelibpath.so'
  hash: '1A23F51C46B2A34D'
  repo: ['someRepo','optionalMirror']
}
// No inicio acho que apenas HTTP/S pode ser suportado, e posso usar um service para lidar com isso.


// snapshots

snapshot somefunc() {
  ...
  let rep = someFunc() // orignal
  return {...resp, ..otherMaybe}
}

// A ideia aqui é ter um codigo que sempre retorne a mesma coisa para uma mesma entrada que possa ser usada em debug/test, ele seria eliminado em produção.
// Também evita que uma malha de teste separada seja feita e facilita o PGO (otimização) seja feita corretamente no codigo de produção primeiro executando um teste depois uma compilação completa.
// Tenho que pensar melhor como recriar esses snapshopts, até porque em alguns casos não precisa de artefato adicional. Acho que a ideia de salvar em disco um artefato poderia ser apenas quando se chama dinamicamente uma função real externa, mesmo de uma outra lib. Poderia ter na cli uma forma de listar/eliminar. Mas uma marcação na IDE parecida com o line-break do debug seria interessante. Uma parte interessante com integração via git é poder usar o diff para isso.

Uma coisa que eu tava pensando... Ao invés de prometer muito em uma "nova" linguagem, como é o caso do zig e do v, varias coisas que pensei para o W percebi só agora que eles já pensaram, isso inclui a questão de chamar qualquer coisa com async, mas acho que o melhor é pensar em algo mais typescript mesmo, onde temos interoperação 1:1 com o C. O mais importante é a questão da especificação, de como as coisas devem ser para poder interoperar.
Pensei em algo como um c_module onde a interoperação segue as regras do C, com free manual, ponteiros etc. Dessa forma ao invés de criar coisas "unsafe" como nas outras linguagens para poder fazer mais coisas, a pessoa simplesmente usa C nessas horas. Simples assim. E como da para criar modulos em qualquer arquivo, a pessoa só declara um c_module no fim [onde quiser] e cria as funções com o que precisa.

O slogam poderia ser: Um simples e podersoso C; E na descrição dize que é para o C o que o typescript é para o javascript.

Como um arquivo pode vir a ter mais de um modulo é facil fazer um bundle também, e facil criar algo em W para ser usado no ambiente C. Poderiamos gerar um arquivo .h compativel com C11/99 para ser usado. No caso do compilador [ w build / w cc ], poderia ter argumentos pra gerar qual versão de C vamos usar, plataformas etc.

Pensei em algo parecido com o gorelease para o comando w release, é legal usar o git, e as tags pra gerar versão. Integração com o github e bitbucket parece legal tb.

Outra coisa legal é fazer algo parecido com o getbin para o gerenciador de pacotes, facil de usar em outros contextos que não o W e facil de manter. O Deno fez algo parecido, mas ainda achei demasiadamente complexo. Se tiver uma verba legal posso tentar ver se eu chego o hash em um worker também.

Dar uma olhada no Nim (conheci agora, parece legal), e porque não o Crystal....

Com tanta lingageum surgindo melhor reservar o W logo.... wlang.tech e wlang.xyz e wlang.dev e wlang.codes estão ativos!

Ah sim, de modo geral o gcc parece ser o mais rapido! Ver como integrar com ele por padrão.



/// Olhar o celular, e ver o protothread em c++: https://github.com/benhoyt/protothreads-cpp
//  int func () asm ("MYFUNC"); Isso parece legal

// https://lwn.net/Articles/285332/
// https://en.cppreference.com/w/cpp/compiler_support

Apesar de pensar em usar o c++, tudo que quero usar já é em C como o caso do mimaloc. Unico problema até agora é a questão das threads, possivelmente melhor implementar direto.
O chato do c++ é ter que aceitar a struct dele, tudo que havia pensado é mais relacionado a estruturas do C. O que mata é que parece que o C++ está muito mais bem mantido em compiladores multi plataforma.

//

Ainda com a ideia de ter uma coisa mais parecido com dabase, os objetos poderiam ter uma propriedade do tipo ID, onde ID é um enum primario com do tipo .auto, mas pode ser do .pointer, .hash, .custom etc... 

eg. Person.Id

Isso ajuda a questão de manter tudo como enum e fazer holding apenas dessa atributo aí.

//

Para não ter que ter uma estrutura, poderia ter também além do _id implicito um _state que nada mais é a fusão da pripedades que devem ser persistidas como se fosse uma struct.

object A {
  _id: .pointer
  _state: .state(name: string, age:number)
  name: string
  age: number

  mut someFunc(){

  }
}

// A parte ruim é porque parece que cada vez mais é melhor ter uma struct e as associações errem analogas a de um enum, incluindo ponteiro por padrão.
// Ainda assim enum é foda, vou pensar um pouco mais:

let package = Package(
  name: "YourProjectName",
  dependencies: [
    .package(url: "https://github.com/gringoireDM/EnumKit.git", from: "1.1.0")
  ],
  targets: [
    .target(name: "YourProjectName", dependencies: ["EnumKit"])
  ]
)

// import PackageDescription

// let package = Package(
//     name: "DeckOfPlayingCards",
//     products: [
//         .library(name: "DeckOfPlayingCards", targets: ["DeckOfPlayingCards"]),
//     ],
//     dependencies: [
//         .package(url: "https://github.com/apple/example-package-fisheryates.git", from: "2.0.0"),
//         .package(url: "https://github.com/apple/example-package-playingcard.git", from: "3.0.0"),
//     ],
//     targets: [
//         .target(
//             name: "DeckOfPlayingCards",
//             dependencies: ["FisherYates", "PlayingCard"]),
//         .testTarget(
//             name: "DeckOfPlayingCardsTests",
//             dependencies: ["DeckOfPlayingCards"]),
//     ]
// )

// <-- Isso é tudo que eu queria, mas pelo que to lendo aqui parece que isso é feito com classe.... tanto faz o nome! Mas é esse o caminho.
// Check: https://github.com/apple/swift-package-manager/blob/main/Sources/PackageDescription/Target.swift
// Pelo que vi o .target é uma função estatica, como o tipo é Target ele só pode ser desse tipo de função... Clever...

// No caso eu posso fazer cada case do obj/enum ser um tipo de contrutor por padrão.

obj-> propriedades contrutoras + id? + state?
struct-> propriedades contrutoras + id?
enum-> id? + state?

// Vou pensar mais, por baixo dos panos já deu pra ter uma idea como vai ser, mas to pensando no uso. Se eu deixar tudo como obj mas usar como se fosse enum seria otimo.

let person:Person = .name(someName) // pessoa apenas com o nome e todas as outras propriedades são opcionais.
let person:Person = .name(someName, age: 33) // nesse caso é porque age não é opcional.
let person = Person.name(someName, age: 33)

// Isso tem poucas vantagens, apenas especulando. Nesse caso o cara tava pensando na vantagem de ter a referencia direta apenas para o nome sem ter que referenciar o resto da classe.

// Entretando:
object Target {
    /// The different types of a target.
    TargetType {
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
  name: string
  ...
}

let target: Target = .executable(...)

Posso pensar na semantica de enum ser adicionada dessa forma, dai tudo seria "estrutura" porem poderia adotar dizer que seu tipo pode variar dessa forma, dai ele só precisa de prover os contrutores ou assinarmos as variantes nas propriedades p.e:

Target {
 ...
.test let TestRuns: Number // ?
.system {
    let os
    let version
}
..
}

// Uma string por exemplo teria seu StringType apeans com o case string por exemplo.

Esses seriam os NameContructors ou EnumerableContructor //  Já que daria pra listar as possibilidades.

let a:SomeObject = .init(...) // default, pode fazer override sem anotação
let b:SomeObject = .someObject(...)  // default, pode fazer override sem anotação
let c = SomeObject.init(...) // Mesma coisa de chamar direto, poderia gerar warning
let d = SomeObject(...) // O linter poderia sempre sugerir isso, o resto é melhor para o caso passar em funções, array etc.
let e:[SomeObject] = [.init(...), .someObject(...), SomeObject(...)] // As opções ficam melhores no caso de um array.
someFunc(obj: .init(...))
someFunc(obj: .obj(...)) // presumindo que o cara definir o contrutor obj lá...

// Os contrutores adicionais podem ser mais restritos que o padrão, nunca menos. Ele pode deixar algo opcional não opcional mas não o inverso. Se ele adicionar uma nova propriedade ela se torna opcional para as outras variantes por padrão quando usar o tipo Pai, nos outros tipos ela nem aparece.


/*******/

Pensando novamente sobre os tipos numericos. Acho melhor evitar os erros do javascript e tomar cuidado com um tipo number unico. Posso ir para o lado do NSNumber onde é apenas uma classe e internamente é o tipo normal, mas acho que tenho uma ideia melhor.

Após compilar usar sempre os tipos _fast do C (por exemplo int_fast32_t )

E no codigo tentar usar estritamente quantidade de bytes que for decladado quando possivel identificar por exemplo

var value = 2 // tipo int_2
var value = 7 // tipo int_3

Dai tentar sempre fazer as operações migrando para tipo maior por exemplo

var value = a + b // se a for int_32 e b <= int_32 value tem que ser int_64! Não pode existir overflow nunca.

A ideia de trabalhar com os tipos menores é evitar subir muito de tipo na hora de tentar evitar um overflow. No final provavelmente eles vão virar int_32 ou int_64 na maior parte dos targets mas já foi provado que não iria ter overflow.

Dessa forma é interessante por algo como:

var value = someInput() using(max_value:30000) // ajudando o compilador encontrar os bounds.
var value = someInput() using(max_value:30000, boundChecking: true) // ajudando forçando um test em runtime.

declare type myInt = int using(max_value:10000) // will be int16 com bound check, ou int16_safe
declare type myInt = int32 using(boundChecking: true) // int32_safe
declare type age = int using(max_value:1000)
declare type age = i1000 // sabe as above
declare type UserNum = i10B // Inteiro que cabe 10 bilhões

var count:i10M = 42 // Inteiro que cabe 10 milhões, mas na agora está com o valor 42 e ele vai gerar um erro se passar do maximo.

guard someNumber {
  // ouve um overflow em runtime, aqui pode ser tratado.
}

//
var dio = Dio()
    ..options.baseUrl = 'https://google.com'
    ..interceptors.add(LogInterceptor())
    ..httpClientAdapter = Http2Adapter(
      ConnectionManager(
        idleTimeout: 10000,
        // Ignore bad certificate
        onClientCreate: (_, config) => config.onBadCertificate = (_) => true,
      ),
    );
//

...
Graphql + Directives

No caso um sistema semelhante ao graphql-state que ajusta a cache local é interessante. Porem eu recomendaria um sistema baseado no numero de mutaçoes que um dado teve em runtime, por exemplo: obj.name = "x"; o resultado dessa opreração poderia deve ser o incremento de mutações que essa propriedade teve, por exeplo 42342 e se a anterior era 42341 siginifica que o dado agora certamente é o novo dado no proximo request vc pode certamente omitir essa propriedade que está na cache local.
...

W?
GC /+/ ARC /+/ Context (Auto / Manual)

HDL support?

///

A importação pode ser igual ao Deno: Explicita com URL(+ versão), e a qualquer momento é possivel criar diretivas para melhorar essa importação por exemplo:

import math from 'https://libs.w.org/supermath@1.2.3/math.w' (Apenas HTTPS assinado é suportado, seria legal tabém limitar alguns sources apenas ou alguns SSLs apenas.)

se torna:

import math from 'supermath'

** em algum lugar no codigo **

define 'supermath' import 'https://libs.w.org/supermath@1.2.3/math.w'

... Aqui algo legal podemos fazer o define set como o macros, e colocar opcionalmente o escopo que ele vai atuar

define < utf8 > < import | const | let | function | ... > < utf8 >


dessa forma o arquivo mod.w poderia apenas declarar essas coisas se ou alguem poderia criar qualquer aquivo como libs.w e definir isso.

Entretando o mod.w seria especial pois poderia ser o que irá ser considerado default pelo package.w caso esse também seja usado.

Nesse exemplo:

/root/lvl1/file1.w
import math from 'supermath'
...

/root/lvl1/lvl1_imports.w
define 'supermath' import 'https://libs.w.org/supermath@1.2.3/math.w'

/root/lvl2/file2.w
import math from 'supermath'
...

/root/lvl3/file3.w
import math from 'https://libs.w.org/supermath@1.3.3/math.w'


/root/mod.w

define 'supermath' import 'https://libs.w.org/supermath@1.2.4/math.w'

define 'supermath' import 'https://libs.w.org/supermath@1.2.4/math.w' 'hashXYZ'


Cada arquivo que tem um import sem URL resolvida ira usar a referencia mais perto, seja no mesmo arquivo (um define no mesmo file), ou algo no mesmo diretorio, caso existe duas declarações em mesmo nivel será considerado erro de compilação.

Já o mod.w é um bom arquivo para deixar na raiz do projeto (poderia ser qualquer nome) e aplicar tudo para toda a tree filha.

A qualquer momento é possivel também dar mais detalhes dessa versão adicionando "event checks"

import on 'supermath' check .hash "XYZ"

Por padrão durante o import ele vai tentar obter o artefato na URL (Se já estive em meria não obviamente) e chegar a integridade usando a hash, nesse caso basta usar o bash style function pra registrar o evento de check para o import, ou via função normal:

import.on('supermath').check(.hash,"XYZ")

o bash style é uma inferencia lazy para se chamar qualquer função em X

import.use(.a, .so, .code)

se torna:

import use .a .so .code
import use .lib .dyn .code // considerando que também tem esses enuns e .use é um varargs

a ideia do bash style é apenas criar mais uma camada de syntax sugar para qualquer função, nem sempre é possivel converter, já que ele vai tentar aplicar cada coisa na ordem e as vezes não vai saber se algo é chamada de função ou argumento... preciso pensar algo melhor sobre isso mas a ideia é criar melhores arquivos de "mod"

Por padrão importar a static library e olhar a hash
Se não tiver a static library (e tiver hash) tentar baixar a lib dinamica.
Se não tiver hash registrada, baixar o codigo e tentar compilar e comparar com o hash da lib caso venha de outro repositorio (codigo e lib)
Se o codigo e a lib vier da mesma fonte baixar só o código

Algo que tambem por ser feito (para produzir um aquivo package.w por exemplo) é os runners, eles podem ser registrado no handle de evento global "on"

on start "comando em string"

Dai poderiamos ter por completo:

// dev-cli

package
 .name "Nome do Projeto" // usando bash style pra call
 .version 1.2.3
 .license "MIT"

on
 .start "command"
 .deploy "command"


deps.
  "name" "version"
  "another" "version"   // multiline bash style call
  "onemore" "version" "url"
  "again" "version" "url" "hash"



.deps é seria uma função que aceita diveros arrays onde o primeiro argumento é o nome, depois a versão depois a hash, ele internamente chamaria o evento global "on"

on.import('supermath').version("1.2.3").url(...)..hash("XYZ")

Dessa forma em qualquer lugar do codigo poderia se registrar comandos para CLI, resolvers etc.

A ideia é que o compilador auto organize isso tudo antes de gerar o executavel, no caso package, on e deps vão ser removidos do codigo e usados apenas quando acionado pela CLI do W;

w start
w deploy
w install

// Pensei em criar arquivos com proposito proprio, mas achei que todos esses recursos poderiam ser interessante para a linguagem como um todo. Algo como o  comentario // dev-cli poderia tirar essas funções da release e a cli poderia inversamente interpretar só elas; Porem provavelmente o nome package.w poderia fazer a mesma coisa sem o comentario

Mas dessa forma teriamos precedencia para usar o comentario na primeira linha para separar o escopo de alguns arquivos


Outra coisa legal é que dessa forma não precisamos de lock file, poderiamos sim declarar hash em outro aquivo por exemplo:

//mod.w
define "name" hash 'hashXYZ'

como "name" antes foi só definido versão, adicionar a hash opcional agora não gera erro, porem:

define "again" hash 'hashXYZ'

gera um erro pois "again" teve hash definida em package.w, o mesmo para url e outras coisas.

//...

Curioso que na parte de licença poderiamos até tentar fazer um lazy check, tipo não importar codigos de licença XYZ ou mesmo checar conformidade em algum momento.


one more thing:

import 'supermath' from 'https://libs.w.org/supermath@1.2.4/math' // sem o .w ele vai tentar primeiro pegar o .a depois o .dyn depois de fato o codigo .w

poderiamos chegar a presença de um aquivo package.w pra pegar informações de licença, ou padronizar que o package manager tem sempre que publicar o aruivo license na raiz, logo
https://libs.w.org/supermath@1.2.4/license; Dai tem que só explicar o padrão desse aquivo...



// as igual typescript satisfies: `https://devblogs.microsoft.com/typescript/announcing-typescript-4-9-beta/`

// olhar o parser Lemon do SQLITE


///// ------------------- ///// -------------- ///

Computer Unit:
128M Memory (Dynamic)
1vCPU
1KV Storage (Dynamic)

Pode-se:
Compartilhar uma ou mais dessas CUs em uma ou todas as caracteristicas;

Duas CUs usando o mesmo espaço de 128M memoria, com duas vCPUs diferentes; Uma unica CPU e memorias isoladas, storage compartilhado ou não.
Ex:

Mais System: 4GB / 8 Cores-HT (16vCPUS)

Kernel -> Basic HW Abstration -> W Module -> CU Workers ->

CUW[0] => Text-Shell Unit Process (Or a Graphic-Shell Unit) | 128M / 1vCPU
CUW[1] => Periphels Controls Unit Process | 128M / 1vCPU
CUW[2] => App1 (One Full CU) | 128M / 1vCPU + Storage
CUW[3] => App2 (Using 4 CUs) | 128GiB / 4vCPU
CUW[4] => AppGroup1 (4 Apps using 4 Shared CUs) | 1GiB / 1vCPU
CUW[5] => AppGroup2 (8 Apps using 8 Shared CUs) | 1GiB / 8vCPU 
CUW[6] => AppGroup2 (4 Apps using 2 Shared CUs) | 256M / 2vCPU
etc..

A ideia é que esses grupos funcionem como os V8 workers e possam compartilhar a mesma area de memoria (ou não), ter afinidade a mesma vCPU (ou não); Dessa forma não é necessario Threads ou SharedMemory por examplo. Você adiciona paralelismo adicionando mais CUs a sua aplicação e pode ou não compartilhar memoria entre essas CUs;

Cada CU tem um entry point bem definido (se usarmos uma forma de traduzir qualquer coisa pra W semelhante ao WebAssymbly) poderia usar isso como base para fazer toda a distry/SO; Se não teriamos que emular esse compartamento; Isso criaria uma sandbox para todas as aplicações e no momento da instalação poderia facilmente poder gerar um profile para decidir quantos CPUs/Memoria a aplicação pode enxergar (semelhante ao cGroup).

Toda CU é single thread por padrão, e ao adicionar mais de uma CU na mesma aplicação deve-se usar o entry point original para conversar entre essas unidades. Tecnicamente não deve-se diferenciar um dado vindo de outra CU ou de um trigger externo. Cada CU é uma unidade de processamento independente (como um serviço) e compartilha dados através do entry point. Similar ao DO poderá existir diversos entry points para diversos tipos de mensagem que podem "acordar" a CU;

Fetch/Main/Alert/Store/Notify/Cron/etc... Alguns entry points serão padronizados porém pode ser criado quantos for necessario durante o processo de instalação do App;

Ele irá avisar ao systema modulo quais o entry points disponiveis e quais argumentos ele aceita. Similar a uma clib; (provavelmente baseado no mesmo artefato) A ideia é ter um conjunto basico de entry points padronizados para que o sistema possa interagir com esse App mesmo sem saber como ele funciona. O "main" (também herdado do C) provavelmente será o basico necessario por exemplo pra poder funcionar em um Shell; E por aí vai. Um códido para um servidor por exemplo pode ter só um fetch, etc. Melhor pensar nessas interfaces depois, pois depois de criadas dificilmente poderão ser diferentes no futuro;

Uma aplicação que tem 4 CUs por exemplo, poderá usar até 4 cores se uma CU chamar os entry points das outras, analago ao service workers. Por padrão o entrypoint chama uma CU default e esse tem que fazer o dispatch para as outras. Cada CU pode ter um EP independente para ser chamado pelo systema. Para um unico código (uma unica CU) usar mais que 128M de memoria deve-se acionar as outras CUs pedindo o bind da memoria (isso não faz com que posa ser usado a outra CPU), que por padrão desativa os outros recursos (CPU / Storage), se precisar dos outros recursos deve-se pedir o bind dos mesmos.

O Sistema se reserva no direito de usar mais de uma copia do mesmo processo na mesma area de memoria já atribuida anteriormente caso ele acredite que caiba, uma CU que usa 10M por exemplo pode rodar +10x o mesmo processo na mesma maquina sem estourar o limite. Isso pode ser bloqueado durante a instalação também. Se um nó travar, deve-se interropender apenas esse nó e não os outros e liberar a memoria que essa CU criou.

O Bind não quer dizer memoria compartilhada, ela é movimentada de um nó a outro. Dessa forma o outro CU fica sem memoria e impossibilitado de executar o EP;

Bind é como se comunica uma CU com outras e outros recursos externos, incluindo storage externo a CU;

//
 Já nem sei mais em que ponto estou... Mas aqui um comparativo com Rust

 https://gist.github.com/GeorgeLyon/c7b07923f7a800674bc9745ae45ddc7f

//

// String & Interpolation

// Semelhante ao Typescript com ${}

print("Hello, ${name}! Today is \"${date.DayOfWeek}\", it's ${date:HH:mm} now.")
print('Hello, ${name}! Today is "${date.DayOfWeek}", it\'s ${date:HH:mm} now.')
print(`Hello, ${name}! Today is "${date.DayOfWeek}", it's ${date:HH:mm} now.`)

// " " ,' ' e ` ` são equivalentes, unica diferença é qual caracter precisa ser escapado dentro da string (se houver)

// Usando o $ não precisa do $ em cada { }

print($`Hello, {name}! Today is {date.DayOfWeek}, it's {date:HH:mm} now.`)

// Literal

print(@"Hello, ${name}! Today is ${date.DayOfWeek}, it's ${date:HH:mm} now."); //<- Literal, irá imprimir ...{name}...

// O @ desabilita a interpolação, logo só resta o literal. O @ não remove a ncessidade de escapar ", 'ou `  

Pensar se é possivel evitar escapar essas coisas todas e facilitar ainda mais a vida do programador. Afinal:

'Hello, ${name}! Today is "${date.DayOfWeek}", it's ${date:HH:mm} now.' 

// O compilador sabe que aí temos uma inconsistencia, pois logo após a string acabar temos o s... o maior problem é como evitar que seja considerado uma função s concatenando etc... Pensar bem aqui, pois mesmo que de certo pra strings podemos gerar outros bugs.

// E com multilinha
print("Hello, Joe!
Today is friday, it's 12/05/198814h25
now its a good time")

// Toda string pode fazer multilinha (semelhante ao Rust).

print(#"Hello, ${name}!
      Today is ${date.DayOfWeek}
      it's ${date:HH:mm} now.")

// Suporte a multilinha e interpolação, com # o espaço em branco até o alinhamento é com # é ignorado. O target de alinhamento é a posição de # na linha.
// Caso esteja desalinhado, é todo o espaço em branco até o primeiro caracter antes de # que não seja branco ou ilegivel.

// Perceba que nesse caso é necessario usar ${} por causa do #" "

print(#$`Hello, {name}!
      Today is {date.DayOfWeek}
      it's {date:HH:mm} now.`)

// Combinando os #$ temos o posicional e o interpolador pra toda a expressão.


print(@$`Hello, {name}! Today is {date.DayOfWeek}, it's {date:HH:mm} now.`) // O @ desativa o $ então essa função não faz sentido, e gera warning.
print(@#`Hello, ${name}! Today is ${date.DayOfWeek}, it's ${date:HH:mm} now.`) // Já isso é okay, mesmo em uma linha.


Refelir mais aqui... Toda essa parte de string pode complicar bastante o parser e poderá influenciar toda a linguagem.

Considerar: 
- Strings como Graphema Clusters igual Swift... Usar lib do ICU ofical deve ajudar nisso. // https://icu.unicode.org/ *** // Pensando bem podemos usar só o UTF-8 e wide char
- Necessidade de escapar strings, dependendo a regra é de uma expressão só acaba até achar a proxima expressão, se não é considerado a continuidade da anterior.
- Quebras de linha no geral, inicio e fim de funções.

ex:

print(" isso


é 

valido?")

func some_func(
  isso,

  é, valido


)


some_func() some_other() // E isso?

// Se o objetivo sor ser felixivel, vejo pouco motivo pra tratar diferenciadamente funções na mesma linha sem o ; já que linha não é a unidade primaria e não é necessario escapar a linha com \

print("Isso é um print com "" ou outra coisa"some_func())

// Ele acabou a string com coisa" e chamou some_func() ??? Pensar aqui pois é estranho, mas se fizer direito pode ajudar.

print('Thats is "nice" because'"it's can be used to "`"union" all types of string`)


// Para identação temos o # porém vejo com bom olhos o escape com \ também


print("Hello, Joe! \
      Today is friday, it's 12/05/198814h25 \
      now its a good time") // <- Vai considerar o espaço

// Possivelmente isso é desnecessario, já que "linha" não é um terminador real na linguagem como todo, então pra não ter uma regra padrão apenas pra string poderiamos desconsiderar o \ ; Dependendo de como for feito o parser vai ter que entender a sequencia com ou sem esse \
// E se ao invés de Today tivesse some_func(), ele ia considerar a string finalizada?

// Eventualmente:

print("Hello, Joe! "
      "Today is friday, it's 12/05/198814h25 "
      "now its a good time")

print("Hello, Joe! ""Today is friday, it's 12/05/198814h25 ""now its a good time")

// Sequencias de string uma ao lado da outra podem ser implicitamente concatenadas.
// Essa expressão acima gera uma sequencia de linha unica ao invés de multilinha

print($somevar $anothervar) // O que fazer com isso?  Isso poderias ser considerado "Implicit Union" ? Provavelmente não em dados não literais.


Acredito que varios tipos lado / lado devem ser considerados concatenações. Ex:

print( 324 050) //> 324050 as number
print( 324_050) //> no caso de alguns tipos pode-se usar outras coisas para formatar e ignorar assim como o espaço em branco.


Inorar espaços em branco deve ser o padrãod a lingua. Unica exeção é dentro de strings.

Essa parte de união implicita pode ser complexa porque a espectativa de alguns dados pode não ser a mesma, principalmente se usar variaveis, porém acho que o ideal seria deixar isso apenas pra tipos literais que podem ser resolvidos na compilação.

// Voltando rapidinho na parte de memoria... Uma coisa que o ARC do Swift faz ( https://docs.swift.org/swift-book/documentation/the-swift-programming-language/automaticreferencecounting/ )

Eles guardam a referencia da memoria em outra parte, similar ao que o mimalloc faz mas "conta" as referencias nas atribuições pra saber quando chamar o deinit etc.
Isso é interessante, mas custoso já que toda atribuição precisa ser interceptada. Porém é mais seguro que tentar achar onde por o free como no V, o que provavelmente é matematicamente impossivel (não olhei, mas acho que sim se não haveriam feito isso no C antes)

Porém podemos por um contador "externo" similar o que acontece na stack que conta quantas vezes naquela função qualquer referencia é tomada ou eliminada. Não na associação em sí, pode ser algo como parte do deffer da função (até pra lidar corretamente com as excessões); Isso deve eliminar todos os casos que o swift tenta otimizar dentro de uma função pra eliminar contagens irrelevantes, por exemplo:

func some(){
      let someClass = new SomeClass()
      someClass.name = "SomeString"
      someClass.someRef = otherObject
      //stuffs
      someClass = nil
}

Aqui não faz sentido contar ou guardar qualquer referencia relacionada a someClass porque ela vai ser dealocada no final mesmo.
Porém:
func some(){
      let someClass = new SomeClass()
      someClass.name = "SomeString"
      someClass.someRef = otherObject
      someFunc(someClass)
      someClass = nil
}

Nesse caso someFunc precisa retornar também a contagem de someClass... Veja que não estou falando nada da contagem das variaveis de someClass, acho que isso é paralelo e tratado dentro do ciclo de vida do objeto.

Seria um caso de ARC mas externo ao objeto, sem interferir nas associações. Seria em nivel de função algo como:



func some(){
      ... Diversas alocações de memoria
      // corpo da função, o compilador irá ler a função e alocar no inicio da função tudo que certamente irá usar na função. A unica coisa que deixaria de lado é o que é alocação condicional.
      // A verdade é que nesse caso poderia inclusive haver otimização no espaço já que eventualmente poderia criar uma unica grande alocação e usar essa memoria HEAP como se fosse uma stack e dai isso seria bem mais rapido.
      ... Dealocações de tudo que chega aqui com contagem zero. Nesse caso tudo estaticamente determinado durante a compialação. O que falta a ser dealocado é que por exemplo foi atribuido ou usado em outras call dentro do corpo da função e dai irá pertencer ao ciclo de vida desses objetos em particular. Seria uma especia de soft ARC onde tudo é week pointer e uma especie de area separada que marca pontos no codigo onde esses ponteiros certamente devem ser desalocados.
}

Para alocações puramente dinamica, condicionar isso ao ciclo de vida dos objetos que "seguram" essas memorias. Por exemplo arrays, claro, se você derrepente associar um objeto que está em um array a um outro objeto recem criado e limpar o array, tudo daquele array deveria ser desalocado mas não esse objeto. Por isso é importante manter track de quem é o owner corrente de cada ponteiro.

Se tudo for weak pointer não existe problema de referencia ciclica nem nada, porém temos o problema de ponteiros que podem ser desalocados inesperadamente, logo em contextos de captura acho que qualquer variavel não primitiva deve ser opcional e forçar o check. Ainda mais no contexto de async...

Se isso for feito poderia rolar um automatic guard

...
var someVar = "lala"
ex.someVar = "blabla"

someFunc(()=>{
  ...
  if let data = ex?.someVar {
    // someVar is not null
  }
  ..
  print(someVar || "x") // também opcional
})

someFunc(autoguard ()=>{
  ...
  print(ex.someVar.count())
  ..
  print(someVar)
})

// O guard olha tudo que é capturado e confere se ele ainda tem um valor real.
// Algo interessante que a capura aqui é de someVar dentro de ex, não de ex... ou seja um ponteiro direto pra variavel interna.

A runtime poderia ter dois passos o contador de referencia da variavel e o de capturas, pra poder colocar nil nesses valores depois. Na pratica capura é uma variavel implicita no c code com as variaveis caputurdas

__closure_x(capture:[ex.someVar, someVar, ...], otherArgs...){
  ...
  void* data
  if (capture[0] != null && data = capture[0])  {
    // coisas...
  }
  ...
  free(capture)
}

Em algum lugar a variavel capture, que na verdade não seria declarado assim, vai ser possivel zerar a ou mudar o valor desse dado nessa posição.
Pelo visto teremos varios hastables de suporte.

// Life clicle....


Uma coisa que pode acontecer é tratar tudo como no swift e rodar o arquivo de entrypoint como se fosse um script e rodar desde o inicio sem chamar nenhum "main", porém se tiver um export no aquivo principal tratar como funções main, seriam:


export {
  fetch(req: Request, ctx: Context){  // ctx aqui poderia ter env entre outras coisas...
    ...
  }, // precisa de , aqui?
  main(args){

  }
  cli(stdin){
    // por linha?
  }
  mouse(event){

  }
  keyboard(event){

  }
  hid(event){

  }
  // etc?
}

O interessante aqui que com isso também definimos no contexto de lib o que é ou o que não é visivel pela lib. Poderia rolar algum tipo de call implicito para coisas publicas e compiladas no modo lib mas no geral:

export {
  fetch(req: Request, ctx: Context){  // ctx aqui poderia ter env entre outras coisas...
    ...
  }, // precisa de , aqui?
  anyFuncion(args){

  }
  anyCall(){

  }
  // etc?
}

Ou seja, esses seriam entry points que a parte "C" que irá chamar o codigo "W" irá processar e chamar, no caso de libs seria o que é exportado e disponivel para call em uma clib. Com isso temos uma interface estavel e facil de usar com outras bibliotecas.

Veja que no caso do fetch poderia rolar algum tipo de integração com alguma stdlib de network. No caso, se for uma std lib poderia tranquilamente ter entry points, no caso cli/mouse/keryboard/hdi etc...

//

entry {
  ...
}

//

Um efeito legal disso é que tudo que for programado em W na verdade pode ser "sempre" uma clib, e a runtime ser apenas um entrypoint em C qualquer que trata o que foi definido na exportação (main, cli etc) e importa essa lib estativamente. Se essa runtime tiver uma assinatura estavel todo executavel "W" da pra ser usado como lib, só importar e dar um jump pra lib estatica.

Não tenho certeza, mas isso também poderia ser usado como uma forma de segurança, ou eventualmente até DRM já que poderiamos deixar esse executavel todo como opcional e dai ter um "conjunto" de executaveis que entre outras coisas chegam a assinatura dessa lib default. 

//

Já me perdi um pouco, tem tempo que não vejo isso, mas lembrei hoje umas coisas que tenho que conferir se já expliquei.

Parte é para o WC mas vale aqui também: 
- Alocação do retorno é resposablidade do caller -> Na função vai ter o metadata relativo a quanto o max/min para alocação e tudo é implicitatemente um callback, dessa forma no WC funções podem ser void e com isso também consigo calcular o quanto de memoria o modulo vai gastar, fica mais facil também fazer tudo stackfull se for necessario.
- Variaveis são ou de alguma estrutura de dados [ class, strucs, enum,...], do modulo [ modulo.etc ], ou global.
- Todas funções com side efect devem ser marcadas e nos metadados da função deve poder saber automaticamente quais o efeitos: Exemplo -> xx.turnOn() -> No autocomplete ou no compiler inspect deve dizer que essa função muta variaveis class.X, modulo.Y, etc... ( usar mut na notação da função pode ajudar)
- Eu deveria focar no botstrap o mais rapido, pode ser até em typescrypt. Não vale a pena ficar ajustando codigo em C ou Zig depois. Um bostrap rapido, compatibilizar a linguagem com o TCC / Clang; E continuar com a ideia das dependencias e intermediarios serem biliotecas estaticas com simblos de debug; Vou precisar do package maneger, mas assim como no caso do Zig não da pra lançar mais linguagem sem o bundle todo.
- mimalloc é legal, mas poderia começar no basico mesmo, usando alocação dinamic no global, modulo etc... Stackfull é complexo pq só no windows é facil deixar fixo no binario para dados dinamicos acima de 10mb, mas não custa nada avaliar isso, tentar chegar a pelo menos um numero "minimo" de stack, e deixar configurado no bilder pra só usar heap quando passar desse valor. Por isso calcular o lance dos modulos é importante.


class example {
   let p1: int;
   let p2: String; // mesmo quero String<auto>
   let p3: String<321>
   let p4: String<dynamic> // vai ter algum padrãi aí...
   let p5: String<dynamic, min:123, max:4321, expected: 321> // ou avg melhor pensar
   let p6: String<auto> // ele vai tentar decidir em compilação, com ou sem auxilio dos testes, default dynamic
   let p7: Blob[]<dynamic, min:123, max:MAX_MEM, expected: 321> // .MAX_MEM? NO_LIMIT? just max:.NO ?? Não acho que por padrão seja sem limite, um limite sempre tem que existir.



   fn someFunc(){

    let a: String = fetch() // default to String<expected>
    let b: String<expected> = fetch() // aloca string com expected value se não dynamic iniciando em min até max

    let c: String<min> = fetch() // não vai estourar se passar, porém vai começar com o minimo até max

    let c: String<max> = fetch() // vai estourar se passar

    let d: String<auto> = fetch() // auto é interessante que sem falar nada (ou se colocar auto) ele vai tentar alocar o maximo (max) que der se ele não for capaz de provar um valor menor

   }

    mut someOtherFunc(){

    let b: String<expected> = fetch() // aloca string com expected value se não dynamic iniciando em min até max

    this.p4 = b + "blablabla" // Side efect, a função agora não é mais pura, precisa de mut

   }
} // Legal que da pra usar alguns tipos semelhantes aos dos DBs


module { // class, struct, global etc... sempre usando o valor do mais especifico, por padrão é a config do proprio modulo.
  #dynamic maxsize=1G
  #auto is static up to 100M
  #String dynamic maxsize=50M
} // pode ser em uma config especifica em cada tipo de objeto:

module ZZZ { // config do submodulo ZZZ importado.
  ...
  #config.dynamic.maxSize=1G
}
module YYY {
  ...
  #config{ // não sei ainda se vou por em varios niveis.
    dynamic.maxSize=1G // deveria ser #dynamic ??
    string.static.maxSize=100M
    fetch.timeOut=100s // why not?
 }
} // A ideia é ter um objeto especial de config para ajudar o compilador nessa hora, na pratica isso vai ser removido e substituido pelos metadados resultantes em compilação.

Eventualmente o # pode ser um shorthand para metodos "internos"... E no caso do #{} cada linha é uma propriedade/setter mas nunca um escopo aberto de função. Evita ter que fazer um json ou outra estrutura para isso. Eventualmente usar a mesma coisa que seria usado no "package.json"... (TOML ?) o simblo em sí agora é irrelevante, depois penso nisso, já que # é comentario no toml, evetualmente seria o desejado não permitir o # como comentario no W/WC apenas o //

Se eu tiver que fazer o parser do toml posso usar varias das regras dele para a linguagem, incluindo a strings.

//

someClass {

  fn func1()
  {
    ...
  }
  fn<C> func2(){
    // Codigo em C
  }
  fn<JS> func3(){
    // Codigo em JS (quickJS)
  }
  fn<TS> func3(){
    // Codigo em JS (esbuild + quickJS)
  }
  fn<Rust> func3(){
    // Codigo em Rust
  }
  fn<WC> func3(){
    // ...
  }
  fn<Zig> func3(){
    // ...
  }
  // Algumas linguagens que podem ser facilmente geradas como bibliotecas C.
  fn<C::1> func2(){
    // Codigo em C no namespace C1 (default é zero e não por nada ou em branco)
    // Codigos no mesmo namespace compartilham escopo do namespace
    // O corpo da função é como se fosse um aquivo/modulo. Qualquer import/include para esse escopo deve ser feito separado. -> Veja abaixo
    // O escopo aqui é o mesmo de uma função na linguagem escolhida
  }
  #C::1 {
    #include <stdio.h>
    //... Qualquer definição, até mesmo outras funções, é como um aquivo C normal, só não tem retorno ou entry point, será como uma lib.
  } // Declarado em qualquer lugar do escopo, só pode haver 1 (1 por class, modulo, global etc, usar modificador de escopo antes: module #C{} p.e)
  
}

Os códigos em outras linguagem irão receber dentro do seu escopo os argumentos já converitdos para sua linguagem e vão retornar normalmente usando sua linguagem. Como tudo em W, cabe ao caller converter. Dessa forma uma função em uma linguagem pode chamar funções na propria linguagem sem overhead. Isso complica um pouco chamar uma terceira linguagem, mas isso seria complicado de qualquer forma. Eventualmente na std library pode ter os utilitarios pra sair de qualquer linguagem para qualquer outra usando W como ponte.


{
  ....

  let someVar = someFuncThatIsC()
  let someOtherVar = someFuncThatIsJS()
  // Qual o tipo de dados é someVar e SomeOtherVar???

  fn<C> someFuncThatIsC(){
    ...
    return X // X must be converted to "W" if called from W.
  }
  // Cada linguagem que for adicionada deve vir com a table a de tipos que pode ser convertido. Alguns casos só será possivel via copia, no caso do C referencia até funciona.
}


Resumindo até aqui:

Botstrap pode ser em qualquer linguagem mesmo zig, gerar WC como backend e WC para binario depois.
Modulos posuem espectativa de consumo de memoria, o gerenciamento é automatico pela convenção de chamada.
As dependencias são resolvidas não com cogido, mas sempre com bibliotecas.
Usar hash / git info para saber se a biblioteca é a original, porém criar um metodo para compilar, quando houver, instruções de compilação.

// github.com/jart/cosmopolitan

Isso pode resolver o problema da LibC e eventualmente do cros compile, se bem que acho dificil já que quero compatiblizar com outras libs... vamos ver...

// E agora parece que a casa branda também recomenda o uso de linguagens memory safe: https://www.whitehouse.gov/oncd/briefing-room/2024/02/26/press-release-technical-report/
// Já era, ou eu faço o W logo como C evoluido como memory safe ou todos vão pro Rust perder tempo de compialação!

let a1: String<dynamic, min:123, max:4321, expected: 321> // ou avg melhor pensar
let b2: String<dynamic, maxLength:22> // Uma forma indireta de dizer o maximo de memoria. Muito util para memory safe.
let c3: Int<maxSize: 20000> // Não deve afetar memoria diretamente na maior parte dos casos, porém eventualmente da pra rolar uma alocação de tipo menor. como i16;

let d4: String<maxLength: 12; mask:CPF, inputType:Number>  = try 12332112332 // Aqui é uma string que sempre faz a mascara para CPF, logo internamente ela só tem os 11 numeros, como o tipo de saida é sempre string, ela é do tipo string, mas ela aceita numeros de entrada. A presença da mascara torna torna necessario o try
// inputType só faz sentido quando se usa mask, filter, e outros tipos de conversores.

let d5: String[]<filter:isOdd> // filtro padrão para a inserção de elemento ou mesmo atribuição completa da variavel. Filtros assim como mascara podem dar throw

typeDef CPF = String<maxLength: 12; mask:CPF, inputType:Number>;

let e6:CPF = ... // auto explicativo.

Pelo visto essa linguagem vai ter grande foco em string

//
Verificar melhor no caso do target ser algum tipo de acelerador AI a questão dos tipos, porém no geral usar sempre:

int_8_fast_t => i8
int_16_fast_t => i8
int_32_fast_t => i32 -> Default para Integer
int_64_fast_t => i64
.. etc..

Usar todos os tipos fast do C como padrão. Não vejo motivo para ser diferente, na maior parte do caso quando o cara quer usar um tipo menor que a palavra só atrapalha
A linguagem vai economizar memoria de outras formas, não precisa ser nisso. Porém com os marcadores de tamanho nas variaveis os tipos exatos podem ser evocados:

let x = Integer<bitSize: 16> // int_16_t 

// Não sei se faz sentido usar os tipos int_leastN_t etc... Poderia economizar memoria, mas prefiro sempre o tipo rapido nesse caso.

Outra coisa ao invés de usar o ICU por padrão poderia usar o utf8proc como padrão. De qualquer forma melhor testar antes pq parece que melhoram bastante o icu4c com funções dedicadas para utf8; // Ou usar o u8 type e o w_char *, e similares.

Fazer uns bencharmks por padrão estilo moleculer..

//
Já que quem chama a função é responsavel por alocar memoria, se usarmos um HEAP para cada ( mimalloc ) modulo (sendo esse semrep singleton) fica facil desalocar todo o modulo de uma vez na ultima referencia.
Para contar essa referencia vou usar ARC? Ou algum tipo de ARC em tempo de compilação (CARC ?)... Pensarei...


module a.w
...
print("Hello A")
etc...
fn someCode(){
  // essa função pertence ao modulo e é privada se não for exportado
}

export {
  SomeClass
  someCode,
} // Qualquer export com uma função chamada entry(args) pode ser utilizada como entry point da aplicação.
...


module b.w
...

import { someCode } from "a"
// coisas

let a = someCode() // var A usado o HEAP(B), já qualquer coisa em someCode usa o HEAP(A)

// someCode é expandido assim em WC
// someCode(&a, ...)

let count = 0;

mut thing(let b: SomeClass){
    count = b.count
    return `Count is ${count}`
}

// isso vira em C:
// void thing(*_return: char* ,*b: SomeClass, *count: int) {
//  count = b->count
//  asprintf(&_return, `Count is %d`, *count)
//  return _return
//}
export {
  other,
  thing,
} // Qualquer export com uma função chamada entry(args) pode ser utilizada como entry point da aplicação.
...

import { thing } from "b" // implicit import the module as "b", aka: moduleName. Colision append _N
let c = thing(x)
// void c* = mi_malloc(b.thing.returnSize)
// thing(c,&x,&b.count) // Ele que manda o b count? Se eu fizer como função de função não precisa, da pra encapsular no modulo, mas por performance seria legal ter isso em algum metadado e ele saber que tem que expandir assim... Seria sempre em tempo de compilação, logo não é o fim do mundo, posso colocar isso na exportação pra avisar pro compilador qual é a assinatura do call. No caso de libs teria que ser pela exportação mesmo, porém eu tenho que conseguir uma forma de ler isso em tempo de compilação se não fica pessimo. Com ponteiro de ponteiro não precisa, mas acho que fica lento...

QUANDO TIVER ASYNC usar uma THREAD para chamar fora do modulo. Mas dentro do modulo masta usar as corrotinas:

async coisaDeOutroModulo() // Paralelo via thread. mi_
async coisaDoMeuModulo() // Concorrente via corrotina.


//

...
void greet(void (*fptr)(const char*)) {
    fptr("World");
}
void sayHello(const char* name) {
    printf("Hello, %s!\n", name);
}
int main() {
    void (*helloPtr)(const char*) = sayHello;
    greet(helloPtr);
    return 0;
}
...

...
int count
inline void thing(void *_return, SomeStructClass x*,int* count*) {
 count = b->count
 asprintf(&_return, `Count is %d`, *count)
}
static void callThing(void *_return, SomeStructClass x*) {
    thing(void *_return, SomeStructClass x*, &count)
}
// ainda pseldo codigo.
// no caso do count nem vale a pena passar por ponteiro, mas como nota poderia ser algo mais complexo, como o caso do side efect de uma variavel de instancia
// no caso dos modulos, se o side efect é sempre nele, basta referenciar direto pelo arquivo.
// mas se for o side efect em outro modulo esse callThing poderia passar certinho o ponteiro pra frente.
// Dai daria pra computar certinho os side efects ai, daria pra retornar em callThing ou poderia só produzir uma variavel const qualquer com esses dados
// 
// static const char*[] callThing_SF = {"module.count", "maybeOther"}
export {
    callThing // as thing
}

hide_export {
    callThing_SF // se ficar encapsulado isso é apenas informativo. Mas eventualmente eu posso usar isso pra gerar codigo intermediario.
}

expands:

extern void module_callThing = callThing // a outra é estatica mas essa não, essa da pra chamar de outro arquivo. // funciona assim: https://stackoverflow.com/questions/8023213/can-a-static-function-be-called-through-a-function-pointer-in-c

no caso module_callThing pode ser module_name_callThing onde "name" eu processo do nome do aquivo na hora que saio de W para WC, ou de WC para C....

...

// https://stackoverflow.com/questions/15125059/how-do-you-prevent-names-from-colliding-in-c // Basicamente a mesma ideia que eu tive de tornar o modulo uma struct para evitar colisão, dai module_name_callThing nem precisa ser assim, eu só preciso deixar essa struct visivel e o resto static

Dessa forma o tal init para inicializar o modulo (para obter a struct) pode ter uns checks para certificar que só roda uma vez (problema disso que tenho que usar semaforos pra evitar problema com thread), rodar o codigo inicial do aquivo...
Ainda não está claro como vai ser o unload do modulo. Assim como o caso do destructor das classes, já que essas também poderiam ser implicitamente modulos. Porém acho melhor ela ficar com o HEAP do modulo real e não criar um HEAP para cada objeto ou classe, o que parece excesso e provavelmente lento.

De modo geral, importante salientar que só precisa de mut nas funções que foram exportadas, ou no caso das classes nos metodos publicos. Todo resto pode ter side effect a vontade. Ainda não sei se propriedades como static dentro da função ou se deixo fora no modulo mesmo.


//

import a from "a" // vai importar o aquivo virtual "a.c" e o "a.h" no escopo do modulo. // Os tipos vão ser lidos do .h
import type a from "a" // apenas o a.h no escopo do modulo.

import type { someClass } from "a" // vai continuar a importar o a.h, de inicio deixa o compilador final otimizar eliminar o dead code mesmo.

a.name
a.size
a...

release("a") // unload("a") // avisa que não vai mais usar aquele submodulo; Reduz o RF count e potencialmente libera toda a HEAP da lib;
release(a) // Caso tenha feito a importação completa do modulo.

fn some(){
const a = import("a") // function/object scope ; Como é "C" por baixo acho que o correto aqui é isso ser sync default;
const { someProp } = import("a")

// release("a") não precisa ser usado, quando sair do escopo ele vai chamar o release do modulo reduzindo o refcount.
}

Eventualmente a pessoa não vai querer liberar toda a memoria do modulo "a" quando a função sair do escopo, então ou a pessoa promove o import para o modulo ou usa o modificador `static` para promover o import para o escopo do modulo porém manter a visibilidade no escopo da função

fn some(){
static a = import("a") // static presume const (eu acho...)
static { someProp } = import("b")

// ou seria melhor promover usando "module"

module const a = import("a") // ??

}

Ou eu uso os metodos de deferred free do mimalloc por função (forçando o free completo na saida do escopo da função mas permitindo por exemplo que um loop crie e mate muita memoria), ou eu deixo deferred free por modulo, permitindo que alguma memoria viva após o função ser chamada... No segundo caso pode até ser mais performatico, mas tenho que ver se da pra otimizar para relaoc; Por padrão o mi_realloc chama mi_malloc se o p é null. Claro, mi_zalloc parece ser um bom default, junto de mi_rezalloc;

// comptime
...

var someValue = #func() // #func vai ser chamado em tempo de compilação e o resultado vai ficar em someValue

#func("static string") // valid
#func(userVariable) // invalid
#func(constOrOtherStaticVariable) // valid
//

Comptime "#" executa qualquer função cujo os parametros são estaticos e o resultado precisa ser printavel em UTF-8 compativel com o source code. O compilador vai usar a saida do modulo como entry point e executar a função a partir de lá. A função comptime se não tiver exportada no modulo irá temporariamente ser exportada. Por exemplo:

... module.a

fn someFuncInSomeClass(...args..) {
  ...
  var someValue = #anyOtherFunc(staticArgs); // Resultado será substituido aqui como se fosse um macro #define
  ...
}

...

Compilador vai temporariamente exportar e chamar anyOtherFunc em um entry point temporario com os exatos parametros estaticos passados. Caso uma dessas statics Args seja fruto de outro comptime, esse será executado primeiro. No fim esse processo irá exporta a representação em UTF-8 da resultado a ser substituido no arquivo fonte.

A execução vai considerar que a função está sendo executada no entry point, entretando não há restrição nos side efects dessa função por exemplo:


fn listFlags(...args..) {
  ...
  String[] flagsPath = #fetchAndStoreFlags("Americas","Europe"); // fetchAndStoreFlags vai baixar os arquivos das flags e guardar em um local no disco relativo ao modulo.
  // flagsPath é a lista paths relativo ao modulo que será usada pelo executavel final.
  // Semelhante a static sites. 
  ...
  var commit = #getCommitHash(.CURRENT) // .CURRENT é um ENUM estatico, logo funciona 
}

Para facilitar o track e import das constantes, listFlags deveria ser reescrito como uma função sem chamadas de paramentros. É um pouco complicado, mas poderia começar simplesmente olhando o que a função #comptime está referenciando dentro da propria função, isso até deixar apenas o que é necessario para executar. Ainda me parece chato sem usar um AST ... Por questão de simplicidade poderia forçar a função # ser um earlyreturn com o dados necessarios já formatado. // Isso tudo vai dar um bom trabalho, mas é interessante começar por isso pois é parcialmente um trabalho do que já vou precisar para o resto da linguagem.

comptime blocs e code elision via args também seria uma boa... tenho que pensar mais.

// ERROR CODES

Usar base do HTML

Result.OK // 200

// TEST

file.w
file.test.w

Aquivos .test. são executados como libs importadas do artefato de debug. É um import dinamico e opcional ao modo debug e importado direto do Entry Point, ele sobe opcionalmente um servidor e executa o teste sobre o artefato compilado direto pelas libs como se fosse o entry point. O rest runner irá ter a mesma visibilidade do arquivo e irá criar os exports necessarios quando compilado para target de test-debug.

Esse servidor existe de forma que a pessoa possa ficar criando casos de testes só com os cabeçalhos sem conhecer o corpo da função. Otimo para separação de responsabilidade em times, mas também porque muitas vezes para o teste ser completo é necessario que a aplicação tenha um grande ciclo execução e acesse muitos recursos.

Os arquivos .test poderão checar, por exemplo a versão da applicação que estão testasdo e executar os testes de acordo. Muito util para testes de regração.

Um aquivo de teste pode ter diversos modulos para cada versão ou mesmo usar isso no proprio nome: file.test.v2.0.1.w
Não existe real correspondencia entre o nome do arquivo e qual modulo ele acessa, é apenas organizacional. É necessario importar os arquivos originais. Quando compilado ele vai precisar apenas das libs, no caso do servidor remoto, ele irá usar como libs remotas implicitamente. Basta passar o endereço, p.e --remote 10.0.1.1

Arquivos .test. não geram side efect na instancia em execução.

// WRPC

Fazer algo estilo TRPC para inter processo, util para testes e debug remoto. Porém para execução remota também é util, o modo remoto pode ser implicito nos imports da libs se ativado o recurso na compilação, o entry pode indicar qual servidor arquele servidor irá executar as libs indicadas:

buildArgs: RemoteModules = ["Address:ModuleName","Alias:ModuleName",...]; Qualquer execução implicita é insegura, porém é util para debug remoto e teste remoto. Ele deve gerar warning e só permitir em DebugMode, para release a notação remota deve fazer parte do código. Claro que aqui poderá ser feito o override.

import { thing } from "Address:ModuleName" // Como não tem https ou outro protocolo no import só o endereço, irá considerar WRPC na porta padrão compilada.


// Remote debug e .debug files

file.debug.w

Mesmas capacidades do aquivo .test. Mas é permitido side efects. Muito util para se fazer testes ou apresentações com perfis completos de execução. Porém mais util em uma dev branch para debugar código em execução por multiplas pessoas sem recompilação do artefato principal. O running server irá comunicar as incosistencias entre os conectados. Não é possivel alterar o código em execução, mas será possivel fazer Swizzling como em Objective-C

// InPlace Test e Debug


//@(2,1) == 3
fn someFunc(Int a, Int b){
 return a + b
}

@() espande para #someFunc(2,1) == 3 ou assert(#someFunc(2,1),3)

fn someFunc(Int a, Int b){
 @(2,1) == 3 // novamente vira o comptime, porém agora sem o comentario irá fazer parte do código debug, assim como um assert explicito. Em release é executado, mas removido do artefato. Também pode ser chamado fora, o @test sempre vale para a função corrente ou se logo antes de uma declaração para a função declarada. Util para chegar protocolos.
 return a + b
}

// Some Docs about that func
// @a: Int, bla bla bla
// @b: Int, bla bla bla
// @@: Int, sum of bla bla bla
// @(2,2) == 4
fn someFunc(Int a, Int b){
 return a + b
}

// Some Docs about that func
// @a: String, bla bla bla
// @@: SomeComplexObject, bla bla bla
// @a = "Very Complex sample"
// @@ = SerializedForm from SomeComplexObject
// @(a) == @return
fn someFunc(string a){
 // very complex parser
 return new SomeComplexObject()
}

@ e qualquer token irá referenciar os argumentos. Caso exista um argumento chamado "return" poderá ser usado @result, caso também tenha conflito, usar um segundo @: @@return ou @@result; Ou apenas @@

De modo geral deixar reservado tudo do JSDocs: @param, @memberOf, @returns etc... @ e @@ apenas versões menores. Não me importo muito com a sintaxe disso

Não será possivel criar funções com argumentos iniciados por @. Caso seja necessario, usar alias. Essa forma o @ se torna apenas um decorador de chamada mas não o nome real do argumento


fn someFunc(string @a a){ // @a é alias de a, ambos podem ser usados, mas no caso dos testes não irá chamar @@a e sim @a ou outro alias atribuido
 // very complex parser
 return new SomeComplexObject() // @@ sempre se refere a resultado final
}


@debug("variavel que na verdade vai mudar em debug a primeira vez que rodar esse aquivo") // Só em debug
mut someFunc(string a){
 @debug("posso me chamar também sempre que executa a função em debug")
 @debug someOtherFunc() // apenas em deub essa outra função.
 #debug ("a") == Result.OK // Seria os códigos de erro propagados automagicamente junto com todos os retornos em debug ? 
 return new SomeComplexObject() // @@ sempre se refere a resultado final
}

DEBUG: Todos os retornos são implicitamente o ENUM Result<T>; O código é ajustado nas chamadas para sempre acessar o valor correto sempre. Em release é necessario reclarar a tupla ou o Result type.

No caso do servidor e do test runner eles devem acessar o result type como padrão:

....
file.test.w / file.debug.w

var a = someFunc()
if (a == Result.OK){
  a.value == "blabla"
}

guard let b = someFunc, where b == Result.err(504){
  //....
  // fora do guard b é Result<T>.value
}

if let c = someFunc() {
  //.... c is value T
  // if let só entra se result for okay, logo b sempre já vai ser value, para ver o código tem que usar o where 
}


...


SELECT and SQL sugar

Assim como where é muito util em if-let e em guard-let, outros primitivos de SQL como select são bacanas.

Basicamente trata qualquer objeto na linguagem como uma tabela e o select apenas escolhe as propriedades do mesmo.


let { age, prop2, prop3} = someObjct // select prop1, prop2, prop3 from someObject

Dai para um select mais avançado precisa do where aí, logo:

let { age, prop2, prop3} = someObjct where age == 10 // let { age, prop2, prop3} = someObjct if age == 10

Se age não for 10; todas as variaveis vão ser do tipo opcional.null (ou optional.undefined tenho que pensar) e devem ser tratados no codigo de arcodo.

Da pra fazer isso no código sem SQL, mas trazer a sintaxe do SQL em alguns casos é mais objetivo que:

let age, prop2, prop3 = Optional.null
if (someObjct.age == 10){
  age = someObjct.age
  prop2 = someObjct.prop2
  prop3 = someObjct.prop3
  // { age, prop2, prop3 } = someObjct // me parece um sintaxe sugar bem legal.
}

No caso o select também poderia ser usado no lugar do "let", isso é: select { age, prop2, prop3 } = someObjct where age == 10
Mas pode ser um coisa a ser ativado via flags de compialação, isso incluio "from" no lugar do "=", pois é apenas uma sintaxe adicional.
Já o where poderia ser sempre ativo.

Adicionar suporte a "join", inner select etc. Tudo isso é facilmente um sugar para algo na propria linguagem.

//

Na parte dos metadados, a parte de alocação e desalocação poderiam ser apenas macros, mas provavemente vai funcionar bem com funções inline também. com inline da para forçar uma call convention com bibliotecas em C/C++; Via macro fica só entre W, mas possivelmente inline vai funcionar okay.


//

Reference Cicles:

class Node  { // Ou seja object Node ?? Não sei se deixo tudo apenas object ...
    let prev: Node
    let next: Node

    Node(Node? prev = null) {
        this.prev = prev;    
        if (prev != null) {
            prev.next = this;
        }
    }
}

fn entry() {
    var n1 = Node();  
    var n2 = Node(n1);
    ...
    n1 = null // free(n1)
    n2 = null // free(n2)
    ...

    // is n1 and n2 alive?
}   

Acredito que aqui seria como se tivessemos tratando toda variavel passada para uma função, mesmo um contrutor como "weak". Já que assim como no C se n1 virar null, a classe n2 ainda vai apontar para um endereço invalido. O que é inseguro. Nesse caso eu teria que assim como em Swift (e em Vala?) tornar a variavel prev `null` também.

Dai não seria extamente um ref count, mas um assing track... Porém "se", o atribuir a null na verdade mudar o conteudo para algo como Optional.null, logo eu poderia considerar que eu trabalho internamente sempre com ponteiro de ponteiro, não preciso manter um assing track.

Nesse caso só posso guardar referencias como optional:


class Node  {
    let prev: Node? // ref Node?
    let next: Node?

    Node(ref Node? prev = null) { // Pensei obrigar por ref para permitir guardar referencia. 
        this.prev = prev;    // sem ref isso não funcionaria...
        if (prev != null) {
            prev.next = this;
        }
    }
}

fn entry() {
    var n1 = Node();  
    var n2 = Node(n1);
    ...
    n1 = null // free(n1.value); n1 = Optional.null
    n2 = null // free(n2.value); n2 = Optional.null
    ...

}   

Eventualmente qualquer coisa que eu lide com ponteiro eu implicitamente consiro que é Optional... O que mata um pouco da ideia, mas posso deixar isso exclusivo para esses casos onde você guarda algo que foi recebido via ref... Sem receber via ref eu posso usar o ponteiro mas não guardar, e se usar struct pode vir a ser copia pois é value type.

Internamente eu poderia lider com qualquer opcional de classe como um ponteiro de ponteiro, apenas ponteiro quando não for optional, isso é, que o ownership está dentro do escopo da classe, struct, module, etc. E no caso de struct posso até também lidar como ponteiro, mas posso ter um metodo copy(), só tenho que ver qual é mais vantagem.
Porém como em C copiar estruturas pode ser algo apenas feito via `=` o que gera uma shallow copy bacana. Posso deixar o metodo copy() para virar um deepCopy();
Não sei se tem vantagem de usar o memcopy no caso de structs....

// Sobre comparar structs: https://stackoverflow.com/questions/141720/how-do-you-compare-structs-for-equality-in-c

Provavalmente em toda declaração de struct eu tenho que fazer automaticamente um comparador padrão para essa struct. Que pode vir a estar assesivel via struct.compare(), dessa forma um == seria apenas um sugar. Ou nem permitira.

// WEAK vs UNOWNED vs ref/transfer

Pense em um caso de buffers... Se eu to guardando buffers dentro do meu modulo até que alguem chame uma função para obter esses buffers. Nesse caso não vale a pena eu usar o esquema padrão que falei, onde quem chama passa o ponteiro alocado, pq eu já tive que alocar e não quero mais essa memoria. Dai pensei em marcar essas variaveis que eventualmente vão ser transfereridas com alguma notação similar ao unowned


class Buffer {
  let unowned data: blob[]

  fn getData(){
    return transfer(data) // data é vai ser transferida para o caller, ao invés do caso implicito onde ele me passa um ponteiro e eu copio o dado pra lá. O transfer vai trocar o ownership com o ponteiro passado, e vai liberar a memoria de blob. Nesse caso eu poderia manter o mesmo padrão de chamada, onde ele aloca um novo "data" e eu apenas troco os ponteiros com ele, eu fico com a memoria dele e ele com a minha.
  }
}

// alternativa
class Buffer {
  let data: unowned blob[] // as vezes podemos por a regra que todo unowned só pode ser lido fora do modulo via exchange

  fn getData(){  // aqui tem o lande ce implicitamente ele estar chamando com a memoria de retorno....
    return exchange(data) // nome melhor, mas seria uma função global ou... algo inplicito a variavel: blog.exchange() ... acho que como função funciona melhor.
  }
}

No caso do transfer/exchange provavelmente poderia ser usado para qualquer variavel, mesmo sem o unowned. Basicamente é a unica mandeira de ler algo dentro de um modulo sem usar as chamadas padrão onde você manda a memorai de retorno durante a call.
Marcar como unowned seria apenas para deixar claro isso. O problema é que se a memoria é unowned, faz sentido o modulo alocar memoria para ela? Entendo que quando acontece sempre a troca faz sentido essa marcação, mas se é um buffer que eu "crio" provavelmente eu teria que deixar ela normal. Com unowned pode acontecer que eu recebo esse buffer de outro lugar e vou guardando aí, nesse caso a alocação faz sentido. Eu poderia forçar sempre quem declarou fazer o malloc da estrutura basica para depois fazer exchange. Nesse caso unowned deveria forçar a unicialização de blob[] para que depois eu possa só fazer data.append()

//

Tip here: Apenas para lembrar que o lance da memoria lá dizer quanto cada modulo gasta é muito importante. <<<________>>>

//

VAMOS FALAR DE GPU? ... Deveria ter integração com OpenCL nativa? OpenMP? AMD HIP? Cuda?

//

DEBUG:

Quando adicionar um ponto de debug no código, criar uma função para obter o state das variaveis locais e chamar o SIGPAUSE, e aguardar o resume. Isso é util no caso de código otimizado, dessa forma da pra rodar um código com perfomance similar ao produto final.
No geral preciso tentar integrar ao maximo aos debugers já existentes //https://stackoverflow.com/questions/18000214/compiling-with-both-g3-and-o3


// HOT RELOAD: Algo assim -> https://github.com/fungos/cr

Basicamente podemos fazer o unload e o reload dos modulos. Mas nesse caso temos que pegar os valores das variaveis e por novamente....
Como por padrão quem chama guarda a memoria, só no caso do modulo ter uma memoria privada para algo que deve dar algum problema; Eventualmente posso ter algum tipo de entry point para hot reload, algo para dar oportunidade do modulo guardar informação e recuperar. Similar ao hibernate do websocket. Na verdade, pode ser feito de forma generica para hibernação em outras situações.

Inclusive é uma forma pratica de fazer o debug

// Pensando de novo na questão do string... não sei se é uma boa ideia usar o ICU, melhor deixar como lib a parte. Se é pra ser um substitudo do C, ser plugavel é melhor que mandatorio nesse caso.

#embed

Alternativa: // https://stackoverflow.com/questions/4158900/embedding-resources-in-executable-using-gcc


// PONTEIRO DE FUNÇÃO

Provavelmente não é uma boa ideia, mas tenho que pensar... ter um "CalbackType" para essas situações me parece o melhor, dai o da pra ter ele em WC e apenas fazer sugar disso. Provavelmente uma struct com tudo que preciso saber pra ser safe. Da pra fazer o sugar do async await em cima disso também.

Nesse callback type dependendo da pra por toda a "engine" de um sistema de eventos e eventualmente usar também para observables. Lembrando que também da pra fazer isso usando o pointer struct, etc.

Colocaro modificador callback encapsula tal variavel com CalbackType

fn some(a: Int, callback b: Int){
  ...
  b(2);
}

b é inplicito uma função que recebe int do tipo CalbackType 

fn some(a: Int, callback x(y:int, z: string)){
  ...
  x(2,"a");
}

x é uma função CalbackType com y e z como argumentos sem retorno

fn add(a: int, b: int){
  return a + b
}
fn some(a: Int, callback x: add){
  ...
  x(2,"a");
}

x tem a mesma assinatura de add, recebe dois int e returna outro int. Mas é do tipo CalbackType e chama a função add quando acionada, sendo então um ponteiro de função.
Nesse caso é um ponteiro do tipo "weak" quando add for uma função de classe ou struct.

// Resumo (depois resumir mesmo e escreveer um outro documento)

* ModuloName: Ascii name case sensitive e lower case name. Só pode haver um nome de modulo em toda a unidade de compilação.
* Modulos: Reference Type, Singleton, ele exporta tudo que pode ser usado, incluindo os tipos. É compilado como um executavel unico e otimizado como uma lib independente.
* Main Module: O modulo apontado para o compialdor para onde o entry vai ser interpretado como "main".
* Sub Modulos: Todos os modulos que um modulo pai chama e não gera um executavel unico. Cada executavel tem só um entry point. Elas são compiladas junto ao modulo principal como libs estatics ou via source code. Otimizado junto com o modulo main. Submodulos não são exportados de forma individual, eles passam a fazer parte do modulo principal.
* Classes: Reference Type, são como submodulos, e podem ter varias instancias. Sua memoria interna é compartilhada com o modulo e seque o ciclo de vida do modulo.
* Structs: Value types, assim como o classes compartilham memoria com o modulo que a declarou.
* Objects: Maneira generica de se referenciar Classes, Structs ou Data Object Types. Possuem construtores __attribute__((const))
* Data Types: Int, Float, String, etc... São os tipos basicos. String por exemplo é u8/chat8_t *; Esses tipos podem ser usados junto com Strucs e Classes e possuem funções de auxlio de acordo com seu tipo. Muitas dessas funções são criadas usando o _Generic; Todos os tipos vão ser reconhecidos em W como se fossem um objeto, porém apenas com intuito de auxiliar a chamar os metodos utilitarios. Esses objetos utilitarios são os Data Object Types, e são removidos já na tradução para WC. String por exemplo será uma classe, Int Struct, etc.
* Object Call: É feito um inverse call em todos os objetos, por exemplo: someString.replace("a","e"), gera: string_replace(const string_t *self, const string_t *patten, const string_t *replace), logo: string_replace(someString, "a", "e"); O sistema sempre vai fazer o sugar de type_*(type_t *self,...) para o object call.

- Toda função em W é pura a não ser que seja usado mut. Apenas Modulos, Classes e Structs podem armazig zenar variaveis/propriedes.
- Quem chama é responsavel pela alocação e free
- É possivel transferir owneship e criar variaveis weak para serem zeradas quando ocorrer o free.
- Callbacks são feitos através do Entry do modulo/object; Uma entrada do tipo event(Event e);
- Ponteiros de função só atraves de um tipo especifico: CallbackType
- CallbackType é um sugar para registrar um handler no entry point e chamar uma função do modulo cujo o target é aquela função. CallbackType não capturam escopo e não podem acessar variaveis dentro da função que não sejam passadas como metadata.

- As variaveis vão ser declaradas como static ou extern; // no caso de extern seria só omitir mesmo o static, dai não preciso fazer um header pra isso.
- As funções vão ser declaradas em WC como static inline e extern inline; // eu poderia só remover o extern também para ficar no mesmo padrão das variaveis.
- Os argumentos das funções são sempre const a não ser que use mut

- O Header File vai ser gerado via pahole;

- Para evitar colisão, o nome real da variavel/função é: moduleName_variableOrFunc, onde o _ pode ser simplesmente trocado para . em W;

- O sistema sempre compila todos os modulos para lib, o main é obrigado a ser uma static lib que por sua vez vai ser importada em um template de main que irá chamar o entry e o contrutor/exports da lib.

- As chamadas de import é um sugar a um require com adição da chamada do contrutor do modulo no entry point de quem importou.
- O modificador "Lazy" adiciona a chamada do construtor quando ocorrer o primeiro uso daquele modulo. Sobe a penalidade de ter um check antes de cada call quando não por possivel provar onde é o primeiro call.
- O modificador "Eager" (ou outro nome) para chamar ativamente qualquer coisa que foi exportada do modulo para garantir que ali é o primeiro call; Nesse caso o compilador não vai tentar adivinhar nem tentar fazer checks em outros pontos do modulo.

// Self vs This

self = modulo atual
this = objeto atual

Como todas as funções são puras não existe this para funções e não existe captura de escopo.
Ou melhor, o this em função pode ser usado quando se usar função dentro de função, mas isso não significa captura de escopo automatico.

// Function Scope and Lambda

fn some(){
  return 2;
}

equal:

const some = ()=>{
  return 2;
}

Todo lamba é uma função static com atributo const assim como o contrutor.

Dessa forma:

let count = 2;
fn some(){
  return count; // okay
} 

const some = ()=>{
  return count; // error
}

const fn some(){
  return count; // error
} 


// LOGICAL AND/OR and bitwise operators

Isso: (s[i] & 0xc0) em C ....
Em W: tudo igual... na duvida consultar Swift: https://docs.swift.org/swift-book/documentation/the-swift-programming-language/advancedoperators/

// Sobre W no geral e o motivo de existir...

Poderia existir um Duffs Device em W? ( https://www.wikiwand.com/en/Duff%27s_device )

Se alguem tivesse uma boa ideia como essa, a linguagem iria se tornar uma barreira? Claso que eu poderia simplesmente usar a sintaxe do C porém, aí seria o C não W... Como o W ajudaria aí? Nos argumentos? Ajudando a marcar a memoria mapeada? Usando um jump mais claro que esse lance do do no meio do switch?

Por exemplo se eu permitir o interleave do do-while com o switch da mesma forma, eu poderia só adicionar um continue no switch e usar o auto break como nas linguagens modernas (como Swift que tem o melhor switch que conheço)... Mas seria legal ver mais exemplos de códigos que usam esse tipo de engenhosidade de tentar achar uma maneira deles existirem em W mesmo adcionando mais segurança.

Eu já havia pensando inclusive a questão dos cases poderem ser usados mesmo sem o switch, como jumps normais. Mas teria que criar uma boa maneira deles terem um escopo sano fora do switch. Jumps são perigosos.

//// REF vs STORAGE vs TRANSFER

fn some(ref User a, storage User b, transfer User c, User d){
//
}

some(a,b,c, d)

Todos os paramentos sem ref, storage, transfer são const, ou seja não podem ser modificados, todos esses modificadores são de transferencia de owndership, Porém:

storage: garante que ele vai guardar essa variavel, logo "b" ainda é valido apos o call até acabar o escopo da função.
transfer: "c" é null após o call...
ref: "a" é opcional.

No caso de "d" eu preciso dar free.

// CoW
fn some(cow User a){
//
}

Com CoW eu ainda fico com ownership, poréme eu permito uma copia automatica quando ele escrever sem um .copy()/.copy(.deep) explicito.
Também criar o .cow() e .cow(.deep)

fn getUser() {
  return this.user // this seria a classe, poderia ser self.user para o modulo.
}

Nesse caso user é copiado por completo com .copy(.deep) por padrão baseado na convesão que criei acima, onde quem chama da o malloc...

Já a propety .user pode se exportada:

someObj.user ou module.user -> Sempre é uma copia do tipo .cow(.deep)

Onde quem chama, se mutar, cria outra instancia. Nesse caso não é necessario notificar nada em relação ao orinal pois ele pode só criar outra variavel noe scopo da chama quando traduzir para o C, no W fica transparente.

A regra aqui muda um pouco, via cow não tem a alocação de quem faz call... O mesmo poderia ser feito via função assim:

fn getUser(): cow User {
  return this.user 
}

fn getUser(): transfer User {
  return this.user 
}

fn getUser(): ref User {
  return this.user 
}
// storage não faz sentido imagino.

Nesse caso quem chama essa função continua com a obrigação de fazer o free, porém no caso de transfer this.user vira null, e no caso de ref this.user precisa ser opcional pra funcionar. Via CoW explicito é a unica forma de retornar propriedades de classe ou modulo.
Quando se exporta proprietades são implicitamente CoW sempre.

copy/cow/ref/transfer para retorno é a unica forma de quem chama não ter que fazer o malloc. Dessa forma da pra retornar buffers, cache etc.

via storage não vejo sentido, pois o ref da para promover para storage, mas o storage obrigaria a função guardar em propeiradade e modulo. Não vejo uso para isso. transfer faz pouco sentido, mas faz um pouco... eventualmente não deixar transfer também, ou deixar storage também.

Isso afeta a parte de concorrencia/paralelismo.

// COW

pointeiro de ponteiro:

_Atomic const struct {
  void* actual
  bool free
  // void* owner // provavelmente nem precisa?
}
ou
const struct {
  void* actual
  _Atomic bool free
} // provavelmente essa é a versão correta...

Vale lembrar que tamanho maximo do atomic em alguns casos é 128bits... ( uint128_atomic ), porém o 64bits provavelmente vai ser mais portatil. Preciso ser criativo aqui depois... Tenho que ver se vou ficar trocando a estrutura base sempre ou só esse valor do free, pois no caso preciso manter o CoW... A doidera seria fazer agregados de objetos de 64/128bits todos lock free... seria uma merda gerenciar isso porém vale lembrar que:
_Atomic(double) atomic_array[5]; // valid
Para strings seria super simples, para array de objetos seria complicado, ou não, nem tentei ainda...

typedef union vec {
        struct {
           _Atomic float x, y, z;
        };
        float xyz[3];
} vec; // Posso criar acessos atomicos e não dependendo da situação
// sample.x = 2;
// sample.xyz[1] = 3;
// problema que isso vai remover um monte de otimizações possiveis, melhor só usar _Atomic quando extritamente necessario...

Uma outra coisa é que posso simplesmente declarar como variaveis diferentes e simplesmente usar algum prefixo nominal como pretendo fazer com as funçoes de classes...
_Atomic class1_prop1_name = ....
// https://stackoverflow.com/questions/50601726/is-it-ok-to-use-stdatomic-with-a-struct-that-is-pod-except-that-it-has-a-const // aqui inclusive diz:
"
If you have 3 separate atomic objects, don't put them all in the same struct if you want to use them from different threads. Having them all in one cache line will cause false sharing, so threads using counter1 will contend with threads using counter2. (If they're usually all used at the same time, then in the same cache line is good, though.)
""
Importante nessa thread é a questão do uso do alignas para entender contenção de cache.

// little old mas segue a ideia, nem tudo está ordenado aqui nesse doc...
Quando sobrescrever vai ter que usar o memcpy e trocar a struct. Isso vai aftar o value do ponteiro que veio a objeto e pra onde eu mandar isso pra frente, mas o original eu deixo pra quem fez o hold fazer free. Nesse caso eu me torno owner e no fim do escopo eu vejo sempre se eu fiz ou não a copia pra fazer free ou não de variaveis obtidas como cow. Da pra otimizar isso em varias situações pra não precisar verificar se for bem explicito e criar esses valores inplace o compilador vai saber o resultado dos ifs e otimizar.
Isso é uma coisa importante aqui, códigos bem explicitos para o O3 pegar
Como eu posso não ter valor aí, variaveis cow são opcionais caso eu guarde esse ponteiro. Em ambiente single thread ela vai ter valor até eu acabar a função. Mas tenho que fazer o transfer se eu for mandar ela pra frente caso eu tenha feito a modificação, logo ela obrigatoriamente se torna opcional, eu posso até eliminar os checks no mesmo call, mas tenho que ver isso em multithread.

// PARALELISMO

Tentar algo parecido com o libdispatch => Ler com queue_sync. Escrever com barrier_async. Provavelmente fazer em C mesmo com mutex e pthread.

let a = async setSome(x) // concorrente
let b = spawn setSome(x) // parallel // ou exec setSome()

let c = queue getSome() // ??
let d  = queue<.background> setSome() // ??

... tenho que revisar o documento, tem muita coisa maluca que escrevi antes...


Substituir propriedades exportadas por getters onde ele usa 0 Read-Copy-Update (RCU) para saber se pode ou não fazer aquela operação?
Funções reescritas quando acessam propriedades para verificar o mutex/semaforo??

Se cada modulo é single thread, cada vez que uma propriedade ou função de outro modulo é chamado usando `async` ele deve então pegar uma thread do pool para aquele modulo, se uma já foi criada para aquele modulo usa-la....

Porem se eu leio `sync` uma propriedade de outro modulo serei obrigado fazer usar o RCU... Como saber se outra thread não está operando lá? Logo tenho que também fazer isso na outra thread? Se sim, facilita mas é menos escalavel. Dessa forma toda operação em outro modulo é uma troca de thread, mas... e se eu não quiser isso? As vezes estou fazendo código single thread. Criar diferentes variantes async/sync/ e sem marcadores para isso?...

Se esse for o caso, eu posso usar essas variantes apenas quando ocorre de qualquer outro modulo estar consumindo o thread-pool. No caso de código W puro, é possivel eu exportar os target modules que eu vou usar e provar que eu to usando recursos que podem colidir, logo nesse caso eu posso forçar os "locks"/RCUs nas operações entre modulos. Mesma coisa se alguma coisa estiver rodando usando |async| * seja lá qual vai ser a notação para necessariamente fazer algo em paralelo além das corrotinas.

Cada modulo poderia ter na sua config padrão quais threads, priodidades e quantas vão usar:

module {
  #dynamic maxsize=1G
  #auto is static up to 100M
  #String dynamic maxsize=50M
  #threads: [.background] // poderia ter algums padrões, mas também permitir .module, que vale apenas para esse moduleName
  #afinity: [.cpu1] // apenas uma, não necessariamente a 1, se por 1 e 3 ele vai tentar essas mas no fim vão ser duas se tiver, tudo é tentativa.
} 

dynamicThreads: .CPUs // ?? auto, false, 2x.CPUs

Ou via import sobrescrevendo:

import Z from module 'Z' using { #threads: [.network] }

Esse tipo de informação explicita pode ajudar a saber qual versão chamar de cada propriedade ou função logo:

export {
  Prop1,
  sync Prop1_bacground_get,
  async Prop1_bacground_set,
  someFunc,
  async someFunc_bacground,
} // Ou qualquer outra notação aqui, seja lá qual for, seria chamado automaticamente de acordo com a necessidade. Por exemplo, se estou na main mas existe código rodando em background, essa função vai chamar o prop/metodo desse modulo em background pra garantir o correto funcionamento.

Seria como o pthread reader-writer lock, porém mais automatico. Da pra evitar essas chamadas quando for possivel provar que são desnecessarias, e basicamente teremos uma estrutura lock-free. Eventualmente podemos marcar as funções para usar o Reader-writer-lock ao invés do RCU

module {
 
 let someProp #rwl // #rcu é default
 
}

export {
  someProp #rwl,
}


Lembrando que o proprio modulo é sempre um execelente candidato para servir de lock, mesmo via mutex.

lock(&this)...

Lado bom que posso deixar o RCU também como padrão ao invés do .CoW(), dependendo se eu for usar uma lib ou mesmo programar algo.

Essa lib parece ser o que preciso: https://github.com/nicowilliams/ctp // http://liburcu.org/

Lembrando que tudo isso é implementando em cima de pthreads de qualquer forma, esse mesmo usa pthread_cond_t, ou seja, posso começar tentando implementar o mesmo conceito, ver e depois ir para a lib, isso para entender melhor o que serve ou não serve no ponto de vista da linguagem. Pois diferente de uma lib eu tenho controle no que é reencristo para C, posso criar multiplas variantes do mesmo codigo etc. Veja o COW por exemplo, a ideia inicial que eu tenho do COW seria usando apenas uma struct e um if... Nessas libs é bem mais complicado pq eles não podem mudar o dado, mas eu faria isso no objeto em sí.

RCU parece não ser tão bom para escrita, ver se da pra superar isso, apesar que minha preocupação de fato é leitura. Toda a linguagem é meio que é se olhar bem..

Claro que também o read-writer-lock me parece uma boa ideia, apesar da issue do openSSL dizer que é 20% pior que o RCU // openssl/openssl#20715

Obviamente também pode ter multiplas implementações por plataforma, no mac pode ser o os_unfair_lock, no windows sei lá o que...

Esse sto: https://stackoverflow.com/questions/59979509/why-would-pthread-mutex-lock-hang-if-pthread-mutex-unlock-has-been-called

Inclusive comenta sobre o patthen que sempre gostei de usar do GCD, onde aqui me inspiro, lembrando obviamente que alguns já apontaram que o GDC não é lá essas coisas também: https://gist.github.com/tclementdev/6af616354912b0347cdf6db159c37057 

E claro lembrando que dificilmente da pra bater a performance de algo implementado direto no kernel: https://trycombine.com/posts/performance-actor-queue-lock-benchmark/

Via lib cosmos que posso usar como padrão para algumas implementações, no linux e windows eles usam FUTEX: https://justine.lol/cosmopolitan/functions.html

Eles se baseam no nsync: https://github.com/google/nsync

"nsync locks are reader-writer locks (but are as efficient as mutexes)." <- O que é uma boa se ficar mais simples que o RCU...

No fim vou ter que fazer varios benchmarks.... No geral a lib cosmopolitan tem varias vantagens de já tratar isso, e pelo que vi nas issues até lib grafica estão colocando! E pelo que vi tem o io_uring no sysv.

De qualquer forma o proprio C11 ja tem muita coisa: https://nullprogram.com/blog/2014/09/02/
Paper: https://mirrors.edge.kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook.html
https://concurrencyfreaks.blogspot.com/2019/10/is-rcu-generic-concurrency-control.html


E provavelemnte o melhor materia nisso: https://mirrors.edge.kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook-1c.2023.06.11a.pdf

Uma boa implementação dessa parte de modulo e RCU vai precisar de CPU afinity.

Outra coisa, o que preciso em W muitas vezes pode ser realizado apenas usando o C11 _Atomic; Não existe motivo de W ter target menor que isso mesmo.
Garantindo a escrita correta, só preciso de um CoW e done...

// RCU vs COW

Segundo o stackoverflow: https://stackoverflow.com/questions/24598636/difference-between-read-copy-update-and-reader-writer-lock
`
RCU - Concurrency is allowed between a single writer and multiple readers
Reader-Writer locks - Concurrent reads are allowed however not the write operation
`
RCU faz algo parecido com um CoW, mas para o contexto de paralelismo.
`
 ...as the writer does not modify the data in-place, but instead allocates a new element which it initializes with the updated data. The old element is replaced with the new element using an atomic pointer...
`
Quem é como eu por padrão pretendo fazer no gerenciamento de memoria do W, sempre trocando a variavel, as vezes com memmv, mencpy etc...

Aqui ele fala que quem tem a copia antiga precisa ser notificada, bem isso acho que pode ser opcional e a pessoa pode usar um observer para isso. Já pretendia por isso na linguagem de qualquer forma.

No contexto de memoria do W:

No .cow() quem obtem que vai saber se precisa ou não criar, "ele" a copia e ele vai dar free se precisar na copia dele. Ele não mexe na original. Porém ele vai ter que verificar sempre se foi dado o free ou não nesse dado, logo ela é opcional.

No fim qualquer uso de RCU em W pode ser tratado como uma especie de "Threaded" CoW onde temos que garantir que a variavel de controle do free, seja consistente. Provavelmente usando atomics ou um read-write-lock. O _Atomic já deve resolver, mas é bom eu olhar com mais cuidado. Até porque eu to trabalhando com struct como holder principal de todos os dados na aplicação.

Aqui um bom take: https://concurrencyfreaks.blogspot.com/2019/10/is-rcu-generic-concurrency-control.html

// property handler e property observers

sono então descrição rapida:

(obj/module).someProperty.use((value, setValue)=> ....) // uma forma de ter que ler pra mutar o objeto, isso evita chamar duas vezes o lock (se implementar coisas com lock), um para leitura e outro pra gravação. simplicando casos como:


someProperty.use((value, setValue)=> {
  setValue(value+1)
})

transaction? state? handle?

Parece muito react.. Nesse caso tem a vantagem que a parte do paralelismo fica mais safe.

Para tratar a volta em uma especie de property behavior:

someProperty.willSet((value, setValue)=> {
  setValue(value+1)
})

someProperty.didSet((value, setValue)=> {
  setValue(value+1)
})

O handler nesse caso é um CalbackType... e só vai executar quando ocorrer o set ...


// GUI

https://github.com/rxi/microui ??
https://github.com/Immediate-Mode-UI/Nuklear ??

// multithreads continuação

module {
  #threads: .module
}

module {
  #threads: [.background, .network]
} 

module {
  #threads: .none  // default
}

Regras basicas:

Se o modulo não threads ativadas, logo não preciso de atomic nesse modulo. Todo call async deve ser transferido para a thread do modulo.
Threads não precisam ser alocadas especialmente para o modulo, o modulo diz que só suporta uma por exemplo, vai usar a thread que chamou, só não vai permitir outra thread ser criada, só com join na já ativa.

Ironicamente quando um modulo só tem uma thread, é necessario chama-lo com async pois nunca se sabe se ele está busy com outra task. A não ser é claro que nenhuma outra thread consiga chamar esse modulo. Por isso também é importante cada modulo expor sua call tree. Se um modulo é leaf e é single thread, ele nunca vai ter outro modulo usando ele ao mesmo tempo. Isso é importante porque modulos em W são singletons. Ao exportar a calltree o sistema sabe se existe change ou não de um modulo ser chamado por mais de uma thread. Dessa forma

modulo com uma thread: todos os exports são chamados por padrão via async. Nào é necessario colocar async na chamada, o sistema vai automaticamente presumir isso.
modulos com mais de uma thread: as chamadas por padrão são como em todas as liguagens, sync, só usando async para criar a concorrencia comperativa.

Por falar nisso: Cada thread tem seu proprio sistema de corrotina independente. Com isso todos os deadlocks podem ser prevenidos. Pois se chamar um modulo que permite multi thread mesmo com sync ele não vai impedir, se chamar um que não tem mais de uma thread permitida, pela config, ele sempre vai ter que usar o async. Por isso:

Main thread / main modulo: Sempre 1 thread. Você nunca pode chamar o modulo main sem async pois ele sempre será single thread. Isso é importante pois o main pode ser usado como thread de sincronia. E o modulo principal é sempre o responsavel por criar todas as outras threads. Os outros modulos apenas herdam eles. No caso das filas principais definidas (net, back etc) são apenas as threads principais que vão ser criadas caso esse modulo seja chamado com `spawn`.

Desencorajar, pois acho dificil proibir por causa do C outros metodos de chamada de thread. Usar os metodos principais, a ideia é que os modulos, mesmo nas libs nunca chamem nada explicito em relação ao paralelismo ou concorrencia, apenas:

* async, await, sync, yield, yield*, spawn, fork // exec?

E os modificadores para definir mais dados como: spawn<.background> etc...
O spawn tem o mesmo efeito do async para paralelo.

Resumindo: // o tipo de dado/função chamada, e qual operador vc usa na chamada. Por exemplo, uma função com contrato async, chamada com await.
// Aqui é legal também pensarmos melhor na questão do que falei lá em cima, seria importnate asumir a assinatura da função como um contrato imutavel.
// uma função anotada com os modificadores de paralelismo devem sempre serem chamadas com alguma coisa, diferente do que falei antes.

* async / await // aguarta até a resposta
* spawn / await // aguarta até a resposta, uma função do tipo spawn é presumido sempre criar, acessar ou estar aguardando alguma thread.
* async / sync // faz com que drop todo o async quando possivel e leia como se fosse uma função normal.
* async - yield / await // vai ler todo o stream, não só o parcial. A diferença aqui é que a função async pausou multiplas vezes.
* yield / await // O tipo intermediario é Stream<T>, await le aos poucos, como um interador um um transformstream.
* ___ / async // qualquer coisa com async vai tornar aquilo async, sendo necessario o await para obter dados de novo. O tipo intermediario seria `Promise<T> `
* ___ / spawn // qualquer coisa com spawn vai tornar aquilo spawn, sendo necessario o await para obter dados de novo. O tipo intermediario seria `Task<T> `
* RAW / yield* // qualquer bitstream ou tipo interavel pode ser chamado via yield*, util pra transformar algo em Stream<T>



yield* ou stream ?

yield*<.waterMark = 4K> // ?? Poderia ter uma config para o modulo/classe, ou individual. Se por na assintaura da função vai valer pro escopo da função e não pode sobrescrito dentro de função porque se torna um contrato.
stream<.waterMark = 4K> // ?? seria melhor chamar de stream pra não ter que usar o *, apesar que yield* já é conhecido em javascript.

** fork // mesmo caso do spawn porém usado para duplicar um modulo. Já que le é singleton o fork pode ser usado para outra instancia do mesmo modulo.

import fork { func as func_alt } from 'X' 

Dai se não existir essas threads por exemplo, vai ser criado, o main vai ter essas variaveis guardadas de forma atomica, provavelmente deixaria essas variaveis globais mesmo, ou passaria para todos os modulos na inicialização, tanto faz. Isso inclui as customizadas, no caso o modulo precisa exportar junto da call tree todos os nomes de threads customizadas que ele vai usar. Assim o sistema vai saber, incluindo o dev, maximo de threads que pode ser criado, etc. Isso casa bem com o gasto explicito de memoria que o modulo também vai exportar.

Pode ser usar um callback do sistema, não um proprio, para matar essas threads depois um tempo de inatividade, 1s por exemplo. Ou podem morrer logo depois da ultima chamada. Por exemplo, se o modulo A queria usar a thread X, mas foi chamada pela thread Y, ele vai executar em Y se tiver o multithread habilitado, porem depois do modulo A acabar for chamado pela thread Y, mesmo que X esta ativa, modulo A agora não tem thread corrente, com ou sem multithread não faz sentido mudar de thread agora, logo ele vai usar Y, e X se estava ativo apenas para ser usado em A pode morrer imediatamente. Ou apenas marcar falta de uso e esperar o callback de um timer para limpar isso. Afinal desperficar tempo da calltree durante a chamada para o modulo para cleanup não é legal. A linguagem não tem GC mas seria interessante algo inteligente aqui para alguns cleanups obvios. Um "setitimer" na main pode resolver isso.

Quando tiver concorrencia, usar atomic por padrão em tudo que é exportado. E como em W tudo o modulo é de fato executado antes do export, é possivel compilar com toda as variações de cada propriedade, atomica ou não e usar a correta na exportação durante a inicialização do modulo para evitar perda de performance depois. As configurações de threads são sempre herdadas se não for explicatamente configuradas para aquele modulo.

O compilador pode ser esperto também e ver que não existe nenhuma chamada paralela para aquele modulo e otimizar isso.

As variaveis são adicionalmente protegidas por Copy-On-Write, gerando um tipo de RCU porém bem mais simples. O resto das regras que progem o APP vai ser o escopo de thredas do modulo e as corrotinas dentro do proprio modulo.

Com isso temos uma linguagem lock free com boa performance sem precisar de event loop ou libs complexas.

Outra coisa, como todos os modulos eu vou usar o `entry` que sempre pode ser o main, eventualmente o verdadeiro main pode nem existir e sempre chamar um pthread para subir o __stacksize quando o sistema calcular o stack maior que 1mb ou outro default...

// Thread Pool:
https://nachtimwald.com/2019/04/12/thread-pool-in-c/
https://github.com/cameron314/concurrentqueue
https://github.com/cameron314/readerwriterqueue
https://moodycamel.com/blog/2014/a-fast-general-purpose-lock-free-queue-for-c++#benchmarks
https://github.com/max0x7ba/atomic_queue  // OptimistAtomicQueue é cabulosa

https://max0x7ba.github.io/atomic_queue/html/benchmarks.html // Tentar bater esses scores aqui kkkk

Uma coisa que vi aqui, é que sim, as vezes deixar os detalhes para fora da linguagem, se async ou spawn vai chamar outra uma fila de uma forma, ou uma pthread ou queue é interessante pq permite otimizações por baixo dos panos incluindo no futuro no compilador e mantendo a mesma semantica. Aqui só preciso definir um conceito comum que posso aplicar usando varias tecnicas. Por isso é interessante mover isso pra linguagem ao invés de fazer via lib, a linguagem evolui e todo mundo ganha junto. Mas claro, fazer de uma forma que também seja possivel fazer essas coisas via lib. A pergunta fica: Quais primitivas manter para que uma lib como a atomic_queue possa ser criada em W sem ou com pouco apoio do C ?

Vejo que essa lib do atimic queue meio que prova o conceito que eu tava querendo... Usando atomic como padrão ao invés de locks é muito, mas muito mais rapido. Ele usa um ring-buffer, não sei se isso substuiria minha corrotinas, mas ainda acho que com elas eu poderia superar eles.

// Leitura de arquivos, sockets etc.

Pelo que vi nada supera o io_ring ... porém isso é trabalho para lib, então na linguagem acho que o melhor usar o basico do posic mesmo.
Tenho que testar o poll() / ppoll() / epoll(), claro que eventualmente posso usar as libs "padrões", como LibAIO, IOCP etc.

Provavelmenete as regras da linguagem são: Usar C/LibC, System Calls, ou libs de referencia já consideradas de alguma forma "padrões"...

Logo boa parte das libs que eu falei acima são só para aprendizados, mesmo a libdill ... Sei que o usockets é muito bom porém... posso deixar em alguma lib do W, não na linguagem ou na std lib.

Tem também as variantes de deixar tudo como stream usando nmap ou simular....

// Pequeno hot take novamente relacionado a atomic:

Strucs provavelmente não precisam disso em todas suas properties por ser value copy... Outra diferença que posso fazer na hora de criar os object likes. Mas sinto que to perdendo muito tempo com isso, melhor eu começar a implementar alguma coisa pra ver o que fuciona melhor.

check: atomic_array.c

atomic int acnt[100]; ou similar pra string pode ser o padrão de quando for multread para qualquer "List"/"Array" (seja lá qual for o nome da coleção); As versões sem suporte a multread devem receber o modificador:

Array<.noAtomic>(100) // ou similar...

// DIRETIVAS

Caso eu não tenha deixado claro:

#config {
  threads = 1 // ou sei lá...
}

tem que set tudo define em compiler time. Para o compilador selecionar o melhor path logo. Assim por exemplo posso saber se vou fazer call nos atomics ou não e outras coisas também.
No caso das libs, tenho que exportar todos os simbolos corretos, algumas coisas vão ter que ser dinamicas nas libs infelizmente. Eventualmente posso usar o source quando ocorrer override de diretivas das libs, posso deixar isso configuravel no build. (--rebuild-on-directive-override, -rodo)

// Modulo dealoc & background threads

Lembrar de rodar o que resta de corrotina em cada modulo no dealoc. Pensar no cenario de ter a thread de background executando left overs nos modulos adicionais.
Para on shot apps eventualmente algumas coisas como limpesa de momoria, sockets etc, pode ingorada e deixar isso para o SO se estivermos no dealoc da *aplicação*...

// On shot executables vs services

Quando um executavel precisar ficar rodando indefinido até um evento externo para-lo; Nesse caso faz sentido de ter uma thread background padrão para executar tasks de baixa prioridade, como logs. Dessa forma da pra dar async em todos os logs e saber que eventualmente eles vão rodar.

Para isso dar certo todas precisamos de uma visão olistica completa de todas as threads e modulos para saber o estado das corrotinas em cada uma delas.


// FORK YOURSELF!

Como modulos são sempre single threads e singletons, para eu rodar algo em outra thread em mim mesmo só com "fork yourself", nesse caso "fork self":

fork self.someFunc() // ao invés de async someFunc.

O self aqui é opcional, como sempre, apenas pra deixar clado que fazer fork em uma função de sua vai gerar um outro modulo no momento.

const myCopy = fork self;

isso seria uma forma de fazer um fork de seu proprio modulo e ficar chamando coisas nele repetidamente.

// THREADS AGAIN!!!!

Primeiro:

#module {
  #threads: [.background] // poderia ter algums padrões, mas também permitir .module, que vale apenas para esse moduleName
  #dynamicThreads: 100 // Aqui dizemos quantas threads adicionais esse modulo pode produzir em runtime. usar +INF para numero ilimitado de threads.
}

Não sei ainda se deixamos alguns desses parametros configuraveis em runtime, via ENV ou algo assim, mas provavelemente deveria usar uma outra notação como:

#runtime module {
  #dynamicThreads: 100 // Essa variaveis agora vai adotar esse valor em compiler time, mas vai aceitar o override via runtime usando "algo"...
}

#env module {
  #dynamicThreads: 100 // Essa variaveis agora vai adotar esse valor em compiler time, mas vai aceitar o override via runtime usando necessariamente ENV 
}

A parte legal aqui é que acho que agora é possivel resolver o problema que eu tava tendo pra saber como chamar funções em outras threads e saber se tenho que subir essas threads.

Basicamente durante a compilação pegar toda o modulo call tree e gerar em global algo como:

struct threads {
    _Atomic main: <pointer> | NULL
    _Atomic background: <pointer> | NULL
    _Atomic network: <pointer> | NULL
    _Atomic io: <pointer> | NULL
    _Atomic moduleA: <pointer> | NULL
    _Atomic moduleXX: <pointer> | NULL
}

É um puta objeto grande, mas fica muito mais facil pro spawn...No WC ficaria bem mais type-safe;

E dai na hora que fizer for subir uma thread, é possivel passar via o endereço de qual thread ela é. Dessa forma vc sempre pode saber a thread corrente. E quando for chamar outromodulo, daria para olhar o current daquele modulo:

_Atomic moduleName_currentThread: <pointer>

Quando for dar thread_exit, colocar esse ponteiro em NULL;

Ainda tenho que pesar em fazer caso alguem tente reusar esse modulo logo quando ele está perto do exit, provavelmente alguma vou ter que ter um mutex na função, não deve ser nada complexo, só não pensei ainda.

O ponto aqui é:

var someVar = async someFuncOnOtherModule() // Se moduleName_currentThread for null ou igual a meu currentThread, apenas chamar a função na respectiva corrotina. Se não schedular lá...

var someOtherVar = spawn anotherFuncOnOtherModule() // Se moduleName_currentThread for null ou igual a meu currentThread, olhar na config qual a thread que ele tem configurado pra subir e criar ela.

Penso que os padrões de thread são:

.main
.background
.network
.io
.module

qualquer modulo pode chamar essas threads, elas são gerenciadas pelo objeto global. Quando os modulos são criados eles recebem o ponteiro desse objeto para poder chamar essas threads quando apropriado.

As threads dinamicas seriam sempre controladas por cada modulo em runtime durante a inicialização do modulo. Caso acabe as threads desse modulo, é necessario um fork do mesmo modulo com outro valor.

void _Atomic dynamic_moduleZ_threads[SIZE] = { <pointer> , <pointer>, ... } // threads dinamicas definidas quando o modulo inicializa.

`dynamic threads` são resolvidas em runtime e computadas individualmente por cada modulo. Para fazer spawn em uma env dynamic especifica é necessario outra sintaxe que vai fazer os checks corretos:

var someOtherVar = spawn<dynamic, num:1> anotherFuncOnOtherModule()

Lembrando aqui que:

import { anotherFuncOnOtherModule } from 'ZZZ' #configOverride
import fork { yAFOOM: anotherFuncOnOtherModule } from 'ZZZ' #configOverride

var xzy = spawn<dynamic, num:1> yAFOOM()

Isso incluive permite criar facilmente grupos / shards... Cada fork pode ser um grupo com até X threads. Além da main de cada modulo/fork.

// TYPE MODIFIERS:

spawn<dynamic, num:1>
string<length: 2>
...

Essas são strucs que podem ser usadas em conjunto com ouros via ;

String<length: 12> 
String<mask:CPF, inputType:Number>
String<maxLength: 12; mask:CPF, inputType:Number> 

Cada tipo deve definir quais modificadores aceita:

class myString{
  #modifiers: [
    {size, min:Number, max:Number, expected: Number}
    {dynamic, min:Number, max:Number, expected: Number}
    {length: Number, maxLength: Number}
    {mask:CPF, inputType:Number},
    {filter, ptr:String<length: 25>}
  ]
}

A ideia é assim: Você pode por um token no inicio de cada tipo e ele vai tipar aquela coleção qualquer de opções, dai pode usar os mesmos indificadores em outro grupo, min, max, etc... Se não usar esse token de nome ele tem que ser unico entre todas as opções. Esse token é um "named index", e pode ser usado em qualquer array statico. Ele define a posição em um array atraves de um nome. No caso o compilador pode gerar um enum que com o valor do index apontando para o nome. Isso pode usar em ranges e outros situações.

No contrutor você vai receber um array com essas opções preenchidas em suas respectivas posições:
modifiers, cada modifier tem um handle padrão.

// THREAD AGAIN!!

O conceito de cada modulo ser single thread é opcional. Sempre considerar N threads, a apenas apontas as threads defaults na config e as que o modulo pretende criar dinamicamente. Mas não impedir de outras threads usarem esse modulo, a não ser que mude isso em callPolice.

To vendo um esquema de sinais para tentar simular deixar alguams threads dormindo e acordar elas para processar mais coisas, dai evito ter que criar as 5 threads padrões toda hora.

// Call Police (module e object)

#callPolice: .none | .singleThread | .rwLock | .rcu | .cow ...

// CORROTINAS!!

Depois de muito pensar, boa parte das implementações de corrotina não vão funcionar para o modelo de paralelismo aqui explicado porém se...

fn someFunc(): O{
  ...variables...
  var o = await otherFun()
  ...
  return o
}

Se transformar em:

fn someFunc_s1(): Promise<O>{
  ...variables...
  let promise = new Promise<O>;
  store_async({variables}, someFunc_s2, promise)
  return promise
}

fn someFunc_s2({variables}, promise): void {
  var o = otherFun()
  ...
  promise.resolve(o);
}

Coisas importantes aqui:

Se cada vez que eu chamar o async/await/spawn etc... Eu quebrar a função em partes. Posso:

1. Resolver a promise em paralelo-> se for spawn principalmente
2. Resolver o resto do segmentos apenas quando ocorrer o await -> quando a quebra foi via async/spawn
3. Não preciso de jump/longjump ou lidar com toda a stack, apenas o que eu copiar para o store
4. Posso pausar em um thread e voltar em outra sem problema.
5. Não preciso de event loop!! Apenas que até o fim da thread, ou do main, antes do exit eu processe todo o store_async.
6. Na main, ele vai ter que executar serializado, já que não tem pra onde "voltar" a call da promise se houver quebra. Virando efetivamente "sync". A não ser que esteja em um runloop.

{
  fn runloop<while(signal)>(){ // só pode ser usado no main como entry
    ....
  }
}

// Dics, json, wlon, comptime e profile

#pragma pack

var obj = SomeObject { literalPro1 = value, literalProp2 = 42, ...}

A ideia é: Ter uma spec de todos as iniciaizações literais. E isso formaria o wlon, que assim como o json deve ser capaz de inicializar todos os objetos possiveis em W.

W.parse<T>(string)  / ObjectType.from(.wlon)
W.stringfy(obj) / obj.toString(.wlon)

obj.toString(.json) // ??

A ideia é ter suporte ao wlon e json, para facilitar a vida da pessoa mesmo. Porém o legal do wlon que posso usar no compilador para gerar qualquer comptime:

...
bla bla bla
var someValue = comptime someFunc(literalArgs)
...

Se someFunc() retorna um objeto compativel com wlon, isso é, todos os objetos que possuem representação literal. Posso simplmente pegar o retorno dessa variabel e por na declaração como string. No caso do comptime esse modulo precisa ser capaz de executar essa função sem intervenção do usario. Isso é, someFunc não deve ter argumentos mas se tiver também devem ser wlon, isso é ligerais ou resolvidos via literação no toString/stringfy; Dai fica facil de contruir o código intermediario pois uso esse resultado direto no source code:

var obj = ObjectType { pro1 = "x", prop2 = 42, ...}

onde { pro1 = "x", prop2 = 42, ...} é o resultado do comptime

E:

var someValue = profile someFunc(nonLiteral) // ou pgo

Isso seria usado em runtime e o valor essa função, se for um wlon, seria utilizado na proxima compilação como literal, se nenhum modulo ou submodulo tivesse modificação.

Com isso também:

var someArray = [{someLabel, 33}, 44, 55, {lolo, 66}]

gera:

enum someArrayLabels {
  someLabel = 0,
  lolo = 3
}

Dessa forma

someArray.someLabel == someArray[.someLabel] == someArray[0] == someArray[enumFromName("someLabel")]
someArray.lolo == someArray[.lolo] == someArray[3] == someArray[enumFromName("lolo")]

fn enumFromName(string name) 
{
   switch (name) 
   {
      case "Sunday" : return Labels.Sunday;
      case "Monday" : return Labels.Monday;;
      /* etc... */
   }
}

Para tudo que da pra computar em compiler time, daria pra produzir Dics O(1) bem rapidos já que

dic = { "k1": "v1", "k2": "v2", "k3": "v3"}

vira:
dic_kv = [{k1, "v1"}, {k2, "v2"}, {k3, "v3"} ]
enum dic_kv_enum {
  k1 = 0,
  k1 = 1,
  k2
}

dic.k1 == dic.get("k1") == dic["k1"] == someArray[enumFromName("someLabel")]

Para estatico ta tudo certo, não preciso computar hash porém para runtime

dic = { }

dic.set(runtimeKey, runtimeValue)

hash = HH32(runtimeKey)

dic_kv.add({k: hash, v:runtimeValue, name: runtimeKey})

get(hash): { // pseudo codigo 
  int len = dic_kv.value.length;
  for (int idx = 0; idx < len; idx++){ // não precisa do for se eu tiver array de tamanho fixo e inserir em hash % size
    struct kv = dic_kv.values[idx];
    if (kv.k == hash){
      return kv.v;
    }
  }
}

remove: dic_kv.value[removeIndex] = dic_kv.value[--logicalSize]; // Quando o tail ficar com X elementos vazios da pra reduzir X-1 buracos via realoc. Aqui ordem não importa mesmo. No caso do dic provalmente é melhor usar uma linked list mesmo... Da pra inserir e pesquisar de forma binaria ordenada. E claro o ideal é ir criando arrays com certo numero de elementos, 100 p.e, e ir ir escalando e reposicionando os elementos da tabela. Mas depois penso melhor.

O ponto aqui é que usando runtime se eu não usar uma estrutura de dados mais inteligente vou precisar interar pra achar a chave. Nesse caso o comparador ser uma hash HH32/XXH64 é muito mais rapido. Não tenho porque de fato guardar as chaves em string. Eventualmente posso até guardar para poder obter via .keys. Mas dai eu teria que ter o .values separado.

Uma implementação possivel semelhante seria: https://www.wikiwand.com/en/Linear_hashing

Posso tentar criar uma linked list e fingir que é um array com tamanho igual ao int, por causa do XXH64. Então criar uma lista encadeada com esses elementos de forma ordenada e pesquisar de forma binaria nessa lista fingindo que ela contem todos os +4bi elementos baseado explicitamente no valor do hash. Por ser uma lista encadeana e não um array não posso buscar em um passo, seria no maximo logn se tivesse todo preenchido, na pratica bem, bem menos... Entre 5-7 passos eu acharia na maior parte dos casos. Ou uma Binary Search Tree, sei lá... Na hora que eu for fazer cada tipo decido.

Aqui tem umas ideias: https://stackoverflow.com/questions/4057258/faster-than-binary-search-for-ordered-list
https://www.geeksforgeeks.org/


// SORT

TimSort all the way...

https://github.com/swenson/sort
https://github.com/patperry/timsort/
https://github.com/LVPGroup/TimSort/

"Timsort was optimized for applications where comparisons are more expensive than moves - as they are in the Python interpreter." // Não é o caso do C no geral.

Or not? Because:

https://github.com/scandum/fluxsort

FluxSorte parece bem promissor!

E essa é uma das coisas que quero pra o W, poder ter diversos algortimos de manipulação de dados etc que outras linguagens simplesmente não atualizam... Sai muita coisa lega pra C nesse setor, mas tem que depender de pessoal fazer lib etc. Sort e Data Storage são coisas que é o ideal da linguagem ter varias opções e a comunidade ir ajudando a otimizar. O Sort padrão pode ser um, mas ter facilemente o resto é importante!

E tem também o wolfsort... https://github.com/scandum/wolfsort
E o blit: https://github.com/scandum/blitsort
E esse: https://github.com/mlochbaum/rhsort

Isso não acaba! E sim, toda hora vamos ter que por uma implementação nova de algum sort... mas a lingua tem que evoluir né? Se for só depender de lib ninguem pesquisa essas coisas o tempo todo!

O blitsort me parece bom para o default em W porque faz bem inplace e usa menos memoria.

E o mesmo cara fez um binary search muito massa: https://github.com/scandum/binary_search

Dai voltando as raizes do projeto.... Vem a pergunta:

Seria possivel criar isso em W? -> https://github.com/scandum/fluxsort/blob/main/src/fluxsort.h

Veja as macros, veja o tanto de otimização possivel.... Seria legal chegar nesse nivel via comptime ou similar.

O lando o `wlon` pode ajudar, mas é insuficiente para esse tipo de otimização aí. Tudo bem que boa parte é apenas por falta de generics em C. Eventualmente daria pra fazer em W usando generics e fork do modulo com novas configs...


// GCC vs CLANG

Tem muita coisa que tem funcionando bem melhor em GCC nessa parte de dados... Clang é muito bom, mas o GCC parece muito melhor quando a coisa aperta! Esses sorts todos aí por exemplo o autor diz que é 2-3x mais rapido no GCC.. muita diferença.

O problema é perder o Zig CC...

// Protocolos e herença!

Essa thread no X me fez refletir uma coisa sobre o W: https://twitter.com/Hasen_Judi/status/1781691223646089467

Como bem é abordado no caso da discursão que ele se baseia: https://github.com/cmuratori/misc/blob/main/cleancodeqa-2.md

Eu sei que muitos na thread meio que concordam com Casey, mas eu meio que - em uma primeira lida - concordei com Bob... A ideia de ter isso resolvido em link time é otima!

Mas claro que pensando em performance Casey está certo, porém... Isso me levou a pensar em diversos problemas no W em como vou lidar com protoclos, ex:

class SomeClass: SomeSuperclass, FirstProtocol, AnotherProtocol {
    // class definition goes here
}

A questão das funções okay, ainda posso usar a assinatura nominal desses protocolos e garantir que dentro de uma unidade de compialação, incluindo as interfaces para libs dinamicas, esses nomes são unicos e assim não preciso manter ponteiros de função na classe... Porém e as propriedades?

Em swift não é possivel definir novas propriedades no protocolo: https://docs.swift.org/swift-book/documentation/the-swift-programming-language/protocols/
Eventualmente vou ter que seguir essa rota... Pq sem uma vtable não vejo como por novas propriedades na struct sem quebrar....
Tem uns racks que da pra fazer em swift pra adicionar storage property, eventualmente posso fazer o mesmo. Mas basicamente no eles usam o associated types, que é um grande dicionario que mantem propriedades para determinados ponteiros de objetos...

Sobre vtable:
https://stackoverflow.com/questions/66040677/implementing-basic-vtable-in-c

Ainda não entra muito na minha cabeça, tirando questão de herança e polimofismo, porque diabos deixar o vtable junto com a struct. Sei que esse lançe de padronizar por nome de modulo/classe etc tem problemas com colisão, mas sinceramente vejo algo similar aqui... Essas estruturas também tem que tomar cuidado com alinhamento, só podem ser aditivas, etc. Nesse caso eu tenho que ir criando vtables para cada tipo na ordem da hereça, que sendo unica okay, da certo. Mas em protocolos? Não tem ordem, não da pra fazer cast para uma struct menor e manter coezão.

Lembrando que existe o _Generic, isso ajuda bastante nessa coisa de encontrar as propriedades para um tipo X. Posso criar diferentes macros para garantir que a função chamada sempre seja de fato unica a compilada com aquele objeto. E no caso compilado também, eventualmente eu padronizo tudo em hash. Nada que um XH3_64 não resolva. Foda é não poder mudar nunca a forma que computa esse hash... Bem, no caso do <moduleName/className>.<funcName> eu ia ficar preso a esse padrão da mesma forma.... Preocupação aqui é só libs mesmo... Quero exportar os simbolos no padrão correto <moduleName/className>.<funcName>; Porque por mais que possa existir colisão do modulo name, vou ter que criar uma forma de fazer override disso em runtime já que quero fazer fork; E mesmo que não fosse o caso, sempre pode haver colisão, não seria a primeira vez que duas libs criam coisas com mesmo nome. No Swift no inicio dava maior problema com libs definindo seu proprio tipo de `Error`. Da pra fazer rename em tempo de copilação também, no caso de libs estaticas é super tranquilo, já ia por o modulo name em um #define ... Mas para dinamicas

void send_narrow(const char*);
void send_wide(const wchar_t*);
void send_utf8(const char8_t*);
void send_utf16(const char16_t*);
void send_utf32(const char32_t*);
#define send(X)                            \
        _Generic((X),                      \
                 char*:     send_narrow,   \
                 wchar_t*:  send_wide,     \
                 char8_t*:  send_utf8,     \
                 char16_t*: send_utf16,    \
                 char32_t*: send_utf32)(X)
void f() {
  send(L"text");   /* Would be ok with distinct types; calls send_wide(). */
  send(u8"text");  /* Would be ok with distinct types; calls send_utf8(). */
}


Vi que esse lance de conflito de simblos muita gente resolve com -Bsymbolic or -Bsymbolic-functions , mas não sei muito bem, tenho que fazer um exemplo pra testar.
Outro exemplo que vi que é o que eu imaginei seria usando os defines mesmo: " if the namespace is LibNS, use -DLibNS=LibNSv1 for one case and -DLibNS=LibNSv2 for the other."
#define LibNS LibNSv1
#include "my_lib.h" // aqui ele vai usar os LibNS pra renomear as coisas e dai ter tudo exportado corretamente, mas novamente, só da certo com lib estatica.
#undef LibNS

Pra dinamica eu teria que possivelmente criar um wraper que chama essa outra lib e não exportar pra fora.

Para ELF existeo patchelf: https://github.com/NixOS/patchelf

Daria pra renomear os simbolos das libs dinamicas.

// GPU e projeto do HVM do BR lá...

Ele traduz linguagem de auto nivel, massa, daria pra usar direto. Mas tive uma ideia alternativa que tenho que testar antes:

Pega a função e gera o assembly em um padrão unico como no caso o da Intel;
Pega a tabela te dodos os comandos assembly e gera um compativel para a GPU mesmo que multiplas intruções, pois no geral é uma operação logica ou aritimetica
Ignora todos os simbolos que dão pra ignorar como o caso do lock, ou ficar zerando registradores para a proxima operação.
Se ainda sobrar comandos incompativeis essa função não vai poder ser usada na GPU, e gerar um warning, se não, produzir o código CUDA/HIP equivalente.

// Struct format

Já que cada sistema tem uma maneira de alinhar, ver se é possivel padronizar tudo via #pragma pack; Isso facilitaria para fazer um RPC baseado na C struct;
https://stackoverflow.com/questions/3318410/pragma-pack-effect
https://stackoverflow.com/questions/39359918/how-should-pragma-pack8-work
https://www.robopenguins.com/cpp-data-memory/

Provavelmente eu poderia criar uma anotação para dizer quais tipos podem fazer parte do RPC e dai aplicar o pack(4) neles ou um outro tipo especifico apenas pro parser e depois copiar pro tipo normal. Eventualmente não preciso me preocipar com isso pois posso definir que essa serialização via struct vale apenas para a mesma maquina via IPC, logo o alinhamento estaria okay. Para o resto posso ver o lance do wlon.


// wRPC / wQL ...

Binary stream:
<messageSize, 8 bytes><messageType, 4 bytes><messageOperation, 4 bytes><messageData, N-bytes>

messageSize: size de todo a mensagem incluindo o proprio messageSize
messageType: código do tipo da operação, tipicamente um enum
messageOperation: operação da mensagem naquele tipo, tipicamente um enum, similar ao http
messageData: body da mensagem

// em decimal
<16><0><0><> = 16 bytes message, type 0, Operation 0;

Por padrão a mensagem type zero, e operação zero é o info, que retorna o tipo de mensagem que quem recebeu a mensagem suporta, bem como as operações, nesse caso por exemplo:

enum int32 Types {
  wQL
  wlon,
  json,
  wStruct,
  ...
}

enum int64 wQL_ops{
  head,
  get,
  put,
  post,
  query,
  delete
  ....
}

enum char wQL_head_ops{
  none = 0,
  values = 1 << 0,
  names =  1 << 1,
}

Uma head sem informar nada, é sempre interpretado que é o info do servidor logo a reply:

<24><0><0><0>

Onde zero é um inteiro de 64 bits informando que o ele está usando o wQL, já o request:

<16><0><0><11> = 16 bytes message, type 0, Operation 0, data 11;

Nesse caso ele quer saber a lista de operações, seus valores e os nomes em string:

<128+><0><0><[0,1,2,3,4..]["head","get","post",..]>

O ponto aqui ele pode pedir só os valores do enum pra bater com o enum que tem lá, já que em C os nomes em strings não vão fazer parte do cast, nas linguagens com mais instropecção da pra conferir os nomes em string, ou eventualmente só consideralos e ignorar os valores, apesar que seriam esses valores que iriam passar como operação.

Veja que aqui temos apenas uma mensagem por vez, com um tipo, com uma operação por mensagem. Já as sub operações é lidado individualmente com cada tipo de mensagem de acordo com a aplicação, no caso do wQL imagino padronizar a struct de info.

Todos os formatos aceitos pelo RPC deve tentar produzir um info na operação zero que ajuda o outro lado saber como parsear as mensagens, como é especifico de cada tipo, pode ter qualquer formado.

Veja que o head pode ser usado para info de outros contextos. Aqui por padrão isso seria sempre no caso do data/body vazio, ou de forma especifica para cada tipo. No caso do wQL, um head de 1 bytes significa as operações do sistema wQL; Já qualquer head acima de 1 bytes são baseados naquele dado:

<32+><0><0><"................"> = 32+ bytes message, type 0, Operation 0, data 16+ bytes strings;

o head vai traduzir isso como um path para tentar executar o head nele... Mas aqui já estamos entrando na parte muito especifica do wQL não do wRPC

Definir algo em JSON pra tambem poder ser usado em web, wlon para RPC entre apps W, e wStruct para IPC em W (já que o alinhamento aqui não seria uma preocupação já que estamos falando da mesma maquina.); Se o lance do pack funcionar bem da pra fazer um merge do wlon com wStruct e definir apenas um tipo de formato de serialização binaria pra W;


// IPC / RPC

var a = rpc getA()
var b = ipc getB()

#config {
  #rpc.addr = "x.x.x.x"
  #rpc.directCall = false // default
  #ipc.name = "abc"
  #ipc.directCall = true // default
 // #ipc.directCall = { include=["*"], exclude=["secret_*"] // glob ou regex processado em tempo de compilação. Ou similar ao .gitignore também
}

var x = try rpc getX()

RPC e IPC retorna sempre opcionais que podem gerar erros


{
  entry{
    ....
  }
  ipc(req, conn){
    conn.status ...
    cont.retry() ...
    return "xyz"
  }
  rpc(req, conn){
    conn.status ...
    cont.retry() ...
    return new Response<wQL>("xyz")
  }
}

// A sintaxy do entry ainda é discutivel, não sei se vale mais a pena ser um entry maior e deixar as coisas como cli etc pra dentro e dai por o "main" no entry:

entry {
  main(){
    ...
  }
  cli(){
    ../
  }
  ipc(){
    ...
  }
  ...
}

dai entry() seria apenas um shotcut para o entry com main apenas...

Tenho que ver uma sintaxy que posso aproveitar no resto, seria isso o padrão para classes então? já não queria por o fn em metodos de classe mesmo.
`Entry` poderia ser um protocolo e no fim `entry { }` seria um shotcut para:

exports class Entry: ModuleEntry {
  main({...}:MainArgs){
    ...
  }
  ...
}

Faz sentido ser uma classe já que o modulo seria singleton... Mas eu deixaria todas as classes ser do tipo Entry? Se for especial provavelmente vai ser mais facil, mas não custa indagar...

//
AND e OR junto com && e || 
ou ao invés, não vejo motivo para não ser mais idiomatico... Por isso falei lá sobre o SQL-like...

//

import { X , Y , Z } from 'alphabet' with { config, ....}

Melhor deixar o # apenas pra tempo de compilação. Já no import rola de fazer umas coisas via runtime time. Fora que #alguma coisa meio que to só usando pra modificadores estaticos. Meio que no C é tudo pre processador com # então não vejo pq aqui não seria, daria inclusive pra fazer o #include pra funcionar igual no caso do C mesmo.

using vs with?

eu estava esquevendo using antes, mas acho que agora `with` faz mais sentido.

// Comptime Type builders

Em typescript isso é possivel:

type Digit = 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9;
type Hour = `${0 | 1}${Digit}` | `2${0 | 1 | 2 | 3}`;
type Minute = `${0 | 1 | 2 | 3 | 4 | 5}${Digit}`;
type Time = `${Hour}:${Minute}`;

someFunc("23:45") // okay
someFunc("24:11") // erro em compiler time...

Okay, daria pra tentar por isso em W porém porque não fazer tudo via comptime functions também

type Time = type(/^(?:[01][0-9]|2[0-3]):[0-5][0-9](?::[0-5][0-9])?$/)
type Custom = type(isCustom(T))

type seria uma função to tipo comptime que aceita qualquer string, regex ou boleano que valida true/false para o tipo em compiler time. Nos casos onde não é possivel verificar em compiler time, um warning é gerado e a pessoa tem que chamar o <T>.isValid()

fn someFunc(c: Custom){
  guard Costom.isValid(c) else {
    return Error
  }
  ...
}

Caso a função seja usada com literais juntos em algum ponto, o compilador, caso faça inline pode pular o guard check, caso não esteja inline, em alguns casos pode gerar uma versão sem esse check da função, ou implicitamente passar um argumento a mais para dizer que já chegou. A ideia é que qualquer otimização aí seja detalhe de implementação. Uma forma simples de otimizar isso é mover essa função para o lado de quem chamou. Ainda mais caso coloque pra fazer o check automatico.

fn someFunc<typeCheck>(c: Custom){ // provavelmente deveria ser default o typeCheck...
  // guard Costom.isValid(c) else {
  //   return Error
  // } // implicito no call side
  ...
}

Todo tipo poderia ter o isValid btw;
//

///

Okay, pensei de novo, o W ainda é util para algumas coisas, basicamente ao invés de fazer as corrotinas em C daria pra fazer no compilador, etc.. plano inicial. Eu só não vou focar nele no inicio.

// CORRITINAS AGAIN

Okay, a ideia aqui já está mais evoluida. Pensando bem, não precido de lib ou macros nem nada para corrotina, eu preciso ajustar o SPLIT das funções... Basicamente o que expliquei acima...

fn someFunc(){
...
var coisa = await someOther();
}

viraria algo:

fn someFunc(){
...
var idx = addToStaticVarThatControlsThat(args, someFunc_p2)
return Promisse(idx) // O index da função para eu poder dar await...
}

fn someFunc_p2(...){
...
}

Então eu teria um circular ringbuffer em cada thread, onde eu iria criando atomicamente esses objetos no tail ou head, provavelmente no head porque se for processar em background preciso fazer do mais recente pro mais antigo e seguir a ideia do STRUCTURE CONCURRENCE do libdill/swift...

#asyncMaxWait = 10; // Se não der pra fazer uma lista bacana pode ser um array...

Dai eu teria:

async => adiciona as entradas na lista para processamento futuro, obtem uma promise e continua.
await => chama a função async, obtem a promise, adiciona nas entradas na lista para processamento futuro e retorna para função pai se houver;
yield => adiciona as entradas na lista para processamento futuro e retorna a promise; // no yield ao invés do next, o then/await pode receber uma tupla com uma outra promessa...

Chega no final da thread... processa a lista do mais recente ao mais antigo; Outros pontos de parada vão ser criados, mas vão ser processados só após acabar essa lista; Termina e verifica se tem mais coisa na lista, vai até o final, se não chegar nada aguarda um evento para continuar a verificar a lista. 

spawn => adiciona as entradas na lista para processamento futuro, obtem uma Task e continua. Porém adicionar um local a mais para onde a nova thread criada vai guardar o resultado; Essa variavel onde vai ter o result é passada como argumento para a thread chamada, e só ela escreve em algum momento.

fork = mesma coisa do spawn, porém outor modulo.

sync => Tenta executar aquela promessa naquele momento. // porque não wait ao invés de sync ?

await-on-<Promise/Task>: verifica se a Promise/Task foi resolvida, se não adiciona outra entradas na lista para processamento futuro, obtem uma promise para aquele ponto no código e continua.

Promise/Task ... Provavelmente eu só preciso de Promisse, pq posso passar o local onde vai ter o result para o caso do await on task/promise, porém na task da para criar uma forma de cancelar, a não ser que eu faça minhas promessas serem canceladas também, que nesse caso é só tirar da lista... Tenho que ver se preciso de um outro tipo, ou faço tudo com promessa, porém não tava querendo expor isso ao usuario... já que posso chamar qualquer coisa com async, a promessa poderia ter um tipo virtual mesmo, não permitir .then() etc, só await mesmo, dessa forma se é promisse ou task acaba sendo meramente um detalhe.

O problema é como cancelar? outra keyword?

revoke promisseOrTask;

Nesse caso faz um certo sentido pq mesmo cancelar pode demorar um pouco, ter que fazer umas operações atomicas etc. Porém não sei se nesse caso ele devolveria o ponteiro de execução. Só se fosse realmente muito lento cancelar.

revoke => Cancela a promessa/task // se é de forma sincrona ainda não sei, mas provavelmente sim. No caso de promisse certamente, no caso de tenho que pensar já que eu provavelmente chamaria o pthread_cancel; Nesse caso o revoke teria que ser similar a await e retornar o ponteiro de execução.

https://man7.org/linux/man-pages/man3/pthread_cancel.3.html

ah sim:

volatile _Atomic courrotines[#max] = []

// SIGUSR1 e SIGUSR2

SIGUSR1 => Thread se tiver parada, leia a lista de eventos e processe;
SIGUSR2 => Thread se tiver em execução, pare o que está fazendo, leia a lista de eventos e processe;

Eles são usados por outros programas, mas eu realmente queria ver se consigo usa-los aqui... Muito mais leve e mais "posix"... Provavelmente eu teria que ainda permitir que o usuario use esses sinais, mas processa-los também. Já que são só hints, eles não param a aplicação nem nada, só destrava a thread se tiver travada. No caso do SIGUSR2 ele meio que para, porém processar qualquer evento para de uma forma ou outra.

Porém aqui tem: https://man7.org/linux/man-pages/man7/signal.7.html

SIGRTMIN => O que pensei para o SIGUSR1 
SIGRTMAX => O que pensei para o SIGUSR2

No bash o trap usa eles: https://phoenixnap.com/kb/bash-trap-command

Mas dai pelo que vi não da pra chamar via kill, e sim via sigqueue ... o que faz muito sentido. Porém tem o lance da portablidade.... No cosmo tem, as vezes faço polifil ou mudo por completo a libC padrão. Ou uso tecnonicas diferentes de acordo com a libC lincada...

//

// Acho que posso deixar minha ideia do custom parser pra depois... => https://tree-sitter.github.io/

Considerando que eu vou aceitar outras linguagens, vou precisar gerar um tipo de type-check para elas... Eu nem tinha pensado nisso antes, posso resolver o lance da minha linguagem mais simples com os regex lá mas vai cagar tudo quando eu por as outras....

Vou começar com esses doidos aí e ver onde chego. Depois que eu gerar o CST, penso em um code generator para eles, agora no caso posso até brincar de gerar usando typescript mesmo, já que o tree-siter tem bind para node.js (devo usar o bun).... Depois do bostratp eu crio o bind para o W, e quem sabe depois até faço meu parser.

// Sobre os outros códigos. Criar uma lista de imputs validos e outputs validos naquela linguagem que vão ser aceitos no W como uma especie de binding e dai o parser deve ender certinho os tipos de entrada/saida...

Tenho que começar de verdade essa parada mas só pra listar ultimas coisas que provavelmente já falei mas não lembro dos detalhes...

SIMD instruction direto na linguagem ou algo assim pode ser muito bom...

SIMD(....)
AVX(...)

Mesmo para outras instruções que posso garantir que vão ser otimizadas para os conjuntos corretos. Isso deve dar uma "cara" de low level bem legal pra linguagem.

O OpemMP tem muita coisa legal => #pragma omp simd

Pode ser que eu integre por padrão na linguagem, to vendo muita gente usando no contexto do LLM... Porém não to muito confortavel de usar nada além da libc

Criar uma forma de gerenciamento de memoria manual sem "cair" par ao WC ... ou então fazer o WC ser mais que uma linguagem intermediaria, já tava mesmo querendo pular ela mesmo, acho legal ter um código mais perto do C para eu criar os templates e testar as coisas, eventualmente posso fazer o WC ser só um W com a mesma chamada de função do C, declarações etc, só adicionar async, uns imports melhores, strings melhores, eventualmente etc... Dai deixo o ponteiro e o free manual. Posso tentar ir vendo isso na hora de produzir o dialeto completo e o exemplo do restaurante.

Testes...:>> 

Provavavelmente tenho que fazer um auto test para da função, chamando com valores randomicos, posso tentar usar o nome das variaveis para tentar, baseado em um dicionario ou um llm, o que for menor e mais rapido produzir um conjunto de imputs e fazer um teste automatico, depois a pessoa vai lá e arruma. A ideia seria:

Produz função. Fica 1s sem mecher nela um teste é criato em

.w/.autotest/mesmo/path/do/src/mesmoarquivo.test.w

Dai já faço varios testes e tento produzir um tipo de bencharm para colocar na IDE o tempo medio de execução memoria etc.

Esse seria os testes autimatico que iria criar um freeze point junto com o commit; Uma vez comitado nas proximas alterações ele da erro e pergunta pra pessoa se quer substituir.

Porém a cois a mais importante é SEMPRE testar automaticament o exemplo que a pessoa colocar na documentação/comentario do código:

// somefunc
// #arg1 ...blabla.. value: 2
// #arg2 ...blabla.. value: 3
// #result ...blabla.. expected: 5
someFunc(type1: arg1, type2: arg2): type3 {
  ...
}

Algo assim, de forma que nesse caso, se ele falhar para o exemplo, um erro é gerado e a pessoa ou arruma o exemplo ou corrige e função.
Para mais testes fora do automatico, ela pode criar arquivos .test junto do orginal, ou preferencialmente no mesmo nivel da pasta src, em uma pasta tests
Em .w/tests também pode ser caso ela não queria que fique muito visivel e seja dificil da pessoa mexer de forma incinciente. Nessas pastas da pra fazer a regressão, só usar tags ou branch do git como primeiro nivel.


NAMED PARAMENTERS:
// ISSO EU AHCO TOP NO SWIFT

some(isso: 1, comIsso: 2, eIsso: 3)

Acho super valido, e acho que deveria ser padrão... Inclusive para o primeiro argumento, mas eventualmente copio swift na cara dura mesmo. Fica até bom para produzir overload também. Mais facil de ligar com defaults já que assim a ordem não importa muito, e como devo usar struct como argumento deve ser tranquilo traduzir pro C/WC já que agora da pra inicializar structs estaticamente com os nomes. Bem melhor que usar a sintax ({}) direto no W ..., não que não possa, ainda rola de suportar isso para destruir objetos.

// Alinhamento
Posso estar enganado mas acho que alinhar todas as structs pensando no size_t seria o ideal sempre, por padrão. Pessoal vive fazendo isso default em C, eu poderia sempre informar no linter que adicionamentos X bytes de padding padrão; Posso fazer os testes olharem isso; E só preciso avisar que vai quebrar quando a pessoa usar mais que que tinha antes, dai aviso que é um breaking change a partir da versão X se for comunicar com as versões anteriores por causa do ABI, etc... Isso é outro selling point da linguagem.


// Range e multirange


[1..5] ==> 1,2,3,4,5
[1..<5] ==> 1,2,3,4
[1>..5] ==> 2,3,4,5
[1>..<5] ==> 2,3,4
[1>..<5, 2] ==> 3 // para int step 1 é o default
[1>..<5, 3] ==> 4
[1..5, 2] ==> 1,3,5

[-5..-1, 2] ==> -5,-3,-1
[-5..-1, -2] // error...
[-1..-5, -2] ==> -1,-3,-5

[1.0>..<5.0, 1.0] ==> 2.0, 3.0, 4.0 // para float e double step 1.0 é o default, mas pode escrever se quiser..
[1.0..<5.0, 1.5] ==> 1.0, 2.5, 3.5, 4.5

Range Igual ao for initializer + condition + step

for (float x = 1.0; x < 5.0; x += 1.5 ){
  //
}

Dessa forma:

let range = [1.0..<5.0, 1.5] // type: Range<float>

range.forEarch(x => {
    // x tem os mesmos valores esperados em um for loop
})


// MultiRange

Mesma coisa do Range porém com multiplos

var mRange = [1.0..<5.0, 1.5; 1>..5] = (1.0, 2), (2.5, 3), (3.5, 4), (4.5, 5) // type: Range<(float, int)>

Mesma coisa que:

for (float x = 1.0, int a = 2.0; x < 5.0, a <=5; x += 1.5, a++){
  // x = 1.0, a=2 ...
}

Nesse caso:

let mRange = [1.0..<5.0, 1.5; 1>..5, 2] = (1.0, 3), (2.5, 5) // WARNING: multi-range with diferent numbers of steps;

Seria o mesmo caso que no for loop, mas aqui tem um warning, enventualmente posso fazer o mesmo no for-loop mas deixar o warning desabilitado;

Range se torna concreto quando operado por exemplo:

range.contains(value: 3.14) === false
//range.isOn(bound: 3.14) === true // Não sei se faz sentido em W ... Esse seria valido como contains em Swift...

De fato 3.14 está nesse range;

for x in [1..5] {
// Uma forma de fazer o for-in
}

['A'..'Z']
['a'..'z']

Olhar bem como a linguagem Ada faz, não só o Swift... Pq Range da pra ser uma especie de subtipo já que penso também limitar os tipos como em Int<size>
Importante: https://langdev.stackexchange.com/questions/1802/why-dont-many-languages-have-integer-range-types
Limitações no tamanho dos tipos complicam as operações. Mas no caso, acho que eu gostaria que o tipo fosse concreto, o range sempre vira um array concreto de possiblidade, e no caso do tipo só diz os bounds daquele tipo, não que ele contem simultanemente todo aquele range de numeros.

Todo Range é uma especie de LazyArray que é construido quando necessario, e cacheado quando conveniente
A ideia do isOn é mais como uma coisa pra saber se algo está entre o lower e o upperbound mas não se é um valor que será produzido quando o range for contruido. Mas tenho que ver se tem um uso real pra isso, se não deixo só como o conceito de array concreto não inicializado.

De um certo modo acho que o swift "mente" quando cria um range de numeros, ontem um dado numero teoricamnete está dentro dos bounds, mas quando vc intera ele nunca encontra ele.

Em um cenario de range de idades por exemplo, acho até interessante de uma verificação sem dados concretos... Talves valha a pena por uma função pra olhar se está entre o lower e upperbound.

range.contains(onBound: 3.14) === true // provavelmente melhor que o isOn ... posso usar também isOnBounds: 3.14, ou só bound: ..

Idealmente quando:
let range = [1>..<5] // [2..4, 1]

range.lowerBound == 2 // Já resolvo o primeiro e ultimo elemento do Array
range.upperBound == 4 // Esse aqui pode ser que seja mais complicado de resolver direto, mas tenho certeza que crio uma formula tranquila.
range.step == 1 // e já coloco o step aqui...
range.count == 3 // já da pra processar o numero de passos também.

let range2 = [1>..<5, 2]// === [3..3, 0] // ou seria [3..3, 2] ??

range2.lowerBound == 3
range2.upperBound == 3
range2.step == 0 // 2 ?
range2.count == 1

let range3 = [1>..<5, 3]// === [4..4, 0] // ou seria [4..4, 3] ??

range3.lowerBound == 4
range3.upperBound == 4 // Viu que não é tão simples kkkk; Mas importante aqui é fazer isso em tempo de compilação, sempre que der.
range3.step == 0 // 3 ?
range3.count == 1

// Nesse caso aqui continuo considerando que o range é algo concreto, pensar em um upperBound de 5 é errado e de 5-3 também é errado... upperBound é igual ao MaxValue ou LastValue nesse caso. O range é um "produtor" de numeros em W; As outras linguagens permite apenas incremento discreto no range, aqui não vejo porque.

let range4 = [0..<10000]// tipico para interar em array === [0...9999, 1]

let range5 = [0..<10, 3] // [0..9, 3]
let range6 = [1>..<10, 3] // [4..7, 3]

1..5 == [1..5] == 0>..<6

Todo range é resolvido, e o comparador do range compara a equalidade da struct

Range<T>{
 lowerBound: T 
 upperBound: T
 step: T
 count: Int // Aqui talves eu possa deixar como UInt, mas pode ser uma má ideia, tenho que pensar nisso melhor quando pensar nas operações aritimeticas
}

// Operações

1 + 1.0 == ??
(u32)1 + (i32)2 == ??

Se eu sempre usar promoção e nunca downgrande posso até usar UInt/u32 como tipos em count, pq quando fosse fazer alguma aritimetica com i32 por exemplo eu promoveria ambos a i64 por exemplo...

Não pra pra eliminar overflow sempre, mas principalmente na multiplicação é uma boa ideia, incluido:

(i32) 10 * (i32) 10 = (i64)100

Pelo menos se eu for trabalhar com promoção automatica... Preciso pesquisar melhor em outras linguagens, incluindo Rust, Haskell, Fortran, Ada... Python apela, e resolve isso logo com precisão arbitraria;

Pesquisar também sobre https://www.wikiwand.com/en/Unum_(number_format)

Porque float / double são um saco, não sei se vou me aventurar nisso, mas não custa nada...

Tipo, o double pode teoricamente representar numeros até 1.8e308 ; Porém só tem 2^53-1 de valor, pois 11 bits são de exponente. Dai da uns 15 digitos de precisão e o resto é tudo aproximação. Não conheço a fundo a area mas acho que qualquer e308 um absurdo de grande o o MAX_VALUE é totalmente lixo, pq por mais que exista um numero real para ele, na pratica qualquer compiutação pouco abaixo é tudo aproximação e brincadeira de exponente... Sei lá, coisa doida. Até entendo porque a Intel fez o fast-math

https://gcc.gnu.org/wiki/FloatingPointMath

// Zero terminated Array

Arrays que terminam com zero podem ser traduzidos da seguinte forma:

while(*array) {
  // 
}

Olhar direitinho pq é mais facil que um for para interar em diversos casos, não é muito otimizavel mas é muito simples...
// https://twitter.com/falco_girgis/status/1788827993130815636

//

Apenas como nota, muita gente tentando fazer stack C + ImgGUI como stack Web... posso ver como compatibilizar algo assim também.

// Começar logo...

// Print: https://github.com/spevnev/uprintf
// outra lib para verificar: https://github.com/Psteven5/cbinc

Implementação do Defer da pra fazer com um simples "goto cleanup;"

// Alternativa para o @test
@Test("Continents mentioned in videos", arguments: [
  "A Beach",
  "By the Lake",
  "Camping in the Woods"
])
func mentionedContinents(videoName: String) async throws {
  let videoLibrary = try await VideoLibrary()
  let video = try #require(await videoLibrary.video(named: videoName))
  #expect(video.mentionedContinents.count <= 3)
}

// Captura: Adicionar |object| no inicio da função ou clousere

func contador(init: Int) {
  let count = init
  return (add: Int) => { |let c = count, self|
    return (c + add)
  }
}

// Permitir capturar em outra variavel também

|let c = count, self, xxx|


// Captura implicita = |weak object|
// Captura explicita = |object| => |strong object|
// Captura por copia = |const object, let object|
// Captura por referencia = |weak const object, weak let object|

func contador(init: Int) {
  let count = init
  return (add: Int) => {
    return (count ?+ add) // count é Int|Null e o operador ?+ é de condicional add
  }
}
func contador(init: Int) {
  let count = init
  return (add: Int) => { |count|
    return (count + add) // count é Int (copia pois é primitivo, mas uma string por exemplo seria uma referencia)
  }
}
func contador(init: Int) {
  let count = init
  return (add: Int) => { |let count|
    return (count + add) // count é uma copia de count de fora, mas poderia ser algo como let c = count
  }
}
func contador(init: Int) {
  let count = init
  return (add: Int) => { |weak const count|
    return (count ?+ add) // count é Int|Null e const ref
  }
}

func contador(init: Int) {
  let count = init
  return somador(add: Int) { |count| // o nome somador vai se perder no retorno. Mas poderia ser qualquer função.
    return (count + add)
  }
}

func contador(init: Int) {
  let count = init
  somador(add: Int) {
    return (count ?+ add)
  }
  return somador
}


// Optional operators

?+ ?- ?* ?/ ?% ?> ?< ?>= ?<= ?+= ?-=

// Funções (novamente) // aida não sei se uso fn/func

func nomeDaFunc(args: Tipo) {}
fn nomeDaFunc(args: Tipo) {}
func<C> nomeDaFunc(args: Tipo) {}
fn<Bend, .gpu> nomeDaFunc(args: Tipo) {}
fn (args: Tipo) {}

nomeDaFunc(args: Tipo) => {}
(args: Tipo) => {}
func<Zig> (args: Tipo) => {}
fn<.gpu> (args: Tipo) => {}

Como por definição a diferença entre função e clousure é a captura de variavel. E nesse caso permito usar captura mesmo em função, não existe uma ditinção clara de um para outro. Apenas que convencionei chamar de closure quando não se usa func/fn mas sim =>, mesmo nos casos onde também uso fn<> junto com =>

// Função com side efect:

let count = 0

mut action(){
  count++
}

return mut () => {
  count++
}

// Function config:

declare configX = <W, .gpu, .heap>

fn<configX> call() {}

Parece interessante deixar também colocar configs em declarações.

// https://blog.cloudflare.com/sqlite-in-durable-objects/

Output gates me parece uma ideia genial para qualquer forma de sync/async single thread... W precisa disso!

Pode ser algo relacionado ao retorno =>> Se a ultima opreção for um IO em disco por exemplo, seria mais interessante a thread que chama executar esse metodo "sync" com os argumentos que a outra thread produziu. Dessa forma toda thread que chama um modulo se torna um output gate. A outra thread é liberada.

Normal <Th A><Call Func><Schudele B Atomic><T B><Schedule B Call Func><Func Produce Return><Schedule A Return><Th A Continue>
Gate <Th A><Call Func><Return Func><Schudele B Atomic><T B><Schedule B Call Func><Func Produce Return Args><Schedule A Return><Call Return Func><Th A Continue>

Se no retorno ele só precisar copiar um valor em meoria ou algo assim, deixar isso tudo para a Thread que chama, a ideia é fazer tudo até a ultima execução possivel antes de produzir o valor de retorno. No caso de funções de IO ou outras que são "puras" e depedem apenas do argumentos produzidos pela thread de execução. Elas podem por completo serem feitas na thread que chama.
Tenho que ver se isso ajuda, tentar desenhar esse cenario obviamente pois não rola de lotar a thread cliente de calls também.

// Storage

Usar SQLite como forma "padrão" para manipular dados persistentes. Tem o lance do SQLite ser potencialmente 35% mais rapido, economizando 20% do disco, etc. Mas o mais importante mesmo é simplicar o acesso entre threads e ter uma API mais rica. Fazer algo parecido com os durables e criar uma API conveniente em uma table interna _keys algo assim. Eventualmente também separando um outro SQLite para logs persistentes por padrão caso a pessoa assim escolha.

storage.get() ; storage.put(), .list(), .append(), etc...

// https://www.sqlite.org/c3ref/blob_write.html

Para melhor integração com o padrão unix poderia ter um utilitario separado para fazer leitura desse banco como se fosse um aquivo, algo assim. wsql db.sqlite | grep awk | sed etc.


// Early Return

// bad...
func example() {
  if (cond) {
    if (cond2){
      if (cond3){
        ...thing...
        return ...
      }
    }
  }
}

// also bad
func example() {
  if (!cond){
   return "reason1"
  }
  if (!cond2){
   return "reason2"
  }
  if (!cond3){
   return "reason3"
  }
  ...thing...
  return ...
}

// better
func example() {
  guard cond return "reason1"
  guard cond2 return "reason2"
  guard cond3 return "reason3"

  ...thing...
  return ...
}

guard <bool_expression> else {
  return <...> 
}
guard <bool_expression> else return <...> 
guard <bool_expression> return <...>

Seria só uma progressão normal do guard.

// Concorrencia

Single Thread mode: <main_thread>
Multi Thread mode: <main_thread> -> <app_thread> + <n_module_thread>

main_thread => struct _w_shared_internal => named_threads, etc.


## 4. Exemplo de Gramática para Tree-sitter (AST)

Para criar um parser Tree-sitter para W, precisamos definir uma gramática que represente a estrutura da linguagem. Abaixo está um exemplo inicial de gramática em `grammar.js` para Tree-sitter, que cobre os principais constructs de W (declarações de funções, módulos, tipos, etc.).
```
```javascript
module.exports = grammar({
  name: 'w',

  extras: $ => [
    /\s/,
    $.comment,
  ],

  rules: {
    source_file: $ => repeat($._definition),

    _definition: $ => choice(
      $.module_declaration,
      $.function_declaration,
      $.type_declaration,
    ),

    module_declaration: $ => seq(
      'module',
      $.identifier,
      '{',
      repeat($._definition),
      '}'
    ),

    function_declaration: $ => seq(
      'func',
      $.identifier,
      '(',
      optional($.parameters),
      ')',
      optional($.return_type),
      '{',
      repeat($._statement),
      '}'
    ),

    type_declaration: $ => seq(
      'type',
      $.identifier,
      '=',
      $.type_expression
    ),

    parameters: $ => seq(
      $.parameter,
      repeat(seq(',', $.parameter))
    ),

    parameter: $ => seq(
      $.identifier,
      ':',
      $.type_expression
    ),

    return_type: $ => seq(
      '->',
      $.type_expression
    ),

    type_expression: $ => choice(
      $.identifier,
      $.restricted_type
    ),

    restricted_type: $ => seq(
      $.identifier,
      '<',
      repeat($.type_restriction),
      '>'
    ),

    type_restriction: $ => choice(
      seq('maxLength:', $.number),
      seq('pattern:', $.string),
      seq('range:', $.range)
    ),

    range: $ => seq(
      $.number,
      '...',
      $.number
    ),

    _statement: $ => choice(
      $.expression_statement,
      $.return_statement
    ),

    expression_statement: $ => seq(
      $.expression,
      ';'
    ),

    return_statement: $ => seq(
      'return',
      optional($.expression),
      ';'
    ),

    expression: $ => choice(
      $.identifier,
      $.string,
      $.number,
      $.call_expression
    ),

    call_expression: $ => seq(
      $.identifier,
      '(',
      optional($.arguments),
      ')'
    ),

    arguments: $ => seq(
      $.expression,
      repeat(seq(',', $.expression))
    ),

    identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    string: $ => /"([^"\\]|\\.)*"/,

    number: $ => /\d+/,

    comment: $ => /\/\/[^\n]*/,
  }
});
```

### Notas sobre o AST e Tree-sitter

- **Estrutura Intuitiva**: Cada nó no AST corresponde diretamente a um símbolo terminal ou não-terminal na gramática.
- **LR(1) Adherence**: A gramática é projetada para ser eficiente com o algoritmo GLR de Tree-sitter.
- **Próximos Passos**:
  - Adicionar suporte para `async`/`await`, corrotinas e modificadores (`ref`, `cow`, etc.).
  - Implementar validações para tipos restritos e gerenciamento de memória.
  - Testar com exemplos reais de código W.

Para compilar e usar esta gramática, siga as instruções do Tree-sitter:

1. Instale Tree-sitter: `npm install -g tree-sitter-cli`.
2. Crie um diretório para o parser: `tree-sitter init --grammar grammar.js`.
3. Modifique `grammar.js` com o código acima.
4. Compile: `tree-sitter generate`.
5. Teste com código W: `tree-sitter parse exemplo.w`.


//

```

A linguagem W vai ter 2 categorias de tipos:

Primitivos: u8, u16... u64, u128 (do uint8_t até o unsigned __int128), passando pelos inteiros, i32, i64 etc. Basicamente os equivalentes dos tipos em C. Esses tipos vão ter todas as vantagens e probelmas do C como overflow ou underflow. Esses vão sempre viver na stack.
Escalar: TaggedPointer como explicado acima...

Nos primitivos não tem bound check. Nos TaggedPointers sim, já que todas as operações são intermediadas pelos metodos do TaggedPointers, lá também vefica-se os limites. (+% (adição com wrapping), -% (subtração com wrapping), e *% (multiplicação com wrapping))

The smallest non-zero (positive) value that a standard 64 bit float can have is 2^-1074 which is:

0.000......04940656.............47265625
            ^                          ^
            |                          |
            Decimal digit #324         Decimal digit #1074

And for 62 bit float is 2^-1074:

0.000......01976262.............90625000
            ^                       ^
            |                       |
            Decimal digit #324      Decimal digit #1071


//


Tiers de pacotes / modulos.

Tier 1: STD - Não pode ser importado. Contem coisas como String e outras funções uteis. Em caso de colisão os outros pacotes devem usar o nome full: mymodule:String por exemplo.
Tier 2: Util - Coisas como JSON, SHA, etc. Tudo que for um padrão bem definido e muito usado. Importar é opcional pra evitar colisão: W:JSON , W:SHA1 etc....
Tier 3: Comunity - Qualquer coisa da comunidade que pode ser interessante. Até aqui o W vai evitar ao maximo quebrar. Coisas como HTTP Server, S3 conector etc. É necessario importar e não é obrigado a desambiguar o modulo, só usar ( ex: import { http } from "TrustedModule"), isso é. Até esse tier o import ainda é pelo nome canonico do modulo.
Tier 4: Packages - É como as listas "awesome", são projetos de terceiro com zero controle É necessario import e nome full com nome do pacote: import { http } from "RandomProject/ServerModuleName". Caso a linguagem evolua e quebre pacotes aqui, os mantenedores que deverão ajustar.

A ideia é que o W Build consiga ter visibilidade de todos esses pacotes, inclusive IAs no futuro podem selecionar pacotes dessa lista e a intenção é que todas as builds possam ser vefificadas online, ver trend de download possiblidade de revisão de segurança manual e automazada, etc.
É possivel instalar por fora do package manager é claro. Esse seria o Tier 5.

Um pacote pode conter multiplos modulos. Um projeto pode ter varios pacotes, mas não é necessario informar nome do projeto a não ser é claro para desambiguar quando conveniente. Por exmeplo você poderia ter o pacote AWS, com o modulo S3 dentro. Mas se outra pessoa fizer um pacote AWS você tem que identificar com o nome do projeto, Amazon/AWS/S3 por exemplo. O Gerenciador de pacotes não vai deixar cadastrar mais de um projeto com mesmo nome. Porem pacotes e modulos podem se chamar do que bem entender. Os artefatos vão estar publicos por padrão. Mas qualquer pacote do Tier 3-5 pode exigir tokens e outros metodos de autenticação. Podendo até limitar o acesso de outras formas. A ideia é permitir aqui que empresas possam publicas inclusive pacotes privados que só vão poder ser listados com as credenciais corretas.

Qualquer um pode publicar um projeto no Tier 4, apesar que existe uma revisão minima obviamente. A promoção para os tiers acima é de responsabilidade dos mantenedores da linguagem. Na pratica não deve vir a ser complexo entrar no Tier 3. Já Tier 2 e 1 são decisões de longo prazo, pode ser complicado como decidir um feature da propria linguagem.

//

Concluir modelo de gerencia de memoria.

//

Voltar a ver questão da concorrencia e paralelismo já que o modelo de gerencia de memoria está quase todo definido.

Threads/Queues padrões:

Main, Background, Network, IO, UX.

Onde UX é o ideal para interagir com STDIN/STOUT, como printf/scanf....

Cada modulo poderia ter sua "thread/queue" default. Eles só estariam em outra por padrão se for explicito essa declaração. Dessa forma o "myLib" vai executar na thread "myLib" em qualquer operação async nessa lib por padrão.

O que pretendo:

Concorrência com async/await:
Use async para definir funções assíncronas que podem ser suspensas sem bloquear a execução.
Use await para pausar a execução até que uma operação (como IO ou timer) esteja concluída.
Ideal para tarefas que esperam por eventos externos, como leituras de socket ou respostas de API.

Ex:
async function fetchData() {
    let data = await api.get("/data");
    console.log("Dados recebidos:", data);
    return data;
}

Paralelismo com spawn/await:
Use spawn para criar uma tarefa que roda em paralelo, aproveitando múltiplos núcleos da CPU.
Use await para esperar o resultado dessa tarefa, se necessário.
Perfeito para computações pesadas ou operações que não devem interferir na thread principal.
Ex:
async function processHeavyTask() {
    let result = await spawn computeIntensiveJob(1000);
    console.log("Resultado:", result);
}

Você escreve código como se fosse sequencial, mas a linguagem decide automaticamente quando suspender tarefas ou criar threads.
Tarefas assíncronas (async/await) rodam em uma estrutura eficiente (como uma fila de tarefas), enquanto tarefas paralelas (spawn) usam threads reais para desempenho máximo.
Não há necessidade de locks ou sincronização manual – a linguagem abstrai isso para você.
Ex:
async function main() {
    let dataTask = fetchData();           // Concorrência: espera IO
    let computeTask = spawn heavyCalc();  // Paralelismo: roda em outra thread
    let [data, result] = await [dataTask, computeTask];
    console.log("Data:", data, "Result:", result);
}

----

Como de costume não li o que tava acima então só vou colar as ideias aqui:

De forma resumida acho que faz mais sentido a parte da corrotinas ser basicamente uma linked list;

Basta na system thread (main) ter lista de threads criadas para fazer o thread pool, pode ser uma simples VLA com o numero de CPU cores. Quando uma lib/queue/modulo for acionado, a parte da corrotina que atua dentro da thread pode ser uma linked list onde cada node aponta a proxima opreação, guardar tambem a ultima operação (alem da corrente) e deixar essas operações corretamente atomicas. Não sei se citei isso antes mas vale lembrar agora.

---

Mais alguns pensamenos pra finalizar... depois é resumir tudo e começar a fazer.

Se eu tiver que resumir as "inovações" da linguagem, isso é, os pontos mais importantes que se destacam em realação a outras:

- Concorrencia / Paralelismo:
Gosto da simplicidade do modelo async/await aqui proposto. De modo geral, se chamar usarmos da forma simplificada (como em varias linguagens), ao chamar um `await` se forma na mesma thread basta colocar ao fim da fila de corrotinas o que deve ser processado, e se for em outra thread por na fila da outra thread e aguardar... Mas ainda abre espaço pra coisas mais avançadas como chamar diretamente um async (para não esperar), mudar de fila etc... E as filas como o "padrão" ao invés de threads deve simplificar para o usuario e permitir QoS eventualmente. Abstratir as threads deve evitar o abuso das mesmas e permitir um threadpool. Memoria compartilhada entre threads também pode ser simplificada. No geral usa-se atomic, porem da pra fazer bypass do mesmo pois é possivel garantir uma estrutura producer/consumer e provar quando é seguro cada operação, da pra fazer algumas coisas comm lock/mutex/signal, mas seria em poucos casos.

- Gerenciamento de memoria:
Assim como no caso da concorrencia, adotar um padrão simples para o padrão permite gerenciamento de memoria automatico: Quem se torna owner e deve fazer o release; Os argumentos das chamadas também são de resposabilidade de quem chama a não ser que os modificadores de ownership digam diferente. Por exemplo pode ser usado o modificador `copy` (mesmo que seja um ref type) pra forçar a copia dos dados passados e assim o metodo que recebe vai se tornar responsavel por limpar essa copia. Se usar o `ref` garante que é apenas uma referencia ao dado (mesmo que seja um value type), o `transfer` seria mudança completa de ownership (via passagem por referencia). Porem a ideia é usa o padrão com tudo implicito. Value types são passados via copia; classes via referencia com o caller se tornando responsavel; Atribuir uma variavel a uma propriedade de classe/struct é uma operação de `transfer`, etc... Pretendo usa um sistema de tagged pointer na linguagem para maior controle, incluindo ativar um modo "ARC" quando for mais conveniente, esse ARC só seria necessario em properties que são acessadas fora do escopo do ciclo de vida, como propriedades exportadas pelo modulo, etc... A ideia é que a memoria no geral pertença ao modulo, podendo ser gerenciado de forma global no modulo/thread ao invés diretamente na classe, isso permite melhor compartilhamento de memoria entre objetos no mesmo escopo. (ao menos é isso que me lembro do que escrevi antes)

- Switch:
Não se se deixer claro antes mas um Switch statment muito poderoso acho que é/deve ser um diferencial. Poder interar em mais de uma variavel ao mesmo tempo como no swift, e permitir handlers para os cases (passar uma função pra validar uma argumento) pode ser a base de muita aplicação complexa, inclusive penso em fazer todo o parser pra gerar o backend em C dessa forma ao invés do tree-sitter...

  switch (var1,var2,var3) 
   {
      case "Sunday",null,_ : return Labels.Sunday;
      case "Monday",checkTask,_ : return Labels.Monday; //checkTask é uma função
      case (v1,v2,v3) => handleVars(v1,v2,v3): return Labels.custom;
      /* etc... */
   }

Tenho que pensar mais no switch, mas penso nele em um ponto central da linguagem, quero que seja mais flexivel que no swift, se possivel o mais flexivel de todas as linguagens.