# Catálogo humano de diagnostics W

> Este arquivo é uma projeção humana gerada de `tooling/diagnostic-catalog.json`.
> O JSON é a superfície de máquina para os metadados dos diagnostics.
> O contrato e a semântica pertencem a `DESIGN.md`.
> O gate verifica drift estrutural do catálogo, do output e dos vínculos de código, família e heading.
> O gate não prova equivalência textual entre `meaning` e `DESIGN.md`.
> `DESIGN.md` continua a autoridade semântica.
> Este documento não cria uma autoridade nova.
> Não edite este arquivo. Use `bun tooling/diagnostic-catalog.mjs --write`.

- Catalog digest: `sha256:a925b7c9fdb71c30a02ca45f56ff6e5cdcc9961601933c94f2264cb7176797f2`
- Entries: `331`
- Families: `51`
- Design references: `237` exact, `94` family
- States: `active` 328, `reserved` 3

## Como ler

Cada entrada mantém os nomes do JSON para facilitar a busca cruzada.

- `state` identifica o estado de ciclo de vida do código.
- `phase` e `severity` são os valores efetivos após a expansão de `profile`.
- `meaning` vem do catálogo. O contrato normativo está no link de `DESIGN.md`.
- O catálogo e seus oracles de design não provam que o compiler atual emite todos os códigos.
- `requiredFacts`, `labelRoles` e `fixes` descrevem os dados exigidos, os papéis de label e a aplicabilidade do fix.
- `exact` usa a primeira ocorrência literal do código em `DESIGN.md`.
- `family` usa a primeira ocorrência literal de `W-FAMILY-*` quando o código não aparece literalmente.

## Índice por família

- [`W-ALLOCATOR`](#w-allocator) — 11 entradas
- [`W-ATOMIC`](#w-atomic) — 16 entradas
- [`W-BEHAVIOR`](#w-behavior) — 5 entradas
- [`W-BORROW`](#w-borrow) — 12 entradas
- [`W-CAPABILITY`](#w-capability) — 1 entrada
- [`W-CONST`](#w-const) — 7 entradas
- [`W-CONTEXT`](#w-context) — 7 entradas
- [`W-CONTRACT`](#w-contract) — 5 entradas
- [`W-DIAGNOSTIC`](#w-diagnostic) — 1 entrada
- [`W-DLPACK`](#w-dlpack) — 32 entradas
- [`W-DOC`](#w-doc) — 2 entradas
- [`W-EFFECT`](#w-effect) — 2 entradas
- [`W-EXECUTION`](#w-execution) — 3 entradas
- [`W-EXPORT`](#w-export) — 7 entradas
- [`W-EXPR`](#w-expr) — 4 entradas
- [`W-FACET`](#w-facet) — 6 entradas
- [`W-FLOW`](#w-flow) — 2 entradas
- [`W-FMT`](#w-fmt) — 2 entradas
- [`W-FOREIGN`](#w-foreign) — 9 entradas
- [`W-GENERIC`](#w-generic) — 5 entradas
- [`W-INIT`](#w-init) — 1 entrada
- [`W-JUPYTER`](#w-jupyter) — 8 entradas
- [`W-LABEL`](#w-label) — 3 entradas
- [`W-LAZY`](#w-lazy) — 6 entradas
- [`W-LEX`](#w-lex) — 1 entrada
- [`W-LOCK`](#w-lock) — 13 entradas
- [`W-MATCH`](#w-match) — 3 entradas
- [`W-MEMORY`](#w-memory) — 1 entrada
- [`W-MOVE`](#w-move) — 1 entrada
- [`W-OWNERSHIP`](#w-ownership) — 8 entradas
- [`W-PARSE`](#w-parse) — 29 entradas
- [`W-PATTERN`](#w-pattern) — 6 entradas
- [`W-PIPE`](#w-pipe) — 4 entradas
- [`W-PIPELINE`](#w-pipeline) — 5 entradas
- [`W-PLACEMENT`](#w-placement) — 4 entradas
- [`W-PRESENTATION`](#w-presentation) — 10 entradas
- [`W-RUN`](#w-run) — 16 entradas
- [`W-SEM`](#w-sem) — 1 entrada
- [`W-SESSION`](#w-session) — 28 entradas
- [`W-SNAPSHOT`](#w-snapshot) — 10 entradas
- [`W-STD`](#w-std) — 1 entrada
- [`W-STREAM`](#w-stream) — 1 entrada
- [`W-SUSPEND`](#w-suspend) — 5 entradas
- [`W-SYNC`](#w-sync) — 1 entrada
- [`W-TIME`](#w-time) — 2 entradas
- [`W-TLS`](#w-tls) — 3 entradas
- [`W-TYPE`](#w-type) — 7 entradas
- [`W-UNIT`](#w-unit) — 1 entrada
- [`W-USE`](#w-use) — 1 entrada
- [`W-WIRE`](#w-wire) — 1 entrada
- [`W-YIELD`](#w-yield) — 11 entradas

## Entradas

### W-ALLOCATOR

#### W-ALLOCATOR-0001

- `state`: `active`
- `phase`: `semantic.capability`
- `severity`: `error`
- `meaning`: an allocator contextual parameter is not the unique standard ref Allocator slot

- `requiredFacts`:
  - `declaration`: `string`
  - `parameter`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-ALLOCATOR-0001` — [9.6.4 Origem e mobilidade](DESIGN.md#964-origem-e-mobilidade)

#### W-ALLOCATOR-0002

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a value, borrow, child, wait, or dependent escapes its allocator scope

- `requiredFacts`:
  - `allocator`: `string`
  - `boundary`: `string`
  - `reason`: `string`
  - `value`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-ALLOCATOR-0002` — [9.6.4 Origem e mobilidade](DESIGN.md#964-origem-e-mobilidade)

#### W-ALLOCATOR-0003

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a local allocation origin crosses an async or service boundary without stable storage or rehome

- `requiredFacts`:
  - `allocator`: `string`
  - `boundary`: `string`
  - `origin`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-ALLOCATOR-0003` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-ALLOCATOR-0004

- `state`: `active`
- `phase`: `semantic.capability`
- `severity`: `error`
- `meaning`: a custom allocator plan lacks a nonzero providerDigest[32], version, failure, deallocator, mobility, const descriptor, or consuming open hook

- `requiredFacts`:
  - `missingFacts`: `string-set`
  - `plan`: `string`
  - `provider`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-ALLOCATOR-0004` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-ALLOCATOR-0005

- `state`: `active`
- `phase`: `semantic.capability`
- `severity`: `error`
- `meaning`: a fixed allocator capacity or placement is unsupported by the selected target profile

- `requiredFacts`:
  - `capacity`: `string`
  - `placement`: `string`
  - `profile`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-ALLOCATOR-0005` — [9.6.4 Origem e mobilidade](DESIGN.md#964-origem-e-mobilidade)

#### W-ALLOCATOR-0006

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: an allocator block closes with an undrained child, wait, loan, or dependent

- `requiredFacts`:
  - `active`: `string-set`
  - `allocator`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-ALLOCATOR-0006` — [9.6.4 Origem e mobilidade](DESIGN.md#964-origem-e-mobilidade)

#### W-ALLOCATOR-0007

- `state`: `active`
- `phase`: `semantic.capability`
- `severity`: `error`
- `meaning`: allocator plan admission or lease acquisition fails before the body and binding exist

- `requiredFacts`:
  - `plan`: `string`
  - `provider`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-ALLOCATOR-0007` — [9.6.4 Origem e mobilidade](DESIGN.md#964-origem-e-mobilidade)

#### W-ALLOCATOR-0008

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `meaning`: a foreign ABI omits the explicit allocator contextual slot

- `requiredFacts`:
  - `abi`: `string`
  - `declaration`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-ALLOCATOR-0008` — [9.5 Declaração lexical de allocator](DESIGN.md#95-declaração-lexical-de-allocator)

#### W-ALLOCATOR-0009

- `state`: `active`
- `phase`: `semantic.capability`
- `severity`: `error`
- `meaning`: a fixed allocator reservation or admission is not proven infallible and the declaration omits try

- `requiredFacts`:
  - `allocator`: `string`
  - `profile`: `string`
  - `reason`: `string`
  - `target`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-ALLOCATOR-0009` — [9.6.4 Origem e mobilidade](DESIGN.md#964-origem-e-mobilidade)

#### W-ALLOCATOR-0010

- `state`: `active`
- `phase`: `semantic.capability`
- `severity`: `error`
- `meaning`: a contextual allocator slot is omitted without a compatible current allocator

- `requiredFacts`:
  - `available`: `string-set`
  - `expected`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-ALLOCATOR-0010` — [9.5 Declaração lexical de allocator](DESIGN.md#95-declaração-lexical-de-allocator)

#### W-ALLOCATOR-0011

- `state`: `active`
- `phase`: `semantic.capability`
- `severity`: `error`
- `meaning`: an initializer or construction declaration declares a contextual allocator slot

- `requiredFacts`:
  - `declaration`: `string`
  - `kind`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-ALLOCATOR-0011` — [9.5 Declaração lexical de allocator](DESIGN.md#95-declaração-lexical-de-allocator)

### W-ATOMIC

#### W-ATOMIC-0001

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `profile`: `atomic-type`
- `meaning`: an Atomic value record is not a closed canonical value-only record

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-record`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0001` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0002

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `profile`: `atomic-type`
- `meaning`: an Atomic value record uses an unsupported field, width, or encoding

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-record`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0002` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0003

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `atomic-interface`
- `meaning`: the target does not provide the requested atomic carrier width or progress fact

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `interface`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-interface`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0003` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0004

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `profile`: `atomic-effect`
- `meaning`: the declared atomic fallback is not compatible with the surrounding context

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `operation`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-operation`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0004` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0005

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `profile`: `atomic-effect`
- `meaning`: an atomic compare-exchange operation or memory-order pair is invalid

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `operation`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-operation`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0005` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0006

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `atomic-interface`
- `meaning`: SemanticInterfaceKey, WAbiKey, representation mapping, or provider identity drifts

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `interface`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-interface`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0006` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0007

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `profile`: `atomic-ownership`
- `meaning`: a generation handle would wrap or its exhausted slot was not retired

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `handle`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0007` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0008

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `profile`: `atomic-ownership`
- `meaning`: a stale handle or owner-table lookup was not checked before dereference

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `handle`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0008` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0009

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `profile`: `atomic-ownership`
- `meaning`: a reclamation adapter omits registration, access, unlink, quiescence, or exact drop

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `handle`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0009` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0010

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `profile`: `atomic-ownership`
- `meaning`: a universal raw-pointer, tagged-pointer, or RCU reclamation path is rejected

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `handle`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0010` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0011

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `profile`: `atomic-ownership`
- `meaning`: an FFI callback drain, unregister, shutdown, or in-flight receipt is incomplete

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `handle`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0011` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0012

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `profile`: `atomic-effect`
- `meaning`: an atomic fallback requests hidden allocation; the current contract is allocation-free per instance and operation

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `operation`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-operation`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0012` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0013

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `profile`: `atomic-type`
- `meaning`: a canonical atomic bit encoding has invalid direction, order, signed code, enum code, or nonzero high bits

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-record`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0013` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0014

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `profile`: `atomic-effect`
- `meaning`: an atomic load, store, exchange, or compare-exchange is a suspension or cancellation point

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `operation`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-operation`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0014` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0015

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `profile`: `atomic-effect`
- `meaning`: a fallback that blocks a thread is used in signal, freestanding, cooperative, or nonblocking context

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `operation`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-operation`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0015` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

#### W-ATOMIC-0016

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `profile`: `atomic-effect`
- `meaning`: parking and thread-blocking facts are conflated or the separate Atomic.wait maySuspend API is hidden

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `operation`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `atomic-operation`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-ATOMIC-0016` — [23.9 ATOM0-G1 — contrato atômico fechado](DESIGN.md#239-atom0-g1-contrato-atômico-fechado)

### W-BEHAVIOR

#### W-BEHAVIOR-0002

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a behavior composition is not a nominal labeled tuple

- `requiredFacts`:
  - `actual`: `string`
  - `behavior`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-BEHAVIOR-0002` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-BEHAVIOR-0003

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a behavior composition contains more than one storage behavior

- `requiredFacts`:
  - `behavior`: `string`
  - `components`: `string[]`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-BEHAVIOR-0003` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-BEHAVIOR-0004

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a behavior alias is duplicated, cyclic, or has no facet path

- `requiredFacts`:
  - `alias`: `string`
  - `behavior`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-BEHAVIOR-0004` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-BEHAVIOR-0005

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a behavior observer hook has an invalid signature or effect

- `requiredFacts`:
  - `behavior`: `string`
  - `hook`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-BEHAVIOR-0005` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-BEHAVIOR-0006

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a behavior facet path violates composition ownership or immediate-use rules

- `requiredFacts`:
  - `path`: `string`
  - `place`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-BEHAVIOR-0006` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

### W-BORROW

#### W-BORROW-0001

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a borrow conflicts, escapes, or crosses an invalid suspension

- `requiredFacts`:
  - `binding`: `string`
  - `borrowMode`: `string`
  - `conflict`: `string`

- `labelRoles`:
  - `borrow-origin`: minimum `1`, maximum `1`
  - `borrow-use`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-BORROW-0001` — [Diagnostics](DESIGN.md#diagnostics-1)

#### W-BORROW-0002

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a loan overlaps another loan or structural mutation at the same storage

- `requiredFacts`:
  - `conflictingLoan`: `string`
  - `mode`: `string`
  - `owner`: `string`
  - `place`: `string`

- `labelRoles`:
  - `loan-conflict`: minimum `1`, maximum `1`
  - `loan-origin`: minimum `1`, maximum `1`

- `fixes`:
  - `split-place-or-scope`: `review`

- Design authority: `exact` `W-BORROW-0002` — [9.2.1.1 Escapes, destruction e diagnostics](DESIGN.md#9211-escapes-destruction-e-diagnostics)

#### W-BORROW-0003

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a lifetime-dependent value crosses a boundary without surviving origins

- `requiredFacts`:
  - `binding`: `string`
  - `boundary`: `string`
  - `missingOrigins`: `string-set`
  - `type`: `string`

- `labelRoles`:
  - `dependent-value`: minimum `1`, maximum `1`
  - `origin`: minimum `1`, maximum `unbounded`

- `fixes`:
  - `materialize-dependent-value`: `review`

- Design authority: `exact` `W-BORROW-0003` — [9.2.1.1 Escapes, destruction e diagnostics](DESIGN.md#9211-escapes-destruction-e-diagnostics)

#### W-BORROW-0004

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a loan crosses suspension with unstable storage or incomplete cleanup

- `requiredFacts`:
  - `cleanup`: `string`
  - `owner`: `string`
  - `place`: `string`
  - `stability`: `string`

- `labelRoles`:
  - `loan-origin`: minimum `1`, maximum `1`
  - `suspension`: minimum `1`, maximum `1`

- `fixes`:
  - `stabilize-or-end-loan`: `review`

- Design authority: `exact` `W-BORROW-0004` — [9.2.1.1 Escapes, destruction e diagnostics](DESIGN.md#9211-escapes-destruction-e-diagnostics)

#### W-BORROW-0005

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a reborrow parent is frozen while a child loan is active

- `requiredFacts`:
  - `childLoan`: `string`
  - `owner`: `string`
  - `parentLoan`: `string`
  - `place`: `string`

- `labelRoles`:
  - `reborrow-child`: minimum `1`, maximum `1`
  - `reborrow-parent`: minimum `1`, maximum `1`

- `fixes`:
  - `end-child-loan`: `review`

- Design authority: `exact` `W-BORROW-0005` — [9.2.1.1 Escapes, destruction e diagnostics](DESIGN.md#9211-escapes-destruction-e-diagnostics)

#### W-BORROW-0006

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: an access, borrow, or structural mutation conflicts with a live dependency edge

- `requiredFacts`:
  - `access`: `string`
  - `edgeMode`: `string`
  - `origin`: `string`
  - `owner`: `string`
  - `place`: `string`

- `labelRoles`:
  - `dependency-origin`: minimum `1`, maximum `1`

- `fixes`:
  - `end-dependent-value`: `review`

- Design authority: `exact` `W-BORROW-0006` — [9.2.1.1 Escapes, destruction e diagnostics](DESIGN.md#9211-escapes-destruction-e-diagnostics)

#### W-BORROW-0007

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a suspension has an unstable referent dependency

- `requiredFacts`:
  - `aggregate`: `string`
  - `origin`: `string`
  - `referentOwner`: `string`
  - `stability`: `string`

- `labelRoles`:
  - `referent-owner`: minimum `1`, maximum `1`

- `fixes`:
  - `stabilize-referent`: `review`

- Design authority: `exact` `W-BORROW-0007` — [9.2.1.1 Escapes, destruction e diagnostics](DESIGN.md#9211-escapes-destruction-e-diagnostics)

#### W-BORROW-0008

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: an owner move, drop, or pin is blocked by a dynamic dependency edge

- `requiredFacts`:
  - `edgeMode`: `string`
  - `operation`: `string`
  - `owner`: `string`
  - `referent`: `string`

- `labelRoles`:
  - `dependent-value`: minimum `1`, maximum `1`
  - `referent-owner`: minimum `1`, maximum `1`

- `fixes`:
  - `drop-dependent-value`: `review`

- Design authority: `exact` `W-BORROW-0008` — [9.2.1.1 Escapes, destruction e diagnostics](DESIGN.md#9211-escapes-destruction-e-diagnostics)

#### W-BORROW-0009

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: an access through a dependency edge requests authority that its mode does not grant

- `requiredFacts`:
  - `access`: `string`
  - `edgeId`: `string`
  - `edgeMode`: `string`
  - `origin`: `string`
  - `owner`: `string`
  - `place`: `string`

- `labelRoles`:
  - `dependency-origin`: minimum `1`, maximum `1`
  - `dependent-access`: minimum `1`, maximum `1`

- `fixes`:
  - `remove-write`: `review`
  - `use-exclusive-dependency`: `review`

- Design authority: `exact` `W-BORROW-0009` — [9.2.1.1 Escapes, destruction e diagnostics](DESIGN.md#9211-escapes-destruction-e-diagnostics)

#### W-BORROW-0010

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: shared construction receives a payload with a dynamic borrow origin

- `requiredFacts`:
  - `operation`: `string`
  - `origins`: `string-set`
  - `owner`: `string`

- `labelRoles`:
  - `dependent-value`: minimum `1`, maximum `1`
  - `shared-binding`: minimum `1`, maximum `1`

- `fixes`:
  - `materialize-before-share`: `review`

- Design authority: `exact` `W-BORROW-0010` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-BORROW-0011

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a bodyless borrowed result has multiple compatible origins without an authoritative receiver or body

- `requiredFacts`:
  - `authority`: `string`
  - `compatibleInputs`: `string-set`
  - `declarationKind`: `string`
  - `result`: `string[]`

- `labelRoles`:
  - `borrow-result`: minimum `1`, maximum `unbounded`
  - `compatible-origin`: minimum `2`, maximum `unbounded`

- `fixes`:
  - `return-owned-nominal`: `review`
  - `select-unique-origin`: `review`

- Design authority: `exact` `W-BORROW-0011` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-BORROW-0012

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a borrows source clause has an invalid slot, edge mode, duplicate mapping, or proof divergence

- `requiredFacts`:
  - `authority`: `string`
  - `declarationKind`: `string`
  - `issue`: `string`
  - `writtenResult`: `string`
  - `writtenSources`: `string[]`

- `labelRoles`:
  - `borrow-clause`: minimum `1`, maximum `1`
  - `borrow-proof`: minimum `0`, maximum `1`

- `fixes`:
  - `align-body-or-witness`: `review`
  - `repair-borrows-clause`: `review`

- Design authority: `exact` `W-BORROW-0012` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

### W-CAPABILITY

#### W-CAPABILITY-0001

- `state`: `active`
- `phase`: `semantic.capability`
- `severity`: `error`
- `meaning`: an operational effect lacks its required capability owner

- `requiredFacts`:
  - `effect`: `string`
  - `expectedOwner`: `string`
  - `provider`: `string`

- `labelRoles`:
  - `transaction-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CAPABILITY-0001` — [Diagnostics](DESIGN.md#diagnostics-1)

### W-CONST

#### W-CONST-0001

- `state`: `active`
- `phase`: `semantic.const`
- `severity`: `error`
- `meaning`: an operation, call, or target semantic is not const-safe

- `requiredFacts`:
  - `callChain`: `string[]`
  - `operation`: `string`
  - `reason`: `string`
  - `symbol`: `string`

- `labelRoles`:
  - `const-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONST-0001` — [3.6.2 Programa const-safe](DESIGN.md#362-programa-const-safe)

#### W-CONST-0002

- `state`: `active`
- `phase`: `semantic.const`
- `severity`: `error`
- `meaning`: the const dependency graph contains a cycle

- `requiredFacts`:
  - `cycle`: `string[]`
  - `cycleLength`: `integer`

- `labelRoles`:
  - `cycle-member`: minimum `2`, maximum `unbounded`

- `fixes`:
  - none

- Design authority: `exact` `W-CONST-0002` — [3.6.5 Termination, quotas e cache](DESIGN.md#365-termination-quotas-e-cache)

#### W-CONST-0003

- `state`: `active`
- `phase`: `semantic.const`
- `severity`: `error`
- `meaning`: const evaluation exceeds a deterministic quota

- `requiredFacts`:
  - `callChain`: `string[]`
  - `consumed`: `integer`
  - `limit`: `integer`
  - `quota`: `string`

- `labelRoles`:
  - `const-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONST-0003` — [3.6.5 Termination, quotas e cache](DESIGN.md#365-termination-quotas-e-cache)

#### W-CONST-0004

- `state`: `active`
- `phase`: `semantic.const`
- `severity`: `error`
- `meaning`: a valid const predicate rejects a static argument and publishes bounded rejection evidence

- `requiredFacts`:
  - `argument`: `string`
  - `failure`: `string`
  - `head`: `string`
  - `predicate`: `string`
  - `rejectionTrace`: `string[]`

- `labelRoles`:
  - `contract-head`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONST-0004` — [3.6.8 Diagnostics](DESIGN.md#368-diagnostics)

#### W-CONST-0005

- `state`: `active`
- `phase`: `semantic.const`
- `severity`: `error`
- `meaning`: a typed error escapes a required const context

- `requiredFacts`:
  - `errorCase`: `string`
  - `errorType`: `string`
  - `requiredBy`: `string`

- `labelRoles`:
  - `const-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONST-0005` — [3.6.2 Programa const-safe](DESIGN.md#362-programa-const-safe)

#### W-CONST-0006

- `state`: `active`
- `phase`: `semantic.const`
- `severity`: `error`
- `meaning`: const evaluation reaches panic

- `requiredFacts`:
  - `callChain`: `string[]`
  - `operation`: `string`
  - `panicKind`: `string`
  - `requiredBy`: `string`

- `labelRoles`:
  - `const-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONST-0006` — [3.6.2 Programa const-safe](DESIGN.md#362-programa-const-safe)

#### W-CONST-0007

- `state`: `active`
- `phase`: `semantic.const`
- `severity`: `error`
- `meaning`: a call parameter requires a compile-time value that is not available

- `requiredFacts`:
  - `callable`: `string`
  - `parameter`: `string`
  - `requiredPredicate`: `string`
  - `sourceCategory`: `string`

- `labelRoles`:
  - `call-owner`: minimum `1`, maximum `1`
  - `const-requirement`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONST-0007` — [3.6.1 Contextos e superfície](DESIGN.md#361-contextos-e-superfície)

### W-CONTEXT

#### W-CONTEXT-0001

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `profile`: `context-local`
- `meaning`: a task-local descriptor is not a valid nominal constant

- `requiredFacts`:
  - `actual`: `string`
  - `descriptor`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `context-binding`: minimum `0`, maximum `1`
  - `context-use`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONTEXT-0001` — [19.3.4 Contexto de task e storage de thread](DESIGN.md#1934-contexto-de-task-e-storage-de-thread)

#### W-CONTEXT-0002

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `profile`: `context-local`
- `meaning`: a task-local payload is not shareable

- `requiredFacts`:
  - `actual`: `string`
  - `descriptor`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `context-binding`: minimum `0`, maximum `1`
  - `context-use`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONTEXT-0002` — [19.3.4 Contexto de task e storage de thread](DESIGN.md#1934-contexto-de-task-e-storage-de-thread)

#### W-CONTEXT-0003

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `profile`: `context-local`
- `meaning`: a task-local binding is mutable or carries hidden authority

- `requiredFacts`:
  - `actual`: `string`
  - `descriptor`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `context-binding`: minimum `0`, maximum `1`
  - `context-use`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONTEXT-0003` — [19.3.4 Contexto de task e storage de thread](DESIGN.md#1934-contexto-de-task-e-storage-de-thread)

#### W-CONTEXT-0004

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `profile`: `context-local`
- `meaning`: a task-local binding requires hidden copy, retain, or ownership

- `requiredFacts`:
  - `actual`: `string`
  - `descriptor`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `context-binding`: minimum `0`, maximum `1`
  - `context-use`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONTEXT-0004` — [19.3.4 Contexto de task e storage de thread](DESIGN.md#1934-contexto-de-task-e-storage-de-thread)

#### W-CONTEXT-0005

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `profile`: `context-local`
- `meaning`: an unstructured boundary attempts to inherit a task-local binding

- `requiredFacts`:
  - `actual`: `string`
  - `descriptor`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `context-binding`: minimum `0`, maximum `1`
  - `context-use`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONTEXT-0005` — [19.3.4 Contexto de task e storage de thread](DESIGN.md#1934-contexto-de-task-e-storage-de-thread)

#### W-CONTEXT-0006

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `profile`: `context-local`
- `meaning`: a task-local scope removes its binding before child drain

- `requiredFacts`:
  - `actual`: `string`
  - `descriptor`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `context-binding`: minimum `0`, maximum `1`
  - `context-use`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONTEXT-0006` — [19.3.4 Contexto de task e storage de thread](DESIGN.md#1934-contexto-de-task-e-storage-de-thread)

#### W-CONTEXT-0007

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `profile`: `context-local`
- `meaning`: a task-local dependency escapes or remains open at scope exit

- `requiredFacts`:
  - `actual`: `string`
  - `descriptor`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `context-binding`: minimum `0`, maximum `1`
  - `context-use`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONTEXT-0007` — [19.3.4 Contexto de task e storage de thread](DESIGN.md#1934-contexto-de-task-e-storage-de-thread)

### W-CONTRACT

#### W-CONTRACT-0001

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a contract argument names a slot that its head does not publish

- `requiredFacts`:
  - `availableSlots`: `string-set`
  - `head`: `string`
  - `slot`: `string`

- `labelRoles`:
  - `contract-head`: minimum `1`, maximum `1`
  - `slot-declaration`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONTRACT-0001` — [Envelope de contrato](DESIGN.md#envelope-de-contrato)

#### W-CONTRACT-0002

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a contract argument kind or value domain is incompatible with its resolved slot

- `requiredFacts`:
  - `actualKind`: `string`
  - `expectedKind`: `string`
  - `head`: `string`
  - `slot`: `string`

- `labelRoles`:
  - `contract-head`: minimum `1`, maximum `1`
  - `slot-declaration`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONTRACT-0002` — [Envelope de contrato](DESIGN.md#envelope-de-contrato)

#### W-CONTRACT-0003

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a contract refinement predicate does not produce Bool

- `requiredFacts`:
  - `expectedType`: `string`
  - `head`: `string`
  - `predicateType`: `string`

- `labelRoles`:
  - `contract-head`: minimum `1`, maximum `1`
  - `slot-declaration`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONTRACT-0003` — [Tokenização e recovery](DESIGN.md#tokenização-e-recovery)

#### W-CONTRACT-0004

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a contract slot is duplicated

- `requiredFacts`:
  - `head`: `string`
  - `slot`: `string`
  - `slotOrder`: `string[]`
  - `violation`: `string`

- `labelRoles`:
  - `contract-head`: minimum `1`, maximum `1`
  - `slot-declaration`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONTRACT-0004` — [Envelope de contrato](DESIGN.md#envelope-de-contrato)

#### W-CONTRACT-0005

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a later contract envelope does not apply to the result of the previous envelope

- `requiredFacts`:
  - `applicableKinds`: `string-set`
  - `envelopeKind`: `string`
  - `head`: `string`

- `labelRoles`:
  - `contract-head`: minimum `1`, maximum `1`
  - `slot-declaration`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-CONTRACT-0005` — [Tokenização e recovery](DESIGN.md#tokenização-e-recovery)

### W-DIAGNOSTIC

#### W-DIAGNOSTIC-0001

- `state`: `active`
- `phase`: `build`
- `severity`: `error`
- `meaning`: diagnostic analysis stopped at an explicit limit

- `requiredFacts`:
  - `emitted`: `integer`
  - `incomplete`: `boolean`
  - `limit`: `integer`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-DIAGNOSTIC-0001` — [21.6 CLI](DESIGN.md#216-cli)

### W-DLPACK

#### W-DLPACK-0001

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a DLPack operation or carrier fact is missing or unknown

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0002

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a legacy unversioned DLPack tensor or capsule name is rejected

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0003

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a DLPack major version mismatch requires deleter release before dereference

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0004

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a DLPack minor version contains an unknown field

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0005

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a DLPack flag is unknown

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0006

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a DLPack dtype is unsupported or lacks a proven W storage mapping

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0007

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a DLPack dtype uses a non-native endian

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0008

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a DLPack dtype uses a lane count other than one

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0009

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: DLPack rank, element, span, or offset arithmetic exceeds a bound

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0010

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: DLPack shape, stride, data, or empty-tensor rules are invalid

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0011

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a tensor Device identity is not provider-scoped

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0012

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: tensor data alignment is insufficient for the consumer

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0013

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: DLPack provenance is missing or input is untrusted

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0014

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a DLPack lease, release job, wait, or limit exceeds its bound

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0015

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a DLPack lifecycle operation is outside its owner phase

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0016

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a DLPack metadata or control allocation exceeds its bound

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0017

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a host oracle operation supplies a derived conclusion instead of evidence

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0018

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: zero-copy open rejects a producer-copied tensor

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0019

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a typed or dynamic DLPack bind does not match dtype, rank, shape, or layout

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0020

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a raw stream integer or missing queue is used for a stream device

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0021

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a tensor Queue and Device do not match

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0022

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a provider Queue lacks a bindQueue or producerWait happens-before receipt

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0023

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a DLPack capsule name or one-shot rename is invalid

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0024

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a DLPack deleter or release obligation is called more than once

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0025

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a tensor view escapes its lexical callback or creates an unproven inout value-in/value-out alias

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0026

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: close or cancellation skips view, queue, or consumer work drain

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0027

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: close failed and the owner requires runtime quarantine

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0028

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: payload materialization or copy-to-host is not explicit

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0029

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a consuming export lacks uniqueness or lets a borrowed owner escape

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0030

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: Python GIL, attached thread state, interpreter lease, or finalization drain is invalid

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0031

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: a DLPack receipt or diagnostic exposes a redacted raw pointer or private field

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

#### W-DLPACK-0032

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `dlpack`
- `meaning`: the C Exchange N0 fast path violates its static Python scope, current-stream, no-escape, no-suspension, or producer-drain contract

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `device`: minimum `0`, maximum `1`
  - `queue`: minimum `0`, maximum `1`
  - `tensor-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-DLPACK-*` — [17.1.5 Bounds, receipts e rejeições](DESIGN.md#1715-bounds-receipts-e-rejeições)

### W-DOC

#### W-DOC-0003

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `meaning`: a documentation example has duplicate or missing terminal outcome

- `requiredFacts`:
  - `example`: `string`
  - `reason`: `string`
  - `terminals`: `string[]`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-DOC-0003` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

#### W-DOC-0005

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `meaning`: a documentation example uses an ambient effect without an explicit fixture

- `requiredFacts`:
  - `effect`: `string`
  - `example`: `string`
  - `fixture`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-DOC-0005` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

### W-EFFECT

#### W-EFFECT-0010

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: an effect prefix is missing, redundant, or outside canonical order

- `requiredFacts`:
  - `actualPrefixOrder`: `string[]`
  - `canonicalPrefixOrder`: `string[]`
  - `effects`: `string-set`
  - `violation`: `string`

- `labelRoles`:
  - `effect-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-EFFECT-0010` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-EFFECT-0011

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a required effect is not handled or propagated

- `requiredFacts`:
  - `effects`: `string-set`

- `labelRoles`:
  - none

- `fixes`:
  - `propagate-error-and-suspension`: `review`

- Design authority: `exact` `W-EFFECT-0011` — [Diagnostics](DESIGN.md#diagnostics-1)

### W-EXECUTION

#### W-EXECUTION-0001

- `state`: `active`
- `phase`: `source.capability`
- `severity`: `error`
- `meaning`: the execution contextual root is unavailable in the current runtime phase

- `requiredFacts`:
  - `member`: `string`
  - `profile`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-EXECUTION-0001` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

#### W-EXECUTION-0002

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a root-bound authority or borrow obtained from execution crosses its structured boundary; value copying does not make the crossing legal

- `requiredFacts`:
  - `boundary`: `string`
  - `member`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-EXECUTION-0002` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

#### W-EXECUTION-0003

- `state`: `active`
- `phase`: `source.capability`
- `severity`: `error`
- `meaning`: an execution member or facet is unavailable in the selected target or product profile

- `requiredFacts`:
  - `member`: `string`
  - `profile`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-EXECUTION-0003` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

### W-EXPORT

#### W-EXPORT-0001

- `state`: `active`
- `phase`: `package`
- `severity`: `error`
- `profile`: `notebook-export`
- `meaning`: a notebook or cell identity is invalid

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `cell-owner`: minimum `0`, maximum `1`
  - `receipt`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-EXPORT-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-EXPORT-0002

- `state`: `active`
- `phase`: `package`
- `severity`: `error`
- `profile`: `notebook-export`
- `meaning`: a selected notebook cell lacks a receipt manifest record

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `cell-owner`: minimum `0`, maximum `1`
  - `receipt`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-EXPORT-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-EXPORT-0003

- `state`: `active`
- `phase`: `package`
- `severity`: `error`
- `profile`: `notebook-export`
- `meaning`: a receipt source, generation, binding, lock, context, target, or effect proof mismatches

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `cell-owner`: minimum `0`, maximum `1`
  - `receipt`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-EXPORT-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-EXPORT-0004

- `state`: `active`
- `phase`: `package`
- `severity`: `error`
- `profile`: `notebook-export`
- `meaning`: invalidation or redefinition prevents a lossless export

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `cell-owner`: minimum `0`, maximum `1`
  - `receipt`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-EXPORT-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-EXPORT-0005

- `state`: `active`
- `phase`: `package`
- `severity`: `error`
- `profile`: `notebook-export`
- `meaning`: an export contains unknown effects, unresolved input, secret, degraded owner, or live resource

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `cell-owner`: minimum `0`, maximum `1`
  - `receipt`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-EXPORT-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-EXPORT-0006

- `state`: `active`
- `phase`: `package`
- `severity`: `error`
- `profile`: `notebook-export`
- `meaning`: dependencies and effects have no deterministic lossless order

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `cell-owner`: minimum `0`, maximum `1`
  - `receipt`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-EXPORT-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-EXPORT-0007

- `state`: `active`
- `phase`: `package`
- `severity`: `error`
- `profile`: `notebook-export`
- `meaning`: notebook export attempted execution or hidden replay

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `cell-owner`: minimum `0`, maximum `1`
  - `receipt`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-EXPORT-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

### W-EXPR

#### W-EXPR-0001

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: an assignment target is not a mutable place

- `requiredFacts`:
  - `operation`: `string`
  - `requiredCategory`: `string`
  - `targetCategory`: `string`

- `labelRoles`:
  - `assignment-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-EXPR-0001` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-EXPR-0002

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: an assignment with Unit result appears in a context that requires a value

- `requiredFacts`:
  - `actualType`: `string`
  - `context`: `string`
  - `expectedType`: `string`

- `labelRoles`:
  - `assignment-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-EXPR-0002` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-EXPR-0006

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a postfix operation or generic envelope does not apply to its resolved head

- `requiredFacts`:
  - `headType`: `string`
  - `reason`: `string`
  - `suffixKind`: `string`

- `labelRoles`:
  - `postfix-head`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-EXPR-0006` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-EXPR-0007

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: each does not occupy the final argument for a compatible rest parameter

- `requiredFacts`:
  - `argumentIndex`: `integer`
  - `callable`: `string`
  - `reason`: `string`
  - `restParameter`: `string`

- `labelRoles`:
  - `call-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-EXPR-0007` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

### W-FACET

#### W-FACET-0001

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a facet target is not a property place or a declared immediate core control target

- `requiredFacts`:
  - `facet`: `string`
  - `place`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-FACET-0001` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-FACET-0002

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a facet projection is reified or dynamically looked up

- `requiredFacts`:
  - `expected`: `string-set`
  - `projection`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-FACET-0002` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-FACET-0003

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a facet violates the synchronous nonthrowing property-safe ceiling

- `requiredFacts`:
  - `effect`: `string`
  - `facet`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-FACET-0003` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-FACET-0004

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a mutable facet does not have an exclusive property place

- `requiredFacts`:
  - `facet`: `string`
  - `ownership`: `string`
  - `place`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-FACET-0004` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-FACET-0005

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a facet is not declared by the applied behavior or the immediate core control target

- `requiredFacts`:
  - `declarations`: `string[]`
  - `facet`: `string`
  - `place`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-FACET-0005` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-FACET-0006

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a facet collides with a behavior or core facet or requests unsupported take

- `requiredFacts`:
  - `collision`: `string`
  - `facet`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-FACET-0006` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

### W-FLOW

#### W-FLOW-0001

- `state`: `active`
- `phase`: `semantic.flow`
- `severity`: `error`
- `meaning`: a structured exit has no compatible active owner

- `requiredFacts`:
  - `expectedOwner`: `string`
  - `flow`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-FLOW-0001` — [Diagnostics](DESIGN.md#diagnostics-1)

#### W-FLOW-0002

- `state`: `active`
- `phase`: `semantic.flow`
- `severity`: `error`
- `meaning`: a path reaches the end without its required result

- `requiredFacts`:
  - `expectedOwner`: `string`
  - `flow`: `string`
  - `resultType`: `string`

- `labelRoles`:
  - `return-requirement`: minimum `1`, maximum `1`

- `fixes`:
  - `add-return-value`: `placeholder`

- Design authority: `exact` `W-FLOW-0002` — [Diagnostics](DESIGN.md#diagnostics-1)

### W-FMT

#### W-FMT-0001

- `state`: `active`
- `phase`: `source.format`
- `severity`: `error`
- `meaning`: source bytes differ from the canonical formatter output

- `requiredFacts`:
  - `canonicalDigest`: `string`
  - `sourceDigest`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - `format-source`: `machine`

- Design authority: `exact` `W-FMT-0001` — [3.5.1 Forma canônica do formatter](DESIGN.md#351-forma-canônica-do-formatter)

#### W-FMT-0002

- `state`: `active`
- `phase`: `source.format`
- `severity`: `error`
- `meaning`: formatting and reparsing do not preserve the normalized concrete syntax tree

- `requiredFacts`:
  - `formattedTreeDigest`: `string`
  - `sourceTreeDigest`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-FMT-0002` — [Invariante do formatter](DESIGN.md#invariante-do-formatter)

### W-FOREIGN

#### W-FOREIGN-0001

- `state`: `active`
- `phase`: `build`
- `severity`: `error`
- `profile`: `foreign-build`
- `meaning`: the inline language adapter is invalid, unavailable, or absent from the lock

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `foreign-body`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-FOREIGN-0001` — [19.2 `fn<Language>`](DESIGN.md#192-fn)

#### W-FOREIGN-0002

- `state`: `active`
- `phase`: `build`
- `severity`: `error`
- `profile`: `foreign-build`
- `meaning`: the adapter scanner identity, version, or lexical profile is unsupported

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `foreign-body`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-FOREIGN-*` — [19.2 `fn<Language>`](DESIGN.md#192-fn)

#### W-FOREIGN-0003

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `foreign-source`
- `meaning`: the foreign body delimiter or scanner phase is invalid

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `foreign-body`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-FOREIGN-*` — [19.2 `fn<Language>`](DESIGN.md#192-fn)

#### W-FOREIGN-0004

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `foreign-source`
- `meaning`: a foreign lexical construct is unterminated

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `foreign-body`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-FOREIGN-*` — [19.2 `fn<Language>`](DESIGN.md#192-fn)

#### W-FOREIGN-0005

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `foreign-source`
- `meaning`: the baseline inline C profile contains a preprocessor directive or token-forming line splice

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `foreign-body`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-FOREIGN-*` — [19.2 `fn<Language>`](DESIGN.md#192-fn)

#### W-FOREIGN-0006

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `foreign-source`
- `meaning`: foreign source encoding, byte count, or nesting exceeds its declared bound

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `foreign-body`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-FOREIGN-*` — [19.2 `fn<Language>`](DESIGN.md#192-fn)

#### W-FOREIGN-0007

- `state`: `active`
- `phase`: `build`
- `severity`: `error`
- `profile`: `foreign-build`
- `meaning`: the scanner or body digest differs from the build recipe

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `foreign-body`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-FOREIGN-*` — [19.2 `fn<Language>`](DESIGN.md#192-fn)

#### W-FOREIGN-0008

- `state`: `active`
- `phase`: `build`
- `severity`: `error`
- `profile`: `foreign-build`
- `meaning`: a structural editor fallback was used as build evidence

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `foreign-body`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-FOREIGN-*` — [19.2 `fn<Language>`](DESIGN.md#192-fn)

#### W-FOREIGN-0009

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `foreign-source`
- `meaning`: an adapter diagnostic span is outside the opaque body byte range

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `foreign-body`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-FOREIGN-*` — [19.2 `fn<Language>`](DESIGN.md#192-fn)

### W-GENERIC

#### W-GENERIC-0001

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a generic parameter domain does not resolve to a protocol constraint, StaticArgumentRepresentable type, or an earlier type parameter

- `requiredFacts`:
  - `domain`: `string`
  - `parameter`: `string`
  - `resolutionReason`: `string`

- `labelRoles`:
  - `generic-parameter`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-GENERIC-0001` — [8.7.1 Kinds, parâmetros e aplicação](DESIGN.md#871-kinds-parâmetros-e-aplicação)

#### W-GENERIC-0002

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a required generic slot is missing or open inference has no unique solution

- `requiredFacts`:
  - `candidates`: `string-set`
  - `equationSources`: `string-set`
  - `parameter`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `call-owner`: minimum `1`, maximum `1`
  - `generic-parameter`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-GENERIC-0002` — [Envelope de contrato](DESIGN.md#envelope-de-contrato)

#### W-GENERIC-0003

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a generic parameter label, anchor boundary, or positional-only binding is invalid

- `requiredFacts`:
  - `externalLabel`: `string`
  - `kind`: `string`
  - `parameter`: `string`
  - `position`: `integer`
  - `reason`: `string`

- `labelRoles`:
  - `generic-parameter`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-GENERIC-0003` — [Envelope de contrato](DESIGN.md#envelope-de-contrato)

#### W-GENERIC-0004

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a generic associated contract value name conflicts with a type-head member

- `requiredFacts`:
  - `head`: `string`
  - `member`: `string`
  - `parameter`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `associated-member`: minimum `1`, maximum `1`
  - `generic-parameter`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-GENERIC-0004` — [8.7.10 Diagnostics](DESIGN.md#8710-diagnostics)

#### W-GENERIC-0005

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a sequence of generic instantiations does not converge

- `requiredFacts`:
  - `declaration`: `string`
  - `instantiationPrefix`: `string[]`
  - `reason`: `string`
  - `transform`: `string`

- `labelRoles`:
  - `generic-declaration`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-GENERIC-0005` — [8.7.8 Instantiation, termination e cache](DESIGN.md#878-instantiation-termination-e-cache)

### W-INIT

#### W-INIT-0001

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a place is read before definite initialization

- `requiredFacts`:
  - `binding`: `string`
  - `ownerState`: `string`
  - `type`: `string`

- `labelRoles`:
  - `declaration`: minimum `1`, maximum `1`

- `fixes`:
  - `initialize-binding`: `placeholder`

- Design authority: `exact` `W-INIT-0001` — [Diagnostics](DESIGN.md#diagnostics-1)

### W-JUPYTER

#### W-JUPYTER-0001

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `jupyter`
- `meaning`: connection authentication, integrity, or replay validation fails

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `channel`: minimum `0`, maximum `1`
  - `request-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-JUPYTER-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-JUPYTER-0002

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `jupyter`
- `meaning`: a Jupyter frame, metadata, history, or pending request exceeds a quota

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `channel`: minimum `0`, maximum `1`
  - `request-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-JUPYTER-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-JUPYTER-0003

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `jupyter`
- `meaning`: a request lifecycle does not end after its reply and related outputs

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `channel`: minimum `0`, maximum `1`
  - `request-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-JUPYTER-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-JUPYTER-0005

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `jupyter`
- `meaning`: silent or user-expression execution is not read-only and effect-free

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `channel`: minimum `0`, maximum `1`
  - `request-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-JUPYTER-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-JUPYTER-0006

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `jupyter`
- `meaning`: stdin or password handling violates the bounded routing policy

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `channel`: minimum `0`, maximum `1`
  - `request-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-JUPYTER-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-JUPYTER-0007

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `jupyter`
- `meaning`: a kernelspec feature or W metadata field is not implemented or namespaced

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `channel`: minimum `0`, maximum `1`
  - `request-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-JUPYTER-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-JUPYTER-0008

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `jupyter`
- `meaning`: interrupt or shutdown claims a boundary without admission, drain, or safe close

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `channel`: minimum `0`, maximum `1`
  - `request-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-JUPYTER-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-JUPYTER-0009

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `jupyter`
- `meaning`: a read-only Jupyter request reads staging, uses a byte offset, executes, or requests an unsupported history mode

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `channel`: minimum `0`, maximum `1`
  - `request-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-JUPYTER-0009` — [Adapter Jupyter](DESIGN.md#adapter-jupyter)

### W-LABEL

#### W-LABEL-0004

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: accepted call forms collide after label policy normalization

- `requiredFacts`:
  - `declarations`: `string[]`
  - `forms`: `string[]`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-LABEL-0004` — [9.5 Declaração lexical de allocator](DESIGN.md#95-declaração-lexical-de-allocator)

#### W-LABEL-0005

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a call uses an unknown label or an invalid positional/labeled form

- `requiredFacts`:
  - `acceptedForms`: `string[]`
  - `declaration`: `string`
  - `label`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-LABEL-0005` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

#### W-LABEL-0006

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a call repeats a label, a declaration repeats an external label, or the same normalized slot is supplied twice

- `requiredFacts`:
  - `declaration`: `string`
  - `label`: `string`
  - `slot`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-LABEL-0006` — [7.2.2 Labels, argument order e pipe holes](DESIGN.md#722-labels-argument-order-e-pipe-holes)

### W-LAZY

#### W-LAZY-0001

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a Lazy initializer uses an effect outside the synchronous nonthrowing contract

- `requiredFacts`:
  - `actualEffect`: `string`
  - `allowedEffects`: `string-set`
  - `property`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LAZY-*` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-LAZY-0002

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a potentially contended Lazy access is reachable from a non-blocking domain

- `requiredFacts`:
  - `domain`: `string`
  - `proof`: `string`
  - `property`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LAZY-*` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-LAZY-0003

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a Lazy initializer reenters its own initialization dependency cycle

- `requiredFacts`:
  - `cycle`: `string[]`
  - `property`: `string`
  - `winner`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LAZY-*` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-LAZY-0004

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a Lazy assignment or get mut ref access lacks exclusive authority

- `requiredFacts`:
  - `actualAuthority`: `string`
  - `operation`: `string`
  - `property`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LAZY-*` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-LAZY-0005

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a Lazy lowering lacks required ownership, mobility, lifetime, or shareability facts

- `requiredFacts`:
  - `lowering`: `string`
  - `missingFacts`: `string-set`
  - `property`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LAZY-*` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

#### W-LAZY-0006

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a Lazy owner is mutated or destroyed before initializer and waiters drain

- `requiredFacts`:
  - `activeInitializer`: `boolean`
  - `property`: `string`
  - `waiters`: `integer`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LAZY-*` — [10. Property behaviors](DESIGN.md#10-property-behaviors)

### W-LEX

#### W-LEX-0001

- `state`: `active`
- `phase`: `source.lex`
- `severity`: `error`
- `meaning`: a literal or comment reaches its boundary without a terminator

- `requiredFacts`:
  - `construct`: `string`
  - `delimiter`: `string`
  - `reachedEof`: `boolean`

- `labelRoles`:
  - `opening-delimiter`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-LEX-0001` — [3.5.2 Grammar normativa G0: statements e controle](DESIGN.md#352-grammar-normativa-g0-statements-e-controle)

### W-LOCK

#### W-LOCK-0001

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a dependency on lock-protected storage escapes the scoped operation

- `requiredFacts`:
  - `dependencyKind`: `string`
  - `lock`: `string`
  - `target`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LOCK-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

#### W-LOCK-0002

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a protected lock operation can suspend

- `requiredFacts`:
  - `effect`: `string`
  - `lock`: `string`
  - `operation`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LOCK-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

#### W-LOCK-0003

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a blocking lock acquisition is reachable from a non-blocking execution domain

- `requiredFacts`:
  - `domain`: `string`
  - `lock`: `string`
  - `requiredCapability`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LOCK-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

#### W-LOCK-0004

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a protected operation attempts to acquire the same lock again

- `requiredFacts`:
  - `heldLock`: `string`
  - `operation`: `string`
  - `origin`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LOCK-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

#### W-LOCK-0005

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a lock or protected access crosses its owning fault boundary

- `requiredFacts`:
  - `actualBoundary`: `string`
  - `lock`: `string`
  - `ownerBoundary`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LOCK-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

#### W-LOCK-0006

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a read-only protected access attempts to mutate the payload

- `requiredFacts`:
  - `accessMode`: `string`
  - `lock`: `string`
  - `operation`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LOCK-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

#### W-LOCK-0007

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: the lock target is not a stable shared place

- `requiredFacts`:
  - `actualType`: `string`
  - `operation`: `string`
  - `target`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LOCK-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

#### W-LOCK-0008

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a lock owner is destroyed before holders and waiters drain

- `requiredFacts`:
  - `activeHolder`: `boolean`
  - `lock`: `string`
  - `waiters`: `integer`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LOCK-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

#### W-LOCK-0009

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `meaning`: a lock provider grants an acquisition that is absent or no longer pending

- `requiredFacts`:
  - `lock`: `string`
  - `request`: `string`
  - `task`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LOCK-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

#### W-LOCK-0010

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `meaning`: cancellation interrupts a protected operation after lock grant and before unlock

- `requiredFacts`:
  - `lock`: `string`
  - `phase`: `string`
  - `ticket`: `integer`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LOCK-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

#### W-LOCK-0011

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a protected lock operation can throw an application error

- `requiredFacts`:
  - `effect`: `string`
  - `lock`: `string`
  - `operation`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LOCK-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

#### W-LOCK-0012

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a protected lock operation can perform another blocking effect

- `requiredFacts`:
  - `effect`: `string`
  - `lock`: `string`
  - `operation`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LOCK-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

#### W-LOCK-0013

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a concurrent access bypasses the synchronization selected for the shared place

- `requiredFacts`:
  - `access`: `string`
  - `place`: `string`
  - `synchronization`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-LOCK-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

### W-MATCH

#### W-MATCH-0001

- `state`: `active`
- `phase`: `semantic.flow`
- `severity`: `error`
- `meaning`: a required switch or catch does not cover its complete proven domain

- `requiredFacts`:
  - `missingCases`: `string-set`
  - `subjectType`: `string`

- `labelRoles`:
  - `match-subject`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-MATCH-0001` — [Diagnostics](DESIGN.md#diagnostics)

#### W-MATCH-0002

- `state`: `active`
- `phase`: `semantic.flow`
- `severity`: `error`
- `meaning`: a case is completely covered by an earlier unguarded case

- `requiredFacts`:
  - `coveredBy`: `string`
  - `pattern`: `string`
  - `subjectType`: `string`

- `labelRoles`:
  - `covered-case`: minimum `1`, maximum `1`
  - `match-subject`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-MATCH-0002` — [Diagnostics](DESIGN.md#diagnostics)

#### W-MATCH-0003

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a short enum member has no unique expected enum type

- `requiredFacts`:
  - `context`: `string`
  - `expectedType`: `string`
  - `member`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-MATCH-0003` — [Diagnostics](DESIGN.md#diagnostics)

### W-MEMORY

#### W-MEMORY-0001

- `state`: `active`
- `phase`: `test`
- `severity`: `error`
- `meaning`: a drained lifecycle boundary retains a strong ownership cycle that no external root reaches

- `requiredFacts`:
  - `boundary`: `string`
  - `cycle`: `string[]`
  - `drained`: `boolean`
  - `externalRoots`: `string[]`
  - `reachableExternalRoots`: `string[]`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-MEMORY-0001` — [9.4.1 Captures e ciclos fortes](DESIGN.md#941-captures-e-ciclos-fortes)

### W-MOVE

#### W-MOVE-0002

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a place is not available on every continuing predecessor

- `requiredFacts`:
  - `binding`: `string`
  - `ownerState`: `string`
  - `type`: `string`

- `labelRoles`:
  - `move-origin`: minimum `1`, maximum `1`
  - `predecessor`: minimum `0`, maximum `unbounded`

- `fixes`:
  - none

- Design authority: `exact` `W-MOVE-0002` — [Diagnostics](DESIGN.md#diagnostics-1)

### W-OWNERSHIP

#### W-OWNERSHIP-0010

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: an ownership prefix receives an incompatible place, owner, borrow, or mobility

- `requiredFacts`:
  - `operandCategory`: `string`
  - `prefix`: `string`
  - `requirement`: `string`

- `labelRoles`:
  - `ownership-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-OWNERSHIP-0010` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-OWNERSHIP-0011

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a consuming member call does not transfer its owned receiver explicitly

- `requiredFacts`:
  - `member`: `string`
  - `receiverCategory`: `string`
  - `receiverMode`: `string`
  - `receiverPlace`: `string`
  - `receiverType`: `string`
  - `requiredForm`: `string`

- `labelRoles`:
  - `receiver-declaration`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-OWNERSHIP-0011` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-OWNERSHIP-0012

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: dispatch loans do not form a closed access sequence for one place and one domain

- `requiredFacts`:
  - `domain`: `string`
  - `place`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-OWNERSHIP-0012` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

#### W-OWNERSHIP-0013

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a unique owner is used where shared ownership is required without a written shared binding and explicit move

- `requiredFacts`:
  - `actualType`: `string`
  - `context`: `string`
  - `expectedType`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-OWNERSHIP-0013` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-OWNERSHIP-0014

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a closed strong ownership component can only release its edges from destructors inside the same component

- `requiredFacts`:
  - `cycle`: `string[]`
  - `edgeOrigins`: `string[]`
  - `releasePlan`: `string`

- `labelRoles`:
  - `cycle-edge`: minimum `2`, maximum `unbounded`

- `fixes`:
  - none

- Design authority: `exact` `W-OWNERSHIP-0014` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-OWNERSHIP-0015

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: an escaping closure would retain or transfer a move-first owner without an explicit capture mode

- `requiredFacts`:
  - `capture`: `string`
  - `destination`: `string`
  - `ownerType`: `string`
  - `validModes`: `string-set`

- `labelRoles`:
  - `capture-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-OWNERSHIP-0015` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-OWNERSHIP-0016

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a parameter modifier appears before its binding or is not a valid parameter mode

- `requiredFacts`:
  - `canonicalForm`: `string`
  - `context`: `string`
  - `modifier`: `string`
  - `parameter`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `parameter-declaration`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-OWNERSHIP-0016` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-OWNERSHIP-0017

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a call-site ownership operation does not satisfy the parameter contract or is missing for an owned place

- `requiredFacts`:
  - `actualCategory`: `string`
  - `expectedContract`: `string`
  - `parameter`: `string`
  - `reason`: `string`
  - `suppliedOperation`: `string`

- `labelRoles`:
  - `argument`: minimum `1`, maximum `1`
  - `parameter-contract`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-OWNERSHIP-0017` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

### W-PARSE

#### W-PARSE-0001

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a token is not accepted by the current grammar owner

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0001` — [3.5.2 Grammar normativa G0: statements e controle](DESIGN.md#352-grammar-normativa-g0-statements-e-controle)

#### W-PARSE-0002

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a grammar owner ends without a required delimiter or keyword

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0002` — [3.5.2 Grammar normativa G0: statements e controle](DESIGN.md#352-grammar-normativa-g0-statements-e-controle)

#### W-PARSE-0003

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a label is attached to a construct that labels cannot target

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0003` — [3.5.2 Grammar normativa G0: statements e controle](DESIGN.md#352-grammar-normativa-g0-statements-e-controle)

#### W-PARSE-0004

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a contextual continuation keyword has no compatible lexical owner

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0004` — [3.5.2 Grammar normativa G0: statements e controle](DESIGN.md#352-grammar-normativa-g0-statements-e-controle)

#### W-PARSE-0006

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: one document mixes a manifest root with module source

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0006` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-PARSE-0007

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a module header or import appears after an ordinary declaration

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0007` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-PARSE-0008

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a declaration modifier is attached to an unsupported target

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0008` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-PARSE-0009

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a declaration lacks its required name, signature, or body

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0009` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-PARSE-0010

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: an import is incomplete or lacks a required from clause

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0010` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-PARSE-0011

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: one source document contains more than one module header

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0011` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-PARSE-0012

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a manifest contains an executable token or a source declaration

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0012` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-PARSE-0013

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: trivia separates a contract head from its opening angle bracket

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0013` — [Tokenização e recovery](DESIGN.md#tokenização-e-recovery)

#### W-PARSE-0014

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a contract, tuple, or array ends without its closing delimiter

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0014` — [Tokenização e recovery](DESIGN.md#tokenização-e-recovery)

#### W-PARSE-0015

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a computed contract expression lacks its required parentheses

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0015` — [Tokenização e recovery](DESIGN.md#tokenização-e-recovery)

#### W-PARSE-0016

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: one tuple mixes labeled and positional elements

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0016` — [Tokenização e recovery](DESIGN.md#tokenização-e-recovery)

#### W-PARSE-0017

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: parentheses group a type without the comma required for a tuple

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0017` — [Tokenização e recovery](DESIGN.md#tokenização-e-recovery)

#### W-PARSE-0018

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a qualifier or postfix appears outside its allowed order

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0018` — [Tokenização e recovery](DESIGN.md#tokenização-e-recovery)

#### W-PARSE-0019

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a one-element tuple pattern lacks its required comma

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0019` — [Diagnostics](DESIGN.md#diagnostics)

#### W-PARSE-0020

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: an operator lacks a required operand

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0020` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-PARSE-0021

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a value-producing conditional expression lacks an else branch

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0021` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-PARSE-0022

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: one tuple or collection mixes incompatible element forms

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0022` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-PARSE-0023

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a one-sided range appears outside an argument, index, or pattern

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0023` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-PARSE-0026

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a contextual token appears without a compatible owner

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0026` — [Invariante do formatter](DESIGN.md#invariante-do-formatter)

#### W-PARSE-0027

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a closing token crosses its owner or uses an invalid lexical reading

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0027` — [Invariante do formatter](DESIGN.md#invariante-do-formatter)

#### W-PARSE-0028

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: recovery would reinterpret a form after its grammar commit point

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0028` — [Invariante do formatter](DESIGN.md#invariante-do-formatter)

#### W-PARSE-0029

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a pattern ellipsis is duplicated or appears before another field

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0029` — [Diagnostics](DESIGN.md#diagnostics)

#### W-PARSE-0030

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: an equality, relation, or range expression attempts to chain

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0030` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-PARSE-0031

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: a reexport is missing its from clause, origin, or valid item

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0031` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-PARSE-0032

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: export import or an invalid reexport form is present

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PARSE-0032` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

### W-PATTERN

#### W-PATTERN-0001

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a capture form is not allowed by the current pattern modality

- `requiredFacts`:
  - `capture`: `string`
  - `context`: `string`
  - `requiredForm`: `string`

- `labelRoles`:
  - `pattern-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PATTERN-0001` — [Diagnostics](DESIGN.md#diagnostics)

#### W-PATTERN-0002

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a payload pattern has incompatible arity, labels, or modality

- `requiredFacts`:
  - `actualArity`: `integer`
  - `case`: `string`
  - `expectedArity`: `integer`
  - `payloadMode`: `string`

- `labelRoles`:
  - `pattern-owner`: minimum `1`, maximum `1`
  - `type-declaration`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PATTERN-0002` — [Diagnostics](DESIGN.md#diagnostics)

#### W-PATTERN-0003

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a nominal pattern selects a field that is absent or not visible

- `requiredFacts`:
  - `field`: `string`
  - `reason`: `string`
  - `type`: `string`
  - `visibleFields`: `string-set`

- `labelRoles`:
  - `pattern-owner`: minimum `1`, maximum `1`
  - `type-declaration`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PATTERN-0003` — [Diagnostics](DESIGN.md#diagnostics)

#### W-PATTERN-0005

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a nominal pattern requests destructuring from an opaque category

- `requiredFacts`:
  - `category`: `string`
  - `destructuringVisibility`: `string`
  - `type`: `string`

- `labelRoles`:
  - `pattern-owner`: minimum `1`, maximum `1`
  - `type-declaration`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PATTERN-0005` — [Diagnostics](DESIGN.md#diagnostics)

#### W-PATTERN-0006

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a guard consumes, mutates, escapes, or suspends with a provisional capture

- `requiredFacts`:
  - `capture`: `string`
  - `operation`: `string`
  - `provisionalMode`: `string`

- `labelRoles`:
  - `capture-origin`: minimum `1`, maximum `1`
  - `pattern-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PATTERN-0006` — [Diagnostics](DESIGN.md#diagnostics)

#### W-PATTERN-0007

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a range pattern has incompatible or non-constant bounds

- `requiredFacts`:
  - `lowerType`: `string`
  - `rangeOperator`: `string`
  - `upperType`: `string`

- `labelRoles`:
  - `pattern-owner`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-PATTERN-0007` — [Diagnostics](DESIGN.md#diagnostics)

### W-PIPE

#### W-PIPE-0001

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `meaning`: pipe-forward RHS must be an explicit free-function call template

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-PIPE-0001` — [5.6 Operadores, assignment e identidade](DESIGN.md#56-operadores-assignment-e-identidade)

#### W-PIPE-0002

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a pipe call must have exactly one required unbound slot; a named hole may appear at any declaration position and positional anchors preserve their boundaries

- `requiredFacts`:
  - `function`: `string`
  - `parameter`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-PIPE-0002` — [5.6 Operadores, assignment e identidade](DESIGN.md#56-operadores-assignment-e-identidade)

#### W-PIPE-0003

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `meaning`: pipe-forward does not accept placeholders, members, UFCS, colon, or facet lookup

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-PIPE-0003` — [5.6 Operadores, assignment e identidade](DESIGN.md#56-operadores-assignment-e-identidade)

#### W-PIPE-0004

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: pipe-forward ownership or single-evaluation rules are invalid

- `requiredFacts`:
  - `lhs`: `string`
  - `mode`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-PIPE-0004` — [5.6 Operadores, assignment e identidade](DESIGN.md#56-operadores-assignment-e-identidade)

### W-PIPELINE

#### W-PIPELINE-0001

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `meaning`: a pipeline mode or contract schema is missing or invalid

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string-set`
  - `mode`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-PIPELINE-0001` — [12.8 Pipeline de tasks e backpressure](DESIGN.md#128-pipeline-de-tasks-e-backpressure)

#### W-PIPELINE-0002

- `state`: `active`
- `phase`: `semantic.flow`
- `severity`: `error`
- `meaning`: a pipeline region uses return instead of its commit terminal

- `requiredFacts`:
  - `mode`: `string`
  - `reason`: `string`
  - `terminator`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-PIPELINE-0002` — [12.8 Pipeline de tasks e backpressure](DESIGN.md#128-pipeline-de-tasks-e-backpressure)

#### W-PIPELINE-0003

- `state`: `active`
- `phase`: `semantic.flow`
- `severity`: `error`
- `meaning`: pipeline modes are combined or nested in a forbidden way

- `requiredFacts`:
  - `modes`: `string[]`
  - `reason`: `string`
  - `region`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-PIPELINE-0003` — [12.8 Pipeline de tasks e backpressure](DESIGN.md#128-pipeline-de-tasks-e-backpressure)

#### W-PIPELINE-0004

- `state`: `active`
- `phase`: `semantic.flow`
- `severity`: `error`
- `meaning`: a tasks pipeline violates positive limit, per-input cardinality, or commit rules

- `requiredFacts`:
  - `input`: `string`
  - `limit`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-PIPELINE-0004` — [12.8 Pipeline de tasks e backpressure](DESIGN.md#128-pipeline-de-tasks-e-backpressure)

#### W-PIPELINE-0005

- `state`: `active`
- `phase`: `semantic.flow`
- `severity`: `error`
- `meaning`: a tasks pipeline has invalid ordering or error policy

- `requiredFacts`:
  - `errors`: `string`
  - `ordering`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-PIPELINE-0005` — [12.8 Pipeline de tasks e backpressure](DESIGN.md#128-pipeline-de-tasks-e-backpressure)

### W-PLACEMENT

#### W-PLACEMENT-0001

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: spawn placement is written on a declaration instead of at a call site

- `requiredFacts`:
  - `declaration`: `string`
  - `domain`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-PLACEMENT-0001` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

#### W-PLACEMENT-0002

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: spawn requires an explicit execution domain that exists in the current product context

- `requiredFacts`:
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-PLACEMENT-0002` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

#### W-PLACEMENT-0003

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a concurrent execution domain cannot satisfy the requested barrier dispatch mode

- `requiredFacts`:
  - `domain`: `string`
  - `mode`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-PLACEMENT-0003` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

#### W-PLACEMENT-0004

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a child launcher must be the unique callable root of a lexical let initializer

- `requiredFacts`:
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-PLACEMENT-0004` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

### W-PRESENTATION

#### W-PRESENTATION-0001

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `presentation`
- `meaning`: a typed presentation media or payload is invalid

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `media`: minimum `0`, maximum `1`
  - `presentation-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-PRESENTATION-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-PRESENTATION-0002

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `presentation`
- `meaning`: active presentation content is rejected

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `media`: minimum `0`, maximum `1`
  - `presentation-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-PRESENTATION-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-PRESENTATION-0003

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `presentation`
- `meaning`: a presentation media type is duplicated

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `media`: minimum `0`, maximum `1`
  - `presentation-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-PRESENTATION-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-PRESENTATION-0004

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `presentation`
- `meaning`: a presentation limit is exceeded

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `media`: minimum `0`, maximum `1`
  - `presentation-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-PRESENTATION-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-PRESENTATION-0005

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `presentation`
- `meaning`: a presentation call uses an effect outside the closed mask

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `media`: minimum `0`, maximum `1`
  - `presentation-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-PRESENTATION-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-PRESENTATION-0006

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `presentation`
- `meaning`: a media type requires a missing provider or sanitizer

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `media`: minimum `0`, maximum `1`
  - `presentation-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-PRESENTATION-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-PRESENTATION-0007

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `presentation`
- `meaning`: a presentation does not provide required text/plain

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `media`: minimum `0`, maximum `1`
  - `presentation-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-PRESENTATION-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-PRESENTATION-0008

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `presentation`
- `meaning`: a presentation writer is closed, cancelled, or used outside its scope

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `media`: minimum `0`, maximum `1`
  - `presentation-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-PRESENTATION-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-PRESENTATION-0009

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `presentation`
- `meaning`: presentation would collect a stream or copy device data implicitly

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `media`: minimum `0`, maximum `1`
  - `presentation-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-PRESENTATION-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

#### W-PRESENTATION-0010

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `presentation`
- `meaning`: a presentation fallback would call user code or expose private data

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `media`: minimum `0`, maximum `1`
  - `presentation-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `family` `W-PRESENTATION-*` — [Notebook e export reproduzível](DESIGN.md#notebook-e-export-reproduzível)

### W-RUN

#### W-RUN-0001

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: a package or workspace edition is missing or invalid

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0001` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0002

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: a package or workspace field is unknown or duplicated

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0002` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0003

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: a dependency alias or record is invalid

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0003` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0004

- `state`: `active`
- `phase`: `source.context`
- `severity`: `error`
- `profile`: `run-context`
- `meaning`: a package or workspace owner is missing, duplicated, or ambiguous

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0004` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0005

- `state`: `active`
- `phase`: `source.entry`
- `severity`: `error`
- `profile`: `run-entry`
- `meaning`: a module has a missing, duplicate, incompatible, or invalid selected entry

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0005` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0006

- `state`: `active`
- `phase`: `source.resolution`
- `severity`: `error`
- `profile`: `run-resolution`
- `meaning`: a resolution is missing, invalid, or not content-addressed

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0006` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0007

- `state`: `active`
- `phase`: `source.resolution`
- `severity`: `error`
- `profile`: `run-resolution`
- `meaning`: a resolution selection, target, edition, node, edge, or alias diverges

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0007` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0008

- `state`: `active`
- `phase`: `source.roots`
- `severity`: `error`
- `profile`: `run-roots`
- `meaning`: a canonical root or import containment check fails

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0008` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0009

- `state`: `active`
- `phase`: `source.capability`
- `severity`: `error`
- `profile`: `run-capability`
- `meaning`: a requirement, source grant, secret, or deployment admission fails

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0009` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0010

- `state`: `active`
- `phase`: `source.fetch`
- `severity`: `error`
- `profile`: `run-fetch`
- `meaning`: network policy, CAS closure, artifact integrity, or action output admission fails

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0010` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0011

- `state`: `active`
- `phase`: `source.provenance`
- `severity`: `error`
- `profile`: `run-provenance`
- `meaning`: identity, recipe, provenance, or promotion equivalence inputs diverge

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0011` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0012

- `state`: `active`
- `phase`: `source.resolution`
- `severity`: `error`
- `profile`: `run-resolution`
- `meaning`: a reachable package graph has a dangling, unreachable, missing, cyclic, or colliding edge

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0012` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0013

- `state`: `active`
- `phase`: `source.roots`
- `severity`: `error`
- `profile`: `run-roots`
- `meaning`: a provider canonical root does not contain the physical candidate

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0013` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0014

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-evidence`
- `meaning`: parser evidence does not match source bytes or normalized module facts

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0014` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0015

- `state`: `active`
- `phase`: `source.resolution`
- `severity`: `error`
- `profile`: `run-resolution`
- `meaning`: a requested target has zero or multiple selected resolution contexts

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0015` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-RUN-0016

- `state`: `active`
- `phase`: `source.provenance`
- `severity`: `error`
- `profile`: `run-provenance`
- `meaning`: an artifact, handle, or action-output record is not bound to resolution and recipe facts

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-RUN-0016` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

### W-SEM

#### W-SEM-0001

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a node does not satisfy its expected semantic use

- `requiredFacts`:
  - `actual`: `string`
  - `expected`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-SEM-0001` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

### W-SESSION

#### W-SESSION-0001

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: a contextual command is unknown

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0001` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0002

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: the normal parser returns a parse diagnostic

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0002` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0003

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: the normal checker returns a semantic diagnostic

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0003` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0004

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: lock, capability, or context facts changed after session open

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0004` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0005

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: the requested base generation is stale, unknown, or not opaque

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0005` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0006

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: a current binding, import, or exact dependency version is unavailable

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0006` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0007

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: a cross-generation take lacks Copy, snapshot, adapter, or deferred proof

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0007` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0008

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: a cross-generation inout lacks an explicit transaction or no-fail proof

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0008` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0009

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: a ref operation escapes its submission

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0009` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0010

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: a borrow operation escapes its submission

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0010` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0011

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: a view crosses generations without an owner-backed proof

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0011` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0012

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: drain preflight rejects replacement or reset before effects

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0012` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0013

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: post-publish drain leaves a committed but degraded generation

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0013` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0014

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: session close requires owner drain or a recorded force boundary

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0014` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0015

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: history reservation exceeds the bounded memory quota

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0015` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0016

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: output budget requires rejection, truncation, or cancellation

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0016` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0017

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: queued or active cancellation or a detached/non-joined child reaches a boundary

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0017` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0018

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: rollback was claimed without explicit provider transaction evidence

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0018` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0019

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: total binding versions exceed the bounded quota

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0019` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0020

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: hard-edge count exceeds the bounded quota

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0020` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0021

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: HIR or artifact count exceeds the bounded quota

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0021` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0022

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: persistent task/resource owner count exceeds the bounded quota

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0022` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0023

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: hard-dependent invalidation closure exceeds the bounded quota

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0023` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0024

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: derived drain deadline exceeds the bounded quota

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0024` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0025

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: source bytes exceed the bounded quota

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0025` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0026

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: provider drain facts are missing or a conclusion boolean was supplied

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0026` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0027

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: effects exceed the per-submission bounded quota

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0027` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

#### W-SESSION-0028

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `profile`: `session`
- `meaning`: queued admission tickets exceed the bounded ring

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `generation`: minimum `0`, maximum `2`
  - `session-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-SESSION-0028` — [3.5.3 Grammar normativa G1: declarations e raízes de source](DESIGN.md#353-grammar-normativa-g1-declarations-e-raízes-de-source)

### W-SNAPSHOT

#### W-SNAPSHOT-0001

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a dependency on a published snapshot escapes its scoped read

- `requiredFacts`:
  - `dependencyKind`: `string`
  - `operation`: `string`
  - `target`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-SNAPSHOT-*` — [12.10.9 Diagnostics e gate](DESIGN.md#12109-diagnostics-e-gate)

#### W-SNAPSHOT-0002

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a snapshot read operation can suspend

- `requiredFacts`:
  - `effect`: `string`
  - `operation`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-SNAPSHOT-*` — [12.10.9 Diagnostics e gate](DESIGN.md#12109-diagnostics-e-gate)

#### W-SNAPSHOT-0003

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a snapshot payload lacks required mobility or lifetime facts

- `requiredFacts`:
  - `actualType`: `string`
  - `missingFacts`: `string-set`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-SNAPSHOT-*` — [12.10.9 Diagnostics e gate](DESIGN.md#12109-diagnostics-e-gate)

#### W-SNAPSHOT-0004

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: published snapshot storage is mutated in place

- `requiredFacts`:
  - `binding`: `string`
  - `operation`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-SNAPSHOT-*` — [12.10.9 Diagnostics e gate](DESIGN.md#12109-diagnostics-e-gate)

#### W-SNAPSHOT-0005

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a move-only snapshot cell is copied

- `requiredFacts`:
  - `actualType`: `string`
  - `operation`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-SNAPSHOT-*` — [12.10.9 Diagnostics e gate](DESIGN.md#12109-diagnostics-e-gate)

#### W-SNAPSHOT-0006

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: snapshot construction or publication does not consume an owned value

- `requiredFacts`:
  - `actualMode`: `string`
  - `requiredMode`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-SNAPSHOT-*` — [12.10.9 Diagnostics e gate](DESIGN.md#12109-diagnostics-e-gate)

#### W-SNAPSHOT-0007

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: an owned snapshot is requested for a value that is not duplicable

- `requiredFacts`:
  - `actualType`: `string`
  - `requirement`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-SNAPSHOT-*` — [12.10.9 Diagnostics e gate](DESIGN.md#12109-diagnostics-e-gate)

#### W-SNAPSHOT-0008

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a snapshot cell is closed while a structured reader remains active

- `requiredFacts`:
  - `activeReaders`: `integer`
  - `operation`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-SNAPSHOT-*` — [12.10.9 Diagnostics e gate](DESIGN.md#12109-diagnostics-e-gate)

#### W-SNAPSHOT-0009

- `state`: `active`
- `phase`: `semantic.ownership`
- `severity`: `error`
- `meaning`: a snapshot cell is used after its structured lifetime ended

- `requiredFacts`:
  - `operation`: `string`
  - `phase`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-SNAPSHOT-*` — [12.10.9 Diagnostics e gate](DESIGN.md#12109-diagnostics-e-gate)

#### W-SNAPSHOT-0010

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: the selected snapshot provider strategy is not admitted by the profile

- `requiredFacts`:
  - `profile`: `string`
  - `strategy`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-SNAPSHOT-*` — [12.10.9 Diagnostics e gate](DESIGN.md#12109-diagnostics-e-gate)

### W-STD

#### W-STD-0001

- `state`: `active`
- `phase`: `link`
- `severity`: `error`
- `meaning`: a standard-library module uses the retired tier field

- `requiredFacts`:
  - `field`: `string`
  - `module`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-STD-0001` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

### W-STREAM

#### W-STREAM-0001

- `state`: `active`
- `phase`: `source.parse`
- `severity`: `error`
- `profile`: `parse-syntax`
- `meaning`: stream is a reserved keyword; rename an identifier to source or cursor

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`

- `labelRoles`:
  - `owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-STREAM-0001` — [12.9.12 Bloco compiler-owned de `Stream`](DESIGN.md#12912-bloco-compiler-owned-de-stream)

### W-SUSPEND

#### W-SUSPEND-0001

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a maySuspend call is used without await or a structured child form

- `requiredFacts`:
  - `callForm`: `string`
  - `callee`: `string`
  - `suspension`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-SUSPEND-0001` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

#### W-SUSPEND-0002

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `information`
- `meaning`: await is removable because the callee is neverSuspend

- `requiredFacts`:
  - `callee`: `string`
  - `suspension`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-SUSPEND-0002` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

#### W-SUSPEND-0003

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a general blocking wait is not allowed in structured execution

- `requiredFacts`:
  - `domain`: `string`
  - `operation`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-SUSPEND-0003` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

#### W-SUSPEND-0004

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a barrier dispatch body may suspend and retain the exclusive domain gate

- `requiredFacts`:
  - `callee`: `string`
  - `domain`: `string`
  - `suspension`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-SUSPEND-0004` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

#### W-SUSPEND-0005

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `meaning`: a sync call requires an explicitly declared async callable whose function type preserves directEntry available

- `requiredFacts`:
  - `callee`: `string`
  - `directEntry`: `string`
  - `reason`: `string`
  - `sourceSpelling`: `string`
  - `suspension`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-SUSPEND-0005` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

### W-SYNC

#### W-SYNC-0001

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a synchronization primitive is not part of the safe standard-library surface

- `requiredFacts`:
  - `alternatives`: `string-set`
  - `primitive`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `family` `W-SYNC-*` — [12.10.7 Exclusão mútua como último recurso](DESIGN.md#12107-exclusão-mútua-como-último-recurso)

### W-TIME

#### W-TIME-0001

- `state`: `active`
- `phase`: `source.capability`
- `severity`: `error`
- `meaning`: an active host-suspend policy request is outside the included/excluded subset or unsupported by the provider

- `requiredFacts`:
  - `provider`: `string`
  - `reason`: `string`
  - `requested`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-TIME-0001` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

#### W-TIME-0002

- `state`: `active`
- `phase`: `source.capability`
- `severity`: `error`
- `meaning`: clock acquisition has no explicit process or Context authority

- `requiredFacts`:
  - `expression`: `string`
  - `reason`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-TIME-0002` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

### W-TLS

#### W-TLS-0001

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `profile`: `thread-local`
- `meaning`: a native TLS descriptor is not constant, Copy, and drop-free

- `requiredFacts`:
  - `actual`: `string`
  - `descriptor`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `tls-binding`: minimum `0`, maximum `1`
  - `tls-operation`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-TLS-0001` — [19.3.4 Contexto de task e storage de thread](DESIGN.md#1934-contexto-de-task-e-storage-de-thread)

#### W-TLS-0002

- `state`: `active`
- `phase`: `link`
- `severity`: `error`
- `meaning`: native TLS is unavailable or would be emulated by a task or fiber

- `requiredFacts`:
  - `actual`: `string`
  - `descriptor`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `tls-binding`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-TLS-0002` — [19.3.4 Contexto de task e storage de thread](DESIGN.md#1934-contexto-de-task-e-storage-de-thread)

#### W-TLS-0003

- `state`: `active`
- `phase`: `semantic.effect`
- `severity`: `error`
- `profile`: `thread-local`
- `meaning`: a TLS operation suspends or lets a dependency escape

- `requiredFacts`:
  - `actual`: `string`
  - `descriptor`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `tls-binding`: minimum `0`, maximum `1`
  - `tls-operation`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-TLS-0003` — [19.3.4 Contexto de task e storage de thread](DESIGN.md#1934-contexto-de-task-e-storage-de-thread)

### W-TYPE

#### W-TYPE-0120

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: values do not have one unique safe common type

- `requiredFacts`:
  - `leftType`: `string`
  - `rightType`: `string`

- `labelRoles`:
  - `branch-result`: minimum `2`, maximum `unbounded`

- `fixes`:
  - none

- Design authority: `exact` `W-TYPE-0120` — [Diagnostics e recovery](DESIGN.md#diagnostics-e-recovery)

#### W-TYPE-0121

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: an enum value is outside the expected normalized case subset

- `requiredFacts`:
  - `actualCase`: `string`
  - `allowedCases`: `string-set`
  - `baseEnum`: `string`
  - `expectedType`: `string`

- `labelRoles`:
  - `expected-type`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-TYPE-0121` — [8.6.1 Subconjuntos de cases de enum](DESIGN.md#861-subconjuntos-de-cases-de-enum)

#### W-TYPE-0122

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: no total exact unique implicit conversion exists between the actual and expected types

- `requiredFacts`:
  - `actualType`: `string`
  - `candidateRoutes`: `string-set`
  - `expectedType`: `string`
  - `reason`: `string`

- `labelRoles`:
  - `call-owner`: minimum `1`, maximum `1`
  - `expected-type`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-TYPE-0122` — [8.8 Conversões](DESIGN.md#88-conversões)

#### W-TYPE-0123

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a mutation does not prove the refinement predicate for its resulting place

- `requiredFacts`:
  - `expectedPredicate`: `string`
  - `operation`: `string`
  - `place`: `string`
  - `proof`: `string`

- `labelRoles`:
  - `mutation-place`: minimum `1`, maximum `1`
  - `refinement-declaration`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-TYPE-0123` — [Diagnostics](DESIGN.md#diagnostics-1)

#### W-TYPE-0124

- `state`: `reserved`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a conditional cast source or target is incompatible with the nominal protocol composition

- `requiredFacts`:
  - `operation`: `string`
  - `reason`: `string`
  - `sourceType`: `string`
  - `targetType`: `string`

- `labelRoles`:
  - `target-type`: minimum `1`, maximum `1`
  - `type-subject`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-TYPE-0124` — [8.8.1 Teste de tipo runtime e recuperação borrowed](DESIGN.md#881-teste-de-tipo-runtime-e-recuperação-borrowed)

#### W-TYPE-0128

- `state`: `reserved`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: an info query requires Reflectable on its static type or existential composition

- `requiredFacts`:
  - `operation`: `string`
  - `reason`: `string`
  - `subjectType`: `string`

- `labelRoles`:
  - `type-subject`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-TYPE-0128` — [8.9.1 Dois planos de introspecção e subject determinístico](DESIGN.md#891-dois-planos-de-introspecção-e-subject-determinístico)

#### W-TYPE-0130

- `state`: `reserved`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: an is test lacks dynamic nominal identity or a nominal target

- `requiredFacts`:
  - `operation`: `string`
  - `sourceIdentity`: `string`
  - `targetType`: `string`

- `labelRoles`:
  - `type-subject`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-TYPE-0130` — [8.8.1 Teste de tipo runtime e recuperação borrowed](DESIGN.md#881-teste-de-tipo-runtime-e-recuperação-borrowed)

### W-UNIT

#### W-UNIT-0001

- `state`: `active`
- `phase`: `semantic.type`
- `severity`: `error`
- `meaning`: a unit literal uses an unbound unqualified unit name

- `requiredFacts`:
  - `imports`: `string[]`
  - `reason`: `string`
  - `unit`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - none

- Design authority: `exact` `W-UNIT-0001` — [12.2.1 Suspensão inferida e formas de call](DESIGN.md#1221-suspensão-inferida-e-formas-de-call)

### W-USE

#### W-USE-0001

- `state`: `active`
- `phase`: `semantic.flow`
- `severity`: `error`
- `meaning`: a non-Unit value is discarded without explicit intent

- `requiredFacts`:
  - `resultType`: `string`

- `labelRoles`:
  - none

- `fixes`:
  - `discard-value`: `review`

- Design authority: `exact` `W-USE-0001` — [Diagnostics](DESIGN.md#diagnostics-1)

### W-WIRE

#### W-WIRE-0001

- `state`: `active`
- `phase`: `interface`
- `severity`: `error`
- `meaning`: a service boundary requires a portable wire value but a member is local to one runtime domain

- `requiredFacts`:
  - `alternatives`: `string-set`
  - `reason`: `string`
  - `requiredProfiles`: `string-set`
  - `type`: `string`
  - `typePath`: `string`

- `labelRoles`:
  - `wire-boundary`: minimum `1`, maximum `1`
  - `wire-member`: minimum `1`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-WIRE-0001` — [22.5.3 Phases e codes](DESIGN.md#2253-phases-e-codes)

### W-YIELD

#### W-YIELD-0001

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: yield is allowed only inside a compiler-owned stream expression

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-YIELD-0001` — [12.9.12 Bloco compiler-owned de `Stream`](DESIGN.md#12912-bloco-compiler-owned-de-stream)

#### W-YIELD-0002

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: yield requires explicit take or copy ownership; bare yield value has no ownership operation

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-YIELD-0002` — [12.9.12 Bloco compiler-owned de `Stream`](DESIGN.md#12912-bloco-compiler-owned-de-stream)

#### W-YIELD-0003

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: a view, borrow, or inout value cannot cross a yield boundary

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-YIELD-0003` — [12.9.12 Bloco compiler-owned de `Stream`](DESIGN.md#12912-bloco-compiler-owned-de-stream)

#### W-YIELD-0004

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: a stream expression cannot add hidden capacity or prefetch

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-YIELD-0004` — [12.9.12 Bloco compiler-owned de `Stream`](DESIGN.md#12912-bloco-compiler-owned-de-stream)

#### W-YIELD-0005

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: a stream expression cannot expose a frame, resume token, or scheduler

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-YIELD-0005` — [12.9.12 Bloco compiler-owned de `Stream`](DESIGN.md#12912-bloco-compiler-owned-de-stream)

#### W-YIELD-0006

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: stream failure and terminal return must use the declared typed contract

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-YIELD-0006` — [12.9.12 Bloco compiler-owned de `Stream`](DESIGN.md#12912-bloco-compiler-owned-de-stream)

#### W-YIELD-0007

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: yield cannot run from defer or cleanup code

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-YIELD-0007` — [12.9.12 Bloco compiler-owned de `Stream`](DESIGN.md#12912-bloco-compiler-owned-de-stream)

#### W-YIELD-0008

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: next is exclusive; concurrent or reentrant stream pulls are rejected

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-YIELD-0008` — [12.9.12 Bloco compiler-owned de `Stream`](DESIGN.md#12912-bloco-compiler-owned-de-stream)

#### W-YIELD-0009

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: stream resume is not an FFI boundary

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-YIELD-0009` — [12.9.12 Bloco compiler-owned de `Stream`](DESIGN.md#12912-bloco-compiler-owned-de-stream)

#### W-YIELD-0010

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: stream captures must be explicit and are evaluated before the Stream is returned

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-YIELD-0010` — [12.9.12 Bloco compiler-owned de `Stream`](DESIGN.md#12912-bloco-compiler-owned-de-stream)

#### W-YIELD-0011

- `state`: `active`
- `phase`: `source.validate`
- `severity`: `error`
- `profile`: `run-validation`
- `meaning`: yield copy requires a Duplicable item and preserves the original binding

- `requiredFacts`:
  - `actual`: `string`
  - `construct`: `string`
  - `expected`: `string-set`
  - `reason`: `string`

- `labelRoles`:
  - `run-owner`: minimum `0`, maximum `1`

- `fixes`:
  - none

- Design authority: `exact` `W-YIELD-0011` — [12.9.12 Bloco compiler-owned de `Stream`](DESIGN.md#12912-bloco-compiler-owned-de-stream)
