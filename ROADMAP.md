# W implementation roadmap

This is the ranked active implementation queue. It is ordered by dependency
and learning value, not by feature visibility. Each item must end in one
bounded, executable observation that becomes an input to the next item.
Completed work belongs in the current capability documents and Git history,
not in this queue.

## Execution principles

- Treat the general language design as the approved implementation baseline.
  Reopen syntax or semantics only when a concrete implementation, executable
  witness, portability requirement, or measured cost exposes a real gap; do
  not schedule open-ended completeness reviews ahead of lower-ranked work.
- Use C's practical systems-programming reach as the minimum expressiveness
  floor: freestanding code, explicit ABI/layout, raw memory, atomics, MMIO and
  target-owned assembly/platform adapters must remain expressible without
  making their unsafe contracts implicit. Libraries and platform SDK coverage
  are implementation/ecosystem work, not reasons to enlarge core syntax.
- Prefer vertical source-to-native slices over isolated infrastructure.
- Generalize only after a bounded implementation exposes the real invariants.
- Keep logical semantics independent from storage, worker, and seed capacities.
- Preserve verified HIR as the authority and lower native W source directly
  through MLIR/LLVM. A C23 compiler, CMake, Bun, and external MLIR/LLVM tools
  may support development and bootstrap only.
- Keep the initial packaged compiler within `<=64 MiB` compressed. Prioritize
  Release performance over size and require benchmark evidence for size work.
- Preserve the cross-target goal: any supported compiler host can emit any
  supported target without downloading a cross toolchain.
- Default native products are CRT-free and statically close reachable W-owned
  runtime and standard-library code. `wrt: .static(.auto)` selects from the
  signed target pack; separately distributed dynamic WRT requires an explicit
  exact provider selection.
- Keep target environment, WRT linkage and CRT provider selection separate,
  and keep them orthogonal to optimization. `crt: .auto` resolves to no CRT
  without a declared transitive requirement; otherwise it requires one exact
  target/ABI-compatible provider. `crt: .none` enforces a CRT-free product. A C
  ABI or `fn<lang: .c>` declaration does not imply CRT. Auto resolution never
  authorizes an optimizer-synthesized CRT symbol; absent an exact selected
  offer, closure fails. Debug, release, benchmark, size, sanitizer and PGO modes
  never widen either dependency axis implicitly.
- Optimize the largest proved closed graph; retain only observable product and
  ABI roots.
- Elide a declared execution relation only with an independently verifiable
  equivalence fact; compare the optimized path against the physical reference.
- Measure public executable behavior only after an exact correctness oracle.
- Keep W's native benchmark kernel authoritative on every host. On Windows,
  measure the declared boundary with a monotonic high-resolution clock and
  native process accounting; separate process cold start, warmed process
  launch, and in-process body throughput instead of hiding them in one number.
  Preserve raw samples, use balanced run order, and derive p50/p95 only after
  enough repetitions. Exact output, CPU, memory, sections, cycles, artifact
  provenance, and environment controls stay in the same auditable receipt.
- Treat performance, memory, binary size, and compile latency as persistent
  optimization signals, never as permission to change semantics.

### Runtime closure and optimizer discipline

LLVM is allowed to rewrite implementation strategy, but not product authority.
It may recognize a scalar loop as a libc operation or lower wide arithmetic,
atomics, floating math, stack growth, unwind, sanitizers, or profiling into
helper calls that did not exist in W source or pre-optimization IR. Every native
route therefore validates three successive closures: external declarations in
post-opt IR, undefined symbols in each object after code generation, and final
imports/`DT_NEEDED` or equivalent dependencies after linking. Each symbol must
belong to the selected reachability-closed WRT, target SDK/provider, a declared
CRT requirement resolved to an exact offer, or an explicit provider selection.
Successful linking is not sufficient evidence.

Use the target-neutral object-reference receipt to select the smallest WRT
startup closure after code generation: a product with no reachable process-
argument accessors must not retain argc/argv capture, while an unknown receipt
selects the complete closure. Do not infer this from filenames, imports, or a
host-specific ELF parser; the same target product must be selected from any
supported compiler host. Keep static PIE and CRT-free linkage as the default
Linux/ELF Hello policy where supported. PE/Windows uses its target hardening
contract, including ASLR/DEP, rather than ELF PIE. An explicit ELF non-PIE build
is a separate, target-bound experiment for platform limits and later
kernel/firmware work, not a silent replacement for the default. Compare size,
compile latency, and runtime only within equivalent link and dependency
policies. The seed CLI's `--pie on|off` remains a temporary ELF control. Do not
remove it in this bundle; retire it only after an implemented target-aware
`build.w` field drives ELF PIE selection, the resolved target and
`w.build-receipt/1` bind the choice, and focused native Windows ASLR/DEP and
Linux/WSL ELF execution gates pass. The durable setting is independent of
optimization and runtime closure.

The runtime-closure axis is independent from target environment and W program
or toolchain profiles:

- CRT-free is the default closure, with reachability-selected static WRT and
  explicit target SDK/provider leaves. This describes linkage, not whether the
  target environment is hosted or freestanding;
- a selected declared CRT requirement or exact CRT provider binds identity,
  version, link mode, target, allowed operations and dependency receipt. It is
  not a faster release profile;
- instrumentation is build evidence, not a deployable closure. Sanitizer and
  PGO-generate runtimes are explicit and training-only; the final PGO-use
  artifact revalidates its own CRT-free or CRT-enabled closure from scratch.

When an optimizer discovers an idiom such as a NUL-terminated byte scan, the
implementation order is: erase the operation when reachability proves it
unobservable; retain a target intrinsic when one owns the semantics; select a
WRT primitive or explicit target provider; and only then compare an explicitly
selected CRT implementation. `.auto` CRT resolution remains limited to
declared transitive requirements and does not authorize newly synthesized
symbols. The former process-wide libcall-disable flag was temporary
containment and is removed from the bounded seed route after optimized Windows
and Linux execution gates passed with helper-specific no-builtin attributes.
This is not a general closure proof or evidence of optimal lowering. Promotion
requires identical semantics and separate measurements for compile latency,
runtime, memory, file/code/import bytes and dependency closure. Freestanding,
hosted, sanitizer and PGO-training results remain separate benchmark lanes.
The executable catalog and every published result carry an explicit
`runtimeClosure` identity; recipe names and linker flags are not substitutes
for that field. Comparisons and best-cell updates require equal closure
identities. Until this schema migration lands, cross-runtime C/Rust/W rows are
correctness or contextual evidence only, not rankable performance evidence.

Compiler-owned `Arguments`, `Context`, owner records, buffers, helpers and
cleanup exist only when reachable or observably required. A source signature
does not force physical materialization. Cleanup follows materialized resources
and explicit language effects, not a monolithic adapter template. The first
optimization slice closes this rule for unused process inputs, then partitions
process helpers by reachability before adding an opt-in CRT-enabled product
mode.
The same slice introduces a target-product closure receipt distinct from the
seed-compiler receipt. It binds target environment, WRT and CRT selections,
provider implementation and version, target/ABI, both link modes, provider
manifests, allowed and observed dependencies, and a closure digest.

### Product optimization feedback loop

Treat optimization as a concurrent product lane, not as a late cleanup phase.
Once a new executable has passed its exact source-to-native oracle, pin the
source, target, profile, runtime closure and compiler revision. While the next
language family advances, inspect that immutable product's verified HIR,
pre/post-opt MLIR or LLVM IR, object references, final imports, sections and
disassembly. Separate semantic work, reachable WRT code and file-format/linker
overhead; total file bytes alone cannot identify a lowering defect. Publish
one compact current receipt and a ranked actionable finding, or explicitly
record that no safe change was found. Do not retain raw traces in Git.

The same independent pass checks memory/CPU cost, platform hardening,
failure behavior, target portability and relevant upstream toolchain changes.
Release monitoring updates the currency record, but a new LLVM/MLIR capability
enters the selected toolchain only after an exact-source regression gate; a
patch release does not silently change the reproducible evidence pin.
Treat each official stable LLVM release as a prompt acceptance candidate,
including patch releases and future major lines. Keep the weekly release watch
as detection rather than assuming a fixed release date. First inventory one
coherent MLIR/LLVM/Clang/LLD version on each available host and review relevant
release notes; then run a small exact-source parse/translate/object/link/run
smoke with dependency-closure inspection. Promote exact toolchain selectors
only after affected compiler gates and representative product benchmarks pass.
Keep historical receipts immutable and isolate this acceptance lane from an
active compiler edit; a partial host installation is not a reproducible pin.

Put generally useful rewrites at the earliest authority that can prove them:
semantic reachability and effect-aware constant facts before target lowering;
canonical typed HIR/MLIR simplification and dead-code elimination before
object emission; target-specific instruction selection only after ABI and
runtime closure are fixed. An absent process argument or context must compile
away from idiomatic W source without requiring a handwritten unsafe entry.
Do not turn a Hello-only string match into an optimizer pass. Check each new
rewrite against adversarial non-Hello witnesses so eliding an unused value
cannot erase an observable effect, resource release, failure or concurrency
relation. LLVM-discovered libc idioms, including byte scans, are candidates
for a semantically equivalent WRT primitive or target intrinsic, not automatic
permission to link a CRT or evidence that scalar W lowering is optimal.

The audit may proceed beside the next implementation package, but promotes a
rewrite only after debug/release exact output and failure behavior, all three
dependency-closure boundaries, Windows/Linux target behavior where relevant,
and comparable compile latency, runtime, memory and binary/section bytes are
measured. Keep PIE, RELRO and auditability in the comparison policy: removing
metadata or protection to meet a sub-1-KiB target is a distinct product mode,
not proof of a better default compiler. A safe no-change result is valid; an
unproven size target must not block bottom-up language work.

### Target and CPU-profile acceptance

Treat the W 1.0 `.portable` baseline table as a versioned hardware-profile
policy, not as a compiler-host or operating-system default. A hardware row
resolves only the ISA minimum and enabled/disabled features; the product profile
joins it with an independently selected target ABI and `platformContract`. The
product receipt keeps that separation, plus tuning CPU, accepted toolchain
version, runtime closure, and any multiversion map. Every supported target must
select an exact primary minimum; compatibility with older hardware is an
optional separate pack, never an implicit tax on the primary product. The
x86_64 W 1.0 design baseline is selected, but remains an implementation gap:
`.portable` uses x86-64-v3; x86-64-v2 and v1 are admitted only through separately
identified compatibility packs; and x86-64-v4, AVX10, or exact
microarchitecture profiles through explicit distributable/tuned recipes. v4
requires AVX-512 and is not assumed to become a universal modern cutoff. None
of the compatibility packs may lower primary-profile code generation. A
host-tuned benchmark/tooling experiment must
first resolve its observed CPU/features into an exact `.explicit` TargetSpec and
recipe receipt; it is local-only and introduces no public `.native` policy.
Operating-system versions remain independent `platformContract` facts and are
not hardware-profile tracking candidates or ISA proxies. LLVM/MLIR 24+ is a
toolchain acceptance candidate, not a runtime floor. The AArch64 `.portable`
candidate is Armv8.2-A with only its
mandatory features; optional FP16, dot-product, SVE/SVE2, and SME remain
explicit. Android `arm64-v8a` Armv8.0+NEON and other Armv8.0 products are
separate compatibility rows. Wasm SIMD128 is the W 1.0 primary candidate and
scalar Wasm is a compatibility row. GPU floors are exact device/capability
contracts. After x86_64/AArch64, riscv64, loongarch64, powerpc64le, and s390x
remain hardware candidates, each gated by LLVM backend/object, WRT/host adapter,
ABI/sysroot/linker, cross-host build, native execution, and CI evidence.

Acceptance must prove profile resolution and execution, not only that LLVM
accepts a CPU name:

- Check the normalized receipt for exact CPU/features, including disabled
  features, ABI and OS minimum, tuning CPU, toolchain version, runtime closure,
  and (when present) the complete variant-to-feature map.
- Execute a portable artifact on a runner constrained to its baseline. For
  multiversioned hot kernels, force and test both baseline fallback and the
  eligible fast path, verify automatic selection, and verify dispatch is cached.
  Tiny products must show no dispatcher or unreachable variant in emitted IR,
  objects, imports, or closure receipt.
- Keep `.portable`, distributable `.explicit`, and separately identified
  host-tuned benchmark/tooling experiments as distinct benchmark identities
  with separate recipes and receipts. Resolve the host-tuned experiment to an
  exact `.explicit` TargetSpec before building; keep it local-only and do not
  publish it as a distributable result. Keep runtime-closure differences in
  separate lanes as well.

Add runtime-dispatched variants only for measured hot kernels, only at coarse
reachability-closed boundaries, and only with a baseline fallback. Baseline
correctness and closure remain independently tested; dispatch must never raise
the declared `.portable` or `.explicit` ISA minimum.

When W raises a primary hardware baseline, add a new versioned row and retain
the previous one only as a separately selected compatibility pack when viable.
The primary product never carries old-ISA fallback code merely because that
compatibility pack exists. Future ISA levels may become primary after measured
coverage and target evidence; they need no syntax change.

The public Linux `w build --audit-dir` route can compare separately optimized W
and WRT0 objects with a generic whole-product candidate: assemble both to
bitcode, link them, run `opt -O3`, and emit one PIC object. Textual `.ll`
linking remains unsuitable because WRT0 contains target module assembly. The
bitcode route is still CRT-free and freestanding; LTO does not change runtime
closure by itself.

The current GNU ld 2.46 audit covers Hello-minimal plus two larger implemented
families. `process-enum-payload` preserved all argument cases while shrinking
the combined object from 3,904 to 2,832 bytes and the final ELF from 13,080 to
13,008 bytes. `nested-loop-terminal-returns` preserved `-1,1,3\n` while
shrinking the object from 1,624 to 744 bytes and the final ELF from 12,936 to
12,864 bytes. Both LTO products remained PIE with RELRO and a non-executable
stack, with no interpreter, `DT_NEEDED`, post-opt declarations, object undefined
symbols, or dynamic imports. The 72-byte final reductions were under 0.6%:
executable `.text` changed by only -8 and 0 bytes respectively, while
`.eh_frame` and `.got.plt` disappeared. The exact-output checks do not prove
equivalent unwind/debug behavior, and this rerun did not collect controlled
compile or runtime samples.

No optimizer default is promoted from that evidence. Keep combined W+WRT0
bitcode/LTO eligible for larger multi-function/runtime families, profile-guided
work, and products where cross-boundary inlining or specialization can remove
material reachable work. Promotion requires equivalent correctness, closure,
unwind/debug and compile-cost receipts plus a material product benefit. Do not
substitute a Hello-specific syscall rewrite or treat intermediate-object
shrinkage as final-product performance evidence. The local GNU-ld audit remains
distinct from the ranked pinned-LLD catalog lane.

The count-only process-arguments candidate is implemented for the exact
verified-reachability case where the selected entry observes only
`Arguments.count` and no argument bytes or descriptors. Windows retains a
bounded current-adapter quote/count scan; the native Linux x86_64 WRT0 route
derives the count directly from its target-owned kernel-entry `argc`, validates
the total range before subtraction, and performs no `argv` load or pointer
walk. An empty argument still counts as one. Both accept 0..256 user arguments
and reject 257 before output. The full value-observing adapter
remains unchanged. Focused Windows raw-command and Linux/WSL execution plus
post-opt/object/final dependency receipts passed; the count-only Linux object
has no process-items table or `.bss`, versus 6,144 bytes in the general lane.
This is `compiler-lifecycle` correctness evidence only, with no performance
claim or new benchmark row. Windows backslash-before-quote behavior remains
parity with the current adapter, not a claim of complete CRT decoding.

`w build --audit-dir` retains one pinned build's pre/post-opt IR, every emitted
object, and final product in an explicit bounded directory; normal builds and
benchmarks retain their cleanup behavior. The W/WRT0 audit above verified this
surface against both separate-object and combined-bitcode builds. Keep future
inspection separate from ranked benchmark samples, remove raw traces at the
next safe checkpoint, and retain only compact conclusions here.

### Package-centric build root (W-1451 owner replacement)

W-1451 now selects a finite package-centric manifest and host-oracle bundle.
`build.w` contains one or more direct package records and at most one local-only
`build { schema: "w.build/1" }` coordinator. Every package declares an exact
local root excluded from public package identity and remains independently
publishable. Package-authored requirements, profiles, and recipes stay inside
the package record. The coordinator owns exact local selection, resolution
contexts, patches, deployments, target/recipe/profile policy, and lock context;
it cannot weaken package requirements. A single package may omit the
coordinator only when selection, resolution, and deployment are unambiguous;
multiple packages require it. Record order does not alter package recipe or
output identity, and `w build all` is explicit only.

The host oracle covers accepted and rejected root shapes, identity exclusion,
package-order invariance, exact roots, local build-plan separation, feature
contexts, publication patch rejection, Last Light fixtures, atlas examples,
and generated classification. This bundle does not implement the compiler's
manifest reader, package resolver, CLI selection, lock persistence, or
publication receipts; these remain explicit bottom-up implementation gaps.

### Evidence promotion and safety closure

A vertical witness proves only the exact boundary that it executes. Every
capability report must keep these stages distinct: selected design, parser and
frontend, verified HIR, lowering, native product, runtime/provider, and target
evidence. A host oracle, hand-built HIR, private adapter, WSL lane, or passing
backend probe cannot satisfy a later stage. W must not be described as
generally memory-safe, race-free, cleanup-safe, or FFI-safe while the relevant
enforcement remains a design oracle or bounded seed subset.

Deterministic positive and adversarial examples remain the fast development
gate, but safety-critical families require the smallest applicable independent
method before promotion:

- property and differential tests for numeric policies, layouts, codecs, and
  source-to-HIR equivalence;
- grammar-guided source fuzzing plus serialized-record mutation for parser,
  frontend, HIR, metadata, and artifact readers;
- ASan/UBSan and leak checks for the C23 seed where supported, with target-
  appropriate Windows diagnostics and MSan/TSan lanes when their prerequisites
  exist;
- deterministic fault injection at allocation, publication, cleanup,
  cancellation, provider, and artifact-I/O boundaries, proving rollback,
  exactly-once cleanup, and failure atomicity;
- bounded schedule exploration for concurrency, followed by stress/replay and
  race instrumentation on the same W programs;
- native ABI/layout assertions and cross-target execution whenever a claim
  depends on calling convention, endian, alignment, atomic width, or platform
  lifecycle.

These methods are tiered rather than run indiscriminately: focused checks stay
fast; package gates exercise the affected family; sanitizer, fuzz, schedule,
cross-target, and long-running performance lanes run in CI or release
qualification according to risk. More tests are not evidence unless they
observe a distinct failure class or product boundary.

The machine-readable safety-evidence catalog and its focused checker make the
procedure auditable, but a catalog status or safety label is a classification,
not proof, and most independent evidence lanes are not implemented yet. Before
any family is promoted as memory-safe, cleanup-safe, race-free, FFI-safe, or
generally safe, its durable record must bind the source and semantic-family
digests, exact maintained W source-to-native route, compiler/HIR/lowering,
runtime/provider and target identities, and the applicable source, verified-HIR,
lowering, native, runtime, target, negative, sanitizer/fuzz, fault-injection,
schedule, resource-exhaustion, and ABI gates. An unavailable, stale, or
non-maintained-route gate blocks promotion; it is not silently omitted.
Independent C/Rust oracles must state how they avoid their own undefined
behavior. Adversarial and fault evidence must execute the same maintained W
source on the claimed route rather than a substitute harness, private adapter,
or alternate native route.

Safety closes in dependency order rather than by accumulating unrelated green
tests. First execute initialization, move, `ref`/`mut ref`/`inout`, bounds,
destruction, allocator failure, and explicit `Option` absence from real W
source; safe references never acquire a universal `null` state. At every
physical fault boundary, every resource that can survive to that boundary must
be statically discharged or registered exactly once in the teardown registry;
custom-allocator storage, foreign owners/leases, and callback/provider
resources that are unregistered block a cleanup-safe claim. Then close general
typed errors, panic/OOM boundaries, exactly-once cleanup, and external I/O
visibility. Only afterward promote tasks, atomics, locks, cancellation, race
freedom, and reclamation through bounded schedule exploration and native race
instrumentation. FFI/unsafe, ABI/layout, MMIO, interrupts, and assembly need
their own target probes because safe-language evidence cannot validate a foreign
trust boundary. Finally, optimization and code-generation correctness need
debug-versus-optimized differential execution, an independent semantic oracle,
and source/serialized-IR fuzzing. Host-model tests remain useful design oracles
but never promote one of these product claims.

The safety review finds this procedure structurally capable of closing the
known risk classes, but not operationally sufficient yet. A finite test suite
cannot by itself prove general language safety: promotion requires the static
language rule, its real verifier and lowering, plus independent executable
evidence on the maintained product route. Every durable safety record must
state its safe-source assumptions, `unsafe`/foreign exclusions, target and
profile, resource bounds, and any liveness preconditions. Race freedom remains
separate from deadlock, starvation, and provider-health guarantees. Merely
registering a command or preserving fresh metadata is not execution evidence;
the durable receipt must bind a completed run and the exact source, compiler,
runtime/provider, target, profile, and applicable adversarial gates.

### Numeric closure discipline

Numeric implementation proceeds by semantic family, with one canonical set of
type and policy facts shared by frontend, verified HIR, evaluators, product
closure, and MLIR lowering. A family is not complete merely because a private
artifact or an alternate native route supports it while the maintained product
closure rejects it.

Before adding executable wider representations, close runtime-parametrized
public-product execution for the already implemented fixed integers and strict `f32`/`f64`:
typed failure, cleanup, process adaptation, exact output, and equivalent-work
C23/Rust correctness references on Windows and Linux/WSL. Then add `i128` and
`u128` as one complete package spanning literals, arithmetic, bit operations,
shifts, conversions, ABI/layout, serialization, and explicit target fallback or
rejection. Follow with strict `f16`, `bf16`, and `f128`, including encoding,
rounding, NaN, signed zero, subnormal, and W-owned fallback rules. Configured
`f4`/`f6`/`f8` remain rank-11 storage/compute elements; BigInt and BigFloat wait
for the rank-6 ownership, allocator, OOM, and generic-value foundations.

Public performance rows use runtime inputs and family-sized workloads. Cold
process launch and in-process numeric throughput remain separate lanes; a
constant-folded W graph is correctness evidence, not a ranking against a
runtime C or Rust workload.

The numeric review finds the selected semantics strong but the implementation
far from closed. Preserve one canonical representation of numeric identity,
width, signedness, conversion policy, rounding, and target capability across
the pipeline, while keeping each trust boundary's verifier independent rather
than copying ad hoc tables or trusting upstream records. After runtime closure
for current fixed integers and strict f32/f64, compare direct logical-width
LLVM integer types against the seed's i64 carrier, specialize count-only
process roots, partition helper emission by reachability, and memoize shared
reachability/constant facts. Promote an optimization only when runtime,
compile-latency, memory, and binary-size measurements improve without weakening
the exact semantic oracle.

The remaining numeric order is: complete runtime W-389/W-392 policies and
typed failure; close target-width ABI/endian/serialization evidence; carry the
now source-backed `i128`/`u128` identities and 16-byte literals through
operations, native lowering and serialization; then strict f16/bf16/f128. The
current bounded compiler-lifecycle increments establish caller-owned verified
HIR identity/value flow and equal-type scalar-if joins for both `i128` and
`u128`, including arm values above `u64`, then admit same-identity comparisons
and bitwise operations in verified HIR. The second increment does not enable
checked arithmetic, shifts, conversions, ABI/layout, serialization, or a
Native0/MLIR artifact: the helper-based process witness is rejected by the
existing process selector after HIR verification, with its entry facts
`direct_entry=ABSENT` and `suspension=MAY`; ProductClosure's current
downstream type/value shape also rejects these wide operation records. The
next prerequisite is a verified, non-suspending helper-call boundary for the
current process route; do not widen the bounded process CFG to bypass it.
Configured f4/f6/f8 and tensor
packing remain storage/compute work, while BigInt/BigFloat wait for ownership,
allocator, OOM, and generic-value foundations. The bounded
`Arguments.count -> i8` witness now feeds one checked runtime expression
containing `+`, `-`, `*`, `/`, and `%` on public Windows and Linux/WSL product
routes. It advances runtime-operand and product-boundary evidence only, not the
general numeric-core status. The expression keeps every intermediate in range
for its first successful case. W-1653 now closes one narrower fault boundary:
the exact-process, straight-line signed `+`, `-`, `*`, `/`, and `%` route keeps
arithmetic fault distinct from typed conversion failure, reaches compiler-owned
reverse-order release and root finalization, maps the fault to process status
2, and never commits buffered output. This does not yet promote general checked
arithmetic as cleanup-safe. A numeric fault boundary can be promoted only when
the maintained W route also proves exactly-once teardown for every resource
that can reach that boundary; a catalog label or compiler-owned subset does not
cover custom allocators, foreign owners/leases, or callback/provider resources.
The next bounded step is complete: alongside the straight-line signed family
and unsigned `u8` helper, one neutral process witness now returns a checked
signed-`i8` scalar `if` join from a synchronous local helper. ProductClosure,
NativeSubset, and process-aware MLIR admit this added four-block comparison
diamond with exact typed arm edges and one-block result join; existing
straight-line and one-block checked-helper support remains intact, while
nested/wider joins and additional multi-block helper shapes fail closed. Zero
and one user arguments print `Joined -1\n` and
`Joined 0\n`; two reaches arithmetic-fault status 2, and 128 reaches typed
conversion status 1, with both failures keeping stdout/stderr empty after
`Context`-then-`Arguments` release and root finalization. Public Windows and
Linux/WSL `w run` and Release `w build` gates execute the W source on the same
four cases. Independent C23 and Rust 2024 reference binaries are separately
compiled and executed against that oracle matrix; the W public gates do not
execute those references. The frontend/HIR type-identity
rule is family-general for existing signed/unsigned 8/16/32/64-bit facts, but
this process execution remains i8-only. This is compiler-lifecycle evidence
with no performance row; general helper graphs, nested/wider process joins,
shifts/power process faults, and cleanup-safe user arithmetic remain later
ranked work.

The first target-layout evidence slice now binds the exact Windows MSVC x64 and
Linux GNU x64 LLVM data layouts to explicit toolchain identity. It validates
Bool plus integer and floating scalar storage/alignment facts through 128 bits,
and little, big, or selected-target-native serialization lengths. This is
`compiler-lifecycle` evidence only: it does not close target-general layout,
W/C calling ABI, aggregate layout, stable ABI/FFI, or a public executable path.

For each promoted numeric family, run the same operation once through const
evaluation and once through opaque runtime input, then compare results and
failure roles across debug and optimized builds. Floating conversion oracles
use exact input bit patterns rather than locale-sensitive decimal parsing and
cover NaN, infinity, signed zero, subnormal, midpoint, and half-open boundary
cases. SIMD promotion follows scalar runtime closure and requires W-source
scalar, split-vector, and native-vector routes with lane-tail, per-lane fault,
reduction-order, and scalar differential evidence; a host SIMD model alone is
not backend evidence.

The numeric ergonomics review does not justify new core syntax. Exact implicit
widening, explicit named lossy/fallible policies, configured low-precision
formats, and separate bit reinterpretation already form a compact surface.
Improve diagnostics instead: report source/destination width and signedness,
the failed policy, the first invalid range fact, and the exact named conversion
that repairs the call. Performance work stays evidence-driven: direct LLVM
logical widths versus the seed `i64` carrier, reachability-only helper emission,
count-root specialization, cached numeric facts, vectorization, and family-
sized runtime benchmarks. W is not numerically optimal until those comparisons
and the remaining families above are executable across targets.

### C-reach closure rule

C's practical systems reach remains the design floor: W must eventually
perform that work with explicit cost and authority, without source
compatibility or preserving C's weakest surfaces. For this project's first
C-expressivity checkpoint, use the self-hosted W compiler at rank 8 rather
than requiring a separate demonstration in every C domain. Validate the
following substitutions when the compiler or a later real workload exercises
them; their absence from the self-hosted compiler is not evidence that the
capability has shipped:

- replace textual preprocessing with typed constants and `const fn`, generics
  and refinements, availability/target selection, hermetic build transforms,
  and verified foreign-header import. Reopen syntax only if a real workload
  cannot express configuration, conditional selection, or generation through
  those mechanisms;
- replace general `goto` with labelled loops/blocks and an explicit enum state
  machine when control is irreducible. Verified HIR may still contain the
  arbitrary CFG required for efficient lowering;
- permit implicit numeric conversion only when it is total and exact for every
  source value. Narrowing, signedness changes, lossy floating conversion, and
  overflow behavior remain explicit named policies;
- represent runtime-sized local data with bounded storage plus views rather
  than a VLA type, and keep W homogeneous rest parameters distinct from C ABI
  varargs. A foreign variadic call uses a typed wrapper or `c.vaList`;
- prove natural C aggregate ABI directly. Packed records, bitfields, flexible
  array members, and unions first cross as opaque imported storage with typed
  accessors/copy operations. Reconsider a first-class W layout only when a real
  target cannot be served by that boundary or the adapter has a measured cost
  that the compiler cannot erase.

## Ranked queue

| Rank | Increment | Completion boundary | What it enables |
| ---: | --- | --- | --- |
| 1 | Scalar literals and operators | Every designed scalar literal and operator family reaches verified HIR, direct MLIR/LLVM lowering, CRT-free Windows and Linux execution, checked failure or explicit wrapping policy, and an independent C23 oracle; syntax-only coverage does not count | Gives W C-level fine-grained arithmetic, comparison, logical and bit-manipulation capability before higher abstractions depend on it |
| 2 | Bindings, assignment, functions and calls | Mutable and immutable bindings, compound assignment, labelled and positional anchors, ordinary calls, returns and overload resolution execute from exact W source without seed-only rewrites | Establishes reusable computation and a stable value-flow substrate |
| 3 | Structured control flow | `if`/`else`, exhaustive selection, guards, loops, `break`, `continue` and multi-block returns lower to general verified CFG and execute adversarial branch and loop witnesses | Removes straight-line restrictions and provides the control substrate for errors, cleanup and scheduling |
| 4 | Value aggregates and central enums | Tuples, structs, payload enums, exhaustive pattern matching and fixed arrays have verified layout-independent semantics plus efficient target layouts and native witnesses; one C-ABI witness proves natural aggregates and the opaque-wrapper route for packed/bitfield/union/flexible-array storage | Matches ordinary C data modelling while preserving W's enum-first design and an explicit foreign-layout boundary |
| 5 | Modules, imports, generics and specialization | Multi-module calls, labelled imports, generic specialization and closed reachable graphs produce deterministic artifacts; unused private graph nodes disappear | Enables real programs and makes whole-module/product optimization the normal case |
| 6 | Explicit views, borrows, storage and ownership | `ref`, `mut ref`, `inout`, moves, views, explicit storage/allocator choices and deterministic cleanup execute for value and resource-bearing aggregates | Establishes memory safety and cost without requiring automatic lifecycle machinery |
| 7 | Errors and effect composition | Typed `throw`/`try`/`catch`, panic boundaries, cleanup and effect propagation compose over general CFG and resource-bearing values | Makes failure semantics complete before asynchronous propagation is generalized |
| 8 | First self-hosted W compiler | The C-seeded compiler builds a compiler written in W; that W-built compiler builds the same W source again and both products pass the same bounded compiler/executable corpus without invoking a C compiler for W product code. Record exact bootstrap inputs, toolchain and runtime closure, then mark the reproducible transition with a Git tag | Provides the project-specific C-expressivity checkpoint and a W-owned base for subsequent implementation without requiring a separate showcase for every C use case |
| 9 | Tasks and structured concurrency | `async`, `await`, `spawn`, groups, cancellation, deterministic outcomes and cleanup execute over measured caller-owned task records with no language-level child limit | Builds concurrency on the completed value, error and ownership model |
| 10 | Provider-neutral scheduler | Target-neutral ready/task/frame state lowers once; capability-selected providers supply platform primitives and cached topology facts across Windows, Linux, macOS, iOS, Android, WebAssembly and future viable targets | Portable parallel execution without a platform-shaped language ABI |
| 11 | Arrays, matrices, SIMD and accelerator lowering | Static and dynamic collections, views, `@`, vectorization and one real CPU/GPU numerical witness share typed semantics and independent oracles; GPU launch remains a provider concern | Adds scientific and heterogeneous performance after scalar, CFG and ownership prerequisites; this branch can advance beside tasks once those prerequisites hold |
| 12 | Automatic lifecycle and memory optimization | Escape/liveness proofs choose registers, stack, arenas, regions or heap; virtual objects materialize only when identity/escape requires it; automatic cleanup remains semantically deterministic | Adds convenience after explicit ownership is measurable and trustworthy |
| 13 | Incremental compiler, test selection and cross-target distribution | Exact dependency invalidation, risk-relevant gates and reproducible signed LLVM/MLIR/LLD target packs cover the Windows/Linux/macOS baseline; the C seed remains maintainable but is not the primary compiler after the self-hosting checkpoint | Fast human/AI iteration and compact offline cross compilation without hollow green tests |
| 14 | Package, registry, service and sandbox slices | Signed binary-first packages, source fallback, independent verification, one service provider and bounded sandbox execution work against the stable compiler/runtime boundary | Opens the ecosystem without freezing premature compiler internals |
| 15 | UI, native graphics, scientific and proof-mode applications | Promote one real workload at a time through correctness, applicability, resource receipts and benchmark evidence; platform SDK/providers remain outside the language core | Broadens targets from proven primitives instead of speculative abstractions |

The current execution wave has separate ownership and a single integration
order:

| Lane | Bounded package | May advance independently | Integration gate |
| --- | --- | --- | --- |
| CFG proof | Bounded two-loop HIR emission, reachability, dominance, typed carriers and adversarial lineage checks now reach public CRT-free products, including one single-loop conditional early return | Native selection and MLIR consume only the verified graph, without another source-shape recognizer | Broaden beyond the pinned two-loop and single-loop early-return shapes while preserving lexical source equivalence and independent graph proof |
| Product values | Flat two-`i64` tuple and one local immutable two-`i64` value struct reach exact public native execution; independent C23/Rust correctness oracles remain non-ranking references | Source→frontend→measured/emitted HIR→independent verifier→NativeSubset0/ProductClosure0→MLIR/native, with virtual constructors erased when unobservable | Add stable layout/ABI or a new aggregate shape only with its own executable witness; current evidence remains bounded and correctness-only |
| Enum value flow | One local scalar-payload enum crosses one typed value-`if` join, a local call boundary, and an exhaustive switch; Bool/i64 payload domain, with physical representation internal | Verified nominal HIR, NativeSubset0 and MLIR0 route; exact CRT-free Windows and Linux/WSL correctness witnesses | Broaden only with a distinct proved CFG/value boundary; stable layout/ABI, nested aggregates, ownership, loops and general mixed CFG remain open |
| Native output | Exact output storage, Hello-minimal layout and dependency-closure receipts | Product-value work uses an isolated checkout while current products are measured | Refresh every affected live public-product receipt before treating the catalog as current |

Each lane should produce one useful family-level result, not a new executable
for every operator or width. The principal integrates in dependency order,
reviews the combined diff, and runs only gates whose relevant inputs changed.
Disjoint file ownership is insufficient when two lanes rebuild the shared
compiler library: a lane needing reproducible measurements must use an
isolated worktree or an immutable pinned source snapshot as well as its own
build directory. Do not compare an artifact compiled across transient edits
from another lane.
An executor may finish with a documented blocker; that is not a reason to
manufacture a passing product claim or keep it polling.
Graph validity alone cannot attest that a same-typed jump to an ancestor loop
matches the label in the original W source. The source-to-HIR gate must retain
the frontend's lexical target proof; the standalone HIR graph gate proves
reachability, dominance and reducible-loop structure, not typed value
availability or source equivalence by itself.
The graph helper has `compiler-lifecycle` benchmark disposition: measure its
verification cost when integrated, without adding a separate executable
performance row for an internal analysis.
The verifier now caches that analysis once per function while checking values:
dominance can justify a binding read across branches, and analyzed loop headers
can justify their carrier reads. Bounded HIR emission and verification cover at
most two natural loops with typed `i64` carriers, and the pinned nested-loop,
terminal-return and single-loop conditional-early-return shapes now execute as
public native products. General CFG, nested early returns and other carrier
types remain open.

Ranks order the next integration proof, not a prohibition on parallel work. The
fundamental `i8`–`u64` and `f32`/`f64` scalar path must feed ranks 2–3, but
`i128`/`u128`, `f16`/`bf16`/`f128`, and configured low-precision storage need
not all finish before general functions and CFG advance. A runtime W-389 source
chosen by `if` exposed a concrete dependency: the process verifier, selector,
and product closure previously required a first-block, four-block
constant-float split. Frontend, verified HIR, NativeSubset0, ProductClosure0,
and MLIR now accept the exact seven-block `Arguments.count`-selected `f64`
join under `rounding:`; public Windows and Linux/WSL gates execute both arms.
This is bounded composition, not general floating-point CFG or arbitrary
runtime float ingress. This composition belongs with ranks 3 and
7; changing `usize` into `u64` implicitly or treating a constant as runtime
ingress would not close it. Rank 8 is the first self-hosting gate; ranks 9–11
may develop in parallel after their shared ownership, error and CFG
prerequisites. Freestanding raw memory, explicit ABI/layout, atomics,
volatile/MMIO, target-owned assembly and platform adapters remain required
W capabilities, but do not impose a second exhaustive C-parity demonstration
before the ranked queue can advance. Packed, bitfield, union and flexible-array
foreign layouts may retain the opaque typed boundary until first-class layout
is justified. A thin self-contained compiler packaging smoke may begin before
rank 13, while signed cross-target distribution remains its full completion
boundary.

The self-hosting tag is a checkpoint, not a claim that every C program or
platform API already compiles in W. Stage 0 is the maintained C bootstrap;
stage 1 is the W compiler built by that seed; stage 2 is the same W compiler
built by stage 1. The tag records the source revision and reproducible build
recipe only after stage 1 and stage 2 pass the same declared corpus and native
product checks. The C bootstrap can be updated when useful, but new compiler
development proceeds in W after that checkpoint. Neither stage may route W
source through a C compiler as its product backend.

### Parallel delivery lanes

The ranked queue orders integration proofs, not agent occupancy. For a
semantic family, pin one exact source witness and its expected HIR shape early,
then assign disjoint writers to the following lanes when the shared contract
is stable. Each lane has a finite acceptance gate; a green lane does not claim
the end-to-end feature until integration executes the same source and oracle.

| Lane | Owned boundary | Can advance while | Integration gate |
| --- | --- | --- | --- |
| Source semantics | Parser/frontend, typed values, verified HIR and adversarial records | Native code studies the pinned HIR shape | Source-to-verified-HIR, exact errors and failure atomicity |
| Native selection | Product closure, target subset, layout/ABI and dependency requirements | Frontend closes the bounded witness; emitter uses a verified fixture | Independent shape rejection and target/product receipt |
| Code generation | MLIR/LLVM lowering, optimized IR and target objects | Selection closes its separate matcher | Exact output, post-opt/object/final dependencies, debug/release equivalence |
| Runtime and measurement | WRT/provider closure, target adapters, C23/Rust oracles and family benchmark | Compiler lanes produce a correctness fixture | Windows/Linux execution, target-sensitive gates and a comparable benchmark disposition |
| Product optimization audit | Pinned successful product, IR/object/final-artifact inspection and ranked generic rewrite candidate | The next source family advances in an isolated worktree or immutable snapshot | Promote only a semantics-preserving change with closure and cost comparison; otherwise record a no-change result |

Use one semantic family or substantial compiler subsystem per package, not one
operator width or one tiny executable per agent. Cap concurrent writers by
disjoint files, independent build directories and an explicit test-runner slot;
the principal integrates the common witness and reviews the combined diff.
If a needed fact is absent from an upstream receipt, stop that lane and add
the fact at its owner instead of inferring it from filenames, imports or a
target-specific shortcut. Keep rank-8 self-hosting prerequisites visible in
each lane, but do not start a second compiler implementation before functions,
modules, ownership and error flow can support it.

For the active rank-3 tranche, run a dependency graph rather than a
single-file queue. The bounded unlabeled `break`/`continue` pre-test `while`
slice now reaches source-backed verified HIR, independent NativeSubset0
selection, MLIR/LLVM typed CFG, and public `w run`/`w build` CRT-free products
on Windows and Linux/WSL. The shared `while-break-continue.w` witness returns
`0,4,8\n` across false-header, continue, and break paths. The MLIR gate
verifies typed edges and LLVM translation; public gates verify exact output,
exit, and target artifact closure. This is bounded correctness evidence, not
general-loop or performance evidence. The C23/Rust references are correctness
oracles only until their runtime inputs are equivalent to W's.

After that proof, form the next packages around whole semantic families:
general CFG/loops, aggregate-and-enum values, module graph/specialization,
then explicit ownership. Source semantics and native/runtime work may overlap
only after a pinned witness and HIR contract give each writer a stable
boundary. Prefer a larger family package with one integration gate over a
series of width-by-width or one-op commits. Keep at most four executors and
retire each at its finite gate; parallel occupancy is not itself progress.
The next rank-3 cut has three distinct gates. The parser and frontend now
represent nested `while` loops and resolve labeled or unlabeled transfers to
an explicit lexical loop statement; malformed, shadowed, or unresolved labels
fail closed. Verified HIR admits a label only when every labeled transfer
resolves to its exact lexical loop and matches its spelling. NativeSubset0
still selects only the earlier single-loop subset and MLIR0 emits that CFG;
public executable evidence for nested loops is still pending. HIR0
now preflights a bounded per-function source CFG plan: it records lexical loop
parentage and frontend-resolved transfer targets, and its emission passes use
the same deterministic planner. The plan now derives canonical per-frame
signed-`i64` carried-root sets, including descendant writes and empty sets.
A two-loop source proves these planning facts and a forged wrong-loop target
fails closed. HIR0 now emits and independently verifies at most two natural
loops, nested or sequential, with signed-`i64` carrier tuples. The verifier
reconstructs each header, preheader, adapter and exit; proves dominance,
lexical transfers, dense typed lanes and binding lineage; and distinguishes a
loop header carrier from the exit carrier required after an adapted `break`
path. Same-root sibling loops and an adapted child followed by a parent write
have resealed adversarial mutations. Capacity and alias failures remain
all-or-nothing. This is bounded HIR evidence, not a language-level loop limit.
The pinned
[`nested-labeled-while.w`](compiler/seed-c/fixtures/nested-labeled-while.w)
has C23/Rust correctness references. Its functions, three local calls, nested
transfers, interpolation, and entry are already represented by the frontend.
A bare frontend probe correctly leaves `print` unresolved because it supplies
no host scope; the Native0 product route supplies the explicit
`native-process@1`/`Console` prelude. NativeSubset0 now selects the admitted
verified `i64` CFG generically, and MLIR0 emits that HIR graph as LLVM-dialect
blocks and typed carrier edges without reparsing W source or adding a second
source-shape recognizer. ProductClosure0 cross-checks exact reachability. The
focused HIR0/Native0/ProductClosure0/MLIR0 units pass; `check-mlir0.mjs`
verifies, translates, compiles and runs the exact `0,1,3\n` oracle. The public
Windows and Linux/WSL `w run` and `w build` gates also pass that exact output,
empty stderr and zero exit; Windows import checks and Linux/WSL static-ELF
closure checks remain target-specific. `benchmarks/executable-catalog.json`
registers `nested-labeled-while` with `benchmarkDisposition: required` and no
performance results. Its bounded
[`nested-loop-terminal-returns.w`](compiler/seed-c/fixtures/nested-loop-terminal-returns.w)
successor carries that mixed nested-loop graph through post-loop scalar
predicates to three terminal return blocks, with C23 and Rust 2024 correctness
oracles. It uses the existing generic CFG planner, verified-HIR selector,
ProductClosure0 cross-check, and LLVM CFG emitter; public Windows and Linux/WSL
`w run`/`w build` gates preserve exact `-1,1,3\n`, exit 0, and empty stderr,
with the platform-specific import and CRT-free ELF checks. The catalog records
`benchmarkDisposition: required` and no performance result. This remains a
bounded mixed-CFG witness, not arbitrary CFG; future-loop roots cannot flow
backward into earlier predicates, and returns inside loop members remain
rejected. General CFG, arbitrary payload types, optimizer quality, and
cross-target performance remain open. Do not add a second source-shape
recognizer. A standalone, well-typed HIR graph still cannot prove which source
label was written; source equivalence remains owned by the pre-emission lexical
plan, while the independent HIR verifier owns graph and carrier validity. The
next promotion must preserve both proofs.

The next bounded rank-3 slice, `loop-conditional-early-return`, now admits one
direct conditional terminal-return arm inside one natural pre-test `while`.
The exact public witness preserves `13,2,0\n` across early-match, exhausted,
and zero-iteration paths. Its lexical HIR plan remains the source-equivalence
proof; an independent HIR CFG verifier checks the reducible loop, typed i64
carrier/version lineage, successor-free return arm, backedge, and false-edge
exit carrier. NativeSubset0 and MLIR0 use verified HIR only. Windows and
Linux/WSL `w run`/`w build` gates retain CRT-free closure checks, and C23/Rust
are correctness references only. The catalog disposition is `required`, with
`not-performance-ready` and no timing/ranking result. General or nested early
returns, arbitrary CFG, and other carrier types remain open.

W-1654 promotes the bounded product-value slice from verified HIR0 through
public native execution. `flat-aggregate-pair.w` covers an unlabeled structural
`(i64, i64)`: HIR records both component types, a virtual tuple constructor
with explicit element ownership, projections with their own ordinals,
immutable locals, local arguments and return. Tuple identity is structural.
The existing `(u64, Bool)` tuple remains in its isolated legacy representation
because it is a different typed shape; the same source shape is not
represented twice.
`flat-value-struct-pair.w` covers one local nominal value struct with two
`i64` fields: HIR identity is the module/declaration pair, field declarations
retain canonical order, and initializers preserve both source evaluation order
and declaration-field ordinal. Its constructor is a virtual value, never a
call or materialized object. Both fixtures pass source→frontend→HIR0
measure/run→program bridge→independent verification, with resealed mutations
for ownership, ranges, types, identities, ordinals, projections, completeness,
capacity, aliasing, result, and digests. The benchmark disposition is
compiler-lifecycle; W-1654 adds exact, correctness-only native execution, with
no performance result.

The public family witness
[`flat-value-aggregates.w`](compiler/seed-c/fixtures/flat-value-aggregates.w)
exercises one structural tuple and one local immutable value struct with the
same helper-call/return/projection shape. The exact oracle is exit 0, stdout
`7,5,26\n`, and empty stderr on Windows x64 and Linux/WSL x64 through both
development `w run` and Release `w build`. The public route consumes verified
HIR rather than recognizing source text. The Windows Release import table is
exactly the Kernel32 output/exit route. The Linux audit's post-opt text scan
reports `write` with explicitly partial parser coverage; the complete
undefined-symbol inventories are exactly `write` in the product object and
`main` in WRT0, and the final ELF has no interpreter, `DT_NEEDED` entry, or
executable stack. This remains bounded compiler-lifecycle evidence, not a
performance result or stable aggregate layout/ABI claim.

The aggregate slice still does not implement stable layout or ABI/FFI, arrays,
mutable or nested/arbitrary aggregates, or aggregate ownership. The separate
[`enum-cfg-join.w`](compiler/seed-c/fixtures/enum-cfg-join.w) witness now carries
a local scalar-payload enum through one typed `if` join and an exhaustive
switch, but it does not add aggregate support or a public enum layout. ProductClosure0
continues to promote only its exact flat aggregate shapes and remains
fail-closed for the other forms. C23/Rust sources are independent output references only. The standalone
[`flat-aggregate-pair.w`](compiler/seed-c/fixtures/flat-aggregate-pair.w) and
[`flat-value-struct-pair.w`](compiler/seed-c/fixtures/flat-value-struct-pair.w)
continue as focused verified-HIR witnesses; labeled tuple type syntax remains
a separate seed-parser gap.
The [flat-pair witness](compiler/seed-c/fixtures/flat-aggregate-pair.w) pins a
positional two-`i64` tuple, construction, projection, labelled calls, return,
and body entry. The bounded frontend records its unlabeled tuple types,
ordered construction, and projection. The separate
[`flat-value-struct-pair.w`](compiler/seed-c/fixtures/flat-value-struct-pair.w)
witness now proves one immutable nominal two-`i64` value struct through the
frontend: initializer labels may be reordered, while duplicate, missing,
unknown, positional, or wrongly typed fields fail closed; field projections,
local bindings, labelled arguments, and returns retain exact nominal and
ordinal identities. The representation reuses kind-discriminated record
fields and does not increase the fixed Native0 storage footprint. Verified
HIR and native lowering now accept and execute exactly these two flat shapes,
as recorded by the W-1654 public native promotion above. Labeled tuple type
syntax remains a separate seed-parser gap. Their C23/Rust fixtures are
independent output references; native execution does not establish stable
aggregate layout or C ABI, and no performance result exists for this family.

### Active rank 1 closure order

Close the scalar surface in dependency order rather than resuming the later
physical scheduler experiments:

1. canonical signed, unsigned, Boolean and floating scalar identities,
   literals and conversions; W-1641 closes only fixed-width integer
   `truncatingBits:`, W-1644 closes the integer-to-integer `saturating:`
   increment, and W-1645 closes the bounded strict binary32/binary64 identity,
   literal, operator, and native execution family. W-1646 closes the exact
   total integer-to-float and f32-to-f64 widening subset across all expression
   contexts, and W-1647 closes only the existing f32/u32 and f64/u64
   representation-bit round trips. W-1650 adds fixed-width integer
   `try D(exactly: source)` through a typed HIR success/error split and private
   MLIR/LLVM artifact. Frontend73 and verified HIR97 additionally preserve the
   complete bounded `f32`/`f64` to fixed-integer `rounding:` matrix and its
   success/non-finite/out-of-range roles. NativeSubset0 now independently
   rederives that closed relation, and the private
   `w-seed-mlir0-float-to-integer-rounding-1` artifact lowers all 100
   width/mode combinations to deterministic LLVM-dialect text; focused checks
   verify exact bounds and ordering for that complete matrix, while real
   `mlir-opt`/`mlir-translate` gates cover both float widths, signed and
   unsigned destinations, and every rounding intrinsic. HIR97 also admits one
   source-derived constant rounding split in the bounded `native-process@1`
   root and proves reverse owner cleanup on all three outcomes. ProductClosure0
   v4 now projects those three process outcomes and binds them into its
   reachable semantic digest. Process-executable v6 lowers the same bounded
   constant root through LLVM optimization and exact CRT-free Windows x64 plus
   Linux/WSL x64 execution: nearest-even success prints `Rounded 2\n` and exits
   0; out-of-range exits 1 with empty output, after reverse cleanup on both
   outcomes. The print is admitted only when it interpolates the binding
   initialized from the verified normal-edge result. A second, seven-block
   process witness selects `2.5_f64` or `3.5_f64` from runtime
   `Arguments.count`, then executes the joined rounding result on both public
   targets (`Rounded 2\n` or `Rounded 4\n`, exit 0); the operands remain
   constants. A third bounded process witness now converts runtime
   `Arguments.count` exactly to `u64`, round-trips it through `f64.fromBits`
   and `.toBits()`, and observes the result on both public targets; independent
   C23 and Rust references cover the same zero/one-argument bit round trip. A
   fourth source-derived witness converts the canonical quiet-NaN raw bits to
   `i8`, reaches the non-finite typed outcome, exits 1, and keeps stdout/stderr
   empty on both public targets. General runtime float ingress and independent
   non-finite conversion references still lack this route. W-1652 now defines the
   canonical `native-process@1`
   mapping for an unhandled typed error. HIR94 retains the restricted local
   payloadless-error direct throw and composes one exact-conversion binding into
   a three-block process root. ProductClosure0 v4 projects either the direct
   typed outcome or the conversion's distinct normal and typed-error
   successors. It authenticates reverse-initialization cleanup on both
   structured exits. Process-executable v6 materializes that cleanup and
   post-cleanup status adaptation for the exact-conversion root. One canonical
   runtime ingress now carries the distinct logical `Arguments.count: usize`
   identity through HIR under the current x64 seed profile; MLIR selects its
   physical `i64` carrier only after target validation. The same source now
   evaluates a bounded checked runtime expression containing `+`, `-`, `*`,
   `/`, and `%`, plus one checked increment; zero/126 arguments prove exact
   arithmetic output, 127 proves the bounded W-1653 status-2 fault path, and
   128 proves out-of-range status 1 with empty failure output on public CRT-free
   Windows and Linux/WSL routes. This does not prove general overflow cleanup and is
   not general `usize`
   support. The direct-throw and general typed process routes remain
   unimplemented.
   Remaining conversion policies continue to block full rank-1 closure, but
   do not block rank-2 functions or rank-3 general CFG that they now need.
   W-1651 closes the design identity of i128/u128, the fixed
   arithmetic float family through f128, configured f4/f6/f8 AI elements, and
   fixed/dynamic BigFloat. Frontend76 added exact i128/u128 type identities,
   16-byte literal magnitudes, boundary rejection, and immutable binding.
   Frontend78/HIR101 add same-identity comparisons and bitwise operations to a
   verified-HIR-only family slice, including all six comparisons, `&`, `|`,
   `^`, unary `~`, exact signed-minimum construction, and wide helper-result
   joins. Checked arithmetic, shifts, conversions, ABI/layout, serialization,
   and native execution remain open. The process selector rejects the current
   helper-based source shape because the entry's direct-entry/suspension facts
   do not meet its existing boundary; this is not permission to widen the
   bounded process CFG. After the
   current 64-bit/f32/f64 packages, rank 1 takes only fixed scalar work:
   i128/u128 and strict f16/bf16/f128 semantics, including target rejection or
   an explicitly permitted W-owned fallback. Configured f4/f6/f8 remain
   storage/conversion elements and join Tensor/Quantized lowering in rank 11;
   TensorFloat32 remains a compute policy, never a scalar type. BigInt/BigUInt
   and dynamic BigFloat wait for rank-6 ownership, allocator, and OOM semantics;
   fixed-precision BigFloat also waits for generic value parameters and closed
   aggregate layout. This ordering preserves the selected design without
   pretending that every numeric family has the same implementation dependency;
2. prefix, arithmetic, comparison, bitwise, shift, overflow and compound
   operators, each with its specified checked or explicit wrapping policy;
3. Boolean short-circuiting, scalar `if`, exhaustive scalar selection and
   conditional expressions;
4. labelled/positional calls, returns, overload identity and ordinary scalar
   mutation;
5. one computation witness that exercises the completed surface against an
   independent optimized C23 oracle on Windows and Linux/WSL.

An item is closed only by exact source-to-native execution and adversarial
failure evidence. Parser acceptance, design oracles, hand-built HIR, and a
backend-only artifact are supporting evidence, not completion.

Scalar implementation advances by semantic package, not by repeating one
operation for one width. A package owns one policy family across its applicable
signed/unsigned widths and aliases, preserves `(signedness, bitWidth)` as data,
and shares resolver, HIR, evaluator, lowering, oracle, and adversarial matrices.
Focused single-operation fixtures may diagnose a failure, but they do not define
the implementation architecture or require separate benchmark catalog entries.
The first migration of a family must remove any seed-only width specialization
that would otherwise be copied into later families.

Keep benchmark layers distinct: atomic unit tests diagnose one rule; executable
fixtures prove one source-to-native capability; public performance rows combine
related capabilities into dense semantic families. Consolidation never permits
different algorithms, inputs, policies, or observable work across W, C, and
Rust. A family that still folds W work while references retain runtime inputs
remains correctness-only and carries no ranking cells.

Every new or materially changed executable example records its expected exit,
stdout, and non-empty stderr beside the source. The executable catalog remains
the machine contract and its checks must reject drift from that local summary.

Keep restaurant-named examples only under `reference/last-light/`, where they
participate in the story or eventual single-module language tour. Isolated
compiler fixtures and benchmark workloads use neutral capability names, even
when they reuse a culinary concept to exercise syntax. Their coverage is
tracked by semantic family rather than by story or file length.

For each family, the executable witness should combine as much of the
currently executable syntax as remains semantically equivalent across W, C,
and Rust; focused positive, negative, and boundary cases cover the rest.
Treat coverage as two separate inventories: selected design forms and forms
actually exercised through native output. Maintain an explicit gap for every
selected variant that cannot yet reach native execution; do not call either
inventory complete merely because one representative benchmark passes. A long
source file is not coverage, and a passing benchmark does not prove the
unimplemented remainder of a family.

The W-1648 increment targets the existing W-392 rotations and count/reversal
functions only for built-in `i8`/`u8`, `i16`/`u16`, `i32`/`u32`, and
`i64`/`u64`. This bounded source-to-native slice is now source-backed-current
on CRT-free Windows x64 and Linux/WSL x64. It does not extend the width claim
to `Int`/`UInt`, `isize`/`usize`, or 128-bit types, and remains
not-performance-ready.

W-1649 now source-backs the existing W-392 `maskedShiftLeft`,
`maskedShiftRight`, and `logicalShiftRight` functions exactly for the same
built-in fixed-width family on CRT-free Windows x64 and Linux/WSL x64. Masked
counts use the logical width; signed arithmetic right shift remains distinct
from explicit logical zero-fill. Aliases, 128-bit integers, other targets, and
equivalent-runtime performance remain open. Its witness is named for the
bit-operation capability rather than for Last Light.

## Native application completeness

The language-level ownership, effects, ABI, callback, domain and kernel models
are sufficient foundations for an SDL-like library written primarily in W.
That does not mean the current SDK can deliver one. “No SDL” still requires the
operating-system window server, input stack, audio service and GPU driver APIs.
The first-party route keeps those raw boundaries inside target SDK/providers so
ordinary applications use safe W modules rather than C wrappers.

The finite dependency order is:

1. target-owned platform ABI adapters, including Windows COM and Apple
   Objective-C/blocks ownership where required;
2. window, display, input and event-loop contracts with main-thread affinity,
   wake/quit, resize/DPI, reentrancy and shutdown;
3. graphics resource, shader artifact, command, synchronization,
   surface/swapchain and presentation lifecycle;
4. realtime audio formats, clocks, deadlines, no-allocation callbacks, xrun and
   device-change behavior;
5. asset/resource packaging and an optional WebView provider.

WGFX0 records the corresponding high-level graphics candidate. It uses the
ergonomic lessons of Three.js without copying its API or JavaScript runtime
model: a small bundled foundation, a separately reachable first-party scene and
render package, first-party asset packages, and target-owned graphics providers.
Its first witnesses are headless transform/culling, a CPU-reference plus real-GPU
triangle/cube, a bounded glTF scene, an animated instanced scene, and a typed
render pipeline. The study is not an implementation or API promotion.

These are SDK/provider and implementation gaps unless an executable slice
proves that a missing general language rule is required. They must not displace
the scalar, control-flow, aggregate and explicit-ownership prerequisites above.

## Computable contract and proof readiness

WCCP0 studies a common `ContractIR` for mathematical propositions, function
preconditions and postconditions, state machines, module surfaces, and imported
data or wire schemas. The study selects `T<(predicate)>` for intrinsic value
invariants, structured documentation `contract:` fields for non-module
declaration relations, and module-header `contracts: [...]` configuration for a
module's static relations. It rejects a public `Proof<C>` from the initial source
surface.
Parser, lowering, proof checking, runtime validation, and optimizer integration
remain open. This work can proceed beside the active implementation queue and
does not displace rank 1.

Implementation remains ordered by dependency:

1. stabilize const evaluation, types, protocols, modules, and canonical
   identities;
2. parse, resolve, and lower selected local and imported carriers into normalized
   `ContractIR`;
3. derive one runtime validator and preserve its contract digest in evidence;
4. validate one bounded proof certificate with a small W-owned checker;
5. erase proof state and compare proof and ordinary runtime semantics;
6. enable one optimizer decision from the checked fact and measure its cost.

Inline documentation examples feed concrete witnesses into this system. They
do not become universal proofs without a checked coverage certificate. JSON
Schema, XML Schema, WSDL, SOAP policy, and similar formats enter through
versioned adapters rather than new literal syntax. Proof-mode applications stay
at rank 15 until these lower boundaries execute.

## Current checkpoint

W-1600 closes the bounded physical process/parallel reference: the unchanged
source consumes runtime input, crosses explicit target-provider launch and
lexical join, validates the emitted task result, and exits through CRT-free
Windows and Linux/WSL products. W-1599 remains a separate target-neutral
lifecycle oracle; it is not the storage or scheduler used by that product.

The native Hello optimization sentinel is closed through the public Release
route and exact-output oracle. The live catalog now retains the 2,048-byte
Windows CRT-free PE and the 1,712-byte Linux/WSL CRT-free PIE together with
compile, process-run, CPU, and memory measurements; the catalog retains no
produced executable. The 1,712-byte ELF is reproducible byte-for-byte through
the pinned Windows-to-Linux LLD recipe. The catalog also records a 2,096-byte
native WSL Hello lane with the same source/recipe digest but a different
toolchain digest. That gap cannot yet be assigned solely to the linker, and
the rows must not be pooled. A portable native-Linux LLD lane remains an
opt-in toolchain task, not a default switch.
The Windows reduction folds unwind metadata into the existing
read-only section while preserving separate executable and writable sections.
Sub-1-KiB is feasible but not yet a `w build` product. The exact reconstructed
784-byte PIE+RELRO and 720-byte PIE/no-RELRO Hello outputs omit ELF section
headers and non-loaded section data, reducing information useful to analysis.
They are distinct non-default size/hardening experiments, not product recipes
or ranking cells. Source/toolchain/output hashes and ELF audit facts are pinned
in their reconstruction evidence, but no public product recipe or receipt binds
them. A promotable size route needs a public target/recipe/output identity,
audit-package review, and its applicable benchmark gate. The no-RELRO variant
additionally requires final-artifact proof of no interpreter, imports, or
relocations. These reconstructions are distinct from the W+WRT0 bitcode/LTO
candidate above.
`--no-rosegment` broadens executable mappings and is not a size-only substitute.

The maintained native route is `W source → verified HIR → MLIR → LLVM IR →
object → link`. It never lowers W source through C. The C23 oracle remains an
independent validation reference, not a native W backend.

W-1601 now supplies the target-neutral two-function/seven-operation GPU0
witness, separate host/device MLIR, GPU/NVVM/PTX lowering, actual result `42`
on the available RTX A400, and a diagnostic-only in-process metric snapshot.
The seed parser and Frontend32 preserve the canonical module contract
`kernels: { label: directFunction }`, ordered direct local kernel bindings, and
grouped named projections across explicit local-document resolver edges.
The new `w-seed-gpu-module-2` bridge copies that meaning into
independently verified, provider- and target-neutral device-module records; its
first source-backed body slice proves zero-parameter signed-`i32` literal-return
kernels and survives source/frontend teardown. A caller-owned, provider-neutral
projection now selects one verified kernel binding, copies its module-contract
module name, kernel label, private implementation name, and payload into the
exact GPU0 records, and remains verifiable after bridge teardown. The recipe is
still mixed MLIR 23.1.1 plus Clang 22. Frontend32 plus ACCINV0 preserve and
independently verify the bounded static accelerated-domain invocation after
producer teardown. ACCBIND0 additionally closes one caller-owned root/profile
relation, including exact module/kernel-instance identity, ABI equality, and
the minimum effective admission budget. It is not a signed product-profile
verifier and contains no provider handle or launch. ACCREQ0 now cross-checks
that binding with the independently verified GPU0 program and copies one exact
device artifact plus its semantic and physical identities into a
provider-neutral request that survives both producer lifetimes. The physical
gate now obtains the device MLIR, kernel symbol, and canonical signed-`i32`
expectation from that verified ACCREQ0, then proves a valid mismatch after
execution; the CUDA adapter remains a private process boundary. General
runtime/provider launch/join/result,
public GPU build/run, qualified/imported-invocation HIR, and a supported GPU ABI remain open;
the accelerated application line therefore stays open until those public and
physical boundaries have evidence. The explicit
`Launch<Module>` route
remains the later dynamic path, not the common static syntax. Lower-ranked
scalar, CFG, aggregate, ownership and error work remains the integration
priority. Existing bounded task-storage and cancellation/lifecycle evidence is
preparatory; it does not close general rank-9 tasks or the rank-10 scheduler.

W-1630 now closes one bounded private ACCPROV0 launch/join/result boundary
above verified ACCREQ0. The target/provider-neutral core binds all caller-owned
outcome and receipt ranges before stage and records the exact
`staged → submitted → deviceRunning → bodySettled → providerDrained → cleanup
→ outcomeCommitted → joined` lifecycle. Its semantic output contains only the
successful signed-`i32` value/result shape and lifecycle order; provider class,
target, implementation, device, queue, generation, raw status, digests, and
cleanup facts remain in pointer-free provenance. Failure, device loss, stale
generation, protocol mismatch, wrong result, callback uncertainty, or cleanup
uncertainty publishes no semantic result and prevents retry/double cleanup.
The existing Windows CUDA adapter exercises this boundary and preserves the
plumbing sentinel `42`; external MLIR → NVVM → PTX remains tooling-owned.
This is compiler-lifecycle correctness evidence only, with no public GPU
runtime/ABI or other provider/target claim.

W-1631 closes the next basic-operator slice before higher-level lifecycle work:
`u64.overflowingPower` now preserves its virtual `(u64, Bool)` product from
source through verified HIR and reachable-only MLIR lowering. The emitted
exponentiation-by-squaring helper has logarithmic iteration count, no fixed
exponent ceiling, no allocation or CRT dependency, and executes exact ordinary,
overflow, wrapped, and `0^0` witnesses natively. Other widths, const evaluation,
stable ABI, timing, and performance remain open.

W-1632 closes bounded `u64.saturatingNegate` and `u64.saturatingPower` next.
Negate lowers to unsigned saturation from zero. Power uses a reachable-only
logarithmic helper, clamps exact multiplication overflow to `u64.max`, and
preserves `0^0 == 1`. The fixture executes all boundary cases through the
native route. Other widths, const evaluation, stable ABI, ranking, and
performance remain open.

W-1633 closes exact native witnesses for the already-lowered unsigned
`overflowingSubtract`, `overflowingMultiply`, and `overflowingNegate` family.
Their `(u64, Bool)` products remain virtual SSA values, the native artifact uses
unsigned overflow intrinsics and tuple projections directly, and Windows plus
Linux/WSL exact execution covers ordinary and overflow boundaries. Other widths,
const evaluation, stable ABI, equivalent runtime-work ranking, and performance
remain open.

W-1634 closes the bounded source-backed-current ConstIR7 evaluation slice for
the ten existing `u64` policies: saturating and overflowing add, subtract,
multiply, negate, and power. ConstIR7 preserves the virtual `(u64, Bool)`
product and checked `.0`/`.1` projections, while power uses exponentiation by
squaring under the step quota. The slice has zero heap, CRT, floating-point
conversion, precomputed result, and artificial exponent cap. It is limited to
`u64`; frontend module-const tuple initializers, generic tuples, a stable tuple
ABI, other widths, and performance remain open. `benchmarkDisposition` is
`compiler-lifecycle`; no public benchmark is added.

W-1635 carries the existing virtual `(u64, Bool)` overflow product across the
next compile-time boundaries: a direct `const fn` return and an explicitly
typed module constant, including export. The frontend canonicalizes that exact
closed type, and module constants reuse the synthetic ConstIR dependency graph,
memoization, quotas, cycle defense, and transactional publication. This does
not add generic tuples, tuple parameters/literals/destructuring, inferred tuple
constants, a stable ABI/layout, runtime or backend materialization, other
widths, or performance evidence. `benchmarkDisposition` remains
`compiler-lifecycle`; no public benchmark is added.

W-1636 closes the wrapping family as one compiler package instead of one task
per width or operation. Signed and unsigned 8/16/32/64-bit builtins plus the
current x86-64 `Int`/`UInt` aliases now cross frontend63, verified HIR78,
generic MLIR lowering, LLVM verification, and exact CRT-free Windows and
Linux/WSL execution. The combined fixture declares its expected exit/stdout in
source; C23 and Rust 2024 are correctness references with runtime inputs, so
performance ranking remains blocked until W performs equivalent runtime work.

W-1637 closes exact implicit integer widening as the next family-sized rank-1
increment. Frontend64 and HIR79 preserve one widening wrapper across signed and
unsigned 8/16/32/64-bit builtins and the current x86-64 `Int`/`UInt` aliases;
bindings, returns, calls, assignments, and mixed operands share the same
frontend legality table. The public native witness closes return, argument,
binding, and alias contexts while general narrow binary lowering remains a
separate operator-family gap. NativeSubset0 and MLIR0 retain source-width
signedness through `llvm.trunc` plus `llvm.sext`/`llvm.zext`. The public fixture owns exact output
for Windows and Linux/WSL, while C23 and Rust remain correctness references.
Target-general aliases, `usize`/`isize`, 128-bit integers, narrowing, explicit
conversions, stable ABI/FFI, and equivalent-work performance ranking remain
open.

W-1638 closes one fixed-width integer-comparison family. The public witness
covers all six comparison operators across signed and unsigned 8/16/32/64-bit
integers and the current x86-64 `Int`/`UInt` aliases; its final case widens a
`u8` call argument to `i16` before comparing. HIR80 records one Bool-producing
integer-comparison identity for equal signedness/width operands, and MLIR
selects signed or unsigned `llvm.icmp` ordering predicates from verified type
facts. The exact-output witness runs through the maintained native Windows
and Linux/WSL routes. C23 and Rust 2024 preserve runtime operands while W's
calls use fixed literals, so the catalog records correctness only and no
performance ranking. Mixed-width/signedness comparisons beyond the explicit
argument widening, `usize`/`isize`, 128-bit integers, other targets, stable
ABI/FFI, and equivalent runtime-work performance remain open.

W-1639 closes checked ordinary integer add/subtract/multiply/divide/remainder
as one family.
The source/HIR type facts preserve signedness and logical width for i8/u8,
i16/u16, i32/u32, i64/u64, and the current x86-64 `Int`/`UInt` aliases while
the backend keeps the i64 physical carrier. Generic checked lowering detects
both carrier overflow and logical-width overflow after the carrier operation;
compound `+=`, `-=`, `*=`, `/=`, and `%=` remain transactional on failure.
Division and remainder reject zero, and signed minimum divided by negative one,
before the target operation; signed minimum remainder negative one yields zero.
Exact Windows and Linux/WSL witnesses cover all ten source types, with
representative runtime overflow and invalid-divisor failures required to trap
without committing buffered output. C23 and Rust 2024 are success-domain
correctness oracles with runtime/black-box operands, so this is one
correctness-only catalog row without ranking. Named `checkedAdd`-style APIs,
named negation policies, power, shifts, target-generic aliases, and
equivalent-runtime performance remain outside this family.

W-1640 extends implementation coverage for the existing integer prefix
operators, without adding syntax or per-width public identities. One generic
operation/type-fact route covers checked unary `-` for signed
`i8`/`i16`/`i32`/`i64`/`Int`, and total width-preserving unary `~` for signed and
unsigned 8/16/32/64-bit integers plus the current x86-64 `Int`/`UInt` aliases.
Unsigned ordinary unary minus stays invalid; logical minimum negation must fail
before observable output. Bool `!`, `f64` negation, and named
wrapping/saturating/overflowing negation remain distinct. The physical seed
carrier remains `i64`. The family fixture
`compiler/seed-c/fixtures/integer-prefix.w` and focused
HIR/scalar-evaluator and NativeSubset0/MLIR test sources are present. Exact
family output and minimum-failure evidence cover the public `w run` routes on
CRT-free Windows and Linux/WSL. The reviewed preflight/type-equality and
per-width MLIR assertions, focused compiler units, and all three public gates
pass on the final source. C23 and Rust 2024 are correctness references without
ranking; `benchmarkDisposition:
correctness-reference-no-ranking`, not-performance-ready until equivalent
runtime work exists. Other targets,
`usize`/`isize`, 128-bit integers, target-general aliases, stable ABI/FFI,
general panic payload/cleanup, named numeric APIs, and equivalent runtime work
remain gaps.

W-1641 closes only the existing fixed-width integer `truncatingBits:` path
under W-389. Its public Windows and Linux/WSL witnesses are correctness-only;
C23 and Rust 2024 are correctness references, not performance rankings.
`exactly:`, `rounding:`, and floating conversions remain outside W-1641.
W-1644 is the current bounded increment for integer-to-integer
`D(saturating: source)`: all 100 pairs among the signed/unsigned 8-, 16-,
32-, and 64-bit types plus current x86-64 `Int`/`UInt` aliases clamp the
mathematical source value to the destination range; the integer form accepts
no `try` or `nan:` label. Frontend66, HIR87, Native0 schema 10, MLIR58, and
Windows43 now carry the generic family. Focused matrices cover every pair and
the compact four-quadrant witness executes exactly on CRT-free Windows and
Linux/WSL; malformed forms fail before output. ProductClosure0 remains
intentionally narrower. C23 and Rust 2024 are correctness references only,
and `benchmarkDisposition: deferred` until equivalent runtime work.

W-1650 now carries the plain integer `try D(exactly: source)` form for all 100
source/destination pairs among signed and unsigned 8/16/32/64-bit integers and
current x86-64 `Int`/`UInt` aliases through Frontend71, verified HIR92,
NativeSubset0, and the private `w-seed-mlir0-integer-exactly-1` artifact. HIR
preserves a three-block typed success/error split whose error edge is canonical
`NumericConversionError.outOfRange`; `tooling/check-mlir0.mjs` verifies the
artifact with `mlir-opt` and `mlir-translate`. HIR94 now admits a bounded
binding continuation and process root for that split; ProductClosure0 v4
projects its normal and typed-error successors while retaining the restricted
direct-throw outcome. Process-executable v6 materializes
reverse-initialization cleanup for the bounded exact-conversion root, retains
the typed carrier through root finalization, and only then adapts the error to
status 1. Public CRT-free Windows x64 and Linux/WSL x64 gates execute the
canonical runtime `Arguments.count` source at zero, 126, 127, and 128 user
arguments, proving the converted success payload and an out-of-range path with
no output committed before typed failure. The
logical `usize` identity remains distinct from portable `u64`; the current
seed profile itself is x64-only. No benchmark or timing claim is made. The
direct-throw route, other conversion
families, floats, 128-bit integers, general `isize`/`usize`, other target
aliases, catch, general typed cleanup, and ABI remain gaps.
W-389 and rank 1 remain open.

W-389 now has its first private lowering artifact. NativeSubset0 rederives the
five rounding modes and all three successor roles from verified HIR. The MLIR
artifact classifies NaN and infinity before rounding, uses environment-
independent LLVM intrinsics, checks the rounded value against exact half-open
power-of-two bounds, and emits `fptosi`/`fptoui` only in the proven-valid
successor. `mlir-opt --verify-each` and `mlir-translate` validate the artifact
for Linux and Windows x64 target triples. ProductClosure0 v4 publishes all
three outcomes for a direct default-unit helper while retaining the
out-of-range compatibility channel. HIR96, ProductClosure0, NativeSubset0, and
process-executable v6 now also close one constant-source process root. Its
public Windows and Linux/WSL witnesses print the rounded nearest-even result as
`Rounded 2\n` with status 0, and keep out-of-range at status 1 with empty
stdout/stderr; both outcomes complete reverse cleanup before process
adaptation. The output path is bound to the verified result binding, not a
hardcoded adapter string. The bounded raw-bit ingress now takes
`Arguments.count` through exact `u64`, `f64.fromBits`, `.toBits()`, direct
bitcasts, and exact Windows/Linux process output, with independent C23/Rust
runtime references. A separate canonical quiet-NaN literal-bit witness now
reaches the public typed non-finite outcome on both targets. The runtime-
selected ingress now composes the exact `Arguments.count == 0` finite-bits /
nonzero-count quiet-NaN-bits diamond with `f64.fromBits` and public toward-zero
`i8` rounding. It prints `Rounded 42\n` at zero arguments and returns typed
status 1 with empty stdout/stderr for one or two arguments. HIR/NativeSubset0
keep this to an exact-width `u64` selector, one bitcast, one float evaluation,
and cleanup-before-adaptation; an arbitrary `args.count`-derived bit pattern
remains rejected by an adversarial test. MLIR uses direct bitcast/fpclass and
avoids `llvm.intr.trunc` on this toward-zero route so Windows stays CRT-free.
Public Windows and Linux/WSL x64 `w run` and Release `w build` checks require
Kernel32-only PE imports and static-PIE ELF with no interpreter or `DT_NEEDED`.
The fixture maps to the existing `float-integer-rounding` owner and this
compiler-lifecycle increment adds no performance row. Unrestricted runtime
float ingress, argument-content parsing, other targets, post-opt/object-symbol
receipts, independent non-finite conversion references, and performance remain
gaps.

W-1645 generalizes the prior strict-f64 seed path into one strict floating
family for `f32` and `f64`. Frontend67 materializes exact binary32/binary64
bits; HIR88 preserves distinct identities; MLIR59 and Windows44 emit direct
width-correct LLVM dialect arithmetic, comparisons, and unary negation without
fast-math. The compact width-neutral compiler witness owns both widths and
the C23/Rust 2024 correctness references. Floating conversions, remainder,
power, total-order helpers, stable ABI/FFI, other targets, and equivalent
runtime-work performance remain open.

W-1646 implements the exact total widening relation already selected by
W-388/W-389: `f32 -> f64`, 8/16-bit integers to `f32`, and 8/16/32-bit
integers to `f64`. Frontend68 and HIR89 insert and preserve one explicit
conversion value across bindings, returns, calls, mixed operators, and
unlabeled `D(value)`. MLIR60 uses `fpext`, or narrows the physical `i64`
carrier to the verified integer width before `sitofp`/`uitofp`, without
fast-math, heap, runtime, or CRT helpers. The compact family witness executes
on Windows and Linux/WSL; C23 and
Rust 2024 remain correctness references only because the current W graph may
fold while their inputs remain runtime values. Lossy/fallible numeric
conversion policies remain open.

W-1647 implements the selected f32/u32 and f64/u64 bit reinterpretation route:
`fromBits` accepts only the same-width unsigned representation, and matching
`.toBits()` preserves it through storage, copy, and round-trip. Frontend69,
HIR90, NativeSubset0, and MLIR61 carry direct width-correct bitcasts through
the seed. Exact source-to-native witnesses pass on Windows x64 and Linux/WSL
x64; other targets, stable ABI/FFI, numeric conversions, byte order, and NaN
payload after arithmetic remain outside this increment. C23 and Rust 2024 are
correctness-only references, so `benchmarkDisposition: deferred` until runtime
work is equivalent.

The first rank-1 increments are now executable. Signed-`i64` `&`, `|`, `^`,
and the original unary-`~` crosspoint cover exact W source, canonical
precedence, verified HIR0, direct LLVM-dialect operations, and the maintained
native routes; W-1640 is the width-family complement increment. W-1642 extends
ordinary binary `&`, `|`, and `^` across signed and unsigned 8-, 16-, 32-, and
64-bit integers plus the current x86-64 `Int`/`UInt` aliases, retaining one
canonical operand/result type after existing exact integer widening. Checked
`<<` and `>>` are now closed by W-1643 for the same fixed-width types: the left
operand and result have exactly the same canonical integer type, and the count
is `UInt` (`u64` on current x86-64). Counts at or above the logical width
fail; left shift also fails on mathematical overflow. Signed right shift is
arithmetic and unsigned right shift is logical. One generic lowering uses
verified signedness/logical-width facts and direct LLVM operations rather than
a per-width operation enum. The exact
`integer-bitwise`, `shifts`, `power`,
`power-prefix`, `compound`, and
`uint-compound` sources/oracles own these bounded crosspoints.
`shifts.w` executes through frontend, verified HIR, Native0/MLIR0,
and the CRT-free native routes on Windows and Linux/WSL. C23 and Rust 2024 are
correctness references only; there is no performance ranking. Its
`benchmarkDisposition` is `deferred` until W retains equivalent runtime
operands. Checked integer
`**` uses a
`UInt` exponent, keeps the base's signedness, defines `0 ** 0` as one, and
lowers with logarithmic exponentiation by squaring. The parser now proves the
W-769/W-1152 boundary as well: power is right-associative, binds before a
prefix on its left, accepts a prefixed base when parenthesized, and gives its
exponent an independent `UInt` inference context. A negative exponent remains
invalid. The eleven signed compound assignment forms reuse the same checked
operation and SSA versioning. The UInt/u64 witness now covers all eleven
unsigned compound forms with checked arithmetic, shifts, and the existing
direct bit operations over typed SSA versions. Immutable targets fail closed.
W-1648 now source-backs rotations and the selected count/reversal functions
for `i8`/`u8` through `i64`/`u64` on the exact Windows x64 and Linux/WSL x64
routes. It makes no `Int`/`UInt` target-width or performance claim. W-392
remains open for named numeric/shift policies, power, other bit primitives
such as `bitWidth`, carry/borrow, and full multiply, SIMD,
`usize`/`isize`, 128-bit integers, non-x86-64 alias widths, stable ABI/FFI,
other targets, equivalent-runtime performance, and the complete integer
operator matrix. Unary `~` is separately covered by W-1640. Checked
arithmetic, wrapping arithmetic, comparisons, and exact widening already span
the current fixed-width integer set and must not be described as 64-bit-only
gaps.
W-1597 remains a legality certificate only; target policy must still combine it
with observability and cost facts and compare any direct-call artifact with the
W-1600 physical reference.

The neutral [`u64-mix-round` witness](compiler/seed-c/fixtures/u64_mix_round.w)
now composes the count-only process ingress with exact conversion to `u64`,
local helper calls, XOR, rotate-left, logical-right-shift, wrapping multiply,
and wrapping add. Its three-user-argument case prints exactly
`Mix 5608831001354178255\n`; zero- and one-argument cases remain distinct and
are checked against independent C23 and Rust 2024 references. The exact source
passes verified-HIR/MLIR-backed Windows x64 and Linux/WSL x64 `w run` and
`w build` gates with CRT-free product audits. Argument text access and other
targets remain outside this process subset. Its disposition is
`compiler-lifecycle`, with no performance row or result; this is one bounded
computation witness, not closure of the scalar operator matrix.

The current fixed task counts and worker capacities are seed evidence limits.
They must not become language, public ABI, or final runtime limits.

W-1606 began the earlier task-storage line: HIR38 no longer uses a four-slot product array as an
admission or verification limit for `.main` or `.domain`, and PARSEL1 measures
dense caller-owned `.domain` task relations independently from worker capacity.
Five siblings now cross verified HIR in both lanes and cross PARSEL1 for
`.domain`; legacy product/selector consumers still stop at four until their
incremental migration.

W-1607 extends that line through invocation storage. PARINV1 measures dense task
and argument relations from verified PARSEL1, normalizes named arguments into
parameter order, and removes the scalar evaluator's fixed sixteen-argument
array. Five tasks and seventeen arguments pass this compiler boundary. The
physical providers and PARMLIR0 still consume the fixed compatibility records;
their migration remains a dependency before later cancellation work.

W-1608 removes the fixed logical-task arrays from the Windows component.
PARPROV1 executes five tasks through constant-size worker waves and produces
the same semantic result at capacities one and two. It also executes the
seventeen-argument witness. W-1609 completes rank 2 for the compiler/component
path: PARMLIR1 emits five runtime-parameterized task entries from measured
records, accepts the seventeen-argument witness, preserves compatible PARMLIR0
bytes, and lowers the five-task artifact to Windows COFF and Linux PIC ELF with
MLIR/LLVM 23.1.1. Public process products retain their bounded compatibility
route. This evidence is preserved, but the active queue now closes the scalar
and control-flow surface before returning to structured cancellation.

W-1610 opens the bounded structured-lifecycle line without reintroducing a storage ceiling. TASKLIFE1 runs the
W-1599 lifecycle state machine over measured caller-owned task and event views.
A five-task fail-fast witness cancels and drains four unfinished siblings before
deterministic error publication, and a 137-event witness crosses the former
fixed trace limit. W-1611 now binds verified successful PARPROV1 outcomes into
that measured lifecycle: provider capacities one and two produce identical
five-task records, trace, and scope value. W-1612 adds the private Windows
provider primitive for tagged success, error, and canceled completions. It
preserves a settled sibling and cancels only later unstarted waves. The next
dependency was the producer side of typed failure: W-1613 now carries a
terminal root `throw E` through parser, frontend29, and verified HIR39 while
rejecting non-`Error` and unproved branch composition. W-1614 adds the first
mixed normal/error CFG: one complete top-level conditional with independent
return/throw arms and no hidden join. W-1615 adds the bounded parser/frontend
`try localCall(...)` propagation marker. It requires one synchronous local
throwing callee and one lexical throwing caller with the same nominal local
error enum. Async and spawn owners carry the thrown outcome without `try`, but
HIR/native lowering, catch, cleanup, conversions, and optional or async `try`
remain unsupported.

W-1616 carries the exact synchronous relation into HIR40. One
terminator-owned W_SEED_HIR0_TERMINATOR_INVOKE owns the direct local call and
routes to normal and typed-error successor blocks with one typed block
argument each. The verifier proves the exact three-block relay and rejects
forged ownership, target, type, and argument relations. No ordinary CALL
instruction, Task, heap, or packed Result carrier is introduced, and
ProductClosure0 remains fail-closed for INVOKE. This is source-backed-current
only for verified HIR. MLIR/native lowering, public ABI, catch, cleanup,
conversions, and performance remain gaps. Its benchmarkDisposition is
compiler-lifecycle.

W-1617 carries that relay across the first downstream compiler boundary. A
dedicated private selector emits an optimizer-visible
`!llvm.struct<(i1, i64)>` carrier, a real `llvm.call`, extracted outcome and
payload, and explicit normal/error branches. MLIR/LLVM 23.1.1 accepts and
translates the artifact. The carrier is not a public ABI; there is no unwind,
Task, heap, process root, native product, or benchmark claim, and ordinary
product routes remain fail-closed.

W-1618 preserves one dominating synchronous `defer` across that exact typed
invoke. HIR41 records one cleanup obligation and materializes an ordinary
direct cleanup call in both the normal and typed-error successors. A separate
private MLIR artifact retains both calls before rebuilding the compact
carrier, and MLIR/LLVM 23.1.1 verifies and translates it. Existing product and
success-only provider routes remain fail-closed. General cleanup stacks,
`defer async`, catch, native products, public ABI, and performance remain open.

W-1619 adds the first integrity-bound typed physical completion without
widening the success-only provider/lifecycle contracts. It derives the nominal
payloadless error case from verified HIR41, checks an independent success
value, stages PLATFORM1 output transactionally, and keeps semantic records
identical across physical capacities one and two. The exact two-child array is
a seed witness, not a language or ABI bound. Its receipt is not provider
authentication and the callback is not native HIR execution.

W-1620 adds a separate caller-owned typed TASKLIFE bridge above fully verified
PARBIND1. Its exact two-task seed transaction has 22 events. Task 0 succeeds.
Task 1 maps nominal `Failure.denied` to TASKLIFE's internal generic `BODY`
error, while a typed sidecar and result retain the exact nominal identity.
Cleanup precedes commit. A late fail-fast cancellation names task 1, and the
joined scope commits the consumed error. Capacities one and two produce equal
semantic task, trace, typed-sidecar, and semantic-digest outputs, while their
physical provenance differs. The bridge publishes transactionally and rejects
capacity, alias, forgery, and generation violations. Its `u32` HIR and count
fields are serialized proof indices and counts, not Task handle width. A future
materialized Task handle is opaque and target-specialized. One native word is
the baseline; lowering may elide it or prove a narrower representation more
efficient. No 32-bit constraint or cost is imposed on 64-bit. This remains
compiler-lifecycle evidence only, not a public Task/runtime/ABI, general
cardinality, native-execution, attestation, or performance claim.

W-1621 adds compiler-owned local provider admission. PARBIND1 now requires an
opaque process-local authority and calls the statically linked Windows x64
provider through that wrapper. The authority receipt binds target, domain,
profile, identity, generation, assurance, and contract digest to provenance;
the callback remains only an untrusted task-body adapter whose completion is
checked against HIR. This is not binary authentication or registry
attestation. External signatures, roots, rotation, revocation, and freshness
remain distribution-layer gaps and do not block the compiler roadmap.

W-1622 adds a distinct private panic completion and fail-closed PARBIND1 signal.
It preserves an allocation-free panic code, makes panic dominate recoverable
outcomes settled in the same wave, and publishes no semantic task result. The
next slice must consume that signal at a real process/Wasm/compartment boundary
and perform physical teardown; verified-HIR panic consumption, `PanicEvent`,
and source-panic execution remain open. Physical cancellation remains cooperative rather than thread
termination.

W-1623 adds only a private Windows x64 compiler-lifecycle root-child boundary
for that signal. A trusted private helper owns the verified HIR/PARBIND witness
and emits a fixed big-endian, versioned frame. The parent proves the root child
live, explicitly terminates and bounded-joins it, revalidates immutable inputs,
and publishes a receipt transactionally. Its unkeyed digest provides integrity
and correlation, not authentication. The helper is trusted private witness
code, and the Job Object does not prove descendant-tree drain. Source
panic/PanicEvent, public Task/runtime/ABI/product behavior, hardware faults,
cleanup/restart/supervision, descendants, other targets/providers, native HIR
child execution, attestation, benchmarks, and performance remain open.

W-1624 closes the upstream source-to-verified-HIR slice for explicit panic.
The accepted seed form is one unlabelled plain String literal; it becomes a
`Never`-typed PANIC terminator with copied message bytes and no ordinary call
or instruction. Invalid shapes fail closed, and a native-process witness keeps
`Never` and `usize` layout disjoint. The next panic step is MLIR/native
lowering into the selected fault boundary; `PanicEvent`, cleanup, teardown,
restart, public runtime/ABI behavior, and performance remain open.

W-1625 connects that verified terminator to the existing executable and
native-process MLIR0 routes. A reachable ordinary/process/CFG panic becomes
`llvm.intr.trap` followed by `llvm.unreachable`; its checked literal remains in
HIR identity but is stripped from this release-style artifact. Empty non-panic
programs and unrelated artifact kinds remain closed. The next panic increment
is composition with the private parallel signal and a bounded `PanicEvent`/
lifecycle boundary, not another synthetic artifact route.

W-1626 extends only the measured PARINV1 compiler boundary. A real source
`panic(...)` child selected by `spawn<.domain>` is admitted beside a scalar
child through the exact frontend and HIR physical predicates. PARINV1 records
the child as a private `PANIC` task with its HIR terminator, message-value, and
explicit-code indices; its scalar evaluator rejects that task without
mutating a value output. This does not execute a provider, materialize a
`PanicEvent`, or define a public Task/runtime ABI. Provider execution,
lifecycle, cleanup, and benchmarks remain open.

W-1627 adds the separate PARPANIC1 (`w-seed-parallel-panic-binding1-1`) bridge
from verified HIR/PARSEL1/PARINV1 through the existing process-local Windows
PLATFORM1 authority. Its compiler-owned adapter maps scalar value tasks and
source-selected panic tasks to their exact checked completions, then publishes
one caller-owned private signal only after complete receipt, authority, identity,
digest, capacity, and alias validation. The signal copies source/module
identity, relevant spans, source facts, code, indices, and message bytes; no
semantic value is published. Semantic identity excludes provider capacity and
physical receipt facts, which remain in a separate provenance digest. Existing
success-only PARPROV1/PARLIFE1 and typed PARBIND1 semantics remain unchanged.
The focused C23 evidence is Windows x64 and `compiler-lifecycle` only; public
`PanicEvent`, Task ABI/runtime, native-HIR execution, cleanup/teardown,
portability, benchmark, and performance claims remain open.

W-1628 adds PANICLIFE1 (`w-seed-parallel-panic-lifecycle1-1`) as a separate
target-neutral private bridge above verified PARPANIC1. It publishes one
caller-owned decision and exactly three ordered events for the deterministic
primary. The decision requires boundary termination, forbids normal outcome
publication, and does not claim user cleanup or resource-registry evaluation.
The semantic digest covers only panic code, copied message, decision fields,
and event order. Source/provider/receipt facts remain provenance, so upstream
capacities one and two produce semantically identical decision/event/message
content. A local PARPANIC1 measure rederives no-panic absence instead of
trusting a status field. PANICLIFE1 has no provider call or own workspace.
This remains bounded `compiler-lifecycle` evidence. The bridge is target
neutral, but current upstream execution evidence is Windows x64. PANICBOUNDARY1,
`PanicEvent`, runtime/ABI, cleanup, public product, benchmarks, and performance
remain open.

W-1629 adds PANICHOSTREG1 (`w-seed-parallel-panic-host-registry1-1`) as a
private Windows x64 host witness above a verified PANICLIFE1 view. Its
caller-owned authority creates exactly one anonymous, non-inheritable Win32
event and owns it from successful `CreateEventW`. `release` validates all
inputs and ranges, calls `CloseHandle` once, and publishes one semantic
`EVENT` transition with `RESOURCE_REGISTERED` and
`RESOURCE_CLOSE_COMMITTED` only after a true close result. A pre-close fault
leaves `REGISTERED` for `destroy`. A false or uncertain result after the real
attempt is terminal `UNCERTAIN` with no retry or double-close. The raw handle
is absent from outputs and digests, but remains in caller-owned C registry
storage. Capacity one is an evidence ceiling only. Semantic record/events are
capacity-independent while provenance retains authority, generation, and
physical close facts. This remains correctness-only `compiler-lifecycle`
evidence. It does not compose PANICBOUNDARY1, implement a general registry,
accept arbitrary handles, claim user cleanup or OS-object destruction, define
runtime/public `PanicEvent` or Task ABI behavior, execute native HIR, support
other targets, or measure performance.
