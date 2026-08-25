# DRC0: design research closure

Status: **Complete design study**. DRC0 closes the finite research questions in
W-1471, W-1473, W-1474, and W-1475. It does not claim that the W frontend,
compiler, runtime, providers, frameworks, or benchmarks exist.

The independent closure machine re-derives SYNC0 from the execution-semantics
machine and reuses the MEM0, SEA0, and LLM0 oracles. Each current case records
the satisfied stop condition and the implementation evidence that remains
missing. A missing case, a failed study oracle, a caller-owned result field, or
an omitted evidence boundary fails the closure.

The resulting dispositions are design-current:

- `sync` is selected only for an explicit `async fn`; all other uses are
  errors. Frontend and runtime support remain implementation gaps.
- MEM0 selects a layered ownership/provider/compiler/unsafe classification and
  rejects a universal mapping promise. It does not select one universal API.
- SEA0 selects the bounded approval machine and its deterministic-test
  crosspoint. It does not promise rollback or exactly-once behavior.
- LLM0 assigns the readiness gaps outside the language core by default. It does
  not claim a training or inference framework.
