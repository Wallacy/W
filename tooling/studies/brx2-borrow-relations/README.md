# BRX2 — relation-owned borrowed results

Status: historical design provenance, superseded by BRX3. This study does not
change the W grammar or define implemented compiler behavior. It does not claim
compiler, runtime, provider, ABI, or foreign-function evidence.

## Problem first

The Last Light restaurant has a menu selector with two borrowed inputs:
selectPrimary(primary, fallback) returns the primary line. A body can prove
that source. A member can use its receiver as the only source. A bodyless
free, static, or protocol requirement cannot inspect the implementation body:
one compatible input is therefore sufficient, but two independent compatible
inputs are rejected with W-BORROW-0011 because no source is authoritative.

BRX2 asks whether a relation record owned by the requirement and its published
interface can close this case. The caller does not choose the relation. Each
implementation and witness must prove the same relation. A witness-only
relation is rejected for generic or open dispatch.

The study keeps these forms separate. The `.w` fixtures are `baseline.w` and
`nominal-aggregate.w`; `relation-contract.txt`, `nominal-aggregate.txt`, and
the rejected `.txt` witnesses keep candidate and authority prose out of W
syntax:

| Form | Meaning | Route |
|---|---|---|
| current receiver/body | The receiver or body derives exact input slots. | current |
| unique bodyless | Exactly one compatible input supplies the result origin. | current |
| ambiguous bodyless | Two or more independent inputs have no authoritative origin. | W-BORROW-0011 rejection |
| requirement/interface relation | A sealed data-only relation maps each result slot to input slots. | Historical candidate, superseded by BRX3 |
| nominal aggregate | An owned sum carries the choice. | Safe API alternative |

The candidate relation has no source lifetime names, GAT, macro, caller flag,
runtime lifetime table, or new ABI carrier. Its digest is part of the semantic
interface and the import expectation. WAbiKey changes only if an existing
W-ABI representation changes. This study does not propose such a change.
The host oracle derives a runtime signature/carrier shape from input and result
slots, then checks equal baseline/candidate WAbi keys with relation metadata
excluded. Any attempted runtime field is rejected.

## Relation candidate

BorrowRelation/1 contains:

- every dependent result slot exactly once;
- canonical input slot names in sorted order;
- source modes derived from declaration slots;
- result mode;
- a sealed owner of requirement or interface;
- a digest over the complete data-only payload.

The verifier rejects missing, duplicate, forged, unknown, recursive, empty, or
mode-incompatible slots. It rejects stale relation and interface-lock digests.
It rejects a witness that diverges from the requirement relation. It rejects
variance during generic substitution because function relations are invariant.
An unverified declaration may publish a historical candidate relation, but an
open/generic/separate-compilation verification stage requires explicit
implementation and witness receipts plus provider and consumer interface keys.
Missing or stale receipts do not default to the relation.

Body-derived traces retain individual edges. OriginSet deduplicates only the
origins. Callable invocation creates a fresh loan per call. any fn preserves
the mapping component. A Stream view blocks a conflicting next while storage
is live. Await requires stable owner and storage, no conflict, cleanup, and
cancel drain. Dynamic boundaries reject dynamic borrow edges.

Callable, any-fn, await, Stream, and boundary cases use the relation mapping
only when that candidate is applicable. A rejected relation keeps the
baseline rejection and diagnostics; a valid relation candidate remains
historical provenance after the BRX0 baseline close and is superseded by BRX3.
Static and immortal edges are
non-dynamic and can cross the bounded boundary case.

Independent results derive from non-dependent result slots. Static and
immortal behavior derives from input slot facts and relation edges. Legacy
result flags and the legacy `verified` boolean are rejected; verification
requires a scope, stage, and explicit receipts.

The case-level `assay.kind: independent-assay` marks the structured problem
trace used by this host study. It is independent assay ground truth, not
compiler evidence, and it is excluded from relation/interface keys. A trace is
not valid compiler evidence for a bodyless current interface. Missing or
forged operations are rejected; a second real derivation is still required for
any promotion.

## Alternatives and rejection

The nominal aggregate fixture returns an owned SelectedLine sum. It is safe,
but changes the API and does not preserve the direct borrowed result.

Caller claims, witness-only mappings, runtime metadata, hidden escape, ambient
inference, Rust-like lifetime or GAT spelling, and universal conservative escape
remain rejected. A relation is not a caller claim because it belongs to the
requirement/interface contract and the provider publishes it.

## Evidence boundary and promotion gate

The fixture uses the real Last Light symbol selectPrimary from
reference/last-light/borrow_expressivity.w. The corpus and host oracle derive
relations, edges, OriginSets, interface keys, routes, and diagnostics from
structured declarations. Expected values never supply a mapping.

Current evidence is source-backed fixture text, Tree-sitter parse for W
variants, official primary references, and a deterministic host oracle.
Missing evidence is W compilation, W execution, compiler HIR verification,
provider/linker validation, foreign execution, human study, and model study.

Promotion requires an authoritative relation schema for open requirements,
separate-compilation import/provider witnesses, generic substitution checks,
real HIR and interface verification, and the existing W-914/OriginSet/ABI
invariants with no runtime lifetime metadata. The BRX0 baseline now closes
unique bodyless provenance and rejects ambiguous bodyless declarations with
W-BORROW-0011. Stop and keep this BRX2 witness historical if the relation needs
a caller claim, hidden escape, runtime field, new lifetime syntax, or an
unverified witness. BRX3 is the current source-clause design; its compiler,
HIR, separate-compilation, provider/linker, FFI, and runtime evidence remain
open. The host oracle closes the relation for
structured inputs, but no closed W mechanism is proven across all these
invariants; initializer borrowed-view results and unsupported declaration kinds
are rejected; no source spelling is promoted.
