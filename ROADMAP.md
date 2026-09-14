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
| 1 | CRT-free parallel process witness | One unchanged W source obtains runtime process input, dispatches through the provider, joins lexically, and exits through static W-owned target support on Windows and Linux/WSL | Honest executable benchmark candidate and cross-target comparison |
| 2 | Native Hello optimization sentinel | The smallest current public Hello is rebuilt from source on Windows and Linux with static whole-graph release closure; correctness is mandatory and the live catalog retains artifact size, compile time, wall/CPU time, and memory without retaining binaries | Early feedback for section elimination, linkage, ABI, startup, and the target-specific sub-1-KiB opportunity |
| 3 | GPU kernel Hello sentinel | One W host plus one target-neutral GPU function writes a known payload to a device-visible result, launches and joins through an explicit domain, and verifies it on the available GPU; device and host artifacts and end-to-end/dispatch/memory metrics stay separate | Tests CPU/GPU partitioning and MLIR GPU applicability before scheduler and memory abstractions harden |
| 4 | Capacity-independent task storage | Replace the seed one-to-four logical-task arrays with measured caller-owned records; logical task count and physical worker capacity remain separate; configured exhaustion fails before effects | Removes an implementation ceiling before scheduler generalization |
| 5 | Structured cancellation and outcomes | Request, propagation, cleanup drain, typed failure, panic boundary, and deterministic outcome publication execute through the same native route | A usable structured-concurrency core rather than successful scalar jobs only |
| 6 | Provider-neutral scheduler core | Target-neutral ready/task/frame state lowers once; Windows and Linux providers supply only platform primitives and cached topology/capacity facts | Portable concurrency without a platform-shaped language ABI |
| 7 | Resource-bearing enum and ownership slice | `Result`-like payload enum, exhaustive match, move, borrow, cleanup, typed success/failure, and adversarial rejection execute natively | Concrete memory-management invariants for general aggregates and tasks |
| 8 | General verified HIR and lowering | Expand types, calls, CFG, ownership/effects, and diagnostics in small executable slices while preserving independent verification | Moves from bounded demonstrations toward ordinary programs |
| 9 | Closed-graph optimizer and static closure | Whole-module is the minimum optimized region; proved package/workspace/product closure internalizes and eliminates unused WRT/std/provider code without changing observable roots | Generalizes the optimization rules first exercised by the Hello sentinel |
| 10 | Incremental compiler and test selection | No-op, local-body, interface, and product-root edits reuse exact artifacts and run only risk-relevant gates, with mutation evidence that product failures are still detected | Faster human and AI iteration without hollow green checks |
| 11 | Native toolchain and cross-target distribution | Reproducible signed LLVM/MLIR/LLD target packs cover the Windows/Linux/macOS host-target baseline, with exact SDK and ABI provenance | Compact offline `w` distribution and supported cross compilation |
| 12 | Package, registry, service, and sandbox slices | Signed binary-first package plus source fallback, independent verification, one local/external service provider, and bounded sandbox execution | Ecosystem work built on a stable executable/runtime boundary |
| 13 | UI, scientific, accelerator, and proof-mode witnesses | Promote one real application or numerical workload at a time through correctness, target applicability, resource receipts, and benchmark evidence; the GPU Hello remains the earlier architecture sentinel | Broader targets without expanding the language from untested abstractions |

## Current checkpoint

W-1599 is the current checkpoint inside rank 1. W-1598 composes the explicit
process root and task entry in caller-owned MLIR, preserves the runtime
`Arguments.isEmpty` dependency, and proves a Linux ELF object with unresolved
provider launch and join symbols. W-1599 adds a target-neutral fixed lifecycle
reducer and oracle for outcomes, cancellation, cleanup, joins, releases, and
transaction snapshots. Provider linkage and equivalent Windows/Linux
execution remain the rank-1 completion boundary. Once that physical reference
is available, target policy can combine the W-1597 legality certificate with
domain-observability and cost facts to select and compare a direct-call build.
The CPU and GPU sentinels follow before task-storage and scheduler
generalization so their measurements can still influence lowering, linkage,
placement, and runtime heuristics cheaply.

The current fixed task counts and worker capacities are seed evidence limits.
They must not become language, public ABI, or final runtime limits.
