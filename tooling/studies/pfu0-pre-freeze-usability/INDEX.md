# PFU0 — índice de artefatos de encerramento

| Artefato | Função |
|---|---|
| `bundle.json` | Bundle R1 com current, candidate e adversarial |
| `study.json` | Contrato host-only, gates e source refs |
| `current.w` | Witness W fino para os controles vigentes |
| `candidate.txt` | Witness reservado para as três hipóteses |
| `adversarial.w` | Witness W fino para rotas rejeitadas |
| `oracle.test.mjs` | Testes determinísticos das três famílias |

O payload semântico fica em `tooling/pfu0-pre-freeze-usability-cases.json`.
O resultado não usa `expected` ou resultado fornecido pelo caller: o corpus e a
machine rejeitam esses campos e `validateCorpus`/`evaluateCase` derivam todos
os outcomes. `bundle.json.inputs[].expected` é somente rubric metadata R1,
fica oculto por `blinding.hide` e nunca é mostrado ao participante. O contrato
de manifesto exige um ou dois records em `build.w`, pelo menos um e ordem
independente. Package standalone é owner de `resolution`/`deployments`; package
member omite esses fields e o workspace declarado é owner. O workspace é owner
quando presente no mesmo arquivo. A comparação de service
registra `stream fn updates(...): Item throws Failure` como rejeitada. APIs
mantêm `some Stream<Item,Failure>` explícito, abertura `try await` e consumo
`for try await`, com `ServiceFailure` separado de `Failure`.
