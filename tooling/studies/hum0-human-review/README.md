# HUM0 — revisão humana e de modelos

HUM0 é um protocolo cross-cutting de ergonomia para os problemas do Restaurante
no Fim do Universo. Ele não é um bundle R1: não possui `bundle.json`, não cria
variantes de syntax e não escolhe uma forma normativa.

O protocolo mantém exatamente oito slices problem-first e quatro tarefas por
slice: `explain`, `recall`, `repair` e `change`. Cada slice usa referências reais
do `reference/last-light`, oracles host independentes com digests, um input
primary e um input adversarial com o mesmo problema e outcome, ordens
counterbalanced e blinding. Cada stimulus é uma janela UTF-8 bounded, alinhada a
limites de linha e derivada por símbolo/digest; a mutation única e o repair
esperado ficam observer-only. IDs, caminhos, digests, oracle, expected e fatos
de implementação ficam fora do input visível. O renderer devolve somente
`scenario`, `task`, `instruction`, `source` e `blindedLabel`. Fatos
determinísticos podem aparecer somente em `w explain`, dentro da lista
`explainableFacts`.

O snapshot mede apenas a prontidão do protocolo: oito slices, 32 tarefas e zero
registros humanos/modelos. Ele não mede score, preferência, vitória ergonômica,
compreensão ou implementação W. Nenhum participante ou modelo foi executado.

Os contratos de registro futuros separam:

- humano: `participantIdHash` sha256, background não-vazio C/Rust/Python/W,
  tempo/queries não negativos, confiança 1–5 e outcomes semanticamente
  verificados por oracle; sem PII;
- modelo: provider, modelo, versão, tokenizer, params JSON fechado, digests
  sha256, tokens input/output/total com soma e outcomes verificados por oracle.

No slice FFI, `BellLease` demonstra registration optional e unsubscribe
guardado; drain de callbacks em voo é obrigação externa do oracle, não uma
garantia inferida do source.

A coleta para no primeiro expected echo, outcome forjado, digest/símbolo ausente
ou stale, vazamento de identidade interna, divergência de problema/outcome,
registro duplicado ou desacordo do oracle. O caso permanece Research e exige um
caso independente antes de qualquer revisão normativa.

Checks scoped:

```sh
bun test tooling/hum0-human-review-reference.test.mjs tooling/studies/hum0-human-review/oracle.test.mjs
bun tooling/check-hum0-human-review.mjs
```

O gate não compila nem executa W, não coleta pessoas/modelos e não promove
design automaticamente.
