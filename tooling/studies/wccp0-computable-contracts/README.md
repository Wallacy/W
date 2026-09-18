# WCCP0 — computable contracts

> Status: design study. This package adds no W syntax, proof mode, runtime
> validator, schema adapter, or optimization claim.

WCCP0 studies one canonical semantic boundary for contracts. A contract can
describe a proposition, value restriction, function relation, state machine,
module surface, or external schema. The design must preserve the difference
between these contract kinds and their evidence.

## Required architecture

The selected architecture pipeline is:

```text
carrier -> ContractIR -> obligation -> producer -> certificate -> W checker
                     \-> runtime validator
                     \-> tests and interoperability metadata
                     \-> optimizer facts after checked evidence
```

`ContractIR` must have canonical bytes and a stable digest. Every derived
artifact must retain that digest and the adapter identities used to construct
it. A proof producer is untrusted until the W-owned checker accepts its bounded
certificate.

Each normalized contract starts in the `declared` lifecycle state. This state
grants no optimizer fact. Evidence remains separated into these lanes:

- `proved` closes one declared obligation with a checked certificate.
- `validated` covers one runtime value and validator identity.
- `tested` covers bounded examples, generated properties, fuzzing, or
  simulation.
- `assumed` retains an explicit axiom or external trust boundary.
- `inconclusive` records timeout, unsupported semantics, or exhausted quotas.

## Selected source model

WCCP0 selects existing surfaces before adding a new declaration kind:

| Contract role | Selected carrier | Reason |
|---|---|---|
| Intrinsic value invariant | `T<(predicate)>` or a refined alias | The invariant can be checked from that value |
| Declaration relation | Structured documentation field `contract:` | The relation can bind input, state, and result without changing the call |
| Reusable predicate or relation | Contract-safe `const fn` | It reuses const evaluation and avoids a second function category |
| Nominal interface requirements | Protocol signatures plus attached relations | Protocol remains the conformance mechanism |
| External schema | Versioned adapter into `ContractIR` | The adapter records loss, references, limits, and trust |

A dedicated top-level `contract` declaration and `contract fn` remain rejected
until an executable case cannot use these carriers. Protocols can carry or
satisfy a contract, but they do not replace `ContractIR`.

A compiler implementation must validate a reusable const relation more strictly
than an ordinary compile-time computation. The relation must be pure, total,
terminating, hermetic, and symbolically normalizable for its declared contract
domain. Failure to prove those properties rejects its use as a contract without
changing ordinary `const fn` semantics. Const closures remain unavailable under
the current ConstIR rules.

## Documentation witnesses

Existing `call:` plus `result:` or `error:` examples are concrete contract
witnesses. A passing example covers only that input. A failing example can
refute a universal claim. A finite set proves a claim only when a checked
certificate also proves exhaustive domain coverage.

The selected relational attachment uses `contract:` in the same reserved
structured documentation channel. Ordinary Markdown remains non-semantic.
Compiler tooling extracts reserved fields before stripping documentation.

```w
type SortedResult<Element: Comparable> = Array<Element><(isSorted(value))>

/// Sorts the values in ascending order.
/// contract: sameElements(before: before values, after: result)
/// call: sort(values: [3, 1, 2])
/// result: [1, 2, 3]
fn sort(values: take Array<i64>): SortedResult<i64> {
  // implementation
}
```

`contract: <(expression)>` carries an inline relation.
`contract: relation(...)` calls a reusable relation. Multiple fields are an
unordered conjunction. The formatter keeps each field on one line when it fits
the preferred 120-column limit.

The compiler recognizes the field only at the start of a top-level
documentation line. Markdown fences, lists, quotes, links, inline code, and
ordinary prose cannot create a contract. The field payload is one W const
expression resolved in the documented declaration scope. Source maps retain
the directive and expression spans.

ContractIR orders the conjunction by normalized relation identity. A duplicate
is semantically idempotent and can produce a lint. Contradictory relations stay
distinct and fail proof or validation. This rule does not reuse nominal
protocol conjunction.

`result` identifies the successful result. `before name` identifies the
logical entry-state SSA value. `after name` identifies the logical exit-state
of a mutable parameter or receiver. These are ghost identities and do not
materialize storage. Runtime comparison with old bytes requires an explicit
bounded snapshot or another explicit witness.

`before name` can name a consumed parameter before transfer. `after name` is
limited to `mut ref`, `inout`, or a mutable receiver. A relation that uses
`result` applies to successful return. Error, cancellation, and panic relations
remain outside the initial binder set.

These binders initially belong only to callable declarations. Other supported
declaration scopes can attach contracts to their static surface. They do not
receive implicit runtime state. Cross-operation laws must name their
state-machine values explicitly.

`value` remains the contextual subject of `T<(predicate)>`. It does not alias
`result`. A relation such as `sameElements` does not belong inside
`SortedResult` because the result alone does not contain the original input.

The selected source model remains a design direction. It is not implemented
syntax until parser, resolver, ContractIR lowering, source maps, diagnostics,
and evidence gates exist.

## Evidence without `Proof<C>`

WCCP0 rejects a public generic `Proof<C>` from the initial surface. The current
cases use smaller existing boundaries:

| Need | Evidence path |
|---|---|
| Prove an intrinsic predicate | Return or construct the refined value |
| Compose a theorem | Call a proved contract-safe const relation |
| Cross a module or package | Import a checked certificate bound to the contract digest |
| Validate dynamic input | Return the refined value or a typed validation error |
| Retain an optimizer fact | Store a compiler-owned `ProofFact` with provenance |
| Prove an existential claim | Return the witness value with its refinement |

This choice keeps proofs out of runtime ownership, overload resolution, ABI,
and generic specialization. It does not forbid an internal proof term or a
certificate format. Reopen a public proof value only when a concrete theorem
needs first-class evidence that cannot compose through the table above.

## Imported formats

JSON Schema, XML Schema, WSDL, SOAP policy, OpenAPI, and similar descriptions
enter through versioned const-safe adapters. Each adapter must record:

- the exact format and profile version;
- canonical reference resolution;
- supported, rejected, and lossy constructs;
- input and expansion limits;
- normalized `ContractIR` identity;
- the trust boundary for remote references and generated code.

W does not need native JSON, XML, or SOAP literal syntax for this feature. A
schema proves neither application logic nor remote-peer behavior.

## Runtime and optimization

A runtime validator must derive from the same `ContractIR` as the proof
obligation. Its typed evidence identifies the contract, validator, input, and
provider boundary. A compile-time proof can erase runtime validation only when
all dynamic inputs and effects are covered by checked facts or independent
boundary receipts.

The first optimization witness must compare ordinary and proof-enabled builds.
It must preserve ABI and observable semantics. It must attribute binary-size,
compile-latency, runtime, and memory changes to the checked fact. Proof state
and ghost values must add no runtime cost.

## Position relative to Lean and Bend 2

| Area | Lean | Bend 2 | Selected W direction |
|---|---|---|---|
| Logical foundation | Propositions are types and proofs are terms | Affine dependent type theory with laws and proof definitions | Contracts lower to `ContractIR`; evidence stays in certificates and `ProofFacts` |
| Everyday type system | Dependent type theory | Dependent and affine | Conventional W types plus local refinements |
| Proof composition | First-class proof terms, tactics, and libraries | Proof definitions and rewriting; no tactics or proof search reported | Proved const calls, refinements, certificates, and implicit proof context |
| Runtime relation | Propositions erase | Proof equations erase inside one language/runtime model | Validation, tests, schemas, and proof share one contract identity |
| Execution focus | Proof assistant with executable programs | CPU/GPU parallel functional programs | Systems, services, accelerators, explicit effects, and provider-neutral targets |
| Current tradeoff | Strongest theorem ecosystem and complexity | Small uniform calculus with young platform limits | Smaller human surface and broader integration, but less general theorem programming |

Lean shows why proof irrelevance and runtime erasure are sound design targets.
Bend 2 shows that laws, proof definitions, and parallel execution can share one
language. W does not copy either source model. It uses contracts as the common
semantic input while preserving its systems-language ownership, effects,
runtime validation, external-schema, and optimizer boundaries.

The main loss is deliberate. W cannot claim Lean-level first-class theorem
programming without proof values, dependent functions, tactics, and a mature
library. It cannot claim Bend 2's uniform proof-and-execution calculus while
ContractIR and the checker remain unimplemented. The main gain is that ordinary
W code does not pay this complexity unless it declares or consumes a contract.

## Non-goals

WCCP0 does not make all W types dependent. It does not trust solvers, models,
schema generators, or remote services. It does not treat tests as proofs. It
does not add `Proof<C>`, a top-level `contract` declaration, `contract fn`, or
runtime `verify` and `check` syntax. It does not add native JSON, XML, SOAP, or
OpenAPI literals.

## Stop condition

Close the study only after parser and lowering evidence for the selected
carriers, canonical identity, composition, versioning, import/export, proof
checking, runtime evidence, erasure, diagnostics, quotas, trust boundaries,
schema-loss model, and one measured optimizer witness are complete.
