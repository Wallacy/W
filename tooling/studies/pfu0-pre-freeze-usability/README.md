# PFU0 — pesquisa pré-freeze de usabilidade

PFU0 reabre o design freeze com três gates de pesquisa. Ele não promove syntax
ou semântica. O oracle host deriva outcomes de facts e source refs.

As três famílias são:

- W-1451: manifest de projeto unificado.
- W-1452: streaming de saída de service.
- W-1453: lifecycle de property.

Cada família possui uma variante current, uma candidate e uma adversarial.
`candidate.txt` é texto reservado. Ele não é source W e não entra na grammar.

Current controls preserve `package.w`/`workspace.w`, explicit `Stream`,
explicit `Channel` and mailbox boundaries, and `get`/`set`/`modify`.
Candidate routes remain `research-gated`. Adversarial routes reject inline
packages, empty or owner-incompatible `build.w`, nested workspaces, implicit
transport, client or bidi streaming, the general `stream fn` route, conflated
`ServiceFailure` admission/open and terminal `Failure`, hidden `oldValue`
copies, and observer bypass ambiguity. `build.w` has one or two records,
at least one, in order-independent form; a workspace record owns the file.
The service candidate is only `stream fn updates(...): Item throws Failure`,
which normalizes to `some Stream<Item,Failure>`; the call remains `try await`
for admission/open and consumption remains `for try await`.

The study is host-only. It does not provide compiler, runtime, provider,
human-study, or model-study evidence.

`bundle.json.inputs[].expected` is R1 rubric metadata. It is hidden by the
bundle blinding policy and is not shown to participants. The no-echo rule
applies to the PFU0 corpus and machine: `expected` or `result` is not accepted
by `validateCorpus` or `evaluateCase`, and no caller-supplied outcome controls
the derived result.
