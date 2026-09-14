# W

> Working draft. Joy for humans. Clarity for machines.

W is a general-purpose systems language designed to make intent explicit for
humans and optimization evident to machines. Its engineering goal is portable,
safe, predictable, and measurably efficient native software, with the ambition
to outperform C and Rust in execution time, memory use, and binary size where
W's semantic model enables stronger optimization. Structured concurrency and
parallelism, proof-directed memory, small CRT-free binaries, reproducible
builds, and benchmark-driven development apply across CPU, GPU, server,
desktop, embedded, WebAssembly, and scientific-computing targets.

That is the engineering objective, not a claim about the current product. The
repository contains design contracts, executable specifications, and a
bounded seed compiler. The complete compiler, runtime, SDK, package manager,
and registry are not implemented.

The seed is a real caller-owned C implementation for selected language
surfaces. It has a native lowering path, a local Windows x64 candidate route,
and a bounded Windows-host cross-build route for Linux x64. The evidence is
correctness-scoped unless a source explicitly states otherwise.

## Start here

- [About W](ABOUT.md) gives the short origin, principles, and current state.
- [Design index](DESIGN-INDEX.md) locates sections in the normative design.
- [DESIGN.md](DESIGN.md) is the authority for current language contracts.
- [RATIONALE.md](RATIONALE.md) records reasons, alternatives, and evidence.
- [Repository map](REPOSITORY.md) explains ownership and generated surfaces.
- [Tooling guide](tooling/README.md) documents local checks and utilities.

## Current capabilities

| Surface | Current boundary |
| --- | --- |
| Language design | Contracts cover ownership, automatic memory management, structured concurrency, parallelism, placement, and explicit boundaries. |
| Seed frontend | The seed provides lossless source reading, parsing, formatting, and bounded semantic validation. It is not the complete frontend. |
| Native seed route | A verified HIR slice lowers through MLIR0 to native code for selected values, calls, returns, structured control flow, arithmetic, pre-test loops, and a bounded post-test `repeat`. |
| Virtual structured execution | A closed scalar async child may remain a compiler-only Task relation across finite root `execution#yield()` points and a finite acyclic same-module graph of pure scalar helpers after verified proof. No scheduler or overlap is claimed. |
| Parallel placement IR | Exact `spawn<.domain>` can cross Frontend26 into HIR37 only with caller-owned concurrent-plus-parallel domain evidence. The HIR owns and independently verifies identity, mode, capability, lexical joins, and a pure non-suspending scalar child graph. No parallel provider or execution is claimed yet. |
| Parallel selection proof | PARSEL0 independently derives a fixed caller-owned one-to-four-task selection from verified HIR37, copies lexical launch/join and placement facts, rejects aliases and forged records, and deliberately contains no provider-capacity field. It is not execution evidence. |
| Windows parallel provider | PARPROV0 consumes verified HIR37 plus PARSEL0 through a private scalar callback seam. Capacity one and two produce identical semantic outcomes; a deterministic rendezvous proves two simultaneously active Windows x64 callbacks without a timing threshold. This is component evidence, not a public W executable or CRT-free target artifact. |
| Enum payloads | The current bounded slice supports Bool and signed i64 payloads, captures, constructor values, and exhaustive switches. It has no public payload ABI. |
| Enum subsets | The bounded seed target admits proper nonempty payloadless subsets of local enums with base tags and no wrapper allocation; focused checks and native Windows plus Linux/WSL execution are current. |
| Source entry | entry { ... } and entry(functionName) are accepted in the bounded surface. An empty entry { } is valid. |
| Public CLI | Explicit source paths support w check, bounded w run, and bounded w build on configured routes. Package and workspace resolution are outside this surface. |
| Public process entry | The bounded Windows and CRT-free Linux/WSL routes lower normal verified HIR/MLIR bodies. [`process-input0.w`](compiler/seed-c/fixtures/process-input0.w) is the minimal fixture. [`process-enum-payload.w`](compiler/seed-c/fixtures/process-enum-payload.w) composes helpers, enum payloads, switch, and interpolation. Identity, owner, CFG, stdout, argument bounds, and exit-range proofs remain required. |
| Windows candidate | A local Windows x64 route uses the pinned LLVM, MLIR, and LLD toolchain when its prerequisites are materialized. |
| Benchmarks | WBench records exact-oracle executable evidence and current artifact or timing cells when available. The published status remains exploratory and measurement-only. |

The enum payload carrier is an internal implementation detail. Bool and
signed i64 fields have bounded native representations. Mixed payloads use
aligned scalar lanes sized by the largest case. This does not establish a
stable in-memory layout, pointer tagging, heap boxing, or a public ABI.

Nominal aggregate representation is proof-directed. `struct`, `enum`, and
`object` share aggregate infrastructure, while object syntax keeps reference
and identity-capable defaults. General aggregate materialization remains
design-only beyond the bounded enum and scalar witnesses. No object declaration
or `ref` use implies a heap, header, address, or storage class.

## Current limits

- The seed implements bounded slices, not the full W language or runtime.
- General types, general control flow, async runtime behavior, and provider
  integration remain outside the current product boundary.
- Parallel-domain placement, PARSEL0 selection, and a bounded Windows x64
  provider component are represented and verified. Public build and run remain
  fail-closed until MLIR emits the invocation bridge and Windows plus Linux
  target artifacts prove the same route end to end.
- COOP0 remains a compiler-host trace oracle. A separate bounded cooperative
  core now lowers to Windows/Linux process projections. The Windows host also
  compiles and CRT-free-links the bounded Linux product with the shared WRT0;
  WSL supplies execution evidence only. A bounded `spawn<.main>` source now
  selects this physical state machine through verified HIR and runs through
  public Windows `w run`/`w build`; Windows-host `w build` also cross-builds
  its CRT-free Linux x86_64 ELF for WSL2 execution. This is not a general
  runtime, scheduler, parallelism, or ranked performance claim.
- w run and w build require one explicit source path. w build also requires an
  exact target triple and a new output artifact.
- Public process execution has bounded native Windows x64 and CRT-free
  Linux/WSL x64 candidate routes. They compose only admitted normal-HIR body
  forms and scalar/enum values. General CFG, runtime, and argument-processing
  resources remain outside the current surface.
- Windows execution is local candidate evidence. It is not a supported
  platform claim.
- The platform matrix currently reports zero supported targets. It records
  one evidence-only Linux target and keeps WSL separate from native Windows.
- Feature coverage defaults to every applicable target. Current Windows/Linux
  evidence cannot exclude macOS or another viable target; missing local
  infrastructure remains a blocker. The cross-compilation goal is any
  supported compiler host to any supported emitted target, and requested or
  release target sets may not be silently narrowed to locally executable ones.
- Package, workspace, lockfile, registry, SDK, distribution, and hosted CI
  behavior remain future work.
- General payload types, recursive payloads, niche optimization, and stable
  public enum layout remain future work.
- General enum-subset conversion, payload-bearing subsets, and stable public
  subset ABI/layout remain future work.
- General aggregate materialization and object identity lowering remain
  implementation-evidence gaps.
- Benchmark results do not rank languages or prove product performance.

See [platform support](PLATFORM-SUPPORT.md), [toolchain policy](TOOLCHAIN.md),
and the [seed compiler boundary](compiler/seed-c/README.md) for exact
conditions. Do not infer a broader capability from a bounded fixture.

## Design direction

W keeps these goals separate from implementation claims:

- Automatic memory management should avoid lifetime annotations on the common
  path.
- Structured execution should make concurrency, parallelism, and placement
  explicit at the call site.
- Performance work must establish correctness and semantic equivalence before
  ranking measurements.
- Ownership, provenance, security, and ABI boundaries must remain explicit.

DESIGN.md defines the current contract. Its oracles check bounded projections
against that contract. An oracle is not a compiler, runtime, provider, or user
result.

## Minimal executable

<!-- w-example role=executable use=print observable=effect -->
```w
entry {
  print("Hello, world!")
}
```

Run this fixture after the local Windows host artifact is available:

```text
bun dev run compiler/seed-c/fixtures/hlo0-hello.w
```

The fixture is also the smallest w run demonstration. Its exact source and
other witnesses live in [compiler/seed-c/fixtures](compiler/seed-c/fixtures).

## Setup

Run commands from the repository root.

Required local tools:

- Bun >=1.4.2.
- Git.
- For the native Windows route, Visual Studio, the Windows SDK, CMake, Ninja,
  and the materialized MLIR/LLVM/LLD cache.

Use [dependencies](DEPENDENCIES.md), [toolchain](TOOLCHAIN.md), and
[platform support](PLATFORM-SUPPORT.md) for current prerequisites. These
documents own version and support inventories.

Install the Tree-sitter workspace and run the fast repository checks:

```text
bun run tooling:install
bun check
```

bun check selects the quick suite. It does not replace the compiler, docs, or
benchmark targets.

## Windows development

The Windows facade is native x64 and uses the local development recipe. It
does not download a toolchain during bootstrap.

If the pinned external cache is absent, acquire it explicitly:

```text
bun tooling/command-runner.mjs --command acquire:mlir0-windows
```

Build and validate the retained development artifact:

```text
bun bootstrap --target host
```

Run the named Hello demo or an explicit source path:

```text
bun demo --target hello
bun dev run compiler/seed-c/fixtures/hlo0-hello.w
```

The underlying executable accepts these bounded public commands:

```text
& build/w-windows/w.exe check compiler/seed-c/fixtures/hlo0-hello.w --json
& build/w-windows/w.exe run compiler/seed-c/fixtures/hlo0-hello.w
New-Item -ItemType Directory -Force build/manual | Out-Null
& build/w-windows/w.exe build compiler/seed-c/fixtures/hlo0-hello.w --target x86_64-pc-windows-msvc --output build/manual/hello.exe
& build/manual/hello.exe
```

The w build output must not exist before the command. The builder publishes
the new artifact without clobbering an existing path.

The internal builder command is useful when the retained w.exe itself must be
rebuilt:

```text
bun tooling/command-runner.mjs --command build:w-windows
```

Use the [tooling guide](tooling/README.md) for profile and receipt details.

## Repository checks

Choose a narrow target when possible:

```text
bun check --target quick
bun check --target docs
bun check --target compiler
bun check --target benchmark
bun check --target all
```

The docs target checks generated documentation projections, dependency
currency, the design index, and links. The benchmark target checks catalogs
and protocols without running workload measurements. The compiler target runs
the bounded seed compiler gates. The all target combines the repository
suites.

Use --list, --list-all, and --dry-run to inspect the available plans:

```text
bun check --list
bun check --list-all
bun check --target docs --dry-run
```

## Benchmarks

[WBench/1](benchmarks/README.md) defines the benchmark protocol.
[EXECUTABLES.md](benchmarks/EXECUTABLES.md) is its generated compact
projection. The projection lists runnable source-backed candidates. Planned
or source-less workloads are not listed.

The runner checks the exact exit code, stdout, and stderr oracle before it
records a measurement. Current records are exploratory, measurement-only, and
not-evaluated. They are not language, runtime, or performance claims.

List and run one local result:

```text
bun benchmark list
bun benchmark run --target hello --language w --output benchmarks/results/hello-w.local.json
bun benchmark validate benchmarks/results/hello-w.local.json
```

Updating the live catalog requires a clean committed HEAD and matching
provenance. A successful update consumes the local result:

```text
bun benchmark update benchmarks/results/hello-w.local.json
bun benchmark check
```

The benchmark documents own exact measurements, runner semantics, and
workload status. This README does not copy those values.

## Canonical sources and references

Read these sources in this order:

1. [DESIGN-INDEX.md](DESIGN-INDEX.md) locates a contract. It is generated and
   does not define semantics.
2. [DESIGN.md](DESIGN.md) defines current language contracts and decisions.
3. [RATIONALE.md](RATIONALE.md) records evidence and alternatives. It is
   complementary, not normative.
4. [Last Light](reference/last-light/README.md) is a rich reference product,
   oracle collection, and source corpus. It is not a claim that the reference
   product executes.

Additional navigation:

- [Syntax atlas](reference/syntax-atlas/README.md) and
  [syntax coverage](reference/syntax-atlas/SYNTAX-COVERAGE.md) show marked
  parse-only examples.
- [W cheatsheet](CHEATSHEET.md) gives editorial usage guidance and trade-offs.
- [Diagnostics](DIAGNOSTICS.md) and [studies](STUDIES.md) index machine-checked
  evidence.
- [Dependencies](DEPENDENCIES.md), [toolchain](TOOLCHAIN.md), and
  [platform support](PLATFORM-SUPPORT.md) own environment facts.
- [Tooling](tooling/README.md) and [benchmarks](benchmarks/README.md) own
  commands, protocols, and generated projections.

The [repository map](REPOSITORY.md) identifies source, generated, and
experimental surfaces. The [portal](portal/README.md) is a frozen visual
prototype and does not define current behavior.

## Contributing, security, and license

Start with [CONTRIBUTING.md](CONTRIBUTING.md). Human, AI-assisted, and
automated contributions are welcome when their results and evidence are
reviewable. [GOVERNANCE.md](GOVERNANCE.md) defines authority and decisions.
[MAINTAINERS.md](MAINTAINERS.md) defines review and maintenance.

Report security issues through the private process in
[SECURITY.md](SECURITY.md). Do not publish sensitive details in an issue.

W is licensed under the [MIT License](LICENSE).
