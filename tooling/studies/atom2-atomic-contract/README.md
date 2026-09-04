# ATOM2 — atomic contract closure

ATOM2 is the successor design study for ATOM0-G1 and W-1352. It promotes a
narrow compiler-synthesized value carrier inside the existing `Atomic<T>` and
`var atomic` contract. It does not add syntax, a protocol, or an annotation.

The study keeps four problems separate:

| Axis | Decision |
| --- | --- |
| A | Promote closed value-only records with canonical declaration-order encoding. |
| B | Keep a generational integer handle with an owner table. Checked generation exhaustion retires the slot and fails allocation. |
| C | Keep `SnapshotCell` and domain composition. Permit only explicit specialized `unsafe` reclamation and FFI adapters. |
| D | Reject raw pointers, tagged pointers, and universal safe RCU or reclamation. |

## A — canonical value records

`var atomic word: SignEpochWord` and `Atomic<SignEpochWord>` use the same
compiler-derived carrier. The record must be closed, value-only, `Copy`,
lifetime-independent, and drop-free. The minimum field set is `Bool`, fixed
width integers, and payloadless enums. The canonical bit encoding is explicit:
`Bool` is `0` or `1`; unsigned integers use their exact width; signed integers
use two's-complement at their exact width; and a payloadless enum uses its
declaration ordinal with at least `ceil(log2(caseCount))` bits. Fields follow
declaration order, with the first field in the least-significant bits. Unused
high bits in the smallest carrier are zero. Physical endian is a provider or
ABI fact, not logical value identity. Safe W constructs only representable
canonical patterns, and compare-exchange encodes both expected and desired
values before comparing canonical representation. The compiler selects the
smallest supported carrier from 1 through 128 canonical bits.

Nested records, custom encoding, floats, `usize`, `isize`, pointers, owners,
borrows, views, allocator origins, drop fields, and unknown fields are rejected.
Only load, store, exchange, and compare-exchange are derived. These operations
are `neverSuspend`, are not cancellation points, and do not hide `Atomic.wait`,
which is a separate `maySuspend` API. Existing static memory-order checks
remain in force. `lockFree: true` requires an exact target fact and never uses a
fallback. Other cases may use a declared, allocation-free fallback per instance
and operation. A pre-reserved or global table requires an explicit provider
profile receipt; `allocation: true` is rejected in this contract.

Carrier progress is a target fact. `blocksThread` is separate from internal
`parking` detail, but `parking: true` requires `blocksThread: true`. A parking or
thread-blocking fallback is valid only when blocking is allowed; it is rejected
in signal/interrupt, freestanding, and cooperative-worker contexts. It is not
task suspension. Provider-only changes stay in the recipe, runtime closure, and
artifact receipt.
SemanticInterfaceKey changes only for a public declaration or requirement
change. WAbiKey and RepresentationMap include the carrier only when exact W ABI
crossing requires it. A carrier never crosses the C ABI directly.

## B — handles and ownership

`{slot: u32, generation: u32}` packs into a value-only `u64`. The owner table
checks the generation before dereference and returns `None` for a stale handle.
The generation increment is checked. A slot at `0xffffffff` is retired and a
new allocation fails. Generation wrap is never accepted. Tagged pointers remain
rejected because CAS does not prove provenance, lifetime, ABA, or reclamation.

## C and D — reclamation boundary

`SnapshotCell` and domain or service barriers remain the safe routes. A
specialized `unsafe` adapter may name its domain, participants, registration,
access and exit, unlink and retire order, quiescence, typed drop, raw reclaim,
retired bound, deleter context, target progress, fault behavior, and shutdown.
Persistent callbacks also require unregister, in-flight drain, destroy, and
unpin receipts. A complete schema permits an adapter only as an implementation
evidence gap. It does not promote a universal hazard-pointer, epoch, or RCU API.

The oracle covers target and fallback facts, bit direction/order and invalid
patterns, allocation-free fallback, never-suspend/cancellation facts, the exact
release/acquire order matrix, ABA and generation exhaustion, exactly-once drop,
panic, OOM, cancellation, shutdown, FFI drain, SemanticInterfaceKey, WAbiKey,
and the rejected universal forms. The corpus has 47 cases in four axes.
Two host reducers must produce the same semantic result. The corpus is evidence
of design only. It does not claim a W compiler, runtime, provider, or FFI
implementation.

## Evidence and stop condition

The Last Light anchors are `HorizonTelemetryEpoch`, `HorizonMenuPublication`,
`BellLease`/`watchClosingBell`, and `export foreign c`. Primary comparisons use
C23 atomics, Rust atomics and `Arc`/`Weak`, Swift Atomics, Python shared memory,
and Linux RCU. Implementation evidence remains separate: W compile and run,
target probes, provider behavior, stress, debug receipts, and human or model
studies are still missing.

ATOM2 closes the design gate and supersedes the historical ATOM1 result. It does
not close the implementation-evidence gaps.
