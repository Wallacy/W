# RDX0 — binary-first registry e execução assinada

RDX0 é o bundle de design e oracle da decisão W-1518. Ele conecta distribuição
binary-first, registry HTTP static-first, publicação closed-source, evidence,
runner assinado, sandbox provider e entitlement capability.

O bundle não implementa registry, compiler, runtime, runner, sandbox, crypto
verifier ou DRM. `DESIGN.md` continua a autoridade normativa. Este diretório
registra o ledger auditável, o reducer estrutural e o corpus host-only.

## Estado e fronteira

W-1486 é histórico e foi superseded por W-1518. RDX0 está em
`complete-design-study`: o contrato é `oracle-backed-current`, mas nenhuma
task afirma implementação ou conformance de provider. Cada task preserva
`stateAtRegistration` (`Direção` ou `Pesquisa`) e agora tem
`currentDesignDisposition: closed-by-W-1518`; `Pesquisa` não é um estado
corrente.

O oracle aceita e rejeita somente estruturas e transições de design. Ele não
faz cryptography, HTTP, server, registry, attestation verification, provider
enforcement ou execução nativa. O status do bundle não congela uma futura
promoção: somente evidence independente e revisão de schema/checker podem
promover uma implementação.

## Contrato current

O protocolo de design corrente usa `w.registry-http/1`. HTTP/1.1, HTTP/2 e HTTP/3 têm
semântica equivalente. HTTPS é obrigatório, salvo transporte local
explicitamente selecionado. Os paths não são syntax de W:

| Recurso lógico | Path | Regra |
|---|---|---|
| discovery | `/.well-known/w-registry.json` | não confiável; não concede authority |
| root update | `/v1/root/<version>.dsse` | imutável; exige `N+1` e threshold old+new |
| timestamp | `/v1/timestamp.dsse` | único object mutável; clock e persistência monotônica |
| todos os demais objects | `/v1/o/sha256/<hex>` | imutáveis; SHA-256 tagged; GET/HEAD obrigatórios, Range opcional |
| channel | `/v1/channels/<encoded-package-id>/<encoded-channel>/<encoded-target-profile>.json` | JSON convenience opcional |
| search | `/v1/search` | projection opcional, sem authority ou lock |

`targets`, `snapshot`, package index, release/state, catalog, evidence e
capsule são objects em `/v1/o/sha256/<hex>`. `trustedGenesis` é o payload
público completo da gênese, fornecido out-of-band e sem assinatura DSSE
obrigatória. Roots seguintes usam root updates DSSE. Roles `root`, `targets`,
`snapshot` e `timestamp` impedem rollback, freeze, mix-match, gap e expiry.
`trustedCheckpoint` é persistido pelo resolver e não é novo trust input.

Metadata owned por W usa payload CBOR determinístico e envelope DSSE
role-specific sobre os bytes exatos do payload. O digest do object cobre os
bytes exatos armazenados, inclusive o envelope. Attestations externas usam
`in-toto Statement v1` e `SLSA provenance v1.2`: Statements JSON permanecem
JSON dentro de objects DSSE imutáveis; W não os converte para CBOR e eles não
criam uma segunda package authority. JSON é somente discovery, search, channel
e update convenience, nunca authority, lock ou payload canônico W. Duplicate
keys são rejeitadas e JSON é UTF-8 estrito.

O JSON pequeno de channel/update pode conter `schema`, `snapshotDigest`,
`package`, `channel`, `targetProfile`, `version`, `releaseDigest`,
`artifactDigest` e `state`. Um update dinâmico pode responder `204` para
“sem mudança”. O resolver deve revalidar a cadeia autoritativa; o JSON não
substitui lock ou release verification. Package index associa version a
release digest. Deprecation, yank e revocation são estados append-only.
Known identity/update nunca usa search. Mirror só serve bytes com digest igual
e não ganha authority.

Read capability privada ou signed URL é curta e scoped por object/package,
audience e expiry; privacy policy pode escolher 401, 403 ou 404 e auth não é
encaminhada para origem/mirror não configurado.

Freshness exige clock/provider explícito e persistência monotônica. Fetch,
install e remote run falham fechados sem prova de tempo/expiry. Uma policy
offline explicitamente pinada apenas reutiliza artifact já verificado e
registra `stale`/`unknown`; não baixa, atualiza ou vira default.

## Publication, capsule e execução

PCB0 exige release intent assinado, OIDC GitHub validado por `iss`, `aud`,
`sub`, `repository`, `ref`, `sha` e `job_workflow_ref` pinado, além de
capability W curta, one-use e scoped. Builder signing, registry admission e
maintainer final authority são papéis distintos. `publicSource`,
`authorizedReproduction` e `independentPublicReproduction` são claims
separados; observação ou remoção declarada pelo provider não prova descarte
físico.

WEC0 usa `w.capsule/1`, bounded por target/profile/toolchain, com chunks
content-addressed, `WInterface`/`WMeta`/`WAbi`, native objects, static archive,
symbols e runtime requirements. Exact capsule reuse/link é a rota rápida
current de design; dylib/exe é futuro e não é claim. IR privado exige exact
toolchain key. Não há claim de hash de memória relocada.

TEV0 e SEV0 mantêm evidence keys e lanes separadas; SBOM, RuntimeClosure,
reachability, advisory e snapshots não produzem badge agregado `safe`.

RSX0 usa a forma exata
`w run registry:last-light/restaurant@0.1.0 --product last-light-native
[--entry default] -- ...`. A admission segue root trusted, freshness/snapshot,
package/release/artifact digests, evidence/policy, sandbox e entrypoint.
Native code remoto usa child process ou compartment, com enforcement antes do
loader/entry. Receipt do provider é obrigatório ou o resultado é
`unsupported`, sem fallback. ENT0 mantém entitlement opaco, sem raw token no
app e sem claim de DRM inviolável.

## Precedentes externos

As referências foram acessadas em 2026-09-03. [TUF latest specification](https://theupdateframework.github.io/specification/latest/)
fornece o precedente de roles, freshness e rollback; [DSSE protocol](https://github.com/secure-systems-lab/dsse/blob/master/protocol.md),
[in-toto Statement v1](https://github.com/in-toto/attestation/blob/main/spec/v1/statement.md)
e [SLSA provenance v1.2](https://slsa.dev/spec/v1.2/provenance) definem o
formato externo de attestation; [GitHub reusable-workflow OIDC](https://docs.github.com/en/actions/how-tos/secure-your-work/security-harden-deployments/oidc-with-reusable-workflows)
fornece o precedente de identity; e [OCI Distribution](https://github.com/opencontainers/distribution-spec/blob/main/spec.md)
fornece um precedente de transporte por digest. Essas fontes sustentam
inferências de design W, não conformance externa nem implementation evidence.

## Tasks registradas

O [task ledger](task-ledger.json) contém dependencies, outputs observáveis,
cases adversariais, evidence missing e stop condition para cada task. O
[`cases.json`](cases.json) e o [`machine.mjs`](machine.mjs) formam o oracle
estrutural; [`results.snapshot.jsonl`](results.snapshot.jsonl) é sua saída
determinística; [`reference.test.mjs`](reference.test.mjs) é o teste host.

| Task | `stateAtRegistration` | `currentDesignDisposition` | Saída finita |
|---|---|---|---|
| RDX0 | Direção | closed-by-W-1518 | schemas current, fixtures de transporte, catálogo e conformance design-only |
| PCB0 | Pesquisa | closed-by-W-1518 | intent, OIDC validada, capability one-use, attestations e autorização final |
| WEC0 | Pesquisa | closed-by-W-1518 | cápsula bounded, `ExecutionDescriptor`, fingerprints e reuse plan |
| TEV0 | Pesquisa | closed-by-W-1518 | descriptor completo, `TestPlan`, evidence keyed e lanes separadas |
| SEV0 | Pesquisa | closed-by-W-1518 | security attestations, SBOM/closure, advisory matches e snapshots |
| SBX0 | Pesquisa | closed-by-W-1518 | profiles, enforcement receipts e negative cases de sandbox |
| RSX0 | Pesquisa | closed-by-W-1518 | resolução exata, admission receipt e launch child-process |
| ENT0 | Pesquisa | closed-by-W-1518 | entitlement lease opaco, policy online/offline e residual risk |

## Evidência e execução do gate

O witness adversarial do Restaurante percorre as oito tasks com release intent,
dois builders, cápsulas por target, doctests/fuzz/security evidence, estados
append-only, search rebuild e `w run` sandboxed. Ele é fixture de design, não
execução.

```text
bun tooling/studies/rdx0-binary-registry-execution/reference.test.mjs
bun tooling/studies/rdx0-binary-registry-execution/check.mjs
```

Uma revisão futura do schema/checker pode promover uma task somente após
evidence independente suficiente. Ainda faltam crypto verifier, registry/server,
compiler, runtime, provider, sandbox enforcement, attestation verifier,
clock/freshness, hardware, fault-stress, multi-builder reproduction,
platform/OS controls, local/split evidence e estudos humano/modelo. Nenhum
timing ou result de benchmark é publicado.

As decisões normativas ficam em
[`DESIGN.md` §§21.3–21.4, 21.6.2 e 24.1.2](../../../DESIGN.md#213-verificação)
e a justificativa fica em
[`RATIONALE.md` §1.17](../../../RATIONALE.md#117-fontes-e-perfis-operacionais-retirados-do-design-normativo).
