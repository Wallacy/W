# FRC0 — snapshot histórico das três gates de pesquisa

FRC0 é um bundle R1 de design-oracle-input. Ele fecha o snapshot histórico de
processo de W-707, W-731 e W-1408 até W-1450. Ele valida as decisões PFU0
W-1451 e W-1453 como `oracle-backed-current`. Ele exige a supersessão íntegra
de W-1452 por W-1480. O snapshot mantém
`Research=0` até esse limite. DRC0 fecha as gates posteriores W-1484, W-1473,
W-1474 e W-1475 e preserva W-1471 como superseded; a classificação global
volta a `Research=0`. FRC0 não compila,
executa ou promove uma implementação W. Ele não cria registros humanos ou de
modelos.

O corpus possui seis casos. Cada gate tem uma rota current e uma rota
adversarial. A máquina deriva o outcome de cópias dos corpora FZ0, da
classificação do ledger e do protocolo HUM0. Ela não lê `expected`, `status`,
score, preferência ou resultado fornecido pelo caller para selecionar o
outcome.

W-707 fecha o protocolo de completude FZ0. W-731 confirma uma disposition
explícita para cada decisão e a lista Research global vazia.
W-1408 preserva stop-on-first-violation,
no-automatic-promotion e zero registros humanos/modelos. A reabertura explícita
posterior PFU0 deixa de ser residual. W-1452 permanece como proveniência
histórica, e W-1480 contém o contrato vigente.

Os gaps de `w-compile`, `w-run`, compiler, runtime, provider, human-study e
model-study permanecem missing. O estudo usa somente source refs, máquinas,
snapshots e oracles host existentes. O stop condition rejeita qualquer Research
residual e exige evidência PFU0 e fechamento DRC0 antes de recascade.

Checks scoped:

```sh
bun test tooling/studies/final-research-closure/oracle.test.mjs
bun tooling/check-final-research-closure.mjs
bun run --cwd tooling/tree-sitter-w check:frc0
```
