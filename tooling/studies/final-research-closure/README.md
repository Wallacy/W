# FRC0 — snapshot histórico das três gates de pesquisa

FRC0 é um bundle R1 de design-oracle-input. Ele fecha somente o snapshot
histórico de processo de W-707, W-731 e W-1408 até W-1450. A autoridade humana
reabriu a pesquisa depois desse snapshot: W-1451, W-1452 e W-1453 são gates
PFU0 `Research` atuais, e o design freeze permanece aberto/bloqueado. FRC0 não
compila, executa ou promove uma implementação W. Ele não cria registros
humanos ou de modelos.

O corpus possui seis casos. Cada gate tem uma rota current e uma rota
adversarial. A máquina deriva o outcome de cópias dos corpora FZ0, da
classificação do ledger e do protocolo HUM0. Ela não lê `expected`, `status`,
score, preferência ou resultado fornecido pelo caller para selecionar o
outcome.

W-707 fecha o protocolo de completude FZ0. W-731 confirma uma disposition
explícita para cada decisão e `Research=0` somente na fronteira histórica
W-001–W-1450. W-1408 preserva stop-on-first-violation,
no-automatic-promotion e zero registros humanos/modelos. A reabertura explícita
posterior é validada como `research-gated` por PFU0, não como residual dentro
do snapshot.

Os gaps de `w-compile`, `w-run`, compiler, runtime, provider, human-study e
model-study permanecem missing. O estudo usa somente source refs, máquinas,
snapshots e oracles host existentes. O stop condition rejeita Research residual
em W-001–W-1450 e exige caso independente, digest novo e decisão de promoção
revisada antes de qualquer extensão.

Checks scoped:

```sh
bun test tooling/studies/final-research-closure/oracle.test.mjs
bun tooling/check-final-research-closure.mjs
bun run --cwd tooling/tree-sitter-w check:frc0
```
