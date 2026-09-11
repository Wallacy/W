# MLS0: modern MLIR lowering and scientific computing

Status: **complete design study**. This package closes a bounded architecture
question; it does not implement a W dialect, runtime, provider, algorithm, or
performance result.

## Decision

W keeps language semantics in verified W-owned IR until ownership, aliasing,
effects, cleanup, task structure, ABI, shape, layout, numeric mode, and device
transfers are explicit. MLIR dialects are replaceable compiler-internal
adapters, never W source semantics or a public artifact ABI.

The intended route is:

```text
W source
  -> CST/AST
  -> typed HIR
  -> verified W semantic IR
  -> structured domain IR
  -> schedule and cost selection
  -> late bufferization and explicit data movement
  -> target-specific IR
  -> object/provider link
  -> signed W artifact
```

`async` models readiness and dependencies; it does not define W ownership,
cancellation, cleanup, fairness, or the physical scheduler. `gpu` is a
middle-level kernel/offload representation; CUDA, ROCm, SPIR-V client APIs, or
another provider still own the final device interaction. `transform` is a
private, deterministic scheduling recipe. `memref` is a physical descriptor,
not a W owner, so bufferization remains late.

The next new lowering bundle targets the exact stable LLVM/MLIR 23.1.1 tag.
Existing 20.1.2 and 23.1.0 executions remain factual evidence and are not
silently rewritten. Ideas from older papers are retained only after their
benefit is revalidated against the pinned release; their APIs are not inherited.

## Scientific layer

The core owns compact universal contracts: numeric and complex types,
Tensor/View/Simd, shape/rank/index/layout, ownership and aliasing, numeric
modes, reduction/scan order, and reproducible RNG stream identity. Algorithms
live in first-party packages. Compiler recognition is justified only when it
enables verification or transformation that an ordinary call cannot preserve.

P0 contains linear algebra and contractions, FFT/DFT, convolution, reductions,
scans, sparse kernels, and stencils. P1 contains RNG, sorting, and solvers. Each
family exposes its relevant layout, precision, determinism, workspace,
placement, transfer, and failure semantics. The compiler chooses among small
static generated code, structured MLIR transforms, and a large/dynamic
provider call using measured cost rather than a hard-coded ideology.

FFTc is useful because it preserves FFT factorization, permutations, shapes,
and complex layout long enough to optimize them. Its experimental API and its
compile-time/code-size tradeoffs are not imported. The same rule applies to
MLIR4HPC, synchronous SSA work, IREE, FIR/HLFIR, Enzyme, and other case studies:
adopt the enduring separation of concerns, then re-prove it on current stable
MLIR and W's benchmark corpus.

## Proof-oriented W

A Lean-like dependent type theory throughout W's ordinary type system is not an
orthogonal addition: it would change elaboration, definitional equality,
termination checking, diagnostics, and ordinary compile-time cost. That does
not prevent W from pursuing the same user-level goal of writing and checking
proofs.

An optional proof/compiler mode is the preferred candidate when it consumes
verified W semantic facts and erases completely before optimization and
codegen. It is a second axis rather than another optimization profile, so
`debug + proof`, `release + proof`, and `benchmark + proof` remain possible. A
candidate CLI spelling is `--verification <standard|proof|certificate>`; this
study does not yet ratify that spelling. Proof-only packages may stop after
certificate production, while ordinary programs proceed to codegen only after
proof success. Candidate inputs include ownership, effects, structured-task
state, shape/layout/bounds, numeric error modes, and protocol state machines.
The layer may expose decidable refinements, contracts, invariants, ghost state,
lemmas, quantified propositions, induction with explicit termination, and
independently checkable external certificates. It must not retain
proof objects in the runtime or ABI, alter runtime semantics through proof
search, or fall back dynamically when a static proof fails.

This can make W proof-capable without making every W program dependently typed.
`PVL0-proof-kernel-and-erasure` is queued as a separate bounded study. No
surface is ratified until a prototype demonstrates a small explicit trusted
base, deterministic bounded checking, complete erasure, byte-equivalent
optimized output, and no ordinary-W compile-time regression when proofs are
absent. Lean's erased `Prop` is useful evidence for the erasure principle, not
a type-system template that W inherits.

## Promotion boundary

An algorithm family is not promoted by a microkernel throughput number alone.
It needs an independent correctness oracle, CPU evidence, applicable GPU
evidence, dynamic cases, memory/workspace/transfer measurements, precision and
determinism evidence, compile-time cost, and artifact size. The machine-readable
catalog and its focused oracle live beside this document.
