# Estudos do W

> Esta página é uma projeção humana gerada de `tooling/study-registry.json`.
> Edite os metadados dos estudos e os READMEs locais. Não edite este arquivo manualmente.
>
> Estudos e seus oracles registram evidência de design e o estado da infraestrutura.
> Eles não definem a semântica do W e não provam resultados de compiler, runtime, provider,
> revisão humana ou modelo. Leia `DESIGN.md` para as decisões normativas.

## Resumo

| Status | Estudos |
|---|---:|
| `complete-design-study` | 8 |
| `design-oracle-input` | 59 |
| `design-oracle-input-cap0` | 1 |
| `design-oracle-input-syn1` | 1 |
| `protocol-ready` | 1 |
| `registered-research-bundle` | 1 |
| **Total** | **71** |

O registry de máquina também registra metadados, fixtures, referências, digests, dependências e entrypoints de scripts.
Use `bun run study:registry` para regenerar as duas projeções e `bun run check:study-registry` para validá-las.

## Status: `complete-design-study` (8)

| ID | Função / estado | Caminho | Gate principal | Entrypoint principal |
|---|---|---|---|---|
| `DRC0` | Drc0 Design Research Closure — `complete-design-study` | [`tooling/studies/drc0-design-research-closure`](./tooling/studies/drc0-design-research-closure/) | `—` | `bun run check:drc0` |
| `FST0` | First-settled selection of existing structured tasks — `complete-design-study` | [`tooling/studies/fst0-first-settled-task`](./tooling/studies/fst0-first-settled-task/) | `—` | `bun run check:fst0` |
| `LLM0` | Training and inference readiness inventory — `complete-design-study` | [`tooling/studies/llm0-training-inference`](./tooling/studies/llm0-training-inference/) | `—` | `bun run check:llm0` |
| `MEM0` | Virtual memory and data movement performance contract — `complete-design-study` | [`tooling/studies/mem0-virtual-memory-data-movement`](./tooling/studies/mem0-virtual-memory-data-movement/) | `—` | `bun run check:mem0` |
| `QOS0` | Domain placement without portable task priority or QoS — `complete-design-study` | [`tooling/studies/qos0-scheduling-boundary`](./tooling/studies/qos0-scheduling-boundary/) | `—` | `bun run check:qos0` |
| `SEA0` | Simulated effects, deferred approval, and deterministic test infrastructure — `complete-design-study` | [`tooling/studies/sea0-simulated-effects-approval`](./tooling/studies/sea0-simulated-effects-approval/) | `—` | `bun run check:sea0` |
| `SVC0` | Directional service streams without implicit channels — `complete-design-study` | [`tooling/studies/svc0-service-stream-directions`](./tooling/studies/svc0-service-stream-directions/) | `—` | `bun run check:svc0` |
| `TGM0` | Closed finite TaskGroup map and collect families — `complete-design-study` | [`tooling/studies/tgm0-task-group-map-collect`](./tooling/studies/tgm0-task-group-map-collect/) | `—` | `bun run check:tgm0` |

## Status: `design-oracle-input` (59)

| ID | Função / estado | Caminho | Gate principal | Entrypoint principal |
|---|---|---|---|---|
| `AEG0` | App Essentials Gate host study — `design-oracle-input` | [`tooling/studies/aeg0-app-essentials-gate`](./tooling/studies/aeg0-app-essentials-gate/) | `AEG0-app-essentials-gate` | `bun run check:aeg0` |
| `ASIC0` | Reuse-only implementation evidence gap closure — `design-oracle-input` | [`tooling/studies/asic0-evidence-gap-closure`](./tooling/studies/asic0-evidence-gap-closure/) | `W-1355/W-1359/W-1420/W-1425/W-1435` | `bun run check:asic0` |
| `ATOM1` | Atomic value derivation, generational handles, and reclamation boundaries — `design-oracle-input` | [`tooling/studies/atom1-atomic-extensibility`](./tooling/studies/atom1-atomic-extensibility/) | `ATOM0-G1` | `bun run check:atom1` |
| `ATOM2` | Canonical atomic value carriers, checked handles, and reclamation boundaries — `design-oracle-input` | [`tooling/studies/atom2-atomic-contract`](./tooling/studies/atom2-atomic-contract/) | `ATOM0-G1` | `bun run check:atom2` |
| `AVF0` | Static package features, availability evidence, and typed runtime configuration — `design-oracle-input` | [`tooling/studies/avf0-availability-feature`](./tooling/studies/avf0-availability-feature/) | `AVF0-R1` | `bun run check:avf0` |
| `BRX2` | Requirement-owned relations for borrowed higher-order results — `design-oracle-input` | [`tooling/studies/brx2-borrow-relations`](./tooling/studies/brx2-borrow-relations/) | `BRX2-R1` | `bun run check:brx2` |
| `BRX3` | Explicit source clauses for open borrowed-result relations — `design-oracle-input` | [`tooling/studies/brx3-borrow-relations`](./tooling/studies/brx3-borrow-relations/) | `BRX3-R1` | `bun run check:brx3` |
| `CYC1` | Explicit cycle lifecycle and conditional liveness — `design-oracle-input` | [`tooling/studies/cyc1-explicit-cycle-lifecycle`](./tooling/studies/cyc1-explicit-cycle-lifecycle/) | `CYC0-G1` | `bun run check:cyc1` |
| `CYC2` | Close conditional liveness by composition — `design-oracle-input` | [`tooling/studies/cyc2-conditional-liveness`](./tooling/studies/cyc2-conditional-liveness/) | `CYC2-R1` | `bun run check:cyc2` |
| `DYN1` | Versioned dynamic behavior, generation switching, and bounded export — `design-oracle-input` | [`tooling/studies/dyn1-versioned-behavior`](./tooling/studies/dyn1-versioned-behavior/) | `DYN0-G1` | `bun run check:dyn1` |
| `FRC0` | Historical snapshot of the three research gates — `design-oracle-input` | [`tooling/studies/final-research-closure`](./tooling/studies/final-research-closure/) | `FZ0-freeze-completeness/freeze-research-close/HUM0-promotion` | `bun run check:frc0` |
| `HRD0` | Development-only hot reload around normal W units — `design-oracle-input` | [`tooling/studies/hrd0-hot-reload-dev`](./tooling/studies/hrd0-hot-reload-dev/) | `HRD0-G1` | `bun run check:hrd0` |
| `IPC1` | Mapped immutable snapshots and bounded process-shared channels — `design-oracle-input` | [`tooling/studies/ipc1-mapped-ipc`](./tooling/studies/ipc1-mapped-ipc/) | `IPC0-R1` | `bun run check:ipc1` |
| `PFU0` | PFU0 closure evidence for roots, service streams, and properties — `design-oracle-input` | [`tooling/studies/pfu0-pre-freeze-usability`](./tooling/studies/pfu0-pre-freeze-usability/) | `PFU0-pre-freeze-usability` | `bun run check:pfu0` |
| `PKG1` | Project identity split and atomic root transactions — `design-oracle-input` | [`tooling/studies/pkg1-project-transaction`](./tooling/studies/pkg1-project-transaction/) | `—` | `bun run check:pkg1` |
| `PRC0` | Provider/runtime closure routes for seven research gates — `design-oracle-input` | [`tooling/studies/prc0-provider-runtime-closure`](./tooling/studies/prc0-provider-runtime-closure/) | `—` | `bun run check:prc0` |
| `R1-allocator-runtime-slot` | Allocator capability position at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-allocator-runtime-slot`](./tooling/studies/r1-allocator-runtime-slot/) | `—` | `bun run check:study-bundles` |
| `R1-arena-scope` | Fixed Arena scope at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-arena-scope`](./tooling/studies/r1-arena-scope/) | `—` | `bun run check:study-bundles` |
| `R1-assignment-unit` | Unit assignment and place evaluation at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-assignment-unit`](./tooling/studies/r1-assignment-unit/) | `—` | `bun run check:study-bundles` |
| `R1-associated-members` | Associated members at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-associated-members`](./tooling/studies/r1-associated-members/) | `—` | `bun run check:study-bundles` |
| `R1-borrow-expressivity` | Higher-order borrowed results at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-borrow-expressivity`](./tooling/studies/r1-borrow-expressivity/) | `—` | `bun run check:borrow-expressivity` |
| `R1-call-label-order` | Money initializer label order at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-call-label-order`](./tooling/studies/r1-call-label-order/) | `—` | `bun run check:study-bundles` |
| `R1-callable-model-last-light` | Callable representation and ownership at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-callable-model`](./tooling/studies/r1-callable-model/) | `—` | `bun run check:study-bundles` |
| `R1-callable-property-surface` | Property-safe and callable surfaces at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-callable-property-surface`](./tooling/studies/r1-callable-property-surface/) | `—` | `bun run check:study-bundles` |
| `R1-conditional-value-block` | Conditional stage values at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-conditional-value-block`](./tooling/studies/r1-conditional-value-block/) | `—` | `bun run check:study-bundles` |
| `R1-consuming-receiver-last-light` | Command-stream ownership at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-consuming-receiver`](./tooling/studies/r1-consuming-receiver/) | `—` | `bun run check:study-bundles` |
| `R1-contract-envelopes-last-light` | Stage-path contract envelopes at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-contract-envelopes`](./tooling/studies/r1-contract-envelopes/) | `—` | `bun run check:study-bundles` |
| `R1-control-flow-last-light` | Diagnostic carrier scan at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-control-flow`](./tooling/studies/r1-control-flow/) | `—` | `bun run check:study-bundles` |
| `R1-data-declaration-surface` | Transparent records and encapsulated objects at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-data-declaration-surface`](./tooling/studies/r1-data-declaration-surface/) | `—` | `bun run check:study-bundles` |
| `R1-delimited-value-surface` | Nested and singleton delimited values at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-delimited-value-surface`](./tooling/studies/r1-delimited-value-surface/) | `—` | `bun run check:study-bundles` |
| `R1-end-relative-access` | Menu tail access at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-end-relative-access`](./tooling/studies/r1-end-relative-access/) | `—` | `bun run check:study-bundles` |
| `R1-fail-fast-last-light` | Galley-pair fail-fast coordination at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-fail-fast`](./tooling/studies/r1-fail-fast/) | `—` | `bun run check:study-bundles` |
| `R1-fluent-self` | Fluent receiver reborrow at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-fluent-self`](./tooling/studies/r1-fluent-self/) | `—` | `bun run check:study-bundles` |
| `R1-gen1-incremental-suspension` | Incremental suspension ergonomics and lowering at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/gen1-incremental-suspension`](./tooling/studies/gen1-incremental-suspension/) | `—` | `bun run check:gen1` |
| `R1-gen2-stream-yield` | Compiler-owned pull producer ergonomics at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/gen2-stream-yield`](./tooling/studies/gen2-stream-yield/) | `—` | `bun run check:gen2` |
| `R1-imports-last-light` | Console-mode imports at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-imports`](./tooling/studies/r1-imports/) | `—` | `bun run check:study-bundles` |
| `R1-manifest-surface` | Data-only package manifests at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-manifest-surface`](./tooling/studies/r1-manifest-surface/) | `—` | `bun run check:study-bundles` |
| `R1-multiple-initializers` | Money construction at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-multiple-initializers`](./tooling/studies/r1-multiple-initializers/) | `—` | `bun run check:study-bundles` |
| `R1-pattern-surface` | Nominal and open patterns at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-pattern-surface`](./tooling/studies/r1-pattern-surface/) | `—` | `bun run check:study-bundles` |
| `R1-post-test-loop` | Post-test counting at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-post-test-loop`](./tooling/studies/r1-post-test-loop/) | `—` | `bun run check:study-bundles` |
| `R1-power-precedence` | Mathematical power precedence at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-power-precedence`](./tooling/studies/r1-power-precedence/) | `—` | `bun run check:study-bundles` |
| `R1-python-transform` | Urgent ticket transformation at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-python-transform`](./tooling/studies/r1-python-transform/) | `—` | `bun run check:study-bundles` |
| `R1-refinement-subject` | Refinement subjects at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-refinement-subject`](./tooling/studies/r1-refinement-subject/) | `—` | `bun run check:study-bundles` |
| `R1-shared-construction` | First shared owner at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-shared-construction`](./tooling/studies/r1-shared-construction/) | `—` | `bun run check:study-bundles` |
| `R1-source-boundaries` | Source boundaries and fixed formatting at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-source-boundaries`](./tooling/studies/r1-source-boundaries/) | `—` | `bun run check:study-bundles` |
| `R1-source-phase-surface` | Import phase and function bodies at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-source-phase-surface`](./tooling/studies/r1-source-phase-surface/) | `—` | `bun run check:study-bundles` |
| `R1-spawn-domain-last-light` | Bounded galley planning at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-spawn-domain`](./tooling/studies/r1-spawn-domain/) | `—` | `bun run check:study-bundles` |
| `R1-static-contract-slots` | Static contract slots at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-static-contract-slots`](./tooling/studies/r1-static-contract-slots/) | `—` | `bun run check:study-bundles` |
| `R1-static-contract-syntax` | Attached and nested contracts at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-static-contract-syntax`](./tooling/studies/r1-static-contract-syntax/) | `—` | `bun run check:study-bundles` |
| `R1-suspend-accounting-names` | Suspend accounting names at the Last Light clock — `design-oracle-input` | [`tooling/studies/r1-suspend-accounting-names`](./tooling/studies/r1-suspend-accounting-names/) | `—` | `bun run check:study-bundles` |
| `R1-tabular-adapters` | Same horizon telemetry through CSV, Parquet, and Arrow TAB1 workflows — `design-oracle-input` | [`tooling/studies/r1-tabular-adapters`](./tooling/studies/r1-tabular-adapters/) | `—` | `bun run check:study-bundles` |
| `R1-tabular-carrier` | Black-hole telemetry carrier at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-tabular-carrier`](./tooling/studies/r1-tabular-carrier/) | `—` | `bun run check:study-bundles` |
| `R1-tensor-broadcast` | Horizon calibration broadcast at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-tensor-broadcast`](./tooling/studies/r1-tensor-broadcast/) | `—` | `bun run check:study-bundles` |
| `R1-tuple-unpacking` | Menu compiler tuple result at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-tuple-unpacking`](./tooling/studies/r1-tuple-unpacking/) | `—` | `bun run check:study-bundles` |
| `R1-units-last-light` | Black-hole impact units at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-units`](./tooling/studies/r1-units/) | `—` | `bun run check:study-bundles` |
| `R1-weak-owner-acquisition` | Weak owner acquisition at the Last Light restaurant — `design-oracle-input` | [`tooling/studies/r1-weak-owner-acquisition`](./tooling/studies/r1-weak-owner-acquisition/) | `—` | `bun run check:study-bundles` |
| `R1C0` | Comparative ergonomics closure with bounded WLO1 fallback — `design-oracle-input` | [`tooling/studies/r1c0-closure`](./tooling/studies/r1c0-closure/) | `—` | `bun run check:r1c0` |
| `SEC0` | Broad security model for safe W across physical targets — `design-oracle-input` | [`tooling/studies/sec0-security-model`](./tooling/studies/sec0-security-model/) | `SEC0-R1` | `bun run check:sec0` |
| `SYN2-DYN2` | Typed generated module sets and bounded versioned behavior closure — `design-oracle-input` | [`tooling/studies/syn2-dyn2-closure`](./tooling/studies/syn2-dyn2-closure/) | `DYN0-G1` | `bun run check:syn2-dyn2` |

## Status: `design-oracle-input-cap0` (1)

| ID | Função / estado | Caminho | Gate principal | Entrypoint principal |
|---|---|---|---|---|
| `CAP0` | Cap0 Capability Matrix — `design-oracle-input-cap0` | [`tooling/studies/cap0-capability-matrix`](./tooling/studies/cap0-capability-matrix/) | `—` | `bun run check:study-bundles` |

## Status: `design-oracle-input-syn1` (1)

| ID | Função / estado | Caminho | Gate principal | Entrypoint principal |
|---|---|---|---|---|
| `SYN1` | Generated module artifacts and hermetic typed generation — `design-oracle-input-syn1` | [`tooling/studies/syn1-typed-generation`](./tooling/studies/syn1-typed-generation/) | `SYN0-R1` | `bun run check:syn1` |

## Status: `protocol-ready` (1)

| ID | Função / estado | Caminho | Gate principal | Entrypoint principal |
|---|---|---|---|---|
| `HUM0` | Human and model review protocol for W ergonomics — `protocol-ready` | [`tooling/studies/hum0-human-review`](./tooling/studies/hum0-human-review/) | `HUM0` | `bun run check:hum0` |

## Status: `registered-research-bundle` (1)

| ID | Função / estado | Caminho | Gate principal | Entrypoint principal |
|---|---|---|---|---|
| `RDX0` | Binary-first distribution, static-first registry, signed execution, and provider research — `registered-research-bundle` | [`tooling/studies/rdx0-binary-registry-execution`](./tooling/studies/rdx0-binary-registry-execution/) | `—` | `bun run check:rdx0` |
