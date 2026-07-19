# Arquitetura de módulos e runtime de instâncias

> **Status:** arquitetura candidata para experimentos; não é uma implementação
> **Data:** 19 de julho de 2026

Este documento transforma a semântica de
[spec/modules.md](../spec/modules.md) em componentes, boundaries e experimentos.
Ele complementa o [runtime do compilador](compiler.md), a
[concorrência estruturada](../spec/concurrency.md), a
[stdlib](stdlib.md) e os [contratos de serviço](../ecosystem/services-and-protocols.md).

## Objetivo e não objetivos

O objetivo é tornar fácil escrever uma unidade stateful previsível sem confundir:

1. módulo estático de source/build;
2. instância runtime com lifecycle;
3. package e seus artefatos distribuídos.

O primeiro runtime deve demonstrar exclusão lógica, tasks estruturadas,
backpressure, falhas e capabilities. Não precisa criar uma plataforma serverless,
um protocolo de internet próprio, um sandbox universal ou um banco obrigatório.

## Visão em camadas

```text
package.w + package.lock
        │ resolve source, tools, artifacts e policy
        ▼
grafo de módulos estáticos
        │ interfaces/HIR/MLIR + metadata reproduzível
        ▼
objetos / archives / dynamic libs / executável / futuro WASM
        │ host carrega o programa e descriptors autorizados
        ▼
instance manager ── service instances ── typed calls/events
        │                  │
        │                  ├─ serial executor + task tree
        │                  ├─ mailbox + budgets
        │                  ├─ capabilities/adapters
        │                  └─ durable store opcional
        ▼
processo / container / VM / host WASM conforme threat model
```

Uma seta indica transformação ou configuração, não identidade. Um módulo pode
ser fundido com outros no mesmo objeto; um package pode conter vários módulos; e
um único tipo exportado pode originar milhares de instâncias keyed.

## Fine-grained compute e “nanoservices”

### Unidade lógica

A direção [W-D014](../STATUS.md) trata fine-grained como uma propriedade do
modelo lógico, não como topologia fixa. A unidade stateful endereçável é
`(serviceType, scope, logicalKey)`; cada evento aceito é um turn com root task,
deadline e outcome. Isso permite dividir estado por usuário, documento, job ou
outro domínio natural sem promover toda função a serviço.

“Nanoservice” é um nome de trabalho para essa lente. Não é keyword, formato de
artefato, categoria de package nem garantia de isolamento. A
[discussão que inspirou a frase](https://news.ycombinator.com/item?id=31759801)
fala em fine-grained compute e em chunks pequenos de trabalho; ela não usa o
termo “nanoservice”.

A origem primária adicional é o [post do
`workerd`](https://blog.cloudflare.com/workerd-open-source-workers-runtime/):
ali, nanoservices são componentes de funcionalidade implantáveis
independentemente, enquanto o runtime reduz overhead co-localizando muitos
Workers e executando calls explícitas na mesma thread/processo. O texto também
prefere unidade lógica de funcionalidade a uma regra sintática “uma função = um
serviço”. Isso calibra bem a lente de W, mas não importa isolates JavaScript,
deployment homogêneo nem uma promessa de custo semelhante a library call.

O debate no Hacker News evidencia ainda que granularidade e security boundary
são decisões relacionadas, mas diferentes. W preserva a inspiração, não números
ou conclusões de plataforma como fatos universais.

Na decomposição W, o componente mais próximo da unidade independently deployable
do `workerd` é `serviceType + implementation descriptor`. As muitas instâncias
keyed desse tipo compartilham normalmente a mesma versão/artefato; cada key não
vira package, release ou deploy separado. Isso preserva a fronteira entre
package, implementação e instância runtime.

Cada unidade granular útil deve responder sem depender da colocação física:

| Eixo | Contrato lógico |
|---|---|
| endereço | service type, key/scope e generation observáveis |
| interface | entradas, saídas, typed errors e schema versionado |
| lifecycle | start, ready, drain, stop, failure e restart |
| concorrência | turn/reentrância, task tree, ordering e backpressure |
| state | owner, durable adapter opcional e commit semantics |
| authority | capabilities mínimas e trust domain declarado |
| resources | budgets e attribution por instância/turn |
| observabilidade | IDs lógicos estáveis em trace, metric e diagnostic |

Esse contrato pequeno é a abstração. `service`, `ServiceRef` e turn devem resolver
os casos antes que W crie famílias distintas de actors, workers, computer units,
isolates e nanoservices. Complexidade aceitável fica nos verificadores, runtime e
gates; a interface do programador permanece pequena e previsível.

### Packing físico e regra *as-if*

O host escolhe uma implementação compatível com trust, target e workload:

| Unidade lógica | Implementação física possível |
|---|---|
| turn stateless curto | call inline/coroutine no mesmo worker |
| muitas instâncias keyed confiáveis | state/mailboxes separados sobre o mesmo executor/processo |
| calls estruturadas e próximas | stub monomorfizado, batching ou pipeline local |
| vários namespaces no mesmo durable adapter | conexão/log compartilhado com isolamento lógico |
| código de outro trust domain | worker process, VM ou futuro host WASM |

O `workerd` demonstra uma realização específica das linhas co-localizadas: muitos
Workers no mesmo processo, isolates separados e calls explícitas na mesma
thread. W precisa medir sua própria combinação de código nativo, ownership e
runtime; o exemplo é inspiração arquitetural, não um backend escolhido.

Compiler/runtime podem co-localizar, inlinear, batch, coalescer wakeups/storage e
colapsar boundaries apenas quando uma prova/teste diferencial preserva:

- identidade e authority de `ServiceRef`;
- exclusão/reentrância e ordem de admissão;
- mailbox, quotas, backpressure e attribution;
- `await`, cancellation tree, deadline e cleanup;
- typed errors, unknown outcome e failure boundary contratada;
- confirmação de durable writes antes dos outputs exigidos;
- spans/metrics da unidade lógica, ainda que a transição física desapareça.

Uma otimização não pode atravessar uma boundary declarada de segurança, dar
borrow do state de outra instância ou converter overload em fila ilimitada.
Inlining não remove `await` do tipo público; batching não inventa atomicidade;
co-location não transforma IPC potencial em call infalível.

Fine-grained tampouco exige um processo/library por função, RPC em tudo ou
microservices distribuídos. Esses mapeamentos são **Rejeitados por enquanto** como
defaults: impõem overhead/topologia sem benefício semântico e confundem build,
runtime e deployment.

### Risco de granularidade excessiva

Separar demais pode produzir:

- metadata, mailbox, task roots e buffers fixos para unidades quase vazias;
- routing, serialization e tracing com cardinalidade desproporcional;
- N+1 calls, ciclos/deadlocks e tail latency acumulada;
- backpressure em cascata e retries multiplicados;
- transactions fragmentadas entre owners, exigindo coordenação distribuída;
- cold starts, stores, connections ou arquivos demais;
- observabilidade cara e difícil de interpretar.

O compiler não deve fundir silenciosamente identidades duráveis ou capabilities
para “consertar” um desenho granular demais. Tooling pode apontar chatty calls,
unidades sem estado/lifecycle próprio e keys quentes, sugerindo inline, batch ou
reagrupamento sob decisão do programador/deployment.

### Métricas e gates

Nenhum número fixo é contrato antes dos workloads. Cada experimento compara a
mesma semântica numa unidade maior e em várias instâncias keyed, variando packing
e cardinalidade. Deve medir:

- CPU por evento e throughput;
- bytes residentes por instância idle/ativa e peak total;
- start, wake, eviction/drain e restart latency;
- queue depth, tempo de espera, rejeições e p50/p99 end-to-end;
- calls/turn, hops, bytes serializados e taxa de batch/inline;
- transactions, confirmações e connections/arquivos físicos;
- tempo de cancellation/cleanup e blast radius de falha;
- cardinalidade e volume de traces/logs/metrics.

Uma estratégia de packing só vira default quando passa testes diferenciais de
ordering, errors, cancellation, durable output e capabilities, melhora o
workload-alvo e mantém memória/tail latency dentro de um budget documentado. Se
o runtime não consegue amortizar o custo, o profiler deve mostrar a boundary; o
overhead nunca é declarado “zero” por ser syntax sugar.

## Artefato de módulo

### Interface compilada

Para type checking incremental, cada módulo produz uma interface versionada com:

- identidade lógica e fingerprint do source público;
- símbolos exportados e identidades com detecção de colisão;
- tipos, generics, layouts estabilizados e calling conventions relevantes;
- ownership, effects, typed errors e requisitos de sendability;
- imports de interface e motivos de recompilação;
- descriptors de FFI e serviços gerados, quando presentes;
- versão do compiler, schema e target constraints.

AST/HIR/MLIR serializados e a interface são caches. O build os invalida quando
qualquer input semântico muda. O formato exato continua ligado a
[W-O012](../STATUS.md).

### Unidade de compilação não é formato de distribuição

O frontend pode analisar por módulo e o backend pode emitir um objeto por módulo
para simplificar o protótipo. Isso não obriga releases a preservar a fronteira:

| Forma | Uso provável | O que não garante |
|---|---|---|
| objeto relocatable | cache e link incremental | distribuição ou ABI estável |
| archive static | deployment simples e otimização | lifecycle ou isolamento |
| dynamic library | plugin/ABI de plataforma/upgrade separado | segurança entre componentes |
| source snapshot | rebuild e auditoria | toolchain compatível por si só |
| interface/metadata | type checking e tooling | código executável |
| WASM/component futuro | playground/plugin/deployment específico | semântica de browser ou substituição de JS |

LTO, dead stripping e deduplicação podem apagar a correspondência byte-a-byte
entre módulo e trecho do executável. Provenance e SBOM devem manter o grafo
lógico, em vez de depender de reconhecer assinaturas depois do link.

## Descriptor de instância

O build ou deployment materializa um descriptor canônico, separado do source da
interface:

```text
serviceType       identidade + versão/schema
implementation    digest + compatibility key
scope             process | request | keyed(...) | deployment-specific
entrypoints        operations/events tipados
concurrency        serial; reentrancy policy; mailbox policy
capabilities       handles/grants mínimos
resources          memory, CPU, I/O, calls, deadlines
storage            none | adapter + schema/migration
failure            drain, panic, restart e retry policies
observability      logs, metrics, traces e redaction
```

Source pode fornecer defaults verificáveis, mas deployment limita autoridade e
resources. Um pacote não aumenta grants de uma aplicação apenas por declarar que
os deseja.

## Instance manager e identidade

O instance manager:

1. valida descriptor, compatibilidade e grants;
2. resolve uma chave lógica para uma instância existente ou nova;
3. cria allocator/budget, mailbox, task root e adapters;
4. executa inicialização antes de publicar o handle;
5. roteia calls para a geração ativa;
6. coordena drain, shutdown, falha e restart;
7. registra transições para tracing/audit.

Identidade conceitual:

```text
InstanceId = (serviceType, scope, logicalKey)
InstanceGeneration = (InstanceId, generation)
```

O primeiro protótipo deve suportar `.process` e `.key(value)`. `.request` pode
ser representado por ownership lexical sem registry. Um singleton global ou
cross-region exige consensus/routing e permanece fora do core.

### Máquina de estados

```text
declared → starting → ready → draining → stopped
               │         │         │
               └─────────┴─────────┴→ failed → starting(new generation)
```

- `starting`: não aceita eventos; erro não publica uma instância parcial.
- `ready`: aceita até os limites da mailbox.
- `draining`: rejeita novas entradas, aplica deadline e conclui/cancela roots.
- `failed`: invalida o estado em memória e registra outcome das calls pendentes.
- `stopped`: executou cleanup observável; durable state não é apagado salvo ação
  administrativa separada.

Restart nunca reutiliza borrows, ponteiros ou task frames da geração anterior.

## Executor serial por instância

### Baseline de protótipo

Cada instância possui um **strand** lógico: um handler externo por vez, do início
à conclusão. O strand pode migrar entre threads; exclusão lógica não é thread
affinity.

Dentro do handler:

- `async let` cria children concorrentes no mesmo scope;
- completions retomam o handler pelo strand;
- `spawn let` envia trabalho `Send` a um pool paralelo limitado;
- estado mutável da instância não pode ser capturado por `spawn`;
- calls a outras instâncias são async e canceláveis;
- o próximo evento externo só entra quando a root atual conclui.

Múltiplas instâncias fornecem o paralelismo natural. Um serviço keyed pode dividir
estado por usuário, documento ou shard; uma operação CPU-bound usa `spawn` sem
liberar acesso compartilhado ao estado.

### Custos e riscos

O default serial reduz interleavings, mas traz:

- head-of-line blocking quando um handler aguarda I/O lento;
- ciclos A → B → A capazes de deadlock;
- uma instância hot limitando throughput;
- necessidade de deadline e admission control desde o início.

O runtime deve detectar self-call síncrona via `ServiceRef` e sugerir chamada
interna normal. Tracing registra queue time, handler time, suspension time,
children e dependências entre instances. Deadlock distribuído não é prometido
como decidível; timeouts e design acíclico continuam necessários.

## Reentrância e gates

Três políticas merecem comparação; somente a primeira entra no protótipo zero:

| Policy | Entrega outro evento durante `await` | Benefício | Risco | Status |
|---|---:|---|---|---|
| closed turn | não | invariantes locais simples | head-of-line/deadlock | **Candidato** |
| storage input gate | somente fora de uma janela protegida de storage | mais throughput | proteção parcial e semântica causal complexa | **Pesquisa** |
| reentrant | sim em suspension points autorizados | utilização/latência | estado muda sob o handler | **Pesquisa** |

O artigo de Durable Objects mostra por que single-thread não basta: `await` pode
permitir interleaving. Seus *input gates* adiam eventos enquanto storage está
pendente, mas duas operações iniciadas pelo mesmo evento ainda podem concorrer.
W deve representar essa diferença em testes, não rotular todo `await` como seguro.

Um eventual bloco reentrante precisaria invalidar/refazer borrows do estado,
declarar invariantes e impedir que valores transitórios atravessem a abertura do
gate. A sintaxe fica **Em aberto** até a análise de ownership provar um modelo
útil.

### Output gate

Um output gate associa writes ainda não confirmadas às saídas externas causadas
por elas. A saída fica retida; se a confirmação falha, ela é descartada, a call
falha e a instância pode reiniciar. Isso pode ocultar latência de durability sem
mentir para o caller.

É uma hipótese **Pesquisa**, porque o runtime precisa provar causalidade:

- quais writes influenciaram qual response/request;
- comportamento com child tasks e mais de um store;
- limite de outputs retidos e backpressure;
- cancelamento antes/depois do commit;
- calls externas não idempotentes iniciadas antes da confirmação;
- shutdown e falha do host.

O MVP usa uma regra explícita e mais conservadora: `commit` confirmado antes de
liberar a response. Um outbox transacional é uma alternativa para mensagens.

## Mailbox, ordering e backpressure

Uma mailbox é limitada simultaneamente por itens, bytes e trabalho em voo. O
descriptor define os máximos; o deployment só pode reduzi-los.

Fluxo de admissão:

```text
resolve handle → validate schema/authority → reserve quota → enqueue
      → execute root → release quota → response/error
```

Antes de reservar quota, o runtime valida tamanho de frame, profundidade e
deadline para evitar allocation descontrolada. A API oferece:

- call que aguarda espaço, com cancellation/deadline;
- `tryCall`/equivalente que retorna overload imediatamente;
- policy administrativa para rejeitar durante drain.

Ordem candidata é FIFO por `(sender, instance)` na admissão. Não há ordem global
entre senders. Prioridade nunca pode causar starvation silencioso. Batching,
watermarks, fairness e reorder explícito precisam de scheduler determinístico de
teste.

## Structured calls e RPC

O descriptor de interface gera um stub e um dispatcher a partir do mesmo schema.
O primeiro profile é unary e in-process, mas mantém as categorias necessárias
para atravessar IPC depois:

```text
callId, serviceId, generationHint, operationId, schemaVersion,
deadline, cancellationId, callerCapability, payload
```

O fast path pode mover valores diretamente quando os dois lados compartilham
ABI e trust domain. Ele ainda respeita:

- `await` no source;
- isolamento de ownership e ausência de borrows entre instâncias;
- limites e admission control;
- typed application errors separados de transport/runtime errors;
- tracing e política de cancelamento;
- mesmo ordering observável do caminho serializado.

O caminho IPC serializa DTOs e valida de novo no receiver. Uma operação aceita
pode terminar com outcome desconhecido se o transporte cair depois da entrega;
o runtime não inventa exactly-once.

### Capabilities remotas

`ServiceRef<Api>` é simultaneamente referência e authority. Delegá-la concede
somente as operações/type bounds da interface. O runtime precisa de:

- IDs não forjáveis e vinculados ao principal/session;
- attenuation para subinterfaces ou methods permitidos;
- revogação/expiração conforme profile;
- lease ou protocolo de release para referências remotas;
- defesa contra confused deputy;
- limites de export table e cycles.

Cap’n Web demonstra que referências remotas e promise pipelining podem compor
calls dependentes em um round trip. Para W, o experimento deve usar uma pipeline
estruturada e one-shot, não mudar a regra de que uma chamada async não vira
Promise implícita. Cancellation, partial failure e liberação de referências são
gates obrigatórios que o artigo não resolve por W.

## Durable adapter

O runtime define um contrato, não um banco:

```text
begin(mode, deadline) → transaction
transaction.read/write/delete/scan
commit → confirmed revision
rollback
snapshot/restore/migrate (capabilities administrativas)
```

O adapter declara isolation, durability, tamanho/concorrência, se calls são sync
ou async e como cancellation funciona. O service profile decide se um handler
mantém uma transaction por turn, cria transactions explícitas ou não usa storage.

### SQLite candidato

SQLite é atraente para uma instância keyed porque oferece transactions, WAL,
índices e uma biblioteca embeddable. O primeiro adapter deve:

- manter connection e schema pertencentes à instância/host definido;
- usar prepared statements e limits;
- versionar migrations como inputs do build/deploy;
- testar crash entre write, commit e response;
- medir um arquivo por instância versus sharding/connection pool;
- definir backup, corruption recovery e observabilidade;
- não assumir que “sync” significa rápido em todo storage/target.

O desenho SQLite de Durable Objects executa a biblioteca na mesma thread e usa
um sistema externo de logs/replicação e output gates para confirmar writes. Essa
combinação é inspiração de produto, não uma propriedade que incluir SQLite
sozinho entregue ao W.

Memory KV serve como oracle determinístico inicial. SQLite pode ser a segunda
implementação para provar que a interface não codificou acidentalmente um banco
específico.

## Memória por instância

O instance manager pode instalar um `AllocatorRef`/região e contabilizar bytes
por instância. O compiler continua responsável por ownership, borrows e drops;
o budget não os substitui.

Matriz de experimentos:

| Estratégia | Caso | Métrica/gate |
|---|---|---|
| allocator default do host | baseline | throughput, peak, fragmentação, cleanup |
| mimalloc adapter | long-lived/multithread host | ganho medido em ao menos dois workloads/targets |
| região por handler | request/batch | escapes diagnosticados, cancelamento e destruction |
| região por instância | state keyed | retenção, restart e migração de valores |
| `shared T`/ARC | graph/callback | cycles, atomics ao cruzar `spawn`, FFI |
| tagged representation | metadata/Option | fallback, sanitizers, ABI e target capabilities |

Alocar tudo numa heap de módulo é **Rejeitado por enquanto**: lifetime estático e
runtime não coincidem. Uma região vinculada à instância é válida como policy
explícita, não como consequência de import.

## Capability-first e sandbox por target

### Contrato portátil

Todo acesso externo passa por handles concedidos: `FileRoot`, `Network`,
`Clock`, `Entropy`, `Process`, `DurableStore` e `ServiceRef` são exemplos
conceituais. O runtime pode:

- negar capabilities ausentes antes do start;
- limitar paths/endpoints/operations;
- contabilizar uso e aplicar quotas;
- registrar grants na receita/deployment;
- substituir adapters por doubles determinísticos em teste.

Isso melhora least authority e testabilidade em safe W. Não é sozinho um
sandbox contra código nativo hostil, FFI unsafe ou exploit de memória no mesmo
processo.

Capability bindings do `workerd` são uma referência direta para conectar
componentes sem namespace global de rede. Ao mesmo tempo, o
[README do runtime](https://github.com/cloudflare/workerd) declara que `workerd`
sozinho não é um sandbox endurecido e recomenda uma sandbox segura adicional
para código possivelmente malicioso. O W mantém exatamente essas duas perguntas
separadas: “a que este handle dá authority?” e “qual boundary contém um escape?”.

### Boundaries de enforcement

| Boundary | O que oferece | Limite |
|---|---|---|
| mesma process/library | type system + API capabilities contra erro acidental | nenhuma contenção contra native code arbitrário |
| processo separado | address space e handles concedidos pelo OS | kernel e IPC entram no TCB; policy varia por target |
| processo + hardening Linux/seccomp | reduz syscalls alcançáveis | seccomp não controla toda lógica/fluxo e não é sandbox completo |
| container/VM | boundary operacional mais forte conforme implementação | custo, startup e integração por plataforma |
| runtime WASM verificado | memória/control flow/imports restritos pelo engine | engine/host são TCB; target futuro, não native W irrestrito |

Para código W nativo não confiável, o baseline de segurança deve ser processo ou
VM. O mesmo runtime pode agrupar instâncias confiáveis in-process por performance,
mas precisa chamar isso de trust domain, não isolamento.

### Por que seccomp não é “por módulo importado”

O projeto `cloudflare/sandbox` facilita filtros seccomp em executáveis Linux,
inclusive via `LD_PRELOAD` ou launcher. O próprio repositório depende de Linux e
libseccomp; o launcher static usa `ptrace`/capabilities privilegiadas. A
documentação do kernel afirma explicitamente que syscall filtering não é um
sandbox e recomenda combinar hardening/LSM para política e information flow.

Uma static library é incorporada ao processo; uma dynamic library carregada
compartilha suas syscalls e memória. Seccomp filtra a task/processo, não identifica
“qual library fez a call”. Aplicar uma allowlist estreita depois de carregar uma
unidade também restringe o restante do processo e não cria memória isolada.

Uso candidato correto:

1. agrupar uma ou mais instâncias com o mesmo trust/capability profile num worker
   process;
2. carregar runtime e dependências necessárias;
3. remover handles/privileges e aplicar limites do OS;
4. instalar seccomp como defense-in-depth no Linux;
5. comunicar por structured RPC;
6. matar/recriar o worker na violação ou falha.

Windows, macOS e outros targets precisam de adapters próprios e testes de
equivalência de **capability outcome**, não da mesma lista de syscalls. Inventar
um sandbox nativo inteiramente em user space equivale a criar uma VM/verifier;
pode ser pesquisa futura, mas não elimina o threat model do OS por criatividade
de API.

## WASM e playground

WASM é um target **Pesquisa** útil para executar exemplos do portal com limites
e imports explícitos. O plano não depende de expor browser APIs:

```text
source W → HIR/W dialect → lowering WASM → host do playground
                                      └→ imports mínimos de console/time opt-in
```

O host fornece capabilities pequenas, deadline, fuel/epoch ou mecanismo
equivalente do engine, limite de memória e captura de output. Network e
filesystem ficam negados no playground inicial.

Wasmtime documenta memória linear bounds-checked, control flow tipado e interação
externa apenas por imports/exports, além de defense-in-depth. Isso torna um engine
WASM candidato real de sandbox; ainda exige pin, updates, threat model e resposta
a vulnerabilidades. Não muda a decisão de não substituir JavaScript no browser.

## Falhas, restart e observabilidade

### Categorias

| Falha | Resultado da call | Estado da instância | Retry |
|---|---|---|---|
| error de aplicação | typed error | preservado conforme handler | somente caller/policy explícita |
| cancellation antes de commit | canceled | cleanup/rollback conforme adapter | explícito |
| deadline após efeito desconhecido | unknown outcome | pode estar ativo | apenas se idempotente/deduplicado |
| panic safe | runtime failure | generation termina conforme profile | restart do host, não replay cego |
| memory corruption/native crash | transport/runtime failure | trust domain comprometido | processo isolado deve ser recriado |
| durable commit falha | storage error; output retido se gate existir | policy pode reiniciar | sem sucesso externo falso |
| overload | admission error/await | preservado | backoff do caller |

### Restart policy

Policies possíveis: `.never`, `.onPanic`, `.onCrash` e limite por janela com
backoff. Elas pertencem ao deployment. Restart storms abrem circuit breaker e
marcam a instância unavailable. Mensagens não confirmadas não são reproduzidas
sem inbox durável e operação idempotente.

### Observabilidade

Cada evento registra IDs de instance/generation/call/task, queue time, suspension
sites, deadline, resource usage, commit revision e outcome. Payloads, secrets e
capability tokens são redigidos. Instrumentação não pode alterar ordering,
fairness ou comportamento debug/release.

## Relação com packages e builds reproduzíveis

O package manager entrega source/artefatos autenticados; o runtime não baixa
código por import. `w build` inclui descriptors, schemas, adapters e policies
compiladas na receita. `w deploy` ou ferramenta equivalente fornece grants e
secrets separados.

Build reproduzível não significa instância determinística: eventos, tempo e rede
são inputs runtime. Para testes, o host injeta clock, entropy, storage e scheduler
controláveis. Para release, provenance liga o descriptor de serviço ao digest do
executável e aos packages, sem inserir metadata mutável no hash principal.

## Sequência de protótipos

### Slice M0 — módulos estáticos

- projeto multiarquivo com interface por módulo;
- import lógico via manifest/lock local;
- rebuild incremental explicado;
- módulos fundidos e separados geram a mesma semântica.

**Gate:** nenhum import executa, baixa ou concede authority.

### Slice R0 — instância serial in-process

- API explícita de start/stop e `.process`/`.key`;
- handler non-reentrant, mailbox limitada e unary calls tipadas;
- memory KV, clock de teste, cancellation e tracing;
- dois serviços em paralelo e `spawn` sem capturar state mutável.
- o mesmo workload em uma unidade, várias keys e dois packings físicos;
- métricas de CPU/evento, memória/instância, queue/tail latency e calls/turn.

**Gate:** scheduler determinístico cobre ordering, overload, drain e panic; o
packing otimizado passa o mesmo oracle sem esconder seu custo.

### Slice R1 — durable adapter

- transaction/confirmed revision e migrations;
- SQLite como segunda implementação depois do oracle memory KV;
- crash injection entre write/commit/response;
- outbox explícito antes de output gate inferido.

**Gate:** nenhum sucesso é observado para uma write não confirmada no profile
forte; recovery preserva a revisão declarada.

### Slice R2 — processo e defense-in-depth

- mesmo stub por IPC;
- grants de handles, limits e kill/restart;
- profile Linux com seccomp depois do threat model;
- teste de escape/violação e matriz de target documentada.

**Gate:** documentação distingue trust domain de isolamento e não atribui
segurança a static/dynamic linking.

### Slice R3 — gates e RPC composto

- comparar closed turn, input gate e reentrância explícita;
- output gate com causalidade e limits;
- pipeline de duas calls dependentes com cancellation/partial failure;
- medir throughput, tail latency e complexidade source.

**Gate:** a sugar reduz código sem esconder ordering, durability ou outcome.

### Slice P0 — playground WASM

- subset síncrono/ownership já conforme;
- imports mínimos, memory/time limit e output capturado;
- sem network/filesystem por default;
- engine e toolchain fixados na receita.

**Gate:** equivalência com o backend nativo no corpus suportado e threat model do
host publicado.

## Matriz de decisões e gates

| Decisão | Estado atual | Evidência necessária |
|---|---|---|
| fronteira module/instance/package | **Direção** | nenhum lifecycle/import implícito nos slices |
| unidades lógicas fine-grained | **Direção** | address/contract/lifecycle/cost observáveis em qualquer packing |
| interface compilada versionada | **Candidato** | invalidation e evolução de schema |
| objeto por módulo no backend inicial | **Candidato** (implementação) | link simples e cache; sem virar ABI pública |
| toda compilation unit como library | **Rejeitado por enquanto** | restringe linking sem oferecer isolamento |
| instance keyed/process | **Candidato** | routing, geração e restart determinísticos |
| strand closed-turn default | **Candidato** | três workloads; deadlock/latency/ergonomia |
| reentrância explícita/input gates | **Pesquisa** | invariantes de ownership e benchmark |
| mailbox bounded | **Candidato** | overload e cancellation property tests |
| service call sempre async | **Candidato** | fast path e IPC com mesma observabilidade |
| capability references/pipelining | **Pesquisa** | authority, lifetime, cycles e partial failure |
| SQLite adapter | **Pesquisa** | oracle, crash tests e mais de um target |
| output gate | **Pesquisa** | causalidade, limits e external effects |
| sandbox por processo | **Candidato** (deployment) | threat model e adapters reais |
| seccomp universal/per-library | **Rejeitado por enquanto** | Linux-only e boundary inadequado |
| WASM playground | **Pesquisa** | backend conforme e host endurecido |
| co-location/inlining/batching | **Candidato** (implementação) | equivalência semântica e ganho medido por workload |
| boundary física por unidade/função | **Rejeitado por enquanto** | deployment e trust escolhem boundary; custo não é implícito |
| `nanoservice` como nome público/keyword | **Em aberto** | só após provar limite conceitual, ergonomia e ausência de feature soup |

## Perguntas para decisão do usuário

1. [W-O024](../STATUS.md): o strand fechado deve ser a experiência default, mesmo que uma call externa
   longa bloqueie a instância, ou o primeiro prototype já compara um input gate?
2. [W-O023](../STATUS.md): o serviço keyed é central o bastante para merecer sintaxe própria no source,
   ou a chave deve existir somente no deployment/`ServiceHost`?
3. Você considera aceitável que uma call local de serviço sempre tenha `await`
   para nunca esconder uma futura mudança para IPC?
4. Qual failure boundary deve ser o default de produção: app process inteiro,
   worker process por trust domain ou processo por serviço?
5. O primeiro caso durable deveria ser counter/seat reservation, cache de build
   ou um request service real?
6. Você prefere transaction explícita no código inicial ou um turn transacional
   automático com escape hatch?
7. Output gates devem cobrir somente response ao caller ou também outgoing RPC,
   publicação de eventos e logs auditáveis?
8. O playground WASM entra antes ou depois do primeiro runtime async nativo?
9. [W-O027](../STATUS.md): “Nanoservice” deve sobreviver como nome público ou apenas como lente interna?
   Que propriedade mínima — state keyed, lifecycle, capability ou failure
   boundary — justifica criar uma unidade em vez de manter uma função/object?

## Fontes primárias

- [Cloudflare — Durable Objects: Easy, Fast, Correct — Choose three](https://blog.cloudflare.com/durable-objects-easy-fast-correct-choose-three/)
- [Cloudflare — Cap’n Web: RPC para browsers e servidores](https://blog.cloudflare.com/capnweb-javascript-rpc-library/)
- [Cloudflare — SQLite em Durable Objects](https://blog.cloudflare.com/sqlite-in-durable-objects/)
- [Cloudflare — sandbox: Linux seccomp](https://github.com/cloudflare/sandbox)
- [Linux Kernel — seccomp BPF](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html)
- [Wasmtime — Security](https://docs.wasmtime.dev/security.html)
- [Cloudflare — apresentação do `workerd` e nanoservices](https://blog.cloudflare.com/workerd-open-source-workers-runtime/)
- [Cloudflare — repositório `workerd`](https://github.com/cloudflare/workerd)

## Inspiração não normativa

- [Hacker News — “the future of compute is fine-grained”](https://news.ycombinator.com/item?id=31759801) — discussão que motivou a lente; não usa nem define o termo “nanoservice”
