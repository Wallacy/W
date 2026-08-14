# CYC1 — explicit cycle lifecycle study

Status: design-only evidence for `CYC0-G1`. This study does not define a W
contract. It does not claim a compiler, runtime, provider, FFI implementation,
stress result, or human/model study.

CYC1 asks whether the current W composition remains predictable for graph
ownership at the Last Light restaurant:

| Route | Problem | Current study decision |
|---|---|---|
| A | weak parent/callback edges and strong SCC diagnostics | Keep `weak T?`; derive a closed strong SCC as `W-OWNERSHIP-0014`. |
| B | explicit close, lifecycle drain, owner scopes, resources, and FFI leases | Keep close/drain/owner composition and require ordered foreign cleanup. |
| C | dynamic graph and leak visibility | Add an opt-in post-drain census that reports known residual SCCs or `unknown`; it never releases or collects. |
| D | transparent collector or finalizer | Reject in the core because it hides cleanup and effect ordering. |

The machine is event-derived. It builds nodes, edges, roots, registrations,
resources, and weak handles, then applies admission, mutation, close/drain,
callback, cancellation, drop, FFI, and census events. It derives Tarjan SCCs,
root reachability, breakability, cleanup order, and opaque-boundary status. It
does not read caller outcome flags to decide a result. The case `expect` fields
are checked only by the validator as a mutation guard.
An `explicitClose` edge names its owner and `close`/`unlink` must provide that
same authority. A targeted `drain` removes only `lifecycleDrain` edges for its
owner; an owner registry is usable only after it is closed.

The graph route and conditional-liveness route stay separate. Three W
compositions are tested for caches that do not need ephemeron semantics:

- a generation/ID cache with detached keys and explicit invalidation;
- an owner-scoped cache lease with explicit close;
- a detached value that has no strong value-to-key edge.

Ordinary weak handles do not implement a weak-key/ephemeron rule. The naive
weak-key and value-to-key cases therefore remain a conditional-liveness
Research subcapability. No primitive, syntax, or collector is proposed.

Foreign edges and roots without adapter metadata are `unknown`. The census
does not call `deinit`, reclaim storage, or repair a graph. A static SCC is
distinct from a post-drain `W-MEMORY-0001` residual and from an opaque-boundary
unknown. Panic and forced termination remain fault-boundary outcomes; user
cleanup is not promised after a panic.

The resource fixture puts a file node in a two-edge `lifecycleDrain` SCC. The
drain breaks that SCC, and an asynchronous `finish` is required before
quiescence. A socket fault case keeps the panic boundary separate from resource
reclamation.

The parseable witnesses compare weak parent/capture, explicit owner close,
post-init self-weak construction, and the current service/FFI boundary. The
transparent collector and public constructor forms are reserved text, not W
syntax. They are included to keep rejected mechanisms visible without adding
grammar.

The conditional witness is source-shaped for three separate compositions:
`generationIdCacheWithInvalidation`, `ownerScopedLeaseWithClose`, and
`detachedValueWithoutBackEdge`. These symbols demonstrate library composition
and do not claim an ephemeron primitive or implementation evidence.

Use the scoped gate:

```sh
bun test tooling/cyc1-explicit-cycle-reference.test.mjs tooling/studies/cyc1-explicit-cycle-lifecycle/oracle.test.mjs
bun tooling/check-cyc1-explicit-cycle.mjs
bun run --cwd tooling/tree-sitter-w parse:cyc1
```

The machine, corpus, manifest, and deterministic snapshot are at the top level
of `tooling/`. The study manifest and R1 presentation bundle are in this
directory. Official primary references are C23 WG14 N3096, Rust `Rc`/`Arc`/
`Weak` and destructor references, Python cyclic GC/weakref/C API lifecycle, and
Swift ARC plus its atomic weak-acquisition proposal. They are evidence only;
the study does not copy their APIs into W.
