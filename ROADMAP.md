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
| 1 | Accelerated-domain GPU Hello sentinel | Canonical `spawn<acceleratedDomain> descriptor.field()` crosses verified W IR into separate host/device artifacts, binds the root-owned launch relation, then launches and joins through a supported provider and verifies a known payload on the available GPU; end-to-end, dispatch, and transfer metrics stay separate | Tests the common source surface, CPU/device partitioning and MLIR GPU applicability before scheduler and memory abstractions harden |
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
The seed parser and Frontend27 preserve the canonical
`accelerator.module<{...}>()` static record and ordered direct local kernel
bindings. The new `w-seed-gpu-module-1` bridge copies that meaning into
independently verified, provider- and target-neutral device-module records; its
first source-backed body slice proves zero-parameter signed-`i32` literal-return
kernels and survives source/frontend teardown. A caller-owned, provider-neutral
projection now selects one verified module field, copies its host-root const
name, kernel field label, private implementation name, and payload into the
exact GPU0 records, and remains verifiable after bridge teardown. The recipe is
still mixed MLIR 23.1.1 plus Clang 22. No typed accelerated-domain invocation,
root-owned launch relation, runtime/provider launch, public GPU build/run, or
supported GPU ABI is claimed; rank 1 therefore remains open until those public
and physical boundaries have evidence. The explicit `Launch<Module>` route
remains the later dynamic path, not the common static syntax. Only then does the
queue generalize task storage, cancellation, and scheduling.
W-1597 remains a legality certificate only; target policy must still combine it
with observability and cost facts and compare any direct-call artifact with the
W-1600 physical reference.

The current fixed task counts and worker capacities are seed evidence limits.
They must not become language, public ABI, or final runtime limits.
