# ATOM1 — atomic extensibility study (historical)

Status: superseded historical candidate study. ATOM2 is the current ATOM0-G1 design
oracle and contract decision. This study does not define a W contract and does
not claim compiler, runtime, provider, or FFI implementation.

ATOM1 answers the ATOM0-G1 question with one adversarial host oracle. It keeps
three problems separate:

| Axis | Concrete restaurant problem | Study route |
|---|---|---|
| A | Publish `SignState` and `generation` as one `SignEpochWord` value. | Try current scalar packing, `SnapshotCell`, lock/domain, then the preferred compiler-synthesized canonical carrier. |
| B | Publish a `MenuHandle` while an owner table controls the `Menu` lifetime. | Use an integer handle with a generation check. Keep tagged pointers unsafe. |
| C | Retire menu nodes or versions after readers leave. | Use `SnapshotCell` for menu snapshots. Keep reclamation adapters unsafe and bounded. |

The preferred candidate for A is a compiler-synthesized canonical carrier for
the value fields behind the current source shape `var atomic word:
SignEpochWord`. No new syntax, protocol, or annotation is proposed. The
compiler derives an injective encoding from closed value-only fields: the three
`SignState` cases use 2 bits and `generation: u32` uses 32 bits, so an opaque
34-bit value selects the smallest supported carrier (for example, `u64`). The
`bool` contributes one bit. Fixed integer fields through `u128`/`i128` are
measured up to a 128-bit carrier; target-sized `usize`/`isize` fields are
rejected in this minimum study. The carrier, not the raw layout of
`SignEpochWord`, defines representation equality for CAS. A raw-layout record
candidate is rejected because it couples padding, alignment, and ABI layout of
`T` to the atomic primitive.

The canonical-carrier candidate is accepted by the oracle only when all of
these facts hold:

- the record is `Copy`, lifetime-independent, and drop-free;
- the representation has no pointer, owner, borrow, view, or allocator origin;
- the field encoding is injective and complete, with no custom or non-canonical
  encoding;
- the canonical encoding has a non-zero width; a one-case enum has no atomic
  carrier in this minimum study;
- the selected carrier has one supported atomic extent;
- operations are load, store, exchange, and compare-exchange only;
- compare-exchange uses bitwise representation equality;
- target progress and fallback come from the selected profile, not type identity.

The equivalent wrapper spelling `Atomic<SignEpochWord>` receives the same
derivation or diagnostic. The study does not add a second source protocol.

The oracle rejects a record with pointer or owner state, enum payloads, unknown
fields, a custom/non-injective encoding, invalid compare-exchange orders, or
generic fetch arithmetic. Raw padding and alignment are ABI receipts only; they
do not determine carrier eligibility. `lockFree: true` rejects a target profile
without that progress fact. A native carrier without lock-free progress uses a
declared, context-compatible fallback; a valid record on a target without a
native carrier can also use that fallback.
`lockFree: true` cannot hide an allocation or lock fallback.

For B, `MenuHandle { slot: u32, generation: u32 }` is value-only. The owner
table checks the generation before dereference. A stale handle is rejected and
never dereferenced. A tagged pointer can still pass a CAS and fail its
provenance, lifetime, ABA, or reclamation proof. The oracle therefore keeps it
blocked without those proofs. Complete provenance, lifetime, ABA, and
reclamation receipts route only to an unsafe specialized-adapter historical candidate
result; safe-wrapper promotion remains unproven. A safer domain/service
composition remains the current route.

The `ReclamationReceipt` source shape studies a verifier-visible receipt. It
does not become a user-implementable reclamation protocol.

Interface mutation changes `SemanticInterfaceKey` when the public declaration
or requirement changes and rejects that drift. A selected carrier enters
`WAbiKey`/the representation map only when the `Atomic<T>` declaration crosses
an exact W ABI; carrier drift then rejects. A provider digest-only change keeps
the observable contract and is recorded as recipe/RuntimeClosure/artifact
evidence, not as a semantic-interface change. Direct FFI carrier exposure is
rejected.

For C, the oracle requires an explicit adapter schema before it returns a
historical-candidate result. The schema names participants, registration, unlink-before-
retire, retired bounds, quiescence, deleter context, shutdown, memory orders,
target progress, fault behavior, and a `foreignBoundary` (`none` or
`persistent-callback`). Events require unlink → retire → reader quiescence →
typed drop exactly once → raw reclaim exactly once, then participant drain and
unregister. A persistent callback requires unregister → in-flight drain →
destroy → unpin; a non-FFI adapter cannot contain those events. Missing
registration, early retirement, reclaim before drop, unbounded retention, a
wrong deleter domain, live shutdown participants/readers/retired/callbacks, or
an invalid foreign boundary is rejected.

## Evidence boundary

The restaurant witnesses are existing Last Light symbols. The study does not
edit `reference/last-light/synchronization.w`.

- `HorizonTelemetryEpoch` and `releaseTelemetryFence` show the current atomic
  operation and order contracts.
- `HorizonMenuPublication` shows the current snapshot route.
- `BellLease` and `watchClosingBell` show explicit foreign cleanup.
- `foreign c` in `abi.w` shows an explicit interface boundary.

The primary references are LLVM's [`cmpxchg` rules](https://llvm.org/docs/LangRef.html#cmpxchg-instruction),
Rust's [target-dependent atomic support](https://doc.rust-lang.org/stable/core/sync/atomic/index.html),
and Linux's [RCU removal and reclamation split](https://docs.kernel.org/RCU/whatisRCU.html).
LLVM constrains the compare value to an integer or pointer with power-of-two
width, valid alignment, and separate success and failure orderings. Rust
exposes atomic widths as target facts. Linux RCU waits for readers from the
removal phase before reclamation.

The host oracle is `tooling/atom1-atomic-extensibility-machine.mjs`. Its cases
are in `tooling/atom1-atomic-extensibility-cases.json`, and its deterministic
projection is `tooling/atom1-atomic-extensibility-results.snapshot.jsonl`.
The tests assert invariants and mutations. They do not execute W.

This artifact is retained for provenance only. ATOM2 supersedes its open
questions: the closed value-only canonical carrier is now a contract within
the existing `Atomic<T>`/`var atomic` surface; generation handles remain
library composition; and universal reclamation remains rejected. The carrier
never crosses the C ABI directly. Evidence remains incomplete for W
compilation, W execution, target probes, provider behavior, FFI drain, and
human or model studies; those are implementation-evidence gaps, not an active
ATOM1 design gate.
