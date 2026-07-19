# Arquitetura do compilador W

> **Status:** Working Draft; direção arquitetural com detalhes candidatos
> **Data:** 18 de julho de 2026
> **Implementação:** ainda não existe um compilador conforme

Este documento descreve como transformar a semântica candidata de W em um
toolchain verificável. Ele não congela sintaxe, ABI ou escolhas de runtime. O
vocabulário **Direção**, **Candidato**, **Em aberto** e **Pesquisa** tem o significado
definido em [STATUS.md](../STATUS.md).

## Objetivo e limites

O compilador deve preservar as informações que tornam W previsível — tipos,
inicialização, ownership, borrows, efeitos, erros e estrutura de tasks — até que
passes específicos as validem e baixem. MLIR fornece infraestrutura de IR,
verificação e transformação; não define sozinho a linguagem, o runtime ou a ABI.

São objetivos do primeiro compilador:

- aceitar um corpus pequeno da [sintaxe candidata](../spec/syntax.md) com erros
  recuperáveis e diagnósticos estruturados;
- construir uma HIR tipada que torne controle de fluxo, ownership e efeitos
  explícitos;
- representar a semântica de W num dialeto próprio do MLIR;
- baixar essa representação até LLVM IR e um executável nativo;
- manter C como fronteira de ABI/FFI de primeira classe;
- oferecer um caminho EmitC/C opcional para um subconjunto definido;
- permitir testes de equivalência entre a semântica de referência e backends.

Não são objetivos iniciais self-hosting, ABI W estável, otimização agressiva,
hot reload, GPU, execução remota, macros arbitrárias ou cobertura completa da
linguagem imaginada. O compilador não resolve registries nem escolhe versões de
pacotes; recebe do toolchain um grafo já resolvido, conforme
[design/packages.md](packages.md).

## Pipeline de direção

```text
source .w
  -> tokens + CST com trivia e recuperação
  -> AST sem açúcar puramente sintático
  -> HIR tipada com CFG, ownership, efeitos e scopes de task
  -> dialeto W no MLIR
  -> verificações e lowerings semânticos
  -> dialetos MLIR de apoio
  -> LLVM dialect
  -> LLVM IR
  -> objeto, biblioteca ou executável nativo

  \-> EmitC/C para subconjunto portátil, referência e inspeção
```

A seta para C não substitui o caminho principal. C continua essencial como ABI e
ecossistema; emitir C cedo demais apagaria distinções que o frontend precisa
provar. O [dialeto LLVM do MLIR](https://mlir.llvm.org/docs/Dialects/LLVM/) e a
[tradução para LLVM IR](https://mlir.llvm.org/docs/TargetLLVMIR/) formam o backend
nativo de direção.

## Frontend

### Source, lexer e CST

O source canônico é UTF-8. Tokens carregam byte range, linha/coluna e identidade
do arquivo; trivia relevante — comentários, whitespace, delimitadores e tokens
inválidos — permanece na CST. Isso permite formatter, refactors e diagnostics em
arquivo incompleto sem tentar reconstruir o texto a partir da AST.

A gramática EBNF e o comportamento do parser do compilador têm status **Em aberto**.
Recursive descent com recuperação deliberada e um parser gerado são candidatos.
O contrato normativo futuro precisa incluir:

- léxico e precedência;
- árvore produzida para cada construção;
- pontos de recuperação e nós de erro;
- diagnósticos esperados para entradas negativas;
- relação entre quebra de linha, completude sintática e `;` aceito.

Tree-sitter é um candidato útil para CST incremental, highlighting e IDE. Ele não
é automaticamente o parser normativo. Se houver duas implementações, o corpus e
a EBNF são compartilhados e testes diferenciais detectam divergência; se uma CST
for compartilhada, essa equivalência precisa ser provada em edição incompleta.
Nenhum sketch isolado é baseline, conforme [spec/syntax.md](../spec/syntax.md).

### Ilhas multilíngues posteriores

`fn<lang>` pesquisa uma ilha de implementação pertencente ao source da aplicação,
não uma library externa implícita. É uma ferramenta de migração semelhante em
intenção ao `asm` de C, mas o body é entregue a um frontend completo. O primeiro
experimento vem somente depois de `foreign c` provar tipos e ownership na
fronteira.

O frontend W reconheceria assinatura e delimitadores, preservaria os bytes do
body na CST e criaria um nó AST opaco. Um adapter da linguagem receberia:

- body inline ou source separado pertencente à mesma receita;
- assinatura W já restrita a tipos de fronteira suportados;
- target, data layout, profile, capabilities e compilation unit;
- imports/includes explícitos e versões fixadas de frontend/toolchain.

O adapter devolveria IR ou object compatível, símbolos, diagnostics mapeados ao
source, dependências descobertas, metadata de efeitos/ownership e digests. O
parser W nunca interpreta statements C, Rust ou Zig como se fossem W.

Convergir em MLIR/LLVM pode habilitar link e otimização conjunta, mas compartilhar
backend não estabelece calling convention, runtime, exception/panic, layout ou
ownership. A própria [conversão de dialetos do MLIR](https://mlir.llvm.org/docs/DialectConversion/)
exige regras explícitas de legalidade e conversão/materialização de tipos; o
[LLVM dialect](https://mlir.llvm.org/docs/Dialects/LLVM/) carrega target triple e
data layout, e a [LLVM LangRef](https://llvm.org/docs/LangRef.html#calling-conventions)
exige calling conventions compatíveis. Cada adapter e target precisa provar
esses contratos. Namespaces como `fn<C::equipment>` podem agrupar calls diretas
numa compilation unit sem transformar a tag em lifecycle. As formas seguem abertas em
[W-O042](../STATUS.md) e no
[experimento do restaurante](../examples/restaurant/multilingual.md).

### Experimento de bootstrap para W-O008

A primeira implementação deve testar Tree-sitter como `SyntaxProvider` do
compilador, em vez de começar mantendo dois parsers. O adapter:

1. recebe source e devolve CST, trivia, ranges e erros estruturados;
2. rejeita `ERROR` e `MISSING` em builds, embora a IDE preserve a árvore parcial;
3. valida regras contextuais e separação de statements antes de formar a AST;
4. normaliza nós para tipos próprios de AST, sem deixar a HIR depender da API ou
   dos nomes de nós do Tree-sitter;
5. é coberto pelo mesmo corpus positivo/negativo usado pelo formatter e pelos
   diagnósticos.

Isso reduz o custo inicial e mantém a troca por recursive descent/Pratt possível.
W-O008 só é encerrada depois de medir qualidade dos diagnósticos, recovery,
tempo de parse completo/incremental, tamanho de distribuição e dificuldade das
construções contextuais. Se surgir um segundo parser, differential tests e fuzz
passam a ser obrigatórios.

### AST e resolução

A AST remove trivia e normaliza apenas açúcar cuja expansão não exige informação
de tipos. Cada nó mantém uma origem rastreável até um ou mais ranges da CST.
Resolução atribui identidades estáveis dentro da sessão de build a módulos,
declarações, generic parameters, labels e overloads. Hash pode indexar tabelas,
mas colisão nunca decide identidade de símbolo.

O compilador recebe do toolchain:

- mapa de nome lógico de módulo para source/interface;
- target, profile, features e capabilities autorizadas;
- versão da linguagem e opções semânticas;
- metadata de dependências e artefatos fixada pelo lockfile.

Imports não causam acesso à rede. `package.w` possui parser declarativo separado,
para que resolver dependências não dependa do compilador W completo.

### HIR tipada

A HIR é a última representação conveniente para diagnósticos de source e a
primeira em que todos os contratos semânticos precisam estar explícitos. Ela deve
registrar, antes de entrar no MLIR:

- tipos resolvidos, substitutions genéricas e constraints de layout/ABI;
- símbolo resolvido, módulo de origem e edição que autorizou cada lookup std
  implícito, sem perder effect/capability ou reachability;
- categorias `struct`, `object`, `enum`, protocol/existential e refinement;
- para cada refinement, tipo lógico base, predicate normalizado, range provado,
  layout canônico e pontos onde o endereço/layout se torna observável;
- dimensões/unidades normalizadas, literal original, overflow, rounding e modo
  floating-point observável;
- estado de inicialização por caminho de controle;
- CFG, patterns normalizados e joins de narrowing;
- owner, move, copy, borrow e exclusividade de `inout`;
- scopes de destruction e `defer`, inclusive saídas por erro/cancelamento;
- efeitos `mut`, `async`, `throws E` e os efeitos adicionais que forem adotados;
- error edges explícitas, sem depender de exception implícita;
- scopes parent/child, captures, sendability e cancellation points de tasks;
- requisitos `foreign c`, calling convention e layout da fronteira.

Inference pode aliviar o source, mas não a IR. Uma decisão ainda aberta, como o
algoritmo exato de last-use move ou `shared T`, deve aparecer como operação ou
propriedade explícita depois de resolvida pelo type checker.

## Dialeto W no MLIR

O dialeto W é **direção arquitetural**; a lista exata de tipos e operações é
**candidata**. Seu papel é ser a fronteira verificável entre a linguagem e os
lowerings, não um espelho um-para-um da AST.

Famílias mínimas candidatas:

| Família | Informação preservada |
|---|---|
| tipos e valores | value/object/enum/option/result, refinement provado, layout ainda abstrato |
| numéricos | dimensões/units, overflow, rounding, strict/reproducible/fast math e shapes adotados |
| ownership | criação, borrow, `inout`, move, copy, drop e fim de lifetime |
| controle e efeitos | calls tipadas, branches, `throws`, panic, cleanup e `defer` |
| tasks | scope, child concorrente, spawn paralelo, await, cancel, join e captures |
| memória | allocas/allocations abstratas, regiões candidatas e destruição |
| ABI | exports, imports C, repr, calling convention e metadata de ownership |
| source | locations, cadeia de inline/callsite e origem de código gerado |

O core do dialeto inicial provavelmente será implementado em C++ e TableGen,
usando as extensões documentadas em
[Defining Dialects](https://mlir.llvm.org/docs/DefiningDialects/) e
[Operation Definition Specification](https://mlir.llvm.org/docs/DefiningDialects/Operations/).
Essa é uma escolha candidata de bootstrap, não compromisso de linguagem de
implementação permanente. O projeto não deve depender da expectativa de que a
[C API do MLIR](https://mlir.llvm.org/docs/CAPI/) cubra com estabilidade todas as
APIs necessárias para definir dialetos e passes; qualquer binding fica atrás de
um adaptador estreito e versionado pelo toolchain.

Range propagation ocorre antes da escolha física de storage. O pipeline pode
usar a análise de ranges inteiros do MLIR e comunicar ranges preservados ao
LLVM, mas mantém no dialeto W a distinção entre tipo lógico, layout canônico e
representação especializada. Um passe tardio de representation selection:

1. encontra niches e larguras mínimas provadas;
2. rejeita compactação em ABI, FFI, persistence, raw view e address-taken;
3. especializa SSA/storage interno e escolhe lanes SIMD/target quando legais;
4. insere extensão, validação e checks necessários antes de perder o refinement;
5. registra a escolha para `w explain layout` e metadata do artefato.

Não se cria um `ref`/`inout` para um proxy descompactado temporário: addressability
é uma barreira de representação. Isso evita write-back implícito em erro,
cancelamento ou aliasing.

### Verificadores obrigatórios

Cada operação verifica sua forma local; passes de análise verificam invariantes
de função e módulo. Antes de apagar uma abstração, o pipeline precisa provar:

1. todo valor é inicializado antes da leitura;
2. um owner movido não é usado novamente e é destruído exatamente uma vez;
3. borrows não escapam do lifetime e `inout` permanece exclusivo;
4. todas as saídas executam drops e `defer` na ordem observável;
5. `try`/`throw` respeitam o error set e não perdem error edges;
6. todo child pertence a um scope e termina por await, cancel/join ou transferência
   para outro owner estruturado;
7. captures de `spawn` satisfazem as propriedades de transferência/compartilhamento;
8. cancellation preserva cleanup e não atravessa uma chamada C bloqueante sem
   adapter;
9. layout público e fronteiras `foreign c` são compatíveis com o target declarado;
10. operações dimensionais são válidas e conversões com escala/offset continuam explícitas;
11. nenhuma otimização altera overflow, opcionais, rounding ou garantia numérica
    entre profiles sem permissão explícita no source/profile correspondente.

Falha de verifier é bug do compilador e interrompe o pipeline. Erro de programa
deve ter sido diagnosticado na HIR com código, mensagem, labels de source e, onde
seguro, fix-it.

## Passes e lowerings

A ordem abaixo é candidata e deve evoluir com testes. O requisito é manter cada
invariante até o passe responsável por consumi-la.

| Estágio | Passes/análises principais | Pós-condição |
|---|---|---|
| HIR | resolução, type/refinement check, definite initialization, efeitos | programa semanticamente tipado ou diagnostics |
| W alto nível | construção de scopes, ownership/borrow check, capture e Send/Sync, error/cancel edges | invariantes de linguagem verificadas |
| W canônico | desugaring de option/result/patterns, monomorphization candidata, cleanup explícito | semântica ainda independente de layout |
| representação | data layout, enum/option layout, stack/heap/region, ABI e calling convention | representações escolhidas com fallback |
| tasks e errors | scopes para frames/continuations, result/control flow, runtime calls | parent/child, cleanup e erro preservados |
| MLIR comum | `func`, `scf`/`cf`, `arith`, `math`, `vector`/`linalg`, memória e outros dialetos aplicáveis | nenhuma operação W sem lowering definido |
| LLVM | conversão ao LLVM dialect, legalização por target, tradução a LLVM IR | módulo verificável pelo backend |
| nativo | otimização, codegen, link e metadata | artefato associado aos inputs do build |

Canonicalization no dialeto W só pode reescrever operações com equivalência
semântica demonstrada. Escolha de tagged/niche representation é tardia, depois de
target, ABI, sanitizer/hardening e fronteiras públicas; o fallback portátil vem
primeiro, conforme [research/tagged-values.md](../research/tagged-values.md).

O lowering de tasks pode aproveitar o
[dialeto Async do MLIR](https://mlir.llvm.org/docs/Dialects/AsyncDialect/),
[coroutines do LLVM](https://llvm.org/docs/Coroutines.html) ou state machines do
runtime. A escolha tem status **Em aberto**. Nenhuma dessas ferramentas, isoladamente,
define structured concurrency, typed errors, cancellation ou sendability de W.

### EmitC/C

EmitC é um backend opcional cujo subset tem status **Em aberto** e será explicitamente
enumerado. Usos plausíveis são bootstrap, portabilidade para targets sem backend
LLVM suportado, inspeção e oracle diferencial. O experimento inicial deve começar
com funções síncronas, scalars, structs de layout conhecido, controle de fluxo e
calls sem tasks.

O caminho é W MLIR já verificado ->
[dialeto EmitC](https://mlir.llvm.org/docs/Dialects/EmitC/) -> C. Ele não recebe AST
diretamente. Cada exclusão do subset deve gerar diagnóstico; não pode degradar
silenciosamente ownership, overflow, typed errors, async ou ABI. C gerado é um
artefato/representação de backend, não a especificação semântica de W.

## Runtime e ABI interna

O runtime mínimo é pequeno, mas não trivial. Sua superfície deve ser uma ABI
interna estreita, versionada junto ao compilador e representável em C para
facilitar portabilidade e inspeção. Isso não congela a ABI pública de W.

Responsabilidades candidatas:

- task control block, árvore parent/child e handles one-shot;
- executor concorrente single-thread e pool paralelo limitado;
- wakeups, timers, cancelamento cooperativo, join e cleanup;
- frames/continuations produzidos pelo lowering;
- alocação, panic e hooks de destruction necessários ao código gerado;
- adapter de I/O por plataforma e executor para foreign calls bloqueantes;
- tracing de task/scope, crash metadata e integração com symbolization.

O runtime não implica GC global, heap por módulo, uma thread por task, event loop
único ou filas lock-free. Regiões, ARC/shared, work stealing e backends modernos
de I/O entram apenas quando o modelo e medições justificarem. A semântica candidata
completa está em [spec/concurrency.md](../spec/concurrency.md).

As chamadas geradas devem declarar ownership de parâmetros/retornos, estado de
erro, cancellation context e destruidor de frames. A decisão entre tagged result,
status + out parameter ou outra calling convention para `throws E` permanece
aberta e deve ser medida junto a ownership e C wrappers.

## Compatibilidade C

C é uma fronteira de direção, não um IR semântico universal. O frontend aceita
declarações `foreign c` e tipos do namespace `c`. A proposta W-O044 faz
structs/enums/unions declarados dentro da fronteira solicitarem layout C validado
para o target; a forma ainda está **Em aberto**. A metadata da fronteira registra
ao menos:

- target ABI, calling convention, header, símbolo e library;
- nullable, `(ptr, len)`, owner/deallocator e lifetime de callbacks;
- status/errno/error conversion e proibição de exception atravessar a fronteira;
- thread safety, blocking, callback executor e cancellation support;
- varargs e tipos ABI-specific usados.

Importar headers automaticamente e gerar bindings são ferramentas candidatas;
adapter declarations/overrides continuam necessárias onde C não expressa
ownership ou concorrência. Exportar W para C requer wrapper e header gerados que não exponham
representações internas instáveis. Testes de ABI compilam um harness C separado,
em vez de comparar apenas texto ou tamanho estimado.

C++ e outros runtimes precisam de adapter próprio depois de a fronteira C estar
provada. O compilador nunca promete tornar uma API C bruta segura sem wrapper que
restabeleça as invariantes de [tipos e memória](../spec/types-and-memory.md).

## Diagnósticos, debug e source maps

Todo estágio mantém `source location`. Expansões e código gerado carregam origem,
callsite e cadeia de transformações; operações fundidas preservam múltiplos ranges
quando isso melhora o diagnóstico. IDs de diagnóstico são estáveis dentro de uma
versão do toolchain e podem sair como texto e formato estruturado para IDE/CI.

O backend LLVM deve emitir informação compatível com o formato do target, como
DWARF ou CodeView, através das capacidades do LLVM. O objetivo é mapear funções,
locals não otimizados e inlining ao source W. Para tasks, o runtime mantém IDs,
parent scope e suspension site, permitindo uma pilha lógica mesmo quando a pilha
física foi dividida em continuations. A fidelidade exata sob otimização é um
critério medido, não uma promessa antecipada.

EmitC deve preservar `#line` quando correto e pode emitir um sidecar source map
versionado. Código de stubs/serviços também precisa registrar schema e range de
origem; detalhes de serviços ficam fora do core em
[ecosystem/services-and-protocols.md](../ecosystem/services-and-protocols.md).

## Bootstrap e fatias verticais

O bootstrap deve crescer por programas executáveis, não por listas horizontais
de features. Bun/TypeScript é útil para corpus, runner, formatter/IDE experimental,
visualização e CLI; o core do dialeto/pass pipeline é candidato a C++/TableGen.
Essa separação evita fazer da integração MLIR via C API uma dependência estrutural
do primeiro compilador.

### Slice 0 — contrato do frontend

- 10–20 programas dourados e pares negativos;
- EBNF de trabalho, lexer/parser recuperável e CST serializável;
- formatter idempotente;
- AST/HIR snapshots para bindings, funções, controle e tipos básicos.

O [corpus versionado](../corpus/README.md) inicia esta fatia com 12 positivos,
11 negativos e CST determinística. Formatter e AST/HIR ainda não estão
implementados; parse aceito não deve ser confundido com semântica aprovada.

**Saída:** nenhum programa nativo ainda; sintaxe e diagnostics podem ser mudados
com evidência antes de contaminar o backend.

### Slice 1 — executável síncrono mínimo

- `fn main`, scalars, `let`/`var`, calls, branches e loops;
- HIR tipada, dialeto W mínimo e verifiers;
- lowering LLVM completo até objeto/link;
- panic/overflow definido e impressão por uma pequena função runtime/foreign.

**Saída:** pelo menos dois programas não triviais rodam em debug e release com a
mesma semântica observável.

### Slice 2 — valores, errors e ownership

- structs/enums/`T?`, narrowing e `throws E`/`try`;
- definite initialization, move/borrow/`inout`, drop e `defer`;
- uma fronteira C com struct declarada no bloco, buffer e callback simples;
- fallback de layout portátil e comparação inicial com EmitC no subset.

**Saída:** positivos, negativos e harness C demonstram as invariantes, inclusive
em caminhos de erro.

### Slice 3 — tasks estruturadas

- `async let`, `spawn let`, await, cancel/join e captures;
- executor concorrente, pool paralelo limitado, timer e um adapter de I/O;
- cleanup de frame e erro/cancelamento determinísticos;
- tracing de task e source location de suspension.

**Saída:** downloader/servidor pequeno e workload CPU exercitam concorrência e
paralelismo sem child vazado ou data race aceita pelo type checker.

### Slice 4 — artefatos reproduzíveis

- interfaces de módulo e metadata de ABI versionadas experimentalmente;
- build local hermético com manifest/lock e cache content-addressed;
- debug symbols/source metadata vinculados ao artefato;
- matriz inicial de Windows, Linux e macOS conforme disponibilidade real.

**Saída:** dois builds com os mesmos inputs têm grafo idêntico; qualquer alegação
bit a bit depende de a receita capturar todos os inputs.

Self-hosting só entra depois dessas fatias, se reduzir a base confiável ou melhorar
a linguagem na prática. Reescrever por prestígio não é milestone.

## Estratégia de testes

| Classe | O que prova |
|---|---|
| golden | tokens, CST, AST/HIR, diagnostics, formatter e IR antes/depois de passes |
| negative | syntax, tipos, ownership, use-after-move, aliases, effects, tasks e FFI inválidos |
| differential | semântica de referência vs LLVM e, no subset, EmitC/C; fallback vs otimização de layout |
| round-trip | formatter idempotente, parse/print de IR e metadata versionada |
| property/fuzz | lexer/parser, tipos serializados, layouts, encode/decode e pass pipelines |
| runtime | scheduler determinístico de teste, cancelamento, falhas concorrentes, cleanup e shutdown |
| ABI | harness C separado, headers gerados, calling convention, buffers e callbacks |
| target | debug/release, níveis de otimização, sanitizers disponíveis e mais de um target |
| build | inputs fixados, cache, artefatos, source/debug linkage e reprodutibilidade declarada |

Cada bug de miscompile ganha caso reduzido no nível mais alto que ainda o
reproduz e, quando possível, oracle no nível abaixo. Golden de IR não substitui
execução; teste end-to-end não substitui verifier/negative preciso.

## Incrementalidade e cache

A unidade inicial de type checking pode ser o módulo, com fingerprints de source,
interface pública, compiler/runtime, target e opções semânticas. AST/HIR/MLIR
serializados são caches descartáveis, nunca autoridade. Um hit só é válido quando
schema, pipeline e todos os inputs relevantes coincidem.

Formato de interface/módulo é questão aberta: MLIR bytecode, formato próprio ou
combinação. Antes de estabilidade, todo artefato inclui versão exata do produtor e
pode exigir rebuild. Identidades do cache e regras de distribuição pertencem ao
[sistema de pacotes](packages.md); o compilador apenas produz metadata determinística
e explica dependências de recompilação.

## Riscos e decisões ainda abertas

| Tema | Estado | Como decidir |
|---|---|---|
| parser normativo e papel do Tree-sitter | Em aberto | recuperação, edição incremental, manutenção e corpus diferencial |
| implementação do core MLIR: C++/TableGen vs bindings | Em aberto | cobertura das APIs, pin de LLVM, portabilidade do build e velocidade de iteração |
| algoritmo de moves e shared ownership | Em aberto | marcadores em programas reais, cycles, FFI e custo em tasks |
| ABI de typed errors | Em aberto | benchmark de tagged result/status-out e wrappers C |
| lowering de async | Em aberto | correctness de cleanup/cancelamento, debug, tamanho de frame e performance |
| subset EmitC | Em aberto | equivalência, legibilidade C, targets atendidos e custo de manter dois caminhos |
| ABI/interface W | Em aberto | evolução compatível de tipos, generics, ownership e metadata |
| tagged values, GPU e WC público | Pesquisa | fallback primeiro, testes por target e ganho medido |

O maior risco técnico é baixar abstrações antes de validar suas invariantes. O
segundo é confundir um spike que executa com uma implementação conforme.
Protótipos só entram no runtime após revisão de provenance, testes negativos,
benchmarks, casos de FFI e uma suite de conformidade.

## Critério para avançar uma decisão

Uma operação, passe ou ABI candidata só avança quando possui:

1. semântica e condição de erro documentadas;
2. verifier ou análise que rejeite estados inválidos;
3. positivos, negativos e ao menos um programa não trivial;
4. lowering completo com cleanup, FFI e target considerados;
5. comparação com uma alternativa mais simples;
6. diagnóstico/source mapping suficiente para depurar a escolha.

Até lá, este documento é um plano de prova: concreto o bastante para orientar a
implementação e explícito sobre o que ainda pode mudar.
