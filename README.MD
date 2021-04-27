```typescript

fn({ a: string, b: number}){ // implicit args property
  
  return 0
}

const x = "x"

fn(args:SomeType){
  { a, b } = args
  return false
}

fn(variable:SomeType){
  { a, b } = variable
  return false
}

let count = 0
fn(SomeType){
  ${ a, b}  
  return { count+b, `${x}-${a}` }
}

type FType = { a: string, b: number} 

fn(FType){
  ${ a, b}
  return `${x}-${a}`
}

type RType = { c: number, d: string} 
fName(FType):RType{
  ${ a, b}
  
  return false // error...
}

type FType2 = { a: string?, b = 0 } 

fn(FType2){
  ${ a ?? 'default', b==0 ? 2, c > 1 ?: 0 }
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

// namespaces/modulos são globais e podem ser referenciados por seu nome canonico em string nos imports.

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
// É possivel refereciar o nome de qualquer modulo como se fosse uma variavel glocal sem importar nada.
// Namespace é apenas uma questão de organização, mas no final é um conjunto de modulos.

namespace > module > arquivo

// deve-se importar sempre o de maior grau se um modulo faz parte de um namespace deve-se importar o namespace primeiro

// um executavel por ter varios entry points, cada um pode ser um modulo separado.
// quando se compilado um arquivo, o entry point deverá ser apontado (um arquivo/modulo/namespace) ele será chamdo por padrão,
// porém se chamar o main/default de cada modulo depois

process.module // modulo corrente em execução.

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

Qualquer outro modulo deverá ser referenciado na build e acessado no codigo em um bind nomeado no processo de build. O bind será uma variavel global ou uma string com mesmo nome.

A diferença da string e da variavel global é que a string precisa ser processa dinamicante pra usar, ou seja irá ter feito o carregamento/inicialização do modulo


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
module{
  a: {}
}

Tudo é na verdade referenciado na struct pre alocada do modulo
c max

//

Objetos são sempre flat, ou seja, como structs. Funções do super são renomeadas na hora de compilar, e ou é feito um marshal para a função quando não for externo como o caso de uma lib.
O sistema deve manter o track de quanto de memoria cada objeto pode ocupar no maximo. e alertar se passar o maximo do modulo em tempo de compilação.

unica diferença da strucs do objeto é a questão da referencia; implicitamente todo objeto tem um paramento chamado _isa

objetos e strucs podem ser extendidas, fazer herança e podem ter construtor; Caso não tenha construitor, nesse caso não precisa usar `new`;

memorias no heap em funções são na verdade parte da estrutura do modulo:

fn(SomeType){
  ${ a, b}  
  let someBigVar = new SomeTypeToHeap(a,b)
  // something with someBigVar
  return someBigVar
}

module.fn.someBigVar = new SomeTypeToHeap(a,b)

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

No linux poderia se manipular a stack como parte do module.memory, isso pode gerar flexbilidade. Pois é interessante do sistema criar a propria stack/ringbuffer para o resto.


```