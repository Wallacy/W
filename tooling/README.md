# Tooling do W

> **Status:** Working Draft. Este diretório contém a infraestrutura local e as
> projeções do W. Ele não define a semântica da linguagem.

O contrato normativo está em [`DESIGN.md`](../DESIGN.md). Use
[`DESIGN-INDEX.md`](../DESIGN-INDEX.md) para localizar uma seção e
[`RATIONALE.md`](../RATIONALE.md) para justificativas, evidência e proveniência.
O tooling valida essas fontes, gera índices humanos e oferece oracles de
design. Um oracle não é compiler, runtime, provider ou resultado de usuário.

## Comandos públicos

Os scripts públicos ficam somente no `package.json` da raiz. O manifesto
declarativo em [`check-suites.json`](check-suites.json) mantém a ordem e os
limites das suítes.

```sh
bun run tooling:install
bun check
bun check --target compiler
bun check --target docs
bun check --target studies
bun check --target benchmark
bun check --target all
bun check --list
bun check --list-all
bun check --target quick --dry-run
bun demo --list
bun bootstrap --target host
bun dev run compiler/seed-c/fixtures/hlo0-hello.w
bun tooling/command-runner.mjs --list
bun check --target study-registry
bun run study:registry
bun benchmark list
bun benchmark update benchmarks/results/<result>.json
bun benchmark check
```

The short facade in [`dev-cli.mjs`](dev-cli.mjs) reads the small catalog in
[`dev-cli.json`](dev-cli.json). `bun check` selects `quick` by default;
`compiler`, `docs`, `studies`, and `all` point to the already ordered suites in
[`check-suites.json`](check-suites.json), without maintaining a second order.
`--list` shows only the small public surface, `--list-all` includes internal
leaf checks, and `--dry-run` only expands the selected plan.

`bun demo` executes a named fixture with the public `w run` binary through the
current MLIR route; it does not use the removed `demo:seed-hello` alias or HLO1 C. `bun bootstrap --target host`
uses the local development recipe, validates `w.exe` and
`receipt.json`, and never downloads a toolchain. In this first cut, bootstrap,
demo, and `dev run` are native Windows x64 operations; Linux is explicitly
unsupported. `dev run` requires an explicit existing regular `.w` path and may
accept one outside the checkout, while catalog, fixture, binary, and receipt
paths remain contained. Only arguments after `--` are forwarded.
Timers appear on stderr as non-benchmark DX metadata. The earlier
Root package scripts intentionally expose only this facade and a small set of
projection/installation maintenance commands. Internal leaves are stored as
shell-free argv plans in [`command-registry.json`](command-registry.json).
Use [`command-runner.mjs`](command-runner.mjs) for a named internal leaf when
the public target facade does not provide a narrower scope. The old colon
aliases are no longer root package commands.

Use o runner para inspecionar uma suíte antes de executá-la:

```sh
bun tooling/check-suite.mjs --list
bun tooling/check-suite.mjs --dry-run --suite root-quick
bun tooling/check-suite.mjs --dry-run --suite root-compiler
```

`bun check --target quick` validates manifests, cleanup policy, the current
platform/dependency/diagnostic projections, maintained parsing, links, and the
BMD/executable catalogs without compiler builds or deep design oracles.
`check:bmd:executable` is a separate Hello correctness smoke: it never runs W,
records no timing, and compiles C23/c2x and Rust when toolchains are available.
The public executable catalog uses `windows-x64` as the shared platform class;
Clang C, W, and Rust use `x86_64-pc-windows-msvc`. The private handler
composite retains its contextual GCC/MinGW lane. Rust uses edition 2024.
Future measured records are exploratory, measurement-only, and not-evaluated;
the catalog's source/oracle readiness does not make W performance-ready.
Raw wall/RSS samples and artifact sizes are strictly positive; CPU counters may
be zero at their disclosed microsecond resolution, and arithmetic means use
integer-floor rounding. Result host identities are derived from normalized
redacted environment classes, never from hostnames, users, or paths. The live
best-metrics contract stores only positive lower-is-better cells; zero CPU
measurements never become best, and migrated cells are historical/unverified.
The executable facade's `run` command measures one W, C or Rust source. W uses
the public Native0/MLIR0 Windows source-to-PE candidate route. Public C requires
final `-std=c23`, Clang/MSVC, LLD, and the DLL runtime; the private composite
alone may probe GCC/MinGW and c2x. Rust records its rustc release, edition 2024
and MSVC ABI. The facade does not
benchmark public `w run` or claim general Windows support. `benchmark update`
is intentionally stricter: it requires clean-HEAD commit/catalog/runner
provenance, atomically replaces the catalog file for improving live cells,
regenerates the projection, and consumes the local result on success. Projection
drift after an interrupted two-file replacement is detected by `benchmark
check`. Valid non-improving updates are idempotent no-ops; raw history files are
never written.
The `benchmark` check also builds the C23 native measurement kernel in a
temporary directory and verifies its Windows QPC/Job Object receipt. This
closes the bounded native measurement primitive, including exact raw-stream
oracles and process-tree lifetime. Bun remains the catalog orchestrator, while
production runtime warmup/sample series are sourced from the native receipt;
compile series remain Bun-orchestrated direct-child observations.
`bun check --target compiler` executa uma vez os gates do compilador seed,
ACQ0, OWN0, MAN0, HIR0, HLO0, HLO1 e do `w run` público bounded. O RUN0
interno permanece um gate focal separado (`bun check --target run0`). Os leaves
`root/check:acquisition`, `root/check:owner-guard` e
`root/check:seed-manifest` aparecem uma vez em `root-compiler`; MAN0 fica
imediatamente depois de OWN0. O gate MAN0 atravessa o caminho OWN0 que consome,
mas não repete a suíte OWN0 inteira. `tree-check` e `root-check` recebem os
mesmos leaves por composição. `check:w-cli` continua depois deles como
regressão pública.
`check --target all` mantém a suíte integrada completa. Use os targets
`docs`, `studies` e `benchmark` para escopos menores; os leaves
internos com dois-pontos são resolvidos pelo command registry. `--target`
prioriza esses nomes de suíte; um leaf sem o prefixo, como `hlo0` ou `w-run`,
resolve `check:<leaf>` no registro. `bun check --list` shows only the public
suites; `bun check --list-all` is the maintenance inventory of every internal
leaf.

Não crie um alias equivalente em `tooling/tree-sitter-w/package.json`. O pacote
Tree-sitter mantém apenas comandos locais da gramática:

```sh
bun run --cwd tooling/tree-sitter-w generate
bun run --cwd tooling/tree-sitter-w test
bun run --cwd tooling/tree-sitter-w parse:reference
bun run --cwd tooling/tree-sitter-w parse:std
```

Os comandos repo-wide ficam no command registry. Um comando `parse:*` pode permanecer local
quando o CLI precisa do diretório da gramática.

## Projeções humanas e de máquina

O registro de estudos tem duas superfícies sincronizadas:

- [`STUDIES.md`](../STUDIES.md) é o catálogo humano, agrupado por status, com
  ID, função, path, gate e entrypoint principal;
- [`study-registry.json`](study-registry.json) é o índice de máquina com
  metadata, fixtures, referências, digests, dependências e scripts.

Ambos são gerados por `bun run study:registry`. O writer prepara as duas
saídas e tenta instalá-las transacionalmente, com rollback diante de erros
comuns do sistema de arquivos; isso não promete atomicidade entre arquivos
depois de crash ou perda de energia. `bun check --target study-registry` rejeita
JSON ou Markdown stale. Detalhes de cada estudo ficam no `README.md` local
quando existir e no catálogo gerado. Não mantenha uma segunda tabela manual
neste arquivo.

Outras projeções correntes são:

| Projeção | Gerador ou gate | Função |
|---|---|---|
| [`DIAGNOSTICS.md`](../DIAGNOSTICS.md) | `bun tooling/diagnostic-catalog.mjs --write` | busca humana por código, fatos, fixes e referência normativa |
| [`DESIGN-INDEX.md`](../DESIGN-INDEX.md) | `bun tooling/design-index.mjs --write` | navegação por heading, ID e seção |
| `reference/syntax-atlas/` | `bun tooling/syntax-atlas.mjs --write` | cobertura de snippets e parsing parse-only |

After an intentional change to sources referenced by studies, run
`bun run study:refresh-digests`. The command updates only well-formed digests
for existing repository-contained paths. It installs each dependency wave as
one transaction and repeats until metadata dependencies reach a fixed point.
Missing paths, invalid JSON, duplicates, and cycles fail before each wave.

After changing normative text or classified evidence, run
`bun tooling/refresh-design-freeze-evidence.mjs`. The command updates only mechanical
identities in the freeze classification: ledger text and claim digests,
source/oracle case digests, remaining file digests, and exact `DESIGN.md`
section digests. Case digests use stable-key JSON, so key order alone does not
invalidate evidence. The command fails if decision order or reviewed
classification fields would change. The explicit
`bun tooling/refresh-design-freeze-evidence.mjs --migrate-local-digests` form
is reserved for a reviewed migration from whole-file pins; routine refreshes
do not require that flag.

`bun run hum0:refresh-evidence` applies the same rule to the HUM0 review
protocol. It updates only file hashes and derived stimuli. Symbols, windows,
tasks, prompts, and other human decisions remain unchanged.

`bun run capability:refresh-evidence` refreshes only local file hashes in the
CAP0 capability matrix and regenerates its result snapshot. Capability levels,
routes, gaps, source symbols, and study questions remain reviewed inputs.
| `tooling/*-snapshot.*` | checker de cada unidade | bytes e resultados derivados reproduzíveis |
| `tooling/*-cases.json` | máquina/oracle da unidade | casos de design e barreiras adversariais |

Regenerar uma projeção não transforma sua saída em autoridade. Se uma
projeção divergir, corrija a fonte canônica e depois regenere os derivados.

## Tree-sitter e editores

[`tree-sitter-w/grammar.js`](tree-sitter-w/grammar.js) é a única gramática
sintática mantida neste corte. O corpus em `tree-sitter-w/test/corpus/` e as
queries em `tree-sitter-w/queries/` alimentam parsing incremental, highlights,
locals e folds.

`tree-sitter-w/src/` contém outputs gerados pelo comando `generate`; não edite
essa pasta manualmente. O scanner `src/scanner.c` é authored e versionado.
TextMate e o portal são fallbacks/projeções editoriais; não aceitam um programa
em nome da linguagem.

Para testar a grammar diretamente:

```sh
bun run tooling:install
bun run --cwd tooling/tree-sitter-w test
bun run --cwd tooling/tree-sitter-w parse:reference
bun run --cwd tooling/tree-sitter-w parse:std
```

Os checks de integração permanecem na raiz, por exemplo:
`bun check --target syntax-atlas`,
`bun check --target maintained-parse`,
`bun check --target cheatsheet` e
`bun check --target links`.

## Compiler seed

O seed C é uma implementação caller-owned e incremental de validação. Ele
contém source reader, lexer lossless, scanner C, parser, formatter, frontend
seed, adapter D0, ACQ0, OWN0 e as fatias verificadas HIR0/HLO0/HLO1/RUN0. Consulte
[`compiler/seed-c/README.md`](../compiler/seed-c/README.md) para a superfície
local. Execute `bun check --target compiler` para os gates do bundle.

The C23 route `source → parser → frontend → verified HIR0 → HLO0 → HLO1`
remains limited to the documented subsets and witnesses. The W-1522 primary
native route is independent: `source → parser/frontend → verified HIR0 → MLIR0 →
mlir-opt → mlir-translate → llc → native host link`; HLO0, HLO1, and RUN0 are
bootstrap, audit, and recovery, not prerequisites for that route. The historical
Linux/WSL adapter remains `w-seed-mlir0-15` for its 20.1.2 evidence; the current
pinned Windows adapter is `w-seed-mlir0-16`/`w-seed-mlir0-windows-7` and Native0
is `w-seed-native0-8`; the
private `PROCESS_HANDLER` artifact uses
`w-seed-mlir0-process-handler-1` without changing the `EXECUTABLE` artifact
bytes. MLIR0 also accepts signed-`i64` interpolation with internal Display and
counted-text helpers; HLO0/HLO1/RUN0 remain single-print.
W-1561 adds one structured natural-loop witness: raw MLIR retains
`scf.while`/`scf.condition`/`scf.yield`, and the pinned recipe explicitly runs
`convert-scf-to-cf` plus `convert-cf-to-llvm` before translation. The capability
scope is `unit-structured-cfg-natural-loop`; Linux/WSL `w run` requires exact
`Served 3\n`. The native Windows 23.1.1 route requires the same output. This is
correctness-only evidence; executable metrics remain exploratory and do not
promote a general PGO or performance claim.
W-1563 adds one closed local payloadless enum exhaustive-switch witness. HIR21
retains nominal enum/case identity and canonical edge order; NativeSubset0
derives the private minimum carrier (`i2` for the three-case fixture); raw MLIR
contains `cf.switch` and a backend-only `llvm.unreachable` default. The pinned
native Windows 23.1.1 `w run` gate executes `restaurant-enum.w` with exact
`Courses 10/30/20\n`, empty stderr, and exit zero. The scope of this live
Windows artifact is `unit-structured-cfg-enum-switch`; the i2 sign-bit tag is
printed as `-2` for MLIR 23.1.1's signed textual parser. Payloads, subsets,
general/mixed CFG, public ABI/layout stability, other targets, timing, ranking,
and performance remain outside this correctness-only compiler-lifecycle cut.
The payload successor runs `restaurant-enum-payload.w` on the same Windows
toolchain. It checks enum-returning calls and reordered signed-`i64` captures
through an internal SSA tag-plus-shared-payload carrier. The executable catalog
owns the corresponding C/Rust references and live measurements.
W-1564 advances HIR0 to HIR22 and preserves the function export bit as verified
semantic input. The `restaurant-wmo.w` gate proves only a same-module
executable product closure: retain the used private helper, omit unused
exported/private functions and dead text, then execute exact `Bill 42\n`.
Cross-module graph lowering and complete product-root planning remain gaps.
ACQ0 executa CHK6 em
storage caller-owned, com retry bounded e sem frontend, policy de filesystem ou
CLI. Execute `bun check --target acquisition` para compilar os cinco targets focais,
rodar o CTest ancorado e exigir duas saídas ACQ0 exatas. OWN0 observa e
reconfirma candidates `build.w` em uma sessão guarded sem selecionar owner ou
autorizar fallback; o gate executa Linux nativo e, em host Windows, exige WSL
Ubuntu. O adapter Windows permanece incondicionalmente fail-closed neste
bundle; seus probes são somente diagnósticos. Execute
`bun check --target owner-guard`. RUN0 consome o plano
HLO0 pelo verifier compartilhado em um gate interno, bounded e test-only.
Execute `bun check --target run0` para esse gate. O subset público W-1521 usa
`w run <explicit-path.w> [-- <args...>]` em Linux x86_64 com a rota nativa
explicitamente habilitada, ou o binário Linux por WSL Ubuntu no host Windows;
o basename explícito é uma source identity
opaca, não um identifier de módulo. Execute `bun check --target w-run` para o
produto; a extensão NAT1 é definida por W-1522.

The public Linux gate uses `llc` for PIC program and WRT0 objects and an
absolute native linker for a static PIE. WRT0 supplies `_start`, stdout write,
and exit; the gate rejects `PT_INTERP` and `DT_NEEDED`, so the product acquires
neither CRT nor libc. It generates no C source and does not require Clang.
LLVM version checks remain separate from linker provenance. The older
`check:mlir0` recipe is unchanged.
`bun tooling/command-runner.mjs --command check:w-run -- --ci` requires Linux x64 and the separately acquired
23.1.1 toolchain. Missing prerequisites fail instead of SKIP. Local WSL gates
passed previously with LLVM 20.1.2 and 23.1.0, host GCC/cc 13.3.0, and Bun 1.3.4;
the pinned 23.1.1 Linux archive still requires native execution evidence.
The mandatory Linux and Windows hosted jobs use Bun 1.4.0 and have not run.

ICMP0/W-1537 extends the comparison fixtures for six signed-`i64` operators.
The HIR9/MLIR12/Windows3 labels describe Bool-producing comparisons through
real `llvm.icmp` operations. Native0 stays v6. Six focused C23 suites passed.
The Windows LLVM 23.1.1 public `w run`/`w build` gate passed with MSVC C11
recovery and `/WX` intact. The narrower historical ICMP0 result was produced
with LLVM 23.1.0 and is not silently relabeled.
The Linux/WSL LLVM 20.1.2 gate passed with the explicit GCC 13.3 host link driver.
Both verify exact admission output, signed boundaries, Bool composition, and type rejection.
ICMP0 did not rerun Linux LLVM 23 or the hosted jobs.
Existing syntax, ownership, nesting, stdout limits, and native
recipes remain unchanged. The gates make no timing or cross-target claim.

W-1547 extends `bun check --target w-run-windows` with the bounded public
`std.process` fixture. The gate runs the source with no argument, one normal
argument, and one empty argument; it then builds one PE and executes those same
artifact bytes both empty and nonempty. Exact `missing\n`/exit 2 and
`received\n`/exit 0 outputs, empty stderr, PE x64 identity, and temporary-file
cleanup are required. The reported process PE byte count is diagnostic gate
feedback, not benchmark history.

W-1549 adds the source-backed `restaurant-nested-scalar-if` witness to the
bounded MLIR0 route. Its unparenthesized tail
`return if outer { if inner { open } else { middle } } else { closed }`
normalizes as one scalar value, emits two typed LLVM diamonds and produces
exact `1,2,3\n` with exit zero and empty stderr. The executable catalog records
the workload as public-end-to-end and source/oracle-ready, while independent
C/Rust sources and benchmark runner wiring remain required before performance
readiness.

Isso não é frontend normativo
completo, typechecker, contexto público/geral de aquisição, manifest parsing,
owner selection, backend, linker, runtime ou o runner `w run` geral.

## Estudos e oracles

Leia [`STUDIES.md`](../STUDIES.md) para o inventário dos 73 estudos, seus
status e entrypoints. Cada diretório em `studies/` pode conter corpus, máquina,
oracle, snapshot e documentação local. `bun check --target studies` executa a suíte
agregada; um estudo também pode ter um alias focal na raiz.

Os estudos preservam a separação entre:

1. contrato ou hipótese em `DESIGN.md`/`RATIONALE.md`;
2. casos e máquinas reproduzíveis;
3. evidência de parser, host oracle ou implementação limitada;
4. lacunas explícitas de compiler, runtime, provider, humano ou modelo.

Parsing Tree-sitter, testes Bun, snapshots e gates host não provam execução de
W. Não use um status de estudo como uma promessa de produto.

## Manutenção

Antes de uma mudança, leia `.codex/W.md`, `.codex/W-WORKFLOW.md` e
`.codex/WRITING.md`. Preserve uma única fonte por conceito:

1. altere a fonte canônica;
2. regenere somente as projeções afetadas;
3. execute o menor gate relevante;
4. execute `bun check --target quick` ou `bun check --target compiler` conforme a área;
5. termine com `bun check --target links` e `git diff --check`.

Não edite projeções geradas manualmente, não copie o catálogo de estudos para
outro README e não mantenha aliases duplicados entre o registry e Tree-sitter.
