# AVF0 — disponibilidade e configuração gradual

Este estudo separa três problemas que costumam receber o mesmo nome de
"feature flag":

1. **feature de package** altera aditivamente o grafo estático antes da
   compilação;
2. **availability** prova se uma declaração existente pode ser usada no target
   e no provider selecionados;
3. **feature de runtime** escolhe um valor tipado em uma configuração imutável
   já autorizada.

Essa separação é o resultado principal. Uma configuração de runtime não pode
carregar módulo, habilitar dependência, conceder capability ou effect, alterar
interface/ABI nem tornar disponível uma declaração que o compiler rejeitou.
Todos os ramos alcançáveis permanecem compilados e auditados.

O corpus source preserva o binding de availability como historical-research
provenance pré-ASIC0. ASIC0 fecha como design current o contrato de facts
autenticados e binding typed fail-closed, sem keyword ou nova runtime authority; W-1449 mantém
as lacunas de compiler, diagnostics e provider. Esta nota não afirma
implementação.

## Formas estudadas

- `current-package-feature.w` usa a feature aditiva já prevista pelo manifest.
- `typed-runtime-feature.w` mostra o carrier tipado que uma biblioteca/provider
  pode publicar com `SnapshotCell`; não propõe keyword.
- `availability-binding.txt` reserva uma possível forma de binding que permite
  ao compiler estreitar disponibilidade sem esconder a evidência do provider.
- `rejected-runtime-authority.txt` registra mecanismos que permanecem
  rejeitados.

O binding de availability é current como contrato de design condicionado a
evidence e receipts; a integração com diagnósticos, o receipt do provider e as
regras por domínio ainda não foram validados por um compiler W e permanecem em
W-1449. A flag dinâmica é **componível** com tipos,
`SnapshotCell`, configuração versionada, hash determinístico e auditoria de
exposição; ela não requer sintaxe nova nesta rodada.

## Invariantes

- fallback pertence ao tipo da chave e é sempre definido;
- schema da chave e geração/configuração publicada têm identidades separadas;
- avaliação local usa somente campos declarados do contexto;
- rollout percentual é determinístico para o mesmo schema, regra e sujeito;
- regras com a mesma prioridade são rejeitadas;
- stale/missing config retorna fallback, sem ampliar authority;
- exposição é uma operação explícita posterior à decisão;
- availability é avaliada antes da feature de runtime;
- package feature só adiciona itens declarados ao grafo.

## Evidência e limites

O corpus tem 38 casos e o oracle é executado no host com Bun. As fontes `.w`
são apenas witnesses parseáveis. As referências Cloudflare Flagship, Swift e
OpenFeature são fontes primárias de comparação, não autoridade sobre W.

Ainda faltam compiler/type checker, provider de availability, provider de
configuração, publicação real por `SnapshotCell`, testes local/split, fault e
stress, além de estudos humano/modelo. Nenhum artefato deste diretório afirma
implementação de runtime.

## Stop condition

Não chamar o binding de availability de implementação pronta enquanto dois
domínios reais não provarem lifecycle, target facts, capability/effect checks,
receipts e diagnósticos. Não promover uma API de flags enquanto type/fallback,
schema e config identities, rollout determinístico, snapshot atômico,
exposição, expiry/owner e ausência de authority amplification não forem
verificados em compiler/provider e em duas projeções de target.
