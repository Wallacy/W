# RDX0 — binary-first registry e execução assinada

RDX0 é um bundle finito de registro de pesquisa. Ele conecta distribuição
binary-first, registry HTTP static-first, publicação closed-source, evidence,
runner assinado, sandbox provider e entitlement capability.

O bundle não implementa registry, compiler, runtime, runner, sandbox ou DRM.
Ele não afirma conformance, isolamento, descarte físico do source ou DRM
inviolável. `DESIGN.md` continua a autoridade normativa. Este diretório registra
o ledger auditável e o seu checker.

## Estado e fronteira

`RDX0` é **Direção** para um candidate protocol. `PCB0`, `WEC0`, `TEV0`,
`SEV0`, `SBX0`, `RSX0` e `ENT0` são **Pesquisa**. Nenhuma task é `Forma vigente`
de implementação. Nenhuma task usa `oracle-backed-current`. Este é o estado
de registro atual. Uma revisão futura do schema e do checker pode promover uma
task somente após evidence suficiente. O checker não congela `Direção` ou
`Pesquisa` para sempre.

O candidate protocol usa `w.registry-http/1`. HTTP/1.1, HTTP/2 e HTTP/3 são
transportes equivalentes. HTTPS é obrigatório, salvo transportes locais
explicitamente selecionados. A spelling dos paths abaixo não é syntax de W.

| Recurso lógico | Candidate path | Limite registrado |
|---|---|---|
| discovery | `/.well-known/w-registry.json` | metadata de schema, origins e profiles |
| package projection | `/v1/packages/<encoded-package-id>/index.json` | signed, bounded e monotonic |
| release record | `/v1/releases/<algorithm>/<digest>.json` | immutable por digest |
| object | `/v1/objects/<algorithm>/<digest>` | GET e HEAD; Range opcional |
| catalog checkpoint | `/v1/catalog/checkpoint.json` | trusted checkpoint assinado |
| catalog pages | `/v1/catalog/pages/<first>-<last>.jsonl` | immutable append-only para mirror/search |
| evidence | `/v1/evidence/<algorithm>/<subject-digest>/index.json` | referências para attestations imutáveis |
| channel | `/v1/channels/<encoded-package-id>/<encoded-channel>/<encoded-target-profile>.json` | signed convenience sem authority |

Known-identity resolution não usa search. Search é uma projection derivada do
catálogo. Um endpoint dinâmico `/v1/search` é opcional e não entra no lock. A
conformance suite deve reconstruir o índice somente de checkpoint e pages. JSON
é UTF-8 estrito e duplicate keys são rejeitadas. O estudo produz um output de
pesquisa para canonical signing payload sem escolher canonicalização sem
evidence. Package index rollback compara com trusted checkpoint, não apenas com
contador do servidor.

Download privado pode usar short-lived read capability ou signed URL scoped por
object/package, audience e expiry. Token só concede acesso. Digest continua a
identity dos bytes. Privacy mode pode retornar 401, 403 ou 404 conforme policy,
e mirror não ganha authority.

## Tasks registradas

O [task ledger](task-ledger.json) contém dependencies, outputs observáveis,
cases adversariais, evidence missing e stop condition para cada task.

| Task | Estado | Saída finita |
|---|---|---|
| RDX0 | Direção | schemas candidate, fixtures de transporte, catálogo e conformance |
| PCB0 | Pesquisa | intent, assertion OIDC validada, capability W one-use, attestations e autorização final |
| WEC0 | Pesquisa | cápsula bounded, `ExecutionDescriptor`, fingerprints e benchmark de reuse |
| TEV0 | Pesquisa | descriptor completo, `TestPlan`, evidence keyed e lanes separadas |
| SEV0 | Pesquisa | security attestations, SBOM/closure, advisory matches e snapshots |
| SBX0 | Pesquisa | profiles, enforcement receipts e negative cases de sandbox |
| RSX0 | Pesquisa | resolução exata, admission receipt e launch child-process |
| ENT0 | Pesquisa | entitlement lease opaco, policy online/offline e residual risk |

## Invariantes de pesquisa

- Release records e objects são imutáveis.
- Deprecation é append-only e recomenda replacement.
- Yank impede nova resolution por default. Lock existente segue a policy.
- Revocation bloqueia install ou execution no scope publicado.
- Search, mirror, object host, portal e builder não concedem package authority.
- Builder signing não usa private key do maintainer.
- Registry não precisa receber source.
- OIDC assertion é uma prova curta da identity do workflow, não uma credencial W
  de uso único. O serviço W valida issuer, audience, subject, workflow e ref
  antes de emitir publication capability W one-use, curta e scoped.
- Provider e tools podem observar source. Pinned builder/toolchain/actions,
  egress mínimo, redaction de logs/artifacts, secret lifecycle e provider
  identity são outputs de PCB0.
- Plano e billing da CI são externos ao protocolo. O bundle não afirma que
  GitHub gratuito atende source fechado.
- Claim do provider sobre descarte não prova descarte físico.
- Native code remoto usa child process ou compartment.
- Signature não concede capability.
- Audit ou learn mode somente sugere policy.
- Consumer install/run policy pertence ao local ou deployment.
- O projeto Linux [cloudflare/sandbox](https://github.com/cloudflare/sandbox) é
  apenas precedente de seccomp allow/deny/log. Não é o Cloudflare Sandbox SDK e
  não prova isolamento W.

Read capability ou signed URL privada concede acesso scoped por object/package,
audience e expiry. Privacy mode pode retornar 401, 403 ou 404. Mirror não
ganha authority. Digest continua a identity dos bytes.

HIR, MLIR e LLVM bitcode permanecem privados da recipe. A cápsula pode apontar
para objects nativos, `WInterface`, `WMeta`, `WAbi`, symbol manifest, runtime
requirements, generic body chunks permitidos e optimization summaries. Seu
`ExecutionDescriptor` registra entrypoints, requirements, sandbox profile e
payload refs. Section/chunk fingerprints e runtime measurement map tratam
relocation/ASLR sem raw in-memory hash. IR privado exige exact toolchain key. A
família de artifact é por target, profile e toolchain-plan row. Benchmark source
rebuild versus exact capsule reuse/link mede compile time, cache granularity,
storage e network. Não existe promessa de universal binary.

`TEV0` exige stable ID, owner declaration, origin carrier, source map, kind,
fixtures/effects, oracle ou expected diagnostic/outcome, target/profile,
seed/limits e body/plan digest. Async/cancellation e snapshot/golden identity
ficam separados. `SEV0` exige analyzer identity/version, policy,
corpus-or-database digest, target/profile, SBOM, RuntimeClosure, reachability,
advisory matches, reevaluation snapshots, portal freshness e conflicting
analyzer records. Não há safe badge agregado.

O witness adversarial do Restaurante parte do fluxo `compile-final-menu /
menu-compiler` em [`reference/last-light/README.md`](../../../reference/last-light/README.md),
do manifest/transform reais e dos oracles de release, simulation e capability.
Ele percorre RDX0, PCB0, WEC0, TEV0, SEV0, SBX0, RSX0 e ENT0 com release intent,
dois builders autorizados, cápsulas por target, doctests/fuzz/security
evidence, update/deprecation, revocation, search rebuild, `w run` sem network
por default e entitlement opcional. É fixture/case de design, não execução.

## Evidência e conclusão

O bundle usa design sources, precedentes primários registrados em
[`RATIONALE.md` §1.17](../../../RATIONALE.md#117-fontes-e-perfis-operacionais-retirados-do-design-normativo),
e casos host-side do ledger. Ainda faltam compiler, runtime, provider, sandbox
enforcement, attestation verifier, hardware, fault stress, local/split
evidence e estudos humano/modelo.

Uma task termina somente quando todos os outputs do ledger existem, os cases
adversariais têm resultado observável, a evidence missing foi resolvida e a
stop condition específica foi satisfeita. Até lá, a task permanece no estado
registrado. Uma revisão futura do schema/checker pode promover o estado somente
após evidence. O checker atual é registration-only e não substitui provider
evidence.

Execute o check scoped na raiz do repositório:

```text
bun tooling/studies/rdx0-binary-registry-execution/check.mjs
```

As decisões normativas e os gates ficam em
[`DESIGN.md` §§21.3–21.4, 22.2, 24.1.2 e 26.9–26.11](../../../DESIGN.md#213-verificação).
