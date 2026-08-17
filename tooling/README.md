# Tooling inicial de W

> **Status:** Working Draft. Highlighting e o parser incremental experimental
> são utilizáveis; parser do compilador, formatter, LSP e compilador ainda não
> existem.

Este diretório antecipa a experiência de escrever W sem transformar cores em
semântica. A autoridade normativa continua em [DESIGN.md](../DESIGN.md).
[RATIONALE.md](../RATIONALE.md) fornece evidência e ledger sem definir
comportamento; nenhum highlighter aceita ou rejeita um programa em nome da
linguagem.

## Registro do design freeze

[`design-freeze-classification.json`](design-freeze-classification.json) possui
uma entrada explícita para cada ID do ledger. O checker exige a união exata do
ledger e rejeita IDs ausentes, novos, duplicados ou com digest stale. Cada
registro carrega `selection: explicit-ledger-id`, `basisRef` para a linha do
ledger e digest atual; o `archiveGapDistribution` registra por que os 916 IDs
que faltavam no gate anterior não foram classificados por range ou default.

As categorias têm estados diferentes:

- `source-backed-current` e `oracle-backed-current` ligam evidência verificável.
- `research-gated` exige uma condição de parada para pesquisa aberta.
- `implementation-evidence-gap` exige uma condição de parada para evidência ausente.
- `superseded` aponta para a decisão corrente que substitui a proveniência.
- `rejected` aponta para a ausência corrente sem promover a forma recusada.

O estado atual é `91 source-backed-current`, `481 oracle-backed-current`, `0
research-gated`, `813 implementation-evidence-gap`, `57 superseded` e `8
rejected` (1450/1450). A lacuna histórica de 916 fica distribuída em 68
oracle, 0 Research, 806 gaps de implementação, 35 superseded e 7 rejected;
nenhuma decisão é selecionada por faixa, época, regex ou default em massa.

Gaps de implementação usam `authorityRef.kind: design-contract` com a seção e
heading exatos de `DESIGN.md`, digest do arquivo e digest do slice. O `gap.gate`
deve ser igual a essa autoridade; §24.4 só é aceito para um gap real do gate de
freeze. Cada gap nomeia componente e testemunho ausente sem fallback por ID.
Entradas de pesquisa nomeiam gate, caso independente, digest novo e decisão de
promoção. Quando uma decisão corrente mantém uma extensão Research, ela usa
`researchExtension` sem rebaixar o baseline. `superseded` carrega
`supersessionClaim` com claim/digest do sucessor e relação semântica explícita.
Refs de fonte/oráculo exigem `caseId`, path e digest; quando o caso é
file-backed, também exigem símbolo. O baseline BRX0 pode usar `decisionBridge`
para ligar seu fechamento ao ID que o caso realmente cita.

O checker mantém sentinelas fixas para W-1381–W-1383 (BRX3 oracle current),
W-1384 (BRX3 implementation-evidence-gap), W-1418 (protocolo em §8.2),
W-1436 (BRX0 current + ponte BRX3), W-281 (sucessor W-1290) e uma amostra de
cada família componente/seção.

Casos host e snapshots descrevem design-oracle evidence. Eles não afirmam
compiler, runtime, provider ou execução W.

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
| `frontend-freeze-cases.json` + `check-frontend-freeze.mjs` + guards/test + snapshot | FZ0 ratifica as seis famílias normalizadas G0–G5 com source Last Light real (digest/symbol), parse sem recovery, pares F0 CST-equivalentes com alvo byte canônico, inversão S0 ou waiver RU0 e D0 exato; 19 decisões são cobertas sem duplicatas ou expected echo | oracle de design; F0 não prova idempotência nem implementa formatter, e o checker não implementa parser, compiler, runtime ou provider |
| `borrow-expressivity-cases.json` + máquina/checker/test + snapshot | BRX0 deriva 24 casos (17 mappings aceitos, cinco candidatos Research e quatro negativos de invocation) para receiver/body mapping, origem bodyless única, callable loans, lending cursor, adapter OriginSet, await, escapes, `any fn`, boundaries, alternativa nominal owned e drift/mutations de interface | oracle host de design; bodyless com duas ou mais entradas compatíveis rejeita com `W-BORROW-0011`; não implementa compiler, runtime, provider nem metadata de lifetime |
| `brx2-borrow-relations-cases.json` + estudo/máquina/checker/test + snapshot | BRX2 informa BRX2-R1 com 56 casos sobre relação requirement/interface-owned após a rejeição baseline de ambiguidade, slots de resultado, modos/edges, OriginSet, SemanticInterfaceKey, WAbi proof, lock/provider receipts, callable/Stream/await/boundary e aggregate nominal | Research data-only; W-914 e a origem única permanecem vigentes, source spelling fica reserved/not-parsed, declaration/invocation status são separados e não há lifetime syntax, runtime metadata, compiler, runtime ou provider |
| `studies/atom2-atomic-contract/study.json` + `atom2-atomic-contract-cases.json` + máquina/checker/test + snapshot | ATOM2 fecha ATOM0-G1 com 47 casos e dois reducers: promove carrier canônico compiler-synthesized para records fechados value-only (Bool 0/1, signed two's complement, unsigned width exato, enum ordinal, declaration order LSB-first, high bits zero, 1–128 bits), mantém handle `{slot,generation}` com owner table e geração checked sem wrap, exige fallback allocation-free com `blocksThread` separado de `parking` (`parking:true` exige `blocksThread:true`), matriz exata de failure orders e operações neverSuspend/non-cancellation, permite apenas adapter `unsafe` de reclamation com lifecycle/FFI drain explícito e rejeita raw/tagged pointer e RCU universal | oracle host de design; compiler/runtime/provider, target probes, fallback realization, FFI drain execution e estudos humano/modelo continuam implementation-evidence gaps; ATOM1 fica histórico |
| `asic0-evidence-gap-closure-cases.json` + máquina/checker/test + snapshot + `studies/asic0-evidence-gap-closure` | ASIC0 é o bundle reuse-only que fecha os cinco gates originais IPC1/AVF0/SEC0 (10 casos primários, current/adversarial): W-1355/W-1359 tornam A immutable mapped snapshot e B bounded mapped byte channel/log contratos condicionais de adapter/provider; W-1420/W-1425 tornam facts/binding typed; W-1435 torna profile, side-channel, patch e deployment receipts contratos de evidence/admission; mapeia W-1448/W-1449/W-1450 e preserva fallback, `unknownDurability`, C universal rejeitado e ausência de security conformance | oracle host design-only com payloads IPC1/AVF0/SEC0 referenciados; Windows, compiler, runtime, provider, hardware, sandbox, attestation, FFI, fault, stress e estudos humano/modelo permanecem implementation-evidence gaps |
| `studies/ipc1-mapped-ipc/study.json` + `ipc1-mapped-ipc-cases.json` + reducers/checker/test + snapshot | IPC1 informa/estreita IPC0-R1 com 69 casos e 138 projeções POSIX/Windows para snapshots file-backed duráveis em generation objects, carriers shm/pagefile voláteis, schema/layout digests, selector publication/receipt ordenada, cap0/capN e bounds no segmento `slots`, commit/cancelamento, checksum/materialization/OOM, crash por actor e recovery ordering, atomics, lifecycle terminal, FFI, provider bindings e fallback explícito; o probe POSIX observado é digest-backed por source/transcript, enquanto Windows, W compile/run, provider, crash/durability e estudos humano/modelo continuam missing | oracle host de design; `ipc1-mapped-ipc-reference.test.mjs` e o study oracle são host evidence; wake provider é explícito, `WaitOnAddress` é same-process, ATOM2 não implica address-free/process-shared; os labels Research de A/B mapped são históricos pré-ASIC0, que fecha os contratos condicionais de adapter/provider; C universal é rejeitado, sem API, syntax, compiler, runtime ou provider |
| `capability-matrix-cases.json` + máquina/checker/test + snapshot | CAP0 consolida oito eixos e 16 subcapacidades por problema comum, tenta composição W com Last Light, preserva invariantes e deriva rotas `current`/`composable`/`research`; 149 refs primárias ou source-backed e oito filas de documentação futura | fonte editorial de staging; não mede maturidade, não copia features e não implementa compiler, runtime ou provider |
| `cyc1-explicit-cycle-cases.json` + máquina/manifest/checker/test + estudo/snapshot | CYC1 informa CYC0-G1 com 41 casos event-derived para weak edges, close/drain, SCC estática/dinâmica, FFI/service/resource lifecycle, concorrência, unknown foreign boundaries e três composições de conditional liveness; 3 rejections estáticas, 3 diagnostics residuais, 2 unknown boundaries e 2 cases Research | oracle host e Tree-sitter parse; census é somente diagnóstico pós-drain, sem collector/finalizer/API/syntax, e compile, run, provider, stress e estudos humano/modelo continuam missing |
| `syn1-typed-generation-cases.json` + máquina/manifest/checker/test + snapshot | SYN1 estreita SYN0-R1 com 65 casos A/B/C/D: generated module sets de `.w` passam pelo Tree-sitter real e por source-shape bounded; action result e interface candidata são publicações separadas; receipts Research cobrem graph/dependencies, identities, maps byte-based, target registry e navigation | oracle host de design; `interfacePublished` é outcome do contrato candidato, não evidência de compiler; semantic frontend, ConstIR, compiler cache, runtime, provider e LSP permanecem ausentes |
| `dyn1-versioned-behavior-cases.json` + máquina/manifest/checker/test + snapshot | DYN1 informa DYN0-G1 com 70 casos A/B/C/D e métricas derivadas para REPL snapshots, generations de service/plugin, identities SemanticInterface/WAbi/runtime-closure, switch/drain, capabilities/effects, export/import, target local/split, FFI unload, crash/cancel e quotas; C é somente a subcapability `DYN0-persistent-generation-reference` | host design-oracle event-derived; reducers local/split são independentes, `expect` não escolhe status, WAbi target-specific e compatible exige novas SemanticInterfaceKey/ServiceIRKey com receipt; native retém mapping e process/Wasm/component usam full unmap; compiler/runtime/provider/isolamento real/std permanecem missing; eval/exec/frame mutation/ambient lookup/native sandbox/live dlclose são rejeitados |
| `hot-reload-dev-cases.json` + `hot-reload-dev-machine.mjs` + `studies/hrd0-hot-reload-dev/{bundle.json,oracle.test.mjs}` + checker/test + snapshot | HRD0 fecha o problema de hot reload somente para desenvolvimento: runner tooling-owned recompila/reabre units normais, compõe REPL snapshots e typed generations, mantém old até drain, rejeita stale events/migração/produção e mantém generated module set e CLI spelling em Research | host design-oracle independente; 20 casos, 5 mutations adversariais (cleanup, nominal contract e interface digest), dois reducers local/split, 13 diagnostics e um contrato Last Light comum usado pelos dois witnesses; não cria syntax/profile, não alega compiler/runtime/provider/isolamento real e reutiliza DYN1/SYN1/CAP0 |
| `gen1-incremental-suspension-cases.json` + máquina/checker/test + snapshot | GEN1 é evidência histórica de GEN0-R1 com 23 traces de pull, travessia, diálogo, failure, delegação, view, backpressure, cancelamento, children e FFI; compara Stream/state/channels e dois witnesses reservados em duas máquinas independentes e deriva métricas estruturais de símbolos source únicos por slices do mesmo cenário | oracle host de design histórico; não executa W, compiler, runtime, provider ou estudo humano/modelo; não mantém gate Research corrente |
| `gen2-stream-yield-cases.json` + `studies/gen2-stream-yield/{bundle.json,oracle.test.mjs}` + máquina/checker/test + snapshot | GEN2 fecha o design estreito de `stream <[capture_item, ...]> { ... yield (take|copy) value }` com 20 casos, 5 ganhos ergonômicos, 13 gates contratuais, dois reducers, captures construction-time, pull capacity zero, Channel para diálogo e rejeição de frame/send/throw/resume público; `copy` exige `Duplicable` | oracle host, parser e bundle são evidência de design; semantic compiler, W runtime, provider, stress, debug/ABI e estudos humano/modelo continuam missing; W-1439/W-1440 são implementation-evidence-gap |
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
| `design-freeze-classification.json` + `check-design-freeze-audit.mjs` | registro versionado e explícito dos 1450 IDs, com uma categoria fechada, claim do ledger, authority ref, digests, casos de fonte/oráculo e stop condition; preserva a cobertura legada 170 source, 408 oracle, 8 explícitas e 52 overlaps; PRC0, ASIC0 e FRC0 promovem gates oracle-backed-current, W-1354 é superseded por W-1437 e W-1448–W-1450 permanecem implementation-evidence-gap | auditoria de design; fonte/oráculo host não são compiler, runtime ou provider |
| `final-research-closure-cases.json` + máquina/checker/test + snapshot + `studies/final-research-closure` | FRC0 fecha W-707, W-731 e W-1408 com seis casos current/adversarial, manifest estrito, bundle R1 reuse-only e três disposições `oracle-backed-current`; W-707 é completude FZ0, W-731 é disposition/Research=0 e W-1408 é stop/no-auto com 0 human/0 model | oracle host design-only; não alega implementation, compiler, runtime, provider ou resultados humano/modelo |
| `substitution-surface.snapshot.json` + runner | baseline determinística de bytes, code points, linhas e lexemes para as 190 formas R0 derivadas pelo script | não mede compreensão, correção nem tokens de um modelo |
| `studies/*/bundle.json` + checker | 55 bundles R1, 156 variantes e 220 tarefas; base R1 51/148/204/69/75, agregados R1C0 52/150/208/69/75, PRC0 reuse-only 53/152/212, ASIC0 reuse-only 54/154/216 e FRC0 reuse-only 55/156/220 | parse e oracle host não equivalem a compilar ou executar W |
| `wlo1-closure-cases.json` + `wlo1-closure-machine.mjs` + `check-wlo1-closure.mjs` + snapshot | WLO1 fecha o perfil `wlo.string.v1` com CBOR determinístico RFC 8949, 14 casos (3 accepted, 11 typed negatives) e uma paridade de target; receipts de schema/versão/limites ficam fora do payload | oracle host de codec; não é W ABI e não alega compiler, runtime, provider, OOM, target, package ou estudo humano/modelo |
| `r1c0-closure-cases.json` + `check-r1c0-closure.mjs` + `studies/r1c0-closure` | R1C0 fecha 21 gates por metadados reuse-only, 52 bundles pinados, 150 variantes e 208 tarefas; W-092 usa WLO1, W-207 é rejected e W-1441 preserva o gap do provider | oracle host de design; não alega implementação, preferência humana ou resultado de modelo |
| `prc0-provider-runtime-closure-cases.json` + máquina/checker/snapshot + `studies/prc0-provider-runtime-closure` | PRC0 fecha sete gates Research com 14 casos (sete current e sete adversarial), 53 bundles, 152 variantes e 212 tarefas; reusa SR0, RU0, PYN3, PYN4, LZ0, ASC0 e R1 units sem copiar payloads, mantém W-1442–W-1447 como implementation-evidence-gap e reutiliza W-1333 no ASC0 | oracle host design-only; não prova compiler, runtime, provider, bridge, W compile/run ou estudos humano/modelo |
| `tabular-carrier-cases.json` + máquina/checker/snapshot | TAB0 fecha publication, schema identity, columns, chunks, copy/device, trust, owner/release e limits com casos positivos e negativos | oracle host independente; não compila W, não executa runtime e não implementa provider ou format adapter |
| `tabular-carrier-reference.test.mjs` | testes host independentes para o carrier tabular e a fronteira explícita de evidência | teste não prova compiler, runtime, CSV, Parquet, Arrow ou DataFrame de produção |
| `tabular-adapter-cases.json` + máquina/checker/snapshot | TAB1 deriva source kind, u64 snapshot offsets/short reads, nominal schema identity, publication, CSV tokenizer/nulls, Parquet footer/page/mapping/key/commit, Arrow IPC dictionary/buffer, borrowed view, copy materialization, progress/cancel, provenance, C quota/trust/release; 86 casos e 193 operações (36 aceitos + 50 rejeitados) | oracle host independente; símbolos Last Light são cross-linked; não implementa reader CSV/Parquet/Arrow, compiler, runtime ou provider |
| `tabular-adapter-reference.test.mjs` | teste host independente para cada caso TAB1 e digest de estado | não executa W, codec binário, C bridge ou device transfer |
| `wire-reference.test.mjs` | codec host mínimo para os vetores `MenuKey` e falhas estritas | primeiro protótipo; não é o encoder do compiler |
| `wire-diagnostic-cases.json` | par portátil/local para `W-WIRE-0001`, com facts e spans esperados | oracle de design; não é output do checker de interface |
| `wire-reference.c` + `wire-reference-c.test.mjs` | segunda implementação independente dos vetores e erros básicos | gate opcional; exige um GCC compatível |
| `hir-memory-reference.test.mjs` | modelo executável de owner, borrow, suspensão, boundary e ABI | oracle de SH3/SH4; não é o verifier do compiler |
| `memory-transition-cases.json` + máquina M1 | 185 sequências do Última Luz com 606 operações (79 aceitas + 106 rejeitadas), estados e traces byte-exact | oracle host tabelado de PlaceId, dependency/allocation origins, allocator-scope/rehome, erasure, shared/weak/ciclos, pinning, construção direta, FFI e ABI; não é HIR emitida pelo frontend nem allocator/runtime real |
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
| `package-release-cases.json` + máquina P0 | 44 sequências e 379 operações cobrem resolver, resolution record (`ownerDigest`), CAS, recipe, mirror, rebuild e release | oracle host de supply chain; não é resolver, registry, CAS ou signer real |
| `pkg1-project-transaction-cases.json` + máquina/checker/test + estudo/snapshot | PKG1 separa ownerDigest, resolutionDigest e deploymentDigest em um único root físico; 25 casos cobrem refresh, add/remove/update, solve failure, dry-run, stale writer, POSIX/Windows replacement, cleanup, aliases, closure, forged facts, reducer divergence e receipts | oracle host de design; identity split e atomic replace são rotas atuais, durable provider receipts permanecem Research; não implementa compiler, runtime, package manager, provider ou filesystem fault probe |
| `avf0-availability-feature-cases.json` + máquina/manifest/checker/test + estudo/snapshot | AVF0 separa feature de package estática, availability de target/provider e policy runtime tipada; 38 casos, 14 aceitos, 24 rejeitados e sete rejeições de authority amplification | oracle host de design; package feature é current, runtime flag é composição e o label Research do availability binding é histórico pré-ASIC0; ASIC0 fecha o contrato typed/fail-closed e W-1449 mantém a evidência de compiler/provider; não implementa compiler, runtime, provider ou control plane |
| `sec0-security-model-cases.json` + máquina/manifest/checker/test + estudo/snapshot | SEC0 amplia segurança para invariantes safe, capability/effect/API mediation, input/resource/secrets/audit, supply chain, profiles, isolation, side channels, FFI, multi-tenant e patch attestation; 101 casos, 24 aceitos, 77 rejeitados, 11 current e 13 Research como labels históricos pré-ASIC0, seis perfis, 16 rejeições de authority e 4 rejeições de caller echo; receipts fechados, mínimos comuns, target/artifact binding e separação de threat exclusions são adversarialmente testados | oracle host de design; ASIC0 fecha os contratos de evidence/admission de profile, residual, patch e deployment, sem alegar security conformance; W-1450 mantém os gaps de compiler, runtime, provider, sandbox, hardware e attestation verifier |
| `module-run-cases.json` + máquina RU0 | 12 casos/58 operações (3 aceitos + 9 rejeitados) cobrem module-run, entry, roots, resolution, imports, identity e cleanup | oracle corrente; não é CLI, compiler, resolver, provider, runtime ou execução W |
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

### HUM0 — protocolo cross-cutting de revisão humana e de modelos

[`hum0-human-review-protocol.json`](hum0-human-review-protocol.json) define
exatamente oito slices problem-first ancorados em symbols reais do Restaurante:
D0/`w explain`; ownership/borrow/shared/weak; allocator contextual; execution
forms; tasks/channels/backpressure; services/generations; package/build/REPL; e
FFI callback lease. Cada slice fixa input primary e adversarial com o mesmo
problema/outcome e exatamente quatro tasks (`explain`, `recall`, `repair`,
`change`), em ordens contrabalançadas e com blinding.

O `stimulus` de cada input é uma janela bounded derivada de bytes UTF-8 reais por
`sourceRefId`, símbolo único, `beforeLines`, `afterLines`, `maxBytes` e
`derivedStimulusDigest`. A janela começa e termina em limites de linha. O
adversarial reaplica uma única mutation find/replace na mesma janela; mutation e
`expectedRepair` são observer-only. O checker gera os bytes do stimulus e
rejeita digest stale, find ausente/duplicado, janela divergente ou leakage para
o participante.

O protocolo, [`machine`](hum0-human-review-machine.mjs), checker, snapshot e
[`study`](studies/hum0-human-review) são uma camada de revisão, não um bundle R1.
O snapshot deriva somente prontidão estrutural: oito slices, 32 tasks e zero
registros humanos/modelos. Não há score, preferência, ergonomic win ou promoção
automática. Fatos internos (IDs de place/loan/origin, geração real, worker,
thread, endereço, PID, segredo, ponteiro e payload) ficam ocultos; fatos
determinísticos de ownership, diagnostics, allocator, lifecycle, receipts e
estimates podem aparecer somente em `w explain`; drain externo de callback é
uma obrigação do oracle, não uma garantia inventada pelo source de `BellLease`.

O renderer participant-only devolve somente `scenario`, `task`, `instruction`,
`source` e `blindedLabel`. Ele não entrega IDs, paths, digests, mutations,
expected, oracle ou outras rotas internas.

Os contratos futuros separam registro humano (`participantIdHash` sha256,
background não-vazio C/Rust/Python/W, tempo e queries não negativos, confiança
obrigatória 1–5, outcomes exatos semantic/repair/change e
`observerReceiptDigest`) de registro de modelo (provider, model, version,
tokenizer, params JSON fechado, input/observer digests sha256, tokens com soma e
os mesmos outcomes). Nenhum registro existe nesta rodada. A coleta para no
primeiro expected echo, outcome forjado, referência stale/missing, vazamento de
identidade, divergência de problema/outcome, duplicata ou desacordo do oracle;
o caso permanece Research e exige caso independente.

O gate scoped é:

```sh
bun test tooling/hum0-human-review-reference.test.mjs tooling/studies/hum0-human-review/oracle.test.mjs
bun tooling/check-hum0-human-review.mjs
```

`w-compile`, `w-run`, estudos humanos/modelos, providers e qualquer claim de
ergonomia continuam missing.

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

O checker deriva 55 bundles, 156 variantes, 220 tasks e 69/75 casos R0
promovidos (FRC0 acrescenta um bundle reuse-only sem payloads copiados). O conjunto contém 103 variantes `.w` parseadas e 30 witnesses
reservados fora do parse. `sourceRefs` sustentam constructs adicionais de fontes
reais sem criar uma segunda autoridade. Cada oracle compara todos os inputs com
`expected` após derivação independente. Parse Tree-sitter e host oracle são
evidência corrente. `w-compile`, `w-run`, `human-study` e `model-study`
permanecem missing.

### R1C0/WLO1 — fechamento comparativo

WLO1 fecha somente o perfil schema-scoped `wlo.string.v1`: o valor lógico
`Last Light` produz bytes CBOR determinísticos (`6a4c617374204c69676874`) e
mantém receipts externos de schema, versão, target e limites. O corpus tem 14
casos, três positivos, onze negativos tipados e uma paridade portable/native;
árvore, rope e interning continuam especializados e rejeitados como default.

R1C0 liga os 21 gates às decisões correntes por casos de fechamento, sem copiar
payloads dos estudos. Seus números são 52 bundles, 150 variantes e 208 tarefas
no agregado (base R1: 51/148/204/69/75; R1C0: 52/150/208/69/75). O checker e os
oracles permanecem host-only: não afirmam compile, run, provider, OOM, target,
package, humano ou modelo.

Use os gates encadeados:

```sh
bun run check:wlo1
bun run check:r1c0
bun run --cwd tooling/tree-sitter-w parse:r1c0
```

### PRC0 — fechamento de provider e runtime

PRC0 fecha os sete gates de pesquisa W-133, W-903, W-1075, W-1124, W-1147,
W-1196 e W-1328 com 14 casos: um route `current` e um route `adversarial`
para cada gate. O corpus deriva os dois routes a partir dos contratos atuais e
reusa SR0, RU0, PYN3, PYN4, LZ0, ASC0 e R1 units por referência; ele não copia
payloads. `current` é autoridade oracle-backed-current de design. Os gaps de
implementação W-1442–W-1447 permanecem separados, com compiler, runtime,
provider, `w-compile`, `w-run`, `human-study` e `model-study` missing; ASC0
reusa W-1333 para o gap de implementação.

W-903 fixa a projeção canônica: `TemperatureDelta`/`deltaK` é distinto do
point `Temperature`/`degC`, `iec.byte` é a API W e `bit` permanece o token de
referência JSON. O source checker rejeita aliases e spellings não qualificados.

```sh
bun test tooling/studies/prc0-provider-runtime-closure/oracle.test.mjs
bun tooling/check-prc0-provider-runtime-closure.mjs
bun tooling/check-quantity-source-contract.mjs
bun run --cwd tooling/tree-sitter-w parse:prc0
```

### ASIC0 — fechamento reuse-only de IPC1, AVF0 e SEC0

ASIC0 liga os cinco gates originais W-1355, W-1359, W-1420, W-1425 e W-1435
a dez casos primários current/adversarial. A e B de IPC são contratos
condicionais de adapter/provider com receipts; availability facts e binding são
typed e fail-closed; SEC0 fecha apenas evidence/admission receipts. W-1448,
W-1449 e W-1450 permanecem implementation-evidence-gap. O bundle referencia
os cases e machines existentes sem copiar payloads. `unknownDurability`, os
fallbacks, C universal rejeitado e a ausência de security conformance são
outcomes explícitos.

```sh
bun test tooling/studies/asic0-evidence-gap-closure/oracle.test.mjs
bun tooling/check-asic0-evidence-gap-closure.mjs
bun run --cwd tooling/tree-sitter-w parse:asic0
```

### FRC0 — fechamento final de pesquisa

FRC0 fecha somente as três gates de processo que restavam: W-707
(`FZ0-freeze-completeness`), W-731 (`freeze-research-close`) e W-1408
(`HUM0-promotion`). O corpus tem exatamente seis casos, current e
adversarial por gate. A máquina deriva outcomes de facts em cópias dos
corpora FZ0, classificação do ledger e protocolo HUM0. Ela não usa ID,
`expected`, status, score, preference ou métricas do caller e não cria
payload, registro humano/modelo ou evidence de implementação. `Research=0` é
uma propriedade derivada da classificação, não um count manual.

Os artefatos são `design-oracle-input`, `reuseOnly` e host-only. O manifest
fixa roles, digests, containment e a cadeia bundle/study/fixtures/oracle/
snapshot. `w-compile`, `w-run`, compiler, runtime, provider, `human-study` e
`model-study` permanecem missing. O checker root e o checker Tree-sitter
aninhado devem permanecer verdes.

```sh
bun test tooling/studies/final-research-closure/oracle.test.mjs
bun tooling/check-final-research-closure.mjs
bun run --cwd tooling/tree-sitter-w check:frc0
```

### Workflow module-run RU0

[`module-run-machine.mjs`](module-run-machine.mjs) é uma máquina host do
contrato corrente. Ela preserva context, roots package/workspace, imports,
resolution aninhada, identity, entry e cleanup. Ela não compila, não consulta
um registry, não executa W e não fornece CLI. A evidência PYN1 superseded fica
em [`history/archive/pyn1-workflow`](../history/archive/pyn1-workflow).

O corpus e o snapshot ficam em
[`module-run-cases.json`](module-run-cases.json) e
[`module-run-results.snapshot.jsonl`](module-run-results.snapshot.jsonl).
O checker exige casos positivos e negativos e liga cada caso ao produto Última
Luz:

```sh
bun test tooling/module-run-reference.test.mjs
bun tooling/check-module-run-cases.mjs
```

Use operações de package/workspace e o workflow module-run RU0. Este tooling não
implementa CLI.

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
blockers, ordem determinística e resultados module-run, package e audit.
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
`bun run check:borrow-expressivity` no root. O checker mantém a rejeição
   estável de bodyless multi-input e exige parse, oracle host e artefatos de
   mapping consistentes.
16. Para validar CAP0 sem compiler ou runtime, execute
   `bun run check:capability-matrix` no root. O checker deriva oito rotas por
   subcapacidade, valida 149 refs e 16 subcapabilities, e mantém a fila
   editorial de oito docs.
17. Para validar CYC1 sem compiler ou runtime, execute `bun run check:cyc1`.
   O checker deriva o grafo event-derived, SCCs Tarjan, reachability,
   breakability, ordem de drop, fronteiras foreign `unknown` e census bounded
   somente depois de admission close, drain e quiescence. As alternativas de
   generation/ID, owner-scoped lease e detached value são as composições que
   CYC2 registra; weak-key, ephemeron, collector e
   finalizer permanecem rejeitados e não há collector ou finalizer implícito.
18. Para validar SYN1 sem compiler ou runtime, execute
   `bun run check:syn1`. O checker valida a máquina, o estudo, os digests de
   Last Light, o parse Tree-sitter dos `.w` candidatos, os negativos de
   authority/phase/cache e as projeções de target; a introdução de módulo
   gerado continua Research.
19. Para validar BRX2 histórico sem compiler ou runtime, execute
   `bun run check:brx2`. Para validar a cláusula vigente BRX3, execute
   `bun run check:brx3`. O checker deriva relação, edges, OriginSet,
   SemanticInterfaceKey e digests de provider a partir de entradas estruturadas;
   deriva também runtime signature/WAbi e exige receipts de separate compilation;
   BRX2 preserva proveniência histórica; BRX3 publica `borrows(...)` no
   requirement/interface e function type, com gaps de implementação separados.
   O mesmo checker é `check:brx2` no pacote Tree-sitter e entra em `check:docs`
   e no aggregate `check` desse pacote.

### BRX2/BRX3 — relações de borrow por contrato

[`brx2-borrow-relations-cases.json`](brx2-borrow-relations-cases.json),
[`brx2-borrow-relations-machine.mjs`](brx2-borrow-relations-machine.mjs) e o
estudo em [`studies/brx2-borrow-relations`](studies/brx2-borrow-relations)
formam o oracle histórico BRX2. A máquina BRX3 em
[`brx3-borrow-relations-cases.json`](brx3-borrow-relations-cases.json),
[`brx3-borrow-relations-machine.mjs`](brx3-borrow-relations-machine.mjs) e
[`studies/brx3-borrow-relations`](studies/brx3-borrow-relations) publica a
cláusula source contextual vigente. A máquina separa a relação atual de
receiver/body-derived da relação candidata owned pelo requirement/interface.
Ela exige slots e modos canônicos, witnesses exatos, `SemanticInterfaceKey`,
lock e provider digest estáveis, e rejeita caller claims, witness-only,
metadata de runtime e derivações Rust-like. O source spelling do candidato fica
reserved/not-parsed no BRX2 histórico; `W-914`, WAbi e runtime continuam sem
mudança. O aggregate nominal é uma alternativa de API, não uma regra nova.
O host assay de caso `assay.kind: independent-assay` não é evidência de
compiler; não existe `problemTrace` dentro de uma declaration current sem body.
Invocation status fica separado de declaration decision. A máquina deriva
runtime signature/WAbi e exige receipts explícitos para separate compilation;
relação rejeitada nunca substitui o baseline vigente. Em BRX3, `borrows(result:
[source, ...])` é autoridade do requirement/interface; body/default e witness
somente provam a relação, e caller/call-site não podem declará-la. Resultados independentes
derivam dos slots não-dependent; flags legadas de result/`verified` são
rejeitadas.

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
O snapshot registra `routeCounts` (6 composable, 1 current, 1 research),
`canonicalSourceCount` (8) e `documentationQueuedCount` (8).

Use os gates locais:

```sh
bun run check:capability-matrix
bun tooling/check-capability-matrix.mjs --write
```

O script `check:capability-matrix` do pacote Tree-sitter entra na cadeia
`check:docs`; a execução ampla desse gate continua separada da validação local.

### CYC1/CYC2 — ciclo explícito e liveness condicional

[`cyc1-explicit-cycle-cases.json`](cyc1-explicit-cycle-cases.json),
[`cyc1-explicit-cycle-machine.mjs`](cyc1-explicit-cycle-machine.mjs), o
manifest e o estudo em
[`studies/cyc1-explicit-cycle-lifecycle`](studies/cyc1-explicit-cycle-lifecycle)
formam a evidência host histórica de CYC1. CYC2 em
[`cyc2-conditional-liveness-cases.json`](cyc2-conditional-liveness-cases.json),
[`cyc2-conditional-liveness-machine.mjs`](cyc2-conditional-liveness-machine.mjs)
fecha o recorte por composição. O corpus deriva estado a partir de
admission, edges strong/weak, close/unlink, unregister, cancel, callback
enter/exit, drain, quiesce, typed drop, destroy/unpin/reclaim e census. O oracle
calcula SCC, reachability, breakability, drop order e unknown boundary; nenhum
campo `expected` ou outcome do caller escolhe status.
`service.callCycle` é metadata de entrada limitada a `metadata` (call-cycle) ou
`external` (deadline); não é uma escolha do caller nem um outcome forjado.
Edges `explicitClose` têm owner declarado; `close`/`unlink` exigem essa mesma
autoridade, enquanto `drain` remove somente edges `lifecycleDrain` do owner
selecionado. Owner registry só vale quando está fechado.

Os 41 casos separam SCC conhecida fechada (`W-OWNERSHIP-0014`), residual
pós-drain (`W-MEMORY-0001`), root vivo, callback in-flight, FFI order,
service call-cycle/deadline, resource finish, panic/cancel, cross-domain
facts, lock/ABA, weak linearization/no resurrection, self-weak two-phase,
linked list e long-chain que requerem suporte de lowering iterativo, ainda uma
preocupação inconclusiva. Foreign hidden edge/root sem adapter é
`unknown`. Census só roda depois de admission close, drains e quiescence, é
bounded e não libera/coleta objetos.

A rota principal continua Componível: weak edge, owner/arena e close/drain são
composições explícitas; collector transparente, finalizer oculto, weak-key e
ephemeron são rejeitados no baseline. CYC2 testa generation/ID cache com key
detached, owner-scoped lease com invalidation/close e detached value sem back
edge strong. Só reabrir primitive se um problema bounded exigir
identidade/semântica ephemeron observável, todas as três composições falharem
sob census pós-drain e houver evidência independente de compiler/runtime/provider.
Use:

```sh
bun run check:cyc1
bun tooling/check-cyc1-explicit-cycle.mjs --write
bun run --cwd tooling/tree-sitter-w parse:cyc1
```

Os fixtures `.w` passam somente por Tree-sitter host. Compiler, runtime,
provider, stress, execução W e estudos humano/modelo continuam missing; CYC1
não promove API, syntax, collector ou finalizer.

### SYN1 — módulo gerado hermético

[`syn1-typed-generation-cases.json`](syn1-typed-generation-cases.json) e
[`syn1-typed-generation-machine.mjs`](syn1-typed-generation-machine.mjs) formam
um oracle host para o gate `SYN0-R1`. A máquina separa composição atual,
artifact de dados, o candidato Research de module set `.w`, C2 rejeitada e os
mecanismos D rejeitados. A matriz cobre 65 casos. A action key inclui o output
descriptor fechado, o graph receipt, inputs/dependencies e target receipt
declarado, mas não output bytes nem paths físicos. Action-result/CAS e
interface candidata são publicações separadas. `observedTrace` termina no
parse/source-shape host; `requiredPhaseTrace` mantém os semantic gates antes de
freeze sem alegar que o compiler os executou.

O estudo em
[`studies/syn1-typed-generation`](studies/syn1-typed-generation) usa quatro
fixtures atuais `.w`, quatorze artifacts candidatos `.w` parseados pelo Tree-sitter
real e três witnesses reservados `.txt`. O
[`study.json`](studies/syn1-typed-generation/study.json) mantém separadas a
evidência de parse/source shape e a evidência missing de compiler, name/type/
ownership/effect, ConstIR, run, target compiler/provider e estudos humano/modelo.
O target registry é apenas fixture host durável. Use:

```sh
bun run check:syn1
bun tooling/check-syn1-typed-generation.mjs --write
```

O output candidato é um module set de source units separadas; nenhuma etapa
injeta AST/HIR na unidade em execução. Logical paths e SourceIds entram nas
identities; checkout paths ficam somente na authority/provenance adapter.
Source maps permitem many-generated-to-one-source, mas exigem cobertura gerada
única e endpoints UTF-8 válidos. O explain record mantém generated sources
read-only inspecionáveis e navigation como requisito Research, sem alegar LSP.

### DYN1 — comportamento dinâmico versionado

[`dyn1-versioned-behavior-cases.json`](dyn1-versioned-behavior-cases.json),
[`dyn1-versioned-behavior-machine.mjs`](dyn1-versioned-behavior-machine.mjs), o
manifest e o estudo em
[`studies/dyn1-versioned-behavior`](studies/dyn1-versioned-behavior) formam a
evidência host design-oracle para `DYN0-G1`. O corpus tem 70 casos derivados de
eventos; o snapshot deriva métricas de route/status, projections e cleanup para
REPL snapshots e invalidation, typed service/plugin generations, identities
`SemanticInterfaceKey`/`WAbiKey`/`RuntimeClosureKey`, schema exact/compatible,
admission close, cancel/drain/unregister/in-flight/destroy/unpin/release/unmap,
stale completions/messages/capabilities, effect/capability audit, export/import
redacted, source maps/digests, target local/split, callback/FFI unload,
crash/cancel e quotas. O Restaurante testa cada fault entre preparação,
publicação e limpeza; falha pós-switch deriva `degraded`, rollback só deriva de
provider receipt estruturado antes da publicação, crash pré-publicação preserva o
antigo ou deriva `unknown-effect`, e crash pós-publicação mantém o novo committed.

Reducers local e split têm loops independentes. A comparação exige o mesmo owner
graph, generation, interface result, effect outcome, cleanup order, capability
state, stale events, selection, crash/degraded, export/import e export digest,
mas aceita trace físico diferente. O caso C é somente a
lacuna `DYN0-persistent-generation-reference`, para facts read-only de uma
generation entre restart/deploy; inspector comum de snapshot continua na rota A.
Seleção concorrente aceita vários candidates ready somente com um winner receipt
atômico. Empate, duplicate, ausência de receipt ou handle stale é rejeitado.
Arbitrary eval/exec, monkey patch, active-frame/debugger write, ambient lookup,
native dynamic library como sandbox e `dlclose` com callback vivo ficam na rota D
intencionalmente rejeitada. `expect` é guard de mutation e nunca escolhe o
resultado.

Schema `compatible` prova uma nova `SemanticInterfaceKey` e `ServiceIRKey` por
receipt old/candidate, compatibility-map digest derivado e decisão explícita; Target A/B altera `WAbiKey` e artifact
físico sem alterar o resultado lógico. Native exact-WAbi retém o mapping até o
fim da runtime island; process, Wasm e component só fazem full unmap após drain.

Evidence atual: source refs/digests Last Light, refs oficiais C/POSIX/Rust/Python,
oracle host event-derived, mutation tests e snapshot. Compiler, runtime,
provider, std provider, isolamento real, stress e estudos humano/modelo ficam
missing. Use:

```sh
bun run check:dyn1
bun tooling/check-dyn1-versioned-behavior.mjs --write
```

O gate não altera `DESIGN.md`, não cria syntax/diagnostic e não lê fontes
geradas em `tooling/tree-sitter-w/src/`.

### HRD0 — hot reload somente para desenvolvimento

[`hot-reload-dev-cases.json`](hot-reload-dev-cases.json),
[`hot-reload-dev-machine.mjs`](hot-reload-dev-machine.mjs), o checker e o
estudo em [`studies/hrd0-hot-reload-dev`](studies/hrd0-hot-reload-dev) fecham
um problema-first de runner, sem criar uma feature da linguagem. O runner é
tooling-owned: recompila e reabre units W normais, então aplica
`prepare → validate → preflight → ready → switch`; a generation antiga fecha
admission e drena antes de ser liberada, e roots novos entram somente na nova.

O corpus tem 20 casos, cinco mutations adversariais (cleanup extra físico,
ausente lógico, ordem errada, nominal duplicado e interface digest drift), dois
reducers independentes (local e split), 13 diagnostics e um contrato Last Light
comum que os dois witnesses importam para records de input/result e funções de
eventos. O cleanup lógico comum exige cancelamento, drains,
unregister, in-flight drain, destroy e release; `unpin` só aparece com pin/FFI
declarado e `unmap` só com mapping não nativo. Ele deriva stale completion,
message e capability rejection; identities de package/recipe/artifact/source
map/`SemanticInterfaceKey`/`ServiceIRKey`/`WAbiKey`/`RuntimeClosureKey`; schema,
effects, capabilities, quotas, isolation, callback lifetime, OOM, crash,
rollback e cleanup. Falha pré-publication preserva old; rollback exige receipt
pré-publication; drain pós-publication deriva `degraded`; unknown provider effect
fica explícito. A comparação local/split exige o mesmo resultado lógico, mas
aceita trace físico diferente.

Generated module sets continuam a Research de SYN1 e devem reabrir/checkar
units novas; escolha de CLI (`w dev` ou `w run --watch`) fica tooling-owned e
não selecionada. Production/release dynamic mode, eval/exec, monkey patch,
active-frame/debugger write, ambient lookup, native dylib como sandbox e
`dlclose` com callback vivo são rejeitados. HRD0 referencia DYN1, SYN1 e CAP0;
não promove compiler, runtime, provider, isolamento real, stress ou estudos
humano/modelo.

Use:

```sh
bun run check:hrd0
bun tooling/check-hot-reload-dev.mjs --write
```

O segundo comando atualiza somente o snapshot host-derived. Nenhum resultado
do checker é comportamento implementado.

### GEN1 — suspensão incremental (histórico)

[`gen1-incremental-suspension-machine.mjs`](gen1-incremental-suspension-machine.mjs)
é um oracle host histórico que informou/estreitou o gate `GEN0-R1`. Ele executa os mesmos traces nas
lowerings `switched-resume-frame` e
`returned-continuation-state-loop`, comparando owner graph, commit/HB, resultado
typed, cancelamento e cleanup/drop/drain. O trace físico e o packing podem
mudar. O corpus é
[`gen1-incremental-suspension-cases.json`](gen1-incremental-suspension-cases.json)
e o snapshot é
[`gen1-incremental-suspension-results.snapshot.jsonl`](gen1-incremental-suspension-results.snapshot.jsonl).
O estudo mantém as variantes em
[`studies/gen1-incremental-suspension`](studies/gen1-incremental-suspension):
Stream/adapters/tasks, máquina nominal, dois canais bounded, um witness textual
evidence de bloco Stream compiler-owned e um witness textual rejeitado de frame
público. GEN1 não mantém um gate Research corrente: GEN2 fecha a forma estreita
de pull e o frame público continua rejeitado.

As métricas são derivadas de declarações source reais e únicas nas slices do
mesmo cenário: conceitos públicos, handoffs de ownership, effects/cancel/
cleanup explícitos, estado oculto, adições públicas de type/ABI e operações de
source. LOC é secundário. Use:

```sh
bun test tooling/gen1-incremental-suspension-reference.test.mjs
bun tooling/check-gen1-incremental-suspension.mjs
```

O bundle registra fontes primárias C/POSIX/LLVM/Rust/Python e permanece
`design-oracle-input`. Compile, run, provider, estudo humano e estudo de modelo
continuam missing. A integração com `check:docs` ocorre pelo `check:studies`
root, pelo parse de studies e por `check:links`; a documentação do CAP0 mantém
`docsStatus: queued`.

### GEN2 — expressão `stream` com `yield` owned

[`gen2-stream-yield-machine.mjs`](gen2-stream-yield-machine.mjs) fecha o contrato
de design para producers lineares pull. A forma corrente é a expressão
`stream <[capture_item, ...]> { ... yield (take|copy) value }`: a lista é explícita,
avaliada e movida/copiada/referenciada na construção, o cursor é exclusivo e a
capacity é zero. `await`/`try`, `return` terminal, `defer`, cancelamento e drop
seguem `Stream`; diálogo bidirecional usa `Channel` bounded. Frame público,
`send`/`throw`/`close`, `yield-from`, buffer oculto, view/borrow/inout, reentrada
e FFI resume são witnesses rejeitados.

O bundle [`studies/gen2-stream-yield`](studies/gen2-stream-yield) liga três
variantes (composição atual, forma estreita selecionada e witness rejeitado),
`bundle.json`, oracle, fontes Last Light e apresentação counterbalanced. O
corpus tem 20 casos, dois reducers independentes e snapshot determinístico; a
decisão mede símbolos e invariantes, não LOC. `take` move o item e `copy` exige
`Duplicable` e preserva o binding; bare `yield value` é `W-YIELD-0002`. Use:

```sh
bun run check:gen2
bun run --cwd tooling/tree-sitter-w parse:gen2
```

O bundle é integrado em `check:studies`, `check:docs` e no aggregate Tree-sitter.
Parser e host oracle não provam semantic checker/compiler, runtime, provider,
stress, debug/ABI/reflection ou estudos humano/modelo; esses permanecem a
implementation-evidence-gap de W-1438/W-1440. A fila de documentação do CAP0
continua `docsStatus: queued`.

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
