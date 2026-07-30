# Restaurante Última Luz

> **Status:** corpus experimental da DB2 · 29 de julho de 2026

O Restaurante Última Luz serve a última janela observável antes do encerramento
do universo. O cenário homenageia o absurdo cósmico popularizado por Douglas
Adams. Os personagens, diálogos, pratos e eventos deste corpus são originais.

O ensaio não prova que a linguagem está implementada. Ele pressiona a forma
integrada de [DESIGN.md](../../DESIGN.md).

## 1. Rotas operacionais

```text
LastLightSimulation
  → cenário fechado
  → simulação por ticks
  → relatório determinístico

LastLight / LastLightTui / LastLightLineHost
  → Command
  → dispatch único
  → RestaurantApi
  → AppResponse
  ├─ texto portátil
  ├─ terminal ANSI
  └─ JSON por HTTP
```

`LastLightSimulation` é o primeiro alvo operacional. Ele não usa relógio,
aleatoriedade, network nem deployment de services. O mesmo profile deve produzir
os mesmos eventos, totais e consumo de energia em qualquer execução compatível.

`LastLight` expõe CLI de texto e `http.fetch`. `LastLightTui` reutiliza a mesma
resposta tipada e adiciona somente controle ANSI. `LastLightLineHost` recebe uma
linha por evento do host. Nenhuma dessas rotas exige uma biblioteca gráfica.

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
  → Região Temporária do Cardápio
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

O contrato do supervisor e o descriptor de deployment ainda estão em
**Pesquisa**. Até essa decisão, `LastLightSimulation` é o alvo executável
independente previsto.

## 2. Mapa de source

| Arquivo | Responsabilidade |
|---|---|
| `domain.w` | newtypes, refinements, enums e errors |
| `command.w` | parser streaming, spans, buffer limitado e comandos tipados |
| `text.w` | UTF-8, unidades de texto, normalização, bytes, paths e C strings |
| `string_storage.w` | construção incremental, reserva, reuse e carrier binário |
| `collections.w` | arrays, views, iteration, Map/Set, hashing e stable sort |
| `views.w` | diferença entre owner, borrow completo e projeção de extent fixo |
| `failure.w` | Option, Result, typed throws, panic, OOM e cleanup |
| `generics.w` | primary associated types, constraints, inference e witnesses |
| `enum_contracts.w` | subsets fechados de enum, narrowing e payloads |
| `state_transitions.w` | paths validados, typestate consuming e snapshots runtime |
| `reflection.w` | TypeId local, reflection opt-in, synthesis e visibilidade |
| `rest_arguments.w` | rest homogêneo, expansão `each`, ownership e call shape |
| `units.w` | SI, dimensão e units customizadas |
| `numerics.w` | literais, conversões, overflow, float, ranges e quantization |
| `kitchen.w` | resources move-only, protocols térmicos, ranges e controle PID |
| `oracle.w` | matriz/tensor, `@`, shape e cálculo de lotes |
| `performance.w` | fatos de prova, largura interna, SIMD e custos de texto |
| `hardware.w` | fronteira C, layout e deallocator |
| `memory.w` | ownership, enum subset, niches, pinning e callback C |
| `allocation.w` | placement, allocator, arena, budget e rehome |
| `callables.w` | function pointer, opaque callable, erasure e callable modes |
| `menu_compiler.w` | compiler pequeno restrito ao profile `bootstrap.w0` |
| `execution.w` | task groups bounded, outcomes, ordering e cancelamento |
| `mobility.w` | transferência exclusiva, sharing verificado e captures |
| `synchronization.w` | atomics, memory orders, CAS e locks scoped |
| `streams.w` | stream pull, views borrowed, channel MPSC e backpressure |
| `io.w` | byte I/O async, file posicional, buffers e chunks borrowed |
| `billing.w` | Money, idempotência, existential, opaque return e behavior |
| `dining.w` | serial turn, backpressure, applause e resposta |
| `restaurant.w` | integração de services, tasks, ownership e compensação |
| `simulation.w` | cenários, algoritmo por ticks, capacidade, energia e receita |
| `presentation.w` | resposta tipada e render portátil ou ANSI |
| `simulation_app.w` | entry determinística sem deployment de services |
| `app.w` | CLI, TUI ANSI, linha por evento, HTTP, Context e entries |

Esses arquivos usam a forma líder da DB2. A versão DB1 está no
[arquivo histórico](../../../Y/W/archive/db1-2026-07-27/examples/restaurant/).

### 2.1 Cobertura e alcance

O corpus separa duas perguntas. Um arquivo de ensaio mostra se uma forma local é
clara. Uma rota operacional mostra se as formas funcionam juntas.

| Camada | Testemunho principal | Benefício observado |
|---|---|---|
| módulos, visibilidade e entry | `app.w`, `simulation_app.w` | composição sem execução por import |
| tipos, refinements e enum subsets | `domain.w`, `enum_contracts.w` | estados inválidos saem do runtime |
| controle, patterns e errors | `command.w`, `failure.w`, `state_transitions.w` | fluxo exaustivo e falha tipada |
| ownership, views e allocation | `memory.w`, `views.w`, `allocation.w` | custo e lifetime aparecem no source |
| texto, collections e streams | `text.w`, `string_storage.w`, `collections.w`, `streams.w` | Unicode e backpressure ficam explícitos |
| async, paralelo e sincronização | `execution.w`, `mobility.w`, `synchronization.w` | estrutura e limites substituem threads soltas |
| services e compensação | `restaurant.w`, `billing.w`, `dining.w` | calls e efeitos remotos permanecem observáveis |
| units, números, matriz e performance | `units.w`, `numerics.w`, `oracle.w`, `performance.w` | provas de domínio autorizam otimizações |
| C e layout | `hardware.w` | a fronteira estrangeira mantém ownership tipado |
| self-host e build reproduzível | `menu_compiler.w` e o contrato de package | bootstrap e provenance têm um oracle pequeno |
| operação integrada | `simulation.w`, `presentation.w`, `app.w` | um modelo tipado atende CLI, TUI e HTTP |

A tabela cobre as famílias aceitas da DB2. Itens em **Pesquisa**, alternativas
contrafactuais e propostas rejeitadas não são requisitos do executável. Cada um
continua preservado em `DESIGN.md`.

## 3. Casos e oracles

### 3.1 Pórtico de Nácar

Famílias: entry, host profile, imports e capabilities.

Aceite:

- `entry { ... }` só funciona com um default slot único;
- `entry LastLight` liga slots tipados;
- importar `app` não executa um handler;
- Context não concede filesystem ou network ausentes.

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
- `tryMaterialize(using:)` torna allocation failure recuperável;
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

Famílias: String flat, literal static, SSO, reserva, mutation, carrier e OOM.

Aceite:

- W0 funciona com literal/static e um buffer UTF-8 flat de owner único;
- criar `String()` vazio não aloca;
- `copy` de storage dinâmico cria outro owner durante a operação;
- COW não desloca allocation ou budget para uma mutation futura;
- SSO pode mudar por target sem mudar source, resultado ou ABI pública;
- a API não expõe capacity nem threshold de SSO;
- `tryReserve(minimumBytes:)` usa o total mínimo e mantém o valor na falha;
- append e replace não alocam quando a reserva comprovada basta;
- `clear()` mantém storage e `reset()` o libera;
- `takeAll()` transfere o conteúdo e deixa o receiver vazio;
- `String.adoptingUtf8` e `String.intoBytes` transferem o carrier sem allocation
  geral;
- uma source view do mesmo owner não entra numa mutation;
- reads não alocam nem atualizam uma cache lazy;
- um summary eager, como `isAscii`, muda somente durante mutation;
- raw pointer fica dentro de uma closure scoped;
- CString ou Bytes pinned atende uma API que guarda o pointer;
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
- `Power * Duration` produz Energy;
- point menos point produz delta;
- point mais point falha;
- `switch` com range e `if` preserva a regra anti-windup;
- `clamp` prova que o `DutyCycle` refinado está em `0.0...1.0`;
- `{unit}` e `[unit]` aparecem somente no corpus comparativo;
- lowering sem reflection remove metadata de unit.

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

Famílias: data-race freedom, happens-before, atomics, CAS e locks scoped.

Aceite:

- `var atomic value: T` baixa para `Atomic<T>`;
- load, store e compound update comuns usam `.sequential`;
- orders mais fracas usam contratos como `load<.acquire>()`;
- load rejeita `.release`, e store rejeita `.acquire`;
- `+=` é uma read-modify-write checked;
- `value = value + 1` é rejeitado como load e store separados;
- `Bool`, integers e enums sem payload podem usar storage atomic;
- compare-exchange devolve `.exchanged` ou `.mismatch`;
- weak compare-exchange permite falha espúria somente quando o nome informa;
- CAS não prova reclamation nem elimina ABA;
- `Atomic<T>` não promete lock-freedom;
- `lockFree: true` falha no build quando o target não oferece a garantia;
- `ref atomicValue` obtém `ref Atomic<T>`, nunca `ref T`;
- `Mutex.withLock` não deixa borrow ou guard escapar;
- `AsyncMutex.withLock` suspende na aquisição, não dentro da closure;
- cancellation durante a espera não executa a closure;
- state de um closed turn não recebe atomic ou lock sem outra razão;
- RCU e cache isolation continuam tipos ou contratos especializados.

O oracle executa litmus tests de publication, store buffering e
compare-exchange. Ele repete os testes com uma, duas e quatro threads. O profile
também força o fallback não lock-free e executa TSan.

Failure injection cobre cancellation antes e depois da aquisição async. O trace
confirma que cada critical section libera o lock uma vez. Benchmarks separam
latency sem contenção, contenção na mesma cache line e counters particionados.

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
- commit de send acontece antes de receive devolver o item.

O oracle usa dois balcões de universos incompatíveis como producers. Um único
maître recebe os pedidos. Ele repete o caso com capacity 0, 1 e 64, e com uma,
duas e quatro worker threads.

Failure injection cancela cada send, receive e reserve antes e depois do commit.
Outro perfil fecha ou destrói o receiver com buffer e permits pendentes. Cada
pedido deve terminar em exatamente um destes destinos:

1. consumer;
2. error que devolve o owner;
3. cleanup registrado.

O teste de view percorre linhas borrowed do cardápio sem allocation. O corpus
rejeita guardar uma linha depois do próximo `next()`, enviá-la por channel ou
movê-la para task detached.

### 3.5.4 Arquivo Posicional das Receitas Extintas

Famílias: byte I/O, EOF, progress parcial, cancellation, rights e buffers.

Aceite:

- `ByteSource` acrescenta somente bytes confirmados a `Bytes`;
- `.data(count)` possui `count > 0`, e `.end` é terminal;
- `ByteSink.write` informa `.complete` ou um prefixo positivo;
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
- readiness, completion e fallback blocking produzem o mesmo trace semântico.

O oracle divide o Arquivo das Receitas Extintas em todos os pontos possíveis.
Cada execução injeta short read, short write, EOF junto com dados, error depois
de progress e cancellation nos dois lados da completion. O payload final, o
prefixo committed e o número de cleanups devem ser iguais em `io_uring`, IOCP,
readiness e executor blocking.

O teste posicional lê blocos sobrepostos com um `shared File`. A ordem de
completion pode mudar. Cada bloco deve manter o offset solicitado. O cursor
sequencial continua único e rejeita duas leituras concorrentes.

### 3.6 Salão Prisma

Famílias: service, serial turn, mailbox, hop e backpressure.

Aceite:

- a mesma `ServiceRef` funciona local e remotamente;
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
- um futuro `CallPipeline` deve reduzir round trips sem ocultar calls ou effects.

### 3.7 Conta da Aurora Tardia

Famílias: Money, errors, idempotência, property behavior, existential e opaque
return.

Aceite:

- Currency diferente exige conversion explícita;
- rounding policy é parte da operação;
- overflow não usa binary float;
- `Versioned` não concede atomicidade fora do serial turn;
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
- o parser W mantém o body `fn<C>` opaco;
- o body scanner C encontra o fechamento sem interpretar statements como W;
- o adapter C gera façade C e static archive reproduzível;
- funções do mesmo adapter compartilham uma foreign unit;
- o deallocator original executa uma vez;
- panic não faz unwind através de C.

### 3.10 Despensa Selada

Famílias: package, lock, digest, mirror e capability.

Aceite:

- lock fixa source e artifact;
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

### 3.12 Turno do Horizonte Violeta

Famílias: todas.

Aceite:

- um product seleciona `entry LastLight`;
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
- `Array<u8><(.count <= 64)>` refina um generic já aplicado;
- `Array<[u8, (.count <= 64)]>` não substitui os dois contratos;
- `spawn<.compute>` e `spawn<domain: .compute>` produzem o mesmo task contract;
- `fn<C>` e `fn<lang: .c>` selecionam o mesmo frontend hermético;
- `<(...)>`, `<{...}>` e `<[...]>` preservam expression, record e list;
- um slot repetido produz diagnostic antes do type-check normal;
- um case abreviado só preenche o slot primário declarado pelo schema;
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
- `upgrade()` retorna ausência depois do último shared owner;
- mover ownership por `spawn` exige `transferable`;
- compartilhar um borrow por `spawn` exige `shareable`;
- um borrow após `await` só compila com owner e task frame estáveis;
- mover ou substituir o owner durante esse borrow falha;
- `Pinned<T>` pode mudar de endereço sem mover o `T`;
- `try pin take state` separa allocation fallible do move;
- não existe `unpin` irrestrito depois que o endereço é publicado;
- a lease mantém o bell e o callback state vivos até unsubscribe;
- unsubscribe ocorre antes de liberar o callback state;
- converter pointer em address não permite reconstruir um pointer seguro;
- profiles portátil e compacto produzem o mesmo resultado.

### 3.15 Cardápio de Fótons

Famílias: bootstrap, lexer, parser, AST, collections, ownership e output
determinístico.

Aceite:

- `menu_compiler.w` usa somente o profile `bootstrap.w0`;
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
- o ensaio cresce pelos gates SH0–SH7 antes de declarar self-host completo.

Cobertura atual:

| Gate | Estado do ensaio |
|---|---|
| SH0–SH1 | parcial: lexer, parser da DSL e tabela CE0, ainda não do source W |
| SH2–SH3 | parcial: AST, symbols, errors, collections, move e drop |
| SH4 | parcial: bytecode e symbol table determinísticos, ainda sem HIR W |
| SH5–SH7 | não implementados |

### 3.16 Observatório do Cometa Paciente

Famílias: task group, backpressure, ordering, cancellation e atomic metrics.

Aceite:

- `parallelMap` mantém no máximo `limit` children ativos;
- o buffer de admissão também usa `limit`;
- `.input` devolve resultados na ordem dos jobs;
- `parallelCollect` preserva todos os outcomes;
- cancelar o batch fecha producer e children;
- `batch.cancel(reason: .shutdown)` preserva o handle para o join;
- `cancel` não existe como statement ou keyword;
- cada job move ownership para um child;
- `shared BrigadeMetrics` cruza a boundary porque usa storage atomic;
- um pointer C ou state mutável de service não pode ocupar o mesmo lugar;
- `TaskOutcome` distingue success, application error e cancellation.

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
- storage de service não aceita `package` ou `export`;
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
- `Money(majorUnits:, currency:)` seleciona o initializer com conversão;
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
segunda call ao `manifest` consumido.

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

Famílias: `TypeId`, reflection opt-in, synthesis e metadata alcançável.

Aceite:

- `TypeId.of<T>()` identifica uma specialization no build atual;
- o programa não persiste ou transmite `TypeId`;
- `Reflectable` emite somente metadata alcançável;
- o descriptor mostra properties exportadas e omite `secretCalibration`;
- `ActionableSignal` mostra somente os dois cases permitidos;
- `Hashable` e `Reflectable` são sintetizados sem annotations;
- backing storage de property behavior não aparece;
- debug symbols podem ser removidos sem alterar reflection;
- nenhum descriptor oferece offset, dynamic construction ou acesso por string.

O fixture negativo deve tentar persistir `TypeId`. Outro fixture deve procurar
`secretCalibration` no descriptor exportado.

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
- o profile compacto produz o mesmo resultado;
- `f64` preserva todos os NaNs e signed zero;
- sanitizer ou hardening pode desativar compactação;
- `w explain layout BellTarget` mostra a prova e o fallback.

Um fixture negativo deve remover `.unavailable` de `describeBell`. Outro deve
tentar passar `BellSignal.corrupted(...)` para o parâmetro refinado.

### 3.30 Duas Cozinhas e Nenhuma Thread Milagrosa

Famílias: execution domain, capacity, paralelismo aninhado, fairness e liveness.

Aceite:

- `async let` herda a preference do parent;
- `spawn` sem domain usa o parallel default;
- `.compute` permanece válido quando a capacity efetiva é 1;
- `.network` pode compartilhar executor físico com `.io`;
- `LastLightDomain.thermal` precisa de binding parallel no product;
- conformar o enum a `ExecutionDomain` não cria um executor;
- um módulo importado não cria domain, queue ou thread;
- os dois `mixBatch` de `mixAcrossTwoKitchens` compartilham o compute budget;
- dois limits de 8 não criam 16 workers quando a domain capacity é 6;
- o parent suspenso não retém o último permit necessário ao child;
- blocking FFI não ocupa o compute budget;
- a correção não depende de dois jobs executarem simultaneamente;
- scheduler replay pode trocar a ordem dos siblings sem trocar o resultado;
- `Task.yield()` não funciona como barrier;
- priority não substitui deadline nem isolation.

O scheduler adversarial usa uma única CPU lógica, inverte a ordem de todos os
children e suspende um nested group quando o budget está cheio. O programa deve
terminar com o mesmo resultado e sem criar um worker adicional.

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
- `w explain performance` separa fato, decisão, estimate e measurement;
- desligar toda especialização produz os mesmos valores, errors e panic.

O oracle diferencial executa `flavorScore` com lowering portátil, vector,
storage estreito desativado e storage estreito ativado. Cada execução precisa
produzir o mesmo tensor e o mesmo overflow. O benchmark registra target, CPU,
dataset, allocations, code size e intervalo de ruído.

### 3.32 Caixa dos Números que Recusam Disfarces

Famílias: literal exato, conversão, integer, float, decimal, quantization e
range.

Aceite:

- radix e exponent não perdem informação antes do expected type;
- um suffix fora do range falha no type checker sem truncar;
- `u8 + u16` usa `u16`, mas `i8 + u8` exige uma escolha explícita;
- debug, release, const evaluation e tensor usam o mesmo overflow;
- divisão signed trunca em direção a zero;
- Euclidean remainder exige uma API nomeada;
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

### 3.33 Região Temporária do Cardápio

Famílias: placement, allocator, arena, budget, escape e OOM.

Aceite:

- um local síncrono fixo que não escapa não usa o allocator geral;
- `object` não implica heap;
- somente calls com `using: staging` usam a região;
- `tryReserve` falha antes de consumir os elementos;
- cada string duplicada mantém a origem da região;
- `rehome` move storage independente e realoca somente storage dependente;
- uma falha de `rehome` consome e limpa o snapshot e o destino parcial;
- `attemptRehome` devolve o snapshot no outcome quando retry é necessário;
- o budget cobra alignment, padding e growth retido;
- `.budgetExceeded` não vira `.outOfMemory`;
- drop executa em ordem inversa da construção concluída;
- um child paralelo não compartilha a arena default;
- `Arena.fixed` não pede storage ao OS;
- o snapshot retornado não depende da região temporária;
- `w check memory --require no-general-allocation` mostra a call chain que viola
  o profile.

O oracle executa `stageMenu` com um allocator de falha injetada em cada
allocation. Antes de `rehome`, toda falha limpa os valores pela região. Durante
`rehome`, toda falha limpa source e destino parcial uma vez. Depois do success,
destruir a região não altera o snapshot. O teste repete com allocator do sistema,
buffer fixo e profile mimalloc. Os valores e drops são os mesmos; somente
measurements podem mudar.

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

Aceite:

- o mesmo profile produz a mesma sequência de `SimulationEvent`;
- o algoritmo usa somente input, ticks inteiros e ordem estável do array;
- número de cozinheiros e mesas limita a admissão;
- o relatório mostra capacidade, duração do tick e event log;
- pedidos que esperam além da paciência saem com `.departed`;
- energia usa `Power * DutyCycle * Duration`;
- receita usa `Money` em minor units e rejeita currency diferente;
- todo pedido termina como completed, departed ou unfinished;
- `queueHighWater` torna overload observável;
- `Command` representa menu, pedido, status, cancelamento, dashboard, simulação
  e encerramento;
- `AppResponse` é o único modelo de saída para CLI, TUI e HTTP;
- o renderer ANSI não muda os dados da resposta;
- o adapter HTTP não recebe autoridade para encerrar o processo;
- construção textual usa `append` no próprio `String`, sem um `StringBuilder`
  público;
- `LastLightSimulation` executa sem service registry;
- o target completo não consome `stdin` por duas APIs ao mesmo tempo.

O oracle de equivalência remove sequências ANSI antes da comparação. O teste de
replay executa o mesmo profile duas vezes. Ele compara métricas e eventos campo
a campo.

## 4. Alternativas visuais obrigatórias

O Book deve mostrar pares lado a lado:

| Tema | Forma líder | Contrafactual |
|---|---|---|
| unit | `9.81<m/s^2>` | `9.81[m/s^2]` |
| domain | `spawn<.compute> let x = ...` | `spawn<domain: .compute> let x = ...` |
| domain relacional | `spawn<.compute> let x = ...` | `spawn on .compute let x = ...` (**Rejeitado por enquanto**) |
| domain customizado | `spawn<domain: LastLightDomain.thermal>` | `"thermal"` ou keyword global |
| QoS | descriptor/policy de group | `.background` como domain |
| mobilidade | facts inferidos `transferable`/`shareable` | protocols `Send`/`Sync` ou `Sendable` |
| constraint de mobilidade | `T<(.transferable)>` | `T: Send` e `<mobility: .transferable>` |
| refinement | `T<(predicate)>` | `T where (predicate)` |
| receiver | `String<(.count <= 40)>` | `String<(value.count <= 40)>` |
| generic refinado | `Array<u8><(.count <= 64)>` | `Array<[u8, (.count <= 64)]>` |
| enum subset | `ServiceStage<[.preparing, .serving]>` | enum base + guard runtime |
| typestate | `OvenSession<.ready>` + `take fn` | state keyword, annotation ou mutation do tipo no lugar |
| protocol composition | `T: Display & Equatable` | postfix `where`; static list de protocols |
| runtime reflection | `T: reflect.Reflectable` | metadata universal e annotations |
| metatype | `TypeId.of<T>()` + generic/factory | `Type<T>` e dynamic construction |
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
| frontend inline | `fn<C>` | `fn<lang: .c>` |
| matrix | `[[1, 2], [3, 4]]` | `[1 2; 3 4]` |
| closure | `(x) => body` | `fn(x) { body }` |
| namespace import | `import std.http as http` | `import http as http from std.http` |
| região | `region request(using:, limit:)` | somente `Arena` manual |
| projeção borrowed | `view T` para famílias core | `StringView`/`Slice<T>` públicos e `Readonly<T>` profundo |
| stream assíncrono | `Stream<Item, Failure>` single-pass | sequence + iterator obrigatórios ou generator |
| loop de stream | `for try await item in stream` | `await stream` lê tudo ou callback push |
| item borrowed | `Stream<view String, E>` com provenance | `StringView` owned ou view transferable |
| channel | MPSC bounded com endpoints separados | bidirecional, MPMC ou unbounded por default |
| endpoint | `Channel<T><.send>` / `<.receive>` | `Sender<T>` / `Receiver<T>` ou direção runtime |
| falha de envio | enum devolve `T` | Boolean, panic ou perda do item |
| close de channel | último sender ou receiver gracioso; drop do receiver aborta | qualquer sender fecha globalmente |
| prefetch | adapter `buffer(capacity:)` explícito | watermark na assinatura ou buffer invisível |
| byte I/O | `ByteSource`/`ByteSink` async-first | `Reader`/`Writer` por backend ou interface sync condicional |
| destino de read | append em `Bytes` com spare privado | `ReadBuffer` público ou `inout view Bytes` genérico |
| EOF | `ReadStep.data(positive)` / `.end` | zero bytes e Boolean adicional |
| arquivo seekable | `read(at:)` posicional por default | cursor compartilhado e lock invisível |
| I/O blocking | adapter em executor bounded | bloquear worker cooperativo ou pool ilimitado |
| zero-copy | operação especializada e explícita em Pesquisa | `sendfile`/`mmap` invisível |
| construção textual | reserve/append no próprio `String` | `StringBuilder` público |
| storage textual | owner único flat + SSO invisível | COW baseline, rope universal ou threshold público |
| reserva textual | `tryReserve(minimumBytes:)` | capacity property e growth factor fixo |
| esvaziar texto | `clear()` / `reset()` / `takeAll()` | `clear(keepingCapacity: Bool)` |
| storage atômico | `var atomic value: T` | wrapper obrigatório ou behavior `Atomic` |
| order atômica | `load<.acquire>()` | `load(order:)` runtime e relaxed default |
| compare-exchange | enum result e orders estáticas | Boolean e combinações runtime |
| lock | `withLock` scoped | `lock`/`unlock` manual ou guard público |
| lock de task | aquisição async, closure sync | guard mantido através de `await` |
| RCU | tipo especializado após prova de reclamation | policy automática por property |

Preferência visual não é medida antes das tarefas de leitura e correção.

## 5. Gate para uma implementação

Cada arquivo DB2 precisa passar:

1. Tree-sitter sem error node;
2. formatter duas vezes sem diff;
3. highlighter com keywords e units corretas;
4. corpus negativo por feature;
5. type-check quando a fase correspondente existir;
6. runtime test ou oracle explícito quando houver lowering;
7. origem e revision disponíveis para geração do Book após o design freeze.

A integração avança em três gates cumulativos:

1. **Simulação:** `LastLightSimulation` gera os três relatórios sem deployment;
2. **Host:** CLI, TUI, line host e HTTP chegam ao mesmo `Command` e
   `AppResponse`;
3. **Turno do Horizonte Violeta:** o grafo real de services, FFI, compensação e
   observabilidade passa fault injection.

No estado atual, somente o gate sintático do Tree-sitter é executável. Os outros
gates são contratos de implementação. A documentação não os apresenta como
testes aprovados.
