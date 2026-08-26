# DRC0: design research closure

Status: **Complete design study**. DRC0 closes the finite research questions in
W-1484, W-1473, W-1474, and W-1475. W-1471 remains historical and is
superseded only for its blocking `sync` semantics. DRC0 does not claim that the W frontend,
compiler, runtime, providers, frameworks, or benchmarks exist.

The independent closure machine re-derives SYNC1 from the execution-semantics
machine and reuses the MEM0, SEA0, and LLM0 oracles. Each current case records
the satisfied stop condition and the implementation evidence that remains
missing. A missing case, a failed study oracle, a caller-owned result field, or
an omitted evidence boundary fails the closure.

The resulting dispositions are design-current:

- `sync` is a direct, nonblocking, same-task call only for an explicit
  `async fn` with a declaration-wide `directEntry: available` proof. It never
  creates a task or uses a runtime/provider fallback. A `sync` call can compose
  with another available direct entry: the published async entry stays `may`,
  while the selected entry is `neverSuspend`. The proof uses a fixed point for
  sync-call SCCs without executing recursion or proving termination. Facet loss
  and invalid sync calls propagate to callers. Semantic checking,
  type/HIR/interface facets, dual-entry lowering and ABI, cross-module erasure,
  diagnostics, and human/model evidence remain implementation gaps.
- MEM0 selects a layered ownership/provider/compiler/unsafe classification and
  rejects a universal mapping promise. It does not select one universal API.
- SEA0 selects the bounded approval machine and its deterministic-test
  crosspoint. It does not promise rollback or exactly-once behavior.
- LLM0 assigns the readiness gaps outside the language core by default. It does
  not claim a training or inference framework.
