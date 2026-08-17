# FRC0 — fechamento das três gates de pesquisa

FRC0 é um bundle R1 de design-oracle-input. Ele fecha somente a fronteira de
processo de W-707, W-731 e W-1408. Ele não compila, executa ou promove uma
implementação W. Ele não cria registros humanos ou de modelos.

O corpus possui seis casos. Cada gate tem uma rota current e uma rota
adversarial. A máquina deriva o outcome de cópias dos corpora FZ0, da
classificação do ledger e do protocolo HUM0. Ela não lê `expected`, `status`,
score, preferência ou resultado fornecido pelo caller para selecionar o
outcome.

W-707 fecha o protocolo de completude FZ0. W-731 confirma uma disposition
explícita para cada decisão e Research vazio. W-1408 preserva stop-on-first-
violation, no-automatic-promotion e zero registros humanos/modelos.

Os gaps de `w-compile`, `w-run`, compiler, runtime, provider, human-study e
model-study permanecem missing. O estudo usa somente source refs, máquinas,
snapshots e oracles host existentes.

Checks scoped:

```sh
bun test tooling/studies/final-research-closure/oracle.test.mjs
bun tooling/check-final-research-closure.mjs
bun run --cwd tooling/tree-sitter-w check:frc0
```
