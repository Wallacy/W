# PFU0 — fechamento de usabilidade pré-freeze

PFU0 fornece a evidência de encerramento para três decisões. Ele não afirma
compilação, execução ou implementação. O oracle host deriva outcomes de facts e source refs.

As três famílias são:

- W-1451: manifest de projeto unificado.
- W-1452: streaming de saída de service.
- W-1453: lifecycle de property.

Cada família possui uma variante current, uma candidate e uma adversarial.
`candidate.txt` é texto reservado. Ele não é source W e não entra na grammar.

Current controls preserve the single `build.w` root, explicit `Stream`,
explicit `Channel` and mailbox boundaries, and `get`/`set`/`modify`.
The build manifest candidate is accepted as the current control. The stream-fn and
implicit-observer candidates are rejected. Adversarial routes reject inline
packages, empty or owner-incompatible `build.w`, nested workspaces, implicit
transport, client or bidi streaming, the general `stream fn` route, conflated
`ServiceFailure` admission/open and terminal `Failure`, hidden `oldValue`
copies, and observer bypass ambiguity. `build.w` has one or two records,
at least one, in order-independent form. A standalone package record owns
`resolution`/`deployments`; a package-only member omits those fields and the
declared workspace owns them. A workspace record is the owner when present in
the same file. Membership is declared, never found by ancestor scan.
The service comparison records `stream fn updates(...): Item throws Failure`
as rejected for general use because captures, lifecycle, and error ownership
remain ambiguous. Service APIs keep explicit `some Stream<Item,Failure>`;
the call remains `try await` for admission/open and consumption remains
`for try await`.

The study is host-only. It does not provide compiler, runtime, provider,
human-study, or model-study evidence.

`bundle.json.inputs[].expected` is R1 rubric metadata. It is hidden by the
bundle blinding policy and is not shown to participants. The no-echo rule
applies to the PFU0 corpus and machine: `expected` or `result` is not accepted
by `validateCorpus` or `evaluateCase`, and no caller-supplied outcome controls
the derived result.
