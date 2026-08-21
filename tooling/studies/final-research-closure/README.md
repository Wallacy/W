# FRC0 — encerramento final das três gates de pesquisa

FRC0 é um bundle R1 de design-oracle-input. Ele fecha o snapshot histórico de
processo de W-707, W-731 e W-1408 até W-1450 e valida as decisões PFU0
W-1451, W-1452 e W-1453 como `oracle-backed-current`. O resultado exige
`Research=0` global e permite fechar o design freeze. FRC0 não compila,
executa ou promove uma implementação W. Ele não cria registros humanos ou de
modelos.

O corpus possui seis casos. Cada gate tem uma rota current e uma rota
adversarial. A máquina deriva o outcome de cópias dos corpora FZ0, da
classificação do ledger e do protocolo HUM0. Ela não lê `expected`, `status`,
score, preferência ou resultado fornecido pelo caller para selecionar o
outcome.

W-707 fecha o protocolo de completude FZ0. W-731 confirma uma disposition
explícita para cada decisão e `Research=0` em todo o ledger. W-1408 preserva stop-on-first-violation,
no-automatic-promotion e zero registros humanos/modelos. A reabertura explícita
posterior PFU0 deixa de ser residual: as três decisões são current/oracle-backed
e o freeze pode ser fechado.

Os gaps de `w-compile`, `w-run`, compiler, runtime, provider, human-study e
model-study permanecem missing. O estudo usa somente source refs, máquinas,
snapshots e oracles host existentes. O stop condition rejeita qualquer Research
residual global e exige evidência PFU0 e revisão do freeze antes de recascade.

Checks scoped:

```sh
bun test tooling/studies/final-research-closure/oracle.test.mjs
bun tooling/check-final-research-closure.mjs
bun run --cwd tooling/tree-sitter-w check:frc0
```
