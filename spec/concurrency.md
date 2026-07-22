# Concorrência, paralelismo e tasks

> **Status:** semântica candidata; nenhum runtime conforme existe ainda
> **Data:** 21 de julho de 2026

W trata **concorrência** e **paralelismo** como intenções diferentes, mas usa a mesma árvore estruturada para lifetime, errors e cancelamento.

## Vocabulário

| Termo | Significado em W |
|---|---|
| task | unidade lógica de execução com lifetime, resultado, error e cancellation state |
| child task | task criada dentro de um scope e owned por ele |
| concurrency | progresso intercalado de trabalhos; não exige execução simultânea |
| parallelism | possibilidade de dois trabalhos executarem ao mesmo tempo em recursos distintos |
| suspension point | ponto em que uma task pode ceder seu executor sem bloquear a thread |
| executor | política/runtime que agenda jobs em uma ou mais threads/loops/queues |
| executor preference | placement herdável para jobs não isolados; não protege estado |
| isolation domain | contexto que serializa ou restringe acesso a estado conforme um contrato |
| affinity | exigência de executar numa thread/event loop/device particular do host |
| task group | owner lexical de children, ordering, join, erro e backpressure; não é executor |
| cancellation | solicitação cooperativa para terminar uma task e seus filhos |
| join | espera estruturada pelo término e cleanup de um filho |

Um executor pode usar event loop, work stealing, IO completion ports, pthreads, signals internos ou outra técnica. Esses detalhes não alteram a semântica do programa.

## Três formas de executar

### Chamada normal

```w
let digest = hash(data)
```

A função síncrona executa no job atual até retornar, falhar ou panicar.

### Await direto

```w
let response = try await http.get(url)
let body = try await response.body()
```

Cada operação começa e o caller aguarda antes de iniciar a próxima. `await` é um suspension point; a thread pode executar outra task.

### Filho concorrente

```w
async let profile = fetchProfile(id)
async let feed = fetchFeed(id)

let (profile, feed) = try await (profile, feed)
```

Os dois filhos começam antes do join. Eles podem intercalar progresso no mesmo executor ou executar simultaneamente se a policy permitir, mas `async let` não promete outro core.

### Filho paralelo

```w
spawn let left = transform(leftHalf)
spawn let right = transform(rightHalf)

return merge(await left, await right)
```

`spawn let` autoriza e solicita execução no executor de trabalho paralelo. O runtime pode limitar o pool e executar inline para evitar oversubscription, desde que preserve a ausência de aliases/data races e os suspension points observáveis.

## Contrato de função async

```w
async fn fetch(id: UserId): User throws NetworkError {
  // ...
}
```

`async` faz parte do tipo. Uma chamada precisa escolher uma forma válida:

```w
let user = try await fetch(id) // começa e aguarda
async let user = fetch(id)     // começa filho concorrente
```

Isto é erro:

```w
let user = fetch(id) // não cria Future/Promise silenciosa
```

Qualquer expressão síncrona pode ser colocada num filho por `async let` ou `spawn let`. O primeiro serve a composição concorrente; o segundo declara intenção paralela.

O handle tem tipo conceitual `Task<T, E>`, mas seu lifetime é lexical e não deve virar um Future solto por default.

## Regras de scope

Considere:

```w
async fn page(id: UserId): Page throws PageError {
  async let user = loadUser(id)
  async let posts = loadPosts(id)

  return Page(
    user: try await user,
    posts: try await posts,
  )
}
```

Baseline:

1. cada child é registrado no scope criador;
2. o scope não termina enquanto seus filhos não terminarem e executarem cleanup;
3. cada handle deve ser joined, cancelado ou transferido para outro owner estruturado;
4. retorno/erro antes do join solicita cancelamento dos filhos restantes e então faz join;
5. destruir/esquecer um handle não destaca a task;
6. um resultado owned é transferido no `await`; um segundo consume é inválido salvo tipo explicitamente shareable;
7. o compiler diagnostica child não consumido mesmo quando o runtime teria como limpá-lo.

Isso transforma “structured concurrency” em regra verificável, não apenas estilo.

## Errors

```w
async let first = loadA()
async let second = loadB()

let (a, b) = try await (first, second)
```

Proposta de propagação:

- o join observa resultados na estrutura lexical, não na ordem de conclusão;
- se um erro é propagado, irmãos ainda ativos recebem cancellation;
- o scope aguarda cleanup de todos antes de propagar;
- o error primário é o que corresponde ao ponto de join/ordem definida; erros de cleanup/irmãos ficam anexos como diagnostics, não substituem o erro aleatoriamente;
- aggregation explícita usa uma API que retorna todos os `Result`.

O error set do pai inclui as operações que ele propaga. O lowering não depende de C++ exceptions.

## Cancelamento

Cancelamento é uma solicitação, não `pthread_cancel` ou encerramento assíncrono de stack.

Uma task observa cancelamento:

- antes/depois de suspension points;
- em `Task.checkCancellation()` para loops computacionais;
- em APIs cancel-aware de I/O;
- em boundaries de task group configuradas pelo compiler/runtime.

Exemplos de intenção:

```w
async let report = buildReport()
cancel report

spawn let index = rebuildIndex()
cancel index, reason: .shutdown

for chunk in chunks {
  Task.checkCancellation()
  process(chunk)
}
```

`cancel` é uma statement contextual, não um método livremente sobrecarregável.
O operando precisa ser `Task`, `TaskGroup` ou conformar explicitamente a
`Cancellable`. A forma com `reason:` registra diagnóstico/policy; não muda
cancelamento em typed error nem permite matar uma thread ou desenrolar foreign
frames assincronamente.

Propriedades:

- parent cancellation propaga a descendants;
- cancelar um filho não cancela irmãos automaticamente, salvo policy do group;
- `defer` e destruction rodam durante unwind estruturado da task;
- foreign blocking call não pode ser interrompida com segurança sem adapter próprio;
- deadlines são cancellation com motivo/metadata, não outra hierarquia de threads.

`async`/`await`/`spawn`/`cancel` formam o vocabulário source; APIs de runtime
podem ainda expor handles/tokens, mas não criam uma segunda semântica.

## Paralelismo e segurança de dados

Um valor que atravessa `spawn` precisa ser:

- transferido (`take`) e então owned pelo filho;
- um value independente/copiado;
- imutavelmente compartilhado por um tipo `Sync`/equivalente;
- um handle de sincronização com operações seguras.

```w
let pixels = image.takePixels()
spawn let encoded = encode(take pixels)
// pixels não está disponível aqui
```

Isto deve falhar:

```w
spawn let result = mutate(inout sharedBuffer)
read(sharedBuffer) // alias durante execução paralela
```

Traits/capabilities conceituais:

| Propriedade | Significado |
|---|---|
| `Send` | ownership do valor pode mudar de executor/thread |
| `Sync` | refs imutáveis podem ser observados concorrentemente |
| `CancelSafe` | uma operação suspensível preserva invariantes se cancelada |

`Send`, `Sync` e `CancelSafe` são nomes públicos candidatos e aparecem em
constraints/diagnostics quando a inferência não basta. Conformance manual que
contorna o verifier exige uma fronteira `unsafe`.

Tipos C são não-Send/não-Sync por default até um wrapper/adapter provar o contrário.

## Mutabilidade compartilhada

W não promete remover locks. Oferece abstrações que tornam a policy localizada:

- ownership transfer/message passing;
- immutable snapshot/COW;
- atomic types para operações pequenas;
- `Mutex<T>`, `RwLock<T>` e condition primitives na stdlib;
- serviço/actor serializado;
- RCU para bibliotecas especializadas.

O compiler pode provar um padrão single-producer/single-consumer e eliminar sincronização, mas o source não deve fingir que atomics sempre são gratuitos nem transformar toda property em `_Atomic`.

### Modifier `atomic`

O caminho comum pode declarar storage atômico no mesmo slot visual dos property
behaviors, sem fingir que ele é um behavior de biblioteca:

```w
var atomic completedOrders: u64 = 0

completedOrders += 1              // RMW seq-cst, não load + store
let snapshot = completedOrders    // load seq-cst
completedOrders.store(0, order: .release)
```

Na HIR, o binding possui storage `Atomic<u64>`. Reads/writes/operators comuns
usam seq-cst; APIs nomeadas selecionam acquire/release/relaxed/compare-exchange.
`ref completedOrders` empresta o handle atômico, nunca um `ref u64`, e `inout` do
payload é proibido. `let atomic` não existe: um handle imutável para storage
mutável continua sendo construído explicitamente como `let counter = Atomic(0)`.

`atomic` é verifier-backed porque altera data-race checking, operações válidas,
layout e target support. Um `behavior Atomic` escrito pela biblioteca não pode
alegar essas garantias. Composição com `Lazy`/`Observed` é rejeitada até existir
uma regra específica que preserve initialization e memory ordering.

## Task groups dinâmicos

`async let`/`spawn let` servem quando a estrutura está clara no source. Coleções dinâmicas usam um owner explícito:

```w
return try await TaskGroup<Thumbnail>.parallel { group in
  for image in images {
    group.add(take image, using: resize)
  }

  return try await group.collect()
}
```

A API/construct final precisa definir:

- limite de paralelismo e backpressure;
- ordem de resultados;
- fail-fast versus collect-all;
- cancellation/deadline;
- memória máxima por item/group;
- behavior quando `add` recebe mais trabalho que o executor suporta.

O runtime nunca deve criar uma thread por item sem policy explícita.

## Streams e backpressure

`yield`, `yield*` e `Stream<T>` foram muito explorados, mas não são necessários para provar tasks one-shot.

Direção futura:

```w
async fn lines(file: ref File): Stream<String> throws IOError
```

O stream precisa especificar demand, buffer/watermark, cancellation e ownership de cada elemento. Até isso existir, `yield` não entra na gramática mínima.

## Executors e filas

Nomes como `main`, `background`, `network`, `io`, `UX` e queue por módulo podem
ser presets de executor, não keywords.

Uma possível API futura:

```w
spawn(on: executors.cpu, priority: .userInitiated) let result = compute()
```

Mas o código comum deve permitir:

```w
spawn let result = compute()
```

Configuração precisa ser hierárquica e consultável:

- defaults do runtime/profile;
- limite do processo;
- override de package/app;
- hint por group/task;
- limite imposto pelo ambiente/cgroup/serverless.

Afinidade a CPU específica é capability de plataforma e ferramenta de profiling, não semântica comum.

### Isolation, preference e affinity

A auditoria histórica recuperou [W-O100](../STATUS.md). Os antigos “thread
groups” só podem voltar à superfície se preservarem quatro contratos distintos:

- um `service`/entry pode **exigir isolation** para proteger seu estado;
- uma task subtree pode **preferir um executor** para placement de trabalho não
  isolado;
- `spawn` declara **intenção paralela**, independentemente do nome do pool;
- somente um host profile pode exigir **affinity** física, como a UI thread.

Um módulo estático não possui nenhum desses recursos. Importá-lo não cria fila,
thread ou hop. A instância/entry recebe o binding no descriptor; tasks recebem
um executor do scope e podem, se W-O100 aceitar a sintaxe, fazer override local.

Direção semântica para a comparação:

1. o executor exigido por isolation prevalece sobre a preference do caller;
2. cruzar isolation exige `await` e transferência válida mesmo num fast path;
3. preference é herdada por `async let` e task groups estruturados, não por uma
   task detached futura;
4. um executor serial fornece exclusão lógica, não identidade permanente de
   thread;
5. remapear placement no deployment é permitido; remover isolation, ordering ou
   `Send` não é;
6. priority/QoS é hint até que um profile publique garantia mais forte.

As formas abaixo são alternativas, não grammar aceita:

```w
async on network let response = fetchMenu()
spawn on compute let forecast = buildForecast(take history)
```

`async on` significaria child concorrente com preference; `spawn on` só seria
válido para um executor capaz de satisfazer a intenção paralela. Trabalho UI
isolado deve normalmente aparecer como call ao owner:

```w
await renderer.show(forecast)
```

e não como `spawn on ui`, que confundiria paralelismo com um domínio serial.
Declaração física apenas no manifest, `service/entry ... on domain`, defaults de
profile e a forma histórica por módulo continuam comparadas em
[DB1_ADDENDUM.md](../DB1_ADDENDUM.md#a01--domínios-de-execução).

## I/O

A linguagem expressa suspensão. A stdlib escolhe backend por target:

- IOCP no Windows;
- epoll/kqueue/poll ou adapters modernos em Unix;
- `io_uring` quando disponível e vantajoso;
- APIs de plataforma no embedded/browser runtime.

Uma API blocking estrangeira deve ser adaptada. A sintaxe source ainda depende
de W-O063; até ela fechar, a declaração FFI permanece conservadora e o adapter
registra o custo fora de uma annotation genérica:

```w
foreign c {
  fn legacy_read(...): c.int
}
```

O runtime pode movê-la a um blocking executor, mas isso aparece na metadata e no
profile. A declaração sem adapter não torna a chamada cancel-safe.

A escolha entre `read`/`write`, `io_uring` ou outro backend é uma decisão de
stdlib medida por workload e target, não regra do language core.

## Módulos e serviços

Módulos de source são namespaces/build units. Eles não recebem uma thread ou queue automaticamente.

Um stateful runtime boundary pode ser explícito:

```w
service ImageCache {
  // proposta de ecossistema, não sintaxe v0
}
```

Isso permite testar single-thread actor, replicas/fork, resource budget e IPC sem transformar todo import num singleton serializado.

## FFI

Uma foreign function recebe metadata de concurrency:

- thread-safe/reentrant;
- blocking/non-blocking;
- callback thread/executor;
- cancellation support;
- borrowed/owned buffers;
- global state;
- signal safety quando relevante.

Sem override declarado, assume-se a policy conservadora. O wrapper pode serializar ou executar no blocking executor, mas custo e risco precisam aparecer nos diagnostics/profiles.

Callbacks que entram no runtime criam um job num executor conhecido; não retomam arbitrariamente uma task em qualquer thread sem synchronization.

## Lowering proposto

Pipeline conceitual:

```text
HIR com scopes/tasks/ownership/effects
  → W MLIR: task.scope, task.child, task.spawn, await, cancel, cleanup
  → análise de captures, Send/Sync, liveness e error edges
  → frames/continuations + runtime calls
  → MLIR async/cf/func/LLVM dialects quando aplicável
  → LLVM coroutine/state-machine ou runtime ABI
```

O [dialeto Async do MLIR](https://mlir.llvm.org/docs/Dialects/AsyncDialect/) oferece tokens, values, groups e operações de coroutine/runtime. Ele é uma ferramenta de lowering, não especifica sozinho parent/child lifetime, typed errors, cancellation, sendability ou `spawn`. Essas invariantes precisam sobreviver no dialeto W até serem verificadas.

O [LLVM coroutine lowering](https://llvm.org/docs/Coroutines.html) pode construir frames e funções resume/destroy. Também não escolhe a semântica de scheduler/structured concurrency do frontend.

## Runtime mínimo

Um primeiro runtime conforme precisa de:

- task control block e árvore parent/child;
- executor concorrente single-thread;
- executor paralelo com pool limitado;
- wakeups/timers;
- cancellation state e cleanup;
- integração de uma forma de I/O por plataforma de teste;
- queues corretas e memory reclamation segura;
- diagnostics/tracing para task IDs e scopes.

Não precisa inicialmente de:

- lock-free em todas as filas;
- work stealing sofisticado;
- signals para preempção;
- hot reload/fork de módulo;
- remote tasks;
- QoS completa;
- GPU.

Protótipos de filas, atomics, signals ou `setjmp` não implementam por si só esta
semântica. Qualquer técnica retomada precisa demonstrar ausência de data races,
reclamation correta, cancelamento estruturado e comportamento portátil.

## Matriz de comportamento

| Source | Começa quando | Pode suspender caller | Pede outro core | Lifetime |
|---|---|---:|---:|---|
| `f()` | agora | não, se `f` é sync | não | frame atual |
| `await f()` | agora | sim | não necessariamente | até retorno |
| `async let x = f()` | na declaração | só no `await`/scope exit | não | child do scope |
| `spawn let x = f()` | na declaração | só no `await`/scope exit | sim | child do scope |
| detached API futura | chamada explícita | depende | depende | owner runtime explícito |

## Questões a prototipar

1. O `async` de declaração deve vir antes de `fn` ou depois do retorno?
2. `Task<T,E>` é publicamente nomeável ou apenas handle lexical?
3. Await consome sempre o resultado ou permite múltiplos readers de `Copy`?
4. Qual error ganha primazia quando vários filhos falham quase juntos?
5. Como representar cancellation reason sem poluir todo error set?
6. Qual é a API mínima de task group e seu default de backpressure?
7. O primeiro lowering usa MLIR Async runtime, LLVM coroutines ou runtime state machines próprias?
8. Quais dados podem ser `Send`/`Sync` por derivação automática?
9. Como instrumentar fairness, latency e queue depth sem alterar semântica?

Responder essas perguntas com um restaurante ou downloader de brinquedo é mais valioso que implementar afinidade, signals e filas lock-free antes do modelo estar fechado.
