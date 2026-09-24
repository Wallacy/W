# PFU0 — pre-freeze usability closure

PFU0 provides closure evidence for three decisions. It does not claim W
compilation, execution, or implementation. The host oracle derives outcomes
from facts and source references. Its scope is W-1451–W-1453; later Research
gates remain outside PFU0 and keep the global design freeze open.

As três famílias são:

- W-1451: package-centric `build.w` roots.
- W-1452: service output streams.
- W-1453: property lifecycle.

Cada família possui uma variante current, uma candidate e uma adversarial.
`candidate.txt` é texto reservado. Ele não é source W e não entra na grammar.

Current controls preserve the direct package records, exact local roots, and
optional local-only `build { schema: "w.build/1" }` coordinator, plus explicit
`Stream`, `Channel`, mailbox, and property-accessor boundaries. The
package-centric manifest candidate is accepted as the current control. The
stream-fn and implicit-observer candidates are rejected. Adversarial routes
reject missing packages, multiple packages without a coordinator, duplicate
coordinators or roots, path escapes, globs, nested build roots, coordinator
weakening of package requirements, implicit transport, client or bidi
streaming, conflated `ServiceFailure` admission/open and terminal `Failure`,
hidden `oldValue` copies, and observer bypass ambiguity. Package record order
does not affect package recipe identity. `w build <package>[:<product>]` is
exact selection; `w build all` is explicit only. There is no workspace record
or workspace identity, and root discovery never scans cwd or ancestors.
The service comparison records `stream fn updates(...): Item throws Failure`
as rejected for general use because captures, lifecycle, and error ownership
remain ambiguous. Service APIs keep explicit `some Stream<Item,Failure>`;
the call remains `try await` for admission/open and consumption remains
`for try await`.

The study is host-only. It does not provide compiler, runtime, provider,
human-study, or model-study evidence.

This is a historical closure record. W-1480 later supersedes only the PFU0
rejection of client-streaming and bidirectional-streaming. It preserves the
rejection of `stream fn`, implicit Channel/capacity, and collapsed failure
phases. SVC0 is the current directional-stream study.

`bundle.json.inputs[].expected` is R1 rubric metadata. It is hidden by the
bundle blinding policy and is not shown to participants. The no-echo rule
applies to the PFU0 corpus and machine: `expected` or `result` is not accepted
by `validateCorpus` or `evaluateCase`, and no caller-supplied outcome controls
the derived result.
