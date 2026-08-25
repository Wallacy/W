# MEM0: virtual memory and data movement

Status: **Complete design study** (W-1473). MEM0 reached its finite stop
condition and its classification is now a current design direction. It does
not implement a W API, a provider, a compiler lowering, or a portable
performance promise.

The study separates file-backed mappings, anonymous virtual memory, and device
memory. It classifies each candidate by ownership: portable semantic owner/API
candidate, provider capability/receipt, compiler optimization, unsafe target
adapter, or rejected universal promise. Every candidate records a move-only
owner, bounded extent, permissions, address-space and provenance rule,
deterministic unmap/drop, live-view exclusion, external-interference outcome,
and target evidence boundary.

The workloads are immutable CAS/model weights, an archive parser, a large
arena, device staging, and mapped IPC with a service fallback. The oracle
checks contracts and classification completeness. It does not measure
latency, bandwidth, page faults, cache effects, or device performance.

## Evidence boundary

The study records primary source pointers and the access date in `study.json`.
Those sources support target vocabulary and constraints only. They are not W
provider receipts. The layered classification is current, but no row is
promoted to a universal API. The `Mapped<T>` universal wrapper is intentionally
absent.

## Executable artifacts

- `study.json` records the decision, source boundary, required fields, and
  finite stop condition.
- `cases.json` contains the classified candidates, workloads, and positive or
  adversarial cases.
- `oracle.mjs` derives validation errors without trusting expected-result
  echoes.
- `oracle.test.mjs` tests positive and adversarial mutations.
- `check.mjs` is the package-level checker used by `check:mem0`.
