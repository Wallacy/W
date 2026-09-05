# W whole-system review

Review date: 2026-09-04.

Status: non-normative review and proposed work queue. This document does not
change the language, promote implementation evidence, or approve a release.
[DESIGN.md](DESIGN.md) remains the normative authority.

## Executive assessment

W has a broad design and a real, bounded native execution path. Its strongest
opportunity is the combination of explicit ownership and costs, structured
execution, typed effects, a high-level compiler representation, and a small
integrated tool. Those parts can support an unusually coherent language.

The main risk is not a missing operator. It is the distance between that design
and verified behavior, plus contradictions where separately revised contracts
meet. Documentation and model validation have advanced much further than the
compiler, runtime, providers, usability evidence, and comparative measurements.

Do not declare a whole-language design freeze from the current evidence. Also,
do not wait for every ecosystem study before continuing implementation. First
resolve the small set of semantic contradictions below. Then use complete,
small programs to test and revise the design as implementation progresses.

Keep the ambition of exceeding other languages. Turn it into measurable goals,
not an unsupported claim that one design wins every trade-off.

## Scope, baseline, and limits

The review was performed in one context without subagents. It covers the
language surface, selected normative crosspoints, compiler architecture,
tests, benchmarks, platform support, packages, distribution, portal, studies,
documentation, repository maintenance, and AI instructions.

The inspection included the full cheatsheet, the design section map, selected
normative sections, repository instructions and maps, authored compiler and
tooling samples, manifests, CI, and selected study records. It is not a
line-by-line audit of every source file or a proof of language soundness.
Generated Tree-sitter sources were not inspected.

The starting HEAD was `01a98e5c`. An existing NCFG0/W-1535 bundle was dirty:
21 tracked files and a new `restaurant-nested-if.w` fixture. Its code and
documentation are work in progress. They were not modified or accepted by
this review. Findings about those files describe the inspected working tree,
not necessarily a regression in a released or committed version.

Evidence collected in this review:

| Check | Observation | What it does not establish |
|---|---|---|
| Existing `build/w-windows/w.exe run compiler/seed-c/fixtures/hlo0-hello.w` | Printed `Hello, world!`; command exited successfully | A rebuild of the dirty source, general W execution, or cross-compilation |
| Existing Windows executable size | 10,034,688 bytes | Complete installed toolchain size or future release size |
| `check-design-examples.mjs` | Passed, 393/393 leaf sections | Semantic validity or execution of those examples |
| `check-suite.mjs --dry-run --suite root-compiler` | Listed 26 steps | Execution or success of those steps |
| `design-slice.mjs --heading ...` | Failed before extracting the requested heading because of the pending ledger order | A language compiler failure |
| Source inspection | Concrete contract conflicts and risky code paths listed below | Exploitation or fault-injection reproduction of every risk |

No broad test suite, dependency installation, destructive cleanup, or benchmark
run was performed. No new performance result was produced. Prior receipts are
historical evidence, not fresh measurements.

## Actual product readiness

Use capabilities and evidence, not a single completion percentage.

| Area | Evidence available | Remaining product obligation |
|---|---|---|
| Language design | Broad contracts, examples, syntax corpus, design oracles | Resolve contradictions; prove interactions in the checker and runtime; test comprehension |
| Frontend | Real source, lexer, parser, diagnostics, resolution, and bounded semantic paths | General type, ownership, effect, generic, and multi-module checking |
| HIR and lowering | Verified bounded records and native subset paths | General control flow, values, resources, lifetime and effect representations |
| Native execution | Existing Windows Hello execution; Linux/WSL route and platform records | Broader programs, mandatory CI witnesses, reproducible packaging |
| Cross-compilation | Design and candidate recipes | Supported host-to-target edges, SDK/sysroot policy, target execution tests |
| Runtime and standard library | Extensive contracts and reference source | Real providers, lifecycle, concurrency, allocation and error behavior |
| Performance | Benchmark catalog, correctness references and recipes | W runtime measurements, repeatable comparisons and optimization evidence |
| Registry and trusted execution | Detailed protocol and threat-model direction | Signing, verification, clients, independent host, revocation, sandbox integration |
| Portal and tooling UX | Small portal prototype, highlighting, indexes, diagnostics catalog | Accurate capability reporting, live compiler integration, user testing |
| Web UI | Candidate embedded/browser/terminal study | Selected provider contract and real platform witnesses |

The saved W-1534 design index classifies 1,534 decisions: 116 source-backed,
505 oracle-backed, 824 implementation-evidence gaps, 81 superseded, and eight
rejected. These are decision classifications, not percentages of a working
language. A source-backed infrastructure rule is not equivalent to a complete
compiler feature. The pending W-1535 changes must not be mixed into that total.

The platform catalog has no fully supported platform or cross-compilation edge
yet. A bounded Windows witness is valuable without implying general Windows
support. HUM0 has eight slices and 32 tasks, but zero human/model observations.
The language benchmark catalog has 21 IDs in seven groups of three; those IDs
do not establish W performance results.

## Priority findings

Priority meanings: P1 blocks the relevant safety or semantic milestone; P2
should be addressed in the next development cycle; P3 is a valuable later
experiment. No P0 incident was established. The numbers below are local review
items, not W diagnostic or normative decision IDs.

### 1. Property declarations have mutually exclusive normative rules — P1

[DESIGN.md](DESIGN.md) section 7.1 says bare fields are immutable and that
`let field: T` is not a second spelling. Section 8.4 says every W property
must declare `let`, `var`, or `const`, and rejects the bare form. Both sections
contain examples that teach their opposing rule.

This is a confirmed contract conflict, not an implementation gap. Keep the
latest approved property-marker direction and remove the incompatible rule
from section 7.1. Audit structs, objects, enums, behaviors, protocols and
extensions together. Keep foreign layout fields and payload labels distinct.

Acceptance: one declaration table, positive and negative examples for every
owner kind, matching parser/checker expectations, and no contrary normative
paragraph. Avoid another complete duplicate table in each chapter.

### 2. `inout` is both forbidden and required on properties — P1

Section 7.3 states that `inout` is only a parameter/call convention and rejects
it as a property accessor or outside parameter/call use. Section 8.4 requires
`var p: inout T` to expose copy-in/copy-out from a computed property. The
[cheatsheet](CHEATSHEET.md) teaches both positions.

Recommended resolution: distinguish a property's access capability from a
storable value type. Preserve the approved property-level spelling and single
`get` accessor. Do not make an arbitrary stored `inout T` value legal by
accident. This is a recommendation for canonical consolidation, not a new rule
introduced by this report.

Acceptance must include stored versus computed places, `let`, throwing calls,
writeback, nested properties, aliases, non-`Copy` values, and forbidden escape.
Copy-in/copy-out must specify what happens when duplication is unavailable or
fallible; a spelling is not permission for an unbounded hidden clone.

### 3. Implicit `Copy` and explicit shared retain conflict — P1

Section 7.3 describes `Copy` as implicit and includes copying a shared handle
and its identity in that explanation. Section 9.4 makes `shared T` and
`weak T` move-first and requires explicit `copy handle` for another owner.
The foundational cost rule also makes retains explicit.

Clarify that preserving identity says nothing about eligibility for implicit
`Copy`. In particular, exclude an implicit retain through an aggregate field
unless a deliberate contract change authorizes it. A bounded operation can
still incur atomic traffic and contention.

Acceptance: a struct containing a shared owner cannot acquire an unmentioned
retain through assignment, argument passing, destructuring, or generic code.
Use observable owner/drop counts in the eventual runtime test.

### 4. Getter observers need a coherent mutation authority — P1 design proof

Section 8.4 gives ordinary and read-only borrowed getters a `ref self`
receiver. Section 10 and the cheatsheet demonstrate `mut willGet` updating an
access counter. A borrowed getter can also defer mutation until the borrow
ends. The inspected material does not establish the full receiver-strengthening
rule when these mechanisms compose.

Do not silently mutate non-atomic observer state through arbitrarily shared
read-only access. Select and specify a rule: stronger exclusive access for such
a property, an explicitly separate mutable state capability, or a constrained
observer implementation. Merely placing the state in a behavior does not prove
race freedom.

Removing the `modify` keyword simplified source syntax. It did not eliminate
the compiler's obligation to represent a suspended accessor continuation and
its cleanup. Specify borrow end, nested access, throw, cancellation, unwind,
and forbidden suspension. Observers are opt-in costs and can inhibit read
elimination or reordering.

### 5. Doctest syntax has two authorities — P2

Section 5.2 uses `call:` to open an inline example and explicitly says
`@example` is not the opener. Section 22.2 still says `@example` blocks are
unit doctests; section 21 also retains that vocabulary.

Make section 5.2 the single syntax definition. Other chapters should reference
it and specify runner behavior only. Exercise multiple examples, expected
errors, fixtures, source mapping, and release exclusion using actual extracted
test cases. A documentation-shaped JSON fixture is insufficient.

### 6. An executable-labelled cheatsheet block violates call labels — P2

The allocator example declares `fn prepare(city: String)` but calls
`prepare("city")`. Section 7.2.2 requires the external `city:` label.
The same example reuses owned strings and passes a scratch allocator without
demonstrating allocation through it. Its ownership and allocator lifetime
claims also require semantic review; the label defect alone is definite.

The `w-example role=executable` marker does not mean that W has executed the
example. Keep declaration plus meaningful use, as requested, but distinguish
authored examples, parsed examples, checked examples, and executed examples in
the supporting tooling. The cheatsheet itself should remain usage-focused.

### 7. Backup cleanup can undo a valid build with a damaged backup — P1

In [atomicInstallOutput](tooling/build-w-windows.mjs), the new output is
installed and the old backup is recursively deleted. If deletion partially
succeeds and then throws, the catch path restores that backup and deletes the
new output. The old backup may already be incomplete.

This risky path is visible in code. It was not exercised against user files.
After the new output rename commits, backup cleanup failure must leave the
new valid output installed. Report remaining cleanup debt separately.
Rollback belongs before that commit point. Also define recovery after a crash
between the two renames; the function name does not make the swap crash-atomic.

Acceptance: isolated filesystem fault injection for rename failures, partial
backup removal, locked files, process interruption, and recovery. Never test
destructive cases against the user's active build.

### 8. Benchmark reproducibility options need behavioral verification — P1

The Windows builder assembles `CMAKE_C_FLAGS` with semicolon-separated flags.
CMake defines that variable as a command-line string, not a CMake list. The
probe also accepts an exit code without rejecting ignored-option warnings,
and its receipt does not record the complete set of path mappings passed to
the build. See the [CMake flag contract](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_FLAGS.html).

The formatting issue is confirmed by inspection. The actual benchmark profile
was not built during this review. Use correctly quoted options or target
compile options, inspect the effective compiler invocation, and make the probe
fail when a required option is ignored. Verify two clean-directory builds,
not merely the presence of `/Brepro` in source text. Record exact compiler,
linker, SDK, flags, mappings, outputs, and debug sidecars.

### 9. Green CI does not require a native W witness — P1 evidence gate

[validate.yml](.github/workflows/validate.yml) has one Ubuntu job and no pinned
MLIR acquisition or Windows job. Native gates can report `SKIP` when the
toolchain is absent. The root compiler plan does not directly list the
standalone MLIR0 and Native0 unit gates; indirect coverage through `w run`
must be explicit and must not disappear through a skip.

Separate optional local discovery from mandatory CI execution. At least one
lane must fail if its compiler toolchain is missing and compile and execute
real W. Add a native Windows lane for the Windows claim. Add macOS before
promoting macOS support. Report passed, failed and skipped capabilities
separately; do not turn a skip into implementation evidence.

### 10. Some tests validate source spelling instead of behavior — P2

[check-design-examples.mjs](tooling/check-design-examples.mjs) accepts any
fence, table, example marker, or reference path as local evidence. Its successful
393/393 result measures presence. It does not validate syntax or semantics.
[check-mlir0-windows.mjs](tooling/check-mlir0-windows.mjs) checks implementation
substrings such as a particular `rename` call and fetch option. Dead code or a
comment can satisfy that kind of check; a correct refactor can break it.

Keep cheap inventory checks if useful, but label them correctly. Replace
security-relevant source-text assertions with fault-injected behavior tests.
Require each product gate to name the defect it detects and include at least
one known-bad mutation that makes it fail. Do not replace useful bounded model
tests wholesale: they are valuable when they test an independent rule and make
no claim about a real provider.

### 11. Reading a design slice depends on unrelated ledger validity — P2

[design-slice.mjs](tooling/design-slice.mjs) imports ledger validation before
extracting a heading. In the pending W-1535 worktree, this blocked navigation
with `ledger is not contiguous at W-1535; expected W-1532`.

Load the ledger only for operations that require it. A reader should still
extract section 7.3 while another section is being edited. Keep strict ledger
validation in its own gate. This is an authoring-workflow defect, not a reason
to weaken release validation.

### 12. Public descriptions and human readiness are stale — P2

[CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md), and parts of the
root/portal material still use pre-compiler descriptions. Other sections
describe bounded native execution. A local compiler can exist while a public
service deliberately returns unavailable; describe those independently.

Replace whole-project phrases such as "no compiler" or "compiler implemented"
with an exact supported capability. Separate a design-study completion from
provider readiness. Keep historical decisions in Git or rationale, not in
every current-status paragraph.

## Language design: highest-value opportunities

These items include proof obligations for existing choices. They do not all
require new syntax. Prefer closing an interaction over adding another feature.

### Ownership, values, and representation

1. **Publish one ownership/cost table.** Cover scalar, enum payload, struct,
   object, shared/weak owner, collection, view, closure, and existential.
   For each, state implicit copy/move, explicit duplication, borrow, mutation,
   allocation, retain, and thread-transfer behavior. Derive user examples from
   it. Do not teach "struct means cheap copy" or "object means shared owner."
2. **Make generic defaults predictable.** A known nominal object parameter
   normalizes to `ref`, while unconstrained `T` is pass-by-value. Test generic
   wrappers, specialization, protocols and existentials against the same
   program. A refactor from concrete to generic code must have an explainable
   ownership change and diagnostic, not an accidental hidden copy.
3. **Prove property composition as a place calculus.** Define projections,
   access reservations, overlapping fields, reborrows, writeback and accessor
   cleanup once. Reuse those rules in behaviors, `inout`, `mut ref`, indexing,
   pattern bindings and foreign calls. This is more valuable than restoring
   three getter spellings or another mutation keyword.
4. **Keep one primary behavior storage owner.** Preserve explicit named
   composition, deterministic hook order and collision checks. Additional
   observer fields still have layout, drop, Send and performance implications.
   Test two observers that mutate state, rejection of a second accessor owner,
   access through a borrow, and removal of an unused facet.
5. **Make CoW a proved implementation strategy, not a universal escape hatch.**
   Logical independence must survive a mutable view, shared backing, custom
   allocator and allocation failure. Uniqueness checks and retains can cost
   more than copying a small value. Compare eager copy, move/reuse and CoW on
   the same workloads before choosing defaults.
6. **Use enums as the primary modeling showcase.** Expand Last Light with
   move-only payloads, typed failures, resource-bearing cases, exhaustive
   matching, state transitions and ABI boundaries. Keep niche/tag optimization
   internal unless a stable representation was requested. A private optimized
   layout and a stable public ABI are different promises.
7. **Keep address bits separate from provenance and permission.** Pointer
   tagging examples must cover restoring valid metadata, extent, alignment,
   liveness and ownership, not just masking an integer. Use checked provider
   adapters for ordinary code and small unsafe boundaries for target-specific
   operations. Rust's own pointer documentation is a useful comparison, not a
   proof of W's model. See [Rust pointer contracts](https://doc.rust-lang.org/std/ptr/index.html).

### Calls, syntax, and functional composition

8. **Keep label binding independent of types.** The current global named-label
   mapping and positional subsequence are a good basis. Preserve textual
   evaluation order separately from ABI order. Test side effects, throwing
   arguments, defaults, allocator context, overloads and cleanup of already
   evaluated temporaries. Reordering labels is not permission to reorder work.
9. **Keep `|>` simple and useful.** Prove the unique missing argument rule and
   receiver shorthand with several chained stages on ordinary objects, not
   only arrays. Cover borrowed and consuming receivers, multiple explicit
   arguments, errors and asynchronous calls. Report an ambiguous missing slot
   directly. Do not add placeholder syntax unless real examples demonstrate
   that the current rule fails an important task.
10. **Separate functional syntax from eager/lazy semantics.** A pipe is not
    automatically lazy, parallel, or fused. Fusion must preserve effects,
    cleanup, bounds and allocation-failure behavior. Offer an explicit stream
    or iterator path where incremental consumption is required.
11. **Stabilize strings before adding more delimiters.** The chosen ordinary,
    multiline and true-raw forms need one scanner/formatter round-trip corpus
    for interpolation, escapes, hashes, Unicode, byte offsets and malformed
    endings. Include contextual character/scalar literals. Do not let portal
    highlighting define a different language from the compiler.
12. **Keep core facets exceptional and explained.** Document the boundary
    between behavior projection, execution context and ordinary methods.
    `TypeId`, `TypeInfo`, tasks and services need compact contract examples,
    including ownership and cost. Avoid converting routine library functions
    to `#` solely to make them look built in.

### Execution, effects, and failure

13. **Make `sync` availability visible across module boundaries.** The current
    design restricts direct entry to a proved non-suspending explicitly async
    declaration. It does not make arbitrary asynchronous I/O synchronous.
    Include effect summaries in binary interfaces and diagnose loss of a
    direct entry through abstraction. Show ordinary calls, `await`, `async`,
    `spawn`, and unavailable `sync` using the same small workload.
14. **Use one outcome matrix.** For value return, typed throw, cancellation,
    panic/fault and unknown remote commit, specify owner state, child draining,
    cleanup, writeback and retry authority. Keep cancellation distinct from
    a typed business error. This also explains when outcome materialization
    is useful instead of duplicating `await` with another spelling.
15. **Bound more than active task count.** A task pipeline with a concurrency
    limit can still retain all results, input storage or queued payloads.
    Specify byte and result-retention budgets, ordering head-of-line effects,
    cancellation and backpressure. Preserve the collect form; add or reuse an
    incremental result form only where the workload needs it.
16. **Do not promise prompt cancellation without a cooperation boundary.**
    First-settled selection can still wait for losing tasks to drain. A child
    that does not cooperate can block structured exit. Distinguish selection,
    resource release and response delivery. Test a slow loser and a failing
    cleanup, not only two immediately completed tasks.
17. **Preserve transaction uncertainty.** The current `unknownCommit(EffectId)`
    direction is necessary at remote failure boundaries. A timeout cannot prove
    rollback. Keep reconciliation and idempotency explicit. Explain that
    `commit` in a dependent pipeline selects output, while transaction commit
    requests a provider operation; shared spelling does not make them the same
    guarantee.
18. **Make simulated effects an executable seam.** Reuse effect identities,
    ordering and capability checks between deterministic tests and providers.
    Simulation approval must bind the exact input, environment assumptions and
    permitted effects. Invalidate approval when those facts change. A simulated
    success is not authorization for a different production action.

### Allocation, optimization, and target diversity

19. **Separate logical allocation budgets from physical placement.** `.none`
    must not be described as "all values live in registers" or "everything is
    stack allocated." Stack, region, heap and virtual-memory capabilities need
    distinct limits. A large stack frame or escaping borrow is not free merely
    because heap allocation is forbidden. Diagnostics should identify the
    operation that requires unavailable storage.
20. **Define optimization legality for allocation failure.** Section 18 already
    constrains transformations by observable failures and cleanup. Close how
    elision, fusion, CoW and region reuse preserve that rule. If allocation
    counts themselves are observable budget charges, preserve logical charges
    or explicitly define the permitted abstraction. Do not promise both exact
    physical allocation behavior and arbitrary allocation removal.
21. **Preserve numerical intent.** Keep checked, wrapping, saturating, carry and
    strict/fast/reproducible modes explicit. Release builds must not silently
    enable reassociation or remove safety. Test overflow, signed zero, NaN,
    reduction order and SIMD tails. Numeric mode belongs in artifact and
    benchmark identity.
22. **Keep memory mapping as a provider capability.** MEM0 correctly separates
    file-backed, anonymous and device memory. Implement one useful witness
    before a universal mapping abstraction: immutable file/model data with
    bounded views, truncation/interference behavior, and deterministic release.
    Compare it with buffered I/O; page faults and address-space pressure can
    reverse the expected performance advantage.
23. **Keep execution portable without pretending every target is an OS.**
    Separate execution context from process, window, filesystem and network
    providers. GPU/FPGA support also needs address spaces, synchronization,
    resource limits and a target runtime, not merely an LLVM target triple.

## Compiler and runtime architecture

### Preserve what is already valuable

Keep verified HIR boundaries, caller-owned bounded seed data, explicit failure
statuses and transactional publication of results. Keep the separation of
semantic and provenance identity. Keep the C23 route as bootstrap/recovery and
comparison evidence; it need not be the primary production backend.

The long-term route should preserve W semantics into typed high-level IR and
lower progressively to MLIR/LLVM and the platform linker. MLIR explicitly
supports retaining higher-level structure for transformations. Its presence
alone does not supply W's ownership, effects, ABI, runtime or optimizer.
See [MLIR rationale](https://mlir.llvm.org/docs/Rationale/Rationale/).

### Make the next increment general within a small scope

The current bounded lowering and output construction are not yet a general
runtime implementation. The next useful evidence is a program whose result
depends on runtime input, then a loop, enum/result branching, allocation and
deterministic cleanup. Do not grow a compiler that primarily proves more
constant output strings at compile time.

The fixed record capacities are bootstrap resource budgets, not CPU registers
or permanent language limits. Generalization should use bounded arenas or
segments, checked growth, clear out-of-capacity diagnostics, and measured high
water marks. Raising every capacity wastes compiler memory and can enlarge
stack/static storage without improving generated code.

### Invest in retained facts and precise invalidation

Carry ownership, alias sets, extents, uniqueness, enum discriminants, effects,
numeric mode and target requirements until the relevant lowering proves them
unnecessary. Use them for bounds-check removal, move-to-in-place reuse,
devirtualization, dead metadata removal, vectorization and region placement.
Emit optimization remarks explaining both a successful optimization and why
one was blocked.

MLIR bufferization can help an appropriate tensor representation choose and
reuse buffers. It is not a substitute for W's general ownership checker or
resource lifetime rules. See [MLIR bufferization](https://mlir.llvm.org/docs/Bufferization/).

Design incremental compilation around stable module/interface identities and
dependency queries. Separate interface changes, implementation changes and
target/profile changes. Cache reuse must include compiler version, dependency
closure, effect and ABI summaries, features, environment inputs and recipe.
ThinLTO is a useful complementary mechanism for cached inter-module native
optimization, not a complete W incremental build system. See
[ThinLTO](https://clang.llvm.org/docs/ThinLTO.html).

### Refactor when boundaries are proved

The authored frontend is roughly 683 KB and HIR implementation roughly 210 KB
in the inspected tree. Size alone does not prove bad code, but these files put
many invariants in one review surface. Extract ownership-based pass boundaries,
record/range validation and target adapters after executable tests constrain
them. Avoid a wholesale rewrite or a generic framework that only moves the
same complexity elsewhere.

Add sanitizer builds, fuzzing of source and serialized boundaries, invalid-IR
rejection, and differential compiler tests. Sanitize the seed implementation
as well as eventual generated programs. Keep independent reference behavior;
do not derive both expected output and output under test from the same helper.
[AddressSanitizer documentation](https://clang.llvm.org/docs/AddressSanitizer.html)
describes one useful implementation check, not a language-safety proof.

## Performance ambition and measurement strategy

W has no demonstrated general performance advantage yet. Measure before ranking.
The objective is the best practical combination of safe idiomatic performance,
fast development, small distribution and small applications.

Safety and correctness are acceptance constraints. Runtime performance has
priority over bundle-size savings, as requested. A separate size experiment
can inform optimization, but it must not silently become the release default.

| Objective | Measure | Initial decision rule |
|---|---|---|
| Execution | Throughput, median and tail latency, cycles where useful | Compare equal work and report uncertainty; investigate repeatable regressions |
| Working memory | Peak live bytes, allocations, retains, RSS | Separate language/runtime cost from provider and OS caches |
| Compilation | Cold build, no-op build, local edit, interface edit, link | Record stage timings, cache hits and peak compiler memory |
| Tool distribution | Compressed download, installed bytes, optional target packs | Retain the provisional 64 MiB client goal; count required components honestly |
| Final application | Code, data, metadata, imports, debug sidecars, startup | Reduce unused components without removing safety or worsening execution |
| Security | Invalid programs rejected, adversarial transitions, sanitizer/fuzz results | A speed improvement cannot buy acceptance of unsafe behavior |
| Usability | Explain, repair and change tasks; diagnostics; success rate | Collect actual observations rather than infer quality from syntax coverage |
| AI maintenance | Correct accepted change, review defects, reruns, latency and usage where observable | Optimize useful verified work, not a target percentage for one model |

Retain learner, idiomatic and frontier variants. Label unsafe use, algorithm
changes, representation changes, target specialization and external native
libraries. C23, Rust and Zig are valuable native comparisons; other languages
are useful for relevant application or development-loop comparisons. Do not
make a learner baseline in another language the headline competitor.

Start with the existing byte scan and add runtime scalar/control-flow,
enum dispatch, copy/move, allocation lifecycle, text/collections and concurrency
as their implementations become real. Use empty, small, large, skewed,
malformed and boundary inputs. Include cold caches, cancellation, resource
exhaustion and contention where relevant. Report environment and confidence
intervals; use a pinned performance host rather than treating noisy shared CI
as a precise ranking system.

Optimization opportunities to revisit periodically:

- ownership-based buffer reuse and allocation/retain elimination;
- enum niche encoding, compact cold data and metadata reachability;
- bounds-check hoisting with exact alias and extent proofs;
- generic specialization budgets with shared fallback bodies;
- incremental frontend queries, interface caches and parallel independent work;
- ThinLTO, PGO and target-aware code layout after representative workloads exist;
- demand-linked runtime components, target packs and stripped debug sidecars;
- no-op build paths that do not start the compiler unnecessarily.

Do not turn a sub-kilobyte Hello into the main project. The previously reported
small Hello artifact was not remeasured here. It is a useful size witness, not
a predictor of a real application's runtime, library or debug footprint.

## Registry, distribution, security, and cross-compilation

### Binary first, but with precise guarantees

The current registry direction already separates simple discovery from trusted
artifact identity. Preserve that separation. HTTP/1.1, HTTP/2 and HTTP/3 should
carry the same protocol; HTTP/3 is not a minimum requirement for a mirror.
Search results, channel pointers and update JSON are convenience data, not
permission to run arbitrary bytes.

Use audited cryptographic implementations and established update-security
roles rather than inventing cryptography. The TUF specification is a primary
reference for rollback, freeze, role and key-rotation concerns. W-specific
metadata should compose with such mechanisms and have independent test
vectors. See [TUF specification](https://theupdateframework.github.io/specification/latest/).

Closed source publication needs separate claims for source availability,
builder attestation, reproducibility, analysis performed and current known
vulnerabilities. A CI build-and-delete workflow does not prove that the runner
never retained source through logs, caches, debug data, network or snapshots.
Secret isolation, fork policy, short-lived credentials and retention rules
belong in the threat model. Never market a successful scan as proof of safety.

A static archive cannot always become a dynamic library without a compatible
PIC, export, TLS and initialization policy. Intermediate representation is
also tied to a compiler/IR version and does not magically preserve all source
level specialization. Define native artifact variants, interface summaries,
optional IR and closed-source generic strategies explicitly.

Public artifact hashes identify exact distributed bytes. Inlining, relinking,
relocation and specialization can change the consumer's bytes, so a universal
runtime fingerprint of a library is not guaranteed. Use origin manifests,
signed build receipts and retained provenance where appropriate.

### Trusted execution is not automatic sandboxing

An official registry signature does not replace OS signing, platform policy,
runtime isolation or user approval. Distinguish verified publisher, verified
build and granted capability. Native code requires an enforceable isolation
boundary to restrict its authority. Registry trust policy should be explicit,
support private registries, and not become a hidden central lock-in.

Revocation must define offline behavior, expiry, clock rollback and emergency
recovery. CVE monitoring has false positives, incomplete component mapping and
zero-day blind spots. Show analysis scope and date rather than a single green
security badge. Runtime DRM or attestation cannot provide absolute secrecy
against a host controlled by its owner.

### Small tool, explicit target packs

Keep the common CLI/frontend small and make target toolchains, sysroots and
providers separately identifiable and cacheable where practical. Measure
first-install total, core download and target-pack download separately.
Moving required megabytes to a first-run download is not a size optimization.

For each host-to-target edge, prove compiler execution, object/link output,
ABI and library compatibility, target startup and actual execution. Linux,
Windows and macOS also require distinct SDK and distribution considerations;
legal redistribution of SDK assets needs explicit review, not an assumed
right. Having an MLIR/LLVM backend is necessary but not sufficient for the
desired cross-compilation experience.

## Portal, services, web UI, and machine learning

Keep the portal's small dependency surface. A framework migration is not the
next compiler milestone. First make status honest, load snippets from shared
authored sources, and connect real diagnostic output only when that endpoint
exists. Preserve a fail-closed unavailable endpoint rather than simulate
successful compilation.

For the future Worker/Assets/R2 deployment, isolate portable registry objects
and request/response contracts from Cloudflare adapters. A static catalog is a
good initial discovery implementation. Introduce an indexed search store when
catalog size, latency or update contention demonstrates the need. Public search
and authenticated publication are different surfaces. Do not create a large
empty service scaffold now simply because those services will eventually exist.

Services need real local, IPC and network implementations of the same typed
contract, with payload bounds, backpressure, close semantics and error mapping.
A local adapter passing tests does not prove a remote transaction or a device
queue. Reuse effect/provider boundaries, not an assumption that transports
have identical failure behavior.

[WVUI0](tooling/studies/wvui0-web-ui-providers/README.md) remains a candidate
study with open questions and no provider witness. It is not research-complete
in the sense of a selected, validated API. Its separation of embedded, browser,
semantic terminal and raster terminal providers is useful. Preserve explicit
availability and no silent fallback. Implement one typed command round trip
and window/session close before a large UI standard library. Test bridge
authority, origin, replay, stale generations and terminal control-byte handling.
Semantic terminal rendering must not promise arbitrary CSS or JavaScript.

[LLM0](tooling/studies/llm0-training-inference/README.md) is a completed design
study, not an implementation of training or inference. Focus W's contribution
on ownership of model weights and KV state, bounded scheduling, data movement,
layout, numeric intent, device synchronization and integration. Keep model
architectures, distributed training algorithms and kernel catalogs outside the
core language. Evaluate an existing MLIR-based runtime such as
[IREE](https://iree.dev/) as an integration candidate when a real workload is
available; do not adopt another stack from its feature list alone.

## Repository structure, dependencies, and information

### Reduce repeated authority, not useful evidence

The tracked-file inventory contained 1,325 files: 952 under tooling, 150 under
compiler, 127 under reference, 33 under std, 14 under benchmarks and 14 under
portal. Working-tree file sizes included roughly 1.68 MB for DESIGN, 1.02 MB for
RATIONALE and 2.95 MB for the design classification JSON. The root package has
180 scripts. These measurements identify review cost, not automatic deletion
targets. Unicode data and executable fixtures can be large for good reasons.

Recommended organization:

- one authoritative contract per concept, with stable links;
- a short human status page with capability-specific evidence and next steps;
- the usage-only cheatsheet and generated diagnostic/study navigation;
- implementation and operational guidance close to its source;
- historical rationale separate from current tutorials and current status;
- machine catalogs for validation, without copying the same prose or full
  snapshot repeatedly into unrelated manifests.

Start by removing duplicate rules within DESIGN. Splitting its physical files
can follow as a mechanical operation with stable anchors and a clear authority
map. Do not begin with a mass move that invalidates every evidence digest.
Generate projections once after a decision stabilizes. Prefer dependency-local
digests over a global hash cascade for unrelated changes.

### Complete English migration at the sources

Many current contracts, maps and generated templates remain Portuguese or
mixed-language. Translate authoritative text and generator templates first;
then regenerate projections. Keep terminology consistent: ownership, borrow,
view, copy, duplicate, task, execution, capability and provider need a small
glossary. Preserve public anchors or provide intentional migration links.
Do not introduce parallel manually maintained English and Portuguese specs.

The cheatsheet should show each approved syntax family with declaration and
meaningful use, plus a few carefully selected invalid boundaries. Operational
gate counts and model instructions belong elsewhere. Use the same examples
for documentation, parser/checker tests and eventual doctest execution, but
retain independent expected behavior.

### Keep useful packages; update exact recipes deliberately

The current C23 seed, CMake, LLVM/MLIR/LLD, Bun and Tree-sitter choices do not
justify a stack rewrite. C23 is the primary C direction; C11 recovery should
remain explicit and narrow. Editor compatibility floors are not dependency
pins and need not be raised to the latest editor release on every update.

Official releases checked on the review date show:

| Component | Repository observation | Review action |
|---|---|---|
| Bun | 1.4.0 selected | Evaluate the newer stable [1.4.1 release](https://github.com/oven-sh/bun/releases/tag/bun-v1.4.1) and refresh the actual pins and receipts |
| Tree-sitter CLI | 0.27.0 | Matches the checked [0.27.0 release](https://github.com/tree-sitter/tree-sitter/releases/tag/v0.27.0); no churn needed |
| actions/checkout | SHA pinned for 7.0.1 | Preserve immutable pin; matches the checked [7.0.1 release](https://github.com/actions/checkout/releases/tag/v7.0.1) |
| setup-bun | SHA pinned for 2.2.0 | Preserve immutable pin; matches the checked [2.2.0 release](https://github.com/oven-sh/setup-bun/releases/tag/v2.2.0) |
| LLVM recipes | Windows 23.1.0; Linux/WSL 20.1.2 | Keep old receipts historical; qualify a Linux successor against the checked [23.1.0 release](https://github.com/llvm/llvm-project/releases/tag/llvmorg-23.1.0) |

`dependency-currency.json` records an observation date of 2026-08-31. A local
gate comparing its `current` and `latestStable` fields cannot discover a new
upstream release. Separate offline consistency validation from online release
discovery. Update a recipe only after its real affected gates pass. Do not
rewrite historical performance evidence to pretend it used the new toolchain.

### Make tests and cleanup proportional

The suite manifest can remove duplicate aliases but cannot automatically
deduplicate repeated work inside nested package scripts. The studies umbrella
also overlaps individual study tests. Flatten the execution plan at the actual
test/build unit, then measure before changing it.

Use one build per profile and toolchain in a bundle. Share compilation between
related unit gates. Record short summaries and keep detailed logs as local
failure artifacts. A documentation-only edit should not rebuild the native
compiler. A native edit should not trigger every unrelated study oracle.

The Markdown checker currently walks directories and excludes only `.git` and
`node_modules`; build output can enter its scan. Use the tracked authored set
plus explicit new files, or an equivalent documented allowlist.

Preserve the existing cleanup safeguards. Add checkout-scoped owner records
and retention policies before automating more deletion. Clean successful
temporary build artifacts at a safe checkpoint, retain one active build and
small failure evidence, and never remove caches merely because a directory is
large. Do not conflate disk cleanup with deletion of useful design evidence.

## Efficient AI-assisted development

The workflow already has useful finite milestones, status boundaries, explicit
stop conditions, pause/resume rules, and a prohibition on idle loops. Keep
those. The review used the OpenAI Docs skill to check how repository
instructions are discovered and layered; that informed the recommendations
below, not the language design.

The current instructions hard-code Sol/Luna roles and a 95% Luna token target.
They can conflict with a direct user request, an unavailable model or a runtime
that cannot expose model metadata. Replace those brittle requirements with
capability and authority rules. Use the user's requested execution mode; never
block work just to satisfy an unverifiable model label or token split.

OpenAI documents hierarchical repository instructions and a default size bound
for the assembled instruction text. Keep top-level instructions concise and
use scoped guidance where it applies. See
[Codex repository instruction guidance](https://developers.openai.com/codex/guides/agents-md).

Proposed operating loop:

1. Record the requested outcome, authority, dirty files, relevant contracts,
   bounded write scope and one completion criterion.
2. Inspect only the needed sources. Treat indexes as navigation, not proof.
3. Resolve architectural questions before a mechanical bundle. If the user
   requests a review, do not silently implement the recommendation.
4. Make a coherent change. Run the narrowest relevant product witness and
   targeted adversarial checks. Reuse an unchanged successful result only when
   its source, toolchain and environmental inputs remain valid.
5. Review the diff for contract, safety, performance and user-facing effects.
   Generate dependent projections once; validate their consistency separately.
6. Commit authorized, explicit paths. Save a compact checkpoint: SHA, dirty
   paths, last commands, outputs, pending decisions and next finite action.
7. Stop when complete or when a user decision is required. A timeout or an
   unchanged state is not progress and is not a reason for another status loop.

For each new test, ask: What product failure makes this fail? Is the oracle
independent? Is a real compiler/provider exercised? What changed that requires
rerunning it? A mock-only test can be appropriate, but its name and evidence
claim must say so.

Measure accepted task outcomes, review defects, repeated gate time, failed
edits, context rereads and usage when the runtime exposes it. Do not infer
quality from output length, number of tests, number of agents or model effort.
Use progress updates for new evidence or decisions. Do not impose technical
prose rules on hidden reasoning or turn every chat reply into a formal report.

## Proposed finite work queue

### Remediation status

The project owner requested prioritized remediation after this review. The
review findings remain recorded above. Their implementation status is tracked
here rather than in another task catalog.

The economic workflow uses one Luna Max worker for a closed implementation
bundle. The coordinator owns the contract, prioritization, and diff review.
Existing NCFG0/W-1535 changes remain separate and must not enter a maintenance
commit by accident. No broader design recommendation is ratified merely by
its inclusion in this queue.

| Findings | State | Next bounded action |
|---|---|---|
| 7 — build publication | Fixed, bounded filesystem evidence | Pre-commit restoration and post-commit partial-cleanup tests pass. Crash recovery remains outside this fix |
| 8 — reproducibility | Recipe and probe fixed; full-build comparison deferred | Space-separated flags, strict option handling, complete mappings and scoped receipt validation pass. Compare complete W builds after NCFG0 closure |
| 1–4 — properties and ownership | Queued, semantic priority | Reconcile approved syntax, explicit retain rules, and observer mutation authority before general property lowering |
| 9 — native CI | Queued | Require pinned native execution and distinguish unavailable local tools from failed CI prerequisites |
| 5–6, 11–12 — documentation and navigation | Queued | Consolidate doctest rules, repair examples and readers, and update capability-specific status |
| 10 — test quality | Queued, applied to new fixes now | Use behavior and known-bad mutations for changed tooling, then remove measured redundant checks |

The first maintenance bundle changes
[`build-w-windows.mjs`](tooling/build-w-windows.mjs), its
[behavior tests](tooling/build-w-windows.test.mjs), and the
[seed build instructions](compiler/seed-c/README.md). Publication returns the
installed path after commit, even when backup cleanup fails. It reports the
remaining backup separately and never restores a partially removed backup.
The recovery diagnostic distinguishes that backup from caller-owned staging.

Validation completed on 2026-09-04:

- `bun test tooling/build-w-windows.test.mjs`: 19 tests passed.
- `bun run check:mlir0-windows`: 24 tests passed, followed by the offline
  structural check. The total includes the builder tests, not 24 extra tests.
- A small real CL/link/CMake/Ninja smoke passed with directory names containing
  spaces. MSVC rejected an invalid option with D8043 and exit code 2.
- `git diff --check` passed. Test-owned temporary files were removed.

The native smoke is local evidence, not a mandatory CI lane or a complete W
build. No process-interruption test or real locked-file test was performed.
The implementation was reviewed by the coordinator, not an independent human.

The build-safety bundle has `benchmarkDisposition: deferred`. The follow-up
task `windows-build-reproducibility` requires a clean, reviewed source baseline
after NCFG0 closure. It ends with two complete W builds under equivalent
recipes, compared artifacts, and retained receipts. Filesystem fault tests and
small compiler probes do not satisfy that follow-up or establish performance.

### Ordered bundles

Effort is relative: S means a narrow correction; M means a cross-file bundle;
L means a capability requiring implementation and platform evidence. These are
not elapsed-time promises. Each row should become one bounded task only when
selected, not an always-running goal.

| Order | Bundle | Priority / effort | Completion and stop condition |
|---|---|---|---|
| 1 | Build publication and reproducibility | P1 / M | Isolated fault tests prove safe post-commit cleanup; actual options and two clean build receipts agree |
| 2 | Property/ownership contract reconciliation | P1 / M | Findings 1–4 resolved in one canonical table and counterexamples; no new syntax family |
| 3 | Mandatory native product CI | P1 / M | Windows and pinned Linux witnesses execute W; missing dependencies fail; skip counts remain explicit |
| 4 | Finish the paused NCFG0 bundle | P2 / M | Review current dirty code, update only required projections and run affected native gates; explicit commit |
| 5 | Honest docs and example consolidation | P2 / M | Findings 5, 6, 11 and 12 addressed; English current-status surface; documentation examples have accurate evidence states |
| 6 | Runtime-input vertical slice | P2 / L | CLI input affects output at runtime; error path and a loop/branch execute; no expected-output shortcut |
| 7 | Enum/result and ownership vertical slice | P2 / L | Resource-bearing enum, match, borrow/move and typed failure work through native execution |
| 8 | First comparative performance loop | P2 / M | Real W measurements for an implemented unit, independent correctness, three disclosed variants, pinned recipe |
| 9 | Test/build execution efficiency | P2 / M | Measured duplicate work removed; changed-input selection and one shared build; no lost product mutation detection |
| 10 | Incremental compiler and diagnostic UX | P2 / L | No-op/local/interface-edit benchmarks plus clear ownership/effect diagnostics on real source |
| 11 | Registry verification vertical slice | P2 / L | Static third-party host, signed artifact, install/run policy, expiry/revocation and adversarial verification |
| 12 | Cross-target distribution | P2 / L | One additional real host-to-target edge, SDK/ABI proof, target execution and complete size receipt |
| 13 | Services/runtime lifecycle | P2 / L | Same contract through local and one external provider; cancellation, close, bounds and failure tested |
| 14 | Web UI and terminal provider selection | P3 / M then L | WVUI0 questions resolved for one provider; real typed command and close/security witness |
| 15 | Mapping/device/LLM performance integration | P3 / L | One measured useful workload per chosen provider; no core-language expansion from a capability list |

The broad English migration and physical documentation split should be staged
with their owners, not bundled into unrelated compiler changes. Release
qualification, dependency updates and cleanup are routine bounded maintenance,
not reasons to reopen the whole design every cycle.

## Where human review has the greatest leverage

Start with property/ownership sections 7.1, 7.3, 8.4, 9.4 and 10. Decide the
observable contracts in findings 1–4 before debating more punctuation.
Then review the outcome matrix across calls, tasks, pipeline transactions and
cleanup. Finally, try small tasks from the cheatsheet without consulting the
rationale; record what was unclear, not just which spelling was preferred.

The author can resolve specific design choices. Other readers are still needed
to establish learnability. Run a small real HUM0 session before expanding its
protocol. A concise failure to explain one ownership example is more useful
than another hundred mock observations.

## Coverage and remaining review limits

| Surface | Review treatment | Next evidence needed |
|---|---|---|
| Design 0–4: authority, invariants, diagnostics and integrated surface | Structure, claims, classification and navigation inspected | Capability-based status and coherent diagnostic ownership |
| Design 5–8: source, modules, calls and types | Deep review of strings, labels, ownership, properties and examples | Unified contracts and real semantic tests |
| Design 9–12: memory, behaviors, failures and execution | Deep crosspoint review and bounded implementation sampling | Lifetime/effect/cleanup implementation proofs |
| Design 13–17: services, std, numerics, text and tensors | Contract map, examples and relevant studies | Real provider, library and numeric witnesses |
| Design 18–20: costs, FFI and compiler | Architecture, code samples, recipes and primary references | General lowering, sanitizers, measurements and ABI tests |
| Design 21–23: builds, registry and ecosystem | Protocol and threat-model review; tooling and study inspection | Independent host, verifier, runner and provider conformance |
| Design 24–26: freeze, reference product and implementation plan | Evidence classification and current subset checked | Milestones tied to executed product behavior |
| Repository, CI, dependencies and AI | Instructions, scripts, source checks, packages and live release references | Narrow maintenance bundles and measured execution savings |

No claim is made that this review found every bug, that all candidate syntax is
implementable at its desired cost, or that any untested platform works. The
conclusion is narrower and actionable: preserve the strong architecture,
resolve demonstrated contradictions, make evidence honest, and move the next
major investment toward running programs and comparative measurements.
