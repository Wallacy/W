# PFU0 — índice de artefatos de encerramento

| Artefato | Função |
|---|---|
| `bundle.json` | Bundle R1 com current, candidate e adversarial |
| `study.json` | Contrato host-only, gates e source refs |
| `current.w` | Witness W fino para os controles vigentes |
| `candidate.txt` | Witness reservado para as três hipóteses |
| `adversarial.w` | Witness W fino para rotas rejeitadas |
| `oracle.test.mjs` | Testes determinísticos das três famílias |

The semantic payload lives in `tooling/pfu0-pre-freeze-usability-cases.json`.
The result does not use `expected` or a result supplied by the caller: the corpus
and machine reject those fields, and `validateCorpus`/`evaluateCase` derive all
outcomes. `bundle.json.inputs[].expected` is only R1 rubric metadata, hidden by
`blinding.hide` and never shown to participants. The manifest requires one or
more direct package records with exact roots and at most one local coordinator;
multiple packages require it, and record order does not affect package-set
identity. Package-authored requirements, profiles, and recipes stay in package
identity; the coordinator owns local selection, resolution, deployment, patch,
and lock facts. The service comparison registers `stream fn updates(...): Item
throws Failure` as rejected. APIs maintain explicit `some Stream<Item,Failure>`,
opening `try await` and consumption `for try await`, with `ServiceFailure`
separate from `Failure`.
