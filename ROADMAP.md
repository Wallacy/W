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
- Measure public executable behavior only after an exact correctness oracle.
- Treat performance, memory, binary size, and compile latency as persistent
  optimization signals, never as permission to change semantics.

## Ranked queue

| Rank | Increment | Completion boundary | What it enables |
| ---: | --- | --- | --- |
| 1 | Parallel emitted-code adapter | A private provider calls the W-1593 runtime-parameterized MLIR task symbols with non-constant runtime values; exact outcomes match the independent oracle and no target HIR evaluator remains | First proof that native parallel work executes emitted W code |
| 2 | CRT-free parallel process witness | One unchanged W source obtains runtime process input, dispatches through the provider, joins lexically, and exits through static W-owned target support on Windows and Linux/WSL | Honest executable benchmark candidate and cross-target comparison |
| 3 | Capacity-independent task storage | Replace the seed one-to-four logical-task arrays with measured caller-owned records; logical task count and physical worker capacity remain separate; configured exhaustion fails before effects | Removes an implementation ceiling before scheduler generalization |
| 4 | Structured cancellation and outcomes | Request, propagation, cleanup drain, typed failure, panic boundary, and deterministic outcome publication execute through the same native route | A usable structured-concurrency core rather than successful scalar jobs only |
| 5 | Provider-neutral scheduler core | Target-neutral ready/task/frame state lowers once; Windows and Linux providers supply only platform primitives and cached topology/capacity facts | Portable concurrency without a platform-shaped language ABI |
| 6 | Resource-bearing enum and ownership slice | `Result`-like payload enum, exhaustive match, move, borrow, cleanup, typed success/failure, and adversarial rejection execute natively | Concrete memory-management invariants for general aggregates and tasks |
| 7 | General verified HIR and lowering | Expand types, calls, CFG, ownership/effects, and diagnostics in small executable slices while preserving independent verification | Moves from bounded demonstrations toward ordinary programs |
| 8 | Closed-graph optimizer and static closure | Whole-module is the minimum optimized region; proved package/workspace/product closure internalizes and eliminates unused WRT/std/provider code without changing observable roots | Smaller/faster products and evidence toward the sub-1-KiB Hello opportunity |
| 9 | Incremental compiler and test selection | No-op, local-body, interface, and product-root edits reuse exact artifacts and run only risk-relevant gates, with mutation evidence that product failures are still detected | Faster human and AI iteration without hollow green checks |
| 10 | Native toolchain and cross-target distribution | Reproducible signed LLVM/MLIR/LLD target packs cover the Windows/Linux/macOS host-target baseline, with exact SDK and ABI provenance | Compact offline `w` distribution and supported cross compilation |
| 11 | Package, registry, service, and sandbox slices | Signed binary-first package plus source fallback, independent verification, one local/external service provider, and bounded sandbox execution | Ecosystem work built on a stable executable/runtime boundary |
| 12 | UI, scientific/GPU, and proof-mode witnesses | Promote one real application or numerical workload at a time through correctness, target applicability, resource receipts, and benchmark evidence | Broader targets without expanding the language from untested abstractions |

## Current checkpoint

W-1594 is complete: PARLINK0 connects mixed-arity W-1593 entries to a private
CRT-free Windows Kernel32 adapter and proves that runtime-dependent values run
through emitted W code without a target HIR evaluator. Rank 2, a W process-root
witness with equivalent Windows and Linux/WSL execution, is the next product
increment.

The current fixed task counts and worker capacities are seed evidence limits.
They must not become language, public ABI, or final runtime limits.
