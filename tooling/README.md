# Tooling inicial de W

> **Status:** Working Draft. Highlighting e o parser incremental experimental
> são utilizáveis; parser do compilador, formatter, LSP e compilador ainda não
> existem.

Este diretório antecipa a experiência de escrever W sem transformar cores em
semântica. A autoridade normativa continua em [DESIGN.md](../DESIGN.md).
[RATIONALE.md](../RATIONALE.md) fornece evidência e ledger sem definir
comportamento; nenhum highlighter aceita ou rejeita um programa em nome da
linguagem.

## Duas camadas, uma só gramática sintática

| Camada | Papel imediato | Limite |
|---|---|---|
| [VS Code/TextMate](vscode-w/README.md) | highlighting lexical local, comentários, pares e indentação | regex tolerante; não produz CST nem diagnósticos |
| [Tree-sitter](tree-sitter-w/README.md) | parser incremental e queries estruturais sobre o subset candidato | protótipo; o gate do parser normativo está em `DESIGN.md` |
| Corpus Tree-sitter | positivos e snapshots de CST em `tree-sitter-w/test/corpus/` | execução W ainda não existe |
| `check-design-examples.mjs` | confirma exemplo local em cada seção normativa terminal | inspeção estrutural; não valida a semântica do exemplo |
| `check-markdown-links.mjs` | valida targets e anchors locais fora do histórico | não consulta links externos |
| `design-index.mjs` | gera intervalos e métricas separadas de `DESIGN.md` e `RATIONALE.md` | projeção navegável; não define decisões |
| `design-ledger.mjs` | lê, valida e exporta as linhas ordenadas do ledger em `RATIONALE.md` para os checkers | helper de uma fonte; não define contratos |
| `design-slice.mjs` | recorta seção/heading de DESIGN ou heading/ledger de RATIONALE com contexto | leitura somente; não cria autoridade paralela |
| `formatter-cases.json` + checker | 27 pares input/output CST-equivalentes e snapshots de `w fmt --check`; os pares allocator cobrem bloco anônimo, call contextual e override nomeado; o par foreign formata a assinatura e preserva cada byte do body opaco | oracle de design; formatter ainda não existe |
| `semantic-cases.json` + checker | pares S0, resultados normalizados e diagnostics D0 | expectativas estruturadas; type checker ainda não existe |
| `frontend-freeze-cases.json` + `check-frontend-freeze.mjs` + guards/test + snapshot | FZ0 ratifica as seis famílias normalizadas G0–G5 com source Last Light real (digest/symbol), parse sem recovery, pares F0 CST-equivalentes com alvo byte canônico, inversão S0 ou waiver PYN1 e D0 exato; 21 decisões são cobertas sem duplicatas ou expected echo | oracle de design; F0 não prova idempotência nem implementa formatter, e o checker não implementa parser, compiler, runtime ou provider |
| `borrow-expressivity-cases.json` + máquina/checker/test + snapshot | BRX0 deriva 22 casos (15 mappings aceitos, sete blockers Research e quatro negativos de invocation) para receiver/body mapping, callable loans, lending cursor, adapter OriginSet, await, escapes, `any fn`, boundaries e drift/mutations de interface | oracle host de design; free/protocol bodyless com dois inputs permanece Research; não implementa compiler, runtime, provider nem metadata de lifetime |
| `studies/atom1-atomic-extensibility/study.json` + `atom1-atomic-extensibility-cases.json` + máquina/checker/test + snapshot | ATOM1 separa atualização de record value-only (A), handle geracional/owner (B) e retirement/reclamation (C); 70 casos cobrem carrier canônico versus raw-layout, facts derivados de fields, zero-bit rejection, packing, SnapshotCell/domain, target native/lock-free/fallback, widths até 128 bits, proofs de pointer, eventos de reclamation, foreign boundaries, shutdown e drain FFI | oracle host de design; o carrier canônico e o adapter de reclamation permanecem Research; não implementa compiler, runtime, provider ou FFI |
| `capability-matrix-cases.json` + máquina/checker/test + snapshot | CAP0 consolida oito eixos por problema comum, tenta composição W com Last Light, preserva invariantes e deriva rotas `current`/`composable`/`research` pelas subcapacidades do problema; 149 refs primárias ou source-backed e oito filas de documentação futura | fonte editorial de staging; não mede maturidade, não copia features e não implementa compiler, runtime ou provider |
| `semantic-diagnostic-matrix-cases.json` + máquina/checker/test | SDM0 deriva SemanticResult, CheckerContext, loop fixed point, AST→HIR schema, D0 records, causality, ordering, limits, policy, lex/parse boundary e cobertura de meta contracts | oracle host independente; não implementa checker, compiler, formatter ou runtime |
| `execution-ergonomics-cases.json` + máquina/checker/snapshot | 80 casos (32 positivos, 46 negativos e duas informações) derivam labels, parâmetros, slots allocator contextuais/ordinários e collision, operações explícitas de ownership, suspension, placement, barriers, process projections, doctests, std e lanes seriais dinâmicas; 26 testes host usam entradas independentes | oracle host de design; não executa W nem implementa S0, scheduler, pool ou provider |
| `check-source-call-shapes.mjs` | aplica labels, posição do contrato e operações explicitamente incompatíveis aos sources do Última Luz e da std | auditoria source; owner place sem marker exige type/value category em S0; member/import é conservador e não executa W |
| `foreign-body-cases.json` + máquina/checker/snapshot | FB0 cobre 45 casos/90 operações (15 aceitos, 28 rejeitados e duas informações) para bytes opacos, delimitação C, fallback editorial, limits, source map e recipe; nove testes host usam source independente | oracle host de design; o external scanner Tree-sitter é projeção e não implementa adapter C, compiler, formatter ou builder |
| `web-body-cases.json` + máquina/checker/snapshot | WB0 cobre 27 casos/160 operações (12 aceitos + 15 rejeitados) para Blob, FormData, retained-byte limits, boundary, attachment e parse; seis testes host usam inputs independentes | oracle host de design; não executa W, compiler, HTTP provider ou codec multipart |
| `process-root-cases.json` + máquina/checker/snapshot | PR0 cobre 48 casos/249 operações (27 aceitos + 21 rejeitados) para root projections, argv nativo, Context, filesystem, clock default/active, stdio, signals, service drain e ExitCode; onze testes host usam inputs independentes | oracle host de design; não executa W, compiler, OS, signal adapter, scheduler ou provider `std.process@1` |
| `filesystem-cases.json` + máquina/checker/snapshot | FS0 cobre 99 casos/665 operações (40 aceitos + 59 rejeitados) para paths nativos, root containment bounded, child scopes, rights, I/O posicional u64, interferência, append, cursor, snapshot, directory, metadata, namespace, durability e cancellation; 14 testes host usam inputs independentes | oracle host de design; não executa W, syscalls, filesystem real, compiler, runtime ou provider `std.fs@1` |
| `io-error-cases.json` + máquina/checker/snapshot | IOE0 cobre 44 casos/219 operações (32 aceitos + 12 rejeitados) para kind portátil, operação lógica, causa opaca bounded, control outcomes, duplicate e recovery contextual; sete testes host usam entradas independentes | oracle host de design; não executa W, adapter, syscall ou provider `std.io@1` |
| `operational-time-cases.json` + máquina/checker/snapshot | TIME0 cobre 52 casos/277 operações (27 aceitos + 25 rejeitados) para Duration exata, Clock root-scoped, origin, default/active HostSuspendPolicy, profile monotônico, deadlines, boundaries e clock virtual; oito testes host usam entradas independentes | oracle host de design; não executa W, clock, timer, scheduler, OS ou provider `std.time@1` |
| `kernel-module-cases.json` + máquina/checker/snapshot | KM0 cobre 32 casos/218 operações (6 aceitos + 26 rejeitados) para head de síntese, identities, call graph, famílias genéricas, artifacts source-backed/closed e ausência de JIT; nove testes host usam inputs independentes | oracle host de design; não executa W, compiler, kernel, linker, driver ou provider |
| `substitution-cases.json` + checker | formas vigentes e substituídas ligadas aos 74 requisitos R0 da seção 1 de `RATIONALE.md` | oracle de design; os estudos com humanos e modelos ainda não foram executados |
| `design-freeze-audit.json` + checker | combina eixos source, oracle e disposition explícita; 547/1351 decisões estão classificadas (170 source, 432 oracle e 8 explícitas), dois contratos exigem múltiplos eixos e 63 overlaps não inflam a cobertura | worklist do freeze; não transforma cobertura parcial em aprovação |
| `substitution-surface.snapshot.json` + runner | baseline determinística de bytes, code points, linhas e lexemes para as 190 formas R0 derivadas pelo script | não mede compreensão, correção nem tokens de um modelo |
| `studies/*/bundle.json` + checker | 38 bundles R1, 98 variantes e 152 tarefas; 67/75 casos R0 são promovidos | parse e oracle host não equivalem a compilar ou executar W |
| `tabular-carrier-cases.json` + máquina/checker/snapshot | TAB0 fecha publication, schema identity, columns, chunks, copy/device, trust, owner/release e limits com casos positivos e negativos | oracle host independente; não compila W, não executa runtime e não implementa provider ou format adapter |
| `tabular-carrier-reference.test.mjs` | testes host independentes para o carrier tabular e a fronteira explícita de evidência | teste não prova compiler, runtime, CSV, Parquet, Arrow ou DataFrame de produção |
| `tabular-adapter-cases.json` + máquina/checker/snapshot | TAB1 deriva source kind, u64 snapshot offsets/short reads, nominal schema identity, publication, CSV tokenizer/nulls, Parquet footer/page/mapping/key/commit, Arrow IPC dictionary/buffer, borrowed view, copy materialization, progress/cancel, provenance, C quota/trust/release; 86 casos e 193 operações (36 aceitos + 50 rejeitados) | oracle host independente; símbolos Last Light são cross-linked; não implementa reader CSV/Parquet/Arrow, compiler, runtime ou provider |
| `tabular-adapter-reference.test.mjs` | teste host independente para cada caso TAB1 e digest de estado | não executa W, codec binário, C bridge ou device transfer |
| `wire-reference.test.mjs` | codec host mínimo para os vetores `MenuKey` e falhas estritas | primeiro protótipo; não é o encoder do compiler |
| `wire-diagnostic-cases.json` | par portátil/local para `W-WIRE-0001`, com facts e spans esperados | oracle de design; não é output do checker de interface |
| `wire-reference.c` + `wire-reference-c.test.mjs` | segunda implementação independente dos vetores e erros básicos | gate opcional; exige um GCC compatível |
| `hir-memory-reference.test.mjs` | modelo executável de owner, borrow, suspensão, boundary e ABI | oracle de SH3/SH4; não é o verifier do compiler |
| `memory-transition-cases.json` + máquina M1 | 185 sequências do Última Luz com 606 operações (82 aceitas + 103 rejeitadas), estados e traces byte-exact | oracle host tabelado de PlaceId, dependency/allocation origins, allocator-scope/rehome, erasure, shared/weak/ciclos, pinning, construção direta, FFI e ABI; não é HIR emitida pelo frontend nem allocator/runtime real |
| `allocation-cases.json` + máquina A0 | 48 sequências com 123 operações (15 aceitas + 33 rejeitadas) e 13 testes independentes | oracle host de layout, receipt, resize, provider, progress, domain e reclamation; não é allocator, verifier nem runtime W |
| `allocator-scope-cases.json` + máquina/checker/snapshot | ASC0 cobre 62 casos (26 positivos + 36 negativos) para named/anonymous owner, current allocator stack, contextual chain/root fallback, explicit override, root default/`.none`, requirement compatibility, first/unique slots, nested push/pop, overload collision, initializer rejection, callable preservation, closure capture, stable await, local spawn rejection, rehome-before-boundary, fixed admission, `.bounded` Research rejection, concrete custom `AllocatorPlan` descriptor validation, open/lease transitions, exactly-once deinit e close order | oracle host de design; não executa compiler, runtime, lowering físico ou provider W |
| `shared-control-cases.json` + máquina/checker/snapshot | SHC0 cobre 45 casos e 84 operações (16 accepted + 6 error + 3 fault + 20 rejected) para default/custom/lexical `shared T`, admission/open separado, eixos `initializerThrows`/site `failure`, error set explícito e exato com collapse de tipos iguais, `try` fora do tipo, reserve/init failure, consuming cleanup, strong/weak lifecycle e acquisition, hidden `$controlBlock` e reachable origins, derived mobility, nested origins, FFI canônico em `memory.w` com unregister/drain/destroy order, cycles e co-allocation sem promessa | oracle host independente; facts de tipo/HIR e lowering ficam em sidecars, o close/drain da lease externa pertence ao ASC0, operações internas M1 não são syntax W e o modelo não executa compiler, runtime, allocator ou provider |
| `execution-concurrency-cases.json` + máquina E0 | 73 sequências e 677 operações (38 aceitas + 35 rejeitadas) cobrem lifecycle, cancellation, dez origens happens-before, atomics, wait/notify, subtrees de ticket, barriers e races | oracle host de eventos; não é scheduler, checker, parking provider nem runtime W |
| `runtime-liveness-cases.json` + máquina E1 | 41 sequências e 473 operações (19 aceitas + 22 rejeitadas), sete testes host; closure, waits, completion/cancel races, generations, frame/outcome split, blocking foreign e shutdown | oracle host de runtime closure e liveness; não prova scheduler, clock, OS I/O, allocator, verifier ou runtime W |
| `ownership-execution-cases.json` + máquina MX0 | 46 sequências e 274 operações (23 aceitas + 23 rejeitadas), 14 testes host; call direta, await, staging, capture, admission, cancellation, cleanup, outcome, join, drop e equivalência de lowering | oracle host cross-axis; compõe M1/E0/E1, mas não implementa checker, scheduler, runtime, allocator ou provider W |
| `channel-cases.json` + máquina CH0 | 47 sequências e 333 operações (28 aceitas + 19 rejeitadas), 12 testes host; ownership linear, capacity 0/1/64, admission FIFO, permits, cancellation, close, abort, happens-before e estratégias ring/mutex | oracle host bounded; não implementa checker, scheduler, runtime, allocator ou provider `Channel` W |
| `context-local-cases.json` + máquina CTX0 | 25 casos e 94 operações (10 aceitos + 15 rejeitados), seis testes host; descriptor nominal, default, rebind, child snapshot, drain, boundaries, TLS físico, migration e availability | oracle host de contexto estruturado; não implementa compiler, scheduler, TLS nativo ou provider runtime |
| `interference-layout-cases.json` + máquina IL0 | 30 casos e 140 operações (22 aceitos + 8 rejeitados), nove testes host; layout privado, fallback, footprint, partition, atomic global e boundary física | oracle host de decisão de layout; não mede cache nem implementa compiler, allocator ou backend |
| `scoped-lock-cases.json` + máquina LM1 | 39 casos e 86 operações (20 aceitos + 18 rejeitados + uma fault), onze testes host; declaração `shared`, três formas de lock, busy sem body, ordem do provider não normativa, cancellation, boundary, drain e seleção lock-avoiding | oracle host da construção `lock`; não implementa compiler, scheduler, runtime ou provider |
| `snapshot-cell-cases.json` + máquina SP0 | 27 casos e 82 operações (14 aceitos + 12 rejeitados + uma fault), sete testes host; publication order, version stability, retirement bounded, drop único e quatro estratégias equivalentes | oracle host de snapshot publicado; não implementa compiler, runtime, scheduler ou provider `std.sync@1` |
| `lazy-behavior-cases.json` + máquina LZ0 | winner, waiters, lowering, publication edge, reentrada, cancellation, mutation e drop | oracle host de `Lazy`; não implementa compiler, runtime ou provider de parking |
| `boundary-effect-cases.json` + máquina B0 | 39 sequências e 320 operações cobrem service turn, commit gate, transaction e pipeline | oracle host de effects; não é adapter, transport ou storage real |
| `service-recovery-cases.json` + máquina SR0 | 48 casos e 392 operações (18 aceitos + 30 rejeitados), 17 testes host; mailbox, dedup, journal, process/network faults, generations, compaction, restart e shutdown | oracle host que compõe B0/E1; não executa W, wWire, database, filesystem, network, runtime ou provider |
| `package-release-cases.json` + máquina P0 | 44 sequências e 379 operações cobrem resolver, lock, CAS, recipe, mirror, rebuild e release | oracle host de supply chain; não é resolver, registry, CAS ou signer real |
| `script-workflow-cases.json` + máquina PYN1 | 91 casos/533 operações (22 aceitos + 69 rejeitados, incluindo multi-target, parse evidence e sidecar records) para header, context, roots físicos opacos, imports, virtual selection, lock P0 (`contexts`/`packages`), grafo transitivo, fetch/CAS por digest, requirement admission, identity, entry, cleanup e promotion | oracle host de design; não é CLI, compiler, resolver, provider, runtime ou execução W |
| `std-api-contracts.json` + checker de std | perfis cobrem 377 APIs em 29 módulos, 92 superfícies qualificadas, 31/31 requisitos contratados e 0/8 carriers missing | catálogo e snapshot são projeções; módulos catalogados são drafts e seus 23/23 providers intrinsics continuam missing; Blob é composição W sem provider próprio; bounds determinísticos entram na recipe key |
| `dlpack-cases.json` + máquina/checker/snapshot | PYN4 fecha DLPack 1.3 versioned e modela o fast path C Exchange N0 scoped; cobre Device/Queue, zero-copy, materialização, capsule, Python lease, drain/release, dtype/layout, provenance e hidden copy; 75 casos/326 operações (26 aceitos + 49 rejeitados) | oracle host independente; não compila ou executa W, Python, CUDA, ROCm, provider ou runtime |
| [portal](../portal/README.md) | preview e leitura lexical no browser | fallback local; não compila nem prova semântica |

### R1E0 — núcleo de expressions

Os bundles de evidência do núcleo usam os sources canônicos de Última Luz sem
alterá-los:

- [`r1-post-test-loop`](studies/r1-post-test-loop) compara `repeat` com um
  `while true` válido e mede body, predicate, `continue`, `break` e cleanup.
- [`r1-conditional-value-block`](studies/r1-conditional-value-block) compara
  value blocks com returns de branch e cobre Unit, joins, effects e discard.
- [`r1-assignment-unit`](studies/r1-assignment-unit) cobre place e RHS uma vez,
  falha preservando o value anterior, Unit, move-only e compound assignment.
- [`r1-power-precedence`](studies/r1-power-precedence) cobre precedence,
  right-association, prefix exponent, XOR e a fronteira de unit grammar.
- [`r1-fluent-self`](studies/r1-fluent-self) compara fallthrough `: self` com
  `return self` e registra os negativos Unit e `take fn`.

Todos os bundles permanecem `design-oracle-input`. Parse Tree-sitter e host
oracle são evidência corrente. Compile, run e estudos humano/model permanecem
missing.

### R1H0 — ergonomia de tempo e memória

Os quatro bundles independentes preservam os invariantes do design e não mudam
`DESIGN.md`:

- [`r1-suspend-accounting-names`](studies/r1-suspend-accounting-names) compara
  `HostSuspendPolicy` com seleção ativa/default e rejeita Boolean e suspensão de
  `await`, task ou coroutine como fato de HOST/SO.
- [`r1-weak-owner-acquisition`](studies/r1-weak-owner-acquisition) compara a
  leitura contextual `weak T?` com as formas `upgrade()`, property `strong` e
  method `strong()` aposentadas para owners live e expired.
- [`r1-arena-scope`](studies/r1-arena-scope) preserva o estudo histórico de `Arena.fixed`, uma região
  lexical reservada e scope por closure. A região usa `reserved-not-parsed`.
- [`r1-allocator-runtime-slot`](studies/r1-allocator-runtime-slot) compara o
  control argument `allocator: memory` da construction expression com um
  envelope genérico rejeitado, além de colisão, ordem e ausência de allocation
  site. `using:` continua um label local livre.

Cada bundle usa quatro tasks em ordem `explain`, `recall`, `repair`, `change`,
ordens counterbalanced, blinding, sourceBase e digests. No checkpoint R1H0,
havia 73 variantes totais, 72 variantes `.w` parseadas pelo Tree-sitter e uma
proposta textual `.txt` `reserved-not-parsed`; o witness region não fazia parte
do parse 72/72. Parse Tree-sitter das variantes `.w` e host oracle são evidência
corrente. `w-compile`, `w-run`, `human-study` e `model-study` permanecem missing.

### R1S1 — estrutura de source e formatter

R1S1 adiciona oito bundles em sete famílias e promove 21 casos R0.
Os bundles usam symbols reais de `reference/last-light`.
Eles preservam o Restaurante no Fim do Universo como contexto adversarial:

- [`r1-source-boundaries`](studies/r1-source-boundaries) fixa newline, semicolon,
  discard e formatter.
- [`r1-static-contract-syntax`](studies/r1-static-contract-syntax) fixa envelopes
  attached, close nested e contrato local.
- [`r1-data-declaration-surface`](studies/r1-data-declaration-surface) separa
  struct transparente e object encapsulado.
- [`r1-manifest-surface`](studies/r1-manifest-surface) compara o manifest
  data-only com o witness de package inline.
- [`r1-pattern-surface`](studies/r1-pattern-surface) cobre patterns nominais,
  tuple scrutinee, cases fechados e rest externo.
- [`r1-callable-property-surface`](studies/r1-callable-property-surface) cobre
  property segura, slot de linguagem e closure arrow.
- [`r1-source-phase-surface`](studies/r1-source-phase-surface) fixa imports antes
  das declarations e body implementado.
- [`r1-delimited-value-surface`](studies/r1-delimited-value-surface) fixa matrix
  nested e tuple singleton.

As variantes `selected` usam as formas correntes de R0. As alternativas vêm
somente do registro R0. Witnesses textuais usam caminho não-`.w` e
`reserved-not-parsed` quando a grammar não aceita a forma. As variantes `.w`
parseiam sem recovery. Cada bundle fixa primary, adversarial, quatro tasks,
ordens contrabalançadas, blinding, digests e host oracle independente.

O checker deriva 37 bundles, 95 variantes, 148 tasks e 66/74 casos R0
promovidos. O conjunto contém 85 variantes `.w` parseadas e dez witnesses
reservados fora do parse. `sourceRefs` sustentam constructs adicionais de fontes
reais sem criar uma segunda autoridade. Cada oracle compara todos os inputs com
`expected` após derivação independente. Parse Tree-sitter e host oracle são
evidência corrente. `w-compile`, `w-run`, `human-study` e `model-study`
permanecem missing.

### Workflow single-file PYN1

[`script-workflow-machine.mjs`](script-workflow-machine.mjs) é uma máquina host
determinística para a direção PYN1. Ela deriva context, roots, imports,
virtual selection, payload P0 `package.lock` (`contexts`/`packages`), closure
transitiva, selected target context, fetch pinned com candidate real, cache CAS
offline por content digest, parser evidence ligada a bytes, artifact/handle/
action-output records ligados ao lock e recipe (somente outputs consumidos entram
na recipe), offered/matched/effective
requirements, identity efêmera sem path físico, entry, cleanup e promotion. Ela
não compila, consulta um registry, executa W ou fornece um CLI.

O corpus e o snapshot ficam em
[`script-workflow-cases.json`](script-workflow-cases.json) e
[`script-workflow-results.snapshot.jsonl`](script-workflow-results.snapshot.jsonl).
O checker exige casos positivos e negativos e liga cada caso ao produto Última
Luz:

```sh
bun test tooling/script-workflow-reference.test.mjs
bun tooling/check-script-workflow-cases.mjs
```

Os nomes `w script add`, `w script remove`, `w script resolve` e
`w script promote` são contratos de design. Este tooling não implementa esses
comandos.

### Sessão/REPL transacional PYN2

[`repl-session-machine.mjs`](repl-session-machine.mjs) é uma máquina host
determinística para a sessão efêmera de `w repl`. Ela deriva `SessionId`,
`SessionIncarnation`, `ExecutionOrdinal`, `GenerationId` opaca começando em g0,
parser/checker facts, snapshots committed, receipts, phases transacionais
(incluindo preflight antes de effects), graph invalidation por BindingId/version,
cross-generation ownership, Copy staging, provider outcomes, drain
preflight/degraded, structured lifetime, output reserve/truncation, FIFO writer,
active/queued cancellation e bounded history. A máquina não compila, executa,
resolve dependency, acessa network ou implementa resource drain.

O corpus, checker, snapshot e teste host ficam em
[`repl-session-cases.json`](repl-session-cases.json),
[`check-repl-session-cases.mjs`](check-repl-session-cases.mjs),
[`repl-session-results.snapshot.jsonl`](repl-session-results.snapshot.jsonl) e
[`repl-session-reference.test.mjs`](repl-session-reference.test.mjs). Use:

```sh
bun test tooling/repl-session-reference.test.mjs
bun tooling/check-repl-session-cases.mjs
```

O corpus atual possui 70 casos e 298 operações (56 programas aceitos e 14
rejeitados), com casos negativos separados para cada operação de ownership,
stale identity, parser/semantic, quota, cancellation, close/reset e drain.

O fixture parseável é
[`reference/last-light/repl_session_oracle.w`](../reference/last-light/repl_session_oracle.w).
Ele é um oracle de design. PYN3/Jupyter/rich output agora estão materializados
como bundles separados; DLPack permanece fora do escopo.

### Apresentação, Jupyter e export PYN3

PYN3 possui três oracles host independentes. Eles usam facts e receipts
serializados. Nenhum deles compila ou executa W.

[`presentation-machine.mjs`](presentation-machine.mjs) valida media e payload
typed, `text/plain`, uniqueness, effect mask, limits, fallback e cancellation.
Também prova que a prévia tabular não coleta stream e que o resumo tensorial não
faz device copy. O corpus, checker, snapshot e teste host ficam em
[`presentation-cases.json`](presentation-cases.json),
[`check-presentation-cases.mjs`](check-presentation-cases.mjs),
[`presentation-results.snapshot.jsonl`](presentation-results.snapshot.jsonl) e
[`presentation-reference.test.mjs`](presentation-reference.test.mjs).

[`jupyter-machine.mjs`](jupyter-machine.mjs) valida kernelspec 5.5 determinístico,
os cinco ports loopback e connection security, HMAC-before-use, Curve Z85,
heartbeat echo, replay e quotas. Ele deriva FIFO PYN2, lifecycle
busy/reply/outputs/idle, `ExecutionOrdinal` separado de `GenerationId`, silent,
expressions, stdin/password, interrupt/shutdown, read-only requests e metadata
namespaced `w`. O corpus, checker, snapshot e
teste host ficam em [`jupyter-cases.json`](jupyter-cases.json),
[`check-jupyter-cases.mjs`](check-jupyter-cases.mjs),
[`jupyter-results.snapshot.jsonl`](jupyter-results.snapshot.jsonl) e
[`jupyter-reference.test.mjs`](jupyter-reference.test.mjs).

[`notebook-export-machine.mjs`](notebook-export-machine.mjs) valida nbformat
cell IDs, source String/Array, bounds, receipt manifest estruturado,
source/generation/binding/lock/effect proof, invalidation e redefinition
blockers, ordem determinística e resultados single-file PYN1, package e audit.
Markdown é companion; raw segue policy. Export não executa cell nem faz hidden
replay. O corpus, checker, snapshot e teste host ficam em
[`notebook-export-cases.json`](notebook-export-cases.json),
[`check-notebook-export-cases.mjs`](check-notebook-export-cases.mjs),
[`notebook-export-results.snapshot.jsonl`](notebook-export-results.snapshot.jsonl)
e [`notebook-export-reference.test.mjs`](notebook-export-reference.test.mjs).

Use:

```sh
bun test tooling/presentation-reference.test.mjs tooling/jupyter-reference.test.mjs tooling/notebook-export-reference.test.mjs
bun tooling/check-presentation-cases.mjs
bun tooling/check-jupyter-cases.mjs
bun tooling/check-notebook-export-cases.mjs
```

O design fixa `w notebook check`, `w notebook export` e `:receipts`. Este
tooling valida somente os contratos; ele não fornece CLI, ZeroMQ, kernel
process, sanitizer, frontend, provider ou runtime.

### Carrier tensorial e DLPack PYN4

[`dlpack-machine.mjs`](dlpack-machine.mjs) é uma máquina host determinística
para o carrier tensorial. Ela valida DLPack 1.3 versioned, flags conhecidas,
dtype/layout, shape/stride, alignment, overflow, provenance, Device/Queue
provider-scoped, provider/profile/target resolution events, receipts derivados
de `bindQueue`/`producerWait`, open zero-copy, dynamic bind, materialização,
export consuming, capsule one-shot,
release exact-once por generation, Python GIL/interpreter lease, drain de
leases/jobs, cancellation, close/quarantine e receipt redaction. Ela rejeita
raw stream, raw pointer, untrusted bytes, hidden copy e callbacks fora do scope.

O corpus, checker, snapshot e teste host ficam em
[`dlpack-cases.json`](dlpack-cases.json),
[`check-dlpack-cases.mjs`](check-dlpack-cases.mjs),
[`dlpack-results.snapshot.jsonl`](dlpack-results.snapshot.jsonl) e
[`dlpack-reference.test.mjs`](dlpack-reference.test.mjs). Use:

```sh
bun test tooling/dlpack-reference.test.mjs
bun tooling/check-dlpack-cases.mjs
```

O fixture é [`reference/last-light/tensor_interop.w`](../reference/last-light/tensor_interop.w).
Ele não executa W e não fornece provider DLPack, Python, CUDA, ROCm ou C
Exchange. `std.tensor@1` e `std.dlpack@1` permanecem missing.

### Device execution DEV0

[`device-execution-machine.mjs`](device-execution-machine.mjs) deriva o scope
`Launch` a partir de module/artifact/instances já fechados por KM0. Ele deriva
staging de ownership, submit/completion receipts, dependencies de Queue,
cancellation, device loss, generations, budgets e equivalência CPU/device. O
corpus, checker, snapshot e teste host ficam em
[`device-execution-cases.json`](device-execution-cases.json),
[`check-device-execution-cases.mjs`](check-device-execution-cases.mjs),
[`device-execution-results.snapshot.jsonl`](device-execution-results.snapshot.jsonl)
e [`device-execution-reference.test.mjs`](device-execution-reference.test.mjs).

Use:

```sh
bun run check:device-execution
```

O fixture é
[`reference/last-light/device_execution_oracle.w`](../reference/last-light/device_execution_oracle.w).
O oracle não executa W, kernel, driver ou provider. `std.accelerator@1`
permanece missing.

### Service recovery SR0

[`service-recovery-machine.mjs`](service-recovery-machine.mjs) deriva mailbox,
input commit, effect policy, output frontier, deduplication, journal prefix,
instance generation, disconnect, compaction, restart window e shutdown. Corpus,
checker, snapshot e testes independentes ficam em
[`service-recovery-cases.json`](service-recovery-cases.json),
[`check-service-recovery-cases.mjs`](check-service-recovery-cases.mjs),
[`service-recovery-results.snapshot.jsonl`](service-recovery-results.snapshot.jsonl)
e
[`service-recovery-reference.test.mjs`](service-recovery-reference.test.mjs).

Use:

```sh
bun run check:service-recovery
```

O fixture é
[`reference/last-light/service_recovery_oracle.w`](../reference/last-light/service_recovery_oracle.w).
SR0 compõe B0 e E1. Ele não executa W, wWire, SQLite, filesystem, network,
runtime ou provider.

TextMate é a integração nativa e mais curta para obter cores no VS Code. A
gramática Tree-sitter é a única candidata a descrever estrutura entre esses
artefatos; TextMate e o scanner temporário do portal são projeções lexicais, não
gramáticas concorrentes.

### O que permanece no projeto

| Artefato | Política de manutenção |
|---|---|
| `tree-sitter-w/grammar.js`, corpus e `queries/*.scm` | fonte estrutural mantida; serve parsing incremental, highlights, locals e folds |
| `tree-sitter-w/src/` | saída gerada e versionada para consumir o parser C sem exigir o CLI no usuário final; nunca editar à mão |
| `vscode-w/syntaxes/*.json` | fallback TextMate pequeno mantido porque é a tokenização lexical nativa do VS Code |
| `vscode-w/icons/w.png` e language configuration | integração declarativa mantida |
| `portal/w-syntax.js` | fallback temporário; remover quando Tree-sitter/WASM local passar os mesmos testes no browser |
| semantic tokens futuros | saem de `wls`/HIR e refinam TextMate; não saem apenas da CST |

Tree-sitter pode ser a entrada do compilador bootstrap, mas não é o tradutor para
o target. Seu limite é produzir uma CST recuperável. O adapter W transforma CST
em AST/HIR, resolve nomes/tipos/ownership/effects e só então baixa para o dialeto
W/MLIR e LLVM. Queries Tree-sitter são excelentes para seleção estrutural de
tooling; usá-las como substituição textual de código esconderia validação
semântica e source locations justamente onde W promete previsibilidade.

Fixtures devem convergir para o corpus Tree-sitter até o frontend normativo
existir. Toda construção nova entra primeiro em `DESIGN.md`, depois em um caso
positivo e, quando estrutural, num negativo correspondente. Só então atualiza
queries, TextMate e portal. Comparar cores pixel a pixel não é um oracle;
comparar tokens essenciais, nós e ausência de divergência silenciosa é.

## Papel recomendado do Tree-sitter

Tree-sitter é candidato a componente **permanente do tooling**: edição
incremental, highlighting estrutural, folds, navegação local e parser WASM do
portal. Isso não o transforma automaticamente na definição normativa de W.
Rust e Go, por exemplo, possuem grammars no projeto Tree-sitter, mas seus
compiladores e language servers mantêm parsers próprios.

Para evitar duas gramáticas durante o bootstrap, o primeiro `wc parse` pode
consumir a CST gerada aqui por uma interface estreita. Nesse uso, o modo do
compilador é estrito: rejeita `ERROR`/`MISSING`, executa validações contextuais e
converte a CST para uma AST/HIR que não expõe tipos de nós acidentais do
Tree-sitter. A validade continua definida por especificação, precedência,
corpus positivo/negativo e diagnósticos esperados — nunca pelo que a recuperação
tolerante conseguiu representar.

Um parser próprio só entra quando medições mostrarem ganho material em
diagnóstico, macros, parsing contextual, desempenho ou distribuição. Se isso
acontecer, ambos rodam o mesmo corpus e testes diferenciais/fuzz impedem drift.
Assim a escolha inicial continua reversível sem jogar fora grammar, queries ou
integrações de editor.

## Começar agora

1. Para usar W localmente no VS Code, siga
   [tooling/vscode-w/README.md](vscode-w/README.md). O caminho mais rápido é abrir
   essa pasta e pressionar `F5`.
2. Para desenvolver o parser incremental, use os comandos documentados em
   [tooling/tree-sitter-w/README.md](tree-sitter-w/README.md).
3. Para ver a superfície no browser, rode o [portal](../portal/README.md). O
   playground identifica explicitamente qual engine de highlight está ativa.
4. Para auditar a cobertura local de exemplos, execute
   `bun tooling/check-design-examples.mjs` na pasta `W`.
5. Para atualizar o índice, execute
   `bun tooling/design-index.mjs --write` na pasta `W`.
6. Para ler somente um recorte do design, execute
   `bun tooling/design-slice.mjs --heading 12.13`,
   `--rationale-heading 1.1` ou `--id W-711`.
7. Para validar documentação, links e índice, execute `bun run check:docs` no
   root do repositório.
8. Para validar o recorte E1 sem executar runtime, execute `bun run check:liveness`
   no root do repositório.
9. Para validar o recorte LM1 sem executar runtime, execute `bun run check:locks`
   no root do repositório.
10. Para validar o recorte SP0 sem executar runtime, execute
   `bun run check:snapshot-cell` no root do repositório.
11. Para validar o recorte LZ0 sem executar compiler ou runtime, execute
   `bun run check:lazy` no root do repositório.
12. Para validar a composição MX0 sem executar compiler ou runtime, execute
   `bun run check:ownership-execution` no root do repositório.
13. Para validar o channel CH0 sem executar compiler ou runtime, execute
   `bun run check:channel` no root do repositório.
14. Para validar o recorte TAB1 sem executar W, execute
   `bun tooling/check-tabular-adapter-cases.mjs --write` e
   `bun test tooling/tabular-adapter-reference.test.mjs`.
15. Para validar BRX0 sem compiler ou runtime, execute
   `bun run check:borrow-expressivity` no root. O checker mantém o blocker
   bodyless multi-input como Research e exige parse, oracle host e artefatos de
   mapping consistentes.
16. Para validar CAP0 sem compiler ou runtime, execute
   `bun run check:capability-matrix` no root. O checker deriva oito rotas por
   subcapacidade, valida 149 refs e 15 subcapabilities, e mantém a fila
   editorial de oito docs.

### CAP0 — matriz de capacidades por problema

[`capability-matrix-cases.json`](capability-matrix-cases.json) é a fonte
editorial de staging para os oito eixos CAP0. O checker deriva a rota a partir
das subcapacidades marcadas como problema e escreve
[`capability-matrix-results.snapshot.jsonl`](capability-matrix-results.snapshot.jsonl).
Cada eixo mantém um cenário Last Light, fontes primárias C/Rust/Python, riscos
de composição, subcapacidades mistas e um alvo estável para documentação futura.
Os exemplos estrangeiros são pseudocódigo original curto (não citações); o
exemplo W é somente um `source-ref` para Last Light. `renderHint: paired`
preserva o formato lado a lado para os guias futuros, sem publicar o Book agora.
Research subcapabilities apontam para gates `kind: design`; gates `kind: evidence`
guardam apenas provider/execution evidence.
O snapshot registra `routeCounts` (5 composable, 1 current, 2 research),
`canonicalSourceCount` (8) e `documentationQueuedCount` (8).

Use os gates locais:

```sh
bun run check:capability-matrix
bun tooling/check-capability-matrix.mjs --write
```

O script `check:capability-matrix` do pacote Tree-sitter entra na cadeia
`check:docs`; a execução ampla desse gate continua separada da validação local.

## Caminho até o browser

1. estabilizar grammar, corpus e queries no host;
2. gerar `tree-sitter-w.wasm` de forma reproduzível e registrar toolchain/digest;
3. servir parser e runtime como assets locais com MIME/CSP testados, sem CDN;
4. trocar o scanner do portal por um adapter incremental, mantendo fallback e
   erro visível quando WASM não carregar;
5. só depois avaliar editor completo e semantic tokens produzidos por HIR/LSP.

O WASM não entra no repositório apenas porque foi possível gerá-lo numa máquina.
Ele precisa de receita reproduzível, teste no browser e regra de atualização.

## Referências de integração

- [VS Code — Syntax Highlight Guide](https://code.visualstudio.com/api/language-extensions/syntax-highlight-guide)
- [VS Code — Language Configuration Guide](https://code.visualstudio.com/api/language-extensions/language-configuration-guide)
- [VS Code — Semantic Highlight Guide](https://code.visualstudio.com/api/language-extensions/semantic-highlight-guide)
- [Tree-sitter — Creating Parsers](https://tree-sitter.github.io/tree-sitter/creating-parsers/1-getting-started.html)
- [Tree-sitter — Syntax Highlighting](https://tree-sitter.github.io/tree-sitter/3-syntax-highlighting.html)
- [Tree-sitter — binding Web/WASM](https://github.com/tree-sitter/tree-sitter/tree/master/lib/binding_web)
- [rustc — lexing e parsing](https://rustc-dev-guide.rust-lang.org/the-parser.html)
- [rust-analyzer — arquitetura da sintaxe](https://rust-analyzer.github.io/book/contributing/architecture.html)
- [Go — `go/parser`](https://pkg.go.dev/go/parser)
