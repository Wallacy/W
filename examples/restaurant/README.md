# Restaurante Última Luz

> **Status:** corpus experimental da DB2 · 29 de julho de 2026

O Restaurante Última Luz serve a última janela observável antes do encerramento
do turno cósmico. O cenário é original. Ele usa escala astronômica e humor
burocrático sem copiar personagens, frases ou eventos de outra obra.

O ensaio não prova que a linguagem está implementada. Ele pressiona a forma
integrada de [DESIGN.md](../../DESIGN.md).

## 1. Rota completa

```text
LastLight entry
  → parser streaming da Comanda de Íon
  → compiler W0 do Cardápio de Fótons
  → Salão Prisma
  → Cozinha de Maré Fria
  → controle PID do forno
  → Brigada do Cometa Manso
  → Observatório do Cometa Paciente
  → Oráculo de Mesas
  → Sonda de Aroma
  → Arquivo de Ecos shared/weak
  → Sino de Encerramento pinned
  → Recepção callable do Último Maitre
  → Conta da Aurora Tardia
  → resposta HTTP/TUI
```

O gate final se chama **Turno do Horizonte Violeta**. Uma falha injetada em cada
seta não pode deixar task, lease, buffer, mailbox item ou pagamento vivo sem
owner e estado observável.

## 2. Mapa de source

| Arquivo | Responsabilidade |
|---|---|
| `domain.w` | newtypes, refinements, enums e errors |
| `command.w` | parser streaming, spans, buffer limitado e comandos tipados |
| `text.w` | UTF-8, unidades de texto, normalização, bytes, paths e C strings |
| `collections.w` | arrays, slices, iteration, Map/Set, hashing e stable sort |
| `failure.w` | Option, Result, typed throws, panic, OOM e cleanup |
| `generics.w` | primary associated types, constraints, inference e witnesses |
| `enum_contracts.w` | subsets fechados de enum, narrowing e payloads |
| `state_transitions.w` | paths validados, typestate consuming e snapshots runtime |
| `reflection.w` | TypeId local, reflection opt-in, synthesis e visibilidade |
| `rest_arguments.w` | rest homogêneo, expansão `each`, ownership e call shape |
| `units.w` | SI, dimensão e units customizadas |
| `kitchen.w` | resources move-only, protocols térmicos, ranges e controle PID |
| `oracle.w` | matriz/tensor, `@`, shape e cálculo de lotes |
| `hardware.w` | fronteira C, layout e deallocator |
| `memory.w` | ownership, enum subset, niches, pinning e callback C |
| `callables.w` | function pointer, opaque callable, erasure e callable modes |
| `menu_compiler.w` | compiler pequeno restrito ao profile `bootstrap.w0` |
| `execution.w` | task groups bounded, outcomes, ordering e cancelamento |
| `billing.w` | Money, idempotência, existential, opaque return e behavior |
| `dining.w` | serial turn, backpressure, applause e resposta |
| `restaurant.w` | integração de services, tasks, ownership e compensação |
| `app.w` | CLI, HTTP, Context e entries |

Esses arquivos usam a forma líder da DB2. A versão DB1 está no
[arquivo histórico](../../../Y/W/archive/db1-2026-07-27/examples/restaurant/).

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
- `String.fromUtf8` falha no byte inválido exato.
- `CString` rejeita NUL interno.
- `Path` nativo converte para `Utf8Path` de forma fallible.
- `PackagePath` mantém uma forma portátil separada.

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

Famílias: Array, fixed array, Slice, Map, Set, hashing, iteration e sort.

Aceite:

- `[0; 32]` avalia o valor uma vez e cria um fixed array;
- um Slice impede mutation estrutural que poderia mover seu storage;
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

## 4. Alternativas visuais obrigatórias

O Book deve mostrar pares lado a lado:

| Tema | Forma líder | Contrafactual |
|---|---|---|
| unit | `9.81<m/s^2>` | `9.81[m/s^2]` |
| domain | `spawn<.compute> let x = ...` | `spawn<domain: .compute> let x = ...` |
| domain relacional | `spawn<.compute> let x = ...` | `spawn on .compute let x = ...` (**Rejeitado por enquanto**) |
| domain customizado | `spawn<domain: LastLightDomain.thermal>` | `"thermal"` ou keyword global |
| QoS | descriptor/policy de group | `.background` como domain |
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
