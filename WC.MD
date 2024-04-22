```typescript

Clang extension to C

Superset do C similar ao Objective-C;

Linguagem intermediaria para a linguagem W; Conter algumas adições com esse unico objetivo, mesmo que pouco pratico para o WC;

Adicionar coisas basicas como o tipo `string` totalmente compativel com o *char[];

Os tipos adicionais do WC devem ser automagicamente gerenciados pelo compilar (safety as possible); Adicionar restrições na interoperabilidade quando necessario;

//
Falar sobre o runtime: Script -> CLI -> Daemon -> Server

// Refletir sobre https://blog.cloudflare.com/assembly-within-bpf-tail-calls-on-x86-and-arm/ | Seria possivel não permitir recursividade no nosso sistema que não fosse algum tipo de otimização de tail call?
// Isso provavelmente seria feito só no W, mas se pensarmos na questão de modulo apenas, toda função de um modulo poderia ser apenas um jump, e dessa forma toda a stack (geralmente 8 megas) seria compartilhada no modulo inteiro.... Se o WC usar modulos (na parte do WC não na parte C), poderia dar certo também....
Entretando faz sentido com algumas ideias antigas do W de poder fixar o tamanho maximo de um modulo... Não sei como seria isso no linux, mas qualquer coisa poderia ser algo apenas na heap... (uma unica heap por modulo)
Nesse caso a linguagem seria stackless... dessa forma: https://www.wikiwand.com/en/Continuation-passing_style
// also: https://stackoverflow.com/questions/796211/what-does-it-really-mean-that-a-programming-language-is-stackless

Also inovador poderia ser possivel escolher stackless ou stackfull.... Poderia ter algo automatico pra decidir por modulo, ou deixar a pessoa fazer uma anotação:

define stackless // W ou WC default é usar os ambos como o Rust
define stackfull // W ou WC

Poderia ter a questão de sempre ter que deixar explicito o tamanho das variaveis (já falei isso no W), mas deixar ele pegar o tamanho do objeto de inicialização como tamanho maximo

string c = "long string" // max size X será baseado na inicialização e C será do tipo string_size (tamanho exato com padding?)

dai poderiamos colocar dados menos mas nunca maior sem gerar erro...

dados dinamicos precisam de declaração previa.

//

Faz sentido cada modulor ser singlecode e quebrar tudo em processos?

//

Ter uma boa implementação do FFT (The Most Important Algorithm Of All Time) e colocar isso como vantagem por estar na stdlib

//

Fork do CLang deve facilitar! Só ir adicionando coisas novas...

// Isso é foda: https://justine.lol/cosmopolitan


Uma coisa importante e interessante salientar é que C não tem uma ABI... É tudo meio que dependente do sistema e da CPU... É uma linguagem muito voltada a CPU e joga quase todo o trabalhado de compatibilização ao compilador. Aqui por exemplo tem uma considração em relação a isso: https://stackoverflow.com/questions/4489012/does-c-have-a-standard-abi

E isso afeta muitas caracteristicas da linguagem como por exemplo: https://stackoverflow.com/questions/161788/are-there-any-downsides-to-passing-structs-by-value-in-c-rather-than-passing-a

Uma linguagem que venha a "substituir C" definivamente precisa dar respostas claras a essas inconsitencias. E gerar o melhor código para cada sistema/arquitura. Veja, é totalmente compreensivel C ser como é. Ele foi criado em paralelo a tudo na infromatica, enquanto varias arquiteuras estavam surgingo, sistemas, etc. Não existia internet, tudo foi feito baseado no melhor conheciemento de cada um na ocasião. Porém hoje não só temos mais informação difundida, não existe o mesmo nivel competição para dizer o que é e como deve ser as chamadas de sistema, etc. Uma abstração em W deve seguir ao maximo o "recomendado" para cada sistema, mas tentar produzir uma visão top level mais unificada possivel.

Para W poder interperar com diversas bibiotecas, é necessario formalizar bem como deve ser essa interface. Principalmente se criarmos uma forma automatica de simplificar as chamadas em bibliotecas em C como faz o Swift com Objective.

//

Como da para ver claramente no caso do struc.c; Ter as variaveis "estaticas/constantes" dentro do escopo permite (em C) otimizar a construção do objeto e ignorar por completo as strucs. (https://godbolt.org/ - fazer testes também com  -fwhole-program -Ofast -flto -fPIC -fno-fast-math etc)


De modo geral, sempre gerar artefatos com "-fwhole-program" (e outras otimizações similares), pois os modulos devem ser importados considerando que são libs... Logo códigos intermediarios não valem e o compilador pode otimizar por completo o executavel e a lib. // https://stackoverflow.com/questions/73526838/for-a-function-that-takes-a-const-struct-does-the-compiler-not-optimize-the-fun

Pelo que vi o GCC sempre produz os melhores resultados (e suporta o -fwhole-program) o que faz sentido para os modulos. O binario é menor, logo menos memoria e melhor para o pkg. I projeto do Clang me parece muito mais facil de entender, e o Zig CC usa ele...

Essa otimização pode ser uma mão na roda: https://gcc.gnu.org/onlinedocs/gcc-4.8.5/gccint/WHOPR.html

O mais similar com o clang é: -O3 -fwhole-program-vtables -flto -march=native  -std=gnu23 
O analogo em gcc é: -O3 -fwhole-program -flto -std=gnu2x  -march=native

Nesse caso o clang é até melhor, por pouco, mas o -fwhole-program pode ser executado sem o -flto

Por padrão os códigos são criados via static library, mas podem ser usados como dynamic. Idealmente os dois são exportados.

// Já ia me esquecendo, coisa antiga mas implementação basica de corrotina em C: https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
// Aqui também em produção: https://github.com/github/putty/blob/master/sshcr.h

// C VERSION

Considerando que seria uma linguagem nova, nada melhor que ser C23 para frente. Pouco motivo para suportar versões anteriores a não ser no caso das libs, mas como vamos usar artefatos compilados como modulos externos não vejo tanto problema.

//https://en.wikipedia.org/wiki/C23_(C_standard_revision)

Sobre String novamente: https://stackoverflow.com/questions/58878651/what-is-the-printf-formatting-character-for-char8-t

printf("Hello %s\n", u8"Jöel");
printf("%s\n", u8"É Você!");

Provavelmente faz mais sentido eu trabalhar só com UTF8 e converter tudo, que não for UTF-8 usando o `iconv` 
Além disso existe o wchar_t no C hoje em dia, incluindo o wprintf, etc...

O iconv() resolve a questão do wchar_t e qualquer outro input padrão do sistema, já que o tipos "w" vão ser o tipos dos argumentos passados. Dai só sobra abrir arquivos, sockets, etc... todo resto né... kkk

Claro também considerar:
    SetConsoleOutputCP(CP_UTF8); // windows
    setlocale(LC_ALL, ".UTF8"); // setlocale(LC_ALL, "")

Consultar: https://dev.to/rdentato/utf-8-strings-in-c-1-3-42a4

"VS Code's default encoding is UTF-8 without BOM."


int u8strlen(const char *s)
{
  int len=0;
  while (*s) {
    if ((*s & 0xC0) != 0x80) len++ ;
    s++;
  }
  return len;
}

Also: -fexec-charset=UTF-8 -finput-charset=UTF-8 (gcc)

Idealmente o source code tem que ser sempre UTF8, no processo de build/pack seria interessante chegar isso. Arquivos que não são UTF-8 podem ser um problema por causa dos literais. Podemos forçar os arquivos fontes serem UTF-8? Pensar melhor nisso depois.
Aqui é um bom inicio: https://stackoverflow.com/questions/2223882/whats-the-difference-between-utf-8-and-utf-8-with-bom

// GENERICS

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

//

Padronização de ponteiros e retorno:

Ao invés de:

void* foo(args...)

struct slice* foo(struct args)

Onde

struct slice<T> {
    T *base;
    intptr_t len;
}

struct args<T> {
    arg1;
    arg2
    ....
}

Dessa forma sempre posso usar ponteiro de ponteiro no caso para fazer as alocações automaticas e o free e manter uma lista de referencias que devem ser zeradas, ( week )


struct pointer<T> {
    T *base;
    intptr_t len;
    weak [];
}

Como já demostrado em C (clang e gcc) temos o VRO (Value return optimization) onde podemos claramente usar uma struct para retorno dos dados, mesmo que só um ponteiro, e ele otimizar a chamada de ponteiro direto caso naquele caso eu apenas o use. Porém nos casos onde é necessario fazer os checks (verificar se o ponteiro é nulo, bound check etc) temos esses dados.

Se essas estruturas de retorno, assim como de argumento forem geradas automaticamente na tradução de W para WC, aqui só preciso no maximo definir um padrão legal na wc.h para depois promover direto pro compialador no futuro.


// PURE and CONST

__attribute__((const)) int f() { // padrão para os contrutores em W
    /* ... */
    return 4;
}

__attribute__((pure)) int f() { // padrão para as funções em W, nessario usar mut para remover isso.
    /* ... */
    return 4;
}

via sugar:


const int f(const int x) {
    /* ... */
    return x + 4;
}

pure int f(const int x) {
    /* ... */
    return 4;
}

Essa thread aqui ajuda a ver a melhor forma de declarar: https://stackoverflow.com/questions/9441262/function-pointer-to-attribute-const-function

Pelos meus testes dependendo o ele aceita modificar o escopo com const dependendo da forma que declara.

Tem isso também: __attribute__ ((access (read_only, 1)))
Aqui: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#Common-Function-Attributes

Eventualmente esses atributos podem não funcionar de qualquer forma, então tentar resolver isso eu mesmo. Provavemente não em WC, mas na tradução do W para WC... A não ser que deixe o WC como algo a mais que o C com sugar...

//

Check: file1.c

// LINTER e IDE
// https://medium.com/ringcentral-developers/build-a-linter-extension-for-visual-studio-code-368a65a95545
// https://code.visualstudio.com/api/language-extensions/syntax-highlight-guide
// https://github.com/jeff-hykin/better-cpp-syntax/
// https://github.com/ziglang/vscode-zig

Começar com VS Code...

// C23: https://gustedt.gitlabpages.inria.fr/c23-library/


// SYCL ??

// Estruturas de dados:

WC vai ter algumas estruturas de dados basicas para complementar a C library.

- Array
- Dic
- Queue
- Set
- List // linked?
...

Shots:

array[removeIndex] = array[--logicalSize]; // Forma simples de remover um elemento de um array não ordenado.

Dic:

key_array + value_array // value array é não ordenado sempre insere no final, sempre remove fazendo pegando o ultimo e colocando no buraco.

etc... isso aqui é todo um mundo separado, tem tanta, tanta solução pronta em C, eventualmente posso referenciar algumas, mas no geral a ideia é reimplementar em WC porque depois farei o mesmo em W.

Aqui um exemplo: https://github.com/tezc/sc
https://troydhanson.github.io/uthash/ // <- Bem legal!, parece rapido e leve.

o XXHASH é um bom candidado de lib padrão de hash para a linguagem. Esse do sc por exemplo usa o mumurhash o que é muito, mas muito mais lento que o xxh3

De qualquer forma seguir a lista da MS: https://learn.microsoft.com/pt-br/dotnet/standard/collections/
E claro alguma implementação de uma b-tree // https://en.wikipedia.org/wiki/B+_tree

Isso deve ajudar a ter uma boa ideia do que preciso fazer "no minimo"... Eles possuem a complexidade lá, o que é maneiro de informar o usuario já na descrição, se possivel até no VSCode.

Tenho que pensar melhor depois mas um tipo de dado que queria testar um dia é o "ShardedList", onde é basicamente uma linked list porém cada nó pode conter um array de N elementos. Ele só quebra em multiplas listas quando existe CRUD no meio do array. Ao remover um elemento do meio por exemplo e adicionar outros 3 poderia apenas quebrar o array, apontar para o outro de 3, e apontar o fim desse outro para o inicio do resto do segmento. Seria basicamente uma implementação de memoria virtual em user space, sem acesso direto a memoria é claro, apenas navegando no index da lista. Daria até pra computar facilmente os saltos se tivermos sempre o index adicional no node a cada X paginas ou sempre apontando para o meio. Ou deixando as paginas em tamamos maximos bem definidos, daria facilmente pra saber quantas paginas preciso pular. Daria pra fazer um binary search também etc.


// print
printf // https://git.musl-libc.org/cgit/musl/tree/src/stdio/printf.c

Seja como for o WC deve ter uma versão safe do printf, mesmo usando o glib-c no geral, vejo o WC usando um pouco de cada lib diferente e criando sua propria no final.

// Colisão de simbolos:

https://stackoverflow.com/questions/6940384/how-to-deal-with-symbol-collisions-between-statically-linked-libraries

// Sobre o lance de por as threads e outras configs no global:

https://stackoverflow.com/questions/3004318/dynamic-loaded-libraries-and-shared-global-symbols

Melhor ir passando tudo pra lib, pq não botei muita fé nesse lance de deixar as libs ler as variaveis globais. ( via -rdynamic )
Passando como argumento posso me certificar que ele será const, e posso criar os metodos certinhos pra criar novas threads.

// INSPECTION POINT VS BREAK POINT

Pelo que vi o GCD usa o ptrace para controlar e inspecionar... Bem, eu acho que posso fazer um pouco diferente...

A ideia é abrir um socket simples, adicionar e "printar" tudo pra lá em cada ponto de inspeção. Sim, nesse caso cada vez que adiocionar um ponto teria que recompilar, mas a lado bom é que não precisa pausar o código, daria para por outras metricas mais realistas como tempo tempo de clock(). Tambem poderia deixar algumas coisas no app de release para ajudar a corrigir bugs em producao. Incluindo gravação de logs. Ou guardar em um sqlite em WAL mode, ao invés de abrir um socket. Ou vou um pooling ou eu faço funcionar um trigger de gravação entre processos, também posso ao invés do pooling usar o SO pra notificar alterações, mas acho que no windows vai ficar uma bosta. Um pooling de 50ms deve dar conta do recado.

No fim a unica diferença entre o release e o debug é a quatindade de data points e quantos dados são coletados em cada um. Os inspections points funcionam mesmo se remover os simbolos, o que é bacana também.

// C-Extensions?

Eu deveria considerar por padrão o as extenções do gnu e clang?
https://gcc.gnu.org/onlinedocs/gcc/C-Extensions.html
https://clang.llvm.org/docs/LanguageExtensions.html

// Block: https://clang.llvm.org/docs/BlockLanguageSpec.html

// MThread Signal: https://codereview.stackexchange.com/questions/284179/proper-implementation-of-signal-handler-and-multithreading-pthread

Dependendo posso apenas guardar o que preciso para a thread consumir em uma queue (OptimistAtomicQueue ?? ) e via sinal eu a thread passa a ler essa fila, ao invés de um pooling. Eventualmente usar um pthread_cond_wait, ou outra forma segura de disparar sinais para a thread.


// NOTA!! WC Pode não ter nenhuma interface publica. Ser apenas a linguagem intermediaria do W... muito mais facil não ter que se preocupar com o WC... Só o W já vai dar muito trabalho. Porém acho legal deixar "reservado" essa possiblidade.

// Promisses? : https://gist.github.com/Geal/8f85e02561d101decf9a


Bloquear sinais? https://www.gnu.org/software/libc/manual/html_node/Blocking-Signals.html

Teve um post dizendo que o ideal é sempre bloquear todos os sinais de todas as threads para fazer um multhread correto. Deixando é claro os poucos necessarios. Nesse post do gnu também fala algo sobre isso.

//

Importante também na hora que for falar da linguagem dizer que o objetivo não é forçar nenhum padrão, e assim como no C, deixar a pessoa livre para produzir tudo. Que o fato de por padrão ter facilitadores, não significa que não pode ser feito tudo como no C; Que nas primeiras versões o foco seria na parte gerenciada da linguagem, como gerenciamento automatico de memoria, mas que antes da versão 1.0 teriamos um modelo totalmente manual também de gerenciamento de memoria e threads, a area onde tem a maior automação em W.

A linguagem está sendo criada de forma top down, bottom up...

// MACROS e C import.

Na primeira versão não teriamos suporte a importar código em C diretamente principalmente por causa dos macros, que vai demorar um pouco pra suportar também. Por isso modulos compilados.


// C Caller:

int functionCall(void* arg1, void* arg2)

becomes:

fn functionCall(ref void arg1, ref void arg2): int

//

Pensando bem não to gostando mais de usar o fn para funções... Poderia ser opcional mesmo:

functionCall(...): int

ou igual C mesmo...

int functionCall(...)

Tem tempo que não paro pra pensar na sintax, comecei a usar o fn e não parei, mas nunca realmente gostei.

Acho que no fim tenho que primeiro produzir o cheatsheet da linguagem inteira...

https://quickref.me/c

// Planos futuros:

{
    #computeUnit: .cpu | .gpu
}
...
functionCall<.gpu>(...)
...
let<.gpu> frame = ...

Daria pra fazer usando o AMD HIP e/ou OpenMP....

// Compile flags:

O Phononix parece ter uma lista do carvalho: https://www.phoronix.com/review/intel-meteorlake-gcc-clang
As do Clang dando pra usar no ZigCC para cada uma das 3 principais plataformas okay.
