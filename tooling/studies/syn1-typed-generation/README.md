# SYN1 — generated module artifacts

Status: historical SYN1 provenance. This study does not define a W contract. It does
not provide a compiler, runtime, provider, build service, or LSP feature.

SYN1 narrows `SYN0-R1` with a problem-first matrix:

| Route | Restaurant use | Disposition |
| --- | --- | --- |
| A | Closed compiler synthesis, generic/protocol composition, and manual declarations | Current composition |
| B | The hermetic `final.menu` transform emits a typed data artifact | Current build transform; no declarations |
| C | A W0 build transform emits a separate generated W module set | Historical candidate; implementation evidence gap |
| C2 | A closed declaration recipe or IR bypasses W source | Rejected because it duplicates frontend rules |
| D | Macro, decorator, metaclass, eval, or AST/current-module mutation | Intentionally rejected |

The C candidate uses `bootstrap.w0` through `w.host/build-transform@1`. Its
closed output descriptor names `generated-w-module-set`, its byte bounds, its
logical module, W edition/features, and source profile. The action graph binds
the tool, exact dependency receipts, one `produces` edge, and declared
consumers. The tool has an empty capability set. Read-only handles provide the
only input authority. Their physical paths are validated but do not enter the
action key. Tool artifact, execution platform, typed binding/schema/digests,
dependencies, target receipt when applicable, authority requests, quotas, and
version do enter that key. Output bytes never enter the action recipe key.

Tool success first publishes a deterministic action result after container,
binding, schema, bounds, inventory, and digest checks. The result can remain in
the build CAS when later parsing or receipt validation fails. A second,
candidate-only publication occurs after the generated files reopen as new W
source units. `interfacePublished: true` records that candidate contract
outcome. It is not current compiler evidence. `compilerCachePublished` remains
false. Action error, cancellation, quota failure, or panic occurs before the
action result and proves cleanup, drain, and discard from strict events.

The current Tree-sitter parser checks real `.w` bytes without recovery. A
bounded source-shape scanner then masks comments and strings, ignores nested
declarations, accepts normal multiline signatures, and inventories only the
closed declaration-module profile. Entry points, manifests, foreign/service/
provider/test forms, extensions/behaviors, and top-level executable statements
are outside this candidate profile. A `frontendReceipt` binds source-shape
symbols to implementation-evidence-gap type, effect, ownership, constant, and conformance
facts. No W name resolver, type checker, ownership checker, effect checker, or
ConstIR normalizer validates those facts.

`observedTrace` ends at staged output and Tree-sitter parse/source-shape host
evidence. `requiredPhaseTrace` states the proposed compiler contract: parse,
name, type, ownership, effect, ConstIR, interface diff, freeze, interface
publication, and consumer processing. It does not claim that those semantic
phases ran. Invalid semantic receipts, maps, or imports preserve a valid action
result but do not publish an interface or compiler cache.

Logical source paths, logical input `SourceId`, module name, edition, features,
source profile, and content define candidate identities. Checkout paths and
physical product artifact names do not. A durable host target-registry fixture
authorizes the two study targets; compiler/provider target evidence is missing.
Uniform output shares the logical module/interface across target projections.
Target-specific output has a complete per-target action, module, interface,
diagnostic-map, and ABI receipt. `WAbiKey` uses registry-backed ABI facts, not a
physical artifact name.

Source maps use byte spans and logical input bindings. Generated spans must be
unambiguous. Source spans can overlap: one editable menu span can produce many
declarations. Endpoints must be UTF-8 code-point boundaries. A fix exists only
when one mapping covers the diagnostic. Generated-only diagnostics have no
invented fix. Physical adapter paths remain navigation provenance, outside the
diagnostic-map identity.

Every accepted candidate result includes an explain/navigation record with the
logical module, output binding, action recipe/result identities, tool, typed
inputs, generated source artifacts, provenance and diagnostic-map keys, and
the explicit missing compiler evidence. Generated source is inspectable as a
read-only artifact. Navigation to generated output and an exact editable
origin remains an implementation evidence gap; no LSP implementation is claimed.

## Evidence boundary

The corpus contains 65 cases across A/B/C/D. Fourteen generated `.w` fixtures
exercise single-file, multi-file, relocation, multiline, Unicode, ordering,
imports, collision, and interface-shape cases. Tree-sitter parsing and the host
oracle are current evidence. W compilation, semantic frontend phases, ConstIR,
execution, target provider behavior, and human/model studies remain missing.
Route and status are separate: malformed C cases remain `historical-candidate`,
while only C2/D mechanisms use `intentionally-rejected`.

Primary comparisons use C23 translation/preprocessing, Rust macros,
procedural macros, Cargo build scripts and `cfg`, and Python class creation,
decorators, imports, and `eval`. These official sources explain tradeoffs.
They do not provide W implementation evidence.
