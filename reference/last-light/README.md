# Restaurante Última Luz

> **Status:** produto de referência experimental · 31 de julho de 2026

O Restaurante Última Luz serve a última janela observável antes do encerramento
do universo. O cenário homenageia o absurdo cósmico popularizado por Douglas
Adams. Os personagens, diálogos, pratos e eventos deste corpus são originais.

Este é o produto de referência oficial do W. Ele orienta design, regressão,
conformance, benchmarks, documentação e treinamento.

Ele não é uma coleção de snippets. Cada subsystem deve evoluir para uma rota
operacional ou para um oracle negativo. O compiler, a std e o runtime só podem
afirmar suporte quando a rota correspondente compila e passa seus gates.

O produto não prova que a linguagem está implementada. Ele pressiona a forma
integrada de [DESIGN.md](../../DESIGN.md). O
[plano de build](BUILD.md) aplica products, target specs, toolchain plans, host
profiles e artifacts.
O record `workspace` data-only e o record `package` principal estão no único
[`build.w`](build.w); o workspace é owner de resolution e deployments.

## Limite de benchmark BMD1

O runner BMD1 usa somente
`reference/last-light/checker_bootstrap.w` para medir
`clean × check-end-to-end` do seed compiler. Ele prova o oracle do comando
`w check` antes das samples e mede processos novos com monotonic wall clock em
ns. O escopo inclui startup do processo e estado de cache do filesystem e do OS.

O runner não mede este produto Restaurant. Não prova runtime, provider, HTTP,
database, as outras stages do compiler lifecycle, no-op, edit, native backend,
language workload ou comparação de performance. `benchmark_app.w` continua uma
fonte de design para a futura matriz de composition. Essa matriz permanece
blocked por runtime/provider e não cria variantes artificiais do app.

## 1. Rotas operacionais

```text
LastLightSimulation
  → cenário fechado
  → simulação por ticks
  → relatório determinístico

last-light-native / entry .default
  → Command
  → dispatch único
  → RestaurantApi
  → AppResponse
  ├─ texto portátil
  ├─ terminal ANSI
  └─ JSON por HTTP

last-light-tui / entry LastLightTui
  → product liga o mesmo shutdown
  → seleciona o terminal adapter do target
  → remove os modes não alcançáveis

last-light-worker / LastLightWorker
  → http.fetch fornecido pelo host
  → o mesmo dispatch e RestaurantApi

last-light-mobile / LastLightMobile
  → lifecycle Android ou iOS
  → domain logic compartilhada

last-light-controller / LastLightController
  → sensors, interrupts e satellite telemetry

TAB1 telemetry / data_formats.w
  → CSV upload typed
  → Parquet archive por SnapshotByteSource
  → Arrow IPC service handoff
  → C Data import trusted
  → todos os batches no mesmo schema, rows e outcome
  → `String?` view somente no scope; `copy` materializa owner
  → nested/custom exige projeção tipada, adapter ou materialização explícita

Web bodies / web_bodies.w
  → Blob compõe shared Bytes imutável
  → FormData preserva ordem e campos repetidos
  → Request/Response movem o body
  → multipart fica bounded e escolhe boundary no adapter

last-light-observatory / LastLightObservatory
  → swarm de satélites e sensores do horizonte
  → duas calls concorrentes com join estruturado

last-light-audio / LastLightAudio
  → callback com deadline
  → sem allocation ou blocking

last-light-accelerators / export lastLightKernels
  → tensor kernels
  → NVVM, ROCDL ou SPIR-V

last-light-ai-lab / LastLightAiLab
  → treino linear no host
  → o mesmo kernel possui lowering de device

last-light-dlpack / tensor_interop.w
  → import DLPack 1.3 versioned trusted
  → zero-copy tensor [samples, 6]
  → queue/device explícitos e callback scoped
  → scores e export consuming
  → copy-to-host explícita e adversariais host

compile-final-menu / menu-compiler
  → build.transform
  → input e output tipados
  → action-result/manifest e resource imutável no CAS
```

O consumer oficial usa `build.Context` com os overloads fechados de `String` e
`Bytes`. O provider `std.build@1` continua missing, portanto este source é um
contrato e um oracle de integração, não uma alegação de execução. `Context`
somente lê inputs e materializa candidatos em staging. O host publica o
action-result/manifest depois de success, outputs obrigatórios e budgets válidos.

`LastLightSimulation` é o primeiro alvo operacional. Ele não usa relógio,
aleatoriedade, network nem deployment de services. O mesmo profile deve produzir
os mesmos eventos, totais e consumo de energia em qualquer execução compatível.

O descriptor anônimo de `app.w` é `.default`. O host liga `runNative` a
`process.main`; o source não repete esse nome. O handler escolhe CLI, TUI ou
servidor local por argumento.

`LastLightTui` é independente de `.default`. O product `last-light-tui`
seleciona esse descriptor. `runTuiEntry` registra `shutdown` no runtime. A
recipe pode remover handlers e modes não alcançáveis.

`LastLightWorker` possui outro módulo de entry. Seu host chama `http.fetch`.
Ele importa `gateway.w`, não o módulo nativo. Importar qualquer entry module
também não registra ou executa seu descriptor.

A rota distribuída mantém o gate **Turno do Horizonte Violeta**:

```text
RestaurantApi
  → parser streaming da Comanda de Íon
  → compiler W0 do Cardápio de Fótons
  → Salão Prisma
  → Cozinha de Maré Fria
  → controle PID do forno
  → Brigada do Cometa Manso
  → Observatório do Cometa Paciente
  → Placar da Improbabilidade Residual
  → Oráculo de Mesas
  → Sonda de Aroma
  → Arquivo de Ecos shared/weak
  → Sino de Encerramento pinned
  → Scope Lexical do Cardápio
  → Janela de Serviço sem Dono
  → Passa-Pratos de Capacidade Finita
  → Recepção callable do Último Maitre
  → Conta da Aurora Tardia
```

Uma falha injetada em cada seta não pode deixar task, lease, buffer, mailbox
item ou pagamento sem owner e estado observável. A configuração de deployment e
alguns services de infraestrutura ainda dependem de contratos não congelados.

O `place()` atual mantém um closed turn até servir o prato. Ele é um oracle de
head-of-line blocking. Ele não é a arquitetura operacional final. O gate de host
exige esta divisão:

1. um turn curto aceita o pedido e devolve sua identidade;
2. um owner supervisionado executa o workflow;
3. turns curtos consultam status e solicitam cancelamento;
4. a identidade do pedido seleciona a instance keyed;
5. trace e idempotency ligam todos os turns ao mesmo efeito.

`SupervisorRef`, o descriptor data-only e a API de steps duráveis são **Forma
vigente**. O journal, o adapter SQLite e o crash oracle ainda não possuem
implementação. Até existir runtime, `LastLightSimulation` continua o primeiro
alvo de execução independente.

## 2. Mapa de source

| Arquivo | Responsabilidade |
|---|---|
| `domain.w` | newtypes, refinements, enums e errors |
| `command.w` | parser streaming, spans, buffer limitado e comandos tipados |
| `text.w` | UTF-8, unidades de texto, normalização, paths e contrato de C string |
| `string_storage.w` | construção, reserva, reuse e carrier String/Bytes consuming |
| `collections.w` | arrays, views, labeled loops e blocks, operators compostos, Map/Set, hashing e stable sort |
| `semantic_matrix.w` | source oracle pequeno para expected-use, owner/effect/control assay e loop fixed-point com entry, continue, back-edge e break; CheckerContext completo permanece interno e é coberto pela matriz host |
| `checker_bootstrap.w` | witness bounded do checker CHK1 para source síncrono no perfil fechado de source único (closed-single-source). Não é um compiler completo |
| `views.w` | diferença entre owner, borrow completo e projeção de extent fixo |
| `failure.w` | Option, Result, typed throws, panic, OOM e cleanup |
| `generics.w` | primary associated types, constraints, static contract atoms, inference e witnesses |
| `enum_contracts.w` | subsets fechados de enum, narrowing e payloads |
| `state_transitions.w` | paths validados, typestate consuming e snapshots runtime |
| `reflection.w` | queries `type of`/`info of`, TypeId local, metadata opt-in, synthesis e visibilidade |
| `rest_arguments.w` | rest homogêneo, expansão `each`, ownership e call shape |
| `units.w` | SI, dimensão e units customizadas |
| `quantity_oracle.w` | Quantity/SI canonical value, affine points, IEC bits e schemas JSON |
| `numerics.w` | literais, conversões, overflow, primitives de bits portáveis, float, ranges, post-test loop e quantization |
| `kitchen.w` | resources move-only, protocols térmicos, ranges e controle PID |
| `oracle.w` | matriz/tensor, `@`, shape e cálculo de lotes |
| `performance.w` | fatos de prova, baseline portátil `std.simd`, largura interna e custos de texto |
| `hardware.w` | fronteira C, layout e deallocator |
| `abi.w` | façade C escrita em W, carriers e export exato |
| `abi_oracle.w` | reuse W exact, expectativa de import, call shape e fallback de boundary |
| `memory.w` | ownership, shared/weak, ciclos, Address, provenance, pinning e callback C |
| `shared_control_oracle.w` | construção declarativa `shared T`, `try` fora do tipo, origins `$storage`/`$controlBlock`, weak lifetime e rehome; a fonte FFI canônica está em `memory.w` |
| `hir_memory_oracle.w` | PlaceId, LoanId, reborrow, OriginSet, suspensão, representação e ABI |
| `borrowed_values.w` | kitchens disjuntas, stored `ref`/`view`/`inout`, Array de refs, reborrow e await stable |
| `borrow_expressivity.w` | callable borrowed por invocation, cursor lending, adapters source-shaped, origem única bodyless e rejeição de ambiguidade |
| `allocation.w` | placement, origem, mobilidade, allocator scope, budget e rehome |
| `allocator_oracle.w` | layout físico, provider, resize, progress e reclamation A0 |
| `representation_oracle.w` | matriz de representação por fronteira e fallback portátil |
| `callables.w` | function pointer, opaque callable, erasure e callable modes |
| `packages/menu-compiler/compiler.w` | compiler pequeno restrito ao profile `bootstrap.w0` |
| `packages/menu-compiler/transform.w` | entry hermética de build e bindings tipados |
| `packages/menu-compiler/build.w` | `.tool` product publicável do compiler |
| `menus/final.menu` | input do build transform |
| `execution.w` | task groups bounded, outcomes, ordering e cancelamento |
| `context_local_oracle.w` | inheritance de task-local, drain, boundaries e TLS físico |
| `mobility.w` | transferência exclusiva, sharing verificado e captures |
| `synchronization.w` | atomics, memory orders, CAS, `lock` residual e snapshots publicados |
| `lazy_oracle.w` | estado lógico, lowering, reentrada, publicação e drop de `Lazy` |
| `abort.w` | AbortSignal Web bounded, controller move-only, timeout, `any` e ponte HTTP |
| `json.w` | JSON bounded, profiles I-JSON/RFC 8259, synthesis explícita, cursors scoped e oracles de falha |
| `streams.w` | stream pull, readable Web, channel CH0, rendezvous, permits, close e owner recovery |
| `io.w` | byte I/O async, file posicional, buffers e chunks borrowed |
| `io_error_oracle.w` | kind portátil, operação lógica, cause opaco e recovery específica da aplicação |
| `time_oracle.w` | Duration exata, Clock explícito e Instant/Deadline limitados ao mesmo root |
| `fs_oracle.w` | root capability, native paths, rights, snapshot e publicação durable de arquivo |
| `data_formats.w` | fluxo TAB1 de CSV typed, Parquet snapshot, Arrow IPC e C Data trusted |
| `net_oracle.w` | addresses tipados, resolve/connect bounded, TCP split, listener accept e UDP truncation |
| `process_oracle.w` | Arguments nativos, ExitCode, signals e Context root-scoped sem singleton ambiental |
| `billing.w` | Money, idempotência, existential, opaque return e behavior |
| `dining.w` | serial turn, backpressure, applause e resposta |
| `restaurant.w` | integração de services, tasks, ownership e compensação |
| `supervision.w` | turn curto, `WorkKeyRef`, identity keyed e cancelamento |
| `workflow.w` | points duráveis, retry, timer, evento e compensação |
| `service_recovery_oracle.w` | fault matrix, dedup, FIFO por sender e isolamento de generation |
| `simulation.w` | cenários, algoritmo por ticks, capacidade, energia e receita |
| `presentation.w` | resposta tipada e render portátil ou ANSI |
| `pyn3_oracle.w` | fixture PYN3 para apresentação typed, adapter Jupyter e export comprovado |
| `gateway.w` | dispatch, routing por URL, body único e oracle de compile surface para clone bounded |
| `http_documents.w` | adapters direcionais de Command/AppResponse e Problem Details |
| `http_oracle.w` | constructors, limits, consuming reads, bounded clone, JSON, copied-headers override e net serve signature |
| `web_bodies.w` | Blob imutável, FormData ordered, upload multipart bounded e Response binária |
| `service_oracle.w` | seleção de link, camadas de boundary, commit gate, pipeline e evolução de schema |
| `service_streaming.w` | client, server e bidirectional streaming com um `Stream` explícito por edge |
| `task_settlement.w` | escolha first-settled de tasks já criadas, com winner, cancellation e drain explícitos |
| `session_security_oracle.w` | channel, transcript, 0-RTT e replay de session wRPC |
| `capability_security_oracle.w` | root grants, attenuation, delegation e revocation |
| `release_oracle.w` | digest, evidência de recipe, threshold de reprodução, mirrors e revogação |
| `metadata_oracle.w` | separação entre CBOR, WMeta1 e wWire; inputs da recipe |
| `wmeta_oracle.w` | envelope, profiles, offsets e open modes do WMeta1 físico |
| `bootstrap_oracle.w` | cadeia de stages, fechamento W0 e convergência do bootstrap |
| `lifecycle_oracle.w` | transições de task, turn de service e commit uncertainty |
| `scheduler_oracle.w` | replay lógico, packing físico e fault outcomes determinísticos |
| `liveness_oracle.w` | runtime closure E1, waits, completion/cancel races, frame/outcome reclamation e shutdown |
| `ownership_execution_oracle.w` | composição MX0 entre ownership, suspensão, child staging, cleanup, outcome e join |
| `domain_oracle.w` | seleção de domain, admission e redução bounded de capacity |
| `remote_stream_oracle.w` | eligibility, créditos, lifecycle, fault points e relay de service stream |
| `transaction_oracle.w` | transaction scope local/remoto, commit e incerteza |
| `wire_oracle.w` | profiles wWire, eligibility, decode preflight, strict decode e unknown fields |
| `restpc_oracle.w` | mapeamento entre operações RestPC e métodos HTTP |
| `simulation_app.w` | entry determinística sem deployment de services |
| `app.w` | processo nativo multimodo, terminal, signal handler e Context |
| `platform.w` | interface uniforme dos adapters nativos |
| `platform/posix/native.w` | implementação selecionada para Linux e Darwin |
| `platform/windows/native.w` | implementação selecionada para Windows |
| `worker_app.w` | component HTTP com lifecycle do host |
| record `package` em `build.w` | products, host bindings, service bindings, runtime graphs, targets e profiles |
| record `workspace` em `build.w` | members, defaults, patches, resolution e toolchain policy locais |
| `BUILD.md` | matriz de toolchains, artifacts, comandos e gates |
| deployment `local` no record `workspace` | plano local com uma unit e adapters de desenvolvimento |
| deployment `distributed` no record `workspace` | plano heterogêneo com services, devices e WASI |
| deployment `benchmark` no record `workspace` | PostgreSQL, cache local, admission e limites do benchmark |
| `orbit.w` | swarm de satélites, telemetria e propagação tipada |
| `horizon.w` | sensores do buraco negro, event time e tensor fusion |
| `horizon_tool.w` | oracle RU0 de módulo normal, dependency chart, entry explícito, requirement admission e menu do horizonte |
| `tensor_interop.w` | fixture PYN4 de carrier tensorial DLPack 1.3, device/queue, zero-copy, callback scoped, materialização e export consuming |
| `device_execution_oracle.w` | fixture DEV0 de descriptor, launch scope, queue receipt, cancel drain e equivalência CPU/device |
| `observatory_app.w` | processo nativo do swarm e da telemetria |
| `audio.w` | render de áudio com buffers fixos e sem allocation |
| `audio_app.w` | callback do audio device |
| `wifi.w` | captive portal, sessions, authority e limits |
| `wifi_documents.w` | adapters direcionais de Login, Revoke e Session |
| `wifi_app.w` | component HTTP do Wi-Fi |
| `ai_harness.w` | kernels, shapes e device bundle |
| `ai_lab_app.w` | harness nativo de treinamento e oracle CPU/device |
| `mobile_app.w` | lifecycle Android/iOS sem UI toolkit W |
| `controller_app.w` | reset, tick, interrupt e MMIO adapter |
| `system_escapes.w` | TLS scoped, MMIO tipado e ilha assembly com contract estático |
| `benchmark_app.w` | workloads HTTP e database para benchmark |
| `formatting.w` | fixture canônico para source order, comments e chamadas multilinha |

Esses arquivos usam a forma vigente. A consolidação substituída permanece
recuperável no histórico Git e não é fonte normativa.

### 2.1 Cobertura e alcance

O corpus separa duas perguntas. Um arquivo de ensaio mostra se uma forma local é
clara. Uma rota operacional mostra se as formas funcionam juntas.

`formatting.w` mantém a forma canônica que o formatter deve preservar. Ele
combina comments, source order, body curto e call multilinha em um único
fixture pequeno.

`representation_oracle.w` impede que uma otimização de memória atravesse uma
fronteira que exige bytes canônicos. Low bits ficam internos. C, wire e storage
persistente usam carriers explícitos.

| Camada | Testemunho principal | Benefício observado |
|---|---|---|
| módulos, visibilidade e entry | `app.w`, `simulation_app.w` | composição sem execução por import |
| tipos, refinements e enum subsets | `domain.w`, `enum_contracts.w` | estados inválidos saem do runtime |
| controle, patterns e errors | `command.w`, `failure.w`, `state_transitions.w` | fluxo exaustivo e falha tipada |
| ownership, views e allocation | `memory.w`, `views.w`, `allocation.w` | custo e lifetime aparecem no source |
| HIR de memória e ABI | `hir_memory_oracle.w`, `representation_oracle.w`, `abi.w` | move, borrow, pinning e fronteiras físicas são verificados antes do lowering |
| texto, collections e streams | `text.w`, `string_storage.w`, `collections.w`, `streams.w` | Unicode e backpressure ficam explícitos |
| async, paralelo e sincronização | `execution.w`, `mobility.w`, `synchronization.w` | estrutura e limites substituem threads soltas |
| services e compensação | `restaurant.w`, `billing.w`, `dining.w` | calls e efeitos remotos permanecem observáveis |
| service links e evolução | `service_oracle.w`, `service_streaming.w`, `session_security_oracle.w`, `capability_security_oracle.w`, `remote_stream_oracle.w`, `transaction_oracle.w`, `build.w` | placement, authority, streams, transaction e compatibility mantêm o mesmo contrato |
| wire portátil | `wire_oracle.w`, `orbit.w`, `kitchen.w` | codec rejeita tempo local, borrows e representações alternativas |
| supervisão e workflow | `supervision.w`, `workflow.w`, `build.w` | trabalho longo, recovery e placement mantêm owners explícitos |
| units, números, matriz e performance | `units.w`, `numerics.w`, `oracle.w`, `performance.w` | provas de domínio autorizam otimizações |
| C e layout | `hardware.w` | a fronteira estrangeira mantém ownership tipado |
| escapes de sistema | `system_escapes.w`, `controller_app.w`, `build.w` | MMIO, interrupt, TLS, placement e assembly mantêm authority explícita |
| ABI W e façade C | `horizon.w`, `abi.w`, `build.w` | interface, key, symbol e carrier ficam separados |
| self-host e build reproduzível | `packages/menu-compiler/` e o contrato de package | bootstrap e provenance têm um oracle pequeno |
| operação integrada | `simulation.w`, `gateway.w`, `app.w`, `restpc_oracle.w` | um dispatch tipado atende CLI, TUI e HTTP |
| products e targets | `build.w`, `BUILD.md` | grafo, variante, execution envelope, target e placement ficam separados |
| toolchains e SDKs | `build.w`, `BUILD.md` | requirements, providers e execution platforms ficam separados |
| satélites e horizonte | `orbit.w`, `horizon.w` | units, event time, services e tensors compõem |
| device e tempo real | `controller_app.w`, `audio.w` | interrupts, fixed buffers e deadlines ficam visíveis |
| mobile e Wi-Fi | `mobile_app.w`, `wifi.w` | lifecycle e authority usam capabilities |
| AI e benchmarks | `ai_harness.w`, `ai_lab_app.w`, `benchmark_app.w` | treino, kernels e desempenho preservam oracles |

A tabela cobre as famílias aceitas no design vigente. Itens em **Pesquisa**,
alternativas contrafactuais e propostas rejeitadas não são requisitos do
executável. Cada um continua preservado em `DESIGN.md`.

## 3. Casos e oracles

### 3.1 Pórtico de Nácar

Famílias: entry, host profile, imports e capabilities.

Aceite:

- `entry { ... }` cria um handler curto para um default slot único;
- `entry(runNative)` cria o descriptor anônimo `.default`;
- `entry LastLightTui(runTuiEntry)` cria um descriptor independente;
- o descriptor explícito não cria `args` ou `ctx`; adapters do host profile
  declaram qualquer binding de process e seus effects;
- o product escolhe um descriptor e declara callbacks ABI adicionais quando necessários;
- process signals usam registration runtime e lifetime explícito;
- callbacks ABI estáticos, como `device.tick`, usam `hostBindings`;
- importar `app` não executa um handler;
- Context não concede filesystem ou network ausentes.

### 3.1.1 Module run RU0

Famílias: módulo executável, entry explícito, imports, package/workspace,
resolution e deployments nomeados.

Aceite:

- `horizon_tool.w` é módulo normal, importa `chart.science` e declara
  `entry(runHorizon)`;
- `w run path/file.w` usa parser, checker e HIR normais; sem `--entry`,
  seleciona o descriptor explícito `.default`, e com `--entry Name`, o
  descriptor nomeado;
- o host profile declara adapters e assinaturas exatas. O baseline
  `native-process` inclui as duas formas vigentes: `fn(): ()` e handlers que
  declaram `std.process` `Arguments`, `Context` e `ExitCode` com os effects
  e return types do profile;
- package selecionado em contexto standalone é owner de sua resolution; package
  member omite resolution/deployments e a membership declarada seleciona o
  record `workspace` owner em `build.w`. Ancestor scan sozinho e duplicate
  owner falham;
- fora de projeto, somente std e imports locais explícitos são aceitos.
  Dependency externa não resolvida orienta criar ou adotar package/workspace;
- `w add/remove/resolve/update` operam no package/workspace. `w run` não faz
  solve, update, install ou fetch oculto;
- `resolution` usa schema `w.resolution/1`, deployments usam records
  `w.deployment/1` nomeados e `--deployment local` seleciona por nome;
- package publication exclui resolution/deployments locais. Package, resolution
  e deployment preservam identities e digests lógicos separados; deployment
  digest não altera `SemanticInterfaceKey`;
- o menu deriva `steady`, `warning` ou `evacuation` do score do horizonte;
- importar um source não executa seu entry, e módulo sem descriptor é importável
  mas não é alvo executável.

Adversariais:

- entry ausente, duplicado, incompatível ou seleção nomeada desconhecida;
- dependency externa em contexto efêmero, owner workspace duplicado, membership
  divergente ou resolution ausente/stale;
- import implícito, scan recursivo/cwd/`PATH`/environment, URL, stdin ou
  shebang;
- solve/update/fetch oculto, deployment selecionado por path, resolution ou
  deployment digest misturado na interface semântica;
- `export import`, reexport sem origem/item, ou bare behavior initializer;
- falha de authority, signature, CAS, artifact, action output ou capability antes
  do entry.

O host oracle [`tooling/module-run-machine.mjs`](../../tooling/module-run-machine.mjs)
exercita o contrato module-run RU0. Ele não executa W nem fornece compiler,
runtime, resolver, provider ou CLI. A proveniência PYN1 superseded está no
histórico Git e não entra no produto de referência.


### 3.1.2 Sessão/REPL transacional PYN2

Fixture: [`repl_session_oracle.w`](repl_session_oracle.w). Máquina:
[`tooling/repl-session-machine.mjs`](../../tooling/repl-session-machine.mjs).
Corpus e checker: [`tooling/repl-session-cases.json`](../../tooling/repl-session-cases.json)
e [`tooling/check-repl-session-cases.mjs`](../../tooling/check-repl-session-cases.mjs).

Aceite:

- `SessionId`, `SessionIncarnation`, `ExecutionOrdinal` e `GenerationId` são
  distintos. A incarnation começa em `g0`, cada publish avança o grafo e o
  prompt usa `w[n]`; `gN` não é aceito como identidade.
- `:reset`, `:restart` e `:quit` são boundary submissions mutating: produzem
  receipt/history e consomem ordinal. Commands read-only e cancellation queued
  não consomem ordinal; cancellation active usa o ordinal da submission.
- O wrapper aceita expressions, declarations, statements, loops, calls, tail
  display/discard, owner async/structured `await` sem annotation, local `spawn`
  e `defer`. Parser facts e
  checker facts produzem diagnostics separados; isso não legaliza execução
  arbitrária de módulo.
- `snapshot = limit * 2` conserva `6` após rebind de `limit`. `fn doubled` é
  dependente compilado com `BindingId`/version e hard-edge kind; fica unavailable
  com reason e closure. O transcript termina em g4 com w[5] unavailable e w[6]
  type error sem nova generation.
- O black-hole watcher conserva owner scope através de gerações independentes.
  Drain preflight deriva closure/replaceability de provider events e exige
  confirmação ligada a session/generation/closure/deadline. `:drain` consome o
  token sem repetir source. Falha pós-publication produz `degraded`.
- `var broken: i32 = "x"` conserva a generation anterior e registra receipt de
  erro. Falha runtime preserva effects externos já observados.
- Provider transactions recordam `attempted`, `committed`, `rolledBack` ou
  `unknown`; um generic external effect não recebe rollback inferido.
- Completion e inspect usam snapshot committed mesmo durante uma request staged
  ou draining; staged visibility não aparece.
- Uma resource/task persistent tem owner por binding, child da sessão, e não
  drena siblings sem dependência. Drain preflight pode rejeitar replacement
  antes de executing/effects. Falha após publish produz `degraded` sem rollback.
- Reset/restart cria nova incarnation. `force` aparece no histórico. Uma
  rejeição de reset no preflight preserva incarnation, generation, phase e
  scopes `owned`; não executa effects. Falha somente depois de publish cria
  `degraded`/`faulted`.
- Close bloqueado fecha admission e entra `closing`, mantendo o owner registry
  observável; force boundary registra scopes sem user cleanup.
- O history é bounded por count e bytes, reserva o receipt antes de effects e
  pode redigir raw source em memory. Requests de vários frontends usam tickets
  FIFO e um único writer serial; cancellation queued não ganha ordinal.
- `:receipts <path>` exporta manifest bounded/redacted. Heap, bindings vivos,
  tasks, resources, handles e capabilities não são restaurados.
- Output policy preserva bytes entregues: partial é `truncated`, item sem budget
  é `dropped`, e output de tamanho zero ainda consome a quota de count.

Adversariais:

- scan de cwd/PATH/environment, resolver/network e mudança de lock/capability;
- command contextual fora do primeiro token não espaço de uma entrada nova ou
  em buffer com source acumulado;
- parse/semantic error, append/complete/clear de buffer incompleto, stale base
  opaco e cross-generation take/inout/escaping ref/borrow/view;
- Copy staging automático (`counter += 1`), snapshot, adapter transaction,
  deferred no-fail e borrow lexical não escapante;
- foreign retention, known unreplaceable, confirmation ausente, quota e deadline
  no drain;
- cancel queued, cancel active antes de publish, cancel depois de publish,
  foreign non-cooperative e `:quit` com force boundary;
- output staged após falha, completion que tenta ler staged e history acima do
  limite; output reserve/truncation e cada família de quota.

O host deriva state, trace, graph fingerprint, receipt, invalidation, effects e
cleanup. Ele não compila, executa W, drena resource físico ou fornece CLI.
O field JSON `allowDrain` é a evidence normalizada de um token já validado; ele
não é uma opção booleana exposta à pessoa usuária.
Jupyter/rich output é PYN3. DLPack permanece adapter de tensor separado.

O fixture [`repl_session_oracle.w`](repl_session_oracle.w) também contém testes
puros separados para retenção de snapshot, predecessor de edge transitivo,
states de drain ready/degraded e cancellation antes/depois de publish. Esses
testes provam somente funções do contrato parseável; não alegam um REPL
executável.

### 3.1.3 Apresentação, Jupyter e export PYN3

Fixture: [`pyn3_oracle.w`](pyn3_oracle.w). Os três oracles host são
[`presentation-machine.mjs`](../../tooling/presentation-machine.mjs),
[`jupyter-machine.mjs`](../../tooling/jupyter-machine.mjs) e
[`notebook-export-machine.mjs`](../../tooling/notebook-export-machine.mjs).
Cada máquina tem corpus, checker, snapshot JSONL e teste host com o mesmo
prefixo. Nenhuma máquina executa W ou implementa um provider.

Aceite:

- `Presentable` escreve em writer opaco com media typed, limits, effect mask e
  `text/plain` obrigatório;
- o cardápio e a tabela usam preview bounded sem collect. O sensor do buraco
  negro e o tensor device expõem somente metadata. O tensor não é copiado;
- o fallback é determinístico e redacted. Cancellation, timeout ou falha de
  apresentação deixam um diagnostic bounded, texto compiler fallback e o
  submission outcome inalterado;
- Jupyter valida kernelspec 5.5 determinístico, cinco ports loopback user-only,
  HMAC antes de JSON, Curve Z85 em todos os sockets, replay, quotas, FIFO PYN2,
  heartbeat echo e lifecycle busy/reply/outputs/idle;
- `execution_count` é o counter corrente em todo reply; o reserved ordinal é
  separado e nullable, e `GenerationId` é opaque. Silent e user expressions
  são read-only. Stdin tem um waiter origin-routed e password não persiste;
- output rico é append-only. Progress cria itens novos; não existe display
  update/clear handle. History aceita somente tail bounded e redacted;
- completion, inspect, completeness e history usam snapshot committed com
  offsets Unicode code point. Metadata usa namespace `w`;
- export valida IDs nbformat, source String/Array, receipt manifest estruturado
  (session, generation, bindings, hard edges, providers, effects, inputs e
  resources), invalidation closure e ordem determinística;
- markdown só entra como companion explícito, raw segue policy, e o plan
  content-addressed deriva single-file/package sem `modules` ou `entries` da
  operação;
- export produz módulo normal, package ou audit manifest sem executar.
- `w notebook check`, `w notebook export` e `:receipts` recebem paths
  explícitos. Check/export não executam cells nem descobrem sessões ambientais.

Adversariais:

- media duplicada, media missing, active content, JSON fora do limite,
  collect/device copy implícitos, effect proibido e fallback que chama user code;
- HMAC inválido, replay, frames ou metadata acima da quota, Curve ausente,
  idle prematuro, silent mutation, user expression effectful, password em
  receipt, interrupt não admitido e shutdown antes de drain;
- feature advertisement falso, metadata sem namespace, read de staging ou
  byte offset, history range/search e live display mutation;
- cell ID inválido, receipt ausente ou divergente, cell invalidada,
  redefinition, effect unknown, stdin não resolvido, ciclo, replay oculto e
  export com execute explícito.

O fixture contém símbolos para `MenuPreview`, `TabularPreview`,
`BlackHoleSensor`, `DeviceTensorSummary`, `BistromathText`, `JupyterIdentity`,
`NotebookCell` e `ExportResult`. Esses símbolos são smoke tests de parsing e
referências do catálogo std. Eles não afirmam um kernel ou um provider
disponível.

### 3.2 Comanda de Íon

Famílias: String, parsing streaming, spans e typed errors.

Aceite:

- qualquer partição dos chunks produz a mesma AST;
- um frame acima de 64 KiB falha antes da alocação integral;
- erro informa byte range e scalar range;
- recovery encontra a próxima comanda;
- cancelamento libera o buffer parcial.

### 3.3 Hóspede das Órbitas Claras

Famílias: newtype, refinement, value parameter, conversão e tuple patterns.

Aceite:

- `GuestId` não é `OrderId`;
- `GuestCount` inválido em literal falha no compile time;
- input dinâmico usa `try GuestCount(value)`;
- refined-to-base é implícito;
- layout materializado continua o do base type.
- `switch (stage, guests)` combina dois valores sem syntax especial;
- cases usam ordem lexical e o compiler detecta um case inalcançável;
- `_` fecha a exhaustividade sem executar custom pattern handlers.

### 3.3.1 Letreiro das Três Contagens

Famílias: UTF-8, bytes, scalars, graphemes, normalização e texto nativo.

Aceite:

- `String` nunca contém UTF-8 inválido.
- Bytes, scalars e graphemes produzem contagens independentes.
- Índices de scalar e grapheme não escondem busca ordinal.
- Normalização não altera `==` sem uma call explícita.
- `String.fromUtf8` informa offset, tamanho e razão do primeiro erro.
- reparo usa uma U+FFFD por maximal subpart inválida.
- decoder incremental preserva até três bytes entre chunks.
- somente `finish()` classifica uma sequência parcial como incompleta.
- `String.viewFromUtf8` devolve `view String` e valida sem copiar.
- adoção consome o buffer e o devolve quando a validação falha.
- core UTF-8 preserva U+FEFF; somente um adapter nomeado consome a assinatura.
- uma edição pode usar índices do owner na última operação que os usa.
- um grapheme não implica limite finito de bytes.
- `CString` rejeita NUL interno.
- `Path` nativo converte para `Utf8Path` de forma fallible.
- `PackagePath` mantém uma forma portátil separada.

### 3.3.2 Janela de Serviço sem Dono

Famílias: `ref`, `view`, mutation exclusiva, lifetime, materialização e FFI.

Aceite:

- `ref Array<T>` observa o owner completo e pode ler `capacity`;
- `view Array<T>` observa somente elementos e `count`;
- criar ou copiar uma view read-only não aloca nem mantém o owner vivo;
- `inout view Array<T>` altera elementos e não altera o extent;
- append, resize e mudança de allocator não existem na interface da view;
- o owner não pode mover ou mudar a estrutura durante um borrow;
- `view String` sempre começa e termina em boundaries UTF-8 válidas;
- `inout view String` e `inout view CString` não existem;
- `materialize()` copia a projeção para um owner;
- `tryMaterialize(allocator:)` torna allocation failure recuperável;
- `await` exige que o owner permaneça estável no task frame;
- um child estruturado termina antes do owner; uma task detached não recebe a
  view;
- uma view contígua usa pointer e count somente dentro de um adapter FFI
  scoped;
- uma view de tensor mantém shape e strides e não promete contiguidade.

O oracle registra owner, base address, offset, count, mutability e provenance.
Ele executa o mesmo caso com Array, fixed array, Bytes, String e Tensor. A
criação da view não muda a contagem de allocations. O corpus compile-fail tenta
usar `capacity`, fazer append, devolver local storage, editar UTF-8 por bytes e
escapar a view por uma task detached.

### 3.3.3 Letreiro que Guarda as Últimas Palavras

Famílias: String flat, literal static, storage privado, refinement, reserva,
mutation, carrier e OOM.

Aceite:

- W0 funciona com literal/static e um buffer UTF-8 flat de owner único;
- criar `String()` vazio não aloca;
- `copy` de storage dinâmico cria outro owner durante a operação;
- COW não desloca allocation ou budget para uma mutation futura;
- SSO pode mudar por target sem mudar source, resultado ou ABI pública;
- a API não expõe capacity nem threshold de SSO;
- `HorizonSignLabel` limita o valor a 64 bytes sem criar outro tipo de storage;
- um guard prova que append preserva o refinement em cada saída;
- uma mutation sem postcondition suficiente falha no checker;
- `no-general-allocation` exige storage privado compatível ou rejeita o build;
- um layout físico usa carrier de boundary. O refinement não publica offsets;
- `tryReserve(minimumBytes:)` usa o total mínimo e mantém o valor na falha;
- append e replace não alocam quando a reserva comprovada basta;
- `clear()` mantém storage e `reset()` o libera;
- `takeAll()` transfere o conteúdo e deixa o receiver vazio;
- `String.adoptingUtf8` e `String.intoBytes` transferem o carrier sem allocation
  geral;
- Bytes preserva empty, static, inline opcional ou dynamic carrier sem virar
  alias público de String;
- uma source view do mesmo owner não entra numa mutation;
- reads não alocam nem atualizam uma cache lazy;
- um summary eager, como `isAscii`, muda somente durante mutation;
- raw pointer fica dentro de uma closure scoped;
- `address(of: string)` observa o descriptor, não o content pointer;
- CString ou Bytes pinned atende uma API que guarda o pointer;
- pinning do descriptor não permite uma mutation que realoca o conteúdo;
- `CString.bytes.count` exclui o NUL terminal e decode UTF-8 continua explícito;
- String com allocator explícito usa a mesma origem em todo growth e drop.

O oracle executa vazio, literal, limite de SSO menos um, limite, limite mais um e
payload grande. Como o threshold é interno, ele é descoberto pela instrumentação
e não pelo programa W. O ensaio injeta falha em cada allocation de copy, reserve,
append e replace. Ele compara conteúdo, owner, allocation count, budget, drop e
origem antes e depois. Os profiles flat e SSO precisam produzir o mesmo
resultado; somente measurements podem mudar.

### 3.4 Cozinha de Maré Fria

Famílias: units lineares, afins e customizadas, ranges e controle PID.

Aceite:

- `m`, `smoot`, `K`, `degC` e `clap` resolvem pelo import;
- `PhysicalDuration`, `Power` e `Energy` são aliases; a dimensão já fornece
  identidade;
- `30<si.s>` e `0.5<si.min>` têm o mesmo canonical value e o mesmo bit pattern;
- `180<degC>` é affine point, e point menos point produz `TemperatureDelta`;
- `64<iec.KiB>` guarda reference bits em `MemorySize`, e `exactValue(in: iec.byte)` produz
  bytes sem arredondamento;
- `Power * PhysicalDuration` produz Energy;
- `Duration` operacional não aceita conversão float implícita;
- point menos point produz delta;
- point mais point falha;
- JSON de domínio fixa `{ "value": 30, "unit": "s" }` para `tickDuration` e
  `{ "value": 12.5, "unit": "J" }` para `energyUsed`;
- JSON usa `{"value":"524288","unit":"bit"}` para `MemorySize` integer;
- `quantity_oracle.w` usa raw String para manter as aspas JSON visíveis, e
  `'λ'` continua um `UnicodeScalar`;
- token alternativo, como `"ms"` no schema de `tickDuration`, é rejeitado por
  igualdade exata;
- `switch` com range e `if` preserva a regra anti-windup;
- `clamp` prova que o `DutyCycle` refinado está em `0.0...1.0`;
- `{unit}` e `[unit]` aparecem somente no corpus comparativo;
- lowering sem reflection remove metadata de unit.

`quantity_oracle.w` registra esses casos. O caso JSON é um oracle de compile
surface e schema documentado. `std.json@1` e o codec wWire de produção continuam
missing. O arquivo não chama helper novo de writer ou decoder.

### 3.4.1 Arquivo das Filas Improváveis

Famílias: Array, fixed array, `view`, Map, Set, hashing, iteration e sort.

Aceite:

- `[0; 32]` avalia o valor uma vez e cria um fixed array;
- uma view impede mutation estrutural que poderia mover seu storage;
- lookup borrowed não copia String ou outro valor move-only;
- `inout` altera um value existente sem novo lookup;
- Map e Set iteram em ordem de inserção para qualquer hash seed;
- collision mantém full keys distintas;
- update preserva a posição e remove/reinsert move a key para o fim;
- `for ref`, `for inout`, `for copy` e `for ... in take` mostram ownership;
- stable sort preserva a sequência original de prioridades iguais;
- pipeline eager e `.lazy.collect()` permanecem formas distintas.

### 3.5 Brigada do Cometa Manso

Famílias: `async`, `spawn`, domains, ownership e cancelamento.

Aceite:

- stock e telemetria progridem concorrentemente;
- calls por `ServiceRef` não escolhem o domínio do callee;
- mistura local solicita paralelismo em `.compute`;
- o scope aguarda todos os filhos;
- join seleciona o error pela ordem lexical;
- cada lease executa cleanup uma vez;
- `let a = async asyncFunction()` e `let b = async ordinaryFunction()` criam o
  mesmo child lexical, e somente o facet de suspensão do callee difere;
- capture local em `spawn` falha quando não pode ser transferida ou compartilhada.

### 3.5.1 Passaporte da Brigada

Famílias: `transferable`, `shareable`, captures, affinity e contracts
genéricos.

Aceite:

- mover um owner para outro domain exige `transferable`;
- compartilhar `ref T` exige `shareable` e um lifetime estruturado;
- um owner mutável pode ser transferível sem ser compartilhável;
- imutabilidade profunda é suficiente, mas Atomic e ServiceRef também podem ser
  compartilháveis;
- `view T` depende do owner e nunca cria mobilidade por copiar seu descriptor;
- allocator, `deinit` e thread-local state participam da prova;
- raw pointer e foreign handle são locais sem um fato trusted;
- `T<(.transferable)>` e `T<(.shareable)>` fixam um requisito genérico;
- a forma omitida é inferida e registrada na interface;
- adicionar um requisito a uma API publicada quebra compatibilidade;
- mobilidade não implica serialization, remote transport, device transfer ou
  pinning;
- uma task detached não recebe borrow;
- nenhuma conformance nominal a `Send`, `Sync` ou `Sendable` existe.

O oracle gera products com um, dois e quatro worker threads, mas avalia domains,
não IDs de threads. Ele move um buffer único, compartilha um snapshot, atualiza
um atomic e chama uma ServiceRef. O resultado deve ser idêntico. Casos negativos
usam destructor affine, allocator local, raw pointer, object mutável sem
synchronization e uma view que escapa do scope.

### 3.5.2 Placar da Improbabilidade Residual

Famílias: data-race freedom, happens-before, atomics, CAS, locks scoped e
snapshots publicados.

Aceite:

- `var atomic value: T` baixa para `Atomic<T>`;
- read, assignment e compound update comuns usam `.sequential`; `let current = value`
  continua sendo uma load atômica;
- orders mais fracas usam contratos como `load<.acquire>()`;
- uma leitura tolerante a staleness usa `load<.relaxed>()`, mas continua
  atômica;
- load rejeita `.release`, e store rejeita `.acquire`;
- `+=` é uma read-modify-write checked;
- `value = value + 1` é rejeitado como load e store separados;
- `Bool`, integers e enums sem payload podem usar storage atomic;
- pointer, owner, float, struct e palavra dupla não entram em `Atomic<T>` safe;
- compare-exchange devolve `.exchanged` ou `.mismatch`;
- success/failure seguem a matriz estática de `MemoryOrder`;
- success de compare-exchange é RMW; failure é somente load;
- uma RMW contígua continua a release sequence;
- uma store posterior encerra a release sequence;
- weak compare-exchange permite falha espúria somente quando o nome informa;
- CAS não prova reclamation nem elimina ABA;
- `Atomic<T>` não promete lock-freedom;
- `lockFree: true` falha no build quando o target não oferece a garantia;
- `ref atomicValue` obtém `ref Atomic<T>`, nunca `ref T`;
- nenhuma leitura comum pode disputar os mesmos bytes com uma write atômica;
- state comum pode evitar atomics quando tickets de domain provam reads
  ordinários e writes de barreira;
- `withExclusive` exige `inout Atomic<T>` ou consumo;
- release/acquire não concede borrow nem prolonga lifetime;
- operações atômicas concorrentes usam endereço e extent idênticos;
- `atomic.fence` exige reads-from atômica e rejeita `.relaxed`;
- `lock owner as value` bloqueia somente em contexto blocking;
- `await lock` suspende na aquisição, nunca dentro do body;
- `try lock` não espera e não avalia o body quando devolve `.busy`;
- o body não suspende, não lança application error e não deixa borrow escapar;
- cancellation durante `await lock` não executa o body;
- cancellation depois do grant é observada somente depois do unlock;
- panic falha a fault boundary do lock em vez de publicar state parcial;
- admission de lock não promete FIFO ou fairness;
- domain concorrente com `.barrier` cobre o caminho task-owned;
- domain serial cobre state mutável task-owned sem async mutex;
- read/write lock e wrappers de mutex não entram na safe std;
- condition variable e `Once` raw não entram na safe std;
- `SnapshotCell.read` observa uma versão completa sem deixar borrow escapar;
- `publish` consome uma nova versão e não espera readers da versão anterior;
- `snapshot` cria um owner somente quando o payload atende a `Duplicable`;
- a versão substituída executa drop uma vez, depois do último reader;
- reference counting, epoch, hazard e lock preservam o mesmo trace lógico;
- state de um closed turn não recebe atomic ou lock sem outra razão;
- RCU safe genérico continua rejeitado; reclamation customizada fica em adapter
  `unsafe`.

E0 executa traces lógicos de publication, release sequence, fences e
compare-exchange. O gate de runtime repetirá litmus tests com uma, duas e quatro
threads. Esse gate também forçará o fallback não lock-free e executará TSan.

Failure injection cobre cancellation antes e depois da aquisição async. O trace
confirma que cada critical section libera o lock uma vez. Benchmarks separam
latency sem contenção, contenção na mesma cache line e counters particionados.

O corpus LM1 possui 39 casos e 86 operações: 21 aceitos, 17 rejeitados e uma
fault. Ele cobre as três formas da linguagem, busy sem avaliação, cancellation,
protected loans, fault boundary, drain e seleção entre owner, atomic, lock,
domain, snapshot, channel e service. Ele rejeita wrappers e read/write lock.
Onze testes host não executam W nem um provider runtime.

O corpus SP0 possui 27 casos e 82 operações: 14 aceitos, 12 rejeitados e uma
fault de allocation antes da publicação. Sete testes host cobrem versões,
publication order, error drain, retirement bounded e estratégias equivalentes.
Eles não implementam o provider `std.sync@1` nem executam W.

### 3.5.3 Passa-Pratos de Capacidade Finita

Famílias: stream pull, channel MPSC, ownership, backpressure e close.

Aceite:

- `Stream<Item, Failure>` possui um cursor single-pass;
- `.none` e o primeiro error terminam o stream;
- `Failure = Never` remove `try`, mas não remove `await`;
- `for try await` baixa para calls exclusivas a `next()`;
- `Stream<view String, E>` mantém a view vinculada ao stream;
- outro `next()` não ocorre enquanto uma view conflitante está viva;
- adapters devolvem `some Stream`, sem classes utilitárias públicas;
- `ReadableStream<Item, Failure>` é move-only e atende diretamente a `Stream`;
- `ReadableStream<Bytes, Failure>` atende a `ByteSource` para BYOB;
- `from` pode usar box, indirection ou storage inline para apagar o source;
- `maximum` em BYOB limita o delta anexado e não impede allocation de `Bytes`;
- HTTP não publica outro protocol `IncomingBody`;
- o carrier mantém um único pull em voo e não possui `getReader` paralelo;
- falha de `cancel` consome o owner, deixa o handle inert e não repete cleanup;
- tee genérico exige `Duplicable`; seu limite é lag em itens, não memória;
- zero itens é rejeitado para não transformar tee em rendezvous;
- tee de bytes usa limite exato e aplica backpressure ao ramo rápido;
- cancel de uma branch não cancela a outra; drop das duas cancela o source;
- `pumpReadableBytes` prova a direção para `ByteSink` sem `WritableStream`;
- `mirrorReadableBytes` mantém os dois pumps em children estruturados;
- `Channel<T><.send>` pode ser copiado somente com `copy`;
- `Channel<T><.receive>` é único e move-only;
- o channel aceita somente payload `transferable` owned;
- `view T` não entra na fila;
- capacity zero faz rendezvous;
- capacity positiva limita itens e permits aceitos;
- `send` suspende e `trySend` devolve `.full` sem ultrapassar waiters;
- `.full(T)` e `.closed(T)` devolvem o owner;
- destruir um permit libera a capacity;
- o último sender fecha o channel;
- `receiver.close()` rejeita admission nova e drena itens aceitos;
- destruir o receiver descarta o buffer e acorda producers;
- sends sequenciais por sender preservam ordem;
- sends concorrentes não ganham uma ordem global fictícia;
- commit de send acontece antes de receive devolver o item;
- collections usam regiões `pipeline<tasks: ...>` e streams usam adapters
  concorrentes ou paralelos
  bounded em vez de um receiver MPMC;
- `ReadableStream.tee` cobre duas branches estáticas;
- fan-out dinâmico pertence a um service que declara policy por subscriber;
- `SnapshotCell` mantém state corrente sem fingir que também é event history;
- quotas de bytes e trabalho pertencem à mailbox, não a um peso arbitrário do
  channel.

O source usa dois balcões de universos incompatíveis como producers. Um único
maître recebe os pedidos. CH0 repete o caso com capacity 0, 1 e 64. O gate de
runtime ainda precisa repetir o corpus com uma, duas e quatro worker threads.

Failure injection cancela cada send, receive e reserve antes e depois do commit.
Outro perfil fecha ou destrói o receiver com buffer e permits pendentes. Cada
pedido deve terminar em exatamente um destes destinos:

1. consumer;
2. error que devolve o owner;
3. cleanup registrado.

CH0 executa 47 sequências e 333 operações no host: 28 são aceitas e 19 são
rejeitadas. Doze testes independentes cobrem capacity 0, 1 e 64, FIFO,
`trySend`, permits, cancelamento nos dois lados do commit, close, abort,
happens-before e equivalência lógica entre ring e mutex. O oracle não executa W
e não implementa scheduler, runtime ou provider.

O teste de view percorre linhas borrowed do cardápio sem allocation. O corpus
rejeita guardar uma linha depois do próximo `next()`, enviá-la por channel ou
movê-la para task detached.

Enquanto `std.readable-stream@1` estiver missing, o oracle readable fixa source
e compile-fail comments, mas não alega execução. O gate futuro usa destinations
com velocidades diferentes. O lag fica em `maximumBufferedBytes`. O ramo rápido
aguarda no limite. Um sink que falha devolve seu outcome e destrói somente a
própria branch. O outro pump continua, e o scope aguarda os dois antes de
devolver. Fault injection também faz `cancel` falhar antes e depois da
solicitação física e confirma owner consumido, handle inert, drain e cleanup
único. Um caso genérico varia o tamanho dos grafos com o mesmo lag para provar
que item count não promete byte bound.

`abort.w` fixa o adapter Web sem substituir cancellation de task. O timeout usa
clock monotônico operacional e mantém um timer-resource independente do
creator/root. `AbortSignal.any` achata e deduplica folhas pending antes de
aplicar o segundo limite, depois de limitar os argumentos diretos. As duas
validações precedem o winner lexical e qualquer registration. O controller é
move-only, seu drop não aborta e o signal não concede authority. O oracle
positivo usa `throwIfAborted`, recebe o reason de `wait`, passa `Request.signal`
para outro fetch e cria um controller apenas para uma lifetime independente.

Enquanto `std.abort-state@1` estiver missing, os casos de runtime permanecem
provider-gated. O harness futuro intercala abort, wait, task cancellation,
drop, timeout e `any` em todos os pontos de commit. Ele exige timeout
independente do creator/root; overflow de argumentos diretos, inclusive signals
abortados, sem registration; overflow de folhas pending aninhadas sem
registration; folha pending duplicada com uma registration; reason publicado,
nenhum wake perdido, cleanup único, zero refcount cycle, zero task órfã e
nenhuma fila de events.

`json.w` fixa o carrier de host de JSON. A conformance é explícita e a synthesis
reconhece somente o protocol `std.json` por identidade. `Writer` e `Reader`
mantêm object/array cursors dentro de closures scoped. `Limits` cobre bytes,
depth, values, strings, number tokens, members e allocation; não existe rota
unlimited; `Limits(maximumBytes:)` usa defaults finitos e fixos. O profile
interoperable aplica I-JSON/Web e o profile RFC 8259 preserva números decimais
exatos dentro do target. `Number` é nominal e validado; `Value` é o sum type,
não `Any`; duplicates, Unicode inválido, nonfinite, trailing comma e comments
falham. Object equality ignora ordem, mas re-encode preserva insertion order.
`ValueKind` e `ValueConstraint` distinguem type mismatch, enum/refinement e
canonical form. `http_documents.w` e `wifi_documents.w` usam adapters
direcionais. Eles validam documents em domain values e codificam responses por
borrow. Os testes são targets de source e oracles provider-gated. O provider
`std.json@1` continua missing e não há alegação de execução.

### 3.5.4 Arquivo Posicional das Receitas Extintas

Famílias: byte I/O, EOF, progress parcial, cancellation, rights e buffers.

Aceite:

- `ByteSource` acrescenta somente bytes confirmados a `Bytes`;
- `.data(count)` possui `count > 0`, e `.end` é terminal;
- `ByteSink.write` informa `.complete` ou um prefixo positivo;
- `writeMany` preserva a ordem e informa o prefixo da concatenação lógica;
- o fallback de `writeMany` usa um `write`, sem concatenação ou allocation;
- `writeAll` informa o prefixo committed quando falha;
- o relay devolve o owner do chunk quando o source avançou e o sink falhou;
- cancellation não libera um buffer antes da completion final;
- a reserva não inicializada de `Bytes` não aparece no source safe;
- `view Bytes` empresta um chunk sem criar `BytesView`;
- a view não entra em channel nem sobrevive à próxima reutilização do buffer;
- `File<[.read]>` não oferece write;
- `read(at:)` não altera um cursor compartilhado;
- o cursor sequencial é um `some ByteSource<IoError>`, não `FileReader`;
- I/O blocking usa um adapter e uma quota explícitos;
- readiness, completion e fallback blocking produzem o mesmo trace semântico;
- `ReadBatch` faz scatter read sem expor memória não inicializada;
- `TransferPlan` preserva intervalo, progresso e scratch no file-to-sink;
- native scatter/transfer e fallback produzem o mesmo trace sem promessa de
  zero-copy.

O oracle divide o Arquivo das Receitas Extintas em todos os pontos possíveis.
Cada execução injeta short read, short write, EOF junto com dados, error depois
de progress e cancellation nos dois lados da completion. O payload final, o
prefixo committed e o número de cleanups devem ser iguais em `io_uring`, IOCP,
readiness e executor blocking.

O teste gather usa zero segments, segments vazios e mais segments que o limite
do host. Ele injeta progress dentro e entre segments. O payload final deve ser
idêntico com fallback, `writev` e `WSASend`.

O teste scatter usa segments de capacidades diferentes, batch cheio, EOF no
meio do batch e progress no último segment parcial. Views observam somente os
prefixos inicializados. O trace deve ser idêntico com fallback, `readv` e
`WSARecv`.

O teste de transferência divide um `FileSnapshot` antes, dentro e depois de
cada chunk, injeta source end, limit reached, short write, error e cancellation.
O plan deve conservar o sufixo não committed. Fallback e operação nativa devem
confirmar os mesmos bytes; a estratégia física aparece somente em explanation.

O teste posicional lê blocos sobrepostos com um `shared File`. A ordem de
completion pode mudar. Cada bloco deve manter o offset solicitado. O cursor
sequencial continua único e rejeita duas leituras concorrentes.

#### 3.5.4.1 Error portátil de I/O

`io_error_oracle.w` usa `IoErrorKind`, `IoOperation`, progress, idempotência e
deadline para uma policy específica do Arquivo das Receitas Extintas. Ele não
adiciona `retryable` ao error comum.

Aceite:

- kind portátil e operação lógica são eixos independentes;
- syscall auxiliar não substitui a operação observada pelo caller;
- cause é um snapshot opaco, redigido e bounded, sem resource owner;
- duplicar o error não duplica request, handle ou authority;
- `wouldBlock` suspende; interrupção sem progress repete;
- EOF usa `ReadStep.end`; cancellation usa `TaskOutcome.canceled`;
- timeout do adapter pode ser `.timedOut`; deadline da task não pode;
- unknown kind ou operation usa `.other`;
- retry depende também de progress, idempotência e deadline.

IOE0 é um oracle host de design. Ele não executa syscalls, scheduler, provider
ou código W.

### 3.5.5 Arquivo das Receitas Extintas

Famílias: authority de filesystem, paths nativos, rights, namespace e
durability.

`fs_oracle.w` usa somente o `FileSystem` concedido ao entry root. O source abre
handles com rights estáticos, captura snapshots bounded, escreve um arquivo de
staging com offsets posicionais e publica por rename. A confirmação durable é
uma sequência explícita: `finish(.data)`, rename e `syncNamespace(at:)`.

Aceite:

- nenhum cwd, path absoluto, `PATH` ou filesystem global concede authority;
- `scope(at:)` deriva uma raiz menor sem ampliar rights;
- o provider prova containment sob limites finitos, inclusive ao seguir
  symlinks;
- `Path` preserva a representação nativa e `Utf8Path` faz conversão fallible;
- display lossy nunca participa de lookup ou identity;
- rights inválidos falham antes de chamar o provider;
- append seleciona o offset dentro da operação do provider;
- shared positional I/O não insere lock; overlap sem ordering fica visível;
- cursor sequencial consome um owner e nunca aparece em `shared File`;
- snapshot valida limite e versão antes de publicar bytes;
- adquirir a listagem pode suspender; o stream é single-pass, bounded e mantém
  a ordem do provider;
- limit ou I/O error termina o stream depois do prefixo já entregue;
- rename no-replace é o default; replace regular é explícito e não cruza mounts;
- namespace atomic não implica durable;
- falha de namespace sync informa que o novo nome já foi publicado;
- `sync(.none)` e drop não inserem uma solicitação de persistência;
- cancellation mantém buffers e borrows até o provider drenar a completion.

O provider `std.fs@1` continua missing. FS0 é um oracle de design e não executa
syscalls reais. Um staging file órfão exige reconciliation separada; uma
`unknownOutcome` de rename nunca autoriza retry ou delete cego.

### 3.6 Salão Prisma

Famílias: service, serial turn, mailbox, hop e backpressure.

Aceite:

- `export service lastLight: RestaurantApi { ... }` contém boundary e provider;
- `import service { PantryApi as pantry }` adapta uma caller-owned boundary;
- o startup resolve todos os slots antes do entry;
- launch config só troca uma binding autorizada pelo artifact;
- a mesma service call funciona local e remotamente;
- toda call usa `await`;
- o trace mostra hop e queue wait;
- mailbox limita itens, bytes e trabalho em voo;
- mailbox cheia aguarda ou retorna `ServiceFailure.overload`;
- error da aplicação e `ServiceFailure` são effects distintos;
- service payload usa value, `take` ou capability, nunca borrow do caller;
- `PlanningRequest` e `PaymentProof` tornam essa cópia explícita;
- call A → B → A conhecida retorna `callCycle`;
- closed turn impede reentrância durante `await`;
- `cancel(orderId)` enfileirado não interrompe um turn ativo;
- a instance `.process` do ensaio expõe head-of-line blocking de propósito;
- instances keyed permitem progresso paralelo entre pedidos, mas não na mesma key;
- `ServiceLink` separa local, component, wRPC e foreign RPC;
- `ServiceTransport` aparece somente dentro do link wRPC;
- resolver de service aceita somente import nominal e binding tipado;
- adapters pertencem ao toolchain, deployment ou product fixado no lock;
- network wRPC usa TLS 1.3 mutual ou QUIC com TLS 1.3 mutual;
- IPC autentica os dois peers e não trata o path como identidade;
- `hello` e `ready` vinculam seleção, peer e channel no transcript;
- 0-RTT e application frame antes dos dois `ready` falham fechados;
- `pipeline` reduz round trips sem ocultar calls, effects ou intermediate owners;
- o turn continua fechado durante output commit;
- um commit provider fecha uma frontier bounded com um terminal receipt;
- `commitFailed` e `unknownOutcome` permanecem outcomes distintos.

O oracle executa o mesmo graph em `single-process` e `split-services`. O primeiro
usa local links. O segundo usa wRPC entre gateway, planning, finance e dining.
Ambos precisam produzir os mesmos application values, errors e causal trace.
Queue wait, copied bytes e transport spans podem mudar.

`service_oracle.w` também verifica a camada de cada link. Local usa mailbox e
thunk, component usa component ABI, wRPC usa session/codec/transport e foreign
usa adapter próprio. Nenhum link local ou component introduz `ServiceTransport`.

O caso de promise pipelining usa `prepareDish`:

```text
ovens.acquire(target, duration)
  → capability OvenLeaseApi
  → preheat()
```

O caller envia `preheat()` antes de receber a capability de `acquire()`. O
pipeline também devolve a capability, pois `bake()` e `close()` ainda precisam
dela. Uma falha antes da entrega deve liberar a capability intermediária. Uma
call para uma instance presente na ancestry deve falhar com `callCycle`.

A forma source está no `prepareDish`:

```w
let (lease, ready) = try await pipeline {
  let lease = ovens.acquire(schedule.recipe.target, duration: schedule.duration)
  let ready = lease.preheat()
  commit (lease, ready)
}
```

O bloco aceita somente um DAG estático de calls e projections. Ele não executa
uma closure remota. O `commit` seleciona os valores que voltam ao caller. Uma
ilha na mesma session pode usar um round trip; outra route cria uma barreira
sem mudar a semântica.

O oracle cobre chain, diamond, fan-out, forward reference, branch runtime,
`await` interno, borrow e pipeline sem dependência. O último recebe warning em
favor de um initializer `async`. Se qualquer node possui outcome incerto, o resultado é
`pipelineUnknown` com todos os effect IDs incertos. Um error da aplicação não
pode esconder uma mutation que talvez tenha ocorrido.

`session_security_oracle.w` usa a edge entre observatory e satellite control.
O deployment lock espera a identity de `satellites/controller`. Uma credencial
válida para outra unit falha antes do `hello`.

Os dois peers vinculam seus nonces, ofertas, seleção e channel binding no
transcript. O oracle remove uma feature, troca o binding e repete o session ID.
Cada caso falha antes de criar tables. Ele também rejeita 0-RTT, sequence
duplicada, gap e capability da session anterior.

`capability_security_oracle.w` separa peer identity de authority. Uma binding ou
um campo `ServiceRef` explícito cria grant. Identity, index, URL e bytes não
criam. `OvenLeaseApi` conforma com `OvenObserverApi`; uma typed binding menor
delega observação sem delegar `bake` ou `close`.

O oracle também injeta revoke antes e depois de admission. A primeira call
falha como unauthorized. A segunda drena sem rollback. Restart pode resolver um
root de binding, mas invalida a capability derivada do oven lease.

`release_oracle.w` separa assinatura, digest, reprodução, transparency e
revogação. A reprodução compara a evidência completa de inputs e outputs; ela
não aceita bytes iguais produzidos por recipes diferentes. O deployment
distribuído exige maintainer authorization, reprodução e metadata fresca. Um
mirror com bytes corretos, mas metadata antiga, continua rejeitado.
O quorum também exige `builderIdentity`, `operatorIdentity`,
`credentialIdentity` e `executionRootIdentity` distintos. Dois jobs na mesma CI
não formam independência. O oracle também liga recipe, toolchain, artifact e
platform envelope. Uma assinatura de platform não prova a segurança do source.

`metadata_oracle.w` mantém três fronteiras: CBOR determinístico para records de
build e distribuição, `WMeta1` para interface e ABI públicas e wWire para
payloads de service. Cache AST/HIR continua interno ao toolchain. A recipe
aceita somente inputs declarados; output digest é evidence posterior e path do
executor ou wall clock são proibidos. `wmeta_oracle.w` fixa as provas locais do
container físico sem se tornar um reader de produção.

`transaction_oracle.w` usa a mesma expressão com um `ServiceRef`:

```w
let receipt = try await pipeline<transaction: {
  isolation: .serializable,
  access: .readWrite,
}> tx = tableLedger {
  let reservation = try await tx.reserve(tableId: tableId, guestId: guestId)
  let receipt = try await tx.confirm(reservation: take reservation)
  commit receipt
}
```

O provider pode usar um link local ou wRPC. Somente calls derivadas de `tx`
pertencem ao commit. Perda da confirmação continua `unknownCommit`. Uma segunda
service independente exige workflow e compensação.

`benchmark_app.w` usa a mesma expressão com `std.database.Database`: a leitura
de vários mundos continua uma operação independente, e a mutation entra em um
único scope do `store`. Se o adapter publicar defaults para o contract, o
mesmo caso pode omitir `<isolation, access>`, mas nunca pode omitir o provider,
o body ou o `commit`. `try await transaction;` fica rejeitado porque não
identifica esses três elementos.

`OvenReady` é um token owned do provider. Ele substitui o antigo `Instant`
local. `bake()` consome esse token. Assim, o caller não interpreta nem compara o
clock monotônico do forno remoto.

O caso de output gate usa uma futura variante durável de `billing.capture`:

```text
gateway capture
  → idempotency record accepted
  → response staged
  → durable confirmation
  → response released
```

O `billing` atual usa um `Map` volátil. Ele não atende esse oracle e não anuncia
output gate. A variante durável deverá injetar falha antes, durante e depois da
confirmação. Falha confirmada produz `commitFailed`. Confirmação perdida produz
`unknownOutcome`. Nenhum caso repete a captura sem a mesma idempotency key.

O caso de stream usa telemetria de satélite. `SatelliteApi` mantém a call unary
e acrescenta uma edge contínua:

```w
export protocol SatelliteApi {
  async fn telemetry(after sequence: u64): SatelliteTelemetry throws SatelliteError
  async fn follow(
    after sequence: u64,
  ): some Stream<SatelliteTelemetry, SatelliteError>
}
```

`SatelliteError` possui o case único `service(ServiceFailure)`. Assim, um
terminal da aplicação e um disconnect continuam no mesmo `for try await` sem
perder a causa. `Stream<..., Never>`, `view` e `any Stream` são inválidos nessa
boundary.

`collectTelemetry` aplica `buffer(capacity: 8)` de forma explícita e para em um
limite refinado. O `break` envia reset e drena o producer. `build.w` limita
streams abertos, item bytes, items e bytes em voo, fila decoded, traversal,
capability slots e taxa. Limites `perStream` e `total` impedem multiplicação do
envelope por streams concorrentes.

O `remote_stream_oracle.w` verifica os dois créditos absolutos. O sender só
avança quando possui item e bytes. Uma redução de crédito ou envio acima do
grant produz protocol failure. Mesma route usa ligação direta. Outra route usa
relay bounded e nunca materializa o feed inteiro.

O corpus de evolução combina dois artifacts:

- N+1 adiciona um field optional a `SatelliteTelemetry`;
- N+1 adiciona um case possível a `SatelliteHealth`;
- N+1 renomeia `telemetry` com ID preservado no `interface.lock`;
- N e N+1 usam wWire `exact` somente com o mesmo `WireSchemaDigest` da raiz;
- peers compatíveis usam wWire `compatible`;
- um enum output com case novo falha na negociação, salvo quando o operation
  subset exclui esse case.

Cada cenário injeta disconnect, cancellation, overload e schema mismatch em
cada commit point. O oracle compara owner cleanup, capability count, effect ID,
application outcome, boundary outcome e trace ancestry.

`wire_oracle.w` acrescenta estes casos:

- o registro core v0 mantém IDs `1–25` sem depender da ordem do enum source;
- quatro seed vectors fixam `MenuKey` em `exact` e `compatible`;
- `SatelliteTelemetry` fixa tensor, unit, String, enum e integer no mesmo valor;
- x86-64, Arm64 e um decoder independente precisam produzir os mesmos bytes;
- `RestaurantSnapshot.activeOrders` usa `u32`, não `usize` target-dependent;
- `OvenReady` usa token owned, não `Instant` local;
- `WorkSnapshot` usa duração restante e mantém o alarm dentro do adapter;
- refinement `u16<(1...128)>` reduz para um byte no wire schema;
- um `ServiceRef` usa capability ordinal e não pointer ou endpoint global;
- Map e Set preservam insertion order;
- ordinary decode descarta unknown field;
- relay explícito preserva um unknown block canônico.

O corpus malformado cobre:

- control integer não mínimo;
- field ID duplicado ou fora de ordem;
- Bool inválido e UTF-8 inválido;
- enum case fora do subset;
- unused presence bit;
- block truncado e trailing data;
- count overflow e item de tamanho zero com count extremo;
- nesting, traversal e allocation acima do budget;
- capability ordinal sem table entry.

O gate compara `exact`, `compatible`, JSON e os adapters de referência. Nenhum
resultado de performance será publicado antes de existir encoder, decoder,
oracle diferencial e fuzzer.

### 3.7 Conta da Aurora Tardia

Famílias: Money, errors, idempotência, property behavior, existential e opaque
return.

Aceite:

- Currency diferente exige conversion explícita;
- rounding policy é parte da operação;
- overflow não usa binary float;
- `WrappedDegrees` normaliza a atribuição e a mutação composta no accessor;
- `WrappedDegrees.modify` retoma seu hook depois do borrow, e uma rotação de
  `350` graus por `25` graus produz `15`;
- um behavior aceita somente o thunk `initialValue`, sem configuração runtime;
- `priceTable` usa lowering isolado no service serial e não cria lock;
- um owner concorrente seleciona um winner e publica um valor completo;
- um contender em domain non-blocking falha sem prova de isolamento;
- reentrada dinâmica falha a fault boundary e não produz deadlock;
- cancellation só fica observável depois da publicação e do cleanup;
- assignment exclusiva antes do primeiro read descarta o initializer;
- captures ou valor executam drop uma vez conforme o estado final;
- `some PricingPolicy` converte para `any PricingPolicy` sem perder o valor;
- falha após captura executa um refund idempotente uma vez;
- retry mutante só ocorre com idempotency key.

### 3.8 Oráculo de Mesas

Famílias: value generics, shape, tensor, `@` e numeric mode.

Aceite:

- shape compatível compila;
- shape estático inválido falha no type checker;
- shape dinâmico inválido retorna error;
- broadcast diferente de scalar é explícito;
- `tensor[i, j]` acessa um elemento sem uma view intermediária;
- view não copia;
- device transfer aparece no source/trace.
- refinements de elemento podem estreitar loads, multiplies e accumulators;
- float `@` não ativa reassociation ou FMA sem um mode explícito.

### 3.9 Sonda de Aroma

Famílias: `foreign c`, `fn<C>`, unsafe, layout, pointer e cleanup.

Aceite:

- size/alignment confere com C no target;
- status e null viram errors tipados;
- NaN não atravessa a wrapper sem policy;
- metadata informa blocking, thread safety e callback executor;
- call blocking usa adapter ou uma isolation boundary dedicada;
- o CST W mantém um único `foreign_body_content` opaco;
- strings, characters, comments, nesting, CRLF e UTF-8 não encerram ou
  normalizam o range por acidente;
- o body scanner C encontra o fechamento sem interpretar statements como W;
- directive de preprocessor não entra no profile C inline; source maior usa
  uma foreign unit;
- fallback editorial sem adapter nunca vira interface, object ou recipe;
- adapter, scanner e body digest ficam ligados ao recipe;
- diagnostic do adapter usa offsets validados no arquivo W;
- o adapter C gera façade C e static archive reproduzível;
- funções do mesmo adapter compartilham uma foreign unit;
- o deallocator original executa uma vez;
- panic não faz unwind através de C.

FB0 possui 45 casos e 90 operações: 15 aceitos, 28 rejeitados e duas
informações. Nove testes host derivam scanner, limits, source map e recipe sem
ler o snapshot. Eles não executam C nem implementam o adapter.

#### 3.9.1 Escapes de sistema

Famílias: MMIO, interrupt, task-local, TLS, linker placement e assembly.

Aceite:

- `DeviceContext` cunha register com width, access mode e side effects fixos;
- volatile não cria atomicidade, synchronization ou uma view comum dos bytes;
- o interrupt handler satisfaz o effect envelope do host slot;
- task-local drena children antes de restaurar o binding anterior;
- child captura o binding task-local vigente na criação, sem copy ou retain;
- service, wire, device, callback e novo entry observam somente o default;
- TLS safe aceita somente `Copy` sem drop e closure `neverSuspend`;
- uma task que migra entre threads observa slots TLS distintos;
- `retain` é verificado no symbol manifest do payload final;
- `fn<Asm>` declara target, clobbers, memory, stack, unwind e volatility;
- nenhum raw interrupt frame, TLS borrow ou assembly unwind escapa da boundary.

CTX0 contém 25 casos e 94 operações: dez casos aceitos, 15 rejeitados e seis
testes host. Ele deriva descriptor nominal, rebind LIFO, snapshot por child,
drain, boundaries, slots por thread, mutation síncrona, migration e
availability. O oracle não executa W, scheduler, TLS nativo ou provider
runtime.

### 3.10 Despensa Selada

Famílias: package, lock, digest, mirror e capability.

Aceite:

- lock fixa source externo; recipe fixa tool artifact e demais inputs;
- mirror diferente entrega os mesmos bytes;
- bytes diferentes falham antes do build;
- tool target sem network não consegue abrir network;
- SBOM distingue build tool de runtime dependency.

### 3.11 Reserva em Fragmentos

Famílias: parser, budgets, streams e resource lens.

Aceite:

- o limite do buffer é verificado antes da expansão;
- estimates mostram intervalo e confiança;
- medição runtime não vira garantia global;
- cancellation e deadline não vazam chunks.

O caso principal usa `allocation.w`:

```text
$ w explain resources restaurant.allocation::stageMenu \
    --target x86_64-unknown-linux-gnu --profile debug
reachability: reachable
code:       1.2..1.8 KiB       estimate
staticData: 0 B                 fact
instance:   0..96 B             estimate
operation:  0..2 MiB staging   contract
peak:       unknown             input-dependent
accounting: payload + allocator
```

O limite `2<iec.MiB>` do plan `.fixed` é um contract de admission. Ele não prova o peak
total de `stageMenu`, porque o input e a cópia final podem usar outro storage.
`countEmergencyTokens` mantém um buffer fixo de `64<iec.KiB>`, mas o lens continua
separando storage local, allocator scope e payload produzido. Uma medição posterior só é
aceita no cache quando recipe, `WAbiKey`, target e profile são iguais.

### 3.12 Turno do Horizonte Violeta

Famílias: todas.

Aceite:

- `last-light-native` seleciona o descriptor `.default`;
- CLI e HTTP chegam ao mesmo service;
- parser, units, tensor, C, billing e resposta ficam alcançáveis;
- shared graph, pinned callback e W0 compiler ficam alcançáveis;
- build `--locked` produz o mesmo payload;
- trace explica task tree, service hops, allocations e resultado;
- toda falha preparada termina com owners e scopes fechados.

### 3.13 Contrato estático fechado

Famílias: generics, refinements, units, domínio de execução e frontend inline.

Aceite:

- `BoundedText<{min: 1, max: 120}>` passa um static record tipado;
- `u16<(1...4096)>` expande para `value in 1...4096`;
- `String<(.scalars.count <= 40)>` usa o subject contextual do refinement;
- `String<(value.scalars.count <= 40)>` explicita o mesmo subject e a mesma ConstIR;
- `Array<u8><(.count <= 64)>` refina um generic já aplicado;
- `Array<[u8, (.count <= 64)]>` não substitui os dois contratos;
- `spawn<.compute>` e `spawn<domain: .compute>` aceitam o mesmo slot opcional e normalizam para o mesmo HIR;
- `fn<C>` e `fn<lang: .c>` selecionam o mesmo frontend hermético pelo mesmo slot opcional;
- `<(...)>`, `<{...}>` e `<[...]>` preservam expression, record e list;
- um slot repetido produz diagnostic antes do type-check normal;
- um case abreviado usa o slot posicional com label opcional declarado pelo schema;
- um case ambíguo nunca escolhe um slot por ordem;
- deadline e executor runtime não entram no contrato angular.
- `Money.zeroCredits` e `Course.fromOrdinal(...)` não criam estado global;
- `OrderState.advance(...): self` retorna um reborrow explícito.

As formas `where` e `on` permanecem no corpus contrafactual. A análise normativa
está na [seção 3 de DESIGN.md](../../DESIGN.md#3-contratos-estáticos-e-orçamento-de-símbolos).

### 3.14 Arquivo de Ecos

Famílias: owner único, shared, weak, borrow suspenso, provenance e pinning.

Aceite:

- children mantêm descendants vivos e o parent fraco não forma ciclo forte;
- uma closure escapante escolhe `take`, `copy` ou `weak` para owners move-first;
- `<[weak hub]>` permite callback armazenado sem reter o próprio hub;
- ciclo forte fechado e destruction-dependent falha com caminho causal;
- ciclo dinâmico que nenhum root externo alcança só é reportado depois do drain
  e nunca é coletado;
- a leitura contextual retorna `some` quando adquire antes do strong release final
  e `none` depois do último shared owner;
- a leitura contextual de weak e o último release possuem uma ordem linearizável;
- o value morre no strong zero e o control block no weak zero;
- um borrow fica ligado ao strong handle que o criou; outro alias pode morrer;
- shared handle não concede `inout` e weak exige binding opcional antes do acesso;
- `OriginSet` de borrow e `AllocationOriginSet` de storage não se substituem;
- `let root: shared T = temporary` mostra o primeiro owner no binding;
- `shared T` é o tipo final; ele não é escrito como `Shared<T>` ou
  `shared<allocator: ...> T`;
- `atomic` modifica storage e baixa para `Atomic<T>`; ele não forma `atomic T`;
- `shared T?` é um handle opcional; `shared Option<T>` possui payload opcional;
- expression e return criam binding local `shared` e movem com `take`;
- trocar a origem do control block não altera o tipo `shared T`;
- argumento, return e inference não promovem owner único para `shared T`;
- payload dependent falha até o storage ser lifetime-independent;
- o allocator do control block continua vivo até o último weak handle;
- mover ownership por `spawn` exige `transferable`;
- enviar um `ref` estruturado por `spawn` exige payload `shareable` e lifetime
  dentro do task scope;
- enviar `shared T` por `spawn` exige `T` shareable, contador thread-safe e
  origins de storage cross-domain;
- um borrow após `await` só compila com owner e task frame estáveis;
- mover ou substituir o owner durante esse borrow falha;
- `Pinned<T>` pode mudar de endereço sem mover o `T`;
- `try pin take state` separa allocation fallible do move;
- `try pin BellState(...)` inicializa no destination estável sem temporary
  completo;
- falha de argumento, allocation ou initializer não publica endereço;
- self-reference safe não obtém acesso ao `self` parcial;
- falha de construção `shared` declarativa, `pin` ou `rehome` consome e limpa o
  source uma vez; `share` permanece somente operação conceitual interna;
- não existe `unpin` irrestrito depois que o endereço é publicado;
- a lease mantém o bell e o callback state vivos até unsubscribe;
- unsubscribe ocorre antes de liberar o callback state;
- converter pointer em address não permite reconstruir um pointer;
- profiles portátil e otimizado produzem o mesmo resultado.

### 3.15 Cardápio de Fótons

Famílias: bootstrap, lexer, parser, AST, collections, ownership e output
determinístico.

Aceite:

- `packages/menu-compiler/compiler.w` usa somente o profile `bootstrap.w0`;
- `packages/menu-compiler/transform.w` recebe somente `menu` e `bytecode`;
- o tool artifact é compilado para o target da execution platform;
- a action executa nessa platform, não no product target;
- nenhum path, environment, clock, random ou network fica disponível;
- o build transform usa somente overloads fechados `String`/`Bytes`, com UTF-8 estrito e
  Bytes identity;
- ceilings menores do provider ficam no host profile ou toolchain plan e entram
  na action recipe key;
- error ou output acima de 1 MiB não publica o action-result;
- panic, cancellation ou output ausente também não publicam o action-result;
- blobs content-addressed sem record podem ser coletados por GC;
- `const fn buildInstructionOpcodes` executa no evaluator CE0;
- a mesma `const fn` continua callable em runtime;
- o ConstValue de Map preserva pares em ordem de inserção e não preserva hash;
- lookup no const Map não exige materializar um Map mutable;
- o profile inclui tudo que o source do compiler pequeno usa;
- retirar qualquer capacidade W0 produz um diagnostic ligado ao fechamento;
- seed-C e W/MLIR emitem o mesmo bytecode e symbol table;
- duas compilações com a mesma recipe produzem os mesmos bytes;
- a ordem de iteração do Map não influencia a symbol table emitida;
- o source não depende de task, service, tensor, unit, behavior ou tagging;
- uma instruction depois de `serve` falha antes da emissão;
- o seed preserva typed errors, move e drop;
- quota, target e evaluator version entram na recipe e no cache;
- clock, environment, FFI e filesystem não entram no evaluator;
- um panic durante const evaluation produz `W-CONST`, não uma fault boundary;
- `bootstrap_oracle.w` rejeita dependência de `compiler/extended` no core e
  diferencia drift de target metadata;
- `lifecycle_oracle.w` rejeita join antes de cleanup, commit fora de ordem e
  confirmação depois de `unknownOutcome`;
- `scheduler_oracle.w` permite mudar o packing físico somente quando o logical
  trace, outcome e ownership permanecem iguais; worker e transport events ficam
  no sidecar físico;
- o ensaio cresce pelos gates SH0–SH7 antes de declarar self-host completo.

Cobertura atual:

| Gate | Estado do ensaio |
|---|---|
| SH0–SH1 | parcial: lexer, parser da DSL e tabela CE0, ainda não do source W |
| SH2–SH3 | parcial: AST, symbols, errors, collections, move e drop |
| SH4 | parcial: bytecode e symbol table determinísticos, ainda sem HIR W |
| SH5–SH7 | não implementados |

### 3.16 Observatório do Cometa Paciente

Famílias: pipeline tasks, backpressure, ordering, cancellation e atomic metrics.

Aceite:

- `pipeline<tasks: .parallel<.compute>, limit: ..., ordering: ..., errors: ...>`
  mantém no máximo `limit` children vivos;
- a admissão mantém no máximo `limit` items staged ou em children ativos;
- `.input` devolve resultados na ordem dos jobs;
- `errors: .collect` preserva um settlement por job;
- cancelar o batch fecha producer e children;
- `task#cancel(reason: .shutdown)` preserva o handle para o join;
- `Task#spawn(domain: lane.reference, input: take job, using: work)` é o
  launcher facet para um `ExecutionDomainRef` dinâmico; `spawn<domain>` é o
  initializer estático para domains conhecidos;
- `Task#withDeadline(until: deadline, input: take job, using: work)` devolve
  `TaskOutcome` e drena o child após expiration;
- `cancel` não existe como statement ou keyword;
- cada job move ownership para um child;
- `shared BrigadeMetrics` cruza a boundary porque usa storage atomic;
- um pointer C ou state mutável de service não pode ocupar o mesmo lugar;
- `TaskOutcome` distingue success, application error e cancellation;
- pipeline tasks `.collect` preserva o índice do input em cada `TaskSettlement`,
  inclusive quando a ordem é `.completion`;
- pipeline tasks é a região repetida para uma coleção finita; `pipeline` dependent
  continua o DAG estático de calls, mesmo quando o HIR/runtime compartilha machinery;
- `Task#firstSettled` consome handles existentes e publica somente após o drain
  dos losers.

Timeline mínima:

| Evento | Estado observável |
|---|---|
| jobs 0 e 1 entram | dois children ativos; job 2 aguarda capacity |
| job 1 termina primeiro | resultado 1 fica retido por `.input` |
| job 0 termina | resultados 0 e 1 ficam disponíveis; job 2 entra |
| parent cancela | producer fecha; children restantes recebem o sinal |
| scope sai | todos os cleanups terminaram; nenhum job ficou detached |

O scheduler de teste deve reproduzir essa timeline por um schedule ID. Outro
packing físico precisa produzir o mesmo resultado quando ordering é `.input`.

### 3.17 Regulador do Forno Improvável

Famílias: construção, definite initialization, computed property e protocol
requirement.

Aceite:

- `PidController(...)` baixa para `construct` sem prometer heap ou stack;
- o `init` customizado remove o memberwise initializer exportado;
- cada gain inválido lança `KitchenError` antes de publicar a instance;
- todos os caminhos normais inicializam os cinco fields;
- uma falha destrói somente os fields já inicializados;
- `PidController.isIdle` usa um getter local, síncrono e sem allocation;
- `PaymentProof.canServe` não oculta I/O, `throws` ou suspensão;
- `BrigadeMetrics.completionCount` atende ao property requirement;
- o corpus estrutural cobre `get`, `set` e `modify`;
- `modify` devolve um borrow escopado com `return inout`.

O ensaio deve rejeitar `async init`, getter com service call e uso de `self`
antes da inicialização completa.

### 3.18 Porta da Despensa Quase Segura

Famílias: visibilidade, records transparentes, objects e protocol witnesses.

Aceite:

- fields de `Guest` herdam `export` do struct transparente;
- o initializer memberwise de `Guest` também é `export`;
- fields de `PidController` ficam no módulo porque o struct declara `init`;
- `StockReservation` publica somente `ingredients` e `release`;
- `id` e `releaser` não cruzam o módulo;
- `CommandStream` deixa de ser exportado porque só apoia `decodeCommand`;
- `BrigadeMetrics` permite construção externa somente pelo `export init`;
- `BrigadeMetrics.completionCount` herda a visibilidade do requirement;
- `Course.fromOrdinal` usa `export` porque associated functions não herdam;
- cases de enums exportados não repetem o modifier;
- storage de service é privado e não aceita `export`;
- `w interface` materializa todos os níveis efetivos.

O ensaio deve rejeitar uma API exportada que menciona um tipo restrito ao
módulo. Ele também deve rejeitar acesso externo ao storage do object.

### 3.19 Reserva para Viajantes de Linhas do Tempo Futuras

Famílias: destructuring, ownership, evolução de records e SemVer.

Aceite:

- `Order(guests, course, ...)` seleciona fields por nome;
- o pattern owned consome o `Order` inteiro antes de iniciar os children;
- o marker `...` cobre `id`, `guest`, `notes`, `timeline` e fields futuros;
- `timeline` possui default e não quebra a construção existente em `command.w`;
- um pattern externo sem `...` é rejeitado;
- `let ref Type(...)` cria borrows compartilhados dos fields;
- `let inout Type(...)` cria borrows exclusivos dos fields;
- o owner não pode ser movido enquanto um borrow de field estiver vivo;
- `object` e `service` não aceitam destructuring;
- `w interface diff` classifica field com default como minor;
- field obrigatório, case novo ou `init` explícito são mudanças major;
- source, layout e schema de wire permanecem contratos separados.

O ensaio deve rejeitar modos de ownership misturados no mesmo pattern. Ele
também deve rejeitar uso parcial do aggregate após um destructuring owned.

### 3.20 A Última Comanda Antes da Implosão

Famílias: receiver consuming, partial failure, deinit e async cleanup.

Aceite:

- `take fn finish()` recebe ownership local de `CommandStream`;
- `(take stream).finish()` mostra a transferência no call site;
- `stream` fica inválido depois da call;
- `finish` pode mutar seu receiver sem usar `mut fn`;
- success ou error executa `CommandStream.deinit` uma vez;
- `take async fn release()` move `StockReservation` para o task frame;
- cancellation executa cleanup e `deinit` do receiver;
- falha de `release` não restaura a reservation;
- `StockReservation` não precisa de um flag `released`;
- `OvenLeaseApi.close()` continua não consuming porque `ServiceRef` é aliasable;
- `(take copy value).method()` preserva uma cópia quando o tipo é `Copy`;
- `: self` é rejeitado em `take fn`;
- um service não implementa `take fn`;
- mudar um method para `take fn` é uma mudança major de interface.

O ensaio deve rejeitar `take stream.finish()`, consumo por `shared` e call após
transferência. Ele também deve rejeitar destructuring owned de tipo com
`deinit` customizado.

### 3.21 O Caixa com Duas Portas e Nenhum Paradoxo

Famílias: overload por forma de call, labels, defaults, initializers e
delegação.

Aceite:

- `Money(minorUnits:, currency:)` seleciona o initializer de unidade mínima;
- `Money(majorUnits:, currency:)` seleciona o initializer total com conversão exata para `i128`;
- os labels selecionam o initializer antes do type-check;
- `self = Money(...)` delega sem trocar a identidade do storage;
- overflow na conversão lança `DomainError.overflow`;
- falha antes de completar `self` limpa somente os fields completos;
- falha depois de completar `self` executa `deinit` uma vez;
- os overloads de `expectedEnergy` possuem formas disjuntas;
- `_`, `during:` seleciona telemetry sem consultar seu tipo;
- `_`, `duty:`, `during:` seleciona power e duty;
- return type, constraints, efeitos e conversões não ordenam candidatos;
- um default não pode criar uma forma aceita por outro overload;
- uma referência a overload exige uma closure que mostre a forma;
- uma forma nova em overload set existente é minor quando não altera lookup;
- o primeiro overload de uma função singular é major;
- um initializer disjunto novo é minor;
- mudar uma forma existente é major.

O ensaio deve rejeitar dois `parse(_)` que diferem somente por tipo. Ele também
deve rejeitar `serve(_)` quando um default cria essa forma duas vezes.

### 3.22 O Último Maitre e Seus Três Telefones

Famílias: `fn`, `some fn`, `any fn`, captures e callable modes.

Aceite:

- `fn(ref Arrival): Welcome` contém somente um target sem capture;
- uma call por function value usa argumentos posicionais;
- `some fn` preserva o tipo concreto e permite specialization;
- `any fn` possui owner, invoke e drop observáveis;
- erasure contextual usa a policy normal de OOM do product;
- `try erase(take value, allocator:)` torna allocator e recovery explícitos;
- inline erasure preserva origins; spill adiciona a origin do box;
- labels e defaults não entram no function type;
- `mut fn` exige um callable mutável e acesso exclusivo;
- `take fn` exige `(take callable)(...)`;
- um ponteiro `fn` atende a `transferable` e `shareable`;
- closures derivam esses predicates do ambiente;
- `mut fn` não atende a `shareable`;
- uma closure que move capture não satisfaz `fn` ou `mut fn`;
- um overload set exige closure explícita antes da conversão;
- uma instance method exige closure explícita para capturar o receiver;
- `unsafe fn<abi: .c>` não aceita capture, `async` ou `throws`.

O ensaio deve rejeitar labels numa call por valor. Ele também deve rejeitar uma
segunda call ao `manifest` consumido e provar que failure de erasure consome o
source sem publicar um existential parcial.

### 3.23 Farol de Falhas Improváveis

Famílias: `Option`, `Result`, typed throws, panic, OOM, cleanup e diagnostics.

Aceite:

- `T?` representa somente `.some(T)` ou `.none`;
- postfix `?` propaga somente `.none` de uma função que retorna Option;
- `?.` faz leitura condicional e `??` avalia o fallback de forma lazy;
- uma mutation usa `if let inout`, não optional chaining;
- `try` aceita uma call `throws E` ou um `Result<T, E>`;
- `try?` converte qualquer error recuperável em ausência, mas não captura panic
  ou cancelamento;
- cada closure fallible possui o próprio `try`;
- `Result.capture` transforma direct style em um valor armazenável;
- `catch` usa ordem lexical e pode ter guard;
- `return`, `throw` e cancelamento executam cleanup LIFO;
- panic não garante user cleanup e não pode ser capturado;
- um service no mesmo process não é uma fault boundary;
- uma instância Wasm dedicada pode ser uma fault boundary;
- `tryReserve` e `tryDuplicate` permitem recovery de alocação;
- alocação normal pode causar panic `.outOfMemory`;
- todo valor non-unit precisa ser usado ou descartado com `let _`;
- o diagnostic JSON preserva code, byte spans, facts e fix applicability.

O ensaio deve injetar falha na abertura, no decode e no close. Cada saída
estruturada deve fechar o recurso uma vez. Um teste em process separado deve
confirmar panic `.bounds`. Outro teste deve executar o mesmo service dentro e
fora de uma instância Wasm. Somente a instância dedicada pode reiniciar sem
encerrar o host.

Os fixtures negativos devem rejeitar:

```w
archive.duplicate(payload) // W-VALUE-0001: unused Result
guest?.visits += 1         // W-OPTION-0004: optional mutation
let guest = guests[id]!    // W-PARSE-0001: force unwrap does not exist
```

### 3.24 Catálogo das Prateleiras Recursivas

Famílias: generics, primary associated types, protocol composition, inference e
conditional conformance.

Aceite:

- `Source<Item>` fixa um associated type por conformance;
- `Catalog<Item>: Source<Item> & Counted` compõe requirements sem ordem;
- `extension<T: Display & Equatable>` declara constraints antes do uso;
- `alias Item = T` fornece o witness de forma explícita;
- `firstEquals` infere `T` e `S` pelos argumentos;
- o body generic usa somente members declarados pelas constraints;
- o call site não escolhe uma conformance por import ou ranking;
- a interface registra generic HIR e witness IDs;
- `name: Type` expõe value compile-time com label, enquanto `_ name: Type`
  mantém o nome interno e torna `name:` opcional;
- múltiplos values com label opcional precedem values nomeados, sem reorder;
- named values de type heads aparecem por lookup estático e não viram fields;
- `Matrix<f32, rows: 3, columns: 4>.rows` consulta a contract value associada;
- monomorphization e shared lowering preservam a mesma semântica.

O fixture negativo deve criar duas conditional conformances que se sobrepõem.
O compiler deve mostrar as duas heads e uma instantiation que pertence às duas.

### 3.25 Corredor dos Futuros que Ainda Podem Acontecer

Famílias: enum case subsets, payloads, flow narrowing, layout e evolução de API.

Aceite:

- `ServiceStage<[.reserving, .preparing, .serving]>` exclui `.cancelled`;
- `StagePath<[...]>` mantém ordem; `ServiceStage<[...]>` normaliza um conjunto;
- `StageAfterAccepted` permite somente `.reserving` ou `.cancelled`;
- `CancellableStage` impede pedidos de cancelamento após `.completed`;
- `RestaurantApi.cancel` retorna somente `CancelledStage`;
- `TerminalStage` exige um `switch` com `.completed` e `.cancelled`;
- a ordem source da lista não muda a identidade do subset;
- um retorno fora do conjunto falha no type checker;
- `switch` exige somente os cases possíveis;
- adicionar um case ao retorno torna um switch explícito nonexhaustive;
- `_` continua uma decisão explícita de aceitar a ampliação;
- subset para enum base ou superset é conversão implícita;
- enum base para subset usa `try` ou `try?`;
- um enum generic aplica o subset em um segundo envelope;
- um subset de um case preserva o payload sem unwrap implícito;
- payloads dos cases mantidos continuam disponíveis;
- `UsableOvenReading` mantém payloads diferentes de `.stable` e `.warming`;
- `throws Enum<[...]>` restringe `throw` e a exhaustividade de `catch`;
- `.case` exige expected enum type; `EnumName.case` resolve colisões;
- membership com `in` testa alternativas; enum comum não representa flags;
- o subset não cria wrapper nem layout público novo.

O fixture negativo deve retornar `.cancelled` como `WorkStage`. Outro fixture
deve adicionar `.cancelled` ao alias e omitir esse case em `workInstruction`.
Outros fixtures devem passar `.completed` a `requestCancellation` e retornar
`.preparing` de `routeAcceptedOrder` sem ampliar `StageAfterAccepted`.

### 3.26 Arquivo dos Nomes que Não Sobrevivem ao Universo

Famílias: queries `type of`/`info of`, `TypeId`, `Reflectable`, síntese e
metadata alcançável.

Aceite:

- `type of T` identifica uma specialization no build atual;
- `type of value` observa concrete ou existential sem mover o source;
- `info of T` retorna `ref TypeInfo` somente para `T: Reflectable`;
- `info of value` exige concrete `Reflectable` ou composição existential com `Reflectable`;
- `TypeKind` fecha `scalar`, `struct`, `object`, `enum`, `refinement` e
  `enumSubset`, e `TypeInfo` expõe views process-lifetime sem layout ou offsets;
- `TypeId` é opaque, local ao build, `Copy`/`Equatable`/`Hashable` e não
  serializável;
- `value as? T` retorna `ref T?` borrowed e avalia o source uma vez;
- para um existential nominal compatível, `value is T` equivale a
  `type of value == type of T`, e `value as? T` é `.some(ref payload)` quando
  o teste é verdadeiro, ou `.none` quando é falso;
- `(info of T).id` equivale a `type of T`, e `(info of value).id` equivale a
  `type of value`; queries não executam accessors nem código do usuário;
- o type namespace vence para Subject não parentetizado;
- `type of (T)` força expression quando `T` também é um valor;
- `type of value == other` termina o Subject antes da relação;
- `type of (value == other)` consulta a expression composta;
- `Reflectable` emite somente metadata alcançável;
- o descriptor mostra properties exportadas e omite `secretCalibration`;
- `ActionableSignal` mostra somente os dois cases permitidos;
- `Hashable` e `Reflectable` são sintetizados sem annotations;
- backing storage de property behavior não aparece;
- debug symbols podem ser removidos sem alterar metadata;
- nenhum descriptor oferece offset, dynamic construction ou acesso por string.

Fixtures negativos devem rejeitar `info of` sem `Reflectable`, target não nominal
em `as?`, escape do borrow de `as?` e `type of` sem Subject. Os dois últimos
usam os diagnostics gerais `W-BORROW-0001` e `W-PARSE-0020`; postfix core
inexistente usa `W-EXPR-0006`. No contexto
baseline core/std sem binding user-defined, eles também registram que não são
fornecidos `std.reflect`, o namespace compiler-owned `reflect.*`, `typeof` como
query, `type(of:)` e `TypeId.of<T>()`. Isso é ausência de superfície core, não
ban lexical: `reflect`, `info`, `of` e `typeof` continuam identifiers de usuário
onde a gramática os permite.

### 3.27 Mesa para um Número Incerto de Convidados

Famílias: rest homogêneo, labels, expansão, ownership e lowering.

Aceite:

- `Course...` aceita zero ou mais valores `Course`;
- o label `courses:` aparece somente antes do primeiro item;
- `each planned` expande uma collection sem colidir com `4...`;
- `Arguments<T>` não pode escapar do body;
- `ref T...` preserva borrows por elemento;
- `take T...` e `each take values` preservam consumo;
- `inout T...` é rejeitado;
- a expansão final não exige heap;
- uma forma fixa que intersecta a forma rest é rejeitada;
- rest W não cruza C varargs.

O fixture negativo deve declarar `serve(table)` junto de
`serve(table, _ courses: Course...)`. O compiler deve mostrar a forma
intersectada `serve(_)`.

### 3.28 Forno que Não Pode Ser Ligado Duas Vezes

Famílias: estado runtime, typestate, transições consuming e paths estáticos.

Aceite:

- `canMove` funciona em compile time e runtime;
- `StagePath` rejeita o primeiro edge inválido;
- `OvenSession<.idle>` oferece `activate`, mas não oferece `close`;
- `activate` consome a session idle;
- cada case de `ActivationOutcome` devolve um novo owner tipado;
- `OvenSession<.ready>` e `OvenSession<.faulted>` oferecem operações distintas;
- o argumento enum não exige uma tag runtime;
- uma collection com states misturados usa `AnyOvenSession`;
- `StageSnapshot` carrega uma revision;
- um snapshot não concede authority sobre uma service instance;
- `AppliedOrStale` reduz a exhaustividade de `MoveOrderResult`.

O fixture negativo deve declarar
`StagePath<[.accepted, .completed]>`. O diagnostic deve identificar o primeiro
edge. Outros fixtures devem reutilizar a session movida, chamar `close` em
`OvenSession<.idle>` e atribuir `OvenSession<.ready>` a um binding idle.

### 3.29 Sino com Três Formas de Ausência

Famílias: validity, niche, enum subset, fallback de layout e fronteira C.

Aceite:

- `BellTarget` pode usar null como niche interno;
- a façade C recebe sempre um pointer canônico;
- `BellSignal<[.ringing, .silent, .unavailable]>` exclui `corrupted`;
- o switch do subset exige somente os três cases publicados;
- adicionar `corrupted` ao resultado invalida o switch anterior;
- `Option<Option<shared BellState>>` preserva três estados;
- o profile portátil pode usar tag e payload explícitos;
- o profile otimizado produz o mesmo resultado;
- `Address` observa os bits sem manter owner ou provenance;
- `Address<space: S>` não se compara nem converte implicitamente com outro
  address space;
- W v0 não converte `Address` em pointer;
- `withAddress` precisa do pointer original e conserva sua provenance;
- cópia tipada preserva estado externo de pointers; Bytes não faz round-trip;
- low-bit exige alignment real, address space, lowering provenance-aware,
  tooling e boundary canônica;
- `f64` preserva todos os NaNs e signed zero;
- sanitizer ou hardening pode desativar compactação;
- `RepresentationMap` não muda somente porque o allocator provider mudou;
- `w explain layout BellTarget` mostra a prova e o fallback.

Um fixture negativo deve remover `.unavailable` de `describeBell`. Outro deve
tentar passar `BellSignal.corrupted(...)` para o parâmetro refinado.

### 3.30 Duas Cozinhas e Nenhuma Thread Milagrosa

Famílias: execution domain, capacity, paralelismo aninhado, fairness e liveness.

Aceite:

- o initializer `async` herda o domain placement do parent;
- `spawn` exige um domain explícito;
- `spawn<.thermal>` é válido e entra na fila FIFO do domain serial;
- `spawn<.main>` é válido e preserva a affinity do host;
- await ou join no domain serial libera seu permit antes de aguardar o child;
- `spawn<.catalog>` admite reads comuns que podem coexistir;
- `spawn<.catalog, .barrier>` espera reads anteriores, executa sozinho e
  libera os tickets posteriores somente depois do cleanup;
- o body da barreira é `neverSuspend` e o mesmo place usa `ref` nos reads e
  `inout` somente no ticket de barreira;
- `ctx.execution.openSerial` cria uma lane runtime bounded no pool existente;
- a lane runtime preserva FIFO no primeiro start, permite só um segmento
  runnable, libera o permit durante suspension, limita jobs e frame bytes,
  devolve o input rejeitado e libera budget somente após drain;
- `.compute` permanece válido quando a capacity efetiva é 1;
- `.network` pode compartilhar executor físico com `.io`;
- `execution` declara `.thermal` e o product fornece binding serial;
- `.thermal` e `.compute` compartilham o pool `cpu`;
- declarar ou importar um domain não cria um executor;
- um módulo importado não cria domain, queue ou thread;
- cada unit recebe task, frame, timer e ready budgets bounded;
- o initializer `async` avalia captures e argumentos uma vez antes de publish;
- budget exhaustion limpa o staging uma vez e não inicia o body;
- um wakeup não aloca uma queue node;
- os dois `mixBatch` de `mixAcrossTwoKitchens` compartilham o compute budget;
- dois limits de 8 não criam 16 workers quando a domain capacity é 6;
- o parent suspenso não retém o último permit necessário ao child;
- blocking FFI não ocupa o compute budget;
- cancellation antes de uma blocking call começar remove o job;
- cancellation depois da entrada foreign mantém owner e buffers até drain;
- a correção não depende de dois jobs executarem simultaneamente;
- scheduler replay pode trocar a ordem dos siblings sem trocar o resultado;
- `Task#yield()` não funciona como barrier;
- `mixBeforeTheLastBell` usa deadline monotônico e devolve `TaskOutcome`;
- `TaskTimeout` usa nanoseconds exatos; ausência de timeout não usa infinity;
- body settled vence cancellation posterior e só fica visível após cleanup;
- fail-fast cancela cedo e escolhe o error primário pela ordem declarada;
- nenhum launcher, task, group, service ou entry possui `priority` ou `qos`;
- a std não possui `.background`, `.userInteractive`, `Task.currentPriority` ou
  `Task.withPriority`;
- política física do provider só muda latência e ordem que o contrato aplicável
  deixa unspecified; ela preserva cada order/arbitration realmente garantida;
- `w explain execution` e o provider receipt mostram a política física, o
  suporte do target e `sourcePriority: absent` como evidência não branchable;
- política física não substitui deadline, admission, domain ou isolation;
- para o mesmo trace lógico, outcome, owner/drop e decisões derivadas são
  iguais; outro trace permitido pode mudar deadline, admission, winner ou outro
  outcome permitido.

O scheduler adversarial usa uma única CPU lógica, escolhe somente entre jobs ou
events cuja ordem o contrato aplicável deixa unspecified, esgota cada budget e suspende um nested group quando a
capacity está cheia. Ele não pode fabricar outcome, ignorar cancellation ou
deadline, violar order/arbitration garantida, burlar admission, capacity,
budget ou drain, nem perder owner/drop. Isso não cria FIFO global de channel ou
service. Replay fixa as decisões lógicas de schedule,
timer/deadline e eventos externos; detalhes de worker e queue ficam no sidecar
físico.

O caso QOS0 usa um pedido com alergia. A service instance dedicada isola state;
admission, reserva e budget protegem overload; deadline produz cancellation. O
product pode usar o domain atual, compartilhado ou dedicado por placement,
performance ou liveness. Domain não protege correctness. Success só ocorre
depois da validação segura. Deadline vencida produz cancellation/rejection sem
unsafe fulfillment ou partial commit, seguida de terminal drain. Progress
continua condicional às premissas do profile.

`domain_oracle.w` verifica inheritance do initializer `async` e o target explícito de
`spawn`. Ele aceita domains seriais, preserva FIFO, mantém `.compute` válido com
capacity um e exige `.parallel` somente para `parallelMap`. Ele também modela
tickets comuns e de barreira, capability `barrierDispatch`, exclusividade e a
rejeição de um body suspending. O oracle também limita jobs e frame bytes de
lanes seriais dinâmicas e verifica `open -> closing -> drained`. O deployment não
pode aumentar a capacity do artifact. Declarar ou importar um domain continua
sendo somente uma requirement. Somente `openSerial` cria uma lane runtime; ele
não cria thread ou executor. W não possui `parallelDefault`.

### 3.30.1 Closure e liveness E1

Famílias: runtime closure, cleanup, provider completion, frame reclamation,
blocking foreign e shutdown.

`liveness_oracle.w` é uma máquina host-pura para E1. Ela usa
`TaskClosurePhase`, `RuntimeWaitPhase`, `BoundaryShutdownPhase`,
`ProviderOutcome`, `CompletionDisposition`, `CancelDisposition`, `CommitGate` e
`FrameReclaimGate`. O corpus também liga
`execution.w`, `supervision.w`, `lifecycle_oracle.w` e `scheduler_oracle.w`; não
duplica suas transições. São 41 casos, 473 operações e sete testes host.

Aceite:

- body settled fecha admission antes de child/wait drain, cleanup, typed drop,
  runtime quiescence e outcome cell;
- `defer` instala em body active/open; `finishCleanup` fecha stack vazia,
  inclusive scope sem valores, e typed drop inverso ocorre exatamente uma vez;
- um cleanup node `defer async` mantém child estruturado até o child committed,
  registra cancellation recebida sem abortar o node e permite cancel local do child;
- um wait criado pelo cleanup termina antes do typed drop e não registra trabalho
  no parent closing;
- registration, queue, timer ou waker criado pelo cleanup exige node `defer async`
  ativo e pode drenar somente no runtime drain;
- cancel request é distinto de provider completion em ambas as ordens; cancel
  antes de submit pode drenar localmente, cancel tardio é idempotente e cancel
  depois de outcome committed não altera o resultado;
- `(OperationId, generation)` suprime callback de completion tardia ou stale e
  ainda drena a registration antiga;
- frame bytes reclamam depois da closure quiescent sem exigir join do handle;
- TCB/outcome cell continua viva por join, observer ou retention;
- children, registrations, queue tickets, timers, wakers e runtime refs impedem
  reclaim prematuro;
- blocking foreign unbounded é rejeitado fora de boundary killable; kill físico
  produz boundary failure;
- shutdown fecha admission, solicita cancelamento, drena, para ou escala para
  termination forced sem converter o resultado em `.canceled`;
- geração nova permanece isolada de completion velha e o host cleanup registry
  é a última autoridade de recursos próprios.

O corpus E1 não é runtime W. Ele não prova scheduler, clock, OS I/O, fairness
absoluta, hazard/epoch/RCU, device scopes, recovery distribuído ou terminação de
user code.

### 3.30.2 Ownership e execução MX0

Famílias: owner graph, suspensão, staging, mobility, loans, admission,
cancellation, cleanup, outcome, join e drop.

`ownership_execution_oracle.w` usa as quatro formas de execução no mesmo fluxo.
O corpus host possui 46 casos, 274 operações e 14 testes independentes. Ele
aceita:

- call direta e `await` na task corrente sem inventar `share`;
- initializers `async` e `spawn` com captures `take`, `copy`, `ref` e `inout` explícitos;
- owner em staging antes da publicação e no child depois dela;
- rejection de admission sem body e com cleanup único;
- cleanup antes de outcome committed e join;
- mutation por `inout` visível somente depois do join;
- saída de escopo que cancela, drena, faz join e destrói resultados ignorados;
- lowering inline ou queued com o mesmo estado lógico.

Ele rejeita uso do parent durante `await`, binding movido, borrow instável,
capture incompatível com o domínio, join prematuro ou repetido, owner em outcome
de falha, child órfão e saída sem progress cooperativo. O preflight de saída não
altera siblings quando o drain não pode começar.

MX0 não executa W. Ele compõe M1, E0 e E1 e não substitui o checker, allocator,
scheduler, runtime ou provider.

### 3.30.3 Direct entry de `async fn` SYNC1

O spelling `try sync` do Atlas chama a ordinary entry de uma declaration
`async fn` concreta cujo body visível prova `neverSuspend`. `fetch` permanece
aceito porque seu body apenas retorna `city`. Se qualquer caminho ganhar
`await`, `Task#yield`, initializer child, join, service ou I/O suspending,
`defer async`, call bare/`await` para `maySuspend` ou `sync` para facet absent,
`directEntry` passa a `absent` e o mesmo call site deixa de compilar.

Uma direct entry pode chamar outra com `sync`. A async entry continua
publicando `may`, mas a entry selecionada é `neverSuspend`. O proof compõe por
ponto fixo, inclusive num SCC de calls `sync`, sem executar a recursão ou provar
termination. Perda de facet propaga aos callers. Uma forma `sync` inválida para
function ordinary ou callee sem summary/facet torna o caller `absent`; ela não
vira call ordinary por ter `suspension: never`.

O caso adversarial do Restaurante compara dois bodies explícitos. Um body sem
suspensão aceita `try sync`; outro que pode executar `await catalog`, mesmo com
cache hit provável, é rejeitado antes de qualquer effect. Não existe
`WouldSuspend`, partial execution, readiness dinâmica ou fallback runtime.
`sync` preserva a mesma task, context e domain e mantém `blocksThread: false`.
Trabalho CPU longo ainda pode pedir `spawn<domain>` por custo, mas placement não
muda a elegibilidade estática.

SYNC1 também rejeita function ordinary, callable async inferida, protocol ou
foreign bodyless e function value sem o facet. Uma export concrete que perde
`directEntry: available` muda `SemanticInterfaceKey`. O corpus host não é
semantic checker, compiler, dual-entry ABI, runtime W, provider ou estudo
humano/modelo.

### 3.31 Balcão dos Oito Bits e das Sessenta e Quatro Colheres

Famílias: refinement, facts de range, enum subset, texto, SIMD e tensor.

Aceite:

- `FlavorSignal = Int<(1...128)>` mantém o carrier de `Int`;
- `FlavorSignal + FlavorSignal` prova o intervalo `2...256`;
- a conversão do resultado para `FlavorPair` não executa um check runtime;
- uma call externa ou um field com layout canônico recebe o carrier completo;
- storage estreito só aparece em valores ou buffers que não escapam;
- 8-bit loads, 16-bit multiply e 32-bit accumulation preservam o resultado;
- `ActiveStage` remove os cases terminais e torna o switch exaustivo;
- `WireName` prova 64 bytes;
- `ScalarName` prova no máximo 256 bytes;
- `DisplayName` não prova um limite estático de bytes;
- `HorizonSignLabel` compõe limite, mutation provada e placement inferido;
- `InlineString<N>` não é necessário para obter storage bounded privado;
- `w explain performance` separa fato, decisão, estimate e measurement;
- `InterferenceCounters` permite layout privado sem adicionar `cache` a
  `Atomic<T>`;
- `countCompleted` e `combineBrigadeCounts` tornam partition e join explícitos;
- `scanMenuDelimiters` aceita somente `RestaurantMenuBytes` de `16...32` bytes,
  recebe `menu: ref`, usa `Simd<u8, lanes: 16>` em chunk completo e tail
  parcial, e combina delimiter e LF dentro de cada chunk. Fill igual ao
  delimiter não conta quando a mask marca lane inactive;
- `wrappingByteVectorOracle` deriva low wrapped bits e overflow mask por lane;
  `duplicateStaticSwizzle` aceita índice static duplicado;
- `floatReductionWitness` usa `ReductionMode` nominal com `reduceAdd(mode:)` e
  `reduceMultiply(mode:)`; o source não alega execução do backend;
- desligar toda especialização produz os mesmos valores, errors e panic.

O oracle diferencial executa `flavorScore` com lowering portátil, vector,
storage estreito desativado e storage estreito ativado. Cada execução precisa
produzir o mesmo tensor e o mesmo overflow. O benchmark registra target, CPU,
dataset, allocations, code size e intervalo de ruído.

O caso SIMD é um oracle de design host-only. Ele não executa W nem implementa
`std.simd`, compiler, runtime ou provider. A baseline semântica fica em
`DESIGN.md` §18.5 e W-1459. Native, split e scalar são lowerings equivalentes,
não claims de speed ou de instruction.

### 3.32 Caixa dos Números que Recusam Disfarces

Famílias: literal exato, conversão, integer, float, decimal, quantization e
range.

Aceite:

- radix e exponent não perdem informação antes do expected type;
- um suffix fora do range falha no type checker sem truncar;
- `u8 + u16` usa `u16`, mas `i8 + u8` exige uma escolha explícita;
- debug, release, const evaluation e tensor usam o mesmo overflow;
- divisão signed trunca em direção a zero;
- `euclideanDivide` e `euclideanRemainder` exigem APIs nomeadas e aceitam ambos
  os sinais do divisor signed;
- `euclideanDivide(i32.min, -1)` falha como `/`, enquanto
  `euclideanRemainder(i32.min, -1)` retorna `0`;
- shift inválido e left shift com perda de bits não viram comportamento
  indefinido;
- serialization escolhe `.little` ou `.big`; `.native` nunca vira wire format;
- float division by zero produz o valor IEEE;
- NaN mantém equality parcial e não entra diretamente como key de `Map`;
- `TotalFloat` fornece uma ordem e um hash compatíveis;
- decimal esperado não passa por binary float;
- `f16`, `bf16` e quantized storage não escondem scalar arithmetic e declaram
  accumulator e conversion;
- um range invertido é vazio;
- uma progressão descendente usa `stride`, não inverte o significado do range.

O oracle gera valores em cada boundary de integer, conversion e range. Ele
compara o frontend C, o frontend self-hosted, o interpreter de ConstIR e o
lowering MLIR. Para floats, ele compara bits das operações strict, classes IEEE,
signed zero e total order. Modes `fast` são avaliados por bounds próprios e não
participam do oracle bit-exact de `.strict`.

### 3.33 Scope Lexical do Cardápio

Famílias: placement, allocator scope, budget, escape e OOM.

Aceite:

- um local síncrono fixo que não escapa usa um bloco
  `allocator scratch: .fixed<capacity: 2<iec.MiB>> { ... }`;
- placement e provider são verificados pelo target; a sintaxe não esconde
  uma alocação transitiva em chamadas arbitrárias;
- o bloco fecha admission, drena children/waits/loans/dependents, executa drops
  tipados e só depois recupera o storage;
- o escape de um valor ligado ao allocator é rejeitado antes do runtime;
- `object` não implica heap;
- construções diretas usam o allocator corrente;
- uma call a um callee com o slot contextual standard pode omitir `allocator:`;
  o compiler insere a referência corrente;
- cada função intermediária precisa declarar o slot para propagar a call;
  uma função sem slot não herda o block;
- `allocator:` explícito continua válido para override e rehome;
- `tryReserve` falha antes de consumir os elementos;
- cada string duplicada mantém a origem do allocator;
- `.fixed` fornece uma capability scoped de `Allocator`; `.bounded` permanece
  Research e não é um plan ativo do oracle ASC0;
- `.fixed<capacity:N>` é lexical; o target escolhe stack, task frame ou storage
  local/fixed. `.bounded<budget:N>` limita bytes committed sobre seu provider e
  não escolhe placement; `.stack<capacity:N>` permanece uma alternativa de
  Pesquisa, sem fallback físico implícito;
- reuso/reset não é a surface comum; o bloco exige que nenhum child, wait, loan
  ou dependent permaneça aberto;
- a origem registra instance lifetime, deallocator, mobility e adoption family;
- storage local não atende a `transferable` sem `rehome` explícito;
- facts do contrato de allocation derivam mobility cross-domain quando um owner
  cruza `spawn`; origens locais são rejeitadas, salvo `rehome` antes da fronteira;
- `rehome` move storage independente e realoca somente storage dependente;
- uma falha de `rehome` consome e limpa o snapshot e o destino parcial;
- não existe `attemptRehome` na surface vigente; `rehome` consuming não restaura
  o source;
- o budget cobra alignment, padding, growth retido e metadata de drop;
- `.budgetExceeded` informa `limitBytes`, `committedBytes` e `requestedBytes`
  sem a identidade do provider;
- `.sizeOverflow` falha antes da comparação de budget e não altera
  `committedBytes`;
- `.budgetExceeded` não vira `.outOfMemory`;
- drop executa em ordem inversa da construção concluída;
- um child paralelo não compartilha o allocator default;
- um bloco anônimo cria owner, lease e scope, mas não cria binding observável;
- o build profile pode publicar um default geral; `memory.generalAllocator: .none`
  não fornece allocator geral/root e rejeita requests gerais no grafo alcançável,
  sem escolher register, stack, static storage ou task frame;
- parâmetro comum chamado `allocator` não é contextual;
- function values preservam o slot; uma closure armazenada/escapante exige
  capture explícita e uma closure não escapante pode inferir `ref` sem tornar o
  allocator externo o current context;
- `.fixed` não pede storage ao OS quando o profile fornece placement suportado;
- `.fixed` sem `try` exige reservation estática, admission infallible e recursion
  fechada no profile; admission dinâmica exige `try allocator`;
- um plan customizado publica o descriptor lógico `AllocatorPlan` com
  `providerDigest: [u8; 32]`, version, failure, deallocator e mobility; o
  protocol usa `const descriptor` e `take fn open()`, e `AllocatorLease`
  fecha o provider em `deinit` exatamente uma vez. A aquisição fallible não
  entra no body nem cria binding:

  ```w
  import iec from std

  try allocator request: RestaurantPool(
    backing: ref processMemory,
    budget: 4<iec.MiB>,
  ) {
    let order = Order(allocator: request, id: id)
  }
  ```
- um allocator aninhado com nome distinto seleciona o binding mais interno pela
  regra lexical nominal geral; não existe uma regra especial de shadowing;
- `await` exige owner e storage estáveis no task frame. Um origin local não pode
  entrar em `spawn`, service ou channel sem `rehome` explícito;
- importar um módulo não cria uma heap implícita;
- o build profile fixa `generalAllocator` e `representation`;
- o snapshot retornado não depende do allocator temporário;
- `w check memory --require no-general-allocation` mostra a call chain que viola
  o profile.

O caso problem-first usa ambas as formas:

```w
allocator .fixed<capacity: 64<iec.KiB>> {
  let snapshot = try stageMenu(ref title, dishes: ref dishes)
}

allocator outer: .fixed<capacity: 2<iec.MiB>> {
  allocator inner: .fixed<capacity: 64<iec.KiB>> {
    let snapshot = try stageMenu(ref title, dishes: ref dishes)
    let portable = Array<String>(allocator: outer)
  }
}
```

`stageMenu` declara o slot contextual primeiro. A call omite o label e recebe a
lease corrente. O primeiro block demonstra a forma anônima. `outer`
demonstra override explícito e `inner` demonstra a precedência innermost.
`rootFallbackAfterIntermediary` perde o lexical caller context e usa somente o
contexto do próprio build profile; sob `memory.generalAllocator: .none`, uma
request geral sem capability falha. `countStagedMenuInParallel`
preserva o slot no function value. Uma closure armazenada não captura `outer`
sem capture explícita; uma closure local pode referi-lo sem capture observável,
mas suas allocations seguem o root ou o próprio slot. A matriz ASC0 também cobre overload
collision, ausência de allocator geral, requirement incompatível, await
stable e spawn de origin local.

O oracle host executa um modelo source-shaped de `stageMenu` com uma falha
injetada em cada allocation; isso não é execução do compiler, runtime ou
provider W. Scope exit e unwind limpam valores dependentes e o owner do bloco.
Se o provider raw não executa drops W, o drop ledger do compiler os executa
antes do bulk release.
Antes de `rehome`, toda falha limpa os valores pelo allocator local. Durante `rehome`,
toda falha limpa source e destino parcial uma vez. Um batch que reutiliza a
capacity reentra em um bloco depois de `rehome`; `reset` continua interno e não
é uma call source comum. O teste repete com allocator
do sistema, buffer fixo e os profiles `benchmark` e `benchmark-mimalloc`. Os
valores, errors e drops são os mesmos. Cada allocation mantém a origem
declarada; provider measurements podem mudar.

### 3.34 As Três Últimas Noites

Famílias: algoritmo determinístico, capacidade, SI, Money, estado, CLI, TUI e
HTTP.

`simulation.w` contém três cargas fechadas:

- `quietOrbit` prova o caminho sem overload;
- `photonRush` pressiona cozinheiros e mesas;
- `timelineCollision` faz a impaciência competir com uma cozinha serial.

Os hóspedes são personagens originais. Entre eles estão Ada Quasar, Capitão
Ontem, a Auditora da Causalidade e a Advogada do Paradoxo. Os nomes ajudam a
identificar traces sem alterar a semântica.

Fluxo interativo previsto:

```text
menu
place 42 7 3 cake please omit causality
status 42
dashboard
simulate timeline
cancel 42
shutdown
```

O oracle de mensagens Web cruza oito sources HTTP. `gateway.w` roteia por
`request.url.pathname` antes de consumir o body. O receiver `take` impede uma
segunda leitura. `boundedRequestCloneCompileOracle` registra somente a
assinatura consuming e o limite do clone. `http_oracle.w` concentra os
constructors, o limite de body, as leituras consuming, o clone bounded, o JSON
de um tipo simples `json.Codable`, a cópia explícita de incoming headers via
`RequestOverride` e a assinatura de `serve` com carriers `net`. O caminho de
produção não chama esse helper, e o corpus
não alega execução enquanto `std.http@1` e os carriers executáveis estão
missing. `web_bodies.w` fecha os dois carriers Web restantes. Blob usa
`shared Bytes` em W e não adiciona provider. FormData mantém a lista lógica,
enquanto `std.http@1` continua responsável pelo parser/serializer multipart,
boundary e admission. Os testes puros cobrem media type, faixa, repetição,
ordem e rollback de mutation; as rotas Request/Response continuam
provider-gated. O corpus WB0 em `tooling/web-body-cases.json` deriva 24 casos
positivos e negativos sem chamar o modelo de compiler ou provider.
`http_documents.w` valida os tagged documents de Command e encoda cada
AppResponse com order canônica. Os adapters são endpoint-owned/dedicated:
exportar sua plumbing para o host não cria `json.Codable` nos types de domínio
nem transforma o schema local em contrato global. Seus Problem Details usam
RFC 9457, a extensão `code` com tokens ASCII estáveis e status derivado do code
em 400, 422 e 403. O helper de produto fixa
`application/problem+json`; somente decode, erro semântico do document e
shutdown remoto são convertidos, enquanto service/response errors propagam.
Quantity usa adapters nominais que escrevem tokens de unit constantes.
Os adapters parseiam e formatam IDs pelos carriers `u64`/`u128`; o voucher usa
o alias nominal `WifiVoucher` antes de construir `LoginRequest`. `benchmark_app.w`
anexa `Headers` a uma resposta HTML e
constrói JSON com `Response.json`. `wifi_documents.w` valida login/revoke e
encoda Session. `wifi_app.w` devolve 204 sem body.
`worker_app.w` liga o gateway ao slot. `app.w` serve o mesmo handler no processo
nativo. Todos usam o mesmo modelo de mensagem.

`net_oracle.w` fixa os carriers de `std.net`. Ele usa somente constructors e
parse textual estrito para IPv4, IPv6, socket e listen addresses. O oracle
também chama resolve e connect com limits finitos e descriptors borrowed,
separa os cursors TCP e UDP, demonstra `finishWriting` e termina o write half
TCP com `finish`, aceita uma conexão no listener e preserva truncation no UDP. Ele não
alega execução enquanto `std.net@1` e a capability do host estiverem missing.

`process_oracle.w` fixa os valores da entry root nativa. `Arguments` preserva
cada argumento como `OsString`; `Context` projeta somente as capabilities do
produto, inclusive o `time.Clock` monotônico quando `.clock` está presente;
`ExitCode` separa conclusão portátil de fault. `process.args` e
`process.context` tomam empréstimos do mesmo owner do root. Dentro de um entry,
`process.clock()` é uma projection curta com a mesma identity, origin, authority
e lifetime de `process.context.clock()`. `process.deadline`
preserva value identity, origin e lifetime de `process.context.deadline`, sem
ampliar authority (`authorityExpanded: false`). A availability de cada alias é a
da projection longa correspondente. Eles não criam um singleton ambiental. PR0
deriva stdio, signals e drain em um oracle host, mas
não executa W, o scheduler, o sistema operacional ou o provider
`std.process@1`.

`time_oracle.w` separa `Duration` portátil de `Clock`, `Instant` e `Deadline`
root-scoped. TIME0 deriva clock não regressivo, resolução, política de suspensão,
origem, expiration sem disparo antecipado, cancellation drain e clock virtual.
`HostSuspendPolicy` descreve somente suspensão do HOST/SO: `.included` soma o
intervalo ao deadline, `.excluded` pausa a medição e `.unspecified` não permite
inferência. Ele não descreve coroutine, task ou `await`. Com 60 ms ativos, 50 ms
de HOST/SO suspend e deadline de 100 ms, included alcança, excluded não alcança
e unspecified exige um case explícito se o profile o exigir. O oracle não
executa W, timer, scheduler, sistema operacional ou o provider `std.time@1`.
Tempo civil não faz parte da capability `.clock`.
`process.clock()` seleciona o relógio root default sem throw quando a
capability está disponível e pode relatar `.unspecified`;
`try process.clock(hostSuspend: .included)` e
`try process.context.clock(hostSuspend: .excluded)` são seleções ativas e
usam `HostSuspendPolicy<[.included, .excluded]>`; `.unspecified` é rejeitado
no compile time. Providers unsupported ainda podem falhar antes do trabalho.
Uma lease de reserva exige `.included`; o
budget de trabalho ativo da cozinha exige `.excluded`; provider unsupported ou
unspecified rejeita uma solicitação ativa. Providers de Linux, Windows e Apple
podem oferecer políticas diferentes.

O oracle HTTP também reserva uma consulta RestPC segura e idempotente. O
request usa o método QUERY padronizado pelo RFC 10008. O content evita uma URI
longa e mantém o filtro tipado.

```http
QUERY /orders HTTP/1.1
Content-Type: application/json
Accept: application/json

{"stage":["accepted","preparing"],"limit":32}
```

`restpc_oracle.w` fixa o mapeamento sem antecipar a syntax de declaração de
rotas. O adapter só cria essa rota quando o handler prova `safe` e
`idempotent`. Ele
publica `Accept-Query`, inclui o content na cache key e exige CORS preflight no
browser. POST continua reservado para commands que podem alterar estado. A
forma source da declaração de rota permanece **Pesquisa**; este request é o
oracle do protocolo.

Aceite:

- o mesmo profile produz a mesma sequência de `SimulationEvent`;
- o algoritmo usa somente input, ticks inteiros e ordem estável do array;
- número de cozinheiros e mesas limita a admissão;
- o relatório mostra capacidade, duração do tick e event log;
- pedidos que esperam além da paciência saem com `.departed`;
- energia usa `Power * DutyCycle * PhysicalDuration`;
- receita usa `Money` em minor units e rejeita currency diferente;
- todo pedido termina como completed, departed ou unfinished;
- `queueHighWater` torna overload observável;
- `Command` representa menu, pedido, status, cancelamento, dashboard, simulação
  e encerramento;
- `AppResponse` é o único modelo de saída para CLI, TUI e HTTP;
- o renderer ANSI não muda os dados da resposta;
- o adapter HTTP não recebe autoridade para encerrar o processo;
- routing usa `request.url.pathname` e não cria `request.path`;
- o body de command possui um único receiver consuming;
- o oracle de compile surface do clone declara `maximumBufferedBytes`, sem
  alegar execução runtime;
- `benchmark_app.w` anexa `Headers` ao `Response` HTML;
- JSON usa `Response.json` e 204 usa `Response` sem body;
- Blob copia somente por `copy`, slices retêm bytes imutáveis e toda
  materialização recebe limite;
- FormData preserva insertion order e nomes repetidos; mutation recusada não
  altera a lista;
- source FormData não define multipart boundary nem `Content-Type`;
- a consulta de pedidos usa QUERY e não GET com content ou POST genérico;
- construção textual usa `append` no próprio `String`, sem um `StringBuilder`
  público;
- `LastLightSimulation` executa sem service registry;
- o target completo não consome `stdin` por duas APIs ao mesmo tempo;
- argv preserva a representação `OsString` e não faz decode lossy;
- projeções repetidas do processo mantêm o mesmo owner sem copiar o root;
- uma capability ausente falha no link antes de chamar um provider;
- `stdin` possui um cursor e linhas UTF-8 estritas com limite em bytes;
- chamadas de `stdout` não intercalam bytes e reportam progresso committed;
- signal registrations são generational e seus callbacks são structured;
- service drain não significa rollback, process exit ou fault recovery.

O oracle de equivalência remove sequências ANSI antes da comparação. O teste de
replay executa o mesmo profile duas vezes. Ele compara métricas e eventos campo
a campo.

### 3.35 Comanda que Sobrevive ao Maître

Famílias: `SupervisorRef`, identity keyed, admission, progress, cancellation,
outcome e deployment.

```text
host entry (target)
  → ServiceFamilyRef<OrderCoordinatorApi, OrderId>
  → turn curto de submit
  → fulfillment.tryStart
  → root owned pelo supervisor
  → prepareDish point
  → durable sleep
  → wait for table signal
  → capturePayment point
  → serveDish point
  → refundPayment point on application failure

status / signal / cancel / outcome
  → mesma instance keyed
  → initializer recebe WorkKeyRef para uma única key
  → snapshot ou control do supervisor
```

`supervision.w` mantém a coordination boundary curta. O trabalho longo não
captura state do service. Service imports são resolvidos para a process
generation. Um durable step não persiste os handles.

`supervision.fulfillOrder` permanece como oracle process-local de compensação.
O product liga `workflow.fulfillOrderDurably`.

Aceite:

- `ServiceIdentity<OrderId>` precisa corresponder ao pedido;
- o descriptor atenua `SupervisorRef` para um `WorkKeyRef`;
- o coordenador não recebe authority sobre outros pedidos;
- `tryStart` não espera capacity dentro do closed turn;
- rejeição tipada antes do commit devolve `FulfillmentInput`;
- uma key duplicada não substitui o primeiro pedido;
- o supervisor possui o root depois do commit;
- cancellation do caller depois do commit não destaca nem cancela o root;
- `unknownOutcome` exige reconciliação pela key e pelo effect ID;
- `WorkSnapshot` separa estado do trabalho de `ServiceStage`;
- `.waiting` libera running capacity, mas continua no limite de roots;
- cada point usa um enum case, não uma string ou ordem de source;
- `EffectId` permanece igual durante retries do mesmo point;
- `.atMostOnce` encerra com unknown outcome depois de uma completion incerta;
- serving at-most-once incerto não inicia refund automático;
- `.idempotent` repete com a mesma key de domínio;
- input é confirmado antes de liberar a função nominal do step;
- output e progress são confirmados antes de voltar ao workflow;
- lógica fora de steps não acessa clock, I/O, services ou mutable global;
- o sleep persiste o alarm do adapter sem manter task frame ou `Instant`;
- `WorkSnapshot` publica somente a duração restante do alarm;
- evento anterior ao wait permanece no inbox bounded;
- `EventId` duplicado não entrega o payload outra vez;
- `trySend` cheio devolve o payload sem bloquear o turn;
- `tableReady(TableId)` seleciona a mesma mesa usada por `serve(at:)`;
- event, timeout e cancellation possuem um único vencedor persistido;
- o deployment seleciona SQLite sem reduzir `recovery: .required`;
- o journal exige storage host-encrypted e nunca aparece em diagnostics como
  payload;
- progress é revisionado e substitui o valor anterior;
- o snapshot terminal preserva o último progress;
- cancelamento é idempotente e não vira `RestaurantError`;
- application error, cancellation e boundary failure são outcomes distintos;
- o root usa tasks estruturadas em seu interior;
- `capture` confirmado entrega `Payment` antes do próximo ponto de cancelamento;
- o `defer async` de refund é instalado antes da próxima suspensão;
- pagamento capturado recebe compensação se serving falhar;
- se refund falha, seu error termina o workflow e o history preserva o error de
  serving;
- restart de operação arbitrária usa `.never`;
- outcome, tombstone e queue possuem budgets separados;
- `WorkId` não concede authority;
- deployment reduz o envelope sem mudar os bytes do artifact;
- operation version não muda em um root ativo.

#### Recovery SR0

`service_recovery_oracle.w` fixa as decisões puras de fault, deduplication,
mailbox e generation. O oracle host fica em
[`tooling/service-recovery-machine.mjs`](../../tooling/service-recovery-machine.mjs).
Seu corpus derruba a instance antes e depois dos commits de input e outcome,
perde a reply, repete effect IDs, corrompe records, excede quotas e força restart
storms. Cada caso termina com ownership, outcome e generation definidos.

SR0 contém 48 casos e 392 operações, com checker, snapshot e 17 testes host
independentes. Ele compõe B0 e E1. Ele não executa W, wWire, SQLite, filesystem,
network ou provider.

### 3.36 Bilheteria para Muitos Universos

Famílias: package, product, entry, runtime graph, packing, toolchain, deployment
e artifact.

Aceite:

- `build.w` usa somente o subset data-only;
- `build.w` lista paths exatos e usa um lock compartilhado;
- `build.w` permite catalog da distribuição e somente system imports
  explícitos;
- o member `last-light/menu-compiler` satisfaz a `.build` dependency local;
- a authority, o name e a version do member conferem com a dependency;
- `w publish check` resolve a mesma dependency sem o workspace;
- build, product, test e benchmark mantêm graphs e target roles distintos;
- o lock fixa packages, a toolchain plan fixa providers, a recipe fixa inputs e
  o artifact record liga outputs;
- `compile-final-menu` declara tool, input, output e budgets;
- o compiler não entra no payload de `last-light-native`;
- o resource gerado entra na artifact key do product;
- cada package usa source allowlist e inclui seu próprio `LICENSE`;
- `.gitignore` não muda o source snapshot;
- `w package check` reconstrói somente com os arquivos publicáveis;
- `moduleSets` expande para a mesma lista em qualquer filesystem;
- `.always` e `.selected` tornam a ativação de cada module set explícita;
- `native-terminal` escolhe um único case por target;
- Linux e Darwin selecionam `platform/posix/native.w`;
- Windows seleciona `platform/windows/native.w`;
- os dois cases exportam a mesma interface `restaurant.platform.native`;
- o worker importa `gateway.w` e não alcança o adapter de terminal;
- o lock grava o case e os module sets selecionados;
- build locked falha quando um arquivo novo não está no lock;
- `.default` resolve o descriptor anônimo de `app.w`;
- `LastLightTui` não herda host bindings de `.default`;
- `runNative` e `runTuiEntry` registram signals no runtime;
- `LastLightWorker` usa os slots declarados pelo profile HTTP;
- um product iniciado por host liga um descriptor e seus host bindings;
- uma service-only unit publica seus providers no artifact index como roots;
- CLI, TUI e servidor local podem compartilhar um `process.main`;
- o worker HTTP possui outro product e outro artifact;
- cada requirement recebe provider, supervisor, host capability ou import;
- cada service ou requirement alcançável recebe uma binding default;
- startup config só altera bindings com override permitido;
- cada dependency de provider entra por um initializer argument explícito;
- um import aberto aparece na interface do artifact;
- `AromaProbeDevice` permanece na unit que recebe a capability do host;
- o raw pointer do probe não cruza a service ABI;
- `single-process` e `split-services` preservam o mesmo grafo lógico;
- uma call normal ou borrow não cruza uma unit;
- cada packing possui recipe, index e digests próprios;
- deployment não reagrupa providers;
- o registro nomeado `deployments` em `build.w` fixa products, releases, units e adapters;
- `w deploy apply --locked` não executa build;
- secrets permanecem handles de host;
- cada target possui recipe e digest próprios;
- `TargetId` não incorpora CPU, platform contract, SDK ou linker;
- a recipe contém o `TargetSpec` expandido;
- API alcançável acima do platform contract falha sem elevar o minimum;
- cada build phase fixa uma execution platform;
- um build tool é compilado para o target da execution platform;
- a action do build tool executa nessa platform e não no product target;
- o package pede roles de toolchain e não fixa executables por path;
- dependency pode pedir role, mas somente a root policy autoriza language e
  provider source;
- package e dependency não fornecem a toolchain policy do consumer;
- a resolução não consulta `PATH`, `SDKROOT`, `INCLUDE` ou `LIB`;
- SDK de sistema exige import explícito e closure por digest;
- CAS ou snapshot local selado é preferido a provider system-backed;
- provider inventory associa records a execution platforms sem expor paths;
- provider não selecionado não muda a recipe;
- provider ambiguity falha sem uma prioridade da raiz;
- `cpuPolicy: .explicit` exige CPU e features explícitos;
- cada slice de um artifact composto mantém target, plan row e digest;
- signing e notarization geram delivery records separados;
- `bit-reproducible` não implica `publicly-rebuildable`;
- clean, no-op, body edit e matrix build publicam records de desempenho;
- WASI 0.3 preserva async na component boundary;
- uma matriz publica um index, não um hash falso entre architectures;
- `w explain product` informa origem de cada binding;
- importar um entry module não executa ou registra handlers.

O oracle gera products e packings de [`build.w`](build.w). Depois ele resolve
os dois planos nos records nomeados de [`build.w`](build.w). Ele rejeita graph aberto,
binding incompatível, quota maior, unit ausente, digest mutável, provider
ambíguo e target sem SDK.

### 3.37 Coreografia das Luas que Perderam o Planeta

Famílias: units, tensor vectors, services, network concurrency e swarm identity.

`orbit.w` modela um swarm de satélites. Cada device possui `SatelliteId`,
telemetry revisionada e uma capability `ServiceRef<SatelliteApi>`.

Aceite:

- posição, velocidade, duração e distância não se misturam;
- propagação preserva shape `[3]`;
- duas telemetrias usam I/O concorrente e mantêm ordering por source;
- um satellite silencioso não vira zero telemetry;
- sequence stale produz error tipado;
- `ServiceFamilyRef` seleciona a instance pela identidade;
- o solver de aproximação usa um número de samples refinado;
- CPU scalar, SIMD e device produzem o mesmo resultado no mode escolhido;
- deployment pode co-localizar ou separar satélites sem mudar a API.

### 3.38 Observatório do Horizonte que Já Aconteceu

Famílias: event time, observed time, tensor fusion, ranges e numeric modes.

`horizon.w` separa o instante físico do instante de observação. O sensor não
finge que latência de rede altera a ordem causal.

Aceite:

- samples fora de sequence falham antes do tensor kernel;
- valores não finitos não entram no forecast;
- `@` fixa as shapes de calibration;
- redução `.reproducible` mantém o oracle cross-target;
- os ranges de anomaly são exaustivos e não se sobrepõem;
- CPU e accelerator registram diferenças de precisão;
- cancellation devolve buffers e device leases;
- o trace mantém event time, observed time e processing time separados.

### 3.39 A Última Música sem uma Única Allocation

Famílias: fixed arrays, deadline, host entry, real time e resource gates.

`audio.w` renderiza um bloco estéreo fixo. `audio_app.w` liga o callback ao host.

Aceite:

- o callback não aloca, bloqueia, faz I/O ou aguarda task;
- `AudioBlock<frames, channels>` fixa o tamanho no compile time;
- o host passa state exclusivo; não existe mutable global;
- uma falha preenche o bloco com silêncio;
- phase e frame count sobrevivem entre callbacks;
- `w check resources --require no-general-allocation` fecha o call graph;
- o benchmark registra deadline misses, mas métricas não mudam o áudio;
- o target embedded pode usar a mesma função sem runtime de process.

### 3.40 Porteiro do Wi-Fi que Não Conhece a Senha

Famílias: HTTP, secrets, rate limit, durable state e capability attenuation.

`wifi.w` modela login e revogação. O handler recebe uma capability de sessions.
Ele não recebe o secret usado para verificar vouchers.

Aceite:

- request body possui limite antes da decode;
- device ID e voucher possuem refinements;
- rate limit informa `retryAfter`;
- session ID não concede authority sem `WifiSessionApi`;
- logs não contêm voucher, secret ou session token;
- logout é idempotente ou informa outcome desconhecido;
- a session devolve duração restante e não serializa um `Instant` local;
- worker e processo local usam a mesma interface;
- storage durable é adapter do product, não propriedade de `String`;
- o mobile app recebe somente state e notification capabilities declaradas.

### 3.41 Cozinheiro de Silício com Avental Vetorial

Famílias: shape generics, `@`, host/device transfer e kernel bundles.

`ai_harness.w` declara kernels sem esconder placement.

Aceite:

- shapes incompatíveis falham no type checker;
- device memory é capability distinta da memória do host;
- transfer, launch e synchronization aparecem no host graph;
- o kernel não executa network, filesystem ou service calls;
- NVVM, ROCDL e SPIR-V usam a mesma HIR verificada;
- address spaces permanecem explícitos no lowering;
- `.reproducible` e `.fast` possuem oracles diferentes;
- o training step preserva shapes e exige learning rate em `0.0>..<1.0`;
- fallback CPU não altera shape, ownership ou error contract;
- um device bundle registra cada object e target por digest.

### 3.42 Garçom dos Sete Benchmarks

Famílias: HTTP, JSON, database, allocation, admission e performance evidence.

`benchmark_app.w` contém um oracle de source para as sete famílias do
TechEmpower. `build.w` fecha o runtime graph. `build.w (deployment "benchmark")`
seleciona PostgreSQL, cache local e limites menores que o envelope do product.
Essa é uma matriz source-backed para futura composition. Ela continua blocked por
runtime/provider. Não é o workload BMD1, não é um resultado de benchmark e não
cria três variantes artificiais do app.

Aceite:

- plaintext, JSON e as cinco rotas de dados usam o mesmo HTTP runtime;
- o host gera os headers `Server` e `Date` exigidos pelo harness;
- o profile desativa o log de cada request em disco;
- query count fica em `1...500` por narrowing exaustivo;
- `/db`, `/queries` e `/updates` leem rows inteiros da tabela `World`;
- cada item de `/queries` e `/updates` mantém um `SELECT` distinto;
- pipeline pode agrupar transporte, mas não SQL, resultados ou semântica
  transacional;
- o adapter PostgreSQL envia uma boundary `Sync` por statement;
- `/updates` modifica cada valor antes da resposta e confirma a transação;
- `/updates` usa `pipeline<transaction: ...> tx = store` e encerra com `commit`;
- o transaction scope não escapa e só aceita efeitos derivados de `tx`;
- uma perda depois de `COMMIT` gera `unknownCommit`; não há retry implícito;
- fortunes escapam HTML no template adapter;
- `/cached-queries` usa `CachedWorld` e um cache com read-through e eviction;
- `LocalCache` continua process-local; um cache remoto usa uma API async;
- pool, cache, requests ativos, filas e bytes possuem limites;
- nenhuma rota retorna dados constantes quando o workload exige database;
- o gate do codec JSON preserva exatamente os nomes dos fields e aplica seus
  limits por request;
- prepared statements usam parameters nomeados e SQL const;
- configuração, hardware, compiler e artifact digest acompanham o resultado;
- monolito e nanoservices executam o mesmo oracle;
- uma regressão de segurança não é aceita por ganho de throughput;
- ranking é measurement, não promessa da linguagem.

O primeiro harness deve validar payload, headers, query count, SQL observado,
cache hit/miss e ausência de logs antes de medir throughput. Ele também deve
registrar toda diferença em relação à especificação TechEmpower fixada pelo
profile.

### 3.43 Janela que Fala C sem Esquecer W

Famílias: interface semântica, ABI W exata, calling convention C, symbols,
runtime requirements e version skew.

`horizon.w` exporta `classifyHorizon` para a static library W. `abi.w` escreve
em W a façade C `ll_horizon_classify_v1`. `build.w` declara cada boundary sem
dar `entry` ou `host` à library.

Aceite:

- documentation e spans não invalidam `SemanticInterfaceKey`;
- metadata de interface e ABI é lida como input não confiável e bounded;
- a library W publica `WInterface`, `WAbiKey` e `RepresentationMap` separadas;
- requirements alcançáveis produzem `RuntimeClosureKey` fora da `WAbiKey`;
- trocar system allocator por mimalloc preserva o representation fingerprint
  quando os bytes e carriers compartilhados são iguais;
- allocator provider e mode continuam na recipe e no runtime closure;
- reuse exige key igual e mismatch com source causa rebuild;
- `HorizonStatus` usa layout W somente na boundary `.wExact`;
- a façade C aceita e devolve somente carriers C;
- o header gerado compila em Clang, GCC e MSVC;
- o header pertence ao mesmo target slice da library e verifica seu layout;
- `ll_horizon_classify_v1` mantém o nome exato em ELF, PE/COFF e Mach-O;
- score não finito ou negativo vira status distinto no carrier C;
- `panic: .forbid` fecha o call graph exportado;
- nenhuma parte exige que o caller use `free` em memória criada por W;
- module constructors não executam antes da validação da ABI note;
- `runtime: .none` não recebe contexto oculto ou faz lazy init;
- runtime requirements ausentes falham antes da primeira call;
- release do handle `.wExact` fecha novas calls sem desmapear código na v0;
- ThinLTO não muda a interface, o export ou o resultado;
- dois exports C iguais falham antes do linker;
- ordem de load e symbol interposition não mudam uma call W;
- uma dynamic library nativa não é tratada como sandbox.

O caso compara a boundary W exata, a façade C e a component schema. As três
formas resolvem problemas diferentes.

### 3.44 Kernel M1 de HIR para memória e ABI

Famílias: PlaceId, LoanId, overlap, reborrow, OriginSet,
AllocationOriginSet, pinning, shared/weak, representação, FFI e `WAbiKey`.

`hir_memory_oracle.w` é o recorte M1 do verifier. Ele não executa código de
produção. Ele modela as transições que o HIR deve rejeitar ou aceitar.

Aceite:

- somente um owner `owned` pode mover ou destruir;
- PlaceId usa root estável e projections com overlap conservador;
- LoanId registra mode, origin, estabilidade e parent de reborrow;
- fields conhecidos distintos, índices constantes e ranges provados podem ser
  disjuntos;
- ProofFacts identificam o prefixo PlaceId exato e não valem para outro root;
- enum variants distintos e projections opaque continuam conservadores;
- move/drop do root e mutation estrutural observam loans descendants;
- reborrow congela parent e restaura a capability no fim do child;
- cópias shared de um child preservam o parent até o fim de todas as cópias;
- child sibling ou mais amplo é rejeitado e children disjuntos podem coexistir;
- edges individuais compõem stored fields e `Array<ref T>` sem apagar erasure;
- edge shared permite read; edge exclusive permite read/write;
- `.lifetimeIndependent` observa somente ausência de origin dinâmica;
- storage local continua local mesmo quando o payload é lifetime-independent;
- rehome reescreve storage origin sem apagar borrow origin;
- strong zero destrói o payload uma vez e libera o control block se não há weak;
  weak zero libera o block somente depois de strong zero;
- erasure inline preserva origins; spill adiciona box origin sem apagar edges;
- falha consuming de pin/share/rehome/erase não restaura o source;
- service, wire, persistence e FFI aplicam gates próprios depois de lifetime;
- dependent escape, channel, share e await seguem regras de origin e drain;
- pin exige zero loans e separa root pinned de handle móvel;
- self-reference safe initializer é rejeitado;
- FFI ref/inout é call-scoped e retenção exige lease pinned e destroy;
- `lowBit` só é aceito em `internal`;
- `provenNiche` não cruza C, wire ou persistence;
- C e capability usam carriers nativos explícitos;
- um owner que cruza uma boundary precisa de allocator origin conhecido;
- mismatch de target, calling convention, representation policy, runtime ABI ou
  SemanticInterfaceKey rejeita o link antes do lowering.

O modelo Node em `tooling/hir-memory-reference.test.mjs` repete essas regras
com estados pequenos. O corpus M1 em `tooling/memory-transition-cases.json`
possui 185 casos e 606 operações. Ele é uma referência de contrato, não o
futuro verifier.
O compiler deve substituir esse modelo por HIR real no gate SH3/SH4.

### 3.44.0.1 Expressividade de borrow BRX0

`borrow_expressivity.w` é o fixture source-shaped para o estudo R1
[`tooling/studies/r1-borrow-expressivity`](../../tooling/studies/r1-borrow-expressivity).
O checker BRX0 deriva 24 casos: 17 mappings aceitos, cinco rotas Research e
quatro negativos de invocation. Ele cobre callable fresh-loan, cursor lending,
storage reuse, adapters `map`/`filter`/chain, await, closure storage, `any fn`,
boundaries e drift/mutations de witness, implementation, `interface.lock` e
mapping-component digest. O item borrowed do adapter nasce no `next` do
receiver/storage; a factory que recebe `take` publica um owner adapter.

O baseline fecha member receiver, body-derived mapping e bodyless com uma única
entrada compatível. Bodyless free/static/protocol com duas ou mais entradas
independentes é rejeitado com `W-BORROW-0011`, porque não há receiver ou corpo
autoritativo; um aggregate nominal owned continua alternativa explícita de API.
Relation schema owned por requirement/interface permanece candidato BRX2
Research, sem lifetime syntax, GAT, compiler/runtime/provider ou metadata de
lifetime em runtime. Parse Tree-sitter e host oracle são a evidência corrente;
compile, run e estudos humano/modelo permanecem missing.

### 3.44.1 Oracle SHC0 de construção shared

Famílias: binding declarativo, `try` fora do tipo, publicação atômica,
`AllocationOriginMap`, lifecycle strong/weak, allocator profile, rehome,
boundaries, FFI e co-allocation.

`shared_control_oracle.w` é o fixture do restaurante para a construction
contract. `tooling/shared-control-cases.json` possui 45 casos e 84 operações
(16 accepted, 6 error, 3 fault e 20 rejected); `shared-control-machine.mjs` e o teste host são
um oracle independente de M1. Nenhum deles compila, executa ou aloca W.

O oracle distingue contrato rejeitado, `AllocationError`/initializer error,
normal product OOM e compiler invariant. Somente o primeiro é `rejected`.

Aceite:

- `let root: shared T = T(...)` usa o product default sem `try` quando OOM é
  normal; `let root: shared T = try T(allocator: memory, ...)` cobre cada site
  fallible publicado pelo contract;
- a declaração exige binding/field explícito, payload lifetime-independent e
  `take` para owner existente; argumento, return e inference não promovem;
- a admission/open lexical ou custom ocorre antes da construção. `allocator:
  memory` usa a capability/profile já aberta; `try` na construção cobre
  initializer e sites publicados, não `AllocatorPlan.open`. `initializerThrows`
  e o `failure` do site allocator são eixos distintos;
- quando ambos os eixos são fallible, o caller declara `InitializerError` e
  `AllocationError` no error set; se o tipo é o mesmo, as edges colapsam em uma,
  e, se são distintos, o set deve ser exato e sem duplicatas. W não cria uma
  união implícita;
- a pipeline prova facts e entra numa fase lógica de staging/initialization,
  grava `strong = 1`/`weak = 0` e cruza uma fronteira atômica; ordem física e
  co-allocation não são promessa. Falha prepublication limpa cada
  payload/bloco parcial exatamente uma vez;
- `AllocationOriginMap` contém `$storage`, `$controlBlock` e o record do block
  (origin, allocator contract, instance, deallocator, mobility, lifetime,
  adoption family e bulk owner); weak mantém o block e essa origin até weak
  zero, inclusive após strong zero, e strong zero deinit o payload uma vez;
- aquisição weak live cria um owner strong; acquisition após strong zero devolve
  none sem resurrect. Strong-only final release libera o block imediatamente;
- `rehome` unique ocorre antes de shared cross-domain; shared não é rehomable;
  a promoção consome o valor rehomed inteiro (parent/children preservados), e
  nested field calls não herdam allocator;
- FFI persistente exige origin cross-domain, payload shareable, contador
  thread-safe, pin, lease, unregister para fechar admission, drain in-flight,
  destroy, unpin e a ordem `unregister-before-drain-before-destroy`;
- `memory.w::watchClosingBell` constrói `BellLease` e chama
  `ll_bell_unsubscribe`, mas o header não prova o drain. SHC0 exige fact
  explícito de unregister/revoke e drain. A ausência do drain é um caso
  adversarial;
- o close/drain da lease allocator externa pertence ao ASC0. SHC0 só conserva
  o control block até weak zero;
  ciclos fortes fechados continuam rejeitados e edge weak quebra o SCC;
- co-allocation é uma escolha do optimizer, não uma promessa de layout ou
  contagem de allocations.

Os casos usam operations conceituais somente dentro do oracle. O texto source
vigente não possui `share`, `try share`, `tryShare`, `Shared<T>` ou container
nominal público.

### 3.45 Kernel A0 de allocation física

Famílias: layout, receipt de origem, resize, relocation, progress, domain,
retirement e reclamation.

`allocator_oracle.w` é o recorte source do contrato físico. Ele não aloca
memória real. `tooling/allocation-machine.mjs` usa providers pequenos e bytes
host para tornar cada transição observável.

Aceite:

- zero bytes cria `noStorage` e não chama o provider;
- toda allocation não vazia preserva layout, capacidade útil e origem;
- alinhamento inválido ou fora do profile falha antes de publicar storage;
- resize falho preserva receipt, tamanho e prefixo anteriores;
- fallback de resize pertence ao caller;
- loan, pin e address lease impedem relocation incompatível;
- `rehome` consome a origem e cria um receipt do provider de destino;
- failure consuming de `rehome` não restaura a origem;
- allocate e deallocate observam seus domains declarados;
- progress é um fato por operação e não uma escala total;
- bulk release exige drops, loans e leases drenados;
- retirement remove acesso lógico antes de physical reuse;
- o receipt volta somente ao provider que o criou;
- tagged address não representa ownership.

O corpus A0 em `tooling/allocation-cases.json` possui 48 casos e 123 operações.
Treze testes host repetem propriedades críticas sem ler o snapshot. O futuro
allocator e o verifier precisam substituir o modelo antes de alegar execução.

### 3.46 Carrier tensorial PYN4

Famílias: DLPack 1.3 versioned, tensor Device/Queue, layout, dtype, ownership,
capsule one-shot, Python lease, synchronization, cancellation e release.

`tensor_interop.w` é um fixture parseável. Ele importa um tensor científico
trusted com shape `[samples, 6]`, mantém Device e Queue explícitos, executa um
callback scoped async, produz scores e expõe export consuming. A rota separa
materialização e copy-to-host explícitos. O Tensor é o core head; `std.tensor`
somente fornece Device/Queue e adapters. Ela não executa provider nem Python.

Aceite:

- `std.tensor@1` resolve Device provider-scoped e Queue capability opaca;
- provider registration resolve o device kind, dtype mapping, base alignment e
  allocation extent; cada target de transferência tem resolução própria e o
  descriptor raw não concede esses facts;
- CPU rejeita queue extra e device stream exige receipt provider de bindQueue/
  producerWait com happens-before;
- `dlpack.open` publica somente zero-copy e rejeita producer-copied;
- `openDynamic` valida bind exato de dtype, rank, shape e layout;
- `materialize` registra producer copy e W materialization separadamente;
- `ImportedTensor` é owner move-only read-only foreign zero-copy e view não
  escapa do callback;
- `close` drena views, jobs e queue e chama release uma vez;
- `dlpack.export` é writable por baseline, consome owner W e transfere a
  obrigação de release; alias ou borrow event rejeita o export;
- copy-to-host aparece como operação separada;
- DLPack flags, overflow, alignment, provenance, untrusted bytes e mismatch
  de queue/device produzem diagnostics `W-DLPACK-*`;
- major/minor mismatch e capsule destructor seguem release exact-once; records
  de release são separados por carrier generation;
- GIL, attached thread state e interpreter finalization mantêm o lease child em
  open → draining → finalized; drain consome leases e release jobs explícitos e
  late callback vai para quarantine/reject;
- receipt redige raw pointer, capsule address, secret e interpreter pointer.

O host oracle independente está em
[`tooling/dlpack-machine.mjs`](../../tooling/dlpack-machine.mjs), com corpus,
checker, snapshot e testes próprios. Os testes W são puros (shape, score e
provider identity); lifecycle, receipt, limits e device mismatch ficam no host
corpus. Ele não compila ou executa W.

### 3.47 Scope dos Computadores que Cabem em Outros Computadores

Famílias: kernel descriptor, Launch owned, Queue, device memory, submission,
completion receipt, cancellation, fault, limits e equivalência CPU/device.

`device_execution_oracle.w` abre um `accelerator.Launch` para o descriptor
fechado de `ai_harness.w`. O initializer `async` continua sendo a forma estruturada; o
device não acrescenta uma quinta forma de execução. O resultado só chega ao
host por `tensor.transfer` explícito.

Aceite:

- cada field de `accelerator.module<{...}>()` nomeia um kernel e um launch stub
  tipado; o descriptor é `const` de module scope e aceita somente símbolos
  diretos sem capture, suspension ou failure;
- cada especialização alcançável normaliza argumentos de tipo e valores const,
  possui identity e artifact; instances ausentes e JIT runtime falham, enquanto
  instances não usados são removidos;
- `Launch` pertence a module, Queue, Device, provider generation e Limits;
- `take`, `copy`, `ref` e `inout` preservam owners e loans durante staging;
- cancelamento pré-submit impede o launch; pós-submit aguarda provider drain;
- submit e completion receipts são cunhados pelo provider. O caller não declara
  `ready` ou uma ordem entre queues;
- device loss fecha admission, drena ou põe storage vivo em quarantine e
  suprime completion stale;
- host read e fallback CPU nunca inserem transfer oculta;
- `.strict`, `.reproducible` e `.fast` mantêm critérios distintos de
  equivalência.

O oracle host fica em
[`tooling/device-execution-machine.mjs`](../../tooling/device-execution-machine.mjs),
com corpus, checker, snapshot e teste independentes. Ele não executa W, kernel,
driver ou provider.

O oracle KM0 em
[`tooling/kernel-module-machine.mjs`](../../tooling/kernel-module-machine.mjs)
fecha a síntese antes de DEV0. DEV0 recebe a `ModuleIdentity` validada e não
redefine source shape ou especialização.

## 4. Alternativas visuais obrigatórias

O Book deve mostrar pares lado a lado:

| Tema | Forma vigente | Contrafactual |
|---|---|---|
| entry anônimo | `entry(run)` | `entry(args, ctx) { ... }` ou `process.main = run` |
| host callback | product usa `hostBindings` | assignment no source ou registro em runtime |
| handler default | `entry Name(run)` | escrever o nome do slot no source |
| seleção | product escolhe descriptor no link | nome de entry escolhido livremente no runtime |
| multimodo | um `process.main` escolhe CLI/TUI/server | vários mains ou OS chama `http.fetch` |
| target | `TargetId` + `TargetSpec` expandido | string livre, OS apenas ou backend implica suporte |
| build platform | target spec e execution platform separados | compiler executa no target final ou usa host implícito |
| toolchain | requirements + providers por digest | executable em `PATH` ou path no package |
| SDK | system import explícito + closure | SDK mais novo instalado |
| envelope | payload, unsigned envelope e delivery records | assinatura dentro da compilation recipe |
| matriz | payload e digest por target + index | um hash para bytes de architectures diferentes |
| library W | `.wExact`, key e layouts compartilhados iguais | static ou dynamic torna a ABI estável |
| export C | `export unsafe fn<abi: .c>` com body W | `fn<C>` ou mangling W |
| plugin isolado | process ou component schema | dynamic library nativa como sandbox |
| unit | `9.81<m/s^2>` | `9.81[m/s^2]` |
| domain | `let x = spawn<.compute> ...` ou `let x = spawn<domain: .compute> ...` | `spawn on .compute let x = ...` (**Retirado antes do 1.0**) |
| domain relacional | `let x = spawn<.compute> ...` | `spawn on .compute let x = ...` (**Retirado antes do 1.0**) |
| domain customizado | `module execution<domains: [...]>` e `spawn<.thermal>` | enum manual ou string |
| execution profile | product escolhe `executionProfile`; deployment só reduz | import cria pool ou deployment troca domain |
| urgência de execução | deadline + service isolada + admission/reserva/budget; domain conforme placement | `priority`, `qos`, domain como safety, `.background` ou `Task.withPriority` |
| trabalho longo | `fulfillment.tryStart(input:)` por `WorkKeyRef` | `spawn<owner:>` ou Promise solta |
| binding singular | `lastLight.menu()` pela service importada | lookup runtime por string |
| binding keyed | `orderCoordinators.at(orderId)` | singleton global ou key inferida |
| identity keyed | `ServiceIdentity<OrderId>` + `WorkKeyRef` | primeiro argumento redefine a instance |
| progress | `WorkSnapshot<ServiceStage>` revisionado | borrow do task frame ou event list ilimitada |
| cancelamento remoto | `WorkRef<[.observe, .cancel]>` | todo observer cancela ou Boolean runtime |
| signal remoto | `WorkRef<[.observe, .signal]>` + event binding | ID concede authority ou event string global |
| workflow durável | points fechados, replay verificado e effect policy | persistência automática do frame async |
| timer durável | `work.sleep(.point, for:)` | manter worker ou recalcular deadline |
| evento durável | binding tipado + `EventId` + inbox bounded | string, payload global ou fila ilimitada |
| deployment | manifest separado ligado ao artifact digest | rebuild por ambiente ou config invisível |
| mobilidade | facts inferidos `transferable`/`shareable` | protocols `Send`/`Sync` ou `Sendable` |
| constraint de mobilidade | `T<(.transferable)>` | `T: Send` e `<mobility: .transferable>` |
| refinement | `T<(predicate)>` | `T where (predicate)` |
| receiver | `String<(.count <= 40)>` | `String<(value.count <= 40)>` |
| generic refinado | `Array<u8><(.count <= 64)>` | `Array<[u8, (.count <= 64)]>` |
| enum subset | `ServiceStage<[.preparing, .serving]>` | enum base + guard runtime |
| typestate | `OvenSession<.ready>` + `take fn` | state keyword, annotation ou mutation do tipo no lugar |
| protocol composition | `T: Display & Equatable` | postfix `where`; static list de protocols |
| runtime reflection | `T: Reflectable` + `info of T` | metadata universal e annotations |
| type identity | `type of T` e `type of value` | `typeof`, metatype e dynamic construction |
| synthesis | conformance no type head | `@derive` e user macro |
| rest | `T...` + `each values` | Array obrigatório e heterogeneous pack |
| retorno fluente | `mut fn advance(...): self` | retorno `self` implícito |
| receiver consuming | `take fn` + `(take value).method()` | consumo implícito e free function |
| falha consuming | owner termina em success, error e cancellation | restaurar owner no `catch` |
| associated member | `Money.zeroCredits` | mutable type storage |
| struct de dados | fields herdam a visibilidade do tipo | `export` em cada field |
| object | storage encapsulado e API explícita | todos os membros herdam `export` |
| protocol witness | herda o requirement | repete `export` na implementação |
| destructuring | `Type(field, field: pattern, ...)` | `{field}` e tuple posicional |
| evolução de pattern | `...` externo explícito | exaustividade aberta implícita |
| construção | `Type(field: value)` | `new Type(...)` e `Type {...}` |
| overload | labels e aridade antes dos tipos | ranking por tipos ou nomes únicos |
| initializer | vários `init` com formas disjuntas | um `init`, `init?` e `async init` |
| delegação de initializer | `self = Type(...)` | `self.init(...)` e factory obrigatória |
| computed property | `name: T { get => value }` | getter method e getter com efeitos |
| static record | `<{name: value}>` | extensão universal de tipo |
| static list | `<[a, b]>` ordenada | set implícito de constraints |
| callable concreto | `some fn(A): B` | todo callable apagado |
| callable apagado | `any fn(A): B` | `CallbackType` universal |
| callable mode | `fn` / `mut fn` / `take fn` | `Fn` / `FnMut` / `FnOnce` |
| frontend inline | `fn<C>` ou `fn<lang: .c>` | schema sem o slot opcional ou label incompatível |
| matrix | `[[1, 2], [3, 4]]` | `[1 2; 3 4]` |
| closure | `(x) => body` | `fn(x) { body }` |
| namespace import | `import http from std` ou `import stdHTTP from std.http` | default export ou `as` externo |
| allocator block | `allocator scratch: .fixed<capacity: N> { ... }` | scope lexical; `Arena` é só lowering interno |
| projeção borrowed | `view T` para famílias core | `StringView`/`Slice<T>` públicos e `Readonly<T>` profundo |
| stream assíncrono | `Stream<Item, Failure>` single-pass | sequence + iterator obrigatórios ou generator |
| loop de stream | `for try await item in stream` | `await stream` lê tudo ou callback push |
| item borrowed | `Stream<view String, E>` com provenance | `StringView` owned ou view transferable |
| channel | MPSC bounded com endpoints separados | bidirecional, MPMC ou unbounded por default |
| endpoint | `Channel<T><.send>` / `<.receive>` | `Sender<T>` / `Receiver<T>` ou direção runtime |
| falha de envio | enum devolve `T` | Boolean, panic ou perda do item |
| close de channel | último sender ou receiver gracioso; drop do receiver aborta | qualquer sender fecha globalmente |
| prefetch | adapter `buffer(capacity:)` explícito | watermark na assinatura ou buffer invisível |
| distribuição de trabalho | pipeline tasks/Stream adapter bounded ou mailbox de service | `WorkQueue` MPMC universal |
| fan-out | `tee` estático ou service com policy por subscriber | `Broadcast` com lag/replay implícitos |
| state mais recente | `SnapshotCell` e notification específica | `Watch` que esconde conflation e lifecycle |
| quota por recurso | mailbox com authority | `WeightedChannel` chamado de limite de memória |
| byte I/O | `ByteSource`/`ByteSink` async-first | `Reader`/`Writer` por backend ou interface sync condicional |
| destino de read | append em `Bytes` com spare privado | `ReadBuffer` público ou `inout view Bytes` genérico |
| EOF | `ReadStep.data(positive)` / `.end` | zero bytes e Boolean adicional |
| arquivo seekable | `read(at:)` posicional por default | cursor compartilhado e lock invisível |
| I/O blocking | adapter em executor bounded | bloquear worker cooperativo ou pool ilimitado |
| gather write | `writeMany(view Bytes...)` com fallback | `IoSlice` público, concatenação ou erro sem backend |
| scatter read | `ReadBatch` owner + `readMany`, initialized prefixes e fallback de um read | `inout view Bytes...`, `IoSliceMut` ou probe runtime |
| file-to-sink | `TransferPlan` + `io.transfer`, scratch/progress bounded e native choice explicável | `sendfile`/`mmap` invisível ou promessa universal de zero-copy |
| construção textual | reserve/append no próprio `String` | `StringBuilder` público |
| storage textual | owner único flat + SSO invisível | COW baseline, rope universal ou threshold público |
| reserva textual | `tryReserve(minimumBytes:)` | capacity property e growth factor fixo |
| esvaziar texto | `clear()` / `reset()` / `takeAll()` | `clear(keepingCapacity: Bool)` |
| storage atômico | `var atomic value: T` | wrapper obrigatório ou behavior `Atomic` |
| order atômica | `load<.acquire>()` | `load(order:)` runtime e relaxed default |
| compare-exchange | enum result e orders estáticas | Boolean e combinações runtime |
| exclusão residual | `lock`/`await lock`/`try lock` scoped | wrapper Mutex, unlock manual ou guard público |
| state de task | domain/service por default; `await lock` é fallback | AsyncMutex e guard mantido através de `await` |
| RCU | tipo especializado após prova de reclamation | policy automática por property |

Preferência visual não é medida antes das tarefas de leitura e correção.

## 5. Gate para uma implementação

Cada arquivo W precisa passar:

1. Tree-sitter sem error node;
2. formatter duas vezes sem diff;
3. highlighter com keywords e units corretas;
4. corpus negativo por feature;
5. type-check quando a fase correspondente existir;
6. runtime test ou oracle explícito quando houver lowering;
7. origem e revision disponíveis para geração do Book após o design freeze.

A integração avança em sete gates cumulativos:

1. **Simulação:** `LastLightSimulation` gera os três relatórios sem deployment;
2. **Host:** CLI, TUI, line host e HTTP chegam ao mesmo `Command` e
   `AppResponse`;
3. **Turno do Horizonte Violeta:** o grafo real de services, FFI, compensação e
   observabilidade passa fault injection;
4. **Products:** package/workspace resolution, toolchain plan e build geram artifacts
   reproduzíveis por target;
5. **ABI:** `WInterface`, `WAbiKey`, symbols, header C e runtime requirements
   passam o laboratório do horizonte;
6. **Devices:** mobile, firmware, áudio e accelerator passam os próprios
   resource gates;
7. **Performance:** benchmarks internos e externos mantêm semântica e evidence.

No estado atual, somente o gate sintático do Tree-sitter é executável. Os outros
gates são contratos de implementação. A documentação não os apresenta como
testes aprovados.
