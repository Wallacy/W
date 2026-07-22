# Mapa técnico do W

> **Status:** Working Draft; resumo executivo, não especificação normativa
> **Data:** 21 de julho de 2026
> **Implementação:** compilador, runtime e stdlib ainda não existem

Este documento é a porta de entrada técnica do W. Ele mostra as camadas, o estado
de cada escolha e onde está o contrato canônico. Detalhes de sintaxe, memória,
tasks, compilador e packages pertencem aos documentos linkados, não são repetidos
aqui.

## Leitura de status

- **Direção:** intenção estável do projeto.
- **Candidato:** baseline coerente a ser provada por protótipo.
- **Em aberto:** alternativas delimitadas ainda precisam de comparação.
- **Pesquisa:** ideia preservada fora do caminho crítico de v0.

As definições e IDs de decisão vivem em [STATUS.md](STATUS.md). Nenhuma escolha
candidata neste mapa garante compatibilidade futura.

## Arquitetura em uma linha

```text
W source -> CST/AST -> HIR tipada -> dialeto W no MLIR
         -> lowerings semânticos -> LLVM dialect -> LLVM IR -> nativo
         \-> EmitC/C para subset portátil, referência e inspeção
```

Preservar ownership, efeitos, errors e tasks no dialeto W até seus verificadores
rodarem é **direção**. LLVM é o backend nativo de direção. EmitC/C é um caminho
**opcional e aberto**; C permanece ABI/FFI de primeira classe, mas não define toda
a semântica interna da linguagem. Veja [arquitetura do compilador](design/compiler.md).

## Mapa das camadas

| Camada | Baseline técnica | Estado | Documento canônico |
|---|---|---|---|
| experiência | custos importantes visíveis, forma canônica e semântica igual em debug/release | direção | [README](README.md), [tour](LANGUAGE_TOUR.md) |
| source | UTF-8, `fn`, `const`/`let`/`var`, tipos e efeitos explícitos | candidata | [sintaxe](spec/syntax.md) |
| tipos | value/object/enum/protocol, `T?`, typed errors, refinements e aplicação generic | candidata; value parameters W-O103 abertos | [tipos e memória](spec/types-and-memory.md) |
| ownership | owner único, borrows locais, `ref`/`inout`/`take`/`copy`, destruição determinística | candidata; inference/shared abertos | [tipos e memória](spec/types-and-memory.md) |
| tasks | `async let` concorrente, `spawn let` paralelo, scopes, join e cancelamento | candidata; lowering e execution domains W-O100 abertos | [concorrência](spec/concurrency.md) |
| frontend | CST recuperável, AST e HIR tipada; EBNF/parser normativo por definir | Em aberto | [compilador](design/compiler.md), [sintaxe](spec/syntax.md) |
| IR/backend | dialeto W/MLIR, lowerings, LLVM dialect/IR e código nativo | direção arquitetural; operações candidatas | [compilador](design/compiler.md) |
| runtime | tasks, isolation/executors, host entries, timers, cancelamento, I/O, panic e tracing | modelo candidato; bindings W-O100–W-O101 abertos | [concorrência](spec/concurrency.md), [módulos](spec/modules.md) |
| stdlib/SDK | T0 independente de ambiente; T1 systems/`print`; T2 domains, com mapa implícito por edição | candidata DB1; inventário por prototipar | [biblioteca padrão](design/stdlib.md), [revisão DB1](DB1_REVIEW.md#h09--sdk-t0t1t2-e-capabilities) |
| numéricos/ML | overflow explícito, quantities `[unit]`, modos float; tensor/shape/linalg/device T2 | baseline numérica candidata; superfície tensor W-O102 aberta | [numéricos e quantidades](design/numerics-and-quantities.md) |
| docs/testes | `///` Markdown, doctests, `test ... for` e runner único | candidata DB1; implementação inexistente | [documentação e testes](design/documentation-and-tests.md) |
| C | `foreign c`, wrappers e metadata de ownership/concurrency | direção; ABI/layout detalhados abertos | [tour](LANGUAGE_TOUR.md), [tipos e memória](spec/types-and-memory.md) |
| módulos/instâncias | módulo estático sem lifecycle; service state e entry/host binding explícitos | direção + runtime candidato; syntax entry aberta | [módulos](spec/modules.md), [runtime de instâncias](design/modules-and-runtime.md) |
| análise de recursos | delta de artefato por import; baseline de instância e peak de operação separados | direção de transparência; tooling em pesquisa | [estimativa de recursos](design/resource-estimation.md) |
| packages/builds | manifest declarativo, lock, cache content-addressed e verificação independente | direção/design em elaboração | [packages](design/packages.md) |
| serviços/protocolos | contratos, wRPC, wQL, RestPC e codecs sobre o core | pesquisa de ecossistema | [serviços e protocolos](ecosystem/services-and-protocols.md) |

## Contrato semântico que o compilador deve preservar

A HIR tipada explicita, mesmo quando o source permite inference:

- tipo, initialization state e refinements provados por caminho;
- kinds e valores compile-time de generic applications, inclusive shape facts;
- owner, borrows, move/copy, exclusividade e ordem de destruction;
- `mut`, `async`, `throws E`, error edges e cleanup por `defer`;
- dimensões/unidades, rounding, overflow e permissões floating-point;
- símbolo std resolvido, effect/capability e edição que autorizou o lookup;
- scopes parent/child, captures, sendability, await/join e cancelamento;
- required isolation, executor preference, parallel intent, host affinity e entry slot;
- tensor shape/strides/alias, materialization, device transfer e numeric mode;
- layout público, calling convention e requisitos da fronteira `foreign c`.

O dialeto W mantém essas propriedades até passes específicos verificarem que não
há use-after-move, dangling borrow, alias mutável, child vazado, error perdido ou
cleanup omitido. Só então escolhe layout, frames/continuations, calls de runtime e
representações LLVM. Tagged/niche layout é otimização tardia com fallback, não
modelo universal de valores.

## Frontend e tooling

A EBNF normativa e a implementação do parser estão **abertas**. Recursive descent
com recuperação e parser gerado são candidatos. Tree-sitter pode atender CST
incremental e IDE; ele só participa do compilador se corpus e testes provarem que
não há duas gramáticas divergentes.

O corpus de exemplos, parser e formatter evoluem juntos. A CST conserva trivia e
nós de erro; AST remove açúcar simples; HIR resolve tipos e semântica. Diagnostics
possuem código, ranges, labels e formato estruturado. Bun/TypeScript continua uma
dependência apenas do portal POC; não pertence ao compilador, formatter, runner
ou bootstrap W.

O [restaurante](examples/restaurant/README.md) é o corpus top-down principal:
dezesseis módulos cobrem quantities, PID, scheduling, billing, ownership,
services, TUI, HTTP e a boundary C. Sua [matriz de requisitos](examples/restaurant/REQUIREMENTS.md)
explicita as alternativas e o que cada estágio precisa provar.

O [tooling inicial](tooling/README.md) separa highlighting lexical imediato de
parsing estrutural. TextMate atende a extensão local do VS Code; Tree-sitter e
suas queries formam um protótipo incremental para corpus/IDE/portal. Nenhum dos
dois é gramática normativa enquanto parser e papel do Tree-sitter permanecerem
em W-O007/W-O008.

O lens de recursos reutiliza o grafo resolvido e, progressivamente, HIR,
reachability pós-DCE/LTO e instrumentação. Cada valor carrega target/profile,
proveniência e confiança; artefato, instância e operação não são somados como se
fossem a mesma dimensão. O desenho experimental está em
[design/resource-estimation.md](design/resource-estimation.md).

O core inicial do dialeto e dos passes provavelmente usa C++/TableGen. Essa é uma
escolha **candidata** condicionada à integração real com MLIR; bindings ficam
atrás de uma interface estreita. Detalhes e links oficiais estão em
[design/compiler.md](design/compiler.md).

## Stack de bootstrap proposta

Esta tabela reduz escolhas implícitas sem congelar a arquitetura antes das
provas verticais:

| Parte | Baseline proposta | Limite da decisão |
|---|---|---|
| contrato da linguagem | spec + corpus positivo/negativo + diagnostics versionados | nenhuma implementação isolada define W |
| sintaxe incremental | `tree-sitter-w`, runtime C e adapter estrito CST -> AST | permanente no tooling; participação no compilador é experimento W-O008 |
| seed do compiler | `w-seed-c`, profile C11 portátil W-owned | subset auditável; Dependable C informa compatibilidade, sem aceitar UB ou impor C89 |
| compiler principal | W self-hosted cedo | versão estável anterior é bootstrap normal; seed C permanece rota de auditoria |
| MLIR core | ABI C estreita para C++/TableGen compatível com a revisão LLVM fixada | detalhes C++ não vazam na HIR W |
| build do toolchain | CMake + Ninja até `w build` assumir | xmake/Bun não são dependências do compilador |
| representações | AST própria -> HIR/CFG tipada -> dialeto W -> dialetos MLIR -> LLVM | MLIR não substitui frontend, type checker ou semântica de runtime |
| runtime | `libwrt` mínima, ABI C versionada, componentes core/task/platform | allocator, scheduler e I/O são backends substituíveis; mimalloc é candidato |
| packages/builds | CLI `w`, CAS de blobs, SQLite para índice/metadata e lock hermético | storage de aplicação e SQLite-by-default não entram no runtime-base |
| editor | TextMate imediato -> `wls` via LSP + semantic tokens da HIR | VS Code não consome Tree-sitter automaticamente |
| portal | Bun/assets estáticos -> Tree-sitter WASM local | execução W/WASM vem depois de parse/diagnostics reproduzíveis |
| qualidade | corpus, goldens, `lit`/FileCheck, fuzz, sanitizers e rebuild bit a bit | cada lowering mantém oracle observável, não só snapshots de IR |

O primeiro corte executável é deliberadamente sequencial: parse/check,
funções e scalars, `emit-mlir`, objeto/link e `libwrt-core`. Ownership, tasks,
services, SQLite durável e isolamento entram depois que seus contratos forem
testáveis. Essa ordem evita fazer de uma dependência útil a semântica acidental
da linguagem.

## Lowering e backends

Ordem conceitual:

1. parse, resolução, tipos, initialization e effects;
2. ownership/borrow, captures, errors e estrutura de tasks;
3. canonicalização no dialeto W sem apagar invariantes;
4. seleção tardia de layout, ABI, allocation e calling convention;
5. lowering de errors/tasks para controle de fluxo, frames e runtime calls;
6. dialetos MLIR de apoio -> LLVM dialect -> LLVM IR -> nativo.

O caminho EmitC recebe o dialeto W já verificado e aceita apenas um subset
declarado. O protótipo começa por funções síncronas, scalars, structs conhecidas,
controle de fluxo e calls. Features sem representação correta falham com
diagnóstico; não mudam silenciosamente de semântica.

## Runtime

O runtime mínimo candidato fornece:

- task control blocks e árvore de scopes;
- executor concorrente e pool paralelo limitado;
- wakeups/timers, cancelamento cooperativo, join e cleanup;
- frames/continuations gerados pelo lowering;
- allocation/destruction, panic, tracing e hooks de symbolization;
- I/O adapter por plataforma e tratamento explícito de C bloqueante.

Isso não promete GC global, heap por módulo, thread por task, ausência de locks,
event loop único ou uma tecnologia específica de I/O. ARC/shared, regiões,
work-stealing e outras políticas permanecem abertas ou em pesquisa.

## Debug e observabilidade

Locations são carregadas de tokens até LLVM/EmitC. O backend nativo deve produzir
debug information do target e preservar inlining/source ranges na medida testada.
Tasks precisam de stack lógica formada por task ID, parent scope e suspension
site, pois continuations não correspondem sempre à pilha física.

EmitC pode usar `#line` e sidecar versionado. Diagnostics textuais e estruturados,
traces de passes e dumps HIR/MLIR reproduzíveis fazem parte da superfície de
desenvolvimento. Fidelidade sob otimização é medida por testes, não presumida.

## C, ABI e artefatos

`foreign c` anuncia a fronteira unsafe. W-O044 propõe que tipos declarados dentro
do bloco usem layout C compatível com o target; essa forma segue **Em aberto**.
Wrappers convertem nullable, `(ptr, len)`, allocator/deallocator, callbacks,
status/errno, thread safety e chamadas bloqueantes para contratos W explícitos.
Exports W para C usam headers/wrappers; não expõem layout interno instável.

`fn<lang>` permanece pesquisa posterior a essa baseline. O
[experimento do equipamento](examples/restaurant/multilingual.md) compara source
externo, body inline, namespace de compilation unit e adapter declarado sem
promover nenhum deles à grammar.

A ABI W pública ainda está **aberta**. Artefatos experimentais registram target,
runtime, toolchain, feature/profile e versão de metadata. O toolchain resolve
packages, verifica lock/provenance e escolhe source/static/dynamic; o compilador
recebe o grafo fechado e nunca consulta registries por conta própria.

## Verificação

A estratégia mínima combina:

- **golden:** CST, AST/HIR, diagnostics, formatter e IR por passe;
- **negative:** sintaxe, tipos, ownership, effects, tasks e FFI inválidos;
- **differential:** oracle semântico vs LLVM e, no subset, EmitC/C;
- **runtime:** scheduler controlado, cancelamento, cleanup, races e shutdown;
- **ABI:** harness C compilado separadamente e headers gerados;
- **property/fuzz:** parser, serializers, layouts e pipelines de passes;
- **source:** doctests, compile-fail e `test ... for` no mesmo grafo, removidos do
  payload release;
- **target/build:** debug/release, otimização, instrumentation disponível e inputs
  reproduzíveis.

Golden de IR não basta para provar execução, e end-to-end não substitui verifier
ou diagnóstico negativo preciso.

## Bootstrap incremental

1. **Seed C:** 10–20 programas, EBNF de trabalho, CST, formatter mínimo e HIR
   suficiente para produzir executáveis síncronos.
2. **Síncrono nativo:** `main`, scalars, controle, calls e lowering completo pelo
   seed, seguido do primeiro compiler W.
3. **Semântica de valores:** structs/enums/options, typed errors, ownership,
   cleanup e um harness C.
4. **Tasks:** `async let`/`spawn let`, executor mínimo, cancelamento e um I/O
   adapter.
5. **Self-host/artefatos:** rebuild em estágios, interfaces versionadas, build
   hermético, lock/cache e debug metadata associados ao output.

Cada fatia termina em programas executáveis e negativos correspondentes. O
self-host é cedo para tornar W o ambiente real de desenvolvimento; não congela a
ABI pública nem transforma reescrita em prova de correctness.

## Decisões prioritárias e pesquisa

Continuam como **gates de protótipo**: algoritmo de move, shared ownership, ABI de
typed errors, parser normativo, papel do Tree-sitter, fronteira do core MLIR,
subset EmitC, lowering async e formato de módulos/ABI.
A lista controlada está em [STATUS.md](STATUS.md).

Continuam em **pesquisa**: tagged pointers/values, arenas por módulo, WC como IR
pública, `fn<lang>`, wQL/wRPC/RestPC, V6/Computer Units, tree strings, GPU/HDL,
snapshots/PGO e autotest por IA. O catálogo e critérios de promoção estão em
[research/README.md](research/README.md); experimentos anteriores não são
implementação do runtime nem substituem as provas propostas ali.

## Índice canônico

- [README.md](README.md): promessa, arquitetura curta e estado geral.
- [ARCHITECTURE.md](ARCHITECTURE.md): planos do sistema, contratos de
  compatibilidade, profiles, bootstrap, segurança e evolução de longo prazo.
- [STATUS.md](STATUS.md): decisões e maturidade.
- [LANGUAGE_TOUR.md](LANGUAGE_TOUR.md): experiência candidata ponta a ponta.
- [spec/syntax.md](spec/syntax.md): sintaxe de trabalho.
- [spec/types-and-memory.md](spec/types-and-memory.md): tipos, ownership e memória.
- [spec/concurrency.md](spec/concurrency.md): tasks, cancelamento e runtime mínimo.
- [spec/modules.md](spec/modules.md): imports, interfaces e instâncias de execução.
- [design/compiler.md](design/compiler.md): frontend, IR, passes, backends e testes.
- [design/documentation-and-tests.md](design/documentation-and-tests.md): `///`,
  doctests, testes co-localizados e runner.
- [tooling/README.md](tooling/README.md): VS Code/TextMate, Tree-sitter e caminho
  de integração do highlighting no portal.
- [design/memory-strategy.md](design/memory-strategy.md): lowering híbrido,
  allocators, regiões, shared ownership e tagged representations.
- [design/modules-and-runtime.md](design/modules-and-runtime.md): executor por
  instância, calls, storage, capabilities e isolamento por target.
- [design/resource-estimation.md](design/resource-estimation.md): lens de import,
  custos de instância/operação, confiança, perfis e budgets.
- [design/stdlib.md](design/stdlib.md): fronteiras, APIs, portabilidade e escopo v0
  da biblioteca padrão.
- [design/packages.md](design/packages.md): resolução, builds e supply chain.
- [design/verification-and-releases.md](design/verification-and-releases.md):
  payload reproduzível, manifests, attestations, builders e registry.
- [ecosystem/services-and-protocols.md](ecosystem/services-and-protocols.md): pesquisa
  de contratos e protocolos.
- [research/README.md](research/README.md): hipóteses fora do núcleo v0.
- [research/long-term-program.md](research/long-term-program.md): trilhas,
  baselines, gates e ondas de pesquisa.
- [ROADMAP.md](ROADMAP.md): ordem das provas e critérios de saída.
