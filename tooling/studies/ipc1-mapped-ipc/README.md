# IPC1 — mapped memory and process-shared IPC

Status: design-only **Pesquisa**. IPC1 informs and narrows `IPC0-R1`. It does
not define W syntax, an API, a compiler rule, a runtime, or a provider.

The current route remains a bounded snapshot, typed wire, or service channel.
The study has two Research candidates:

- an immutable mapped snapshot with pointer-free relative layout;
- a bounded mapped channel or log with explicit slot ownership and commit.

The provider is the authority for target facts. The corpus separates
file-backed durable snapshots (POSIX and Windows) from volatile channel
families (POSIX `shm_open` and Windows pagefile mappings). A caller cannot
forge address, layout, atomic, durability, or lifecycle receipts. The POSIX
and Windows reducers expose different physical events and one compact logical
outcome. Every immutable generation is a separate object or extent. A selector
catalog publishes the current object; a live lease keeps its object and
generation valid until that lease closes.

## Contract under test

An immutable candidate validates magic, version, schema identity and schema digest,
layout digest, length, alignment, endianness, relative segments, bounds, and generation before
creating a typed view. The writer stages and hashes a separate generation
object. For a durable request it requests durability, flushes generation data
and metadata, releases selector visibility, flushes the selector/namespace,
and then receives a terminal receipt. A visibility-only publish has no receipt.
A crash before selector release preserves the prior generation. A crash after
selector release and before the terminal receipt leaves live visibility known
but recovered visibility unknown; a crash after receipt is success. Existing
leases remain valid while a later generation becomes current, and generation
reuse is rejected while a lease is live. `drop-view`/close precede unmap.

A channel candidate is a bounded wire/byte carrier, not generic `Channel<T>`
storage. Capacity, slot count, slot size, schema, layout, and generation come
from a validated mapped header. Header length must equal the mapped extent and
`slotCount * slotSize` must fit the validated `slots` segment. Cap0 has no
storage slots and uses a paired rendezvous; capN occupancy derives from the
slot states. A map event without an explicit generation uses that validated
current generation; a supplied generation must match it. Relative offsets and
provider-proved process-shared atomic width, order, alignment, lock-free
progress, and wait/wake facts are mandatory. The sender owns local bytes before
commit. Commit publishes canonical wire bytes and transfers ownership to the
mapped generation. The receiver validates length, schema, and checksum before
materializing a fresh W owner. Host traces prove at most one owner per committed
slot, not distributed exactly-once delivery. Cancellation keeps the existing
pre-commit and post-commit rules. A producer crash in `writing` faults the
generation; a committed full slot survives that producer crash and the reader
can materialize and release it. A receiver
crash while reading/materializing faults the generation. A supervisor must stop
access, drain, drop views/loans, unmap, close handles, and open a higher
generation in that order. FFI leases use stop-access, unregister-callback,
drain, drop-view, unmap, close. The study does not repair a slot in place.
Normal channel and lifecycle traces must prove map, validate, view, and
explicit close; typed cancel, backpressure, and fault outcomes are the only
early terminals.

The study rejects native pointers, W owners, borrows, capabilities, dropful
values, ambiguous `usize`/`isize`, hidden locks, hidden allocators, hidden
schedulers, implicit mapping, resize under live views, raw address equality,
automatic crash repair, and an ordinary in-process atomic as process-shared
proof.

A robust blocking provider is a separate provider profile. It is valid only for
a blocking service context. A cooperative worker cannot receive an invisible
blocking fallback. Owner death reports a typed generation fault. It does not
repair a slot.

## Restaurant witnesses

The baseline uses `snapshotRecipe`, `archiveParquet`, and `handoffArrow` for the
Restaurant telemetry path. `HorizonTelemetryEpoch` and its explicit fence show
the existing local publication contract. `publishStagedRecipe` shows a staged
file publish with a durability receipt. `installMenuObserver` shows explicit
lease and callback cleanup. IPC1 does not edit these Last Light sources.

## Evidence boundary

The host oracle derives state from ordered operations. It compares POSIX and
Windows target projections, validates source paths, file digests, unique
symbols, provider bindings, lifecycle outcomes, crash ordering, durability,
channel commit, and mutations. Legacy caller result flags are schema errors.
The snapshot is a design-oracle output. It does not run W.

Missing evidence keeps `IPC0-R1` open:

- two-process POSIX and Windows probes;
- `w-compile` and `w-run` evidence;
- provider receipts and FFI lease probes;
- human and model ergonomics studies.

The documentation queue keeps paired problem examples for C/POSIX
`mmap`/`shm_open`/`msync`, Rust atomics/`Arc`/FFI, and Python
`SharedMemory` close/unlink. Each pair must cite the W source refs above. The
queue target is `guides/problems/process-shared-data`.

Use the snapshot/wire/service baseline when any layout, process lifetime,
atomic scope, durability, or crash outcome cannot be proved.
