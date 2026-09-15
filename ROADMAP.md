# W implementation roadmap

This is the ranked active implementation queue. It is ordered by dependency
and learning value, not by feature visibility. Each item must end in one
bounded, executable observation that becomes an input to the next item.
Completed work belongs in the current capability documents and Git history,
not in this queue.

## Execution principles

- Prefer vertical source-to-native slices over isolated infrastructure.
- Generalize only after a bounded implementation exposes the real invariants.
- Keep logical semantics independent from storage, worker, and seed capacities.
- Preserve verified HIR as the authority and lower directly through MLIR/LLVM.
- Default native products are CRT-free and statically close W-owned runtime and
  standard-library code by reachability.
- Optimize the largest proved closed graph; retain only observable product and
  ABI roots.
- Elide a declared execution relation only with an independently verifiable
  equivalence fact; compare the optimized path against the physical reference.
- Measure public executable behavior only after an exact correctness oracle.
- Treat performance, memory, binary size, and compile latency as persistent
  optimization signals, never as permission to change semantics.

## Ranked queue

| Rank | Increment | Completion boundary | What it enables |
| ---: | --- | --- | --- |
| 1 | Accelerated-domain GPU Hello sentinel | Canonical `spawn<domain> descriptor.field()`, with an accelerated domain, crosses verified W IR into separate host/device artifacts, binds the root-owned launch relation, then launches and joins through a supported provider and verifies a known payload on the available GPU; end-to-end, dispatch, and transfer metrics stay separate | Tests the common source surface, CPU/device partitioning and MLIR GPU applicability before scheduler and memory abstractions harden |
| 2 | Capacity-independent task storage | Replace the seed one-to-four logical-task arrays with measured caller-owned records; logical task count and physical worker capacity remain separate; configured exhaustion fails before effects | Removes an implementation ceiling before scheduler generalization |
| 3 | Structured cancellation and outcomes | Request, propagation, cleanup drain, typed failure, panic boundary, and deterministic outcome publication execute through the same native route | A usable structured-concurrency core rather than successful scalar jobs only |
| 4 | Provider-neutral scheduler core | Target-neutral ready/task/frame state lowers once; Windows and Linux providers supply only platform primitives and cached topology/capacity facts | Portable concurrency without a platform-shaped language ABI |
| 5 | Resource-bearing enum and ownership slice | `Result`-like payload enum, exhaustive match, move, borrow, cleanup, typed success/failure, and adversarial rejection execute natively | Concrete memory-management invariants for general aggregates and tasks |
| 6 | General verified HIR and lowering | Expand types, calls, CFG, ownership/effects, and diagnostics in small executable slices while preserving independent verification | Moves from bounded demonstrations toward ordinary programs |
| 7 | Closed-graph optimizer and static closure | Whole-module is the minimum optimized region; proved package/workspace/product closure internalizes and eliminates unused WRT/std/provider code without changing observable roots | Generalizes the optimization rules first exercised by the Hello sentinel |
| 8 | Incremental compiler and test selection | No-op, local-body, interface, and product-root edits reuse exact artifacts and run only risk-relevant gates, with mutation evidence that product failures are still detected | Faster human and AI iteration without hollow green checks |
| 9 | Native toolchain and cross-target distribution | Reproducible signed LLVM/MLIR/LLD target packs cover the Windows/Linux/macOS host-target baseline, with exact SDK and ABI provenance | Compact offline `w` distribution and supported cross compilation |
| 10 | Package, registry, service, and sandbox slices | Signed binary-first package plus source fallback, independent verification, one local/external service provider, and bounded sandbox execution | Ecosystem work built on a stable executable/runtime boundary |
| 11 | UI, scientific, accelerator, and proof-mode witnesses | Promote one real application or numerical workload at a time through correctness, target applicability, resource receipts, and benchmark evidence; the GPU Hello remains the earlier architecture sentinel | Broader targets without expanding the language from untested abstractions |

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

W-1601 now supplies the target-neutral two-function/seven-operation GPU0
witness, separate host/device MLIR, GPU/NVVM/PTX lowering, actual result `42`
on the available RTX A400, and a diagnostic-only in-process metric snapshot.
The seed parser and Frontend28 preserve the canonical
`accelerator.module<{...}>()` static record and ordered direct local kernel
bindings. The new `w-seed-gpu-module-1` bridge copies that meaning into
independently verified, provider- and target-neutral device-module records; its
first source-backed body slice proves zero-parameter signed-`i32` literal-return
kernels and survives source/frontend teardown. A caller-owned, provider-neutral
projection now selects one verified module field, copies its host-root const
name, kernel field label, private implementation name, and payload into the
exact GPU0 records, and remains verifiable after bridge teardown. The recipe is
still mixed MLIR 23.1.1 plus Clang 22. Frontend28 plus ACCINV0 preserve and
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
execution; the CUDA adapter remains a private process boundary. Runtime/provider launch/join/result,
public GPU build/run, and a supported GPU ABI remain open;
rank 1 therefore stays open until those public and physical boundaries have
evidence. The explicit
`Launch<Module>` route
remains the later dynamic path, not the common static syntax. Only then does the
queue generalize task storage, cancellation, and scheduling.
W-1597 remains a legality certificate only; target policy must still combine it
with observability and cost facts and compare any direct-call artifact with the
W-1600 physical reference.

The current fixed task counts and worker capacities are seed evidence limits.
They must not become language, public ABI, or final runtime limits.

W-1606 begins rank 2: HIR38 no longer uses a four-slot product array as an
admission or verification limit for `.main` or `.domain`, and PARSEL1 measures
dense caller-owned `.domain` task relations independently from worker capacity.
Five siblings now cross verified HIR in both lanes and cross PARSEL1 for
`.domain`; legacy product/selector consumers still stop at four until their
incremental migration.

W-1607 extends rank 2 through invocation storage. PARINV1 measures dense task
and argument relations from verified PARSEL1, normalizes named arguments into
parameter order, and removes the scalar evaluator's fixed sixteen-argument
array. Five tasks and seventeen arguments pass this compiler boundary. The
physical providers and PARMLIR0 still consume the fixed compatibility records;
their migration is the next rank-2 dependency before cancellation work.

W-1608 removes the fixed logical-task arrays from the Windows component.
PARPROV1 executes five tasks through constant-size worker waves and produces
the same semantic result at capacities one and two. It also executes the
seventeen-argument witness. W-1609 completes rank 2 for the compiler/component
path: PARMLIR1 emits five runtime-parameterized task entries from measured
records, accepts the seventeen-argument witness, preserves compatible PARMLIR0
bytes, and lowers the five-task artifact to Windows COFF and Linux PIC ELF with
MLIR/LLVM 23.1.1. Public process products retain their bounded compatibility
route. Structured cancellation and outcomes are now the next active increment.

W-1610 opens rank 3 without reintroducing a storage ceiling. TASKLIFE1 runs the
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

Next comes an explicit panic boundary. Physical cancellation remains
cooperative rather than thread termination.
