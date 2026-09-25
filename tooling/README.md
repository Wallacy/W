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
bun check --target gpu0
bun benchmark gpu0
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

## Artifact inspection receipts

`artifact-inspection-receipt.mjs` resolves LLVM inspection tools from `PATH`
when no toolchain option is supplied. On native Windows, pass the root of an
already materialized portable MLIR0 toolchain to use its pinned cache instead;
the command revalidates the materialization and resolves exact, unique tool
basenames from its archive inventory without falling back to `PATH`:

```powershell
bun tooling/artifact-inspection-receipt.mjs build/w-windows/w.exe `
  --toolchain-dir "$env:LOCALAPPDATA/W/toolchains/portable-mlir-toolchain/2026.09.11/x86_64-pc-windows-msvc"
```

The selected executable paths and resolution source are recorded in the
receipt. Repeat `--object` for every emitted object; `llvm-nm.exe` is required
when at least one is supplied. `--allowlists <file.json>` accepts a
`w-artifact-inspection-allowlists-1` policy with independent entries for
post-opt IR externals, undefined object symbols, final PE imports, and final
dependencies. Requested boundaries must pass or the CLI exits with status 2.
The receipt records the policy and digest, but the product gate must still
authenticate its provider authority and target/ABI applicability. ELF symbol
imports and the current partial textual IR scanner remain explicit unknowns,
never inferred successes.

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
GPU0 stays in a separate diagnostic catalog because it is not yet a complete
source-backed W product. `bun check --target gpu0` first proves the versioned W
fixture through Frontend32 and the independently verified target/provider-free
device-module bridge, and the caller-owned provider-neutral projection into
GPU0 records and device artifact. The accelerated binding gate additionally
cross-checks verified ACCBIND0 and GPU0 meanings into one independently
verified ACCREQ0 request whose copied artifact survives producer teardown.
The GPU gate obtains the exact device MLIR, kernel symbol, and expected result
from that ACCREQ0, then proves MLIR GPU/NVVM lowering, CUDA result `42`, and
fail-closed provider cases when the Windows
provider is available. `bun benchmark gpu0` refreshes
only `benchmarks/gpu0-device-linkage-catalog.json` and its concise
`benchmarks/GPU0.md` projection from 101 warmups and 1001 in-process samples;
context/module/allocation setup remains outside the four H2D, dispatch-sync,
D2H, and end-to-end measurements. No binary is retained and these values do
not enter executable or language rankings.
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
`bun tooling/refresh-design-freeze-evidence.mjs`. Classification schema 2 keeps
exact ledger claim equality, ledger count/bounds, section headings, and case
digests without repeating claim hashes, whole-ledger hashes, DESIGN section
hashes, or repository-local source-file hashes. Local source references are
bound by repository-contained paths and uniquely occurring symbols; case
digests use stable-key JSON, so key order alone does not invalidate evidence.
FRC validates classification order, count/bounds, and every canonical claim
against the ledger text directly; its manifest carries no whole-RATIONALE
digest.
The refresh accepts only schema 2 and updates linked claims, current section
headings, and exact source/oracle case digests. There is no legacy migration
mode. It fails if decision order or reviewed classification fields would
change.

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
bootstrap, audit, and recovery, not prerequisites for that route. The current
Linux/WSL adapter remains `w-seed-mlir0-15` with exact 23.1.1 toolchain
evidence acquired from the verified external portable bundle;
the live producer is `w-seed-mlir0-46`, the current Windows label is
`w-seed-mlir0-windows-31`, and Native0 is `w-seed-native0-9`. Historical pinned
toolchain manifests retain the schema they actually validated; the
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
native Windows 23.1.1 `w run` gate executes `enum.w` with exact
`Courses 10/30/20\n`, empty stderr, and exit zero. The scope of this live
Windows artifact is `unit-structured-cfg-enum-switch`; the i2 sign-bit tag is
printed as `-2` for MLIR 23.1.1's signed textual parser. Payloads, subsets,
general/mixed CFG, public ABI/layout stability, other targets, timing, ranking,
and performance remain outside this correctness-only compiler-lifecycle cut.
The payload successor runs `enum-payload.w` on the same Windows
toolchain. It checks enum-returning calls and reordered signed-`i64` captures
through an internal SSA tag-plus-shared-payload carrier. The executable catalog
owns the corresponding C/Rust references and live measurements.
W-1564 advances HIR0 to HIR22 and preserves the function export bit as verified
semantic input. The `wmo.w` gate proves only a same-module
executable product closure: retain the used private helper, omit unused
exported/private functions and dead text, then execute exact `Bill 42\n`.
Cross-module graph lowering and complete product-root planning remain gaps.
W-1575 closes the preceding resolver-complete local-document graph into
verified HIR only. W-1576 adds the internal ProductClosure0 projection after
complete HIR verification. It publishes caller-owned deterministic reachable
and omitted module/function projections and a reachable semantic digest. The
`.default` entry at source index zero in module 0 is the only root. Exported
metadata is not a root. NativeSubset0 accepts the generic bounded
multi-module scalar/local-call path, while MLIR0 performs an independent
cross-check. The app→lib helper is retained, and a synthetic dead module is
omitted with byte-identical MLIR. Enum, switch, pattern, external, process,
effect, service, reflection, FFI, and dynamic-loading families fail closed.
The milestone has `benchmarkDisposition: compiler-lifecycle` and correctness
evidence only. It does not add public multi-file build/run or native
artifact/runtime equivalence. W-1568 remains open.

W-1583 adds Cooperative0 as a compiler-host oracle only. HIR34 selects a
distinct cooperative trace profile and kind for exactly two sibling scalar
async tasks with one or two yields each and a shared 64-function helper-graph
ceiling. Fixed caller-owned frames and a deterministic provider/test-profile
FIFO trace expose reserve/publish, dispatch,
resume, yield, settle, cleanup, outcome commit, join, and release. Independent
checks verify the plan, program counter, queue, frame, lifecycle, and outcome
relations. The bridge does not emit a NativeSubset0 or MLIR0 state machine or
provide a product runtime, scheduler provider, threads, parallelism,
cancellation, I/O, general Task behavior, benchmark, or performance claim.
Normal W-1582 elision remains unchanged. Future product emission remains
target-neutral; currently available Windows/Linux gates do not narrow the
candidate matrix or exclude macOS and other viable LLVM targets.

W-1584 starts with a target-neutral selection boundary. The versioned and reserved
caller-owned `w-seed-cooperative-selection0-1` record carries copied HIR
indices and admission facts only. NativeSubset0 independently
rederives its deliberately narrower one-block subset from verified HIR35:
exactly two ordered scalar async children, one or two yields each, at most 64
reachable functions, a fixed anonymous Unit root, and zero or more post-join
`print` calls. MLIR0 reverifies that proof and emits the distinct
`w-seed-mlir0-cooperative-1` target-neutral `func`/`arith`/`scf` scalar
state-machine core. The C unit exposes
`--emit-target-neutral-mlir`; the pinned MLIR 23.1.1 parser and lowering
pipeline accept its bytes. The core has no triple, data layout, physical
pointer, WRT/process/OS surface, or artifact format. Product process projection,
root output, public execution, runtime/scheduler ABI, benchmark, and platform
support remain gaps. All applicable catalog targets consume the same semantic
core, so Windows/Linux evidence cannot restrict macOS, cross-compilation, or
other targets. M1 selects Bool/signed-`i64`; the current M2 execution emitter is
signed-`i64` only and fails closed for Bool-dependent bodies. A local compiler
request may select one target, while CI/release fan-out can emit every supported
target from the same core; host, emitted-target, and evidence matrices remain
independent.

W-1586 adds the bounded exact-two source-selected `spawn<.main>` product
path; W-1587 is its current bounded cardinality successor.
Frontend25 and HIR35 preserve a distinct main-domain dispatch relation, and
Native0 selects the physical state machine only from verified HIR. Run
`bun tooling/check-cooperative-mlir0.mjs` for Windows and Windows-host Linux
target execution. Run `bun check --target w-run-windows` for the public Windows
`w run`/`w build` path. The executable catalog owns the exact source, oracle,
and separate exploratory Windows and Linux/WSL2 W measurements. The explicit
`bun benchmark run ... --platform linux-wsl-x64` lane invokes public
Windows-host `w build` for the Linux target and executes only the retained ELF
through WSL2; it is same-host diagnostic evidence, not a cross-platform rank.
Runtime measurement stages the ELF and the bounded native measurement helper on
the WSL-native `/tmp` filesystem. Linux `CLOCK_MONOTONIC`, `fork`/`exec`, and
`wait4` samples therefore exclude `wsl.exe` startup and DrvFS access. Compile
latency still describes the Windows-host cross-build, while runtime CPU and
peak RSS describe the root Linux process and do not aggregate descendants.
The WSL environment receipt uses the stable Windows-host physical-memory total
for host partitioning rather than WSL's dynamically provisioned `MemTotal`.
Current evidence cannot narrow required
emission or release fanout for macOS or another applicable target.

W-1587 extends the physical seed `spawn<.main>` path from the W-1586 exact-two
witness to one through four ordered sibling launches in the current product.
Selection uses reserved caller-owned `w-seed-cooperative-selection0-2`; four is
that legacy product record's capacity rather than a language, HIR38, runtime,
scheduler, or ABI bound. HIR38 preserves larger finite verified relations for
later measured consumers. The Cooperative0 trace oracle remains exact-two.
NativeSubset0 rederives and zeroes the selected record; MLIR0 schemas
`w-seed-mlir0-cooperative-2`/`w-seed-mlir0-cooperative-executable-2` emit the
count-driven serial FIFO core and target leaves, starting at ordinal zero and
wrapping at the selected count. Joined signed-`i64` outcomes are folded in
lexical order with one verified leaf per join.

Focused k=1/k=3/k=4 cases complement k=2 with exact
`Dispatched 20\n`/`Dispatched 66\n`/`Dispatched 92\n`; the lower-level
external gate samples k=1 and k=4 endpoints, while the C product path covers
k=1 through k=4. Separately, executable workload
`main-cardinality` owns the public k=4 `w run`/`w build` evidence on
Windows and on the Linux target through WSL2. The bounded product selector
rejects k=5 transactionally while HIR38 retains it; reordered or orphan joins,
mixed launch kinds, and no-route sources fail closed. The primary
`benchmarkDisposition` remains `compiler-lifecycle`; the executable catalog
separately owns exploratory public W measurement, with C23 and Rust blocked
until equivalent serial-main-domain baselines exist. This bounded seed evidence
makes no general runtime, scheduler, parallelism, cancellation, stable ABI,
target-adapter, benchmark-ranking, or performance claim. No syntax changes.

W-1588 adds an IR-only parallel-placement boundary. Frontend28 accepts exact
`spawn<.domain>` only from a caller-owned concurrent domain with the parallel
capability. HIR38 owns and independently verifies the copied identity, mode,
capabilities, pure non-suspending scalar child graph, and lexical joins. The
seed C unit suite covers missing, duplicate, serial, capability-free, mixed,
and forged forms. No MLIR/provider/public executable exists for this route, so
there is intentionally no executable benchmark or platform-performance gate
yet; its disposition is `compiler-lifecycle`.

W-1589 adds the fixed `w-seed-parallel-selection0-1` PARSEL0 proof. The seed C
HIR test derives one, two, and four task selections from verified HIR38,
checks copied placement and lexical launch/join facts, canonical zero tails,
record mutations, HIR lifetime independence, output/input alias rejection, and
transactional failure. Provider capacity is intentionally absent. This is a
compiler-lifecycle gate only; it adds no executable or performance benchmark.
The same gate now admits five tasks into HIR38 and PARSEL1's measured
caller-owned records while proving that fixed PARSEL0 rejects the larger scope
without mutation.

W-1607 extends that focused gate through PARINV1. The test measures exact task
and argument storage, checks declaration-order argument normalization, runs
five-task and seventeen-argument witnesses, and verifies capacity, producer
alias, bridge, digest, and malformed-input barriers. The unchanged PARINV0
failure proves that the compatibility limit did not leak into PARINV1.

W-1608 adds PARPROV1 to the same gate on Windows x64. Five tasks execute at
physical capacities one and two with identical semantic outcomes, and the
capacity-two receipt proves overlap through a monotonic barrier. The gate also
executes the seventeen-argument task and checks exact workspace, short
capacities, producer aliases, and forged outcomes. No public executable or
performance result is added.

W-1609 extends `bun check --target parallel-mlir0` with PARMLIR1. The unit
proves compatible two-task output is byte-identical and covers five tasks,
seventeen arguments, capacities, aliases, and forged producer proofs. The
toolchain gate lowers the five-task target-neutral artifact into Windows x64
COFF and Linux x86-64 PIC ELF objects with MLIR/LLVM 23.1.1. It remains a
compiler-lifecycle check, not a public executable benchmark.

W-1626 extends the focused C23 HIR gate with a real source `panic(...)` child
selected by `spawn<.domain>` beside a scalar child. PARINV1 stores the exact
panic task identity and rejects scalar evaluation without mutating output;
verification covers forged task kind and HIR provenance. This remains a
`compiler-lifecycle` check only: it does not execute a provider or claim
`PanicEvent`, cleanup/lifecycle behavior, public Task/runtime/ABI, a native
product, or a benchmark.

W-1627 extends the focused C23 HIR gate through the separate PARPANIC1 private
bridge. The exact source-selected PARINV1 panic plan crosses the existing
Windows x64 process-local PLATFORM1 authority; the compiler-owned callback
maps scalar values and explicit panic completions without changing PARPROV1,
PARLIFE1, or PARBIND1 semantics. The unit verifies copied source/module
identity and message bytes, exact receipt and authority facts, semantic versus
provenance digest separation across capacities, producer teardown readability,
two panic tasks, no-panic non-publication, pairwise representable writable-range
aliases, and descriptor barriers. This remains bounded `compiler-lifecycle`
evidence only: it does not
claim `PanicEvent`, Task ABI/runtime, native HIR execution, cleanup/teardown,
portability, a public product, or a benchmark/performance result.

W-1628 extends the same focused C23 HIR gate with PANICLIFE1, a target-neutral
private bridge above verified PARPANIC1. The unit checks the caller-owned
decision, exactly three ordered events, copied message, semantic equality
across upstream capacities, provenance separation, rederived no-panic
classification, forged upstream and decision facts, event order/cardinality,
short/null buffers, pairwise representable writable-range aliases, unchanged
outputs, and teardown readability. This is bounded `compiler-lifecycle`
correctness evidence. The upstream execution lane is Windows x64. PANICLIFE1
does not call a provider or own a workspace, and the slice does not claim
PANICBOUNDARY1, resource-registry evaluation, `PanicEvent`, runtime/ABI,
native-HIR execution, cleanup, portability, a public product, or a benchmark.

W-1629 extends the same focused C23 HIR gate with PANICHOSTREG1, a private
Windows x64 host witness above verified PANICLIFE1. The authority creates one
anonymous, non-inheritable Win32 event and transfers ownership to the
caller-owned registry at successful `CreateEventW`. Complete pre-effect
validation precedes one `CloseHandle` call. A true result publishes the
semantic `REGISTERED` to `RELEASED` transition and the ordered events
`RESOURCE_REGISTERED` and `RESOURCE_CLOSE_COMMITTED`. PRE_CLOSE_FAILURE keeps
the event registered for `destroy`. A false or uncertain result after the
real call is terminal `UNCERTAIN` with no retry or double-close. Semantic
record/events are independent of upstream capacity, while provenance records
the authority, generation, and physical close fact. The raw handle remains in
caller-owned C storage but is absent from outputs and digests. Checks cover
real CreateEventW/CloseHandle, capacity equality, multi-panic primary
correlation, forgeries, alias barriers, and replay verification. This is
correctness-only `compiler-lifecycle` evidence. It does not claim a general
registry, arbitrary-handle registration, PANICBOUNDARY1, user cleanup or
OS-object destruction, runtime/public `PanicEvent` or Task ABI, native HIR,
other targets, or performance.

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
LLVM version checks remain separate from linker provenance. The local
`check:mlir0` recipe remains a separate Clang-based evidence gate and uses the
same exact 23.1.1 manifest.
For a new stable LLVM release, probe the exact candidate without changing the
selected manifest: on Linux/WSL run
`W_MLIR0_ACCEPT_VERSION=23.1.2 bun tooling/check-mlir0.mjs`; in PowerShell set
`$env:W_MLIR0_ACCEPT_VERSION='23.1.2'` for that command. The candidate must be
an exact `X.Y.Z` stable version. This gate covers the Clang-based MLIR route,
not the public `llc`/LLD link or a pin promotion. Clear the environment
variable afterward; the default gate still uses the selected manifest.

The manual-only `Build W LLVM target-pack bootstrap` workflow is a separate
upstream acceptance/build lane. Dispatch it with an exact stable upstream tag,
its full resolved commit, and the SHA-256 of `git archive --format=tar <commit>`.
It stages Release development packs on Linux x86_64, Windows x86_64, and
Apple-silicon macOS; the Windows benchmark/C-oracle lane needs both `clang` and
`clang-cl`, so these are included here even though a future compact end-user W
package will exclude them and LLVM command-line tools. Linux and Windows request
X86, AArch64, ARM, RISCV, WebAssembly, NVPTX, and AMDGPU backend support; the
7-GiB arm64 macOS hosted runner requests the first five and records both GPU
backends as omitted for runner capacity.

For example, after independently resolving the tag and calculating the archive
digest from an official `llvm-project` checkout:

```sh
gh workflow run build-llvm-target-pack.yml --ref <W-ref> -f llvm_tag=llvmorg-X.Y.Z -f expected_commit=<full-peeled-commit> -f expected_source_sha256=<64-hex-git-archive-sha256>
```

The workflow re-resolves the tag and checks both supplied values before CMake
configuration. It never chooses a version or digest on the caller's behalf.

The development payload includes LLVM/MLIR headers, static-library targets and
CMake exports, Clang resource headers, `clang`, `clang-cl`, the LLD driver and
its host aliases, plus `llvm-readobj`, `llvm-objdump`, and `llvm-nm` for artifact
inspection. It does not ship Clang/LLD API libraries or headers that W does not
currently consume. LLVM's configure-time distribution check validates each
selected install component for the exact dispatched source revision. The
Windows build uses Ninja with the runner's x64 MSVC environment; Visual Studio
generators are incompatible with this scoped distribution mode.

This is a bootstrap scaffold, not a selected LLVM pin, supported target-pack,
complete dependency SBOM/closure receipt, signature, or reproducibility result.
No run has established that all three hosted builds fit their storage/time
budgets; each job has a six-hour limit, and the arm64 macOS build is serialized
to one compiler job to limit memory pressure. The layout adapts the staged host builds documented by the
[Apache-2.0 portable-mlir-toolchain project](https://github.com/munich-quantum-software/portable-mlir-toolchain);
its files are not copied. The workflow uses upstream LLVM distribution
components for this deliberately scoped development payload.
`bun tooling/command-runner.mjs --command check:w-run -- --ci` requires Linux x64 and the separately acquired
23.1.1 toolchain. Missing prerequisites fail instead of SKIP. On a local Linux
or WSL host, set `W_MLIR0_TOOLCHAIN_ROOT` to the persistent external root
materialized by `bun tooling/acquire-mlir0-ci-linux.mjs`; the checker resolves
`bin/mlir-opt`, `bin/mlir-translate`, `bin/llvm-config`, and `bin/llc` from
that root and never assumes versioned `/usr/bin` names. The portable archive
does not contain Clang; the public runner therefore uses its direct object and
link stages, while the separate Clang recipe requires a Clang-capable root.
The mandatory Linux and Windows hosted jobs use Bun 1.4.2 and have not run.

ICMP0/W-1537 extends the comparison fixtures for six signed-`i64` operators.
The HIR9/MLIR12/Windows3 labels describe Bool-producing comparisons through
real `llvm.icmp` operations. Native0 stays v6. Six focused C23 suites passed.
The Windows LLVM 23.1.1 public `w run`/`w build` gate passed with MSVC C11
recovery and `/WX` intact. The narrower historical ICMP0 result was produced
with LLVM 23.1.0 and is not silently relabeled.
The Linux/WSL LLVM 23.1.1 gate uses the explicit GCC 13.3 host compiler and
GNU `ld` link driver.
Both verify exact admission output, signed boundaries, Bool composition, and type rejection.
The hosted jobs remain separate evidence and have not run locally.
Existing syntax, ownership, nesting, stdout limits, and native
recipes remain unchanged. The gates make no timing or cross-target claim.

W-1547 extends `bun check --target w-run-windows` with the bounded public
`std.process` fixture. The gate runs the source with no argument, one normal
argument, and one empty argument; it then builds one PE and executes those same
artifact bytes both empty and nonempty. Exact `missing\n`/exit 2 and
`received\n`/exit 0 outputs, empty stderr, PE x64 identity, and temporary-file
cleanup are required. The reported process PE byte count is diagnostic gate
feedback, not benchmark history.

W-1549 adds the source-backed `nested-scalar-if` witness to the
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

Antes de uma mudança, leia `REPOSITORY.md` e a seção aplicável de
`CONTRIBUTING.md`. Preserve uma única fonte por conceito:

1. altere a fonte canônica;
2. regenere somente as projeções afetadas;
3. execute o menor gate relevante;
4. execute `bun check --target quick` ou `bun check --target compiler` conforme a área;
5. termine com `bun check --target links` e `git diff --check`.

Não edite projeções geradas manualmente, não copie o catálogo de estudos para
outro README e não mantenha aliases duplicados entre o registry e Tree-sitter.
