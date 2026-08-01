# Inventário de referências históricas de Y

> **Status: Pesquisa.** Este é um catálogo de proveniência, não uma decisão de
> linguagem, arquitetura ou implementação. Uma citação histórica não endossa o
> seu conteúdo nem implica adoção em W.

## Escopo, método e reprodução

Inventário produzido a partir do commit histórico
`2676d5602038dcda4d3d0127209f809981cb60f1`, usando-o como base estável mesmo
com `Y/` restaurada como arquivo histórico externo a W, mais as notas locais
existentes `TK/wQL.MD`, `TK/RestQL.MD`, `TK/V6.MD`,
`TK/tree_string.md` e `TK/package.MD`.

Método determinístico: listar `Y/` com
`git ls-tree -r --name-only 2676d5602038dcda4d3d0127209f809981cb60f1 -- Y/`;
ler cada objeto com `git show COMMIT:CAMINHO`; ler as cinco notas TK quando
existentes; extrair cada token de esquema HTTP ou HTTPS até espaço, aspas,
`<`, `]` ou `)`; retirar pontuação terminal inequívoca; agrupar por URL e
conservar `arquivo:linha`. O mesmo procedimento pode ser reproduzido sem
modificar a árvore de trabalho. Para contagens brutas, links Markdown contendo
URL tanto no rótulo quanto no destino contam duas ocorrências.

- Fontes examinadas: **56** (51 sob `Y/`, 5 sob `TK/`).
- Fontes com URL: **6**, todas históricas sob `Y/`; as cinco notas TK não
  continham token HTTP(S) extraível.
- Ocorrências HTTP(S) normalizadas: **254**; URLs históricos distintos: **133**.
- Acréscimos atuais da revisão: **7 ocorrências**, **5 URLs distintos novos**;
  outros dois já estavam no corpus histórico.
- Total catalogado abaixo: **138 URLs distintos**.

Legenda: **[H]** referência histórica; **[A]** referência atual adicionada nesta
revisão; **[D]** duplicado (já havia URL idêntica); **[L]** link não validado
ou truncado. Não foi feita navegação em massa: `[L]` não significa que um link
normal esteja quebrado, apenas que sua forma/proveniência não permite validá-lo
com segurança neste inventário.

Os ponteiros de fonte são também o contexto curto: `WIP` são notas de projeto;
`WC` são notas de linguagem/implementação; `build` são notas de construção;
`referencias` é uma lista histórica curada; `README` e `C/btreemap.c` são
exemplos. Contextos temáticos abaixo são paráfrases, não transcrições.

## Concorrência, corrotinas e execução

- [H] [protothreads](http://dunkels.com/adam/pt/index.html) — protothreads; `Y/WIP.MD:645`, `Y/_w_/referencias.md:44`.
- [H] [protothreads-cpp](https://github.com/benhoyt/protothreads-cpp) — protothreads; `Y/WIP.MD:882`.
- [H] [liburcu](http://liburcu.org/) — RCU; `Y/WIP.MD:2469`, `Y/_w_/referencias.md:50`.
- [H] [BPF tail calls](https://blog.cloudflare.com/assembly-within-bpf-tail-calls-on-x86-and-arm/) — execução/restrição de recursão; `Y/_w_/WC.MD:16`, `Y/_w_/referencias.md:5`.
- [H] [branch predictor](https://blog.cloudflare.com/branch-predictor/) — desempenho; `Y/WIP.MD:505`.
- [H] [RCU como controle](https://concurrencyfreaks.blogspot.com/2019/10/is-rcu-generic-concurrency-control.html) — RCU; `Y/WIP.MD:2495,2526`.
- [H] [fila lock-free](https://github.com/cameron314/concurrentqueue) — filas concorrentes; `Y/WIP.MD:2636`, `Y/_w_/referencias.md:51`.
- [H] [reader-writer queue](https://github.com/cameron314/readerwriterqueue) — filas concorrentes; `Y/WIP.MD:2637`, `Y/_w_/referencias.md:52`.
- [H] [Concurrency Kit](https://github.com/concurrencykit/ck) — primitivas concorrentes; `Y/WIP.MD:721`.
- [H] [nsync](https://github.com/google/nsync) — sincronização; `Y/WIP.MD:2487`.
- [H] [libfiber](https://github.com/iqiyi/libfiber) — fibras; `Y/WIP.MD:648`, `Y/_w_/referencias.md:47`.
- [H] [protothread multicore](https://github.com/LarryRuane/protothread-multicore) — protothreads; `Y/WIP.MD:646`, `Y/_w_/referencias.md:45`.
- [H] [atomic_queue](https://github.com/max0x7ba/atomic_queue) — filas atômicas; `Y/WIP.MD:2639`, `Y/_w_/referencias.md:54`.
- [H] [nng](https://github.com/nanomsg/nng) — mensagens/concorrência; `Y/WIP.MD:647`, `Y/_w_/referencias.md:46`.
- [H] [ctp](https://github.com/nicowilliams/ctp) — concorrência; `Y/WIP.MD:2469`, `Y/_w_/referencias.md:49`.
- [H] [corrotinas de PuTTY](https://github.com/github/putty/blob/master/sshcr.h) — técnica de corrotina; `Y/_w_/referencias.md:40`.
- [L] [URL concatenada de PuTTY/corrotinas](https://github.com/github/phttps://www.chiark.greenend.org.uk/~sgtatham/coroutines.htmlutty/blob/master/sshcr.h) — token malformado; `Y/_w_/WC.MD:77`.
- [H] [C11 e concorrência](https://lumian2015.github.io/lockFreeProgramming/c11-features-in-currency.html) — memória/atômicos; `Y/WIP.MD:720`.
- [H] [perfbook HTML](https://mirrors.edge.kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook.html) — RCU; `Y/WIP.MD:2494`.
- [H] [perfbook PDF](https://mirrors.edge.kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook-1c.2023.06.11a.pdf) — RCU; `Y/WIP.MD:2498`.
- [H] [benchmarks atomic_queue](https://max0x7ba.github.io/atomic_queue/html/benchmarks.html) — filas; `Y/WIP.MD:2641`, `Y/_w_/referencias.md:55`.
- [H] [fila lock-free, artigo](https://moodycamel.com/blog/2014/a-fast-general-purpose-lock-free-queue-for-c++#benchmarks) — filas; `Y/WIP.MD:2638`, `Y/_w_/referencias.md:53`.
- [H] [thread pool em C](https://nachtimwald.com/2019/04/12/thread-pool-in-c/) — pools; `Y/WIP.MD:2635`, `Y/_w_/referencias.md:29`.
- [H] [lock-free e threads](https://nullprogram.com/blog/2014/09/02/) — concorrência; `Y/WIP.MD:2493`.
- [H] [RCU versus RW-lock](https://stackoverflow.com/questions/24598636/difference-between-read-copy-update-and-reader-writer-lock) — sincronização; `Y/WIP.MD:2507`.
- [H] [mutex pthread](https://stackoverflow.com/questions/59979509/why-would-pthread-mutex-lock-hang-if-pthread-mutex-unlock-has-been-called) — mutex; `Y/WIP.MD:2479`.
- [H] [stackless](https://stackoverflow.com/questions/796211/what-does-it-really-mean-that-a-programming-language-is-stackless) — modelo de corrotina; `Y/_w_/WC.MD:20`, `Y/_w_/referencias.md:14`.
- [H] [actor queue](https://trycombine.com/posts/performance-actor-queue-lock-benchmark/) — atores/filas; `Y/WIP.MD:2483`.
- [H] [corrotinas de Simon Tatham](https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html) — corrotinas; `Y/WIP.MD:644`, `Y/_w_/WC.MD:76`, `Y/_w_/referencias.md:18`.
- [H] [sinais bloqueáveis](https://www.gnu.org/software/libc/manual/html_node/Blocking-Signals.html) — sinais; `Y/_w_/WC.MD:302`, `Y/_w_/referencias.md:28`.
- [H] [signal handler multithread](https://codereview.stackexchange.com/questions/284179/proper-implementation-of-signal-handler-and-multithreading-pthread) — sinais; `Y/_w_/WC.MD:292`, `Y/_w_/referencias.md:27`.
- [H] [pthread_cancel](https://man7.org/linux/man-pages/man3/pthread_cancel.3.html) — cancelamento; `Y/WIP.MD:3319`, `Y/_w_/referencias.md:20`.
- [H] [signal(7)](https://man7.org/linux/man-pages/man7/signal.7.html) — sinais; `Y/WIP.MD:3332`, `Y/_w_/referencias.md:21`.
- [H] [bash trap](https://phoenixnap.com/kb/bash-trap-command) — sinais/shell; `Y/WIP.MD:3337`, `Y/_w_/referencias.md:7`.

## Linguagem, compiladores, sintaxe e ferramentas

- [H] [Blocos do Clang](https://clang.llvm.org/docs/BlockLanguageSpec.html) — extensões C; `Y/_w_/WC.MD:290`, `Y/_w_/referencias.md:67`.
- [H] [extensões de linguagem Clang](https://clang.llvm.org/docs/LanguageExtensions.html) — extensões C; `Y/_w_/WC.MD:288`, `Y/_w_/referencias.md:66`.
- [H] [syntax highlight do VS Code](https://code.visualstudio.com/api/language-extensions/syntax-highlight-guide) — editor; `Y/_w_/WC.MD:215`, `Y/_w_/referencias.md:71`.
- [H] [TypeScript 4.9](https://devblogs.microsoft.com/typescript/announcing-typescript-4-9-beta/) — `satisfies`; `Y/WIP.MD:1217`.
- [H] [operadores Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/advancedoperators/) — operadores; `Y/WIP.MD:2288`.
- [H] [protocolos Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/protocols/) — protocolos; `Y/WIP.MD:3023`.
- [H] [suporte de compiladores C++](https://en.cppreference.com/w/cpp/compiler_support) — compatibilidade; `Y/WIP.MD:886`.
- [H] [WHOPR](https://gcc.gnu.org/onlinedocs/gcc-4.8.5/gccint/WHOPR.html) — otimização/link-time; `Y/_w_/WC.MD:67`, `Y/_w_/referencias.md:17`.
- [H] [extensões GCC](https://gcc.gnu.org/onlinedocs/gcc/C-Extensions.html) — extensões C; `Y/_w_/WC.MD:287`, `Y/_w_/referencias.md:65`.
- [H] [atributos GCC](https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#Common-Function-Attributes) — atributos de função; `Y/_w_/WC.MD:205`.
- [H] [atributos C23](https://gustedt.gitlabpages.inria.fr/c23-library/) — C23; `Y/_w_/WC.MD:221`, `Y/_w_/referencias.md:41`.
- [H] [Bend](https://github.com/HigherOrderCO/Bend) — linguagem; `Y/_w_/WC.MD:353`.
- [H] [EnumKit](https://github.com/gringoireDM/EnumKit.git) — pacote Swift; `Y/WIP.MD:920`.
- [H] [Swift Package Manager](https://github.com/apple/swift-package-manager/blob/main/Sources/PackageDescription/Target.swift) — pacotes; `Y/WIP.MD:949`.
- [H] [better-cpp-syntax](https://github.com/jeff-hykin/better-cpp-syntax/) — sintaxe de editor; `Y/_w_/WC.MD:216`, `Y/_w_/referencias.md:72`.
- [H] [vscode-zig](https://github.com/ziglang/vscode-zig) — editor; `Y/_w_/WC.MD:217`, `Y/_w_/referencias.md:73`.
- [H] [Godbolt](https://godbolt.org/) — inspeção de compilação; `Y/_w_/WC.MD:60`.
- [H] [C quick reference](https://quickref.me/c) — consulta C; `Y/_w_/WC.MD:339`.
- [H] [Tree-sitter](https://tree-sitter.github.io/) — parser/gramática; `Y/WIP.MD:3343`, `Y/_w_/build.md:117`.
- [H] [linter VS Code](https://medium.com/ringcentral-developers/build-a-linter-extension-for-visual-studio-code-368a65a95545) — extensão; `Y/_w_/WC.MD:214`, `Y/_w_/referencias.md:70`.
- [H] [C23 Wikipedia](https://en.wikipedia.org/wiki/C23_(C_standard_revision) — padrão C; `Y/_w_/WC.MD:83`.
- [L] [Unum Wikiwand](https://www.wikiwand.com/en/Unum_(number_format) — URL truncada no texto; `Y/WIP.MD:3535`.
- [H] [CPS Wikiwand](https://www.wikiwand.com/en/Continuation-passing_style) — continuations; `Y/_w_/WC.MD:19`, `Y/_w_/referencias.md:6`.
- [H] [Duff's device](https://www.wikiwand.com/en/Duff%27s_device) — técnica C; `Y/WIP.MD:2292`, `Y/_w_/referencias.md:36`.

## Memória, ABI, bibliotecas de sistema e portabilidade

- [H] [UTF-8 em C](https://dev.to/rdentato/utf-8-strings-in-c-1-3-42a4) — strings; `Y/_w_/WC.MD:99`.
- [H] [ARC Swift](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/automaticreferencecounting/) — memória; `Y/WIP.MD:1396`, `Y/_w_/referencias.md:19`.
- [H] [FloatingPointMath GCC](https://gcc.gnu.org/wiki/FloatingPointMath) — ponto flutuante; `Y/WIP.MD:3541`, `Y/_w_/referencias.md:22`.
- [H] [printf musl](https://git.musl-libc.org/cgit/musl/tree/src/stdio/printf.c) — libc; `Y/_w_/WC.MD:261`.
- [H] [mimalloc](https://github.com/microsoft/mimalloc) — alocador; `Y/WIP.MD:529`.
- [H] [cbinc](https://github.com/Psteven5/cbinc) — interoperabilidade C; `Y/WIP.MD:3561`, `Y/_w_/referencias.md:62`.
- [H] [uprintf](https://github.com/spevnev/uprintf) — formatação; `Y/WIP.MD:3560`, `Y/_w_/referencias.md:61`.
- [H] [Cosmopolitan](https://justine.lol/cosmopolitan) — portabilidade; `Y/_w_/WC.MD:47`, `Y/_w_/referencias.md:4`.
- [H] [funções Cosmopolitan](https://justine.lol/cosmopolitan/functions.html) — API portável; `Y/WIP.MD:2485`.
- [H] [ICU](https://icu.unicode.org/) — Unicode; `Y/WIP.MD:1329`.
- [H] [musl vs glibc](https://wiki.musl-libc.org/functional-differences-from-glibc.html) — libc; `Y/_w_/build.md:11`.
- [H] [ABI C](https://stackoverflow.com/questions/4489012/does-c-have-a-standard-abi) — ABI; `Y/_w_/WC.MD:50`, `Y/_w_/referencias.md:15`.
- [H] [struct por valor](https://stackoverflow.com/questions/161788/are-there-any-downsides-to-passing-structs-by-value-in-c-rather-than-passing-a) — chamadas; `Y/_w_/WC.MD:52`, `Y/_w_/referencias.md:16`.
- [H] [UTF-8 BOM](https://stackoverflow.com/questions/2223882/whats-the-difference-between-utf-8-and-utf-8-with-bom) — codificação; `Y/_w_/WC.MD:117`.
- [H] [pragma pack](https://stackoverflow.com/questions/3318410/pragma-pack-effect) — layout; `Y/WIP.MD:3076`.
- [H] [pragma pack(8)](https://stackoverflow.com/questions/39359918/how-should-pragma-pack8-work) — layout; `Y/WIP.MD:3077`.
- [H] [atômico em struct](https://stackoverflow.com/questions/50601726/is-it-ok-to-use-stdatomic-with-a-struct-that-is-pod-except-that-it-has-a-const) — atômicos; `Y/WIP.MD:2391`.
- [H] [char8_t/printf](https://stackoverflow.com/questions/58878651/what-is-the-printf-formatting-character-for-char8-t) — strings; `Y/_w_/WC.MD:85`.
- [H] [ponteiro para atributo const](https://stackoverflow.com/questions/9441262/function-pointer-to-attribute-const-function) — atributos; `Y/_w_/WC.MD:200`.
- [H] [const struct e otimização](https://stackoverflow.com/questions/73526838/for-a-function-that-takes-a-const-struct-does-the-compiler-not-optimize-the-fun) — passagem/otimização; `Y/_w_/WC.MD:63`.
- [H] [memória C++](https://www.robopenguins.com/cpp-data-memory/) — memória; `Y/WIP.MD:3078`.
- [H] [SQLite blob write](https://www.sqlite.org/c3ref/blob_write.html) — SQLite; `Y/WIP.MD:3694`, `Y/_w_/referencias.md:23`.

## Dados, algoritmos, coleções e interface

- [H] [B+ tree](https://en.wikipedia.org/wiki/B+_tree) — árvore; `Y/_w_/WC.MD:253`, `Y/_w_/referencias.md:32`.
- [H] [TimSort LVP](https://github.com/LVPGroup/TimSort/) — ordenação; `Y/WIP.MD:2970`.
- [H] [rhsort](https://github.com/mlochbaum/rhsort) — ordenação; `Y/WIP.MD:2984`, `Y/_w_/referencias.md:58`.
- [H] [TimSort patperry](https://github.com/patperry/timsort/) — ordenação; `Y/WIP.MD:2969`.
- [H] [binary_search](https://github.com/scandum/binary_search) — busca; `Y/WIP.MD:2990`, `Y/_w_/referencias.md:59`.
- [H] [blitsort](https://github.com/scandum/blitsort) — ordenação; `Y/WIP.MD:2983`, `Y/_w_/referencias.md:57`.
- [H] [fluxsort](https://github.com/scandum/fluxsort) — ordenação; `Y/WIP.MD:2976`.
- [H] [fluxsort.h](https://github.com/scandum/fluxsort/blob/main/src/fluxsort.h) — ordenação; `Y/WIP.MD:2994`, `Y/_w_/referencias.md:35`.
- [H] [wolfsort](https://github.com/scandum/wolfsort) — ordenação; `Y/WIP.MD:2982`, `Y/_w_/referencias.md:56`.
- [H] [sort](https://github.com/swenson/sort) — ordenação; `Y/WIP.MD:2968`.
- [H] [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) — UI imediata; `Y/WIP.MD:2559`.
- [H] [microui](https://github.com/rxi/microui) — UI imediata; `Y/WIP.MD:2558`.
- [H] [sc](https://github.com/tezc/sc) — tabela/coleções; `Y/_w_/WC.MD:247`, `Y/_w_/referencias.md:42`.
- [H] [uthash](https://troydhanson.github.io/uthash/) — hash table; `Y/_w_/WC.MD:248`, `Y/_w_/referencias.md:43`.
- [H] [linear hashing](https://www.wikiwand.com/en/Linear_hashing) — hash table; `Y/WIP.MD:2956`, `Y/_w_/referencias.md:34`.
- [H] [busca ordenada](https://stackoverflow.com/questions/4057258/faster-than-binary-search-for-ordered-list) — busca; `Y/WIP.MD:2960`.
- [H] [GeeksforGeeks](https://www.geeksforgeeks.org/) — busca/algoritmos; `Y/WIP.MD:2961`.
- [H] [btreemap example](https://www.mycompiler.io/view/1BTkenQhGFI) — árvore; `Y/_w_/C/btreemap.c:2`.

## Build, módulos, empacotamento e carregamento

- [H] [sandboxing Linux](https://blog.cloudflare.com/sandboxing-in-linux-with-zero-lines-of-code) — isolamento de build; `Y/_w_/build.md:107`.
- [H] [superconfigure](https://github.com/ahgamut/superconfigure) — configuração; `Y/_w_/build.md:42`, `Y/_w_/referencias.md:39`.
- [H] [cloudflare/sandbox](https://github.com/cloudflare/sandbox) — sandbox; `Y/_w_/build.md:106`, `Y/_w_/referencias.md:48`.
- [H] [patchelf](https://github.com/NixOS/patchelf) — ELF; `Y/WIP.MD:3060`.
- [H] [hot reload cr](https://github.com/fungos/cr) — recarga; `Y/WIP.MD:2167`, `Y/_w_/referencias.md:60`.
- [H] [pacote Fisheryates](https://github.com/apple/example-package-fisheryates.git) — pacote Swift; `Y/WIP.MD:935`.
- [H] [pacote PlayingCard](https://github.com/apple/example-package-playingcard.git) — pacote Swift; `Y/WIP.MD:936`.
- [H] [recursos em executável](https://stackoverflow.com/questions/4158900/embedding-resources-in-executable-using-gcc) — empacotamento; `Y/WIP.MD:2178`.
- [H] [símbolos globais em bibliotecas](https://stackoverflow.com/questions/3004318/dynamic-loaded-libraries-and-shared-global-symbols) — carregamento; `Y/_w_/WC.MD:271`.
- [H] [colisão de símbolos estáticos](https://stackoverflow.com/questions/6940384/how-to-deal-with-symbol-collisions-between-statically-linked-libraries) — linkagem; `Y/_w_/WC.MD:267`.
- [H] [colisão de nomes C](https://stackoverflow.com/questions/15125059/how-do-you-prevent-names-from-colliding-in-c) — módulos; `Y/WIP.MD:1822`.
- [H] [função static por ponteiro](https://stackoverflow.com/questions/8023213/can-a-static-function-be-called-through-a-function-pointer-in-c) — linkagem; `Y/WIP.MD:1816`, `Y/_w_/referencias.md:24`.

## Banco de dados, isolamento e referências atuais

- [H] [SQLite em Durable Objects](https://blog.cloudflare.com/sqlite-in-durable-objects/) — persistência; `Y/WIP.MD:3676`, `Y/_w_/referencias.md:10`.
- [D] [cloudflare/sandbox](https://github.com/cloudflare/sandbox) — fornecido agora, mas já listado acima como histórico.
- [D] [SQLite em Durable Objects](https://blog.cloudflare.com/sqlite-in-durable-objects/) — fornecido agora, mas já listado acima como histórico.
- [A] [Durable Objects: easy, fast, correct](https://blog.cloudflare.com/durable-objects-easy-fast-correct-choose-three/) — fornecido pelo usuário; sem proveniência local anterior.
- [A] [Cap'n Web RPC](https://blog.cloudflare.com/capnweb-javascript-rpc-library/) — fornecido pelo usuário; sem proveniência local anterior.
- [A] [JavaScript-native RPC](https://blog.cloudflare.com/javascript-native-rpc/)
  — fornecido pelo usuário; service bindings, stubs tipados e promise pipelining.
- [A] [“The future of compute is fine-grained”](https://news.ycombinator.com/item?id=31759801) — comentário de Kenton Varda fornecido pelo usuário; inspira granularidade de execução, sem usar ali o termo “nanoservice”.
- [A] [Introducing workerd](https://blog.cloudflare.com/workerd-open-source-workers-runtime/) — fonte primária localizada na revisão; define a proposta de nanoservices e seus trade-offs.
- [A] [cloudflare/workerd](https://github.com/cloudflare/workerd) — runtime e princípios de nanoservices, capability bindings e limite de sandbox.

## Miscelânea, exemplos e fontes a triagem posterior

- [H] [API example](https://api.example.com/users/${userId}) — exemplo de cliente; `Y/_w_/README.md:121`.
- [H] [example.com](https://example.com) — exemplo de rede; `Y/_w_/README.md:51`.
- [H] [cleancodeqa](https://github.com/cmuratori/misc/blob/main/cleancodeqa-2.md) — discussão de estilo; `Y/WIP.MD:3011`, `Y/_w_/referencias.md:9`.
- [H] [gist Geal](https://gist.github.com/Geal/8f85e02561d101decf9a) — promises; `Y/_w_/WC.MD:299`.
- [H] [gist GeorgeLyon](https://gist.github.com/GeorgeLyon/c7b07923f7a800674bc9745ae45ddc7f) — nota técnica; `Y/WIP.MD:1269`.
- [H] [gist tclementdev](https://gist.github.com/tclementdev/6af616354912b0347cdf6db159c37057) — GCD; `Y/WIP.MD:2481`.
- [H] [google.com](https://google.com) — exemplo/consulta; `Y/WIP.MD:1052`.
- [H] [integer ranges](https://langdev.stackexchange.com/questions/1802/why-dont-many-languages-have-integer-range-types) — tipos inteiros; `Y/WIP.MD:3470`.
- [H] [coleções .NET](https://learn.microsoft.com/pt-br/dotnet/standard/collections/) — coleções; `Y/_w_/WC.MD:252`, `Y/_w_/referencias.md:33`.
- [H] [supermath 1.2.3](https://libs.w.org/supermath@1.2.3/math.w) — exemplo de pacote W; `Y/WIP.MD:1078,1086,1104`.
- [H] [supermath licença](https://libs.w.org/supermath@1.2.4/license) — exemplo de pacote W; `Y/WIP.MD:1213`.
- [H] [supermath 1.2.4](https://libs.w.org/supermath@1.2.4/math) — exemplo de pacote W; `Y/WIP.MD:1210`.
- [H] [supermath 1.2.4 fonte](https://libs.w.org/supermath@1.2.4/math.w) — exemplo de pacote W; `Y/WIP.MD:1116,1118`.
- [H] [supermath 1.3.3](https://libs.w.org/supermath@1.3.3/math.w) — exemplo de pacote W; `Y/WIP.MD:1111`.
- [H] [LWN](https://lwn.net/Articles/285332/) — consulta técnica; `Y/WIP.MD:885`.
- [H] [Twitter falco_girgis](https://twitter.com/falco_girgis/status/1788827993130815636) — nota externa; `Y/WIP.MD:3552`, `Y/_w_/referencias.md:11`.
- [H] [Twitter Hasen_Judi](https://twitter.com/Hasen_Judi/status/1781691223646089467) — nota externa; `Y/WIP.MD:3009`, `Y/_w_/referencias.md:8`.
- [H] [Intel Meteor Lake](https://www.phoronix.com/review/intel-meteorlake-gcc-clang) — desempenho de compiladores; `Y/_w_/WC.MD:359`.
- [H] [White House ONCD](https://www.whitehouse.gov/oncd/briefing-room/2024/02/26/press-release-technical-report/) — segurança de memória; `Y/WIP.MD:1675`.
- [H] [igualdade de structs](https://stackoverflow.com/questions/141720/how-do-you-compare-structs-for-equality-in-c) — C; `Y/WIP.MD:2122`.
- [H] [G3 e O3](https://stackoverflow.com/questions/18000214/compiling-with-both-g3-and-o3) — compilação; `Y/WIP.MD:2164`.
- [H] [vtable em C](https://stackoverflow.com/questions/66040677/implementing-basic-vtable-in-c) — OO em C; `Y/WIP.MD:3028`.

## Inspirações nomeadas sem URL próprio

Estas ocorrências são preservadas como nomes e contexto de pesquisa, não como
escolhas de W. Quando também há URL no inventário, o nome aponta para ele; caso
contrário a proveniência é apenas textual.

- **libmill / libdill** — fibras e concorrência; `libmill` é comparado com
  libfiber em `Y/WIP.MD:648`; nenhuma URL própria de libmill/libdill foi
  extraída.
- **mimalloc** — alocador; ver URL histórico acima.
- **xmake** — ferramenta de build; citado textualmente em notas históricas,
  sem URL extraído.
- **MLIR** — infraestrutura de compiladores; citado textualmente, sem URL
  extraído.
- **Tree-sitter** — parser incremental; ver URL histórico acima.
- **SQLite** — persistência; ver URLs histórico e atual acima.
- **Cosmopolitan** — libc/portabilidade; ver URLs históricos acima.
- **Turbo / outras técnicas de concorrência** — rótulo de pesquisa para fibras,
  filas lock-free, RCU, protothreads, atores e corrotinas; não é decisão nem
  nome de dependência confirmado.

## Limitações e integridade

- Não foram abertos centenas de destinos; a classificação é por contexto local
  e caminho/URL, não uma validação de conteúdo ou disponibilidade.
- Dois tokens tinham delimitador ausente no material de origem: o URL de PuTTY
  concatenado e os URLs de C23/Unum com parêntese final ausente. Foram
  preservados literalmente e marcados quando a forma impede validação segura.
- Algumas referências são exemplos (`example.com`, `api.example.com`,
  `libs.w.org`) e não fontes externas endossadas.
- A árvore histórica pode conter texto sem provenance ou nomes sem URL; a seção
  de inspirações registra apenas os nomes discerníveis e não inventa destinos.
- Não há links relativos locais neste documento; todos os destinos catalogados
  são URLs absolutos. Os ponteiros `Y/...:linha` localizam a origem tanto no
  arquivo histórico restaurado quanto, de modo reprodutível, no commit indicado.
