# SEC0 — modelo de segurança amplo

SEC0 amplia a segurança de W além de memória e paralelismo. O estudo trata a
segurança como composição de provas no compiler, capability/effect mediation,
limites de entrada e recursos, supply chain, secrets, auditoria, isolamento,
deployment, FFI e risco residual.

O corpus source preserva 13 outcomes como historical-research labels pré-ASIC0. ASIC0
fecha como design current os contratos de evidence/admission para os seis
profiles, residual de side-channel, patch attestation e deployment/hardening;
W-1450 mantém a evidência de implementação. O bundle não afirma security
conformance.

O princípio central é uma regra de substituição. W pode omitir uma proteção de
runtime somente quando uma prova estática, enforcement de hardware ou mediação
externa cobre o mesmo invariante. Uma exceção de threat model é restrita a
isolation, tenant ou side-channel, exige `threatExclusions` separado e um
receipt de `policy-review`. Toda proteção, presente ou omitida, usa um receipt
fechado com issuer/stage compatíveis, escopo de profile/target e digests
SHA-256. `runtimeEnforcement: present` só aceita basis
`runtime-enforcement` com issuer `runtime-provider` e stage `runtime`;
`omitted` exige uma basis substituta. O profile fixa target e artifact digest,
e o receipt deve coincidir exatamente com ambos. Um booleano, uma flag, uma
configuração ambiental ou uma opção de performance não satisfaz essa regra.

## Perfis estudados

- `trusted-native-cpu`
- `sandboxed-native-process`
- `wasm-component`
- `multi-tenant-isolate`
- `embedded-freestanding`
- `fpga-asic-hardware`

Os perfis podem mudar `WAbiKey`, runtime closure e hardening receipts quando a
representação física muda. Eles preservam `SemanticInterfaceKey` quando o
contrato público permanece igual. Deployment pode reduzir budgets, mas não
enfraquece o product minimum.

## Invariantes

- Safe W mantém memory, type, effect, capability, input e resource proofs.
- Os mínimos comuns a todos os seis perfis são `memory-safety`,
  `effect-capability-checks`, `input-bounds` e `supply-chain`. Cada profile
  adiciona somente controles físicos próprios.
- API access vem de capability explícita e effect declarado.
- Lookup ambiental, authority por string e capability amplification falham.
- Input traversal, resource use, secrets e audit têm limites explícitos.
- Supply chain liga source, lock, artifact, signer e attestation.
- Check provado pode ser elidido. Check não provado permanece, falha o build ou
  atravessa um `unsafe` explícito com contrato completo.
- FFI e raw pointers pertencem a uma boundary explícita.
- Runtime feature e availability do AVF0 não alteram profile ou authority.
- Side channels têm threat model e residual risk. Nenhum perfil promete uma
  solução universal.

## Evidência e limites

O corpus possui 101 casos. O oracle host deriva 24 aceitos e 77 rejeitados. Ele
produz 11 accepted current e 13 accepted current-design-evidence-gap outcomes;
essas labels são historical-research provenance pré-ASIC0. O corpus cobre seis perfis,
16 rejeições de authority e quatro rejeições de caller echo.
As fontes Last Light são witnesses de capability, channels, filesystem, limits,
deployment capacity e ABI. Cloudflare Workers, Linux seccomp, WebAssembly,
WASI, RATS e Sigstore são fontes primárias de comparação.

O estudo não implementa compiler, runtime, sandbox, provider, hardware,
attestation verifier ou deployment control plane. Ainda faltam provas reais de
isolation, side-channel budgets, secret lifecycle, patch rollback, FFI fault
injection, local/split equivalence e compreensão humana ou de modelo.

## Stop condition

Não promover SEC0 a contrato implementado até que cada perfil tenha compiler
facts, provider receipts, artifact and hardening receipts, negative tests e
fault-injection. A evidência deve cobrir pelo menos um target local e um target
split ou externally mediated. O produto mínimo deve continuar monotônico.
