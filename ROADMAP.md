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
- Default native products are CRT-free and statically close W-owned runtime and
  standard-library code by reachability.
- Optimize the largest proved closed graph; retain only observable product and
  ABI roots.
- Elide a declared execution relation only with an independently verifiable
  equivalence fact; compare the optimized path against the physical reference.
- Measure public executable behavior only after an exact correctness oracle.
- Adopt pinned Hyperfine 1.20.0 as the future cross-platform public wall-time
  layer: preserve per-run JSON samples, use `--shell=none` for sub-5 ms
  commands, and schedule one-command invocations in balanced ABBA order. Its
  integration remains pending and must use a receipt separate from native
  samples; exact oracles and native CPU, memory, section, cycle, and artifact
  evidence remain with W's native runner. Do not infer those metrics from
  Hyperfine or merge the two sample populations.
- Treat performance, memory, binary size, and compile latency as persistent
  optimization signals, never as permission to change semantics.

## Ranked queue

| Rank | Increment | Completion boundary | What it enables |
| ---: | --- | --- | --- |
| 1 | Scalar literals and operators | Every designed scalar literal and operator family reaches verified HIR, direct MLIR/LLVM lowering, CRT-free Windows and Linux execution, checked failure or explicit wrapping policy, and an independent C23 oracle; syntax-only coverage does not count | Gives W C-level fine-grained arithmetic, comparison, logical and bit-manipulation capability before higher abstractions depend on it |
| 2 | Bindings, assignment, functions and calls | Mutable and immutable bindings, compound assignment, labelled and positional anchors, ordinary calls, returns and overload resolution execute from exact W source without seed-only rewrites | Establishes reusable computation and a stable value-flow substrate |
| 3 | Structured control flow | `if`/`else`, exhaustive selection, guards, loops, `break`, `continue` and multi-block returns lower to general verified CFG and execute adversarial branch and loop witnesses | Removes straight-line restrictions and provides the control substrate for errors, cleanup and scheduling |
| 4 | Value aggregates and central enums | Tuples, structs, payload enums, exhaustive pattern matching and fixed arrays have verified layout-independent semantics plus efficient target layouts and native witnesses | Matches ordinary C data modelling while preserving W's enum-first design |
| 5 | Modules, imports, generics and specialization | Multi-module calls, labelled imports, generic specialization and closed reachable graphs produce deterministic artifacts; unused private graph nodes disappear | Enables real programs and makes whole-module/product optimization the normal case |
| 6 | Explicit views, borrows, storage and ownership | `ref`, `mut ref`, `inout`, moves, views, explicit storage/allocator choices and deterministic cleanup execute for value and resource-bearing aggregates | Establishes memory safety and cost without requiring automatic lifecycle machinery |
| 7 | Errors and effect composition | Typed `throw`/`try`/`catch`, panic boundaries, cleanup and effect propagation compose over general CFG and resource-bearing values | Makes failure semantics complete before asynchronous propagation is generalized |
| 8 | Arrays, matrices, SIMD and accelerator lowering | Static and dynamic collections, views, `@`, vectorization and one real CPU/GPU numerical witness share typed semantics and independent oracles; GPU launch remains a provider concern | Adds scientific and heterogeneous performance only after its scalar, CFG and ownership prerequisites are real |
| 9 | Tasks and structured concurrency | `async`, `await`, `spawn`, groups, cancellation, deterministic outcomes and cleanup execute over measured caller-owned task records with no language-level child limit | Builds concurrency on the completed value, error and ownership model |
| 10 | Provider-neutral scheduler | Target-neutral ready/task/frame state lowers once; capability-selected providers supply platform primitives and cached topology facts across Windows, Linux, macOS, iOS, Android, WebAssembly and future viable targets | Portable parallel execution without a platform-shaped language ABI |
| 11 | Automatic lifecycle and memory optimization | Escape/liveness proofs choose registers, stack, arenas, regions or heap; virtual objects materialize only when identity/escape requires it; automatic cleanup remains semantically deterministic | Adds convenience after explicit ownership is measurable and trustworthy |
| 12 | Incremental compiler, test selection and cross-target distribution | Exact dependency invalidation, risk-relevant gates and reproducible signed LLVM/MLIR/LLD target packs cover the Windows/Linux/macOS baseline | Fast human/AI iteration and compact offline cross compilation without hollow green tests |
| 13 | Package, registry, service and sandbox slices | Signed binary-first packages, source fallback, independent verification, one service provider and bounded sandbox execution work against the stable compiler/runtime boundary | Opens the ecosystem without freezing premature compiler internals |
| 14 | UI, native graphics, scientific and proof-mode applications | Promote one real workload at a time through correctness, applicability, resource receipts and benchmark evidence; platform SDK/providers remain outside the language core | Broadens targets from proven primitives instead of speculative abstractions |

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
   representation-bit round trips. Remaining conversion policies continue to
   block this rank-1 prerequisite;
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

Every new or materially changed executable example records its expected exit,
stdout, and non-empty stderr beside the source. The executable catalog remains
the machine contract and its checks must reject drift from that local summary.

Reserve `restaurant-*` names for examples that actually participate in
Last Light lore or its eventual single-module language tour. New isolated
compiler and benchmark witnesses use neutral capability names. Migrate or
consolidate older misnamed fixtures opportunistically when their semantic
family is next touched; do not mass-rename them.

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
equivalent-runtime performance remain open. Its neutral witness is the first
opportunistic migration away from an unrelated `restaurant-*` fixture name.

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
at rank 14 until these lower boundaries execute.

## Current checkpoint

W-1600 closes the bounded physical process/parallel reference: the unchanged
source consumes runtime input, crosses explicit target-provider launch and
lexical join, validates the emitted task result, and exits through CRT-free
Windows and Linux/WSL products. W-1599 remains a separate target-neutral
lifecycle oracle; it is not the storage or scheduler used by that product.

The native Hello optimization sentinel is closed through the public Release
route and exact-output oracle. The live catalog now retains the 2,048-byte
Windows CRT-free PE and the 2,096-byte Linux/WSL CRT-free PIE together with
compile, process-run, CPU, and memory measurements; no produced executable is
retained. The Windows reduction folds unwind metadata into the existing
read-only section while preserving separate executable and writable sections.
The sub-1-KiB target remains an optimization opportunity, not a completion
gate.

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
remains the later dynamic path, not the common static syntax. Only then does the
queue returns to the lower-level language surface before generalizing task
storage, cancellation, and scheduling.

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
`compiler/seed-c/fixtures/restaurant-integer-prefix.w` and focused
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
and `benchmarkDisposition: deferred` until equivalent runtime work. Fallible
`D(exactly:)` remains deferred until typed conversion-error lowering is
end-to-end; it is not implemented. W-389 and rank 1 remain open.

W-1645 generalizes the prior strict-f64 seed path into one strict floating
family for `f32` and `f64`. Frontend67 materializes exact binary32/binary64
bits; HIR88 preserves distinct identities; MLIR59 and Windows44 emit direct
width-correct LLVM dialect arithmetic, comparisons, and unary negation without
fast-math. The compact width-neutral Restaurant witness owns both widths and
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
`restaurant-integer-bitwise`, `restaurant-shifts`, `restaurant-power`,
`restaurant-power-prefix`, `restaurant-compound`, and
`restaurant-uint-compound` sources/oracles own these bounded crosspoints.
`restaurant-shifts.w` executes through frontend, verified HIR, Native0/MLIR0,
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
