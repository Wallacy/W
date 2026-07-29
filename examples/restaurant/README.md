# Restaurante Última Luz

> **Status:** corpus experimental da DB2 · 28 de julho de 2026

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
| `units.w` | SI, dimensão e units customizadas |
| `kitchen.w` | resources move-only, protocols térmicos, ranges e controle PID |
| `oracle.w` | matriz/tensor, `@`, shape e cálculo de lotes |
| `hardware.w` | fronteira C, layout e deallocator |
| `memory.w` | shared/weak, borrow suspenso, pinning e callback C |
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

Famílias: newtype, refinement, value parameter e conversão.

Aceite:

- `GuestId` não é `OrderId`;
- `GuestCount` inválido em literal falha no compile time;
- input dinâmico usa `try GuestCount(value)`;
- refined-to-base é implícito;
- layout materializado continua o do base type.

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
- `Money.zeroCredits` e `Money.fromMajor(...)` não criam estado global;
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
- o profile inclui tudo que o source do compiler pequeno usa;
- retirar qualquer capacidade W0 produz um diagnostic ligado ao fechamento;
- seed-C e W/MLIR emitem o mesmo bytecode e symbol table;
- duas compilações com a mesma recipe produzem os mesmos bytes;
- a ordem de iteração do Map não influencia a symbol table emitida;
- o source não depende de task, service, tensor, unit, behavior ou tagging;
- uma instruction depois de `serve` falha antes da emissão;
- o seed preserva typed errors, move e drop;
- o ensaio cresce pelos gates SH0–SH7 antes de declarar self-host completo.

Cobertura atual:

| Gate | Estado do ensaio |
|---|---|
| SH0–SH1 | parcial: lexer e parser da DSL, ainda não do source W |
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
- o `init` customizado remove o memberwise initializer público;
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

## 4. Alternativas visuais obrigatórias

O Book deve mostrar pares lado a lado:

| Tema | Forma líder | Contrafactual |
|---|---|---|
| unit | `9.81<m/s^2>` | `9.81[m/s^2]` |
| domain | `spawn<.compute> let x = ...` | `spawn<domain: .compute> let x = ...` |
| domain relacional | `spawn<.compute> let x = ...` | `spawn on .compute let x = ...` |
| refinement | `T<(predicate)>` | `T where (predicate)` |
| receiver | `String<(.count <= 40)>` | `String<(value.count <= 40)>` |
| generic refinado | `Array<u8><(.count <= 64)>` | `Array<[u8, (.count <= 64)]>` |
| retorno fluente | `mut fn advance(...): self` | retorno `self` implícito |
| associated member | `Money.zeroCredits` | mutable type storage |
| construção | `Type(field: value)` | `new Type(...)` e `Type {...}` |
| initializer | um `init` + factories nomeadas | overload e `async init` |
| computed property | `name: T { get => value }` | getter method e getter com efeitos |
| static record | `<{name: value}>` | extensão universal de tipo |
| static list | `<[a, b]>` ordenada | set implícito de constraints |
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
