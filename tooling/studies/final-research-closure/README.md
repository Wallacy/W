# FRC0 — fechamento histórico e residual current

FRC0 é um bundle R1 de design-oracle-input. Ele fecha o snapshot histórico de
processo de W-707, W-731 e W-1408 até W-1450. Ele valida W-1451 como
`oracle-backed-current` e exige as supersessões íntegras de W-1452 por W-1480
e de W-1453 por W-1516. O snapshot mantém
`Research=0` até esse limite. AEG0/SIMD1 preservam o fechamento histórico até
W-1459. DRC0 fecha as gates posteriores W-1484, W-1473, W-1474 e W-1475 e
preserva W-1471 como superseded; o snapshot histórico preserva W-1486 e W-1503
como gates posteriores. W-1517 fecha W-1503 e W-1518 fecha/supersede W-1486;
o residual current de research é exatamente `[]`. W-1517 e W-1518 são
closures de design-only, não implementação.
FRC0 não compila,
executa ou promove uma implementação W. Ele não cria registros humanos ou de
modelos.

O corpus possui seis casos. Cada gate tem uma rota current e uma rota
adversarial. A máquina deriva o outcome de cópias dos corpora FZ0, da
classificação do ledger e do protocolo HUM0. Ela não lê `expected`, `status`,
score, preferência ou resultado fornecido pelo caller para selecionar o
outcome.

W-707 fecha o protocolo de completude FZ0. W-731 confirma uma disposition
explícita para cada decisão e que o residual current de research é vazio;
W-1486/W-1503 ficam apenas na lista histórica post-snapshot.
W-1408 preserva stop-on-first-violation,
no-automatic-promotion e zero registros humanos/modelos. A reabertura explícita
posterior PFU0 deixa de ser residual. W-1452 e W-1453 permanecem como
proveniência histórica; W-1480 e W-1516 contêm os contratos vigentes.

Os gaps de `w-compile`, `w-run`, compiler, runtime, provider, human-study e
model-study permanecem missing. O estudo usa somente source refs, máquinas,
snapshots e oracles host existentes. O stop condition rejeita qualquer Research
residual current (o residual permitido é `[]`) e exige evidência PFU0 e
fechamento DRC0 antes de recascade.

## Research-state inventory

`tooling/research-state-inventory.json` is the authoritative maintained
cross-surface registry. Its `active` list is exactly `[]`; each listed family is
classified as `historical`, `rejected`, `current-design-evidence-gap`, or
`future-reopen-candidate`. Historical text and implementation gaps remain
traceable without becoming active research. `normalizationPending` is `false`
for every registered family. FRC0 requires the authoritative status,
`normalized: true`, and a zero pending count; future reopening candidates
remain classified without becoming active research.

Checks scoped:

```sh
bun test tooling/studies/final-research-closure/oracle.test.mjs
bun tooling/check-final-research-closure.mjs
bun run check:frc0
```
