# SEA0: simulated effects, approval, and test infrastructure

Status: **Complete design study** (W-1474). SEA0 reached its finite stop
condition and selects the bounded approval machine and its test-infrastructure
crosspoint as a current design direction. It is not a transaction system, a
closed-turn output gate, a pipeline, or an `unknownOutcome` implementation.

The same bounded state machine is the production simulation model and the test
double model. A proposal binds an effect id, input digest, authority and
capability, provider and generation, simulated result digest, dependency DAG,
approval group, limits, and expiry. A commit revalidates the external
generation and state. A stale proposal becomes `conflict` or requires a new
simulation. The real dispatch boundary can produce `unknownOutcome(effectId)`.
The study makes no rollback, compensation, or exactly-once claim.

## Test-infrastructure crosspoint

The study data names deterministic scheduler, virtual clock, deterministic RNG,
virtual network/storage/provider, bounded semantic fault injection, and bounded
schedule exploration. It requires state/model/property/fuzz/differential/
metamorphic lanes, transition/effect/owner/fault coverage, reproducible seeds
and minimized traces, provider contract kits, explicit snapshots/goldens, and
independent oracles. The lanes remain separate: pure simulation, real-provider
conformance, multi-process or hardware fault, and performance. Simulation is
never evidence of a real provider.

## Evidence boundary

`cases.json` is design data. It records no provider receipt and no production
execution. The oracle validates state transitions, causal invalidation,
topological bulk approval, stale revalidation, and the required fault cases.
`Proposal<T>` and `Tentative<T>` are study names only. They are not promoted
syntax or types in this bundle.
