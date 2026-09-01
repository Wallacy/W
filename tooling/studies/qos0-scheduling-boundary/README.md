# QOS0 — physical scheduling boundary

> Historical W-1483 evidence. Current task placement uses the W-1511 pipeline
> tasks mode; the legacy TaskGroup case labels below are retained as adversarial
> evidence and do not define a source API.

QOS0 closes the W 1.0 boundary between domain placement and physical
scheduling policy. W source has no portable task priority or QoS surface.

The host or provider can select a physical policy. That policy can change
latency and ordering that the source and the applicable domain, barrier,
channel, or service contract leave unspecified. It cannot violate an order or
arbitration rule that those contracts guarantee, fabricate an outcome, ignore
cancellation or a deadline, or bypass admission, budgets, drain, or profile
liveness. QOS0 does not invent a global FIFO for channels or services.

The same logical trace can differ only in latency and non-observable physical
details; it has the same outcome, owner/drop ledger, and derived decisions.
Different permitted logical traces can choose different unspecified order and
can observe another deadline, admission result, first-settled winner, or
permitted outcome. This is existing nondeterminism, not QoS semantics.

The Last Light route models an allergy order. A dedicated service isolates
state. Admission, reservation, and budgets protect overload. A deadline drives
cancellation. The product can use a current, shared, or dedicated domain for
placement, performance, or liveness. Domain placement does not provide safety.

The provider receipt reports physical policy support and
`sourcePriority: absent`. The program cannot branch on this evidence.

The host oracle classifies declarative records. It does not implement or
simulate the W runtime. It does not claim cross-target policy support. A
deterministic scheduler or replay fixes the logical trace; physical details
remain sidecar evidence.

Research can reopen only after a bounded Last Light workload shows material
loss across targets. The evidence must close every inversion, starvation,
cancellation, fault, and liveness contract.
