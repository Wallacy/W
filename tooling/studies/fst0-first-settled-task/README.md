# FST0 — first-settled structured task selection

> Historical W-1481 evidence. The current spelling is the core facet
> `(take tasks).firstSettled()`; `TaskSettlement` and `TaskOutcome` remain data accessed
> with `.`.

FST0 closes the undefined `race` reference in the concurrency contract. The
selected algorithm is the ordinary collection member `firstSettled`. It consumes an array of existing lexical
task handles and returns the selected index plus `TaskOutcome`.

The API creates no task and selects no execution domain. The caller creates
each candidate with `async` or `spawn<domain>`. The operation arms all handles,
selects one settlement, requests cancellation of the losers, and waits for all
loser cleanup before it publishes the winner.

An empty array returns `none`. A child application error or child cancellation
is data in the selected `TaskOutcome`. Parent cancellation remains a control
outcome. When observed before publication, it suppresses the settlement and
propagates only after every candidate drains. Committed effects are not rolled
back.

FST0 rejects hidden branch creation, a `select` statement, first-success
semantics, lexical-order selection after the arm, return before loser drain,
duplicate handles, rollback claims, and persistent stream/channel multiplexing
through repeated one-shot races.

The host oracle derives winner and publication order from candidate state. It
does not implement the frontend, task runtime, provider, cancellation hooks,
or cross-domain liveness.
