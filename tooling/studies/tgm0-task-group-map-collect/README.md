# TGM0 — closed TaskGroup map and collect families

TGM0 closes the finite dynamic-task surface. The selected calls use only
`limit`, `ordering`, and `using`. `concurrent...` inherits the current domain;
`parallel...<domain>` requires an explicit domain with parallel capability.

The positive limit bounds live children. It does not make the consumed input
array or final result array sublinear. The input and callable are staged once,
each admitted item moves once, and result structure is reserved before the first
child can publish effects.

Map is fail-fast and returns only a complete success array. Collect observes
every application error and child cancellation. It returns one
`TaskSettlement` per input, so completion ordering never loses input identity.
Parent cancellation and faults suppress the array, cancel remaining work, and
propagate only after drain. Committed effects are not rolled back.

TGM0 rejects legacy labels, zero or implicit-unbounded limits, an invalid
parallel domain, mutable or consuming callables, repeated staging or moves,
late result reservation, early return, collect cancellation on application
error, collect results without indices, and active-child counts above `limit`.

The host oracle derives result order, primary error, publication step, and
retained input indices. It does not implement the frontend, intrinsic lowering,
runtime scheduler, provider, or cross-domain liveness.
