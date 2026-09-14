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
| 1 | Native Hello optimization sentinel | The smallest current public Hello is rebuilt from source on Windows and Linux with static whole-graph release closure; correctness is mandatory and the live catalog retains artifact size, compile time, wall/CPU time, and memory without retaining binaries | Early feedback for section elimination, linkage, ABI, startup, and the target-specific sub-1-KiB opportunity |
| 2 | GPU kernel Hello sentinel | One W host plus one target-neutral GPU function writes a known payload to a device-visible result, launches and joins through an explicit domain, and verifies it on the available GPU; device and host artifacts and end-to-end/dispatch/memory metrics stay separate | Tests CPU/GPU partitioning and MLIR GPU applicability before scheduler and memory abstractions harden |
| 3 | Capacity-independent task storage | Replace the seed one-to-four logical-task arrays with measured caller-owned records; logical task count and physical worker capacity remain separate; configured exhaustion fails before effects | Removes an implementation ceiling before scheduler generalization |
| 4 | Structured cancellation and outcomes | Request, propagation, cleanup drain, typed failure, panic boundary, and deterministic outcome publication execute through the same native route | A usable structured-concurrency core rather than successful scalar jobs only |
| 5 | Provider-neutral scheduler core | Target-neutral ready/task/frame state lowers once; Windows and Linux providers supply only platform primitives and cached topology/capacity facts | Portable concurrency without a platform-shaped language ABI |
| 6 | Resource-bearing enum and ownership slice | `Result`-like payload enum, exhaustive match, move, borrow, cleanup, typed success/failure, and adversarial rejection execute natively | Concrete memory-management invariants for general aggregates and tasks |
| 7 | General verified HIR and lowering | Expand types, calls, CFG, ownership/effects, and diagnostics in small executable slices while preserving independent verification | Moves from bounded demonstrations toward ordinary programs |
| 8 | Closed-graph optimizer and static closure | Whole-module is the minimum optimized region; proved package/workspace/product closure internalizes and eliminates unused WRT/std/provider code without changing observable roots | Generalizes the optimization rules first exercised by the Hello sentinel |
| 9 | Incremental compiler and test selection | No-op, local-body, interface, and product-root edits reuse exact artifacts and run only risk-relevant gates, with mutation evidence that product failures are still detected | Faster human and AI iteration without hollow green checks |
| 10 | Native toolchain and cross-target distribution | Reproducible signed LLVM/MLIR/LLD target packs cover the Windows/Linux/macOS host-target baseline, with exact SDK and ABI provenance | Compact offline `w` distribution and supported cross compilation |
| 11 | Package, registry, service, and sandbox slices | Signed binary-first package plus source fallback, independent verification, one local/external service provider, and bounded sandbox execution | Ecosystem work built on a stable executable/runtime boundary |
| 12 | UI, scientific, accelerator, and proof-mode witnesses | Promote one real application or numerical workload at a time through correctness, target applicability, resource receipts, and benchmark evidence; the GPU Hello remains the earlier architecture sentinel | Broader targets without expanding the language from untested abstractions |

## Current checkpoint

W-1600 closes the bounded physical process/parallel reference: the unchanged
source consumes runtime input, crosses explicit target-provider launch and
lexical join, validates the emitted task result, and exits through CRT-free
Windows and Linux/WSL products. W-1599 remains a separate target-neutral
lifecycle oracle; it is not the storage or scheduler used by that product.

The next increment is rank 1, the native Hello optimization sentinel, followed
by the GPU kernel Hello sentinel. Only then does the queue generalize task
storage, cancellation, and scheduling. This keeps early size, startup, target
partitioning, and GPU-applicability evidence available while those later
representations are still cheap to change. W-1597 remains a legality
certificate only; target policy must still combine it with observability and
cost facts and compare any direct-call artifact with the W-1600 physical
reference.

The current fixed task counts and worker capacities are seed evidence limits.
They must not become language, public ABI, or final runtime limits.
