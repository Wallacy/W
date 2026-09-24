# WBench/1 and benchmark-driven development

`WBench/1` defines W's benchmark-driven development protocol. It separates
language workloads, compiler lifecycle, and product runtime. BMD1 runs only the
source-backed ready point of the compiler lifecycle as a single series. BMD2
adds a source-backed comparison between two local commits of the same seed.
Neither bundle produces a language or product-runtime result.

### Native measurement kernel

WBench uses its native runner as the sole measurement authority. External
timing wrappers may be useful for informal reproduction, but their samples do
not enter catalog results and are not a publication dependency.

Each scenario names the boundary it measures: cold process start, warmed
process launch, or in-process body throughput. The Windows kernel must use a
monotonic high-resolution clock and native process accounting, avoid charging
shell or WSL startup to the executable, retain raw samples, schedule compared
variants in balanced order, and derive p50/p95 only after enough repetitions.
Target, profile, platform, and scenario lanes remain separate.

The receipt owns exact exit/stdout/stderr validation, process-tree CPU, peak
working set and commit, executable sections, artifact size and provenance,
and declared environment controls. Cycle counts require a supported native
counter. A timing result is publishable only after equivalent work and exact
correctness have been established independently.

### Post-product inspection

After an executable passes its exact oracle, inspect the same pinned artifact
while the next language feature advances. For a local PE or ELF product, run:

```sh
bun tooling/artifact-inspection-receipt.mjs <artifact> \
  [--post-opt-ir <optimized.ll>] \
  [--object <product.o> ...] \
  [--allowlists <closure-policy.json>]
```

The read-only JSON receipt separates file size, named section bytes, bytes
covered by declared sections, bytes outside those sections, disassembly
inventory, observed dependencies and optional IR/object externals. Every
supplied object is inspected. An optional
`w-artifact-inspection-allowlists-1` policy independently constrains post-opt
IR externals, undefined object symbols, final imports, and final dependencies;
a requested boundary that is absent, unsupported, ambiguous, or rejected makes
the command exit with status 2. Partial textual IR parsing can reject an
unexpected external but cannot prove closure. Bytes
outside sections are not automatically linker overhead or wasted space.
It is diagnostic input for a generic optimization candidate, not a benchmark
result. Inspect every emitted object, bind the policy to the selected provider
and target ABI in the product gate, and validate the final artifact before
claiming CRT-free output. Raw disassembly and temporary binaries remain local;
keep only the current compact finding and reproducible recipe in Git.

### Executable benchmark catalog (M3a)

[`executable-catalog.json`](executable-catalog.json) is the machine-readable
catalog of executable workloads. It keeps stable IDs for `hello`,
`process-entry`, `process-enum-payload`, `process-arguments-count`,
`process-handler-lifecycle`, current capability-family witnesses, and the
future full Last Light language-tour composition. [`EXECUTABLES.md`](EXECUTABLES.md) is the
generated compact inventory of the current rows and lanes; do not duplicate
its changing counts in prose. Hello has W, C, and Rust sources. Family
witnesses with independent C and Rust sources are verified against exact
oracles; W-only witnesses retain explicit C/Rust blockers. Public C
uses final C23 through Clang and the MSVC ABI; the private handler composite
retains its explicitly contextual GCC/MinGW lane. Equivalent Hello sources live in
[`executable/`](executable/) and share the exact `Hello, world!\n` / exit `0`
oracle. The shared public artifact target is `x86_64-pc-windows-msvc` for W,
Clang C, and Rust. Public C has no silent GCC or c2x fallback. The current Rust
baseline uses edition 2024.

Use neutral capability names for isolated compiler and benchmark examples.
Reserve Last Light product names for sources that participate in that narrative
and live under `reference/last-light`; benchmark fixtures and catalog IDs use
capability names. The former misnamed fixture and benchmark prefixes have been
removed, with no compatibility aliases retained.

Every new or materially changed executable example or benchmark reference must
declare its expected exit code and literal stdout in a compact source comment.
It must also declare expected stderr when stderr is non-empty. The catalog is
the mechanical source of truth, and benchmark tests reject drift between these
source comments, catalog oracles, and generated projections. Argument-dependent
workloads declare every tested argv vector, exit code, and literal stdout in a
compact case list; unchanged legacy sources without local comments remain
validated against the catalog oracle. Some unchanged W fixtures still lack a
source-local expectation marker; that comment coverage is tracked debt, not a
reason to churn unrelated sources or relax the exact catalog-oracle gate.

```text
// Expected exit: 0
// Expected stdout:
// Hello, world!
```

Each commented output line represents that literal line plus `\n`; an empty
line ends the block. Omit `Expected stderr` when stderr is empty.

The `float-strict` witness is `not-performance-ready`. W's current
Windows and WSL artifacts are compile-time-folded semantic/output witnesses,
not algorithmically comparable runtime-float work. C and Rust retain binary32
and binary64 runtime operations as independent correctness references. The
catalog excludes this
workload from live best-metric derivation and equivalent-runtime ranking until
runtime-equivalent W evidence exists.

The `float-integer-rounding` witness is also `not-performance-ready`. A runtime
`args.count` branch selects constant `2.5_f64` (no user arguments) or
`3.5_f64` (one argument), then converts with nearest-even rounding to `i8`.
The catalog's exact-output oracle covers the no-argument default only:
`Rounded 2\n`; the `Rounded 4\n` result for `args=["x"]` is a public fixture
gate only because this float family has no supported multi-case catalog
oracle. Runtime branch selection does not make the float operands runtime data:
they remain constants and can be folded. There is no raw-bit runtime float
ingress or independent C23/Rust boundary oracle, so this row is correctness-only
and makes no timing or ranking claim. The public W gate passed on Windows and
Linux/WSL, so catalog `demoEvidence` is `bounded-w-demo`. The separate
out-of-range conversion fixture remains a focused failure gate, not a benchmark case.

The `numeric-widening` witness is likewise
`not-performance-ready`. It keeps the exact implicit integer-to-float and
binary32-to-binary64 conversion family in one row across return, argument,
binding, mixed arithmetic, mixed comparison, and explicit total-conversion
contexts. C23 and Rust 2024 retain runtime operands while the current W witness
may fold the closed expression graph, so the row is correctness evidence only
and publishes no cross-language ranking.

The `float-bit-representation` witness is also
`not-performance-ready`. It checks exact f32/u32 and f64/u64 bit round trips
for signed zero, infinity, and a quiet-NaN payload; focused compiler tests also
cover subnormals and storage/copy preservation. W supplies foldable literal
inputs while C23 and Rust 2024 retain runtime inputs. The benchmark is deferred
until runtime work is equivalent and makes no timing or ranking claim.

The `checked-integer-arithmetic` witness is
`not-performance-ready`. It covers successful fixed-input checked ordinary and
compound `+`, `-`, `*`, `/`, and `%` over signed and unsigned 8/16/32/64-bit
integers and the current x86-64 `Int`/`UInt` aliases. Its single exact-output
row includes the matching `+=`, `-=`, `*=`, `/=`, and `%=` cases; fixed-width
operators do not create separate benchmark rows. The C23 and Rust 2024
references use volatile and `black_box` runtime operands, while W may fold its
literal call arguments, so they are correctness references, not evidence of
equivalent runtime work. Fault behavior is checked only on the W Windows and
Linux/WSL run gates; the C and Rust success references avoid zero divisors and
signed minimum divided by negative one. This family has no performance ranking.

The `integer-wrapping` witness is `not-performance-ready`. Its
single fixed-input policy matrix covers signed and unsigned `i8`/`u8`,
`i16`/`u16`, `i32`/`u32`, `i64`/`u64`, and the `Int`/`UInt` aliases with
representative wrapping add, subtract, multiply, negate, power, and left-shift
cases. Its exact oracle is
`i8/u8 -128/0\ni16/u16 32767/2\ni32/u32 -2/4294967295\ni64/u64 -9223372036854775808/0\nInt/UInt -9223372036854775808/18446744073709551615\n`.
W may fold these calls in the final artifact. The C23 and Rust 2024 references
retain independent runtime operands. Runtime equivalence is not proven, so
this workload has no performance ranking.

The `integer-comparison` witness is `not-performance-ready`. Its
single fixed-input family covers `==`, `!=`, `<`, `<=`, `>`, and `>=` across
signed and unsigned 8/16/32/64-bit integers and the current x86-64 `Int`/`UInt`
aliases, plus one `u8`-to-`i16` widening call before a signed comparison. The
C23 source uses volatile operands and Rust 2024 uses `black_box`; W supplies
literal call-site values, so equivalent runtime comparison work is not
established. The catalog keeps this as one correctness-only family row with no
performance ranking.

The public catalog keeps one dense benchmark per semantic family. Focused W
fixtures still gate individual operations, widths, and failure paths, but they
do not create separate C/Rust comparison rows. Each family witness should cover
the broadest coherent set of currently executable syntax and resources for its
capability, rather than being constrained by source length. Preserve the same
algorithm, inputs, output, and observable policy in each language. `scope`
describes the behavior actually exercised by that witness; `blockers` and
`blockedLanguages` describe measurement or equivalence barriers. Separate
coverage inventories track selected design separately from native-exercised
behavior. A source-backed witness proves only its listed cases: ready means
that this witness is runnable, not that the design or language is complete.

The `terminal-returns` witness is a correctness-only row in the control-flow
family, not a performance candidate. It covers negative, zero, and positive
returns through an `else if` chain followed by a sequential return, with exit
`0`, stdout `-1,0,1\n`, and empty stderr. Reproduce the public Release `w run`
gates with `bun run tooling/check-w-run-windows.mjs` on Windows and
`bun run tooling/check-w-run.mjs` for Linux/WSL. Both public gates passed that
exact oracle, so the catalog records `demoEvidence: bounded-w-demo`. Independent
C23 and Rust sources reproduce the oracle on Windows, but their hosted runtime
and opaque inputs differ from W's freestanding literal-input route. This row
makes no runtime-equivalence or timing claim and records no best metrics.

`while-break-continue` adds a correctness-only loop witness: it skips index 2,
breaks at index 5, and prints `0,4,8\n` for limits 0, 3, and 9. The public W
build-and-run gates passed on Windows and Linux/WSL, with the Linux/WSL gate
asserting a CRT-free ELF; the independent C23/Rust 2024 references passed the
exact oracle on Windows. C keeps its limits volatile and Rust uses `black_box`,
while W supplies literals that may be folded; runtime equivalence is not
established, so the row makes no timing or ranking claim. Reproduce the W gates
with `bun run tooling/check-w-run-windows.mjs` and
`bun run tooling/check-w-run.mjs`.

`nested-labeled-while` promotes the pinned nested labeled/unlabeled transfer
witness through verified HIR, the typed LLVM CFG, and public W `run`/`build` on
Windows and Linux/WSL. The exact oracle is exit 0, stdout `0,1,3\n`, empty
stderr; the target gates check the Windows Kernel32 allowlist and Linux/WSL
CRT-free static ELF. The C23 and Rust 2024 fixtures are independent correctness
references only. The catalog records `benchmarkDisposition: required`, while
`benchmarkStatus` remains `not-performance-ready`; no timing or performance
result has been added. Refresh measurements only after integration, and do not
rank this witness until runtime closure and equivalence are independently
receipted.

`hello-platform-minimal` is registered and runner-supported for contextual,
non-ranking measurement; the catalog status does not make it an idiomatic
comparison or a language ranking. Source/oracle registration alone is not
evidence that a run has occurred; the catalog now contains current Windows and
Linux/WSL execution cells for its supported lanes. WSL cells remain
same-host diagnostics, not native-Linux or cross-host ranking evidence, and
`demoEvidence` remains separate from measurement status. On Linux/WSL, this
workload is the explicit non-PIE lane: W uses `w build --pie off`, and the
existing C23/Rust freestanding recipes produce ET_EXEC. Its Windows W route
keeps the default link mode unchanged.

`hello-platform-minimal-pie` is a separate Linux/WSL x64 variant using W's
public `w build` route, freestanding C23, and Rust 2024 `no_std`; its current
catalog contains measured W/C/Rust contextual lanes. The C and Rust
recipes use LLD PIE linking with no CRT/libc or dynamic loader. Before runtime
correctness, the runner checks the final ELF itself for `ET_DYN`, no
`PT_INTERP`, no `DT_NEEDED`, a `GNU_RELRO` segment, and a `GNU_STACK` segment
without execute permission; W explicitly builds with `w build --pie on`. The
no-PIE workload requires `ET_EXEC`, no `PT_INTERP`/`DT_NEEDED`, and the same NX
stack check, without requiring GNU_RELRO. Both modes must match exit `0`, exact
`Hello, world!\n`, and empty stderr. The Rust PIE recipe adds `-z now` and
therefore requests BIND_NOW; W and C do not, so this is contextual PIE evidence
rather than a claim that their RELRO binding policies are identical.

Run one language at a time with a fresh output path:

```sh
bun tooling/executable-benchmark-runner.mjs --target hello-platform-minimal --language w --platform linux-wsl-x64 --output benchmarks/results/hello-platform-minimal-w-nopie-wsl.local.json
bun tooling/executable-benchmark-runner.mjs --target hello-platform-minimal --language c --platform linux-wsl-x64 --output benchmarks/results/hello-platform-minimal-c-nopie-wsl.local.json
bun tooling/executable-benchmark-runner.mjs --target hello-platform-minimal --language rust --platform linux-wsl-x64 --output benchmarks/results/hello-platform-minimal-rust-nopie-wsl.local.json
bun tooling/executable-benchmark-runner.mjs --target hello-platform-minimal-pie --language w --platform linux-wsl-x64 --output benchmarks/results/hello-platform-minimal-pie-w-wsl.local.json
bun tooling/executable-benchmark-runner.mjs --target hello-platform-minimal-pie --language c --platform linux-wsl-x64 --output benchmarks/results/hello-platform-minimal-pie-c-wsl.local.json
bun tooling/executable-benchmark-runner.mjs --target hello-platform-minimal-pie --language rust --platform linux-wsl-x64 --output benchmarks/results/hello-platform-minimal-pie-rust-wsl.local.json
```

Both policies remain distinct catalog workloads and result identities. The
existing C/Rust rows retain their non-PIE recipes; the W no-PIE recipe is
separate from W's explicit PIE recipe. WSL results remain same-host
diagnostics, not native-Linux support or a cross-host ranking.

The `uint-bitwise` witness is the representative family executable:
it covers complement, binary bitwise operations, population counts, and
leading/trailing zero counts including zero. Focused W fixtures retain
bit/byte reversal and rotation correctness without separate benchmark rows.
The neutral `integer-shift-semantics` witness combines checked ordinary shifts
with masked left/right and logical-right policies across signed and unsigned
8/16/32/64-bit integers, plus the current x86-64 `Int`/`UInt` aliases for the
ordinary operations. The older `shifts.w` and
`fixed-integer-shift-policies.w` sources remain focused compiler fixtures, not
public benchmark rows. The C23 and Rust 2024 references are correctness oracles
only. The bounded native route does not yet execute the combined W source, and
the separately proven W operations may fold literal inputs, so
runtime-equivalent ranking remains deferred.

The `uint-overflowing-family` witness covers add, subtract,
multiply, negate, and power while printing both wrapped low bits and overflow
flags. The `uint-saturating-policy` witness covers the same five
operations with zero and `UInt.max` boundaries.

All three family rows are `not-performance-ready`: W may fold their fixed
inputs while the C23 and Rust references preserve runtime operands. Their exact
exit/stdout contracts live beside each W/C/Rust source and are checked against
the catalog. Runtime-equivalent ranking remains deferred.

The catalog declares compile latency, median and P95 target-run wall time,
user/system/total CPU time, peak working set, artifact size, exit code, and
stdout/stderr. A local
`executable-result` retains correctness artifact facts, one warmup, and an odd
set of at least nine raw compile samples and 101 raw fresh-process run samples
by default; summaries are derived from those samples. P95 uses nearest rank.
Compile samples preserve Bun's direct-child CPU microseconds and peak working
set. Production runtime samples consume the C23 native kernel receipt: wall
time comes from QPC, CPU user/kernel totals come from the complete contained
Job Object and are normalized to floor microseconds for the current result
shape, and peak working set remains the root process. Job peak committed memory
is validated as a distinct receipt fact and is never called RSS. Results keep
an explicit disclosure when CPU samples are zero. CPU best cells use the
arithmetic mean across 101 runs because short Windows processes are charged in
coarse scheduler quanta; a median can remain zero even when work occurred.
New runner-bound results also retain PE layout evidence from that same
validated artifact: FileAlignment, SectionAlignment, SizeOfHeaders, and each
section's name, VirtualSize, and raw size, as defined by the
[PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#section-table-section-headers). VirtualSize includes section
padding and zero-fill; it is not a useful-instruction count. Historical cells
without this optional metadata remain `not measured`, and missing or ambiguous
`.text`/`.rdata` sections are not fabricated as zero.
Linux/WSL runner-bound results retain the named ELF section `sh_size` values as
decimal byte counts, including `.text` and `.rodata` (and any other named
sections present in the bounded section table). The human projection labels
these as ELF measurements; ELF `.rodata` is not a byte-equivalent replacement
for PE `.rdata`. Historical cells without this optional metadata remain `not
measured`, and missing or ambiguous ELF sections are not fabricated as zero.
The current runtime series is a target-run measurement: each raw sample
creates, executes, and waits for one fresh target process, while production
samples are collected by one native helper batch per warmup/raw series. This
keeps the helper and its host wrapper outside the per-sample wall time. On
WSL2, the distribution is initialized once for the batch, the ELF is staged on
WSL-native `/tmp`, and `wsl.exe` startup plus DrvFS access are outside every
sample. The target process launch remains intentionally included, so these
cells measure product invocation cost rather than in-process body throughput.
The future persistent/body-throughput lane must have a distinct identity and
must never be merged with target-run cells.
The current Windows and Linux/WSL target-run cells are not body-performance
comparisons. Windows includes target process creation, security, Job Object,
scheduler, and accounting; the WSL lane times the Linux executable from a
Linux-native helper inside an already-running distribution with
`CLOCK_MONOTONIC` and `wait4`. Values such as Windows milliseconds and WSL
hundreds of microseconds are therefore expected to differ by platform startup
mechanism. Compare regressions only within the same platform and runner lane.
Each result freezes a
fixed-count, monotonic-clock, fresh-process protocol and a redacted environment;
compile-side Bun CPU/working-set counters do not aggregate descendants. The
arithmetic mean is an integer floor, and the safe host identity is derived from
the normalized redacted environment rather than a hostname or user identity.
The live `bestMetrics` catalog stores only positive lower-is-better cells, never
exit-code/stdout/stderr; zero CPU measurements cannot become best. W execution
and timing are currently public `w build` candidate evidence; runtime CPU now
covers the Job tree while compile CPU/memory remains direct-process evidence.
Recorded measurement evidence is `exploratory`,
`measurement-only`, and `not-evaluated`; it is not a correctness gate. Hello,
branch, natural loop, closed enum switch, bounded same-module product closure,
public process entry, and public process enum-payload are
`exploratory-ready`: each has
equivalent W/C/Rust sources, an exact oracle, and the native runtime
process-tree route. Published cells remain optional evidence rather than the
definition of runner readiness.
Sources with a materialized, runner-supported executable but explicit missing
language equivalents may use `partial-exploratory-ready`; that status permits
measurement of the materialized source without claiming cross-language
equivalence. The generated projection omits planned or source-less workloads,
while retaining source-backed candidates whose recipe is supported by the
runner. `catalog-ready` validates only the catalog contract; `source-and-oracle-ready`,
`bounded-w-demo`, and `not-performance-ready` are separate workload states and do
not claim that a W benchmark is performance-ready.

The replacement measurement kernel is implemented in C23 under
`compiler/seed-c`. On Windows it launches every warmup and sample as a fresh
process inside a kill-on-close Job Object, uses QPC and one deadline for the
complete process tree, drains bounded raw stdout/stderr concurrently, checks
the exact oracle on every run, and publishes caller-owned samples only after
the job is empty. Its JSON receipt separates root-process CPU/peak working set
from aggregate Job CPU/peak committed memory; Job commit is not mislabeled as
RSS. `bun check --target benchmark` builds it with Clang C23 in a temporary
directory, exercises quoting, timeout, descendant completion, capture
overflow, oracle failure, and all-or-nothing publication, validates the
receipt, and deletes the build. Bun remains the catalog orchestrator, but the
production executable runner builds this kernel temporarily, consumes its
versioned receipts for warmup and runtime series, and deletes it with the
measurement directory. Test-only injected runners remain explicitly identified
as `bun-direct-test/1`; they cannot publish.
Production `bun benchmark run` also holds one OS-managed lease derived from the
checkout identity for the complete build-and-run interval. A concurrent run for
the same checkout fails before toolchain setup instead of publishing
scheduler-contaminated samples; the operating system releases the lease if the
runner exits unexpectedly.

Each workload declares one machine-checked `structureClass`. `public-end-to-end`
identifies a user-visible workload and its complete executable path.
`integration-linkage` identifies a composite that links implementation pieces
for integration evidence. `transient-internal` identifies an ephemeral
execution descriptor or implementation witness. Hello, `branch`,
`process-entry`, `process-enum-payload`, and `process-arguments-count` workloads
use `public-end-to-end`.
`process-handler-lifecycle` uses `integration-linkage`,
and its private execution descriptor uses `transient-internal`. The field
identifies the measured subject or intended subject. It does not identify
readiness or completeness.

### GPU0 diagnostic lane

GPU0 deliberately stays outside `executable-catalog.json`: its device request
is source-derived through ACCREQ0, but the current C23 witness and private CUDA
adapter are not a complete source-backed W executable.
[`gpu0-device-linkage-catalog.json`](gpu0-device-linkage-catalog.json) keeps one
current native-Windows diagnostic snapshot, and [`GPU0.md`](GPU0.md) is its
concise human projection. It records temporary adapter/device artifact sizes and
nearest-rank p50/p95 for in-process H2D, dispatch-plus-synchronize, D2H, and
complete round trip. Context, module lookup, and device allocation happen
before warmup and timing. No binary or raw run history is retained.

Use `bun check --target gpu0` for correctness and `bun benchmark gpu0` to
refresh the snapshot when CUDA is available. A missing provider produces a
skip and never fabricates a result. These measurements are compiler-linkage
diagnostics only; they cannot enter W/C/Rust or product rankings until the
canonical W module-contract source reaches a supported provider/product route.

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
out-of-bounds sections, overlay bytes and release sidecars fail closed.
POGO-only debug directories and payload-free PE `REPRO` markers are accepted
as linker optimization/reproducibility metadata, not source-level debug
symbols. This cleanliness statement applies
only to new results produced by the current runner. New runner-bound records
carry `artifact.cleanliness` with exact zero counts for COFF symbols, CodeView
entries, sidecars and overlay bytes plus bounded POGO or REPRO entries and
payload sizes when present. Migrated best cells are explicitly historical/unverified
cleanliness and are not current clean-run evidence.
C and Rust use direct compiler recipes with their declared ABIs. Every route
remains exploratory and measurement-only. Workloads with a complete native
W/C/Rust cell are promotable only after semantic equivalence; incomplete
workloads remain contextual. Compile CPU/RSS stays direct-process and is not a
promoted metric; compile latency covers the complete observed build interval.
Local
measurements remain ignored under `benchmarks/results/`; only a rerun from a
clean committed HEAD may update the compact catalog, and raw results are
consumed after successful publication.

Windows runtime cells currently measure cold process invocation with QPC:
native launch setup, target execution, Job Object quiescence, and bounded
capture are inside each sample. They are not body-throughput measurements. A
future steady lane requires equivalent W/C/Rust adapter artifacts exposing one
stable callable ABI, an untimed handshake and warmup, fixed timed batches, and
an output digest. Its no-op harness cost is reported separately and is never
subtracted from samples; cold and steady receipts remain distinct.

#### Public process-entry executable measurements

The `process-entry` workload is the public end-to-end process contract. Run
`bun tooling/executable-benchmark-runner.mjs --target process-entry --language w|c|rust --output benchmarks/results/<new>.json`. Correctness
executes no arguments, one empty argument, and one payload argument before any
timing; the exact cases are `missing\n` with exit `2` for no arguments and
`received\n` with exit `0` for either argument case, always with empty stderr.
W uses the public `w build` Release route, C and Rust use standalone reference
executables, and Rust does not call shared C support. Runtime samples pin the
`[payload]` vector. The workload remains contextual/non-ranking until
compile-side process-tree accounting exists; no result or best-metric cell is
claimed until a validated measurement runs.

#### Public process-enum-payload executable measurements

The `process-enum-payload` workload is the public end-to-end process composition
witness. Invoke the same runner with `--target process-enum-payload`; correctness
covers no arguments, one empty argument, two ordinary arguments, and three
ordinary arguments before timing, while `[alpha, beta, gamma]` is timed. The
exact oracle reports `arguments-missing count=0 amount=17 over-limit=false\n`
with exit `7` for no arguments; the other cases report `arguments-present`, the
argument count, payload amount, and over-limit flag with exit `0`. Stderr is
empty for all cases. Runtime input flows through functions, enum payloads,
branches, and interpolation; scalar replacement is permitted.

#### Public process-arguments-ordering executable measurements

The `process-arguments-ordering` workload selects compact or extended argument
mode. Correctness covers zero arguments, one empty argument, two ordinary
arguments, and three ordinary arguments; `[alpha, beta, gamma]` is timed. Every
case prints `Argument mode compact: count=N\n` for counts below two or
`Argument mode extended: count=N\n` otherwise, exits `0`, and writes no
stderr. W, C23, and Rust 2024 share the exact four-case oracle.

#### Public process-arguments-count executable measurements

The `process-arguments-count` workload is the full public executable path for
`Arguments.count`. Invoke the runner with `--target process-arguments-count`.
It is a `public-end-to-end` workload, not a linkage or transient-internal lane.
Correctness executes zero user arguments, one empty user argument, and two
ordinary user arguments before timing. Each case must print
`Argument count N\n`, exit `0`, and write no stderr. The timed vector is
`[alpha, beta]`. W, C23, and Rust 2024 use the same Windows x64 MSVC target and
the shared release profiles.

#### Private process-handler lifecycle executable measurements

The W-1546 `process-handler-lifecycle` workload is a separate executable-catalog lane.
After cleanup, configure its private W handler build before benchmarking:

```sh
cmake -S compiler/seed-c -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc
```

This bootstrap is outside the measurements. The runner builds its gate target.
Run `bun benchmark run --target process-handler-lifecycle --language w|c|rust`. Each
private composite combines its handler with the shared C harness and PROCESS0
provider.
The private lane is not a compatibility alias for the public `process-entry`
workload.
Correctness checks cover empty and nonempty caller-selected CRT byte vectors
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
Compile-side Bun CPU/working-set counters do not aggregate descendants;
runtime CPU is Job-tree aggregate and runtime working set is the root PE.
These are descriptive `exploratory`,
`measurement-only`, `not-evaluated` artifact measurements, not language-track
results; W-1546's deferred language comparison does not defer this catalog
work. Published live cells are kept in [`EXECUTABLES.md`](EXECUTABLES.md);
this README does not duplicate measured values. No result or number is claimed
until a validated run exists.

#### Enum-payload executable registration

`enum-payload` is a fixed-input, end-to-end executable witness, not
an isolated enum-layout or dispatch microbenchmark. It constructs payloads,
reorders named arguments and switch captures, and prints the exact
`Bills 32/44/10/7\n` oracle. The C23 reference uses a tagged union and the Rust
2024 reference uses an enum with payload fields; all three sources declare the
same inputs and arithmetic under language-specific portable MSVC-target release
recipes. Constant folding is allowed by this scope. A runtime-driven enum
workload would be a separate future witness. Live measurements belong to
[`EXECUTABLES.md`](EXECUTABLES.md), without an isolated dispatch ranking.

`enum-bool-payload` follows the same fixed-input end-to-end policy.
Its boolean and scalar payload variants, reordered named fields, and reordered
captures are one executable contract with the exact
`States true/false/false/true; charges 17/31; licensed true\n` oracle. The C23
tagged union and Rust 2024 enum preserve those inputs and results; this target
does not claim a runtime-only enum-layout ranking. Live measurements belong to
[`EXECUTABLES.md`](EXECUTABLES.md), without a timing or ranking claim here.

#### Async-join executable registration

`async-join` launches two explicit `async fn` scalar calls, joins
both results, and prints `Prepared 42\n`. The compiler proves an ordinary
direct entry for each never-suspending body, so the physical Task carrier is
erased. Its C23 and Rust 2024 references call the same scalar `prepare`
function sequentially. The workload measures virtual structured-task elision
overhead and does not claim overlap or concurrency.

`async-yield` advances that lifecycle lane through two finite root
`execution#yield()` points in each scalar child. Verified HIR keeps both
suspension markers, then the closed product selects a legal immediate-resume
schedule and erases the transient Task relation and both yields. C23 and Rust
use the same sequential scalar work.
The exact oracle is `Prepared 88\n`; this remains representation and compiler
lifecycle measurement, not scheduler, fairness, overlap, or concurrency
ranking.

W-1582 extends this same workload relation to a finite acyclic same-module
graph of ordinary synchronous pure scalar helpers. The updated fixture calls
`stage`. HIR33 re-proves graph locality and cycles before NativeSubset0 and
MLIR0 emit ordinary scalar calls and erase Task and yield markers. The Windows
product gate must be rerun after the source change. This remains a
`compiler-lifecycle` workload with no concurrency, fairness, scheduler,
overlap, or Linux claim.

`main-dispatch` is the first non-elidable physical Task witness.
Two `spawn<.main>` children preserve serial FIFO main-domain dispatch and print
exact `Dispatched 88\n`. The current W cell measures the bounded public product
only. C23 and Rust remain blocked until equivalent main-domain baselines exist,
so the row does not claim scheduler quality, parallelism, or cross-language
performance ranking. Windows and Linux/WSL measurements are separate platform
evidence and are never combined into one ranking.

`main-cardinality` exercises the current upper endpoint of that
seed route with four `spawn<.main>` children, lexical joins, and exact
`Dispatched 92\n`. The implementation ceiling of four is caller-owned bounded
storage, not a language limit or runtime ABI. The public `w run`/`w build`
gates cover the source path on Windows and the Linux target through WSL2; the
lower-level cooperative gate samples cardinalities one and four externally
while its C product path validates every cardinality from one through four.
C23 and Rust remain blocked until equivalent serial-main-domain baselines are
defined, so this workload is also non-ranking across languages.

The short facade is `bun benchmark`: use `list` to inspect catalog readiness,
`run --target <runnable-catalog-id> --language w|c|rust --output benchmarks/results/<new>.json`
for a local candidate measurement, `validate <json>` for a contained result,
`check` for catalog/live-best/projection consistency, and `update <json>` only
from a clean committed HEAD. The runner uses the exact oracle before one
warmup, nine odd compile samples, and 101 odd fresh-process run samples by
default. `--compile-samples`, `--run-samples`, or the shared `--samples` alias
may override the bounded odd counts. Public C requires Clang with final
`-std=c23` support and the MSVC target; its portable release recipe uses O3,
full LTO, per-function/data sections, LLD dead-code/identical-code folding,
the MSVC DLL runtime, no CodeView/PDB data or COFF symbol table, and only
payload-free REPRO metadata. `-fms-runtime-lib=dll` is material: without it
Clang links the static UCRT and a trivial PE grows even when section GC works.
The DLL-runtime artifact is not a self-contained distribution-size comparison
with CRT-free W, so runtime dependencies remain part of artifact policy and
provenance.
Rust records its rustc release,
edition 2024 and MSVC ABI; its portable release recipe uses O3, fat LTO, one
codegen unit, panic abort, dead-code elimination, `/OPT:REF`, `/OPT:ICF`, and
stripped symbols. The public W build Release route uses
MLIR canonicalization/CSE, llc O3, lld dead-code/identical-code folding and no
CRT. These profiles prioritize runtime performance while removing distributable
symbols; none selects a size-only optimization level or host-specific CPU.
Host tuning is a separate future/local `release-native` category (`-march=native`
for C and `-C target-cpu=native` for Rust), never a portable-cell replacement. W
compile CPU/RSS is not a promoted cross-language metric. Runtime CPU uses the
same native Job-tree protocol for W, C, and Rust. The catalog's current
`release` cells therefore mean portable release. C stays
in standards-only `c23` mode rather than `gnu23`; GNU extensions are not needed
by these sources. PIE/hardening remains a separate artifact-policy axis; the
dedicated `hello-platform-minimal-pie` workload records that lane separately
instead of changing the existing C/Rust non-PIE recipes.
Publication accepts one or more result paths and atomically replaces the live
catalog plus its concise human projection. Passing all W/C/Rust results from a
single clean HEAD avoids stale provenance between updates. A crash between the
two generated files is detected as projection drift by `benchmark check`.
Valid non-improving updates are idempotent no-ops, and successful updates
consume the local result.

## Manual reproduction

The supported path runs the oracle, one discarded runtime warmup, nine
fresh-process compile samples, 101 fresh-process runtime samples, artifact
inspection, and cleanup. The reproducible full-catalog wrapper runs eligible
Windows lanes serially, plus all six contextual `hello-platform-minimal`
Windows/WSL W/C/Rust lanes and the three Linux/WSL-only
`hello-platform-minimal-pie` lanes. It keeps the runner's full default sample counts,
validates every result, updates the catalog once, and publishes only a latest
successful-suite receipt. A missing WSL/toolchain or failed lane aborts without
publishing a success receipt. WSL remains host-specific diagnostic evidence and
is never pooled with Windows. The suite receipt preserves each lane's observed
toolchain identity, recipe, and toolchain provenance digest; compiler versions
may differ by lane. It does not automatically change `demoEvidence`.

```powershell
bun tooling/executable-benchmark-suite.mjs
```

`--platform windows-x64` and `--platform linux-wsl-x64` are optional filtered
runs; their receipts and generated summary are explicitly marked filtered, not
full-suite. The timer includes lane build/run, result validation, catalog
update, and catalog/documentation checks. It excludes the final receipt and
projection write to avoid self-referential timing. Measure this full tier
before proposing any distinct faster CI tier; do not reduce samples in the
full-suite command.

For an individual lane, the supported runner path is:

```powershell
bun benchmark list
bun benchmark run --target enum-switch --language w --output benchmarks/results/enum-w.local.json
bun benchmark run --target enum-switch --language c --output benchmarks/results/enum-c.local.json
bun benchmark run --target enum-switch --language rust --output benchmarks/results/enum-rust.local.json
bun benchmark update benchmarks/results/enum-w.local.json benchmarks/results/enum-c.local.json benchmarks/results/enum-rust.local.json
bun benchmark check
```

For a manual single build, first create the Release W compiler, then build and
run the same source-to-PE route:

```powershell
bun tooling/build-w-windows.mjs --profile release
New-Item -ItemType Directory -Force build/manual-benchmark | Out-Null
build/w-windows/w.exe build benchmarks/executable/enum.w --target x86_64-pc-windows-msvc --output build/manual-benchmark/enum-w.exe
& build/manual-benchmark/enum-w.exe
```

The equivalent portable comparison recipes are:

```powershell
clang -std=c23 -O3 -flto=full -ffunction-sections -fdata-sections -fuse-ld=lld -fms-runtime-lib=dll -Wl,/Brepro -Wl,/OPT:REF -Wl,/OPT:ICF -Wl,/INCREMENTAL:NO -Wl,/DEBUG:NONE benchmarks/executable/enum.c -o build/manual-benchmark/enum-c.exe
& build/manual-benchmark/enum-c.exe
rustc benchmarks/executable/enum.rs --edition=2024 -C opt-level=3 -C lto=fat -C codegen-units=1 -C panic=abort -C debuginfo=0 -C strip=symbols -C link-dead-code=no -C link-arg=/OPT:REF -C link-arg=/OPT:ICF -C link-arg=/INCREMENTAL:NO -C link-arg=/DEBUG:NONE --target=x86_64-pc-windows-msvc -o build/manual-benchmark/enum-rust.exe
& build/manual-benchmark/enum-rust.exe
Remove-Item -LiteralPath build/manual-benchmark -Recurse -Force
```

The C command requires a Visual Studio x64 developer environment. The runner
captures that environment once, then invokes Clang directly so compiler CPU/RSS
remain attributable to the measured child. The W backend
recipe is `mlir-opt --verify-each --canonicalize --cse`, then
`llc -O3 -filetype=obj -mtriple=x86_64-pc-windows-msvc`, then `lld-link` with
`/entry:mainCRTStartup /subsystem:console /nodefaultlib /machine:x64 /opt:ref
/opt:icf /incremental:no`. These arrays are canonical in
[`../tooling/executable-release-recipes.mjs`](../tooling/executable-release-recipes.mjs).

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

The corpus keeps `benchmark_app.w` as a source-backed matrix for future full
product-composition workloads. That matrix is blocked by runtime/provider
support. It does not create three artificial app variants and is not the BMD1
runner workload.

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
- [How to Correctly Compare Program Versions on Windows](https://comcomponent.com/en/blog/2026/03/16/002-windows-benchmark-comparing-program-versions/)

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
