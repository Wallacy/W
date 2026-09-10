# WBench/1 and benchmark-driven development

`WBench/1` defines W's benchmark-driven development protocol. It separates
language workloads, compiler lifecycle, and product runtime. BMD1 runs only the
source-backed ready point of the compiler lifecycle as a single series. BMD2
adds a source-backed comparison between two local commits of the same seed.
Neither bundle produces a language or product-runtime result.

### Executable benchmark catalog (M3a)

[`executable-catalog.json`](executable-catalog.json) is the machine-readable
catalog of executable workloads. It keeps stable IDs for `hello`,
`process-handler-lifecycle`, the five
source-backed Restaurant witnesses, and the future full Restaurant
composition. Hello has W, C, and Rust sources. The `restaurant-branch` witness
also has a public `w build` Release source-to-PE candidate plus C and Rust
sources verified against its exact oracle. The other
Restaurant witnesses remain W-only with explicit C/Rust blockers. C is
contextual and non-ranking across its MinGW ABI. Equivalent Hello sources live in
[`executable/`](executable/) and share the exact `Hello, world!\n` / exit `0`
oracle. The shared platform target is `windows-x64`; W and Rust use
`x86_64-pc-windows-msvc`, while GCC C uses `x86_64-w64-mingw32`.
The C c2x fallback is correctness-only and cannot enter promoted C23 ranking;
the current Rust baseline uses edition 2024.

The catalog declares compile latency, run wall time, user/system/total CPU
time, peak working set, artifact size, exit code, and stdout/stderr. A local
`executable-result` retains correctness artifact facts, one warmup, and an odd
set of at least nine raw compile/run samples; summaries are derived from those
samples. Bun's native CPU microseconds and RSS bytes are preserved, including
an explicit disclosure when CPU samples are zero. Each result freezes a
fixed-count, monotonic-clock, fresh-process protocol and a redacted environment;
direct Bun-process CPU/RSS counters do not aggregate descendants. The
arithmetic mean is an integer floor, and the safe host identity is derived from
the normalized redacted environment rather than a hostname or user identity.
The live `bestMetrics` catalog stores only positive lower-is-better cells, never
exit-code/stdout/stderr; zero CPU measurements cannot become best. W execution
and timing are currently public `w build` candidate evidence, not process-tree-
complete timing. Recorded measurement evidence is `exploratory`,
`measurement-only`, and `not-evaluated`; it is not a correctness gate.
`catalog-ready` validates only the catalog contract; `source-and-oracle-ready`,
`bounded-w-demo`, and `not-performance-ready` are separate workload states and do
not claim that a W benchmark is performance-ready.

Each workload declares one machine-checked `structureClass`. `public-end-to-end`
identifies a user-visible workload and its complete executable path.
`integration-linkage` identifies a composite that links implementation pieces
for integration evidence. `transient-internal` identifies an ephemeral
execution descriptor or implementation witness. Hello and Restaurant workloads
use `public-end-to-end`. `process-handler-lifecycle` uses `integration-linkage`,
and its private execution descriptor uses `transient-internal`. The field
identifies the measured subject or intended subject. It does not identify
readiness or completeness.

### M3b executable candidate evidence

[`EXECUTABLES.md`](EXECUTABLES.md) is the generated human-readable projection
of the executable catalog and its compact live best-metrics cells. The
W route for workloads declaring `public-w-build-release` is a public `w build`
Release Windows source-to-PE candidate backed by the external, materialized
MLIR/LLVM/LLD toolchain. It measures the complete build wall interval while
CPU/RSS counters cover only the direct `w.exe` process; child process counters
are unavailable and never aggregated, so compile CPU/RSS is non-comparable to
C/Rust until process-tree accounting exists. Release artifacts must be
sidecar-free. The runner builds `build/w-windows/w.exe` once as a bootstrap
outside sample directories and leaves it retained. A pre-existing bootstrap
may be replaced during that Release build. Sample directories and target EXEs
are removed after each run.
C, Rust and W retained correctness artifacts are checked by a bounded in-process
PE32+ verifier: COFF symbols, CodeView/PDB data, certificate directories,
out-of-bounds sections, overlay bytes and release sidecars fail closed. A
POGO-only debug directory is accepted and measured as linker optimization
metadata, not source-level debug symbols. This cleanliness statement applies
only to new results produced by the current runner. New runner-bound records
carry `artifact.cleanliness` with exact zero counts for COFF symbols, CodeView
entries, sidecars and overlay bytes plus bounded POGO entries and payload sizes
when present. Migrated best cells are explicitly historical/unverified
cleanliness and are not current clean-run evidence.
C and Rust use direct compiler recipes with their declared ABIs. Every route
remains exploratory and measurement-only. W remains contextual/non-ranking
until process-tree accounting makes its compile CPU/RSS comparable. Local
measurements remain ignored under `benchmarks/results/`; only a rerun from a
clean committed HEAD may update the compact catalog, and raw results are
consumed after successful publication.

#### Private process-handler lifecycle executable measurements

The W-1546 `process-handler-lifecycle` workload is a separate executable-catalog lane.
Run `bun benchmark run --target process-handler-lifecycle --language w|c|rust`. Each
private composite combines its handler with the shared C harness and PROCESS0
provider;
`process-entry` remains reserved for a future public autonomous end-to-end
workload and is not a compatibility alias.
correctness checks cover empty and nonempty caller-selected CRT byte vectors
plus six fault cases before timing, and only successful `[alpha,payload]` is
timed. The handler receives `Arguments` and `Context` values but does not read
the arguments. The timed vector tests provider construction and handler
lifecycle, not W-visible argument processing. Runtime timing covers the full shared CRT startup, harness, PROCESS0
provider, and handler path, not handler-only speed. Compile timing spans handler
and support compilation plus the final link, excluding compiler bootstrap.
Catalog artifact size and digest refer to the final GCC-linked
`x86_64-w64-mingw32` PE; source/support closure, recipe, and toolchain
provenance identify the composite, while W and Rust handler COFF origin triples
are disclosed separately as MSVC-origin. The private C/Rust/W recipes pin
shared Release optimization and stripping flags. GCC LTO can optimize the C
handler together with its support; W and Rust cross a native COFF boundary.
Rust fat LTO does not extend across that boundary into the GCC-built support.
Direct-child CPU/RSS counters do not
aggregate descendants. These are descriptive `exploratory`,
`measurement-only`, `not-evaluated` artifact measurements, not language-track
results; W-1546's deferred language comparison does not defer this catalog
work. Published live cells are kept in [`EXECUTABLES.md`](EXECUTABLES.md);
this README does not duplicate measured values. No result or number is claimed
until a validated run exists.

The short facade is `bun benchmark`: use `list` to inspect catalog readiness,
`run --target hello|restaurant-branch|process-handler-lifecycle --language w|c|rust --output benchmarks/results/<new>.json`
for a local candidate measurement, `validate <json>` for a contained result,
`check` for catalog/live-best/projection consistency, and `update <json>` only
from a clean committed HEAD. The runner uses the exact oracle before one
warmup and at least nine odd raw samples for every language. C probes `-std=c23`
then `-std=c2x` and records the accepted standard plus MinGW ABI; its portable
release recipe uses O3, LTO, per-function/data sections, linker section GC,
stripped symbols, and probed `-fwhole-program`. Rust records its rustc release,
edition 2024 and MSVC ABI; its portable release recipe uses O3, fat LTO, one
codegen unit, panic abort, dead-code elimination, `/OPT:REF`, `/OPT:ICF`, and
stripped symbols. The public W build Release route uses
MLIR canonicalization/CSE, llc O3, lld dead-code/identical-code folding and no
CRT. These profiles prioritize runtime performance while removing distributable
symbols; none selects a size-only optimization level or host-specific CPU.
Host tuning is a separate future/local `release-native` category (`-march=native`
for C and `-C target-cpu=native` for Rust), never a portable-cell replacement. W
compile CPU/RSS is non-comparable to C/Rust until process-tree accounting exists.
The catalog's current `release` cells therefore mean portable release. C stays
in standards-only `c23`/`c2x` mode rather than `gnu23`; GNU extensions are not
needed by these sources. PIE/hardening remains a separate artifact-policy axis
until W, MinGW C, and MSVC Rust have an equivalent declared recipe, so the
portable comparison does not add `-fpie` to only one ABI.
Publication atomically replaces the live catalog file for lower values in
matching category/metric cells, then replaces the derived projection. A crash
between those two files is detected as projection drift by `benchmark check`.
Valid non-improving updates are idempotent no-ops, and successful updates
consume the local result.

O programa BMD1 fica em [`program.json`](program.json). O schema fica em
[`wbench-1.schema.json`](wbench-1.schema.json). O manifesto do seed fica em
[`seed-check-lifecycle.manifest.json`](seed-check-lifecycle.manifest.json).
Os descriptors [`seed-check-graph.json`](seed-check-graph.json) e
[`seed-check-input.json`](seed-check-input.json) são source-backed. O checker
valida os bytes e os digests antes de aceitar o manifesto.

## Perfis

Todo workload de linguagem usa exatamente três perfis:

- `learner` contém código correto e plausível de quem transfere patterns de
  outra linguagem e subutiliza W. O perfil não usa sleep, trabalho inútil,
  flags piores de propósito ou bypass.
- `idiomatic` é a forma recomendada para produção. Ele é a métrica primária e
  a base de regressão.
- `frontier` declara o teto de desempenho. O record declara unsafe, FFI,
  target specialization, manual layout, algoritmo e qualquer perda de
  legibilidade.

As lacunas `learner → idiomatic` medem performance cliffs. As lacunas
`idiomatic → frontier` medem specialization burden.

## Lanes

A lane `equivalent` exige o mesmo algoritmo, representação, validação,
numeric contract e input. A lane `open` permite um algoritmo melhor, mas o
resultado não mede a qualidade do compiler. O record deve declarar cada
diferença semântica ou física.

O default usa baselines independentes C/Clang e Rust quando razoável. Uma
exceção registra sua razão. O Computer Language Benchmarks Game é exploratório.
Ele nunca é authority de W.

O catálogo de language reserva 21 unidades de workload. Essa contagem pertence
à track de language. Ela não é a matriz de 27 células do compiler lifecycle.
Cada unidade usa os perfis e as lanes que o manifesto declarar. Uma unidade sem
backend, runtime ou provider permanece blocked. O catálogo fechado e versionado
está em [`language-catalog.json`](language-catalog.json): sete estratos contêm
três IDs cada. `catalog.status: ready` valida a forma do catálogo; não torna as
unidades reservadas prontas para execução.

### Catálogo BMD3 e `byte-scan-view`

W-1490 materializa o catálogo da track `language` e sua primeira unidade
source-backed em [`byte-scan-view.manifest.json`](byte-scan-view.manifest.json).
`byte-scan-view` conta um delimitador recebido em runtime em uma `view Bytes`
binária bounded a 64 MiB e publica exatamente
`{"bytes":"<u64>","matches":"<u64>"}`. Os casos determinísticos incluem
empty, boundaries 15/16/17 e 64/65, ASCII, UTF-8/mixed binary, dense, sparse e
no-matches; criação do input fica fora de timing futuro. O oracle host é
independente, bounded e completo para a operação. `oracle.status: declared` é
o contrato do catálogo, enquanto `readiness.oracle: host-ready` registra a
evidência corrente.

As fontes W `learner` e `idiomatic` são lane `equivalent`; `frontier` é lane
`open` somente pela estratégia física SIMD declarada. C23 e Rust são referências
de correção independentes sem ranking agora; C11 é somente recovery explícito;
após equivalência, podem ter papel
de comparação independente com toolchain e recipe fixos. O baseline primário e
a regressão futura continuam sendo W histórico. O checker usa CMakeLists
versionado e `rustc --edition=2021`, valida stdout/exit completos e rejeita
falhas de toolchain presente; ausência de toolchain é `SKIP` explícito. Não há
execução W, timing ou result W.

## Compiler lifecycle

O seed usa a fixture source-backed
`reference/last-light/checker_bootstrap.w`, símbolo
`export fn canAcceptOrder(`, que o `w check` público valida sem imports de
`std`. O manifesto fixa os digests de source, graph e input. A matriz tem
27 células. Ela cruza os cenários `clean`, `no-op` e `edit` com os estágios
`check-end-to-end`, `source`, `lex`, `parse`, `semantic`, `hir`,
`lowering`, `codegen` e `link`. `startup` e `execution` pertencem a
product-runtime e não aparecem nessa matriz.

Somente `clean × check-end-to-end` está ready. No-op e edit são blocked por
`incremental-cache`. Os estágios source, lex, parse e semantic são blocked
por `stage-instrumentation`. HIR, lowering, codegen e link são blocked pelos
componentes homônimos. Não chame wall time externo de tempo de estágio interno.
O manifesto usa `languageProfiles.applicability: not-applicable` porque esta
track não compara os três profiles de source.
No compiler lifecycle, C/Clang e Rust são baselines contextuais e non-ranking.
A regressão primária futura usa W histórico com recipe equivalente.

O corpus mantém `benchmark_app.w` como matriz source-backed para futuros
workloads de composition do Restaurant. Essa matriz é blocked por
runtime/provider. Ela não cria três variantes artificiais do app e não é o
workload do runner BMD1.

## Runner BMD1 e comparação BMD2

Use um output path explícito. Crie o parent e execute o runner. O CLI recusa
overwrite:

```text
mkdir benchmarks/results
bun tooling/benchmark-driven-development-runner.mjs --output benchmarks/results/seed-check.local.json
```

O default é exatamente 1 warmup e 9 samples raw. Overrides de `--warmup` e
`--samples` exigem warmup >= 1 e samples raw ímpares >= 9. O runner constrói
`compiler/seed-c` em Release em diretório temporário. Esse build fica fora da
medição. Depois ele executa o `w check` source-backed e exige exit 0 com
stdout/stderr vazios. Cada warmup e cada sample inicia processo novo e usa
monotonic wall clock em ns. O escopo inclui startup do processo e estado de
cache do filesystem e do OS.

O result BMD1 é `exploratory`, `measurement-only` e `single-series`, com
`comparison: null`. Ele preserva a execução de um único seed.

Para BMD2, os dois refs devem ser SHAs completos de 40 hex e existir no
repositório local:

```text
bun tooling/benchmark-driven-development-runner.mjs --baseline <40-hex-sha> --candidate <40-hex-sha> --output benchmarks/results/seed-check-comparison.local.json
```

O runner extrai somente `compiler/seed-c` por `git archive` para diretórios
temporários próprios e faz builds Release independentes com CMake/Ninja fora
da medição. Não usa working tree suja, rede ou worktree Git. Os digests de
commit, closure, artifact, recipe, recipe-class e toolchain são registrados por
papel. Recipe-class, toolchain e workload divergentes falham antes de samples.
Os dois oracles exigem exit 0 com stdout/stderr vazios antes de warmup e raw.

Warmup usa pelo menos um par, com rounds próprios de `1..warmupPairCount` na
mesma orientação do primeiro round raw. Raw usa número ímpar fixo de pelo
menos nove pares. Cada round executa baseline e candidate uma vez. A ordem é
gerada pelo runner com `balanced-paired-interleaved-sha256-v1`, registrada com
seed, e a máquina recompõe e valida o schedule. O caller não escolhe seed. A
máquina recalcula as estatísticas, deltas candidate-baseline, ppm com sinal,
counts e calibration com `BigInt` e arredondamento explícito; ela valida também
o workload corrente e a consistência entre as identidades de papel duplicadas.
O runner deriva a proveniência de archive, build, artifact, recipe e toolchain e
executa os oracles. Um result isolado não permite à máquina recomputar essa
proveniência nem reexecutar o oracle.

O result BMD2 é `exploratory`, `comparison-only`, lane `equivalent`, cenário
`clean`, estágio `check-end-to-end` e `verdict: not-evaluated`. Ele não é claim
de performance. Regression continua bloqueada por
`managed-regression-runner`, que exige provider controlado, repetição,
uncertainty e threshold. O record valida antes da publicação e os controles de
ruído conhecidos e desconhecidos ficam explícitos.

Outputs são evidência local explícita. Não rastreie automaticamente os arquivos
gerados. O diretório `benchmarks/results/` é ignorado. O runner recusa target
existente e publica somente um JSON completo por operação atômica fail-if-exists.

## Metodologia externa

As referências abaixo são evidência metodológica sobre medição. Elas não são
autoridade semântica para W:

- [Computer Language Benchmarks Game — how programs are measured](https://benchmarksgame-team.pages.debian.net/benchmarksgame/how-programs-are-measured.html)
- [LLVM — Benchmarking](https://llvm.org/docs/Benchmarking.html)
- [Google Benchmark — User Guide](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
- [Google Benchmark — Random Interleaving](https://github.com/google/benchmark/blob/main/docs/random_interleaving.md)
- [rustc-perf — tests/perf](https://rustc-dev-guide.rust-lang.org/tests/perf.html)

Execute os checks focais com:

```text
bun check --target benchmark
bun check --target bmd:byte-scan
bun check --target bmd:parse
bun check --target bmd:smoke
bun check --target bmd:comparison-smoke
```

O primeiro check é um gate estrutural rápido: valida protocolo, matriz, corpus,
schema e runner host-side, sem compilar baselines de language. O
`check:bmd:byte-scan` é o smoke de correctness separado: cria inputs
temporários, executa o oracle e testa C23/Rust quando os toolchains existem.
Uma toolchain que só aceita c2x recebe disclosure correctness-only e não gera
ranking final C23; C11 exige solicitação explícita de recovery.
`check:bmd:parse` executa o parser Tree-sitter nos três sources W; isso é uma
checagem de forma sintática e não execução W.
O smoke BMD1 constrói o seed e executa uma medição real em diretório temporário.
O smoke de comparação faz HEAD×HEAD com dois builds independentes, um warmup
pair e nove raw pairs, e verifica apenas a estrutura do result sem gravá-lo.
Os checks não publicam resultados no repositório.
