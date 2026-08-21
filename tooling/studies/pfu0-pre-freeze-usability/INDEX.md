# PFU0 — índice de artefatos

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
fica oculto por `blinding.hide` e nunca é mostrado ao participante. O candidato de manifesto exige um ou dois
records (pelo menos um), em ordem independente, e workspace owner quando
presente. O candidato de service é apenas a declaration `stream fn
updates(...): Item throws Failure`; a interface normaliza para
`some Stream<Item,Failure>`, enquanto `ServiceFailure` de admission/open e
`Failure` terminal ainda precisam de decisão de promoção separada.
