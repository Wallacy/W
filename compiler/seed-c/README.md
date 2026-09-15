# Source reader, lexer, parser, formatter, frontend seed e D0/D1 do seed C

**Status:** componente real do w-seed-c. O parser seed abaixo é uma fatia
incremental de CST/recovery. O formatter, o adapter D0 e o frontend seed
semântico são fatias fechadas caller-owned. O target bootstrap `w` usa o núcleo
privado para o perfil CHK9 de root efêmera explícita e imports locais
alcançáveis. O target bootstrap não é um compiler driver completo.

## Política de dialeto C

O seed usa C23 como padrão explícito no CMake, nos probes e checkers C, na
compilação do artefato C conservador HLO1 e na receita BMD byte-scan-view.
`W_SEED_C_STANDARD=23` é o
default e o cache aceita somente `23` ou `11`; `11` é uma lane explícita de
recovery/compatibilidade. O CMake exige o standard e mantém extensões off.
GCC/Clang usam `-std=c23`; uma toolchain que só aceita `-std=c2x` pode rodar
correctness com disclosure `c2x-preview (correctness-only; not a final C23
result)`, sem ranking final C23. Não há fallback silencioso para C11. No
Windows, MSVC usa `/std:clatest` somente na lane `c23-msvc-preview`, com
correctness-only e sem resultado C23 final. A lane `c11-recovery` exige
solicitação explícita.

O código continua compilável em C11 recovery. Essa escolha não cria requisito
C23 para uma ABI C externa. C permanece backend de validation, differential e
recovery; MLIR0 é a rota nativa primária somente para o subset fechado, e W/MLIR
geral continua futuro.

## Native benchmark measurement kernel

`include/w_seed_native_benchmark.h` and
`src/w_seed_native_benchmark.c` provide the bounded C23 measurement kernel used
to replace Bun's coarse direct-child counters. The Windows adapter launches a
fresh suspended process, assigns it to a kill-on-close Job Object before
execution, applies one QPC deadline to the root and all descendants, drains
bounded stdout/stderr concurrently, and checks exact raw-byte output and exit
status for every warmup and sample. Caller-owned samples are committed only
after the Job Object is empty and both readers have joined; timeout, capture
overflow, and oracle failure leave the sample array unchanged.

The ABI v2 API reports user/kernel/total CPU for the root separately from the
aggregate Job CPU, plus root peak working set and Job peak committed memory.
After tree exit, final root CPU counters are fused as lower bounds when the Job
snapshot has not yet converged, so an aggregate can never be smaller than its
root member.
Peak Job commit is deliberately not called RSS. Zero warmups are valid for a
caller that owns warmup and measured series separately.
`cli/native_benchmark.c` exposes the same boundary as one compact JSON receipt
with raw samples plus min, median, nearest-rank P95, and arithmetic mean. Run
`bun check --target benchmark` for a temporary Clang C23 build and adversarial
integration test. The executable runner consumes these receipts for production
runtime measurements and binds the C sources into its runner digest. Bun still
orchestrates compilation, publication, and cleanup. Linux and macOS adapters
remain future work.

## Limite de medição BMD1

O runner BMD1 mede somente o ponto `clean × check-end-to-end` da matriz
compiler-lifecycle. Ele constrói este seed em Release fora da medição. Depois
executa `w check reference/last-light/checker_bootstrap.w --json` como oracle.
O oracle prova exit 0 e stdout/stderr vazios antes de warmup e samples. Cada
sample inicia um processo novo e usa monotonic wall clock em ns. O tempo inclui
startup do processo e o estado de cache do filesystem e do OS.

Essa medição não prova tempo de source, lex, parse, semantic, HIR, lowering,
codegen ou link. Ela não prova no-op, edit, native backend, runtime, provider,
linguagem ou uma comparação de performance. O runner não transforma o seed em
compiler W completo.

Este componente fornece uma view de bytes sem cópia. Ele valida UTF-8 estrito,
detecta o BOM inicial, conta linhas por LF, valida spans half-open e converte
offsets de bytes para pontos determinísticos. Neste primeiro seed, cada
conversão de ponto faz um scan O(bytes) a partir do início da view. O lexer
lossless consome a mesma view e devolve spans contíguos para prefixo BOM,
trivia, palavras do profile Unicode, números, pontuação, eventos de literal e
spans foreign pinados pelo harness.

A ordem de bootstrap e o pipeline que motivam este componente estão em
[DESIGN §20.5 — Bootstrap](../../DESIGN.md#205-bootstrap) e
[DESIGN §20.2 — Pipeline](../../DESIGN.md#202-pipeline). O gate SH0 continua
ausente, conforme [DESIGN §26.6.1 — Gates internos do self-host](../../DESIGN.md#2661-gates-internos-do-self-host).

O lexer é uma fatia interna destinada a SH0. Strings normais com aspas simples
ou duplas publicam o mesmo kind e permitem interpolation; a expected type decide
se um literal de um scalar satisfaz `UnicodeScalar`. Raw simples ou multiline
desativa interpolation, e byte literals permanecem separados. Palavra é sempre
crua: a tabela de
keywords pertence ao owner/parser. O sinal numérico permanece pontuação
separada. Números preservam os sufixos correntes e só recebem a flag de
quantity quando a expressão de unidade lexical fechada está adjacente. UTF-8
fora de literais, comentários e BOM inicial usa o profile Unicode 17.0.0:
`XID_Start` mais `_` no início, `XID_Continue` na continuação, e rejeição de
`Default_Ignorable_Code_Point`. O WORD mantém os bytes e o span raw. NFC,
colisões no resolver, confusables, scripts mistos, formatter normativo e scanner de
foreign não pertencem a esta fatia. CRLF é um único item
NEWLINE; CR isolado é UNSUPPORTED_CONTROL interno. Erros internos não são
diagnósticos D0.

## Parser seed interno (fatia incremental)

`include/w_seed_parser.h` e `src/w_seed_parser.c` adicionam uma API C23 sem
alocação para uma fatia incremental: header `module` opcional, imports
ordinários no topo, `fn` com parâmetros simples e requirements
`ref`/`inout`/`take`/`const`, retorno opcional (incluindo `()`), `throws Type`,
qualificadores de tipo `view` e `shared`, e cláusula contextual `borrows(...)`
somente em declarações `fn` com body, após o retorno/`throws` e antes do bloco. A
cláusula preserva `borrow_clause`, `borrow_pair` e `slot_ref` em ordem de origem;
cada slot aceita somente a folha lexical WORD ou NUMBER. O parser também
reconhece a expressão delimitada
`lock expression as identifier { ... }`, com os prefixos `await lock` e
`try lock`. O reconhecimento é somente sintático: o CST preserva as folhas e a
ordem estrutural. Nesta fase, `await`, `ref` e outro `lock` no corpo são aceitos
apenas como sintaxe. A rejeição semântica de casos inválidos fica para uma etapa
futura.
`entry(name)`, `struct` simples exportável com fields, enums fechados com cases
posicionais ou rotulados e payloads, `test "..." for name`
com `expect`, blocos, `let`, `return`, `if`/`else`, `repeat`/`while`, arrays
repetidos `[expression; expression]`, `for` com marcador opcional
`ref`/`inout`/`copy`, um binder WORD, `in expression` e bloco, labels para
`repeat`, `for` ou bloco, `break`/`continue`, argumentos posicionais ou
`label: expression`, declarações `async fn` e `export async fn`, e os prefixos
sintáticos `copy`/`take`/`pin`/`inout`/`ref`. A expressão estruturada
`pipeline` aceita bloco dependente, cadeia curta com dois ou mais passos
`.name(args)`, tasks `pipeline<tasks: ...> each item in expression { ... }` e
transaction `pipeline<transaction: { ... }> tx = provider { ... }`; `commit`
permanece um statement estrutural com expression opcional. O parser Pratt
delimitado também reconhece tuple types e tuple expressions com dois ou mais
itens, inclusive trailing comma, e o statement
`spawn<.domain>` ou `let name = spawn<domain: .domain> expression`. O parser
mantém `()` e `(expression)` como formas unitária e parenthesized. `(T)` e
`(T,)` não são aceitos como tuple type, e `(expression,)` não é aceito como
tuple expression. O parser Pratt é usado pelos vinte e oito casos F0
selecionados. A tabela de
reconhecimento inclui atribuições compostas, pipe `|>` left-associative abaixo
de `??`/OR, coalescing, operadores lógicos e bitwise, comparações, ranges,
shifts, aritmética, `@`, potência e `in`/`is`; postfix `#identifier` e paths
estáticos qualificados também são preservados, sem afirmar immediate use;
isso é reconhecimento sintático, não uma declaração de semântica, tipos ou
validade contextual. O CST é
flat e caller-owned: cada nó usa `first_child`/`next_sibling`, as folhas raw e
trivia formam uma partição exata dos bytes e todos os textos continuam views
do source. O parser mantém somente lookahead caller-owned e frames caller-owned;
capacity exhaustion é fatal determinístico. Cada instância é single-use: a
primeira chamada a `w_seed_parser_parse` consome o parser; uma segunda chamada
retorna `false` sem alterar o resultado ou os buffers caller-owned.

Uma fatia sintática anterior reconhece `generic_parameters` append-only em
`struct`, `fn`, `type` e `alias`, declarações de `type`/`alias` com ordem de
origem, e
envelopes de contract sequenciais em tipos e em postfix de expressão. Para
`struct`, a normalização semântica publica agora o schema caller-owned dos
parâmetros genéricos. Ela distingue `type` de `value` por resolução de domínio,
preserva policy de label (`positional-only` ou `required`), normaliza o
domínio base sem incluir um refinement posterior e registra predicate const,
span e subject `.member` somente para um call direto `identifier(.member)`
com assinatura compatível. Refinements inline/range e calls compostos ou
aninhados permanecem `UNSUPPORTED` nesta fatia. O resolver usa ordinais de
declarations e aceita predicate declarado depois do `struct`. Domínio nominal
não resolvido fica `INVALID` e produz `W-GENERIC-0001`; predicate com retorno
diferente de `Bool` produz `W-CONTRACT-0003` e mantém o refinement inválido.
O registro preserva `external_label` separado de `internal_name`. A fatia atual
também publica aplicações genéricas de `struct` locais no mesmo módulo/documento.
Cada aplicação tem owner type, head, envelope, argumentos ordenados e status de
binding; cada argumento preserva ordinal, span, label, parâmetro, kind, o índice
de type ou `ConstValue` e o índice sentinel/relacionado de `TypedConstExpr`. O
root liga à aplicação por `generic_application_index`.
`W_SEED_FRONTEND_SCHEMA_VERSION` is `w-seed-frontend-29`. Earlier D2/D3 fields
anteriores permanecem append-only; a versão 6 acrescenta records, ranges,
counts/capacities e relações de module const; a versão 7 acrescenta
`effective_type` e preserva `declared_type` como annotation source-only para
inferência scalar D7; a versão 8 separa `logical_source_id`, `module_id` e
`local_module_name` e acrescenta edges de import resolvidos caller-owned; a
versão 9 acrescenta o carrier de diagnostics frontend com facts, items e
labels tipados, counts exatos e ranges caller-owned append-only. Version 11
publishes `resolved_binding_statement` as an explicit indexed relation and
keeps `effective_type` separate from `declared_type`. Version 12 preserves
literal-event identity on CST leaves and adds ordered interpolation segment
records. Text segments own `const_bytes`; expression segments own normalized
expression indices. The current seed accepts plain ordinary String text and
built-in integer, Boolean, or String interpolation. It defaults unconstrained
integer interpolation to canonical signed `i64`. Version 16 retains those
append-only records and adds resolver-owned external nominal identity pairs;
W-1542 documents its bounded `std.process` use. Version 27 appends
provider-neutral accelerator-module and ordered kernel-binding records for
exact `accelerator.module<{...}>()` static records. The separate
`w_seed_gpu_module` bridge owns and independently verifies the first bounded
signed-`i32` device-module slice. HIR0 and MLIR0 lower only their bounded
subsets. Version 28 appends a discriminated caller-owned domain kind and the
exact typed relation from `spawn<domain>` (where the selected domain is
accelerated) to an immediate
accelerator-module field, including module/kernel indices and the static
submission budget. It rejects bare or `async` module-field calls, host-domain
offload, missing fields, and malformed accelerated bindings. Escape decoding,
Boolean/String value Display, general Display conformance, general device IR,
and native provider linkage outside those subsets remain gaps.

Version 29 gives `throws E` and `throw value` a typed compiler relation.
The CST owns the throws type separately from the normal return type; frontend
functions publish `error_type`, and a throw statement owns one contextually
typed expression. HIR0 schema `w-seed-hir0-39` copies the concrete local enum,
its exact core-`Error` conformance fact, and a typed `THROW` terminator into
caller-owned records. The new fields are covered by semantic/provenance
verification, receipts, capacity and alias barriers, and remain valid after
frontend teardown. Current HIR evidence covers a terminal root throw and one
complete top-level conditional whose two arms end in return or throw. That
branch has no synthetic join, and verification rejects cross-arm jump forgery.
Non-`Error` enums and a throwing branch with a later continuation fail closed;
nested terminal branches, try/catch, cleanup, provider/Task binding, and
MLIR/native execution are not implemented by this slice.

Version 30 adds `W_SEED_CST_TRY_EXPRESSION` and Frontend30 schema
`w-seed-frontend-30`. The parser wraps `try` and adjacent `try?` prefixes, but
frontend support is limited to exact plain `try localCall(...)`. The nested
call must be synchronous, local, and declared `throws E`; the lexical caller
must also declare `throws E` and use the same nominal local error enum. The
`EXPR_TRY` record copies the call result type, records the call index, and
publishes `propagated_error_enum` in the receipt. A direct synchronous
throwing call without a valid `try` owner is unsupported. Supported async or
spawn owners carry the thrown outcome without `try`. Optional or async `try`,
conversions, non-direct or non-local calls, HIR/native lowering, catch, and
cleanup remain unsupported. This is compiler-lifecycle correctness evidence,
not a product or performance claim.

Version 40 extends HIR0 with exact synchronous typed propagation. The
terminator-owned W_SEED_HIR0_TERMINATOR_INVOKE owns the direct local call and
routes to one normal successor and one typed-error successor. Each successor
has one block argument at ordinal zero. The normal argument carries the
function return type. The error argument carries the function error-enum type.

The verified relay has exactly three empty blocks: invoke, normal return, and
error throw. It has no ordinary CALL instruction, Task, heap object, or packed
Result carrier. The HIR verifier checks the complete ownership, target, type,
span, range, capacity, alias, and digest relations. ProductClosure0 rejects
INVOKE because this slice has no MLIR, native, or public lowering. Catch,
cleanup, conversions, general propagation, public ABI, benchmarks, and
performance remain unsupported. This is source-backed-current evidence only
for verified HIR.

W-1617 adds a separate compiler-lifecycle MLIR adapter for that exact relay.
It emits a private `!llvm.struct<(i1, i64)>` carrier, where zero is success,
one is the only payloadless `Failure.denied` case, and the `i64` field carries
the normal value. The relay uses a real `llvm.call`, extracts the two fields,
and branches into explicit normal and error blocks. The same target-neutral
text is produced for the current Linux and Windows selectors.

`w_seed_mlir0_measure_typed_propagation`,
`w_seed_mlir0_emit_typed_propagation`, and
`w_seed_mlir0_verify_typed_propagation` are bounded, caller-owned,
transactional, alias-safe, and digest-bound. The test binary's private
`--emit-typed-propagation` mode lets repository tooling pass the exact output
through `mlir-opt` and `mlir-translate`; it is not a public W command. The
artifact has no unwind edge, Task, heap allocation, process root, `main`, or
public ABI, and ordinary product selectors still reject it. Native product
execution, catch, cleanup, conversions, general errors, benchmarks, and
performance remain gaps.

Version 41 adds one bounded synchronous cleanup relation to the exact typed
propagation witness. The accepted `defer` body is one direct local
zero-argument nonthrowing `Unit` call before the terminal `return try`.
`w_seed_hir0_cleanup` binds the lexical registration to the invoke and to the
ordinary cleanup instruction/call in each normal and error successor. The
record participates in capacity and alias checks, receipts, semantic and
provenance digests, independent verification, and frontend-lifetime tests.
All older native, process, cooperative, and ProductClosure selectors reject a
nonzero cleanup count.

The existing typed-propagation MLIR API emits
`w-seed-mlir0-typed-cleanup-1` for this HIR41 shape and leaves the HIR40
artifact byte-identical. The private MLIR defines `clean`, calls it once in
each successor before returning the compact `{i1, i64}` carrier, and remains
target-neutral for the current Linux and Windows selectors. The private
`--emit-typed-cleanup` test mode lets the repository gate parse and translate
those exact bytes with MLIR/LLVM 23.1.1. This is not a public W command or ABI.
Multiple or nested cleanup, `defer async`, runtime stacks, closures, catch,
native products, benchmarks, and performance remain unsupported.

`w_seed_parallel_typed_binding1` adds the next private boundary without
widening the success-only provider or lifecycle schemas. It re-verifies the
exact HIR41 typed invoke and dual-path cleanup, derives the payloadless nominal
error identity, and validates one independently evaluated success completion
plus one typed-error completion from PLATFORM1. The provider error code is
never interpreted as a W enum case; an explicit case ordinal must match the
verified HIR identity.

The current two-task array is only the seed witness shape. It is not a W,
HIR, scheduler, target, runtime, or ABI limit. Physical capacities one and two
produce identical semantic records and different physical provenance. The
adapter stages all PLATFORM1 writes until its checks pass, publishes into
caller-owned storage transactionally, rejects aliases against all HIR backing
ranges, and supports independent verification. Its SHA-256 receipt proves
integrity, not provider authentication. Native HIR execution, trusted
attestation, typed TASKLIFE, other platform providers, public products,
benchmarks, and performance remain unsupported.

`w_seed_accelerated_invocation0` consumes only that successful Frontend28
relation plus a verified `w_seed_gpu_module` program. ACCINV0 copies one exact
zero-argument static launch and lexical await into caller-owned invocation and
text storage. Its semantic digest binds domain policy, copied identities,
signed-`i32` result shape, and the device-module semantic digest; source
ordinals, spans, frontend receipt, and device-module provenance remain in its
provenance digest. `measure`, `run`, `program_from_output`, and `verify` are
transactional and reject aliases, short capacities, malformed producers, and
forged relations. Verification remains valid after source, frontend, and
gpu-module storage is discarded. The one-invocation and zero-argument limits
are the current seed array/bridge bounds, not W language or ABI limits.
ACCINV0 contains no provider, target, queue, geometry, transfer, residency,
pointer, launch handle, MLIR handle, or physical ABI, and it does not yet claim
the nominal `LaunchError` carried by the eventual Task type.

`w_seed_accelerated_binding0` consumes only a verified ACCINV0 program/result
and one closed product/profile record. ACCBIND0 copies the root, canonical
domain, local descriptor, module, artifact, kernel-instance, target and
provider-class identities into caller-owned storage. It accepts only the
reject fallback in this first slice, requires the selected instance and
provider ABI to match the closed record, and sets the effective in-flight
limit to the exact minimum of the invocation, profile, root, deployment and
resource-limit values. Queue, device and provider-generation identities are
provenance rather than semantic identity; the signed-`i32` result shape remains
explicit in the relation. Its verifier survives producer and
profile teardown and rejects malformed offsets, forged digests, aliases and
short capacities. The supplied closed-profile receipts are upstream evidence,
not cryptographic authentication performed by ACCBIND0. No provider handle,
submission, join, result, transfer, residency, public product route or
supported GPU ABI is implemented here.

`w_seed_accelerated_request0` consumes only verified ACCBIND0 and GPU0
program/results. ACCREQ0 cross-checks the source-local descriptor, GPU0 host
root, kernel label, private device function and explicit signed-`i32` result
shape, then copies one request, every identity byte and the exact device
artifact into caller-owned storage. Semantic identity excludes queue, device,
generation and source ordinals; provenance binds those facts. Its independent
verifier reconstructs dense text ranges, rehashes the artifact and survives
both producer lifetimes. Measure, emission and bridging fail closed on aliases,
short capacity, malformed producers and forged records or digests. This is a
provider-neutral compiler boundary, not a submission API: it contains no
provider handle, CUDA call, queue operation, completion receipt or public ABI,
and the existing CUDA adapter does not yet consume it.

O seed materializa `Bool`, inteiros bounded (incluindo `usize`), strings simples
sem escape, cases enum contextuais e `StaticList` caller-owned. Inteiros usam
bytes little-endian canônicos; strings usam offsets em `const_bytes`; listas
preservam ordem, vazio e duplicatas com `const_elements`. O teto explícito de
lista é 4096 elementos e o de slots de uma aplicação é 64. `value: T` é um
value domain dependente somente quando
`T` é type parameter anterior e resolve para `StaticArgumentRepresentable`.
Todos os slots continuam obrigatórios: `_ value: T` é positional-only e cria uma
âncora; value parameters sem `_` exigem seu label externo.
O status de binding não prova predicate, especialização ou execução posterior.

A resolução exige head `struct` local, inclusive forward reference, e não chama,
inclui ou depende do componente ConstIR. A forma D3 parentetizada publica
`TypedConstExpr` e `TYPED_PENDING_CONST` somente para árvore fechada de literais,
grouping, unary e binary operators com resultado Bool ou integer explícito;
o frontend não avalia. Generic calls, identifiers/named const, heads importados,
enum/object/type/alias/function, quantity/size, `Bytes`, listas aninhadas,
String result e outras formas permanecem `UNSUPPORTED` ou fora do seed conforme
a forma. O seed não apresenta esta fatia como compiler W completo.

Os argumentos de contract aceitam somente formas sintáticas: tipo/path WORD, membro contextual
`.id`, argumento nomeado `id: static_value`, predicado `(expression)`, lista
`[static_value, ...]`, número, literal, bool ou quantity. `switch expression`
aceita pelo menos um arm `case .id|literal: expression`. Para as aplicações
locais suportadas, listas vazias e duplicatas são preservadas; predicate truth,
expressions calculadas e inferência permanecem fora do seed.

Esta fatia de `for` não inclui `async`, patterns de destructuring ou `take` como
marcador de iteração; um rótulo aplicado a `while` permanece STOP. O prefixo
`async` só é aceito no owner root de `async fn` ou `export async fn`; o parser
preserva `try`/`await` como folhas raw, sem validar a ordem semântica dos
efeitos.

Esta fatia também reconhece a forma sintática de tipo callable necessária ao
F0: qualificadores externos `some` e `any`, modo `mut` ou `take`, `fn(...)` e
retorno opcional `: type`; `throws` e `borrows(...)` reutilizam os helpers
existentes quando aparecem depois do tipo. O owner preserva
`function_type` e `function_type_parameters` sem inferência ou validação de
ABI, contratos, efeitos ou ownership.

Closures explícitas com captura têm a forma `<[copy|ref|take|weak WORD, ...]>`
seguida de parâmetros entre parênteses, `=>` e uma expressão ou bloco value.
A lista de captura não pode ser vazia; parâmetros podem ter `: type` e trailing
comma. O CST preserva `capture_expression`, `capture_item`,
`closure_expression`, `closure_parameters` e `closure_parameter`. Duplicatas,
nomes desconhecidos, escape, drop, inferência de captura e regras de borrow
ficam fora do parser. Bare closures `(x) => value` e `(x) => { ... }` continuam
STOP nesta fatia. `capture(...)` continua uma chamada ordinária em WORD; o
parser não reserva esse identificador para uma forma antiga.

O lexer continua emitindo `>>` como uma folha raw de dois bytes. Um owner de
type cria duas `w_seed_parse_token_view` virtuais sem duplicar a folha; um owner
de expression mantém `>>` como shift. Newline continua trivia. Recovery só cria
`ERROR` com os bytes ignorados e `MISSING` zero-width. Os `w_seed_parse_issue`
internos têm mapping futuro para D0, mas não são diagnósticos D0. `manifest`,
members/methods dentro de enum são recuperados como unsupported; declarations
além de `fn`/`struct`/`enum`/`type`/`alias`/`test`/`entry`, patterns e bare
closures, semântica de effects/async/lock, contratos de pipeline,
AST/HIR,
name/type resolution e formatter normativo permanecem fora; `foreign` falha fechado antes
do body. `unsafe fn<C>` e `export unsafe fn<C>` são aceitos somente pela ilha C
validada abaixo; `unsafe fn` sem tag de linguagem permanece STOP. Imports só
aparecem antes de qualquer declaration; `export` aceita `fn`, `const fn`,
`async fn`, `struct`, `enum`, `type` e `alias` nesta fatia. Enum generics são reconhecidos
sintaticamente, mas continuam unsupported no frontend.
As quatro formas de `pipeline` são uma supergrammar sintática nesta fatia. O
parser não valida owner, provider, nesting, commit, rollback, effects, schemas
ou atomicidade; o frontend publica a família como unsupported e checker,
lowering e runtime permanecem gaps. `transaction` bare continua um identifier,
e a forma legada `transaction tx = provider { ... }` não produz
`pipeline_expression` nem é uma forma corrente completa. Statements `commit`
podem aparecer em qualquer block, porque owner e cardinalidade pertencem à
validação semântica futura. `const fn` e `export const fn` preservam o modifier no CST e
são as únicas formas const desta fatia. `const async fn`, `const unsafe fn`,
`async const fn`, duplicatas e `const` sem `fn` falham fechado. `static` e
receiver modifiers permanecem fora; `unsafe` sem uma ilha de linguagem também
falha fechado. `expect` fora de `test` falha fechado.

Statements `allocator [binding:] expression { ... }` são reconhecidos em
qualquer block, inclusive de forma aninhada. O owner `allocator_block` preserva
o keyword `allocator`, a binding WORD opcional e seu `:`, uma única expressão de
plan e um único block na ordem dos bytes; o CST não adquire leases, valida
capacidades ou resolve chamadas contextuais. `try allocator` e `allocator` na
raiz continuam STOP, e o parser não afirma a semântica de providers, contexto
ou recuperação de allocation.

Corpos `fn<C>` e `fn<lang:.c>` usam um scanner C23 caller-owned com o profile
`c-inline-1`. A entrada do scanner é somente a view que começa em `{` e os
limites explícitos `maximum_body_bytes`/`maximum_nesting`; não há filesystem,
locale, environment, shell, alocação ou estado global. O resultado é uma
`w_seed_foreign_source_validation`: spans relativos (`body_start_byte`,
`body_end_byte`, `close_byte`, `next_byte`), limites, nesting observado, estado
terminal e SHA-256 do body. Este é um registro de validação de fonte, sem
`adapterDigest`, `scannerDigest`, ABI/lock, recipe ou publicação de build.

O profile valida strings/caracteres com escapes, comentários, braces aninhadas,
digraphs `<%`/`%>`, UTF-8 estrito sem NUL, CRLF e limites. Diretivas de
preprocessador e line splice fora de literal/comentário falham antes de o parser
continuar. Uma falha produz exatamente um issue fatal `FOREIGN_SCANNER` com o
span primário do scanner e um `ERROR` para o remainder; o C nunca é lexado como
W. Em sucesso o parser consome `{`, exige cache interior vazio, faz
`require_opaque`/`claim_opaque` no span exato, consome o leaf existente
`FOREIGN_BODY` (inclusive zero bytes), verifica `}` e então permite o sufixo W.
`<abi:.c>` continua o envelope ABI ordinário e não seleciona o scanner.

O lexer permanece responsável apenas pelo handshake e pelo leaf raw; os owners
append-only `FOREIGN_LANGUAGE_TAG` e `FOREIGN_BODY_OWNER` preservam a CST. Esta
fatia não afirma AST, ABI, fallback editorial ou build do Last Light. O
formatter interno só aceita CST `COMPLETE` sem issues e o adapter D0 só emite
records determinísticos para `source.lex`, `source.parse` e `source.format`.
Ele não inventa códigos para fatos sem mapping suportado.

## Formatter seed e adapter D0

`include/w_seed_formatter.h` e `src/w_seed_formatter.c` formam um formatter
C23 sem heap, path, locale, clock ou environment. A API recebe buffers de
tokens, grupos e output do caller, mede antes de escrever e rejeita
CST recuperado/fatal. A renderização usa a estrutura CST e as folhas raw; não
carrega o oracle JSON nem procura IDs ou digests em runtime. O gate compara os
31 pares de [`formatter-cases.json`](../../tooling/formatter-cases.json),
reparseia o output, verifica a assinatura CST recursiva, idempotência, capacity
all-or-nothing e preservação byte-a-byte de `FOREIGN_BODY`. A política de
quebra usa a coluna preferida 120 sobre largura sem trivia e é uma política
limitada do seed, não uma especificação do formatter normativo.

`include/w_seed_diagnostic.h` e `src/w_seed_diagnostic.c` formam o adapter D0
mínimo. O record `W-FMT-0001` tem o schema JSONL canônico e SHA-256 de source e
canonical; `W-LEX-0001` cobre somente literals/comments não terminados com
facts semânticos estáveis; os mappings atuais de `W-PARSE-*` preservam
`actual`, `construct`, `expected` e labels com spans. Identity, UTF-8, NUL,
spans e capacity são validados. Lex facts não mapeados e parser internos sem
catalog truth retornam `UNSUPPORTED`; não há claim semântico. O mapping
frontend adicional usa o carrier v9 e mapeia exatamente os 17 codes
`W-SEM-0001`, `W-TYPE-0120`, `W-TYPE-0121`, `W-TYPE-0122`, `W-LABEL-0005`,
`W-LABEL-0006`, `W-MATCH-0001`, `W-MATCH-0002`, `W-MATCH-0003`,
`W-CONST-0001`, `W-CONTRACT-0001`, `W-CONTRACT-0002`, `W-CONTRACT-0003`,
`W-CONTRACT-0004`, `W-GENERIC-0001`, `W-GENERIC-0002` e `W-GENERIC-0003`.
Facts, items e labels são caller-owned, tipados e append-only; o adapter valida
profiles, schemas, UTF-8, sets únicos byte-sorted, groups/order/cardinality de
labels, SourceIds não vazios e únicos, todos os source views, documentos,
spans e counts exatos antes de medir ou escrever. STRING usa `text`, INTEGER
usa `integer_value` e ARRAY/SET usam a faixa de items. O teste prova matrix
17/17, origins cross-document e um documento não referenciado corrompido;
outros diagnostics retornam `UNSUPPORTED`. O snapshot de `W-SEM-0001` mantém
os bytes D0 existentes.

## Build local

Use C23 (C11 recovery explícito), CMake e Ninja. Mantenha o diretório de build
fora do repositório:

    $build = Join-Path $env:TEMP "w-seed-source-reader-build"
    cmake -S compiler/seed-c -B $build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build $build
    ctest --test-dir $build --output-on-failure

O corpus dirigido de lexer também pode ser executado com:

    bun tooling/check-seed-lexer.mjs

O parser seed e os vinte e oito IDs F0 completos (input e output) podem ser
validados com:

    bun tooling/check-seed-parser.mjs

O formatter seed compara os 31 outputs canônicos, reparses e prova a
idempotência, assinatura CST, capacidade e foreign body:

    bun tooling/check-seed-formatter.mjs

O adapter D0 compara os 31 records `W-FMT-0001` byte-a-byte ao snapshot e
valida records lex/parse com JSON.parse e schema/ordem determinísticos:

    bun tooling/check-seed-diagnostic.mjs

O driver interno bounded lê um path `.w`, verifica o frontend seed e exerce o
mapping `W-SEM-0001` em JSONL D0 e no renderer humano. Ele não é o comando
público `w check` e não resolve package ou workspace:

    bun tooling/check-seed-check-driver.mjs

O gate dedicado do scanner C constrói o probe em diretório temporário e compara
32 operações de scan C do corpus FB0, o witness source-backed atual de
`hardware.w` (`unsafe fn<C>`), limites e digest adulterado; sem claim de build:

    bun tooling/check-seed-foreign.mjs

## Scanner de origins de módulo (CHK3)

`include/w_seed_module_scan.h` e `src/w_seed_module_scan.c` formam o scanner
interno de origins usado pelo frontend. A API C23 é caller-owned, não aloca e
não possui estado global mutável. Ela recebe a source, o CST e o parse completo
e mede/escreve, em ordem de bytes, o span opcional do nome de `module` e os
records de imports diretos. Cada record preserva o ordinal do import, índice do
node, span da declaração e span exato do module path. Os estados
`OK`/`CAPACITY`/`INVALID`/`UNSUPPORTED`, capacidade exata, short-by-one e
all-or-nothing são parte do gate.

O scanner cobre `import dep`, paths pontuados, alias `import x from dep.path`,
wildcard e named braces. O frontend reutiliza o helper de span do scanner; não
há uma segunda heurística de token scanning. Parse incompleto, node/link/span
inválido, ordem inválida, boundary UTF-8 ou forma não suportada falham fechado.
Reexport e service-import ainda não possuem CST seed e permanecem gaps; NFC é
responsabilidade futura do resolver.

O gate dedicado é executado com:

    bun tooling/check-seed-module-scan.mjs

## Graph efêmero seed (CHK4)

`include/w_seed_ephemeral_graph.h` e `src/w_seed_ephemeral_graph.c` formam um
builder C23 bounded para o graph efêmero W-1485. A entrada é uma lista de
documentos CST completos, facts de provider já adquiridos pelo caller e um
índice explícito do root. O builder só expande
`W_SEED_MODULE_ORIGIN_IMPORT` produzido pelo scanner CHK3. Ele não abre
filesystem, não descobre owner/provider e não adquire source. Sources, facts,
text views, outputs e todo o scratch são caller-owned.

O profile fixa no máximo 64 sources alcançados, 4096 imports/edges, depth 64 e
16 MiB de source bytes. `measure` e `write` recebem o mesmo scratch explícito
com arrays e capacities fornecidos pelo caller; uma capacity menor retorna
`CAPACITY` sem publicar output. A projeção valida identidade ASCII parser-validada
(NFC completo fica fora desta fatia), header/stem, mapping root-relative,
provider/root/owner, canonical token, snapshots antes/depois e o digest
`SHA-256("w-module-source-v1\\0" || source bytes)`. Ela rejeita missing local,
`std`, aliases canônicos, cycles/SCC e limites excedidos.

O output mantém root ordinal 0, inventory alcançado em byte order, edges
determinísticas com spans/proveniência e a projeção `document_order` mais
`w_seed_frontend_resolved_import` na ordem estrita do frontend. Este builder
ordena edges por mergesort bounded O(E log E) e reconstrói a ordem do frontend
em O(E + S), sem scratch implícito. Este builder não publica recipe ou key.
Provider acquisition/filesystem não pertence a CHK4. Owner detection, NFC
completo, std provider, reexport/service-import, diagnostics e package/workspace
permanecem gaps. CHK9 compõe os adapters e o pipeline para a rota pública
local de root efêmera. O builder CHK4 isolado não abre essa rota.

O gate dedicado é executado com:

    bun tooling/check-seed-ephemeral-graph.mjs

## Provider de source efêmero (CHK5)

`include/w_seed_ephemeral_provider.h` e `src/w_seed_ephemeral_provider.c`
formam a fronteira C23 bounded de aquisição e revalidação para o builder CHK4.
O caller fornece uma root física explícita e uma lista explícita de `SourceId`
root-relative. O core não faz scan, discovery de vizinhos, fetch, fallback,
lookup de cwd/PATH/environment ou detecção de owner. A root física e o
`SourceId` lógico continuam campos distintos.

O backend é injetável e caller-owned. O preflight valida as capacities dos
quatro tokens de cada fase contra a metadata (`required_capacity` deve cobrir
`maximum_emitted_length`); `maximum_emitted_length` é metadata do backend, não
um campo do result. As capacities de staging, revalidação e output são
verificadas com o tamanho observado durante read, revalidate e commit. O
agregado é aplicado como limite durante a aquisição.
Cada sucesso confirma root/provider/owner tokens coerentes, containment
canônico, um token canônico por arquivo e snapshot antes/depois com
`byte_count` e digest. Só depois da revalidação o core publica bytes, `source` e
facts compatíveis com CHK4. Falha, alias, escape, path inválido, UTF-8 inválido,
NFC não comprovado, mutação, truncation/growth ou limite excedido deixa esses
outputs bitwise inalterados.

`include/w_seed_ephemeral_provider_linux.h` e
`src/w_seed_ephemeral_provider_linux.c` fornecem o adapter real testado em
Linux. Ele ancora root relativa em `base_dir_fd` emprestado, root absoluta em
`/`, abre somente arquivos regulares e exige `openat2` com
`RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_XDEV`; sem essa syscall ou
flags retorna `UNSUPPORTED`, sem fallback por `realpath` ou `openat` parcial.
Fora de Linux, a mesma vtable é um stub válido que retorna `UNSUPPORTED` e não
abre handles. O limite padrão é 64 sources e 16 MiB de bytes agregados, com
paths e tokens bounded a 4096 bytes; o caller pode escolher limites menores.

Os testes separados cobrem core fake, capacities exatas/curtas, alias,
mutation na revalidação, snapshots, digest e falhas all-or-nothing. O teste do
adapter cobre root relativa e absoluta, child nested, missing, symlink,
directory/FIFO, hardlink, zero bytes, limites e fechamento de handles. O gate
repete os binários, exige stdout determinístico e stderr vazio, registra
`linux-real=passed` somente com prova Linux e preserva
`SKIP linux-openat2=unsupported` quando a capability não existe:

    bun tooling/check-seed-ephemeral-provider.mjs

O discovery loop interno bounded tem evidência no CHK6 abaixo. NFC completo,
provider std, reexport/service-import, package/workspace, diagnostics
completos e conformance multiplataforma não testada permanecem gaps. CHK9
compõe o provider com CHK6 e CHK7 na rota pública local.

## Driver de descoberta efêmera (CHK6)

`include/w_seed_ephemeral_driver.h` e `src/w_seed_ephemeral_driver.c` formam
um driver C23 interno, bounded e caller-owned para descoberta local iterativa a
partir de uma root e de um `SourceId` root explícitos. Em waves limitadas, ele
compõe a aquisição/revalidação CHK5, parser e module scan, e o graph builder
CHK4. O resultado entrega a um caller futuro os documentos em ordem lógica e
os imports resolvidos. Ele não chama o frontend, não abre a CLI pública
`w check` multi-file e não faz scan de diretório, cwd, PATH, environment, fetch
ou fallback.

O driver separa scratch mutável de outputs publicados e falha fechado. A última
wave estável é a aquisição cujos bytes, CST e facts alimentam o graph, mas waves
sucessivas não constituem uma transação única de snapshot: candidates antigos
podem ser readquiridos, e o CHK4 publica somente nodes alcançados. Os limites
finitos de sources, edges, depth, bytes, rounds e capacities são caller/provider
owned; `std`/`std.*`, NFC completo, package/workspace e owner detection
permanecem fora. A proveniência de capacity do parser é
evidência interna do driver, sem novo mapping de diagnóstico D0 público.

O teste fake backend cobre cadeia transitiva, root header divergente,
discovery determinístico, candidate não alcançado, missing, `std`, não-ASCII,
header mismatch, parse incomplete/issues, cycle, limites, overlap e outputs
inalterados. O adapter Linux cobre a árvore real, child nested, missing e
symlink/escape; fora de Linux o stub retorna `UNSUPPORTED` sem abrir handles.
O gate repete os testes, exige stdout determinístico e mantém a prova real de
Linux ou registra um skip explícito:

    bun tooling/check-seed-ephemeral-driver.mjs

## Aquisição ACQ0 interna e compartilhada

`include/w_seed_acquisition.h` e `src/w_seed_acquisition.c` formam uma camada
C23 standalone, bounded e caller-owned ao redor do driver CHK6. A lane C11 é
somente recovery explícita. A camada não
chama frontend ou D0, não seleciona policy de filesystem ou contexto de projeto
e não publica CLI. Um caller futuro pode chamar o pipeline ACQ0 diretamente.

`w_seed_acquisition_storage` possui as arenas de staging, revalidação,
publicação e nodes CST. O owner não pode ser copiado. Init exige objeto zero;
growth é monotônico, bounded e transacional; destroy é idempotente para zero ou
destruído. O allocator deve produzir allocations não vazias alinhadas e
distintas. Bind valida todos os ranges de storage, arenas, slots, requests e
buffers preservados antes de alterar records.

`w_seed_acquisition_retry_apply` valida o envelope CHK6 completo. Somente
capacities de bytes do provider e nodes do parser crescem. Capacities fixas,
limites, envelope impossível, falta de progresso e allocation produzem outcomes
explícitos. `w_seed_acquisition_pipeline_run` preflighta cada range antes da
primeira chamada e de cada tentativa, então executa o loop bounded
`bind → CHK6 → retry`.

O backend declara o tamanho completo do contexto mutável. O par pointer/size é
canônico e não pode sobrepor outro backing. Callbacks confiáveis escrevem
somente seus out-parameters e esse contexto; ACQ0 não sandboxa callback C
malicioso. Storage e todos os backings exigem acesso exclusivo durante a
chamada. Reentrância, concorrência, growth, destroy e mutation de backing são
proibidos enquanto o pipeline está ativo.

Em toda falha, o output publicado do driver, seus counts e suas cinco arrays
permanecem bitwise inalterados. Storage, scratch e descriptors vinculados podem
mudar. O result terminal registra attempts reais, o último driver result e a
última decisão de retry. Em sucesso, `document_count` e os counts do graph
delimitam os ranges escritos.

Views dependem de source/module IDs, tokens, slots, requests, scratch, graph,
output, contexto e storage. Growth bem-sucedido, nova execução, reuse, destroy
ou mutation invalida views anteriores. Growth que falha ou não faz trabalho
preserva essas views. Somente a última wave estável é publicada. ACQ0 não prova
snapshot global do filesystem entre waves.

CHK9 embute esse storage e acrescenta somente as arenas JSON. O bind do driver
e a lane `DRIVER` de retry delegam às autoridades ACQ0. O `w check` público
continua no retry externo de CHK7 e preserva bytes, exits e renderers. Owner
detection, package/workspace, provider `std`, resolver geral e contexto
público/geral de aquisição permanecem gaps.

O gate compila explicitamente os cinco targets ACQ/CHK9 relacionados, executa
o CTest focal ancorado e exige duas execuções ACQ0 byte-idênticas com stdout
exato e stderr vazio. Ele reutiliza os backends injetáveis e não duplica as
fixtures de filesystem:

    bun tooling/check-acquisition.mjs

O `benchmarkDisposition` é `compiler-lifecycle`. O gate é somente oracle de
correção para `bmd1-seed-check-lifecycle`, célula
`clean × check-end-to-end`; não adiciona stage, timing ou result. `startup` e
`execution` permanecem `product-runtime` deferred.

## Guard OWN0 de candidates build.w

`include/w_seed_owner_guard.h` e `src/w_seed_owner_guard.c` formam o core C23
caller-owned e bounded. O core separa lifecycle e disposition, usa generations
não zero, exige storage disjoint para staging, revalidação e publicação e
publica candidates densos em ordem folha → root. O primeiro fato é observado;
somente uma segunda wave na mesma sessão pode produzir
`CANDIDATES_RECONFIRMED` ou `NO_CANDIDATE_RECONFIRMED`.

Candidate refs contêm apenas generation, ordinal do diretório e índice. Elas
são descritivas e dependem do guard vivo. O backend retém a source, a cadeia de
diretórios e os markers. Begin e revalidate podem refazer o binding descendente
seguro do base até a source pela cópia bounded do path; somente begin descobre a
ancestry ascendente. Revalidation reconfirma cada identity e parent edge, a
root terminal, cada candidate e cada ausência por handles retidos. O core não
aceita reopen textual da ancestry como authority. Falha envenena o guard e
mantém recursos somente para `destroy`.

`w_seed_owner_guard_linux` implementa o backend real com `openat2`, barriers de
symlink/magic-link/cross-mount e identidade que exige
`STATX_MNT_ID_UNIQUE`, device e inode. Somente `ENOENT` no lookup literal de
`build.w` significa ausência. O gate executa Linux nativo e, em host Windows,
exige WSL Ubuntu; skip não é aceito. `w_seed_owner_guard_windows` permanece
incondicionalmente `UNSUPPORTED` fail-closed neste bundle. Os probes de
localidade e parent `..` por handle são somente diagnósticos e não promovem a
capability, mesmo se tiverem sucesso em outro host. Não há fallback textual.

O guard não seleciona owner, não interpreta manifest, não autoriza fallback
efêmero, não é snapshot/lease global e não integra CHK9, `w check` ou `w run`.
Um composer MAN0/WSP0 futuro deve vincular a source da sessão ao token/receipt
ACQ0 antes de compor essas fronteiras. Os testes não provam a ordem reversa
física de closes nem a
matriz geral de mounts, namespaces e volumes.

O gate compila core e adapters com warnings-as-errors, repete os executáveis
host e executa Linux nativo com stdout exato; em host Windows, WSL Ubuntu é
obrigatório:

    bun tooling/check-owner-guard.mjs

O `benchmarkDisposition` é `compiler-lifecycle` somente como classificação da
track futura. OWN0 não integra o `w check` medido por BMD1, e seu gate não é
oracle dessa célula. Não há nova evidência de benchmark, stage, timing ou
result.

## Reader MAN0 guarded estrutural

`include/w_seed_manifest.h` e `src/w_seed_manifest.c` formam o reader C23
caller-owned, bounded e sem heap. Ele lê todos os candidates OWN0 em batch,
faz parse e measure na primeira wave, revalida OWN0 uma vez e relê as mesmas
referências na segunda wave. Length, bytes, bindings e os digests de
backend/core devem coincidir antes de `run`, `verify` e do commit
all-or-nothing. `program_from_output` e verify são read-only nas pontes de
output.

`w_seed_manifest_linux` compõe a sessão retida OWN0 com leitura bounded e
identidade mount/device/inode. O gate root roda depois de OWN0 e exige duas
execuções Linux byte-idênticas, inclusive por WSL Ubuntu no host Windows. A
factory Windows é somente um stub direto `UNSUPPORTED` fail-closed, sem I/O ou
efeitos. Use:

    bun tooling/check-seed-manifest.mjs

MAN0 não seleciona owner, não acopla schema e não integra ACQ0, WSP0 ou produto
público. A classificação geral permanece `implementation-evidence-gap`;
Windows operacional, vínculo ACQ0, schema decoder, WSP0 e produto público são
gaps deste bundle.

## Composição interna BND0

`include/w_seed_source_binding.h` e `src/w_seed_source_binding.c` compõem uma
aquisição ACQ0 completa, um resultado MAN0 guarded e um link preso ao guard
OWN0. A API é caller-owned, bounded e sem heap. Ela valida a sequência inteira,
calcula os digests e a generation, e publica a binding somente uma vez. Toda
failure deixa o destination bitwise inalterado.

`src/w_seed_source_binding_linux.c` aceita somente o provider
`linux-openat2-v2`. O link reconcilia tokens ACQ0 com a identity OWN0 baseada em
`STATX_MNT_ID_UNIQUE`, device major/minor e inode. Provider, token ou adapter
ausente falha fechado. O stub não-Linux retorna `UNSUPPORTED` sem efeitos.

O unit fake cobre `verify`, statuses de link, alias, copy, mutation, limites e
publication all-or-nothing. O gate Linux executa ACQ0, OWN0 e MAN0 reais, testa
generation, tokens e manifests forjados, source incompatível e determinismo:

    bun tooling/check-source-binding.mjs

Em host Windows, o gate exige WSL Ubuntu. BND0 não seleciona owner, interpreta
schema, resolve WSP0, abre `w run`, consulta registry ou implementa backend e
runtime. A classificação geral é `implementation-evidence-gap`, com
subevidência Linux bounded. O `benchmarkDisposition` é `compiler-lifecycle`;
não há timing ou result.

## Composição interna CHK7 — discovery, frontend e D0

`include/w_seed_ephemeral_check.h` e `src/w_seed_ephemeral_check.c` compõem
internamente CHK6, o frontend seed e o adapter D0 em uma API caller-owned
JSON-only. O driver, os records do frontend e o buffer JSON staging são scratch
separados; somente o JSONL final e `jsonl_length` são publicados. A composição
preflighta todos os diagnostics em ordem de `document_index`, `SourceId` lógico e
span. Todo o trabalho falível termina antes do commit: o JSONL é copiado uma vez
para o buffer final e então `jsonl_length` é atualizado, sem novo ramo falível.
Qualquer falha de capacidade, validade, suporte ou I/O deixa o JSONL final e
`jsonl_length` bitwise inalterados.

O teste fake prova que `root` importa e chama um export de `child`, e que `if 1`
em `child.w` produz somente `W-SEM-0001` com source lógico `child.w`, em duas
execuções determinísticas. A API não abre CLI pública, filesystem novo,
provider `std` ou package/workspace; não prova frontend completo nem adiciona
diagnostics além de `W-SEM-0001`.

## Adapter Windows do provider efêmero (CHK8)

`include/w_seed_ephemeral_provider_windows.h` e
`src/w_seed_ephemeral_provider_windows.c` fornecem o adapter Windows real do
provider CHK5. O adapter usa `NtCreateFile` com
`OBJECT_ATTRIBUTES.RootDirectory`, `OBJ_DONT_REPARSE` e
`FILE_OPEN_REPARSE_POINT`. Ele confirma o handle final com
`FileAttributeTagInfo`, obtém identidade com `FILE_ID_INFO` e aceita somente
handles de disco e arquivos regulares.

O adapter primário é C23, caller-owned, não reentrante, sem heap e bounded. A
lane C11 é somente recovery explícita. O
`base_handle` é emprestado e nunca é fechado pelo adapter. Handles próprios são
mantidos em slots com generation e fechados uma vez. O perfil aceita root
relativa e root absoluta drive-local. UNC retorna `UNSUPPORTED`; namespaces,
devices, ADS, drive-relative e outras formas rooted inválidas retornam
`INVALID`. A indisponibilidade da API, do filesystem ou da consulta de
identidade retorna `UNSUPPORTED`, sem fallback por canonical path ou
`CreateFile` permissivo.

O core CHK5 mantém a revalidação e o commit all-or-nothing. Durante a
revalidação, o adapter reabre o mesmo nome relativo e valida root, tipo,
tamanho e identidade. O core compara tokens, bytes e digest da aquisição e da
revalidação e só então faz o commit. O teste cobre roots relativa e
absoluta, nested child, missing, zero byte, UTF-8 físico válido, path inválido,
directory/special, hardlink alias, mutation, replacement, removal, junction
final/intermediário, capacities, tokens, slots, handles e determinismo.

O gate compila os targets Linux e Windows separadamente e executa CTest scoped.
No Windows, `windows-real=passed` exige o teste nativo real. O mesmo gate prova
Linux real via WSL e os stubs Windows e Linux fail-closed cruzados. A saída
esperada inclui `SKIP` somente para symlink sem privilégio, cross-mount ou
`openat2` indisponível. Este adapter é interno. Ele não habilita `w check`
multi-file, package/workspace ou provider `std`.

    bun tooling/check-seed-ephemeral-provider.mjs

## `w check` público para múltiplos arquivos (CHK9)

`cli/check.c` integra `check_host`, storage adaptativo, retry bounded e a
composição CHK6 → CHK7. O target bootstrap `w` aceita uma root explícita em
contexto efêmero e alcança somente imports locais root-relative. A rota não
faz scan de diretório, cwd, `PATH`, environment, URL, stdin ou fetch.

Linux exige `openat2`. Windows exige `NtCreateFile`. Outras plataformas ou
capabilities ausentes falham fechadas. O host fecha somente o handle base que
abriu. A root usa basename ASCII `[A-Za-z_][A-Za-z0-9_]*.w` como `SourceId`.
O core/provider aceita diretório físico codificado em UTF-8, e o gate Windows
prova cwd Unicode; um path Unicode recebido por `argv` narrow não está provado
e permanece gap. Header override altera o module path da root, não o
`SourceId`. Sources filhos usam `SourceId` root-relative.

Os limites bootstrap são 64 sources, 4096 edges, depth 64, 16 MiB por source
e agregado, CST de 32768 nodes por source e 262144 nodes agregados. Source
bytes, CST e JSON staging/final crescem adaptativamente. JSON tem teto de
64 MiB. Cada retry repete CHK6 → CHK7 e permanece bounded.

Exit `0` indica clean. Exit `1` indica diagnostics mapeáveis do subset CHK10.
Exit `2` indica invocation, source, parse, unsupported, barrier, capacity ou
check incompleto. Exit `3` indica allocation, invariant, renderer ou falha de
escrita. JSON preflighta os diagnostics e faz uma única `fwrite` do buffer final.
Human preflighta todos os diagnostics antes do primeiro diagnostic. Uma falha
de escrita pode produzir saída parcial.

O gate público prova o witness single-source de Última Luz e o Restaurant
multifile temporário com child nested, diagnóstico determinístico, source
inalcançado, missing, `std`, cycle, identidade inválida, UTF-8, parse, frontend,
limites de source e graph e escape por symlink ou junction.

O witness público Restaurant de `W-MATCH-0001` prova `missingCases` set
byte-sorted, label `match-subject` source-backed, JSON canônico repetível e
exit `1`. O mapping é uma fatia bounded: frontend normativo completo,
package/workspace, provider `std`, resolução externa, owner detection e codes
fora dos 17 profiles continuam gaps.

CHK9 não fecha owner detection, resolução externa, package/workspace, provider
`std`, NFC completo, identifiers Unicode no SourceId bootstrap,
reexport/service-import no CST seed, diagnostics além do subset, frontend
normativo, compiler, backend ou runtime.

## Frontend seed interno (fatia semântica)

`include/w_seed_frontend.h` e `src/w_seed_frontend.c` formam a primeira fatia
caller-owned do frontend. A API C23 mede antes de emitir e não usa heap,
filesystem, locale, environment ou clock. Ela aceita somente documentos CST
`COMPLETE`; CST `RECOVERED`/fatal cruza uma barreira sem alterar nenhum buffer.
`logical_source_id`, o `module_id` completo pertencente ao resolver e o
`local_module_name` são entradas separadas; o header CST nunca substitui o
`module_id`. Header presente deve coincidir com o nome local, e header ausente
aceita o nome fornecido pelo builder/resolver. Imports externos usam somente
stubs estruturados fornecidos pelo caller (símbolos exportados, parâmetros,
política de labels e retorno).

A normalização preserva módulo, imports e aliases de itens, structs/fields,
enums/cases/payloads, declarações de tipo/alias, funções, parâmetros, entry,
bindings, argumentos e expressions suportadas. Enum declarations produzem um
tipo nominal `ENUM`; conformance é uma superfície de tipo, e generics de enum
geram fato explícito `UNSUPPORTED_TYPE`. A projeção bounded de módulos/imports na
ordem de input detecta duplicate de identidade completa, header/local mismatch,
unresolved import/local e entry inválido, e registra fatos explícitos para
nodes, types e expressions fora do subset. Com
`import_resolution_complete=false`, imports ficam unresolved e preservam os
facts bounded atuais. Com `true`, o caller deve fornecer exatamente um edge por
import direto, em ordem estrita, com target local ou external explícito; o
frontend valida bounds, spans, self-edge, ciclos e exports no target exato. Ele
não compara raw path com module IDs nem encontra stubs externos por path. O checker cobre
Unit, Bool, String, bytes, inteiros e floats fixos, Option, nominais/opaque e
assinaturas de função. Literals,
bindings, returns, calls, condição Bool, aritmética/comparação e widenings
conhecidos têm checagem mínima; narrowing produz `W-TYPE-0122`, condição não
Bool produz `W-SEM-0001` e label inválido de assinatura resolvida produz
`W-LABEL-0005`.

O receipt é texto determinístico com schema interno, digests de source e
records ordenados por documento/ordem de input. Campos textuais usam
comprimento e bytes hex; assim, `|`, newline e identificadores longos não mudam
a separação. `measure` e `run`
produzem a mesma contagem exata; capacidades insuficientes têm comportamento
all-or-nothing. Esta fatia aceita um documento por identidade completa;
contribuições de vários documentos para a mesma identidade são rejeitadas como
`INVALID` em vez de serem mescladas silenciosamente. Formas de import que o
parser ainda recupera (por exemplo, alias de item não reconhecido pelo CST)
continuam unsupported. O teste CHK3 cobre dois documentos, header local
diferente da identidade completa, redirect de um mesmo raw import para targets
distintos e as barreiras de edges incompletos, mal ordenados ou fora de bounds.
Ownership/HIR completo, async/services/providers, avaliação de
initializers/dependencies, cache e materialização, generic calls completas,
heads importados e aplicações de enum/object/type/alias/function, tensor,
runtime, W/MLIR geral e WInterface permanecem fora desta fatia. A ponte MLIR0
terminal bounded é descrita abaixo.

`cli/check.c` compõe o núcleo bounded de source → parser → frontend → D0 na
rota pública CHK9. `tests/check_driver.c` fornece o wrapper da evidência interna
`w_seed_check_driver`. O frontend seed e o driver continuam caller-owned e
aceitam um path explícito de até 16 MiB; o target bootstrap `w` fornece as três
formas de help e a rota pública `w check` em root efêmera local, com imports
alcançáveis root-relative e os limites descritos em CHK9. Package/workspace,
resolução externa, owner detection, provider real de `std`, loader geral e o
frontend normativo completo continuam gaps.

Exit `0` significa que a composição síncrona terminou sem diagnostics. Exit `1`
significa que os diagnostics pertencem ao subset CHK10 mapeável. Exit `2`
representa invocation, source, parse, unsupported, barrier, capacity ou
resultado incompleto. Exit `3` representa falha interna. `--json` faz o
preflight de todos os diagnostics antes de emitir JSONL. O gate público é
`tooling/check-w-check-cli.mjs`.

Funções `const` no D0 conservam a normalização runtime. Literals, parâmetros,
bindings, valores/construtores de enum, operadores já suportados, `switch` e
chamadas diretas a funções locais `const` são const-safe. Uma chamada direta a
função local não-const ou a símbolo externo sem `is_const` produz um único
`W-CONST-0001` no span da chamada e marca `const_body_supported=false`. Um
fato existente `UNSUPPORTED_NODE`, `UNSUPPORTED_TYPE` ou
`UNSUPPORTED_EXPRESSION` dentro do corpo produz o mesmo root, sem alterar os
facts ou diagnósticos existentes. CE0 ainda não fornece ConstIR, evaluator ou
análise de initializer/dependency.

A fatia fechada de enum aceita valores `.case` somente com expected type nominal
enum local inequívoco e aceita `Enum.case` nominalmente. Cases sem payload são
values; cases com payload exigem uma chamada que valida arity, labels e tipos e
retorna o tipo enum. `switch` sobre enum local fechado preserva um owner por arm,
resolve patterns `.case` e `Enum.case`, aceita `_`, exige cobertura exaustiva e
faz um join seguro único dos resultados. Os records de expression, switch arm e
receipt retêm enum/case identity, spans, owner relation, ordem e sentinelas
caller-owned.

Esta fatia implementa somente o D0 executável de subsets locais de enum. A
forma fechada é um alias local `Name = Enum<[.case, ...]>` (também aceita a
forma qualificada `Enum.case`); o enum base deve ser local e inequívoco. A
lista rejeita vazio, duplicatas, cases desconhecidos e qualificadores de outro
enum. O frontend normaliza a lista na ordem declarada pelo enum e colapsa o
conjunto completo para o descritor nominal base (sem records de subset para
essa ocorrência). Para conjuntos próprios, o resultado caller-owned acrescenta
`ENUM_SUBSET`, identidade do enum base, intervalo de membros e records de cada
membro com owner, case e span de origem. `measure`/`run`, capacidade,
sentinelas e receipt repetido permanecem determinísticos. Declarações inválidas
ficam como `UNSUPPORTED_TYPE` fact/barrier explícito; esta fatia não inventa um
código de diagnóstico para elas.

A expressão de membership D0 aceita somente subject enum local inequívoco e
lista parenthesized não-vazia de cases payloadless, em forma curta `.case` ou
qualificada `Enum.case`. A normalização gera `EXPR_ENUM_MEMBERSHIP` com tipo
`Bool` e records caller-owned por case; a identidade dos records segue a ordem
canônica do enum, enquanto cada span preserva a origem no source. Duplicatas,
cases desconhecidos, enum errado, payload ou forma malformada ficam como
`UNSUPPORTED_EXPRESSION` explícito (não usam códigos `W-MATCH`). Um subject de
subset pode listar cases da base fora do subset; o resultado é `false` nesse
caso. A implementação usa scans bounded e suporta enums com mais de 64 cases,
sem bitset.

O expected type aplica o case-set em returns, bindings tipados, chamadas locais
e chamadas externas resolvidas por stub; um case fora do conjunto produz
`W-TYPE-0121`. Subset para base e para superset é implícito; base para subset
não é. `switch` usa somente o conjunto do subject: case fora é
`W-MATCH-0002`, membro ausente é `W-MATCH-0001` e wildcard cobre o conjunto.
Frontend schema `w-seed-frontend-20` preserves enum payload patterns as
structural CST owners and publishes a caller-owned capture relation. Each
capture identifies its switch arm, declaration payload ordinal, name, span,
and resolved type; identifier reads point back to that relation rather than
reparsing source spelling. The current executable seed accepts signed `i64`
captures only. HIR0 still rejects this new record family explicitly, so this
is frontend evidence rather than native payload-enum evidence.

The same schema applies the ordinary W call-binding rule to local enum
constructors: labeled payload arguments may be reordered, unlabeled payload
arguments retain their declaration order, and every accepted argument records
its resolved parameter ordinal. Types never choose between payload slots.

Este D0 não implementa conversão explícita `try Subset(base)`, subsets
importados, aliases genéricos ou empilhados, payload lowering, guards,
switches de tuple/range/struct ou facts completos de fluxo. Literals em enum
switch preservam fato explícito unsupported. As formas sem código normativo
continuam fatos/barreiras explícitos; o seed não apresenta esta fatia como
implementação ampla da linguagem.

## HIR0 verificada do seed

`include/w_seed_hir0.h` e `src/w_seed_hir0.c` formam uma representação
intermediária fechada, bounded, caller-owned e sem heap para o subset inicial.
O lowering copia para a HIR0 os módulos, identidades, tipos `Unit`/`String`,
funções, qualifiers, parâmetros e labels HIR, blocks, ordem, constantes como
byte slices, calls host-prelude ou Unit locais, argumentos tipados com ordens
de source e de parâmetro separadas, requirements,
terminators e entry com target e slot. O programa não retém pointers do
frontend e permanece válido depois que os buffers de source/CST/frontend são
descartados.

A forma inicial W-1494 do schema HIR0 aceitava exatamente um document, um module
e um entry em `.default`. O bounded successor W-1575 aceita um grafo local de
documents resolvido, com um module por document e um único `.default` root entry
no module 0. As ranges de function/entry do module, parameters, blocks, host
parameters/requirements e call arguments/values são partições densas: não há
gap, overlap ou record órfão. `symbols` é um índice auxiliar validado na ordem
module → parâmetros → function → entry; ele não é autoridade para o lowering.
Famílias frontend sem record HIR0 falham fechadas. Labels host required copiam o
nome público, positional usa label vazio e qualquer outra policy permanece fora
deste subset.

W-1519 introduced the binding records in HIR0 schema `w-seed-hir0-2`.
Current schema `w-seed-hir0-9` generalizes that contract. The caller-owned
`w_seed_hir0_binding` record contains `owner_instruction`, `owner_block`,
`ordinal`, `type_index`, `name`, `initializer_value`, `source_span`, and
`is_mutable=false`. `BINDING` carries its binding index. `CALL` carries none.
Every binding owns one initializer root in the common postorder value graph.
`BINDING_READ` carries a valid prior binding index and the same type.

Bindings are present in program, output, counts, capacities, alias tables,
`program_from_output`, receipt, semantic digest, and provenance digest. The
verifier requires owner, order, type, span, dense ranges, prior binding order,
and contiguous value bytes without gap or overlap. Alias barriers remain
fail-closed. Lowering copies binding names and the initializer graph. It never
performs downstream textual lookup.

W-1524 introduced the postorder value graph in schema `w-seed-hir0-3`. Current
schema `w-seed-hir0-9` gives binding initializers explicit roots in that graph,
adds indexed parameter reads, and carries scalar terminator and call results.
The canonical type table contains Unit, String, signed `i64`, and Bool.
`w_seed_hir0_value` is a typed
postorder graph with explicit argument, binary-parent, or interpolation-segment
ownership. `w_seed_hir0_interpolation_segment` discriminates copied text bytes
from an embedded typed value. Parentheses normalize away; arithmetic remains a
binary operation and is not evaluated during HIR construction.

The focused HIR unit lowers `"The answer is ${6 * 7}"` to `i64(6)`, `i64(7)`,
`multiply`, and `interpolated String` values plus two ordered segments. It also
checks short segment capacity, output aliases, graph-edge mutations, segment
ownership, exact bytes, digests, and receipts. MLIR0 and Native0 intentionally
rejected those value kinds at the W-1524 boundary. W-1525 lowers the
panic-free signed-`i64` subset. W-1527 adds constant Bool and
compile-time-known String value segments. W-1528 adds later reads of immutable
`i64`, Bool, and String bindings. W-1529 adds direct Unit calls with `i64`/Bool
parameters, separate source and ABI ordinals, and indexed parameter reads.
W-1530 adds final signed-`i64`/Bool terminator values and direct call results
used by immutable bindings. W-1531 advances HIR0 to `w-seed-hir0-7` and adds
bounded top-level Unit `if` diamonds with explicit `BRANCH`/`JUMP` edges.
Runtime String results, nested calls, runtime panic paths, and general Display
formatting remain outside MLIR0.

Esta HIR0/W-1494 é uma representação bounded mais ampla que a seleção HLO0:
ela pode carregar múltiplas funções e os records correspondentes de blocks,
calls, arguments e values dentro do schema validado. O seletor HLO0 W-1505
aplica a forma mais estreita somente depois de verificar a HIR0.

`w_seed_hir0_verify` recompõe o semantic digest field-by-field com encoding
explícito, sem padding, spans ou provenance. O provenance digest separado
inclui source identity, `module.source_sha256`, comprimento e spans dos records
HIR; o receipt serializa counts, semantic_digest e provenance_digest.
`measure`, `run` e `program_from_output` são all-or-nothing; capacity,
truncamento, alias, overlap, owner, range, type, ordinal, identity,
requirement, terminator, entry ou digest inconsistente falha sem alterar os
buffers do caller. HLO0 chama o verifier na entrada e não acessa source,
frontend, CST ou host scope.

## HLO0 verified-HIR-backed de print-literal

`include/w_seed_hlo0.h` e `src/w_seed_hlo0.c` formam um adapter interno,
caller-owned e sem heap para a primeira fronteira de plano HLO. O fixture
canônico é:

```w
fn main() { print("Hello, world!") }
entry(main)
```

O frontend v11 recebe um `host_scope` explícito, e a etapa anterior faz lower e
verify de HIR0. O profile
`native-process@1` oferece `print(String): ()` como símbolo normal do host
prelude, com requirement nominal `Console`. A resolução preserva identidades
distintas para função local, símbolo importado e símbolo do host; a HIR0 publica
essa identidade e o adapter não infere a origem por índices ausentes nem
procura texto no source ou receipt. Ele também consome qualifiers estruturados
e o record Unit criado quando o retorno é omitido.

`w_seed_hlo0_measure` faz o preflight e mede um plano e receipt. Depois do mesmo
preflight, `w_seed_hlo0_run` copia os dois outputs uma única vez. Capacity,
alias, corrupção do grafo e frontend não concluído não alteram os buffers. Um
grafo coerente fora do subset retorna `UNSUPPORTED`; records incoerentes
retornam `INVALID`.

W-1505 generaliza a rota para o subset print-literal input-driven. Sobre uma HIR0
W-1494 já verificada, o seletor HLO0 exige exatamente um entry `.default` que
aponte para a única função alvo; a função tem zero parâmetros, retorno Unit e é
sync, nonthrows, safe e no-borrow. O corpo tem um block, uma call host-prelude
`print`, um argumento posicional `String` literal, uma requirement `Console` e
retorno Unit. O plano usa schema `w-seed-hlo0-2`, copia `entry_target` e
`handler` como byte strings derivados da HIR0 verificada, não vazios, terminados
em NUL, com zero-tail e igualdade byte a byte, e nunca fixa o nome da função nem
o payload. O verifier de plano isolado comprova somente essa representação e
igualdade; ele não prova source provenance nem que o conteúdo é um identifier
válido.

W-1519 adds one verified immutable local String shape. Frontend v11 resolves
only one unambiguous prior binding by source order and publishes its statement
index. HIR0 emits `BINDING` before `CALL`, then the call reads that binding with
`BINDING_READ`. HLO0 accepts exactly this two-instruction chain in one block.
The HLO0 selector rejects unused, duplicate, forward, nested, shadowed,
cross-read, forged, and mutable binding chains. `var` remains unsupported.

The Restaurant witness `let message = "Table 42 remains open"` followed by
`print(message)` reaches MLIR0, translation, native link, and execution. Its
stdout is exactly `Table 42 remains open\n`. The direct literal shape remains
unchanged, and equivalent binding and literal plans and receipts are
byte-identical at HLO0. HLO0 proves its binding plan independently; MLIR0
consumes the same verified HIR directly under W-1520.
W-1519 has `benchmarkDisposition: compiler-lifecycle`. Its evidence is
correctness-only, with no timing or result.

O payload aceita de zero a 256 bytes e preserva cada byte publicado pela HIR0,
inclusive NUL. O tail não usado é zero. O stdout esperado é payload seguido de
LF, com tamanho checked e SHA-256 sobre essa sequência; exit success é o único
resultado publicado. Isso não é execução W. HLO0 não prova HIR geral, Console
provider W, w-linker, `w run` ou runtime.

```text
bun check --target hlo0
bun check --target seed-frontend
bun tooling/command-runner.mjs --command parse:hlo0
```

O `benchmarkDisposition` deste bundle é `compiler-lifecycle`: a evidência é
correctness-only e não publica timing ou result. O benchmark
`hlo3-hello-world-runtime-benchmark` permanece deferred. C23 é a lane primária;
C11 é recovery explícita. Compile, link, startup e execution W só podem ser
medidos depois de um runner público/pinado com fases separáveis, reproduzíveis,
output e exit verificados.

## HLO1 emissão de artefato C em modo C23 verified-HIR-backed

`include/w_seed_hlo1.h` e `src/w_seed_hlo1.c` consomem um plano HLO0 já
validado e produzem, sem heap, um arquivo C conservador bounded em buffer
caller-owned; o build primário o compila em modo C23.
`measure` e `emit` revalidam o plano completo antes de qualquer escrita. Em
qualquer falha, os records e buffers do caller permanecem inalterados; alias,
capacidade curta, plano corrompido e payload fora do subset retornam status.
O plano isolado não prova sua própria proveniência. Essa prova pertence ao gate
integrado source → parser → frontend → HIR0 → HLO0 → HLO1.

O arquivo emitido começa pelo comentário de schema `/* w-seed-hlo1-1 */` e usa
stdio e um array hexadecimal `unsigned char` com o payload HLO0 seguido de LF.
O source termina em LF. Em `_WIN32`, o adapter CRT acrescenta `<fcntl.h>` e
`<io.h>` e chama `_setmode(_fileno(stdout), _O_BINARY)` antes de `fwrite`; depois
verifica a contagem escrita e `fflush(stdout)`. O buffer C é all-or-nothing,
mas stdout externo não é transacional.

O gate reproduz a rota HIR0 verificada, compila o C gerado em modo C23 em um
diretório temporário fora do repo e compara byte a byte Hello, `Table 42 remains
open` e a string vazia, sempre com stderr vazio e exit 0. Trivia preserva o
artefato. Comentário com `print`, noop, duas calls e formas fora do subset não
produzem C. CMake, Ninja ou compiler ausente produz `SKIP`; falha de toolchain
presente produz `FAIL`.

```text
bun check --target hlo1
```

Este gate é correctness-only e pertence à classificação
`compiler-lifecycle`. Não publica timing ou resultado de performance;
`hlo3-hello-world-runtime-benchmark` continua deferred até existir um runner W
público/pinado com fases separáveis e reproduzíveis. C11 é recovery explícita.

## MLIR0 ponte nativa terminal para LLVM

`include/w_seed_mlir0.h` and `src/w_seed_mlir0.c` form a seed-only adapter that
consumes `w_seed_mlir0_input { program, hir_result, artifact_kind }`. The
header includes HIR0, and the implementation does not include, call, or create
HLO0. A zero `artifact_kind` selects the existing `EXECUTABLE` artifact.
W-1530 advances
MLIR0 to `w-seed-mlir0-9`; W-1531 advances it to `w-seed-mlir0-10`; Native0
remains `w-seed-native0-6`. W-1537 advances HIR0 to `w-seed-hir0-9`, MLIR0 to
`w-seed-mlir0-12`, and the Windows label to `w-seed-mlir0-windows-3`.
W-1538 advances HIR0 to `w-seed-hir0-10`, MLIR0 to `w-seed-mlir0-13`, and the
Windows label to `w-seed-mlir0-windows-4`; Native0 remains v6. W-1539 advances
HIR0 to `w-seed-hir0-11`, MLIR0 to `w-seed-mlir0-14`, and the Windows label to
`w-seed-mlir0-windows-5`; Native0 remains v6. W-1540 advances HIR0 to
`w-seed-hir0-12`, MLIR0 to `w-seed-mlir0-15`, and the Windows label to
`w-seed-mlir0-windows-6`; Native0 remains v6. W-1541 advances frontend to
`w-seed-frontend-15` and HIR0 to `w-seed-hir0-13`; MLIR0, its Windows label,
and Native0 remain unchanged. W-1542 advances only the frontend to
`w-seed-frontend-16`. W-1543 advances HIR0 to `w-seed-hir0-14`; W-1544
advances the current HIR0 schema to `w-seed-hir0-15`. MLIR0, its Windows
label, and Native0 remain unchanged. W-1546 advances the current HIR0 schema
to `w-seed-hir0-16`, adds the private MLIR0 process-handler schema
`w-seed-mlir0-process-handler-1`, and advances Native0 to `w-seed-native0-7`.
The existing MLIR0 Windows label remains `w-seed-mlir0-windows-6`; the
process-handler artifact is distinct from the executable artifact.
MLIR0 re-verifies HIR
through the private `native_subset0` helper. The current path retains the
linear NAT1 form and adds actual labeled LLVM-dialect blocks for bounded
top-level Unit `if` diamonds using `llvm.cond_br`/`llvm.br`. It accepts bounded
String interpolation with checked signed-`i64` `+`, `-`, and `*`, safe constant
`/` and `%`, constant Bool, compile-time-known String values, and
later reads of typed immutable binding initializers. The multi-function path
also emits real internal `llvm.call` operations for bounded acyclic Unit calls
with `i64`/Bool parameters. It also emits typed scalar `llvm.return` and
result-producing `llvm.call` operations for direct call-result bindings.
Source argument evaluation order and declaration slot order remain distinct.
Bool uses exact lowercase ASCII; String values keep their counted bytes,
including NUL. The executable selector still supports HLO0/HLO1/RUN0.

`measure` e `emit` são caller-owned, bounded, sem heap, determinísticos e
all-or-nothing; status, required, written e digest pertencem ao result.
Preflight verifica HIR antes de capacidades e ranges. HIR inválida, forma ou
target não suportado, capacity curta e alias entre descriptors, result, output
ou ranges HIR falham sem alterar result ou output. O texto não tem NUL
implícito. O único target é `x86_64-unknown-linux-gnu`; o módulo fixa
`llvm.target_triple` and contains only builtin and LLVM dialect. The static
path escapes each payload byte and uses POSIX `write`. The interpolation path
uses a bounded stack buffer, a counted text bank, internal LLVM-dialect copy
and signed-`i64` decimal helpers, an on-demand Bool helper, and one checked
`write`. Its generated MLIR contains no `snprintf`, `%ld`, or variadic call.
There is no W-level `printInt`, C source generation, custom W dialect,
TableGen, or object cache.

The separate `PROCESS_HANDLER` mode and its process-entry gate are described
in the W-1546 section below. The executable selector remains the only mode
used by public `w run`.

### ICMP0 signed-`i64` comparisons (W-1537)

The bounded comparison cut uses existing `==`, `!=`, `<`, `<=`, `>`, and `>=`
syntax. Both operands are signed `i64`, and the result is Bool.
`BINARY_I64` names the operand domain. The verifier distinguishes arithmetic
results from comparison results. MLIR emits the corresponding `llvm.icmp`
predicate: `eq`, `ne`, `slt`, `sle`, `sgt`, or `sge`.

The existing binding, Bool argument/return, interpolation, and Unit `if` paths
consume comparison results. This cut does not extend runtime arithmetic or
add logical operators, String/Bool comparisons, mixed operands, mutation, or loops.
Native0 remains v6 with no new record-layout or capacity fields.
Ownership, 64-IF nesting, stdout bounds, and native linking recipes remain unchanged.

`fixtures/restaurant-comparisons.w` requires exact stdout
`Seat party\nSeat party\nWaitlist\n`. The companion composition fixture
checks six predicates, signed boundaries, and Bool result composition.
Six focused C23 suites passed: frontend, HIR0, MLIR0, Native0, ConstIR,
and generic validation. The active native Linux/WSL lane is pinned to LLVM
23.1.1, matching the Windows LLVM 23.1.1 lane, with exact outputs and type
rejection required by the gates.
The frontend fixes align contextual type interning across dry/emit passes,
retain canonical integer-literal `let` typing, and resolve prior bindings within
expression descendants. Existing function, direct-block, and declaration-order guards remain.
The bounded lookup scans do not establish linear-time complexity.
This is compiler-lifecycle correctness
work, not timing, performance, ABI, or cross-compilation evidence.

### BOOL0 short-circuit logical values (W-1538)

The bounded logical cut accepts the existing `!`, `&&`, and `||` operators.
HIR0 `w-seed-hir0-10` verifies typed `VALUE_UNARY_BOOL` and structured logical
diamonds with one Bool block argument at the join. AND carries literal false
from the lhs-false skip arm and evaluates the RHS only on lhs-true; OR carries
literal true from lhs-true and evaluates the RHS only on lhs-false. Incoming
values are explicit Bool operands on the two jumps to the same join. Nested
logic and RHS direct calls with named Bool arguments retain left-to-right,
once-only evaluation and dense caller-owned call-argument ranges.

MLIR0 `w-seed-mlir0-13` emits `llvm.xor` for `!`, `llvm.cond_br` for the
logical branch, and `llvm.br ^join(%operand : i1)` for carried incoming values.
The join declares one `i1` block argument and reads it directly; no stack
temporary or eager RHS is introduced. The logical verifier retains owner,
range, dominance, capacity, alias, receipt, and digest barriers, while normal
CFG and general scalar `if` value flow remain outside this cut. Native0 stays
`w-seed-native0-6` because its published record/receipt interface is unchanged.

`fixtures/restaurant-bool-short-circuit.w` requires exact stdout
`Override checked\nClosed allowed true\nCapacity checked\nOpen allowed true\n`.
Focused HIR0, MLIR0, and Native0 units plus Linux/WSL and native Windows gates
passed the skip/evaluate, nested, RHS-call, and malformed-record boundaries.
This is compiler-lifecycle correctness evidence only: it makes no general CFG,
ABI, cross-target, timing, or performance claim.

### SCALAR-IF0 bounded scalar values (W-1539)

The first scalar-value cut accepts the existing
`if condition { scalar } else { scalar }` form only in an immutable `let`
initializer or a scalar `return`. The condition is Bool, each arm is one
nonnested side-effect-free expression from the existing bounded literal,
parameter or immutable-read subset, and both arms have the same type `i64` or
Bool. Missing `else` retains `W-PARSE-0021`; a non-Bool condition is
`W-SEM-0001`; mismatched arm types are `W-TYPE-0120`. String/enum/aggregate
arms, declarations, calls/effects, nested or `else if` scalar values,
mutation and loops remain unsupported. Runtime `+`/`-` remains behind W-390
checked-overflow semantics and is not admitted by this cut.

HIR0 `w-seed-hir0-11` gives the branch its yielded type: `result_type == 0`
for Unit statement-if, `3` with logical metadata for BOOL0, and `2`/`3` for
scalar `i64`/Bool with logical metadata unset. A scalar diamond has exactly one
typed join argument and one typed incoming from each arm. Verification retains
owner, ordinal, range, read-location, join-target, dominance, capacity, alias,
receipt, digest and all-or-nothing barriers. Incoming spans identify their arm
values, while the enclosing jump may keep the `if` span.

MLIR0 `w-seed-mlir0-14` emits a real `llvm.cond_br`, arm-local operations and
typed `llvm.br ^join(%operand : i64)`/`llvm.br ^join(%operand : i1)`. It does
not emit `llvm.select`, precompute both arms or evaluate an unselected arm.
Native0 remains `w-seed-native0-6`; its public record and receipt interface is
unchanged. Focused frontend14, HIR11 and MLIR14 tests cover i64/Bool return and
immutable-let positives plus malformed and forged-record rejection.

`fixtures/restaurant-scalar-if.w` calls the scalar-returning service with both
`true` and `false` and requires exact stdout `Open 5; closed 2\n`, exit zero
and empty stderr. The public Windows Release route reused the pinned external
cache and passed `bun check --target w-run-windows`; Linux/WSL and C/Rust
scalar-if evidence is not claimed. A local Release build measured its
`w.exe` at 10,078,208 B before post-validation cleanup; the generated tool
artifact was discarded afterward. This is a tool-build fact only, not a
baseline or benchmark of the produced Restaurant executable. This is
compiler-lifecycle correctness evidence, not general scalar CFG, ABI,
target-coverage or performance evidence.

### Nested SCALAR-IF0 tail values (W-1549)

W-1549 supersedes only W-1539's former nesting exclusion. A value block may
end with an unparenthesized nested scalar `if`; the nested `if` is the block's
final value:

```w
fn choose(outer: Bool, inner: Bool, open: i64, middle: i64, closed: i64): i64 {
  return if outer { if inner { open } else { middle } } else { closed }
}
```

HIR0 and Native0 accept at most 64 nested scalar `if` values. Depth 65 fails
before HIR output changes. Conditions are Bool. The root and pure arms yield
the same `i64` or Bool type. Calls/effects, String/enum/aggregate values, `var`,
mutation, loops, `else if`, terminal branch returns and general CFG remain
unsupported. HIR0, MLIR0 and Native0 public record schemas are unchanged.

The Restaurant fixture `fixtures/restaurant-nested-scalar-if.w` has two typed
scalar diamonds and writes exact `1,2,3\n`, exit zero and empty stderr through
`bun check --target mlir0`. Native0 requires zero instructions in every scalar
arm block during recursive traversal; C/Rust equivalents, public Windows
execution, imports, async process entry, other targets and performance remain
gaps. This is compiler-lifecycle correctness evidence only.

### ARITH0 checked signed-`i64` arithmetic (W-1540)

ARITH0 admits runtime signed-`i64` `+`, `-`, and `*` in the bounded
source → frontend → HIR0 → MLIR0 route. At the W-1540 boundary,
HIR0 used `w-seed-hir0-15`. MLIR0 is
`w-seed-mlir0-15`, the Windows artifact label is
`w-seed-mlir0-windows-6`, and Native0 remains v6. Runtime arithmetic calls
LLVM signed-overflow intrinsics. An overflow edge calls the LLVM trap
intrinsic and ends at `llvm.unreachable`, so the process returns nonzero and
does not publish later success output. This proves only bounded process/fault
termination. It does not prove `PanicEvent`, runtime payload, cleanup, or a
general panic runtime.

Existing HIR evaluation retains left-to-right and once-only call behavior.
Checked helpers are emitted only for reachable arithmetic trees. Hello and the
dead-function witness emit no checked helper or dead text. Constant overflow
and faulting constant `/` or `%` are rejected. A safe fully constant `/` or
`%` emits `llvm.sdiv` or `llvm.srem`. W-1551 supersedes only the former
dynamic/runtime `/` and `%` exclusion. W-1552 separately supersedes the
unary-negation exclusion. Power, other widths, named numeric APIs, and general
numeric surfaces remain unsupported.

`fixtures/restaurant-checked-arithmetic.w` uses `entry {}` and
produces exact `Open 6; closed 1\n` on the Linux/WSL 23.1.1 route. No native
Windows evidence is claimed. The bundle keeps caller-owned all-or-nothing,
capacity, alias, receipt, and digest invariants. Its `benchmarkDisposition` is
`compiler-lifecycle`, correctness-only, with no timing or benchmark result.

### Checked runtime signed-`i64` division and remainder (W-1551)

The existing HIR binary value now crosses the native selector with runtime
operands for `/` and `%`. No public record schema changes. Reachable division
uses `w_seed_checked_divide_i64`, which traps before `llvm.sdiv` for a zero
divisor or `i64.min / -1`. Reachable remainder uses
`w_seed_checked_remainder_i64`, which traps on zero and returns zero for
`i64.min % -1` before the ordinary `llvm.srem` path. Safe fully constant trees
retain direct `llvm.sdiv`/`llvm.srem`; invalid constant trees still fail before
emission. Both helpers are omitted when unreachable.

`fixtures/restaurant-runtime-divrem.w` uses two parameterized W functions and
the short entry form. It produces exact `Each 7; left 2\n` through Linux WRT0
and native Windows. The Linux MLIR gate additionally executes zero-divisor and
signed-overflow processes and requires nonzero termination with empty stdout.
The checks do not claim `PanicEvent`, payload/cleanup semantics, other widths
or targets, timing, ranking, or performance.

### Checked signed-`i64` unary negation (W-1552)

HIR0 `w-seed-hir0-17` appends `VALUE_UNARY_I64` with `UNARY_NEGATE`. The
verifier requires a canonical signed-`i64` operand/result and rejects forged
operator, type and ownership records. Safe constant negation emits direct
`llvm.sub`; a runtime operand reuses the reachable checked-subtract helper with
zero on the left, so `i64.min` traps before output. MLIR0 and Native0 artifact
schemas remain unchanged.

`fixtures/restaurant-unary-negate.w` prints exact `Balance -7\n` through the
Linux WRT0 and native Windows public runners. W-1553 separately closes the
direct interpolation-root composition gap; other widths/targets, general panic
payload/cleanup and performance remain outside this bounded cut.

### Direct unary interpolation composition (W-1553)

An interpolation expression no longer inherits the enclosing `String`
expectation. For a representable leading unsuffixed prefix-negative expression,
the frontend applies the canonical signed-`i64` default before it appends the
literal and unary records. The HIR therefore receives one type-consistent
explicit tree, not a late root-only repair, hidden binding or folded text
segment.

`fixtures/restaurant-unary-interpolation.w` uses
`print("Balance ${-7}")`. Frontend and HIR units verify its literal, unary and
segment relations. The MLIR gate emits direct `llvm.sub` without the checked
runtime helper, and Linux WRT0 plus native Windows produce exact
`Balance -7\n`. No public frontend, HIR0, MLIR0 or Native0 record schema changes.

### Straight-line local mutation as SSA (W-1554)

Frontend17 accepts a local signed-`i64` `var` and simple `=` in one linear
block. HIR18 represents declaration and reassignment as distinct binding
versions linked by `source_binding`, `previous_version`, and `next_version`; a later read names
the latest preceding version. The verifier rejects assignment to `let` and
forged version chains before lowering.

`fixtures/restaurant-mutation.w` executes exact `Open 6\n`. MLIR lowers the two
versions to SSA values and introduces no variable `alloca`, `load`, or `store`.
Compound assignment, branch/loop merges, nested mutable scopes, aggregate or
aliased mutation, other widths, and performance evidence remain outside this
bounded cut.

### Conditional mutation through an SSA join (W-1555)

`fixtures/restaurant-conditional-mutation.w` composes the existing scalar-if
diamond with local mutation. A root-block signed-`i64` `var` is readable in
both pure arms; their results feed one typed join argument, and one following
assignment creates the next HIR binding version.

The public Linux/WSL and native Windows runners execute exact
`Open 6; closed 4\n`. MLIR retains `llvm.cond_br`, typed incoming values, and the join argument, with no
source-variable `alloca`, load, or store. Assignment inside an arm, general
dominance, loops, nested or aggregate mutation, aliases, other widths/targets,
and performance evidence remain outside this bounded cut.

### Boolean local mutation as SSA (W-1556)

`fixtures/restaurant-bool-mutation.w` widens the ordered binding-version route
to `Bool`. The replacement is a same-typed parameter read, later code reads the
new version, and MLIR returns the corresponding `i1` value without a
source-variable stack cell. Linux/WSL and native Windows execute exact
`Open true; closed false\n` and retain the bounded Boolean display writer.

Implicit conversions, compound or branch-local assignment, loops, aggregates,
aliases, other types/targets, and performance evidence remain outside this cut.

### Symmetric branch-local mutation as SSA (W-1557)

`fixtures/restaurant-branch-mutation.w` assigns the same root-block
signed-`i64` `var` exactly once in each arm of a top-level statement `if`.
Frontend17 resolves the outer declaration into both descendant branches while
preserving nearest nested shadowing and rejecting sibling access or branch
escape. HIR18 carries both right-hand-side values on the two jumps and creates
one successor binding version from the typed join argument; it does not create
mutually exclusive linear versions.

MLIR emits the conditional diamond and returns the joined `i64` directly.
Linux/WSL and native Windows produce exact `Open 6; closed 4\n`, with no
source-variable `alloca`, load, or store. Missing `else`, unequal targets,
extra statements, calls/effects, Bool/multiple/nested/loop/aggregate mutation,
general dominance, other targets, and performance remain outside this bounded
compiler-lifecycle cut.

### Multiple symmetric branch-local mutations as SSA (W-1558)

`fixtures/restaurant-branch-mutation-multi.w` extends the accepted top-level
statement `if` to a nonempty set of root-block mutable signed-`i64` `var`
bindings. Each root is assigned exactly once in both pure arms; pairing uses
resolved declaration identity, so opposite arm assignment order is accepted.
HIR19 emits one destination block argument and one merged binding version per
root in declaration order. The statement branch remains Unit (`result_type ==
0`), and each predecessor carries the complete typed edge-argument list.

MLIR emits the two join parameters and two typed branch operands directly in
SSA, with no source-variable `alloca`, load, or store. The public Linux/WSL and
native Windows runners execute exact `Open 18; closed -4\n`. Missing, duplicate,
or unmatched targets, same-arm dependencies, calls/effects, nested control,
Bool or mixed-type joins, aggregates, aliases, other targets, and performance
evidence remain outside this bounded compiler-lifecycle cut. Its benchmark
disposition is correctness-only with no timing or benchmark result.

### Ordinary `while` parser/frontend projection (W-1559)

The seed parser now owns `while condition { body }` as a real append-only CST
node. Frontend18 publishes a dedicated `WHILE` statement with a typed `Bool`
condition and one ordered child chain. A root declaration before the loop is
resolved in both the condition and body, and a non-`Bool` condition follows the
existing `W-SEM-0001` diagnostic path.

W-1559 itself is a parser/frontend boundary, not native loop support. At that
milestone HIR19 rejected the record until the loop header and backedge could be
represented and verified together. W-1560 is the separate bounded HIR
successor. No hidden stack cell, host-C loop, or textual lowering is used.
Labels, `break`, `continue`, `while let`, nested loops, native execution, and
performance remain outside W-1559.

### Bounded natural loop in verified HIR (W-1560)

HIR19 now consumes a deliberately narrow successor to W-1559: one ordinary
pre-test `while` per function, one root-block mutable signed-`i64` carrier, and
one pure scalar assignment in the body. It emits a preheader, header, body, and
exit; the header owns one block argument, while the initial and backedge values
arrive through explicit typed edge arguments. The condition and update read
that SSA definition; later exit reads of the root use it as well. The update
creates one ordered successor binding version.

The verifier accepts only this exact natural-loop shape and rejects missing or
substituted edge values, malformed ownership or types, broken binding versions,
and additional backedges. Source barriers reject conditions or updates that do
not use the carrier, multiple body statements, calls, nested loops, and non-i64
roots. This is the W-1560 HIR-only claim. W-1561 separately lowers and executes
this exact shape; general loops, timing, and performance remain pending.

### Structured MLIR natural loop and public execution (W-1561)

NativeSubset0 accepts only the exact verified W-1560 shape. MLIR0 emits one
`scf.while` carrying the signed-`i64` value through `scf.condition` and
`scf.yield`, with no source-variable `llvm.alloca`. The pinned tool recipe uses
`convert-scf-to-cf` and `convert-cf-to-llvm` before translation. Public
Linux/WSL and native Windows `w run` gates execute `restaurant-while.w` and
require exact `Served 3\n`, empty stderr, and zero exit. A non-carried condition
fails closed.

The original artifact record and byte envelope remain unchanged; the current
global schema also contains later independent enum records. The capability
scope is `unit-structured-cfg-natural-loop`. Correctness evidence covers Linux
x86_64 under WSL with LLVM/MLIR 23.1.1 and Windows x86_64 MSVC with 23.1.1.
General or nested loops, multiple carried values, effects, macOS, PGO,
code-size quality, ranking, and general performance remain outside this cut.

### Multi-carrier structured natural loop (W-1569)

HIR0 and NativeSubset0 now admit a nonempty tuple of mutable signed-`i64`
roots in the same bounded four-block natural-loop form. Tuple ordinals follow
root declaration order. Body values and instructions retain source order, so
a later assignment may observe an earlier assignment's new binding version
without changing the physical tuple order. The verifier checks each owner,
ordinal, type, initial/backedge value, and version chain. The existing
caller-owned capacities are the only lane bound.

MLIR0 preserves the tuple in one `scf.while`, with ordered
`scf.condition` operands and `scf.yield` values and no source-variable
`llvm.alloca`. [`restaurant-while-multi.w`](fixtures/restaurant-while-multi.w)
prints exactly `Served 9\n` through the native Windows PE lane and the
Linux/WSL ELF lane. Calls, effects, suspension, nested or mixed control,
non-`i64` carriers, and mutation after the loop remain unsupported. The
executable catalog owns the separate equivalent W/C23/Rust exploratory
measurement; no cross-platform performance claim follows from the two
correctness gates.

### Post-loop SSA continuation after a multi-carrier natural loop (W-1570)

The exit block may contain exactly one pure post-loop `=` assignment to an
existing mutable signed-`i64` loop carrier. Its RHS may use literals,
same-function signed-`i64` parameters, and latest loop results, and it must use
at least one loop result. The function return must depend on the continuation
binding. The continuation uses existing HIR0 binding, instruction, and value
records. HIR0, NativeSubset0, and MLIR0 recheck the SSA links and return
dependency without a HIR schema change or source-variable `llvm.alloca`.

[`restaurant-while-post.w`](fixtures/restaurant-while-post.w) prints exactly
`Final 9\n` through the native Windows PE lane and the CRT-free Linux/WSL ELF
lane. A second assignment, `let` or unrelated target, call, effect, post-loop
control, missing loop-result use, return bypass, nested or mixed control, and
non-`i64` carrier remain rejected. Other targets, ABI/layout, optimization
quality, timing, ranking, and general post-loop mutation remain gaps. The
executable catalog owns separate exploratory W/C23/Rust measurements.

### Structured post-test repeat (W-1574)

Frontend21 preserves `repeat` as a distinct statement. HIR29 admits one bounded
helper with a nonempty tuple of mutable signed-`i64` carriers and pure scalar
assignments. It emits preheader, carrier body, condition, latch, and exit blocks.
The latch owns the updated back-edge tuple because branch records cannot attach
edge arguments to only one successor. The verifier independently checks dense
ownership, carrier order, initial and updated values, version chains, condition
dependence, dominance, and exit projection.

Native0 schema `w-seed-native0-9` records a dedicated post-test fact. MLIR0
schema `w-seed-mlir0-17` and Windows label `w-seed-mlir0-windows-8` lower it to
one structured `scf.while` with a private Bool carrier initialized to true.
Each body trip yields the updated signed tuple and trailing condition. Safe
constant division remains `llvm.sdiv`; dynamic division remains checked. No
source-variable `llvm.alloca` or host-C loop is introduced.

[`restaurant-repeat.w`](fixtures/restaurant-repeat.w) calls the same helper for
zero and 42424 and prints exactly `Receipt digits 1/5\n`. Native Windows emits
a PE x64 artifact; Linux/WSL emits a CRT-free ELF x86_64 artifact. Both require
empty stderr and exit zero. WSL is correctness evidence, not native Linux
performance evidence. Nested/mixed loops, calls/effects in the body, aggregate
or non-`i64` carriers, labels, `break`, `continue`, general CFG, ABI/layout,
other targets, timing, and ranking remain outside this slice. The executable
catalog separately owns exploratory W/C23/Rust measurements.

### Resolved local-document graph in verified HIR (W-1575)

W-1575 closes an HIR-only prerequisite for W-1568. The resolver supplies a
bounded acyclic local-document graph. HIR0 copies one module per document and
keeps exactly one explicit `.default` root entry in module 0. Imported modules
have no entry in this slice.

The focused `w_seed_hir0_multidoc_tests` unit uses an `app` root that imports
`{ helper as h } from lib`, calls exported `lib.helper` through `run`, and keeps
`lib` entry-free. The HIR preflight independently checks import path, target
identity, dense ranges, cycle freedom, and source-span ownership. The verifier
checks the copied module, function, call, identity, range, and root-entry
relations. Forged path, cycle, owner, entry, or call records fail closed.

The unit also checks repeated semantic/provenance digests, frontend-lifetime
independence, output aliasing, and capacity transactionality. This evidence is
limited to verified HIR; W-1576 owns the public local-module product route.
DCE, WMO/WPO, and native artifact equivalence remain outside W-1575. W-1568
remains the stronger future product-equivalence gap.

### ProductClosure0 reachable product projection (W-1576)

`include/w_seed_product_closure0.h` and `src/w_seed_product_closure0.c` form a
bounded, borrowed projection over complete verified HIR0. ProductClosure0
verifies HIR before the reachability walk, keeps all HIR records caller-owned,
and publishes output arrays, source-to-closure remaps, counts, and a reachable
semantic digest transactionally. A capacity, alias, malformed-record, or
unsupported-family failure leaves the caller's published output unchanged.

The projection accepts exactly one `.default` entry at source entry index zero
in module zero. Exported function metadata remains visibility metadata and is
not a product root. The supported domain is the scalar/local-call family with
Unit, String, signed `i64`, and Bool, pure scalar value trees, local function
calls, and the native `print`/`Console` relation. Synchronous, non-throwing,
safe functions without borrow clauses are required.

NativeSubset0 now selects the generic bounded multi-module path. MLIR0 keeps an
independent reachability walk and cross-checks every ProductClosure0 candidate.
`w_seed_native0_run_frontend_graph` is the internal composition boundary for a
resolver-complete local graph: it consumes borrowed ordered documents and
resolved local-import edges, runs frontend normalization, verified-HIR
lowering, ProductClosure0 selection, and MLIR0 emission with caller-owned
storage. `w_seed_check_compile_local_graph` is the bounded CLI acquisition seam:
it confines discovery to the explicit source's parent, acquires the reachable
local graph, and passes the resolver-complete documents to that internal
boundary. The ordinary single-document path remains first and the graph route
requires at least two documents and one resolved local edge.
The focused app→lib source witness retains its reachable `helper`. A synthetic
dead module is omitted, while the reachable digest and emitted MLIR bytes stay
identical. Enum, switch, pattern, external-module, process, effect, service,
reflection, FFI, and dynamic-loading families fail closed.

The physical `fixtures/local-graph/app.w` → `lib.w` witness is covered by public
`w run` and `w build` gates on Windows x64 and Linux/WSL x64; the artifacts
print exactly `answer 42\n`, exit zero, and keep stderr empty. Making the target
private fails before output or artifact publication. This does not establish
package/workspace/provider resolution, multi-document process entry, native
dead-node artifact equivalence, general WMO/WPO quality, or W-1568 completion.
Run `bun tooling/check-hir0.mjs` for the focused HIR/ProductClosure0 units and
`bun check --target w-run-windows` or `bun check --target w-run` for the public
native route. Performance ranking remains outside this correctness evidence.

### Virtual structured-task elision (W-1577)

HIR0 schema `w-seed-hir0-30` represents the bounded local
`let task = async call()` / `let value = await task` pair without creating a
Task object. The accepted result is `Bool` or signed `i64`; both bindings are
immutable and live in the root block; the task has one lexical consumer; and
the local callee is synchronous, non-throwing, and proven never-suspending.

The call result and awaited value are ordinary scalar SSA relations. HIR keeps
only proof metadata (`execution_kind`, launch/await roles, reciprocal peer, and
source-expression ordinal), and the independent verifier rechecks their closed
shape. NativeSubset0/MLIR0 introduce no Task-specific allocation, frame,
handle ABI, TCB, WRT entry, or native symbol. The current lowering is allowed
to execute sequentially; it is not evidence of scheduling overlap.

The public Windows gate executes
[`fixtures/restaurant-async-join.w`](fixtures/restaurant-async-join.w) and
requires exact `Prepared 42\n`, empty stderr, and exit zero. The Linux gate is
wired but is not current evidence when the local MLIR toolchain is unavailable.
Suspending callees, explicit domains, cancellation, arbitration, sharing,
runtime owners, additional result types, and physical Task state remain outside
this bounded slice.

### Explicit-async direct-entry Task elision (W-1578)

The same bounded launch/join relation may target a local explicit `async fn`
when HIR0 proves `directEntry: AVAILABLE` for its ordinary entry. The public
function still records `suspension: MAY`; only the proven call path becomes an
ordinary scalar call. The current preflight accepts non-throwing, non-unsafe,
borrow-free `Bool` or signed-`i64` functions whose body has no `async`,
`await`, or host call and whose ordinary local callees are scalar,
synchronous, acyclic, and within `W_SEED_HIR0_MAX_NESTING`.

Preflight completes before caller-owned HIR output changes. The standalone
verifier rejects a forged direct-entry fact and rejects an async target
relabeled as an ordinary direct call. NativeSubset0 and MLIR0 then emit the
ordinary entry with no Task frame, allocation, WRT dependency, or public Task
identity. The upgraded `fixtures/restaurant-async-join.w` declares
`prepare` as `async fn` and preserves exact `Prepared 42\n` output on the
Windows x64 public route. This remains representation-erasure evidence, not a
suspension, overlap, scheduler, or concurrency claim.

### Virtual Task across one statically discharged yield (W-1579, historical)

Frontend23 recognizes exact `await execution#yield()` as one Unit expression
inside an async function. HIR31 preserves it as a distinct instruction rather
than pretending the function never suspends: the public suspension fact stays
`MAY` and `directEntry` stays absent.

The first product slice is deliberately closed. The local child has one block,
exactly one yield, a `Bool` or signed-`i64` scalar signature, scalar bindings,
and no nested call, control flow, host operation, throwing, unsafe or borrow
surface. Its immutable Task binding has exactly one lexical await in the
launcher's root block. Source preflight and the standalone HIR verifier both
prove this shape before NativeSubset0 accepts it.

Because yield is an opportunity rather than a guaranteed handoff, immediate
resumption is a legal schedule. MLIR0 therefore emits ordinary scalar work and
calls while erasing the Task relation and yield marker. It emits no Task
allocation, frame, TCB, WRT or scheduler call. The Windows public fixture
[`fixtures/restaurant-async-yield.w`](fixtures/restaurant-async-yield.w)
prints exactly `Prepared 88\n`; `tooling/check-mlir0.mjs` also rejects a
product that retains Task, async, yield, or WRT names. This does not prove
fairness, overlap, a scheduler, cancellation, general suspension, Linux
execution, or concurrent performance.

### Virtual Task across finite statically discharged root yields (W-1580)

Frontend24 retains each exact root-level `await execution#yield()` expression
but interns their shared Unit type identity. HIR32 preserves each source marker
as an ordered `EXECUTION_YIELD` instruction. The public async suspension fact
stays `MAY` and `directEntry` stays absent.

The closed proof accepts one or more finite markers in one linear local async
scalar function. It still rejects nested calls, control flow, host operations,
mutation, allocation, throwing, unsafe and borrow surfaces. The immutable Task
relation retains exactly one lexical await. The source preflight and standalone
HIR verifier independently prove the positive marker count and exact
owner/order before the native selector may erase anything.

NativeSubset0 and MLIR0 select an immediate-resume legal serial schedule and
emit ordinary scalar SSA work with no Task, frame, handle, TCB, scheduler, WRT,
or yield symbol. The current
[`fixtures/restaurant-async-yield.w`](fixtures/restaurant-async-yield.w) uses
two markers and prints exactly `Prepared 88\n` on the Windows public route.
This remains call-site-specific representation evidence; it does not claim
fairness, overlap, physical admission behavior, a scheduler, general
suspension, Linux execution, or concurrent performance.

The verified virtual relation has normative zero logical admission cost. It
does not debit task, frame, timer, or ready budgets and cannot synthesize a
budget-exhaustion outcome. A source relation that fails this complete proof
returns to the ordinary physical admission contract rather than reusing this
lowering.

### Proof-directed virtual aggregate materialization (W-1581, design-only)

W-1581 gives `struct`, `enum`, and `object` one aggregate representation rule.
The compiler may erase a value, use SSA or registers, or select stack, fixed,
device, or runtime storage. Object syntax keeps reference and identity-capable
defaults, but an object declaration or `ref` use does not imply a heap, header,
address, or storage class. `struct` and `object` may declare `init` and
`deinit`; enums retain case construction and synthesized payload cleanup. A
custom `deinit` makes the type non-`Copy`, while automatic drop glue does not.
`isSameInstance` may fold without allocation when the identity relation is
proven.

The general rule is not seed implementation evidence. Existing bounded enum
payload and scalar routes remain separate. General aggregate lowering,
materialization, device or runtime residency, FFI, layout, ABI, and lifecycle
support remain implementation-evidence gaps.

### Virtual Task across a same-module scalar helper graph (W-1582)

HIR33 extends the W-1580 `STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED` relation to a
finite acyclic same-module graph of ordinary synchronous pure scalar helpers.
Helpers may use already verified closed scalar control and local scalar
mutation. They cannot use host or external calls, `async`, `await`,
`execution#yield()`, effects, throwing, unsafe code, borrows, allocation, or
runtime owners.

The root remains one linear async block with finite root yield markers and one
lexical join. Standalone HIR verification re-proves graph locality, ownership,
acyclicity, helper admissibility, marker order, scalar types, and the closed
return. NativeSubset0 and MLIR0 emit ordinary scalar calls and erase the
transient Task and yield markers only after complete proof. Zero logical
admission remains conditional on that proof.

The updated `fixtures/restaurant-async-yield.w` source calls `stage`. The
Windows product gate must be rerun for this source change. This remains
compiler-lifecycle correctness evidence. It makes no concurrency, fairness,
scheduler, overlap, or Linux claim.

### Cooperative0 compiler-host trace oracle (W-1583)

COOP0 is an explicitly requested compiler-host oracle and specification-only
profile. HIR34 adds the distinct `COOPERATIVE_TRACE` execution profile and
`STRUCTURED_ASYNC_COOPERATIVE_TRACE` call kind for exactly two sibling scalar
async tasks in one closed root. Each task has one or two
`await execution#yield()` markers and fixed caller-owned frame data. HIR and
the oracle share a 64-function ceiling for the complete cooperative helper
graph. The root keeps launch order, two joins, and its final host print.

The bounded plan and result records remain caller-owned. Execution first
reserves both task slots, then evaluates arguments and publishes each frame to
the deterministic provider/test-profile FIFO queue. Dispatch and resume retain
program counters. Each yield requeues the task, followed by settle, cleanup,
outcome commit, join, and release. Independent plan, trace, and execution
verifiers cover lifecycle, program counter, queue, frame, and outcome
relations, including forged-record, capacity, and alias rejection.

The `fixtures/restaurant-cooperative0.w` oracle witness has four yields and a
thirty-event trace, and requires exact stdout `Cooperative 88\n`. This route
does not emit a NativeSubset0 or MLIR0 state machine and does not provide a
product runtime or executable, scheduler provider, threads, parallelism,
cancellation, I/O, or general Task behavior. It makes no public benchmark or
performance claim. Normal W-1582 static-yield elision remains unchanged. Any
later emitted state machine must be target-neutral; Windows/Linux gates are
evidence lanes, not restrictions on macOS or other viable LLVM targets.

### Cooperative product selection and scalar state-machine core (W-1584)

W-1584 starts with a target-neutral, versioned, reserved, caller-owned selection
proof with schema `w-seed-cooperative-selection0-1`. The record carries
copied HIR indices and admission facts. It is not a task frame,
scheduler record, runtime object, or ABI.

NativeSubset0 independently rederives this narrower one-block product shape
from verified HIR35. It requires one module, one fixed anonymous Unit root,
exactly two ordered scalar async children, one or two
`await execution#yield()` markers per child, and a complete reachable scalar
helper graph of at most 64 functions. After both joins, zero or more host
`print` calls are admitted in the root. Invalid or forged HIR and selection
facts fail closed.

MLIR0 independently reverifies that record and emits
`w-seed-mlir0-cooperative-1`, a target-neutral scalar `func`/`arith`/`scf`
state machine. It carries two logical frames, explicit PCs and completion
states, and returns the joined `i64` result without target triple, data
layout, pointer, allocation, Task object, WRT, process, OS, linker, SDK, or
packaging facts. The hidden unit mode
`w_seed_cooperative0_tests --emit-target-neutral-mlir` writes the exact
verified bytes for MLIR parser/lowering checks.

Selection remains Bool/signed-`i64`; this first state-machine emitter supports
only signed-`i64` task computation and rejects Bool-dependent bodies.

    w_seed_cooperative0_tests --emit-target-neutral-mlir |
      mlir-opt --convert-scf-to-cf --convert-arith-to-llvm --convert-func-to-llvm --convert-cf-to-llvm --reconcile-unrealized-casts --verify-each

This bounded turn policy is not a language FIFO/fairness contract. Normal
W-1582 elision and the COOP0 compiler-host oracle remain unchanged. Process
projection and root output formatting belong to W-1585. Public product routing,
runtime and scheduler ABI, and benchmark evidence remain gaps.

### Cooperative process projections (W-1585)

The separate `W_SEED_MLIR0_ARTIFACT_COOPERATIVE_EXECUTABLE` kind reuses the
verified target-neutral core and derives its output action plan from verified
HIR. The bounded plan accepts static UTF-8 fragments and exactly one dynamic
signed-`i64` value when that value is the ordered sum of the two joined task
bindings. It never hardcodes the fixture output.

The hidden unit modes expose the two current target leaves for parser and
lowering evidence:

    w_seed_cooperative0_tests --emit-cooperative-linux-mlir
    w_seed_cooperative0_tests --emit-cooperative-windows-mlir

Both lower with MLIR 23.1.1. The Windows leaf produced and executed a 3072-byte
CRT-free PE using `mainCRTStartup` and Kernel32, with exact stdout
`Cooperative 88\n`. The Windows-host LLVM tools also produced the Linux ELF
object and the shared Linux x86-64 WRT0 object. Windows-host `ld.lld` links
them into a stripped CRT-free static PIE with no interpreter or dynamic
dependency; Linux/WSL only executes it and observes the same output. This is
complete bounded Windows-to-Linux product evidence for this exact source, not
general SDK/sysroot packaging or a complete host-target matrix.

`w_seed_native0_run` accepts the cooperative artifact kind for internal
product-policy callers and lowers that request with the cooperative HIR
profile. W-1585 itself keeps public `w run` and `w build` on the normal artifact,
while
the same pure witness eligible for W-1582 Task/yield elision. No filename,
stdout, or fallback heuristic forces the cooperative state machine.

The seed target enum currently exposes only the two proven x86_64 leaves; it is
not the W target universe. Feature coverage defaults to every applicable target
in the platform catalog. Missing local hardware or evidence is a blocker, not
permission to exclude macOS, AArch64, mobile, WebAssembly, GPU, embedded, or
another viable target. A real exclusion requires a target-inapplicability
rationale and catalog record. A requested multi-target invocation must not
silently emit only locally executable targets. Release automation must fan the same
verified core to all supported applicable targets, with any supported host able
to build any supported target. W-1586 owns the first bounded public product
selection. Other target WRT/adapters, stable ABI, runtime/scheduler providers,
parallel overlap, cancellation, and ranked benchmark evidence remain gaps.

### Main-domain product dispatch (W-1586 historical exact-two witness; W-1587 current bounded successor)

Frontend25 records `spawn<.main>` separately from ordinary `async`. HIR35
lowers the admitted pair to `STRUCTURED_ASYNC_MAIN_DISPATCH` with the
`MAIN_SERIAL` profile. The independent verifier requires exactly two uniform
launches, one or two yields per scalar child, lexical joins, and the existing
bounded output plan. Another domain or a mixed launch pair fails closed.

Native0 promotes an ordinary executable request to the cooperative executable
only when verified HIR contains the main-dispatch relation. It does not inspect
the fixture name or output. Ordinary pure `async` remains eligible for W-1582
elision.

`MAIN_SERIAL` is derived output, not a caller-selected HIR input profile. One
explicit effective-artifact selector maps the ordinary public executable
request after HIR verification. The separate internal cooperative-oracle
artifact continues to request `COOPERATIVE_TRACE`. Frontend recognition is
wider than this bounded product: unsupported task counts or placement, mixed
launches, synchronous or zero-yield children, and process-root composition
fail closed without defining a language prohibition.

The fixture `fixtures/restaurant-main-dispatch0.w` executes exact
`Dispatched 88\n`. Public Windows `w run` and `w build` produce and execute a
CRT-free PE. The cooperative product gate also lowers, links, and executes the
same source as Windows and Linux x86_64 products from the Windows host.
Public Windows-host `w build --target x86_64-unknown-linux-gnu` now uses that
same pinned target-neutral route and authored WRT0 closure to publish a new
CRT-free ELF; WSL2 supplies target execution only. This does not establish the
native-Linux-host compiler route or general cross-compilation.

The hidden target-leaf modes are:

    w_seed_cooperative0_tests --emit-main-dispatch-linux-mlir
    w_seed_cooperative0_tests --emit-main-dispatch-windows-mlir

Current target leaves are evidence only. Every requested applicable target
must receive an artifact, and release fanout must attempt every supported
applicable target. macOS and other viable targets remain open even without a
local execution machine.

### Main-domain task cardinality (W-1587)

W-1587 extends the physical seed `spawn<.main>` product from the W-1586
exact-two witness to one through four ordered sibling launches in one linear
root. The reserved caller-owned selection record is
`w-seed-cooperative-selection0-2`. Four is its fixed product array capacity,
not a language semantic, HIR38 admission rule, runtime, scheduler, or ABI
bound. HIR38 preserves a larger finite verified relation for later measured
consumers. The Cooperative0 compiler-host trace oracle remains exact-two.

NativeSubset0 independently rederives the count from verified HIR, preserves
lexical order, and zeroes unused selection slots. MLIR0 uses
`w-seed-mlir0-cooperative-2` and
`w-seed-mlir0-cooperative-executable-2`; its serial FIFO state machine starts
at ordinal zero, wraps at the selected count, and left-folds signed-`i64`
outcomes in lexical join order. The target-neutral core remains free of target,
process, OS, runtime, scheduler-provider, and ABI facts.

Focused k=1, k=3, and k=4 checks complement the retained k=2 source witness and
produce exact `Dispatched 20\n`, `Dispatched 66\n`, and `Dispatched 92\n`.
The lower-level external gate samples k=1 and k=4 endpoints, while the C
product path covers k=1 through k=4. Separately, executable workload
`restaurant-main-cardinality` owns the public k=4 fixture and exact `w run`/
`w build` evidence on Windows and on the Linux target through WSL2. The legacy
product selector rejects k=5 without destination mutation; HIR38 does not.
Reordered or orphan/duplicate joins, mixed launch kinds, and a source without a
main-dispatch route fail closed before output publication. The primary
`benchmarkDisposition` remains `compiler-lifecycle`; the executable catalog
separately owns exploratory public W measurement, with C23 and Rust blocked
until equivalent serial-main-domain baselines exist. This bounded correctness
evidence makes no general runtime, parallelism, scheduler, cancellation, stable
ABI, target-adapter, benchmark-ranking, or performance claim, and it changes no
W syntax.

### Verified parallel-domain placement (W-1588)

Frontend28 accepts exact `spawn<.domain>` only when its caller supplies an
exact domain binding with scheduling mode `CONCURRENT` and capability
`PARALLEL`. Mode and capabilities are distinct fields. The input table is
caller-owned product evidence, participates in the frontend receipt, and is
never an ambient runtime catalogue. Missing, duplicate, serial,
capability-free, unknown, and malformed bindings reject before publication.

HIR38 copies the `.domain` identity into its own text storage and records mode,
capabilities, and the distinct
`STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH` relation. Verification remains valid
after frontend and domain input storage are discarded. The current bounded
shape admits a finite sibling scope bounded by verified HIR records, including
the five-task witness, of ordinary pure
non-suspending scalar functions. It rejects async/yielding or effectful
children, host calls, throws, unsafe and borrow clauses, nested tasks, and a
mix of `.main` with `.domain`. Fixed four-slot storage remains only in legacy
PARSEL0 and its bounded downstream witnesses.

This is frontend/HIR evidence, not parallel execution. Native0 now admits the
configured domain and PARSEL0 selects the process root, but MLIR0 does not yet
emit this composed route; the public CLI remains fail-closed, and there is
no provider, overlap, executable-catalog entry, timing, or performance claim.
W-1589 now owns the independent parallel selection. The next slice must prove
the same observable result at provider capacity one and two before adding a
public executable route.

### Bounded parallel selection proof (W-1589)

`w_seed_parallel_selection0` consumes only verified HIR38. Its fixed
`w-seed-parallel-selection0-1` record rederives either the bounded anonymous
Unit root or native-process root and
copies one through four parallel task-call indices, target functions, launch
bindings, lexical join bindings, exact `.domain` placement facts, HIR counts,
and the HIR semantic digest. Unused slots and reserved bytes are zero.

Selection and verification reject non-parallel structured calls, mixed or
forged placement, malformed peer bindings, truncated HIR, and destination
overlap with the HIR descriptor, result, or any HIR backing range. Selection
uses a local candidate and publishes only after every check, so failure leaves
the caller-owned destination unchanged. Verification compares explicit fields
rather than implicit C padding.

Provider capacity is not a field in PARSEL0. W-1590 now runs the same semantic
selection under capacities one and two in a bounded Windows component. PARSEL0
itself has no provider, MLIR, thread, runtime task, public executable, overlap
evidence, or benchmark result; its disposition is `compiler-lifecycle`.

`w_seed_parallel_selection1` is the measured successor used for new scheduler
work. It reports exact task count before publication, accepts caller-owned task
records, copies one dense call/function/instruction/launch/join relation per
task, and binds those records to the HIR38 semantic digest. A five-task source
passes HIR38 and PARSEL1 while PARSEL0 rejects it unchanged. Provider capacity,
workers, frames, cancellation, and Task identity remain outside this record.

`w_seed_parallel_invocation1` consumes verified HIR38 and PARSEL1. It measures
exact task and argument counts, then publishes dense caller-owned relations.
Arguments are stored in declaration parameter order even when named call
arguments use another order. Its canonical digest uses `u32` record counts and
indices after explicit bounds; `size_t` remains a storage-capacity type only.

The shared pure scalar evaluator now resolves parameter values through HIR
relations with a constant-size call frame. It has no fixed arity array, heap,
or variable-length array. Five tasks and a seventeen-argument task pass
PARINV1. PARINV0 still rejects its fixed compatibility limits transactionally.
PARPROV1 and PARMLIR1 consume PARINV1; compatibility products remain on the
fixed chain.

`w_seed_parallel_provider1` is the measured Windows successor. Measure reports
one semantic outcome and one physical workspace value per PARINV1 task. The
platform adapter receives one indexed job descriptor and executes constant-size
waves at capacity one or two. Logical task count does not allocate a job array
on the stack.

Workspace is explicit scratch and can contain partial values after a provider
or task failure. Outcomes, result, and receipt remain unchanged on failure.
Successful capacity-one and capacity-two runs publish identical semantic
digests; the separate receipt records capacity and maximum active workers. A
monotonic ready/release barrier proves overlap without observing a transient
active count. The current C23 test executes five tasks and one
seventeen-argument task. PARMLIR1 now consumes the same measured relations;
public products remain on the compatibility route.

### Measured parallel task-entry MLIR (W-1609)

`w_seed_mlir1_measure_parallel_entries`,
`w_seed_mlir1_emit_parallel_entries`, and
`w_seed_mlir1_verify_parallel_entries` consume verified HIR38, PARSEL1, and
PARINV1. They preserve `w-seed-mlir0-parallel-entry-2` bytes for compatible
inputs while deriving task and argument counts from caller-owned records.

Argument ordering walks verified parameter ordinals directly, so no fixed
arity array, heap, or VLA is required. Result identity binds all three producer
digests plus the artifact digest. The C23 suite covers byte equivalence, five
tasks, seventeen arguments, short capacity, aliases, and forged proofs. The
focused `bun check --target parallel-mlir0` gate additionally lowers the
five-task artifact to Windows x64 COFF and Linux x86-64 PIC ELF using pinned
MLIR/LLVM 23.1.1. Existing function-reachability and artifact-size bounds remain
seed implementation limits. Public process linkage, cancellation, general
scheduling, ABI, benchmarks, and performance remain open.

### Bounded Windows parallel provider component (W-1590)

`w_seed_parallel_provider0` is the first physical PARSEL0 consumer. PARINV0
re-verifies HIR38 and the selection, derives exact call/function/argument-value
facts for signed-`i64` jobs, and dry-evaluates them through the bounded checked
scalar authority. PARPROV0 then executes one through four pure non-suspending
HIR-derived jobs at provider capacity one or two. Its input exposes no callback
or arbitrary context. The implementation is fixed-storage and no-heap.
Capacity two uses Windows x64 Kernel32 threads in waves of at most two; capacity
one is the serial equivalence lane.

The semantic outcome excludes provider kind and capacity and seals lexical
function/value pairs with the HIR semantic digest. The physical receipt is a
separate record containing capacity and maximum simultaneous active workers.
The capacity-two provider uses an internal two-party active-worker rendezvous,
so overlap is proved by two workers being active at once rather than by a
wall-clock threshold. k=1,
k=2, and k=4 exercise the provider; the k=2 witness proves overlap and exact
semantic equality with capacity one.

Both outputs publish only after complete success. Alias checks cover the input,
invocation plan, selection, HIR descriptor/result, and all HIR backing ranges.
Invalid capacity or plan, checked arithmetic failure, provider failure, forged outcomes, and
aliases leave outputs unchanged. Created Windows handles are joined and closed
before publication.

PARINV0 is a compiler-host proof/evaluator, not a retained W runtime
interpreter or emitted W ABI. This host component does not prove CRT-free target
linkage, Linux/WSL support, runtime-dependent input, public commands,
cancellation, a scheduler or worker pool, benchmark measurements, or
performance. Those remain explicit implementation gaps.

### Reachable zero-argument parallel task-entry MLIR module (W-1592, historical)

PARMLIR0 consumes verified HIR38, PARSEL0, and PARINV0 and emits one public
`w_seed_parallel_task_<ordinal>() -> i64` function per selected task. It walks
the argument and body value trees and emits only the reachable direct local
helper closure. Helpers use explicit internal LLVM linkage; unrelated source
functions, the cooperative core, process roots, providers, schedulers, and
runtime symbols are absent.

`w_seed_mlir0_measure_parallel_entries`,
`w_seed_mlir0_emit_parallel_entries`, and
`w_seed_mlir0_verify_parallel_entries` retain the caller-owned,
all-or-nothing contract. Alias checks cover the output and result plus all
verified HIR and proof inputs. The focused `bun check --target parallel-mlir0`
gate lowers the exact target-neutral bytes with MLIR/LLVM 23.1.1 and emits a
Windows x64 COFF object and a Linux x86-64 PIC ELF object in temporary storage.

This is linkable compiler-lifecycle evidence. The wrappers currently retain
compile-time signed-`i64` arguments. No provider calls these symbols yet; no
runtime-dependent process input, standalone executable, CRT-free import set,
cancellation, scheduling, benchmark measurement, or performance result is
claimed.

W-1593 supersedes this zero-argument wrapper shape while retaining the closure,
linkage, transaction, and object-format lessons.

### Runtime-parameterized parallel task-entry MLIR module (W-1593)

PARMLIR0 v2 emits each internal public task wrapper with the exact signed-`i64`
arity derived from its selected function declaration. Parameters and calls use
parameter-ordinal order even when named W arguments are reordered. Launch
values are no longer constants inside the wrappers. Only the transitive
reachable helper closure is emitted and every helper retains explicit internal
LLVM linkage.

`bun check --target parallel-mlir0` proves one one-argument task and one
two-argument task with reversed source labels, omits an unreachable sibling,
checks translated LLVM signatures and calls, and writes valid temporary
Windows x64 COFF and Linux x86-64 PIC ELF objects with MLIR/LLVM 23.1.1.
Launch-argument evaluation remains the responsibility of the future process
root.

The signature is versioned internal compiler-lifecycle surface, not a stable
Task or runtime ABI. Provider binding, runtime process input, a standalone
executable, scheduling, cancellation, CRT-free imports, measurements, and
performance remain gaps.

### CRT-free Windows emitted-entry adapter (W-1594)

PARLINK0 compiles
`runtime/w_seed_parallel_entry_windows0.ll` with the pinned LLVM 23.1.1
`llc`, links it with the W-1593 task object through LLD `/nodefaultlib`, and
imports only Kernel32. The adapter obtains a runtime process ID, invokes the
one- and two-argument generated task symbols on native workers, joins and
closes their handles, and requires the exact relational outcomes `pid + 1`
and `pid + 3`. It contains no C output, CRT, HIR evaluator, external callback,
or copied task body.

The focused command remains `bun check --target parallel-mlir0`; its PE is
temporary and silent. This is private Windows x64 compiler-lifecycle evidence,
not W process input, public execution, Linux provider support, stable ABI,
benchmark data, or a performance claim.

### Process-root parallel HIR composition (W-1595)

PROCPARHIR0 keeps HIR38's record layout and admits one resolved native-process
entry with a pure scalar prelude binding, one `spawn<.domain>` call, its
lexical `await`, and an `ExitCode` return. The focused source makes
`Arguments.isEmpty` feed the prelude and passes its result to the task, so the
verified graph already owns the runtime-input dependency needed by later
target emission. No Task allocation, process-specific task record, or target
fact is introduced.

`bun check --target hir0` covers the positive composition and rejects a second
prelude, an effectful helper, and additional root effects before HIR
publication.

### Native0 domain admission and process-root PARSEL0 (W-1596)

Native0 owns one explicit caller-supplied `.domain` record with concurrent
mode and parallel capability. PARSEL0 uses its unchanged fixed record to
rederive the process entry's one direct scalar prelude binding, physical task
call, launch binding, and lexical join.

The ordinary NativeSubset0 process path now requires every local call it emits
to be DIRECT. The W-1595 parallel dispatch therefore reaches verified HIR and
PARSEL0 but fails MLIR publication transactionally until a dedicated composed
emitter exists. The focused Native0 unit checks unchanged output and result on
that failure. This is compiler-lifecycle evidence without provider linkage,
public execution, benchmark data, or performance claims.

### Parallel direct-call legality certificate (W-1597)

`w_seed_parallel_elision0` rederives a fixed PARELIDE0 certificate from
verified HIR38 plus PARSEL0. The current proof accepts exactly one task, an
immediate lexical join, one virtual-task consumer, and a complete acyclic local
callee graph that is pure, non-throwing, non-suspending, and free of host or
external calls. The record is caller-owned, semantic-digest bound,
transactional, alias-safe, and independently verifiable.

The certificate does not rewrite the parallel HIR call or choose direct-call
emission. A later target optimizer must separately prove that placement and
execution events are unobservable and that its cost model prefers elision.
The provider-backed launch/join route remains the correctness and performance
reference. `bun check --target hir0` covers the positive process graph,
forgeries, aliasing, transactional failure, and the two-task rejection.

### Explicit process-root and task-entry MLIR composition (W-1598)

PARMLIR0 schema `w-seed-mlir0-process-parallel-1` composes the resolved
native-process root with the verified one-task PARSEL0 relation. The root reads
runtime `Arguments.isEmpty`, evaluates one direct scalar prelude, passes that
binding to one task launch, and performs the lexical join. The task wrapper is
private to this composed target module and remains separate from the process
root.

`w_seed_mlir0_measure_process_parallel`,
`w_seed_mlir0_emit_process_parallel`, and
`w_seed_mlir0_verify_process_parallel` keep measurement, publication, and
replay verification separate. All operations are caller-owned and
transactional. The module declares unresolved
`w_seed_parallel_launch_task_0` and `w_seed_parallel_join_task_0` symbols for a
future provider. The ordinary process selector remains DIRECT-only and cannot
silently erase the parallel dispatch.

`bun check --target parallel-mlir0` checks the exact process markers, the
absence of baked task values, MLIR/LLVM 23.1.1 parsing and lowering, LLVM
translation, and a Linux x86-64 PIC ELF object. It does not link a provider or
execute the process artifact. Provider linkage, public execution, benchmark
data, performance, and direct-call selection remain gaps.

### TASKLIFE0 lifecycle reducer and oracle (W-1599)

`w_seed_task_lifecycle0` is a target-neutral semantic kernel with fixed
caller-owned transaction, result, and measurement records. It replays task
states from `UNINITIALIZED` through reservation, publication, activity,
readiness, suspension, body settlement, cleanup, outcome commit, join, and
release. It also replays scope opening, cancellation request, draining, child
drain, outcome commit, join, and retention.

Success, error, and canceled outcomes use explicit tags. Cancellation is
monotonic and control-flow only. A body settled before cancellation keeps its
outcome, while cancellation before settlement commits a canceled snapshot.
Fail-fast cancellation drains unfinished siblings and scope arbitration keeps
the lowest lexical/input error. Cleanup precedes outcome commit. Joins and
releases must follow lexical order.

`run` and `measure` publish exact snapshots only after a complete reduction.
`verify` replays the transaction and checks every result field, copied trace,
and digest. `bun check --target hir0` includes the focused C23 tests for normal
success, suspension, error arbitration, cancellation races, stale or reordered
events, invalid outcomes, capacity, forgery, aliasing, and unchanged-output
failures. The one-to-four task and 128-event limits are seed evidence only.
No source-HIR integration, scheduler, provider, parallel runtime, task ABI,
benchmark, or performance claim is made.

TASKLIFE1 reuses the same reducer with dense pointer/count views instead of
embedded task and event arrays. Semantic counts are u32 on every target;
caller-owned physical capacities are `size_t`. Measure and run use an explicit
task-record workspace, reject byte-count overflow and every input/workspace/
output alias, and publish semantic outputs only after complete reduction.
Workspace is scratch and may change on failure.

The focused suite includes compatible TASKLIFE0/TASKLIFE1 records, a five-task
fail-fast trace whose four unfinished siblings cancel and drain before scope
error publication, and a valid 137-event trace. These are target-neutral
lifecycle witnesses.

PARLIFE1 binds the ordered successful outcomes of a verified PARPROV1
execution to a measured TASKLIFE1 transaction. It derives exact caller-owned
scratch and semantic output counts, emits the complete reserve/publish/body/
cleanup/commit/join/release trace, and folds task values into the scope outcome
with checked signed arithmetic. Provider capacities one and two yield
byte-identical lifecycle output for five tasks. Capacity, alias, producer
forgery, and lifecycle-result forgery fail closed without semantic publication.
Typed physical failure/cancellation, physical interruption, panic
representation, a scheduler, Task ABI, and performance remain open.

The private Windows PLATFORM1 primitive is the next physical boundary. Its
callback returns a canonical tagged completion (`success`, `error`, or
`canceled`) rather than a Boolean or thread exit code. Capacity-two workers use
the monotonic wave rendezvous. After a completed wave contains an error or
cancellation, later waves are not started and receive canceled records; a
sibling already settled in that wave is preserved. The receipt reports started,
settled, canceled-before-start, maximum-active, and cancellation-source facts.

The focused witness produces equal five-task completions at capacities one and
two, exercises an explicit cancellation, and rejects a noncanonical tagged
payload. W-1619 later validates one exact error completion against nominal
HIR41, while provider authentication and TASKLIFE input remain open. Callback
failure is a component failure. Panic/fault containment, physical preemption,
general source-level throw, a scheduler, Task ABI, and performance remain open.

### Bounded CRT-free process/parallel provider linkage (W-1600)

PARLINK1 closes the private physical reference for the W-1598 composition.
The emitted root reserves four aligned `i64` slots for an opaque provider
frame. This is a private seed ABI only: it is not a W Task layout, scheduler
frame, reusable allocation policy, or public runtime limit.

`runtime/w_seed_process_parallel_windows0.ll` obtains bounded command-line
presence through Kernel32, launches `w_seed_parallel_task_0` with
`CreateThread`, joins it with `WaitForSingleObject`, closes the handle, and
validates the task result. `runtime/w_seed_process_parallel_linux0.S` reads
`argc` from the initial x86-64 process stack and uses raw `clone`, `wait4`, and
exit syscalls. Its fixed child stack is private provider evidence and does not
generalize task storage.

`bun check --target parallel-mlir0` emits target-specific process modules,
lowers them with pinned MLIR/LLVM 23.1.1, and links without CRT/default
libraries. It executes empty and nonempty runtime inputs plus a gate-only `!`
provider-failure injection on Windows and Linux/WSL. Success exits zero;
provider failure exits three; stdout and stderr remain empty.

The gate proves this bounded physical route, not public `w build`/`w run`, a
general Windows argument parser, scheduler, task ABI, capacity-independent
storage, retained artifact, benchmark, timing, or performance. The W-1597
direct-call candidate must later be selected by separate target facts and
compared with this physical reference.

### Target-neutral GPU0 and Windows CUDA linkage (W-1601)

`w_seed_gpu0` is a caller-owned, provider-free semantic witness with one host
root, one device kernel, and seven operations. It validates ranges, forbidden
kernel effects, artifact identities, lifecycle phases, aliases, capacities,
and the matching payload carried by its verify/store operation pair, then emits
separate target-neutral host and device MLIR. The canonical fixture payload is
`42`; the fixed shape is evidence-only and does not constrain the language or
ABI.

`bun check --target gpu0` uses one C23 seed build for the source/module,
ACCINV0, ACCBIND0, ACCREQ0, and GPU0 witnesses. It obtains the exact device
artifact, kernel symbol, and expected result from the independently verified
ACCREQ0, parses that artifact with pinned MLIR/LLVM 23.1.1, and lowers it through
GPU/NVVM/LLVM, emits `sm_86` PTX with system Clang 22, and executes it through
a dynamic `nvcuda.dll` Driver API adapter when the provider is available.
Missing-provider and missing-kernel cases fail closed. The adapter takes a
canonical signed-`i32` expected result instead of embedding the sentinel;
malformed expectations fail before provider loading and a valid-but-wrong
expectation proves post-execution comparison. Every produced file is temporary;
the adapter remains a private process boundary rather than a public W runtime
or provider ABI.

`bun benchmark gpu0` refreshes the separate diagnostic snapshot in
`benchmarks/GPU0.md`. Context, module, function lookup, and device allocation
stay outside timing; H2D, launch-plus-synchronize, D2H, and complete round-trip
use 101 warmups and 1001 in-process samples. These numbers are compiler/linkage
diagnostics, not W product rankings.

The seed parser accepts `accelerator.module<{ hello: kernel }>()` and emits
distinct `W_SEED_CST_STATIC_RECORD` and `W_SEED_CST_STATIC_FIELD` owners.
Frontend28 then publishes provider-neutral accelerator-module and ordered
kernel-binding records for nonempty, uniquely labeled static records whose
values are direct same-document functions. Focused tests cover one and multiple
kernels, deterministic receipts, exact ownership, short capacities, empty and
malformed records, duplicate labels, missing functions, and runtime arguments.

`w_seed_gpu_module` is the next target-neutral compiler boundary. It validates
Frontend28 independently, measures caller-owned module/kernel/text/receipt
storage, copies no frontend or source pointer, and publishes separate semantic
and provenance digests. Its verifier works after source, CST, and frontend
teardown and rejects aliases, short capacity, malformed spans and identities,
forged indices or payloads, receipt changes, and digest changes. The fixture
`fixtures/gpu0-module.w` proves the current function-body slice: a direct,
zero-parameter, effect-free signed-`i32` literal return. Multiple module fields
remain representable; this exact body shape is not a language or ABI limit.

`w_seed_gpu0_program_from_gpu_module` is the provider-neutral projection after
that independent verification. It selects one module field, copies the module
const name, field label, and private implementation name into caller-owned
projection text, and carries the verified kernel payload into the exact GPU0
operation pair. The projected program remains verifiable after the bridge
storage is released; no heap or provider/runtime handle is introduced.

`bun check --target gpu0` runs this source bridge and projection through the
ACCREQ0-derived GPU0 artifact/CUDA experiment. This package still does not implement
typed `.launch`, a W runtime/provider, public GPU build/run, a supported GPU
ABI, or a homogeneous pinned production toolchain. Those boundaries keep
roadmap rank 1 open.

### Closed local payloadless enum exhaustive switch (W-1563)

HIR21 (`w-seed-hir0-21`) adds one explicit `SWITCH_ENUM` terminator and dense
caller-owned switch edges. Each edge preserves the nominal enum and case,
canonical declaration ordinal, source span, and arm-entry block; the target
block's `RETURN_VALUE` terminator carries and proves the arm result.
The verifier admits only one dispatch block followed by one direct-return block
per case and rejects incomplete, duplicate, unknown, forged, or cross-function
relations. The bounded switch cannot be mixed with the existing `if`, logical,
or `while` CFG shapes.

NativeSubset0 derives a private minimum logical carrier (`iN`; three cases are
`i2`, with at most 64 cases supported). MLIR0 schema
`w-seed-mlir0-16`/Windows label `w-seed-mlir0-windows-7` emits canonical
`cf.switch` tags and a unique backend-only default block ending in
`llvm.unreachable`; Native0 is `w-seed-native0-8`. Windows x86_64 MSVC and
CRT-free Linux/WSL x86_64 run the exact
[`restaurant-enum.w`](fixtures/restaurant-enum.w) fixture through verified HIR,
MLIR conversion, translation, native link, and execution, requiring
`Courses 10/30/20\n`, empty stderr, and exit zero. The i2 sign-bit tag is
spelled `-2` for the pinned MLIR textual parser while retaining tag-2 bits.

This remains a correctness-only compiler-lifecycle witness. Payload-bearing
cases, general or mixed CFG, public ABI/layout stability, other targets, timing,
ranking, and performance are not implemented or claimed here. W-1571 separately
records the bounded payloadless-subset successor with focused and dual-platform
correctness evidence.

### Bounded local payloadless enum subset switch (W-1571)

W-1571 is the first executable subset successor to the closed enum switch. HIR0
schema w-seed-hir0-28 canonicalizes an alias by the semantic pair (base enum,
normalized base-case indices), not by alias spelling or source list order. A
case-set containing every base case remains the base enum identity and does not
create a subset record. The seed boundary admits only a proper, nonempty,
payloadless subset of one local closed enum; empty, duplicate, unknown,
cross-enum, non-local, payload-bearing, imported, and generic forms remain
outside this implementation cut.

The full base enum remains authoritative for the private carrier width and
declaration tags. Subset members are normalized in base declaration order
without renumbering or narrowing that carrier; a five-case base therefore keeps
i3 and members at base indices/tags 2 and 3 keep those tags. The value stays
scalar: no wrapper, vtable, or enum-specific allocation is introduced. MLIR0
emits one switch edge per normalized subset member in base declaration order,
even when source arms use another order. Its backend-only synthetic
llvm.unreachable default closes the already verified lowered CFG; it is not a
runtime narrowing guard.

Only a proven subset-to-the-same-base widening is a no-op in this slice.
Base-to-subset checked conversion and conversion to an arbitrary superset remain
outside it, rather than becoming unchecked reinterpretation. The same source
and exact Work 1/2\n oracle, with empty stderr and exit zero, are acceptance
targets for a Windows x86_64 PE and a CRT-free Linux/WSL x86_64 ELF. Focused
HIR/native/MLIR checks and both executable lanes pass for this bounded slice.
Its primary disposition is compiler-lifecycle; the executable catalog
separately owns exploratory W/C23/Rust measurements and does not establish
timing or language ranking.

### Enum payload declarations, constructors, and captures (current HIR25)

Current HIR0 (`w-seed-hir0-25`) copies the bounded frontend's signed-`i64`
enum case parameters into a separate caller-owned dense range. Each case keeps
`first_payload`/`payload_count`; each parameter keeps its owner case, ordinal,
type, optional label, and source span. The semantic and provenance digests,
receipt counts, capacity checks, alias barriers, and independent verifier cover
the new records. Cases without payloads keep a zero-length range and add no
payload record.

Local constructors now create one `VALUE_ENUM_CASE` plus a dense
`w_seed_hir0_enum_payload` range. Each payload relation retains both its source
ordinal and its resolved declaration `parameter_ordinal`: child values are
evaluated in source order, while the declaration ordinal determines the closed
sum slot. Named payloads may therefore reorder without type-directed matching
or an effect reorder. Constructor payloads are not represented as calls or call
arguments. The verifier independently rejects missing, duplicated, orphaned,
cross-case, mistyped, aliased, truncated, or digest-forged relations.

Switch edges own dense capture records with source ordinals and declaration
parameter ordinals. Capture reads identify that relation and belong to the
selected arm block. This representation preserves logical payload access without
choosing byte offsets or allocating storage for the enum.
The tests cover reordered arms and labels, positional `_`, labeled trailing
`...`, separate captures in multiple functions, and passing captures to local calls. Resealed
forgeries must still fail ownership, type, slot, and arm-scope verification.
Capture names remain valid after the frontend records and source bytes are cleared.

The native route now accepts and returns these enums through local calls.
Payload fields currently admit `Bool` and signed `i64`. Their private field
sizes and alignments are 1/1 and 8/8 bytes. Each case has its own field offsets.
An enum containing `i64` uses `!llvm.struct<(iN, array<M x i64>)>`, where
`M` covers the largest case byte extent, rounded to eight bytes. A Bool-only
payload enum uses `array<M x i8>` instead. `N` is the minimum supported tag width.
Bool bytes contain zero or one. Packing uses shifts and masks, not pointer tags.
Construction starts with a zero aggregate and writes each case's field offsets
after evaluating child values in source order. Dispatch extracts the tag.
Capture reads decode the selected case's field inside its arm.
The aggregate remains SSA data, without an enum-specific heap allocation or
forced stack slot. Target lowering determines any materialized alignment and
padding. This recipe does not establish a public ABI. The existing payloadless
minimum-width carrier and its artifact bytes remain unchanged.

The Windows and Linux/WSL `w run` gates execute
[`restaurant-enum-payload.w`](fixtures/restaurant-enum-payload.w) as
`Bills 32/44/10/7\n`. It constructs three variants, returns an enum from a
local function, and computes four bills through reordered captures.
The mixed-payload witness
[`restaurant-enum-bool-payload.w`](fixtures/restaurant-enum-bool-payload.w)
produces `States true/false/false/true; charges 17/31; licensed true\n`.
Its four-Bool case shares two `i64` lanes with the larger Bool-plus-i64 case.
Both targets preserve the same observable value. The Windows gate also changes
Bool fields and signed amounts independently.
Native0 tests cover both target adapters and short-capacity atomic failure.
The bundle's primary disposition is `compiler-lifecycle`; its runnable fixture
also belongs to the executable benchmark catalog. Initial live measurements
and C/Rust references use that catalog, not compiler-unit timings.
General payload types, recursive payloads, niches, public payload ABI, and
general mixed control flow remain unsupported. These limits are not syntax
restrictions in the language design.

### Bounded same-module executable product closure (W-1564)

HIR22 (`w-seed-hir0-22`) introduced copying the frontend function `exported`
fact; current HIR25 preserves that contract,
binds it into the semantic digest, and verifies it independently. Export is
module visibility, not an unconditional executable retention root. For the
current one-module executable recipe, `.default` is the product root and local
call reachability retains only its transitive closure.

[`restaurant-wmo.w`](fixtures/restaurant-wmo.w) proves the bounded shape: the
used private `bill` helper is present, while unused exported/private functions,
the `Never served` text, and unrelated checked-divide support are absent from
the raw program artifact. Public native routes require exact `Bill 42\n`, empty
stderr, and exit zero. Package/workspace graph lowering, library export roots,
reflection/FFI/provider/service/dynamic roots, cross-module optimization, and
optimization-quality claims remain gaps.

### Short default entry (W-1541)

The seed parser accepts `entry { statements }` and `entry(functionName)`.
Frontend15 represents the short form as one private zero-argument Unit function
and one `.default` descriptor. The function carries `is_anonymous_entry`. The
entry carries `is_body` and a direct `target_function` index. The internal
`<entry.default>` identity is not source-addressable.

HIR13 introduced copying and verification of those facts. Its text measurement counts the private
name used by both the function and entry target. Parser, frontend, and HIR tests
cover the short shape, measure/emit parity, semantic/provenance digests, mode
forgeries, and duplicate default rejection. The canonical Hello fixture passes
HLO0, HLO1, MLIR0, and public Linux/WSL `w run`. The checked-arithmetic
Restaurant fixture passes with exact `Open 6; closed 1\n`.

This cut does not implement named entries, inline parameters, custom returns,
typed errors, async short entries, native Windows execution, or general runtime
entry adapters. Its `benchmarkDisposition` is `compiler-lifecycle`,
correctness-only, with no timing or benchmark result.

### External process identity in frontend16 (W-1542)

The seed parser and module scanner accept grouped import aliases such as
`Arguments as ProcessArguments`. Frontend16 stores the source alias separately
from a resolver-owned external module/symbol pair on nominal types. A present
pair must identify one exported external `TYPE`; partial or forged pairs fail
closed. Duplicate local aliases are rejected across symbol kinds.

The bounded process fixture admits only the exported constant
`std.process.ExitCode.success` with receiver and return type `ExitCode` and no
parameters. The enum-case expression keeps that member's external identity.
Focused tests cover valid aliases, malformed and duplicate aliases, forged
metadata, deterministic receipts, alias-sensitive provenance, and receipt
capacity preservation.

This is the frontend portion of `PROC-ABI0`, not a complete process ABI. HIR13
did not publish these external identities; W-1543 adds that bounded HIR step.
This frontend cut by itself does not prove
handler compatibility, `directEntry`, argument access, lowering, runtime
input, native execution, Windows, or performance.

### External process identity and handler adapter in HIR14 (W-1543)

HIR14 deep-copies one canonical `std.process` module and exactly four symbols:
the exported `Arguments`, `Context`, and `ExitCode` types plus the exported
constant zero-parameter `ExitCode.success` value. Nominal types and the
external enum-case value retain atomic module/symbol pairs. The verifier checks
the copied names, kinds, export/const state, arity, receiver and return metadata
without consulting frontend or resolver storage.

The entry record carries an explicit native-process adapter kind. The bounded
row accepts exactly one same-module declared async handler with two required
value parameters (`Arguments`, then `Context`), an `ExitCode` return, and a sole
return of canonical `.success`. Wider effects, ownership and body shapes are
unsupported in this seed cut. Alias spelling does not change the semantic
digest, while source spelling remains in provenance.

Counts, capacities, copied text, both external arrays, receipt and digests are
covered by the caller-owned overlap and all-or-nothing barriers. Focused tests
mutate the copied graph and adapter independently. At the W-1543 boundary,
HLO0 and MLIR0 deliberately rejected a valid process HIR without output
mutation. The current private handler mode is described in the W-1546 section
below. `directEntry`, argument access, providers, ABI lowering, runtime input,
native execution, Windows, and performance remained gaps at that boundary.

O gate `bun check --target mlir0` comprova source → parser/frontend → HIR0 → MLIR0 →
`mlir-opt` verify → `mlir-translate` LLVM IR → `clang -x ir` native link →
executable for Hello, Restaurant binding, Restaurant literal, linear output,
empty output, `restaurant-interpolation.w`, the Bool/String Restaurant witness,
a direct-call Restaurant witness, and a scalar-return Restaurant witness. It
also runs the W-1531 Restaurant diamond, separate minimal and no-else
microproofs, and three equivalent correctness-only source-style candidates:
learner (two inline diamonds), idiomatic (`serve(Bool)` called twice), and
frontier (two diamonds calling distinct service helpers). All three candidates
require the exact Restaurant stdout; `frontier` is an exploration role only,
not a ranking or benchmark result. The diamond artifact requires a typed `i1`
condition, `llvm.cond_br`, two arm-to-one-join `llvm.br` edges, both branch
payloads, one post-join body, and real calls. It also executes the checked
runtime integer operators, safe constant `/` and `%`, a negative result, a
literal percent sign, NUL in both text and a String value, and multiple ordered
integer fields. It requires byte-identical MLIR for the equivalent static Restaurant forms,
exact stdout, empty stderr, and exit zero. It preserves
MLIR em trivia e rejeita comentário com `print`, noop, limits excedidos e formas
fora do subset sem artifact parcial. O manifest `tooling/mlir0-toolchain.json` fixa
MLIR/LLVM/Clang/LLVM-config 23.1.1 e a recipe; sua evidência tem status
`current`. Linux/WSL resolve as ferramentas por um root externo persistente
explícito (`W_MLIR0_TOOLCHAIN_ROOT`) materializado a partir do archive portátil
23.1.1, ou pelo `PATH` de um host já compatível; não há caminho versionado
presumido em `/usr/bin`. O archive portátil do runner não contém Clang, então
`check:mlir0` requer um root Clang-capable separado. No checkout Windows
`hostEvidence` é `wsl-linux` e `windowsNative` é `false`, logo a prova não é
suporte Windows nativo. Windows native, macOS, packaging da toolchain, HIR
geral, runtime-produced String or Bool, general Display dispatch, mutable
locals, nested/general CFG and SSA beyond the diamond, W MLIR dialect, MLIR C API
builder, ownership/effects/tasks lowering, optimizer/pass pipeline,
provider/runtime/linker/SDK, o runner `w run` público geral e performance são
gaps; W-1521 fecha somente o subset público seed bounded em Linux/WSL e aponta
NAT1. HLO0, HLO1 e RUN0 continuam bootstrap, auditoria e recovery e rejeitam
multi-call. O bundle tem
`benchmarkDisposition: compiler-lifecycle`, correctness-only, sem timing ou
result.

### Direct-entry facts in HIR15 (W-1544)

W-1544 adds two independent fields to each HIR0 function record:
`suspension` is `NEVER` or `MAY`, and `direct_entry` is `ABSENT` or
`AVAILABLE`. The explicit `async` modifier in the CST supplies `is_async`.
The verifier does not infer the declaration kind from either field.

The bounded gate is source → HIR0 measure/emit → independent read-only
verification. Ordinary pure functions are `NEVER`/`ABSENT`; explicit async
functions remain `MAY` and receive `AVAILABLE` only after a complete-body
`neverSuspend` proof. Local ordinary calls and recursive groups use a bounded
fixed point; unknown hosts, local async calls without call form or summary,
`String` parameters/returns/values, and opaque owners without lifecycle facts
remain conservative. The process handler remains `MAY`/`ABSENT`.

Scratch is 4 KiB under the inherited CST32768 bound, with preflight and
verifier guards, no heap, and no 256-function capacity. The emitter derives
facts before receipt/digest publication; the verifier rederives before
comparison and never commits output. At the W-1544 boundary, HLO0/MLIR0
rejected process HIR. The current private handler mode is described in the
W-1546 section below. The
bounded implementation is source-backed-current through
`hir0_compute_body_never`, `hir0_publish_direct_entry_facts`, and
`verify_direct_entry_facts`; focused C units `test_direct_entry_facts` and
`test_direct_entry_effect_barrier` plus compiler emitter gates pass. The later
`PROC-INPUT0` run is separate and must
reuse one artifact (`missing\n`/exit 2 without arguments;
`received\n`/exit 0 with `-- payload`). No `sync` execution, general ABI/provider/
runtime support, native process execution, Windows, or timing claim is made.
See the canonical contract in
[`DESIGN.md`](../../DESIGN.md) §26.4.1.27.

### Process-owner lifecycle facts and private handler artifact in HIR16 (W-1546)

HIR16 adds closed, independently verified lifecycle facts to each type:
`UNKNOWN`, `VALUE_COPY`, or `ENTRY_ROOT_OWNER`. A separate release fact is
`NONE`, `UNKNOWN`, or `PROCESS_V1_WRAPPER_RELEASE`. Only the exact copied
`std.process@1` external identities for `Arguments` and `Context` receive
`ENTRY_ROOT_OWNER` and the compiler-owned
`stdProcessArgumentsDrop`/`stdProcessContextDrop` `wrapper-release-v1`
contract. Unit, `i64`, `Bool`, and `ExitCode` remain `VALUE_COPY`. String,
unknown, opaque, and foreign types remain conservative.

Each entry also carries a cleanup obligation and exact owner parameter range.
The bounded process shape uses `RELEASE_HANDLER_OWNERS` for its two owner
parameters on the supported normal return. The general contract covers
structured exits, but this seed shape has no throw or cancellation witness.
`hir0_publish_process_lifecycle_facts` publishes the facts before digests, and
`verify_process_lifecycle_facts` recomputes them after structural and identity
verification. Names, adapter identity, `.success`, and generic synchronous
drop assumptions do not grant the proof.

The existing whole-body `neverSuspend` proof remains mandatory. The process
handler may report `MAY`/`AVAILABLE` only for this complete bounded shape.
HLO0 remains closed to process HIR. MLIR0 now has a separate
`PROCESS_HANDLER` artifact mode with schema
`w-seed-mlir0-process-handler-1`; Native0 advances to `w-seed-native0-7`.
The zero-valued `EXECUTABLE` mode preserves the previous artifact bytes.

Native0 resolves the compiler-owned `std.process@1` catalog and selects only
the verified one-module handler. The private handler exposes opaque
`Arguments*` and `Context*` parameters with an `int32` result. It calls
`w_seed_process_entry0_context_drop` before
`w_seed_process_entry0_arguments_drop`, checks both statuses, and traps on a
release failure. It emits no `main`, I/O, or root-finalization operation.
The handler selector does not use function-name spelling and rejects unknown
external identities or unverified HIR.

This artifact remains a test-harness input, not the public process ABI. At the
W-1546 boundary `w run` rejected process entries. W-1547 adds a separate
bounded public executable adapter; it does not widen or replace this private
handler. General `std.process` ABI, argument access, Context capabilities, and
general runtime ABI remain pending.

The fixture, C harness, and command `bun check --target process-entry0` form
the native gate. With the strict MLIR/LLVM 23.1.0 manifest, configured CMake
build, and GCC 13.2 `x86_64-w64-mingw32`, it passes source → frontend →
verified HIR16/Native0 selection → MLIR `mlir-opt` verification → LLVM IR →
`llc` x64 COFF → GCC C-ABI private harness plus the real PROCESS0 provider →
Windows PE execution. The same artifact runs with empty and nonempty
caller-selected CRT byte vectors; the exercised alias/trivia variant emits
byte-identical MLIR. Synchronous, non-success, and missing-entry source cases
reject with exit 1 and empty stdout; omitted/no-op release exposes harness exit
11; stale-generation, reversed-argument, wrong-context/arguments, and
reordered-generated-call cases reach the generated trap (Bun-visible exit 29)
with empty output. The runner deletes temporary artifacts.
Focused scanner, MLIR0, and Native0 unit checks pass in the same configured
build.

This is private-handler evidence, not public process support. It does not
establish native Windows UTF-16 startup-vector behavior or an `argv[0]` policy,
or async/general provider/runtime behavior. Existing HIR lifecycle evidence
remains `source-backed-current` and compiler-lifecycle correctness narrative,
with no language-track timing or result. Separate executable-catalog
measurements use
`bun benchmark run --target process-handler-lifecycle --language w|c|rust` for the private
handler plus shared C harness and PROCESS0 provider: empty/nonempty vectors and
six fault cases are checked before timing successful `[alpha,payload]`. The
runtime interval covers shared CRT startup, harness, provider, and handler, not
handler-only speed; compile timing spans handler/support compilation and final
link, excluding compiler bootstrap. The final artifact is GCC-linked MinGW,
with W/Rust MSVC-origin COFF disclosed and direct-child CPU/RSS limits recorded;
this remains exploratory artifact evidence, not a public process or language-track
result. Benchmark disposition,
blockers, and stop condition remain canonical in [`DESIGN.md`](../../DESIGN.md)
§26.4.1.28; the measurement protocol is in the
[`private process-handler lifecycle benchmark section`](../../benchmarks/README.md#private-process-handler-lifecycle-executable-measurements).

### Public bounded process executable bodies (W-1547, W-1572)

The canonical fixture [`fixtures/process-input0.w`](fixtures/process-input0.w)
remains the smallest public process body. It imports the exact compiler-owned
`std.process@1` identities, keeps `Arguments` before `Context`, reads
`args.isEmpty`, prints one of two literals, and returns `.success` or a bounded
`.failure` status. The public route lowers the verified body through existing
HIR25 and normal MLIR/LLVM records. It does not reconstruct source spellings or
recognize one fixed function shape.

The linked [`fixtures/process-enum-payload.w`](fixtures/process-enum-payload.w)
is the composition fixture. It uses helpers declared before the entry, a local
enum with Bool and signed `i64` payloads, constructor and exhaustive switch
captures, the canonical process read, interpolated output, and
`.failure(7)`. Accepted process bodies are currently one-block returns or
three-block terminal `if` bodies over the bounded normal call graph. Failure
status values must be compile-time constants in `1..255`. Dynamic values and
out-of-range values are rejected without truncation.

Native0 automatically selects `w-seed-mlir0-process-executable-1` for this
verified HIR on `x86_64-pc-windows-msvc` and
`x86_64-unknown-linux-gnu`. Explicit artifact selection uses the same verified
route. W-1565 appends `Arguments.count` to the seven-symbol
public catalog. W-1566 adds its bounded `==`/`!=` comparison form, W-1567
records flat/selective import equivalence, and W-1573 completes the bounded
count-versus-literal operator set with `<`, `<=`, `>`, and `>=`. The private
four-symbol handler remains unchanged.
The generated `mainCRTStartup` captures
`GetCommandLineW`, skips the program token, and retains at most 256 borrowed
UTF-16 descriptors. The descriptor table is a zero-initialized private PE
global rather than a large stack frame, so the `/nodefaultlib` link needs only
`kernel32.lib` and does not acquire `__chkstk`/CRT support. This storage is
single-startup artifact state, not a W runtime ABI or public layout.

W-1572 adds the corresponding CRT-free Linux x86_64 adapter. WRT0 owns the
process-entry assembly and passes the untouched kernel stack pointer to an
ordinary LLVM helper before calling `main()`. The helper publishes only private
`argc`/`argv` accessors. Generated process MLIR excludes `argv[0]`, validates
zero through 256 user arguments, and records borrowed NUL-terminated POSIX-byte
descriptors without copying their contents. The stack and argument bytes remain
live for the process lifetime. The process root derives its encoding from the
selected vector, rather than assuming Windows UTF-16. WRT0 exits through the
x86_64 syscall ABI; the ELF has no CRT, libc, dynamic loader, or `DT_NEEDED`
dependency.

The adapter constructs a private root and distinct `Arguments`/`Context` owner
records, executes the verified body, releases `Context`, releases `Arguments`,
and then finalizes the root. W-1547 admitted only the canonical `isEmpty`
receiver read; W-1565 also admits the canonical scalar `count` read, and
W-1566 admits its bounded equality or inequality predicate. Ordinary copies,
local-call
arguments, enum payloads, and returns are rejected. Direct entry is published
only after the complete reachable body proves non-suspending. Direct text,
signed `i64`, and Bool print values, including known String literal chains, are
admitted without a global String relaxation. The shared callgraph/path proof
caps stdout at 4096 bytes and rejects over-limit 4097- and 8192-byte cases
before publication.

The focused frontend, HIR0, MLIR0, and Native0 CTest units cover positive
composition and fail-closed identity, owner, range, capacity, CFG, and receipt
cases. The pinned Windows LLVM/MLIR/LLD 23.1.1 `w-run-windows` gate passes the
source-to-PE route, and the Linux/WSL gate passes the source-to-CRT-free-ELF
route for the public process fixtures. They execute the bounded
no-argument, empty-argument, payload, and count cases from source or from one
built artifact per product, with exact stdout, empty stderr, exit status, and
cleanup checks. The Linux count artifact additionally proves the 256-argument
boundary and rejects 257 before output. The public `process-entry`,
`process-enum-payload`, and
`process-arguments-count` benchmark lanes
retain their current correctness and measurement evidence in the
[`executable benchmark catalog`](../../benchmarks/EXECUTABLES.md). The catalog
owns artifact and timing cells and cross-language comparability. W-1547 makes
no independent performance or ranking claim.

General argument decoding/indexing, mutable argument storage, general CFG and
loops, throws, cancellation, Context capabilities, general async/provider
runtime, other architectures and OS adapters, cross-compilation, stable public
ABI/layout, and performance remain gaps.

### Public bounded `Arguments.count` (W-1565)

HIR26 reuses `W_SEED_HIR0_VALUE_EXTERNAL_MEMBER` for the exact exported
constant `std.process.Arguments.count: usize` symbol at public ordinal 6. The
producer and verifier require the resolver-owned identity, zero parameters,
no parameter ABI, the actual entry `Arguments` receiver, and canonical scalar
type. HIR represents it with a distinct logical `USIZE` kind. Raw
`Arguments`/`Context` values still cannot bind, escape, enter a local call, or
become an enum payload; only the resulting `usize` is an ordinary copy value,
currently admitted in bindings and direct-print interpolation. General `usize`
arithmetic, ordering, and comparisons outside the bounded count-literal form
remain unsupported. Non-optional
`Arguments` also makes `args?.count` invalid rather than a spelling alias for
`args.count`.

MLIR0 emits `w_seed_process_arguments_count`, which loads the count already
stored in the public process root. The helper does not rescan the command line,
allocate, copy, or suspend. Both startup adapters exclude `argv[0]`, count an
empty argument as one, support 0 through 256 user arguments, and reject the next
argument before publishing the vector. Their verified x86_64 layouts select
physical `i64` for logical `usize`, with no loss in the bounded domain. The
focused Windows and Linux/WSL gates execute the same source for zero, empty,
ordinary multiple, and exactly 256 user arguments. The executable catalog owns
the separate exploratory W/C/Rust measurement lane.

### Bounded public `Arguments.count` comparisons (W-1566, W-1573)

HIR27 advances the schema to `w-seed-hir0-27` and adds a dedicated
`USIZE_COUNT_COMPARISON` value plus a logical `CONST_USIZE` literal. The public
process subset accepts `==`, `!=`, `<`, `<=`, `>`, or `>=` between the exact resolver-owned
`std.process.Arguments.count` member on the entry's real `Arguments` owner and
a nonnegative unsuffixed integer literal. Either operand order is valid. The
result is `Bool`.

General `usize` arithmetic and comparisons outside this count-literal form
remain unsupported. Helper function
parameters and returns of type `usize` remain unsupported too. Negative and
computed literals, owner escape, and forged
identity or type metadata fail closed. Raw `Arguments` and `Context` values
remain non-lowerable.

HIR retains logical `usize`. NativeSubset0 and MLIR0 recheck the relation, and
the verified Windows and Linux x86_64 layouts use physical `i64` only at the MLIR
boundary. Ordered count predicates use unsigned `ult`/`ule`/`ugt`/`uge`;
ordinary `i64` predicates remain signed. The helper reads the existing process-root count without scanning,
allocation, copying, or suspension. Focused HIR0, MLIR0, and Native0 tests plus
the pinned Windows and Linux/WSL gates cover the accepted predicate and bounded
runtime count cases.

### Flat and selective `std.process` imports (W-1567)

[`fixtures/process-arguments-count.w`](fixtures/process-arguments-count.w) is
the canonical flat-import witness. It uses `import std.process` with direct
`Arguments`, `Context`, and `ExitCode` names. The selective witness
[`tests/fixtures/process-arguments-count-selective-import.w`](tests/fixtures/process-arguments-count-selective-import.w)
uses grouped local aliases and the same body.

The frontend and HIR accept one flat import item or three selective items. Both
forms produce the same semantic HIR digest. Their provenance digests differ by
source spelling and spans. Under the same target and profile, the pinned
Windows x86_64 gate builds byte-identical PE images and runs both images with
zero, empty, ordinary multiple, and exactly 256 user arguments. It checks exact
output, empty stderr, exit status, and cleanup for each image. The direct link
uses `/Brepro`, so the byte comparison is independent of COFF timestamp timing.

This is bounded same-module process evidence. It does not establish general
cross-module import equivalence or WMO/WPO.

### PROCESS0 provider kernel (post-W-1546)

`include/w_seed_process0.h` and `src/w_seed_process0.c` provide a small
seed-only, caller-owned process-input kernel. The caller supplies an explicit
selected vector with one encoding: length-delimited POSIX bytes or UTF-16
units. The kernel preserves order, empty arguments, malformed UTF-8 bytes, and
lone UTF-16 surrogates without copying or discovering an OS startup vector.
`argv[0]` remains a caller-selection policy. The descriptor table and backing
storage must stay immutable and live until root finalization.

Root initialization validates ranges and destructive output overlap before
publishing two distinct root-scoped wrappers. `Arguments` exposes only count,
borrowed get, and exact native contains. `Context` is intentionally a zero-
authority wrapper with no projections, tasks, providers, or I/O. Each wrapper
drop is one-shot and invalidates only that wrapper. Stale copied records and
generations are rejected while the C storage contract holds.
Root finalization is separate and requires
both wrapper obligations to be released. Borrow views end before finalization
or reuse. This is provider correctness evidence, not W execution or the public
`std.process` ABI. Strict W-text conversion, compiler binding, public native
entry/root adaptation, OS-root acquisition, and runtime lowering remain
pending.
The root and wrapper records remain address-stable through finalization.
They do not define the future movable W-value ABI. Pointer provenance, borrow
lifetimes, and exclusive mutation remain explicit C-caller obligations.

`tests/test_process0.c` covers synthetic POSIX/UTF-16 vectors, invalid bytes,
lone surrogates, empty and out-of-range access, kind and generation barriers,
all-or-nothing malformed/overlap initialization, and an explicitly selected
`argc`/`argv` slice. Its `benchmarkDisposition` is `deferred`: this package
publishes no timing. The deferred task id is
`process-arguments-native-access-benchmark`. Its blockers are argument-access
lowering, a native entry/root adapter, and an executable benchmark runner. The
stop condition is one compiled W program reading runtime arguments under
matched C/Rust provider, semantic, and optimization profiles. W measurements
then cover learner, idiomatic, and frontier forms. Existing C correctness
tests are not that benchmark.

With the existing CMake build directory configured, run the focused checks:

```text
cmake --build build --target w_seed_process0_tests
ctest --test-dir build -R w_seed_process0 --output-on-failure
```

The compiler suite also discovers these CTests through the existing seed
source-reader gate. No separate command registry entry is required.

### Native Windows x86_64 candidate (W-1532)

The same MLIR0 subset has bounded native evidence for target
`x86_64-pc-windows-msvc`. The pipeline is
`mlir-opt.exe → mlir-translate.exe → llc.exe → lld-link.exe + kernel32.lib`;
it does not use Clang, the CRT, or WSL. The artifact uses `GetStdHandle`,
`WriteFile`, and `ExitProcess`, with `mainCRTStartup`, the console subsystem,
and `nodefaultlib`. The runner uses explicit paths from the materialized
manifest, `CreateProcessW`, `CREATE_NEW` temporaries, and all-or-nothing cleanup.

To keep the development cache outside the repository:

```text
bun tooling/command-runner.mjs --command acquire:mlir0-windows                         # network is an explicit opt-in
bun tooling/command-runner.mjs --command build:w-windows                               # release, primary C23
bun tooling/command-runner.mjs --command build:w-windows -- --c11-recovery              # release, explicit C11 recovery
bun tooling/command-runner.mjs --command build:w-windows -- --profile development --c11-recovery      # Debug, explicit C11 recovery
bun tooling/command-runner.mjs --command build:w-windows -- --profile size-experimental --c11-recovery # MinSizeRel
```

The 23.1.1 archive requires a 1 GiB Zstandard decode window on this host.
Network acquisition therefore downloads a separately pinned 1.5.7 `zstd.exe`
bootstrap from the same immutable release, verifies its size and SHA-256,
extracts only that single regular file, and deletes it with the owned download
workspace after atomic toolchain materialization. Offline acquisition may use
`--zstd <explicit-path>`; PATH lookup and an unbounded decompression fallback
remain forbidden.

`build:w-windows` discovers Visual Studio through `vswhere`, probes the Windows
SDK explicitly, and does not copy the heavy toolchain. The default `release`
profile maps to CMake `Release`. The `development` profile maps to `Debug`.
The `benchmark` profile maps to a recipe-constrained `Release`, requires a
clean Git worktree, records HEAD, and probes `/options:strict`, `/Brepro`,
`/pathmap:<build>=B`, and `/pathmap:<workspace>=W` before the build. The build
uses the same space-separated compiler fragment, with the specific build map
before the broad workspace map, and uses `/Brepro /WX` for the linker. The
receipt records normalized options and mappings. This bounded recipe evidence
does not claim a reproducible binary or a double-build result. The
`size-experimental` profile maps to `MinSizeRel` for size comparison only.
These are toolchain profiles. They do not add a profile option to `w run`,
`w check`, or another W command. The
`bun check --target w-run-windows` gate proves Hello, Restaurant/if, interpolation,
linear output, a forwarded empty argument, invalid source without stdout, and
an x64 PE. The cache has role `development-and-release-only`,
`bundledWithW: false`, and its extracted size is not a W package budget. This is
candidate evidence, not general support. Unicode source paths, the general
ABI/runtime, packaging, CI, cross-compilation, and other targets remain gaps.
The builder preserves C23 as the request and maps it to the MSVC
`/std:clatest` preview lane. The receipt labels this lane
`c23-msvc-preview`, correctness-only and not a final C23 result. The current
local evidence uses the explicit `--c11-recovery` option. There is no implicit
standard fallback. The builder reads each fixture before execution,
records its SHA-256, and runs exact Hello and Restaurant smokes from the staged
executable before it atomically installs `build/w-windows/w.exe` and
`build/w-windows/receipt.json`. The receipt is local evidence only, and is not
a package, budget, or performance proof. A HEAD change during the build is
rejected.

NAT1 accepts exactly one module, function, `.default` entry and block. The
function is linear, returns Unit, has no parameters or effects, and the block
contains 1..32 instructions made only of 0..32 immutable String bindings and
1..32 ordered `print` calls. Every binding is read at least once; repeated
reads are allowed. Instruction count equals bindings plus calls. Each argument
is a direct String literal, a read of a prior binding, or a bounded
interpolated String. Each static payload is at most 256 bytes and ordered
stdout (payload plus LF per call) is at most 4096
bytes. MLIR0 may coalesce the pure calls into one global/write while preserving
bytes and W order, without promising syscall boundaries. The static artifact
retains its original 13190-byte derivation. The adapter capacity is now
`W_SEED_MLIR0_MAX_BYTES = 98304` for bounded value operations and runtime
formatting. Measure and emit remain
all-or-nothing with alias and digest invariants. HLO0/HLO1/RUN0 remain
single-print.

The interpolation extension accepts at most 64 HIR values and 64 segments.
It proves constant integer trees panic-free without substituting their result.
The emitted artifact preserves the matching LLVM arithmetic operation.
Constant Bool uses `true` or `false`; a literal or prior immutable String
binding contributes exact counted bytes. Counted text preserves NUL and
percent bytes. User-defined Display, nonconstant Bool, and runtime-produced
String values remain outside this implementation cut. This is not a language
restriction.

```text
bun check --target mlir0
```

## Public bounded `w run` (W-1521, NAT1 extensions through W-1527)

The public seed command is limited to:

```text
w run <explicit-path.w> [-- <args...>]
```

It accepts one explicit `.w` path and non-empty valid UTF-8 source up to 4096
bytes. It does not discover source recursively, from cwd or PATH, or through
imports, packages, workspaces, registries or network. Native0 is caller-owned
and no-heap; the logical source id is the opaque basename supplied by the
caller, including hyphens and the terminal `.w`, rather than a W identifier or
module name. The direct route is
`source → parser/frontend → verified HIR0 → MLIR0 → mlir-opt →
mlir-translate → llc → native host link`. HLO0, HLO1 and RUN0 are not
prerequisites. `llc` emits position-independent program and WRT0 objects. The
absolute native linker produces a static PIE with compiler-owned `_start`,
stdout write, and exit adapters. The final ELF has no `PT_INTERP`,
`DT_NEEDED`, CRT, or libc. This path generates no C source and does not require
Clang.

The gate checks LLVM versions separately from native-linker provenance. A
Linux native run is compile-time opt-in. The default `OFF` build returns 2 from
`w run` without launching a tool. An `ON` build requires five existing absolute
executable paths and a native x86_64 ELF target. The linker's `-V` result must
advertise `elf_x86_64`. CMake accepts spaces and rejects quotes,
backslashes, semicolons, and control characters before generating the header.
A private `/tmp/w-run-XXXXXX` directory uses mode 0700 and fixed files use modes
0600/0700. The runner uses `execv` without a shell and cleans every path
on all returns. Arguments after `--` are forwarded byte-for-byte, and the
child inherits stdout/stderr. Normal exit is propagated; signal exit is
`128 + signal`. Invocation, source, unsupported and missing-tool errors return
2; internal, I/O or cleanup errors return 3. `--entry` and `--offline` are
rejected. The bounded native Windows route is a separate W-1532 candidate;
macOS, the general runner and performance remain gaps. The gate is
compiler-lifecycle correctness evidence only.

The NCI1 Linux 23.1.1 archive passed local size and SHA-256 verification and
was materialized in the persistent external WSL cache; its extraction contract
also validates archive paths and executable tool paths.
Extraction uses Zstandard long-window support. The archive's missing Clang
driver does not block the public runner's separate object and link stages.
The CI workflow adds mandatory native Linux and Windows jobs. Neither remote
job has run. These jobs do not promote general platform or cross-target support.
The `check:mlir0` recipe remains separate and unchanged in shape, but uses the
same exact 23.1.1 manifest and requires a Clang-capable external root.

The active local WSL lane resolves LLVM 23.1.1 tools from
`W_MLIR0_TOOLCHAIN_ROOT`, not versioned `/usr/bin` names. The public runner
uses host GCC/cc 13.3.0 and target `x86_64-linux-gnu`; its gate checks exact
output, stage failures, missing tools, restored execution, and cleanup.
The host compiler builds the C seed separately from LLVM object generation.
Run `bun tooling/command-runner.mjs --command check:w-run -- --ci` only on Linux x64 with the acquired CI tools.
Mandatory mode fails when prerequisites are absent. It cannot pass through SKIP.

On Linux x86_64, run:

```text
bun check --target w-run
```

On a Windows host, the same gate builds and runs the Linux binary in WSL
Ubuntu. It does not claim general native Windows support. The separate
bounded candidate gate is:

```text
bun check --target w-run-windows
```

The versioned fixtures can be run directly from the repository root after an
enabled Linux build:

```text
./build/seed-c-run/w run compiler/seed-c/fixtures/hlo0-hello.w
# Hello, world!
./build/seed-c-run/w run compiler/seed-c/fixtures/restaurant-linear.w
# Table 42 remains open
# Kitchen is ready
./build/seed-c-run/w run compiler/seed-c/fixtures/restaurant-if.w
# Kitchen open
# After service
# Kitchen closed
# After service
```

The opaque-basename rule is local to `w run`. `w check` keeps its existing
identifier helper and grammar; no general source identity or runner surface is
claimed.

## Public bounded `w build` retained artifact

The finite seed build command is limited to:

```text
w build <explicit-path.w> --target <exact-supported-triple> --output <new-artifact>
```

Source, target, and output are mandatory. Linux accepts only
`x86_64-unknown-linux-gnu` in an explicitly enabled Linux native build.
Native Windows accepts only `x86_64-pc-windows-msvc` in an explicitly enabled
Windows native build. Disabled or unsupported routes return 2 without staging
or tool invocation.

The output must not exist. Its parent must already be a physical directory.
The route creates private staging under that parent and publishes one
caller-owned executable without replacement. Before publication it removes every
intermediate and staging entry from the staging directory, retaining only the
hidden sibling publication source. Linux prefers exactly one
`renameat2(..., RENAME_NOREPLACE)` operation from a hidden sibling in the output
parent; on a filesystem without that syscall it uses one atomic hard-link
no-clobber publication and best-effort post-commit hidden-link cleanup. Windows
uses exactly one `MoveFileExW` without `MOVEFILE_REPLACE_EXISTING` after the
staging directory is clean. The route does not search PATH, invoke a shell, use
network access, choose a host or target implicitly, or invoke WSL implicitly. It
writes no receipt and makes no general artifact-record claim.

The compiler stage is shared with `w run`. `w run` retains its private
compile-to-execute cleanup lifecycle and its fast development recipe. `w build`
uses the explicit internal release recipe by default: `mlir-opt --canonicalize
--cse`, `llc -O3`, and Linux direct-link stripping with `-s`; native Windows
uses `llc -O3` and LLD `/opt:ref /opt:icf /incremental:no`. There is no public
profile option in this seed. `w build` retains only the published executable.
The route supports the current seed subset, including `restaurant-if.w`,
without changing language semantics. Separate compile/run benchmark migration
is not part of this bundle.

The Linux/WSL and native Windows gates cover the retained-artifact route:

```text
bun check --target w-run
bun check --target w-run-windows
```

For a manual Linux or WSL smoke from the repository root:

```sh
export W_MLIR0_TOOLCHAIN_ROOT=/home/<user>/.cache/W/toolchains/portable-mlir-toolchain/2026.09.11/x86_64-unknown-linux-gnu/toolchain
cmake -S compiler/seed-c -B build/seed-c-run -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc \
  -DW_SEED_ENABLE_LINUX_NATIVE_RUN=ON \
  -DW_MLIR0_LINUX_MLIR_OPT:FILEPATH=${W_MLIR0_TOOLCHAIN_ROOT}/bin/mlir-opt \
  -DW_MLIR0_LINUX_MLIR_TRANSLATE:FILEPATH=${W_MLIR0_TOOLCHAIN_ROOT}/bin/mlir-translate \
  -DW_MLIR0_LINUX_LLVM_CONFIG:FILEPATH=${W_MLIR0_TOOLCHAIN_ROOT}/bin/llvm-config \
  -DW_MLIR0_LINUX_LLC:FILEPATH=${W_MLIR0_TOOLCHAIN_ROOT}/bin/llc \
  -DW_MLIR0_LINUX_LINK_DRIVER:FILEPATH=/usr/bin/ld
cmake --build build/seed-c-run --target w
./build/seed-c-run/w run compiler/seed-c/fixtures/hlo0-hello.w
# Hello, world!
./build/seed-c-run/w run compiler/seed-c/fixtures/restaurant-linear.w
# Table 42 remains open
# Kitchen is ready
rm -rf -- ./build/seed-c-run
```

The automated reproduction remains `bun check --target w-run`; it also checks
rejection cases and cleanup of its private build and fixture directories.

## RUN0 execução interna bounded verified-HLO0

`include/w_seed_run0.h` e `src/w_seed_run0.c` formam o adapter de execução
interna RUN0. `w_seed_run0_execute` consome somente um plano HLO0 caller-owned
do subset print-literal input-driven. O adapter não aloca no heap e usa storage
bounded na stack.
Ele não é um runner W público ou geral.

`w_seed_hlo0_verify_plan` é a autoridade compartilhada por HLO1 e RUN0. O
verifier exige todos os fields e o digest do plano input-driven. Cada text array
tem um terminador NUL e zero tail. Os bytes não usados do payload também são
zero. Planos forjados falham antes de qualquer callback.

RUN0 valida pointers, plano e overlap entre plano e result no preflight. Uma
falha de preflight preserva o result e não chama o sink. Depois do preflight,
RUN0 stageia payload mais LF e chama o sink exatamente uma vez. O callback não
pode reter o pointer dos bytes staged.

O sink retorna `accepted_bytes` e `flush_status`. Depois do callback, o result
publica attempted bytes, accepted bytes, flush status e uma chamada. `OK` exige
aceitação completa e flush `SUCCEEDED`. Short write, rejeição, flush failed e
reports inválidos retornam `IO`. Bytes aceitos podem ter efeito externo, e
RUN0 não promete rollback desse efeito.

`tests/run0_gate.c` é um harness test-only. Ele lê uma fixture por `fopen` com
limite inclusivo de 4096 bytes. Esse path não prova aquisição pública ou geral
de source nem seleção de contexto.
O fluxo funcional do gate é
`source → parser/frontend → HIR0 → HLO0 → verify HLO0 → RUN0 sink`. Erros do
source ou features fora do subset usam exit `2`. Falhas internas, do verifier
ou do sink usam exit `3`.

O oracle cobre source canônico, o witness Restaurant, string vazia, whitespace,
comentários, shape, identidade, payload, UTF-8, parse incompleto, limites,
argumentos e repetição exata. Ele
injeta short write e flush failure pelo mesmo adapter de stdout. O primeiro
caso preserva o prefixo aceito. O segundo preserva a saída completa. Sink
reject continua com zero bytes.

`cli/io.c` centraliza `_setmode`, writes e flushes verificados. O target
público mantém o help, a rota `w check` e o subset bounded W-1521 de
`w run`. O oracle RUN0 prova somente a superfície interna RUN0; não prova o
runner público geral.

```text
bun check --target run0
```

RUN0 é source-backed-current somente para esse subset bounded. Aquisição pública
ou geral de source, seleção de contexto ou owner, workspace, backend, linker,
runtime, provider geral e outros programas W continuam gaps. ACQ0 cobre somente
a aquisição interna bounded no contexto efêmero já fornecido. O `benchmarkDisposition`
é `compiler-lifecycle`. O oracle de correção corresponde somente à célula ready
`clean × check-end-to-end` de W-1488. Nenhuma etapa RUN0 ou de execução se
torna um estágio medido. `startup` e `execution` permanecem na track
`product-runtime` e deferred. Este corte não publica timing nem result.
`hlo3-hello-world-runtime-benchmark` permanece deferred.

## ConstIR D1-D6 seed

`include/w_seed_constir.h` e `src/w_seed_constir.c` formam um executor interno
caller-owned para uma projeção ConstIR D1. O componente recebe documentos CST,
o output completo do frontend e o `w_seed_frontend_result`. Ele não reparseia
source e não faz resolução de nomes ou tipos.

Para manter essa fronteira, o frontend publica campos append-only normalizados
para valor Bool/integer e para ordinal de parâmetro e target de call. O seed de
aplicações genéricas desta seção é independente: ele não chama nem inclui este
executor. Uma integração posterior pode consumir facts compatíveis; ela usa os
spans apenas para provenance e diagnósticos.
Para parâmetros com dois nomes, o primeiro é o label externo required e o
segundo é o nome interno (`from current: Stage`, `at index: u8`). Um único nome
publica o label homônimo required; `_ name` é positional-only. Não existe um
modifier `named`: use diretamente `external internal: Type` quando os nomes
externo e interno forem distintos.

O lowering publica registros tipados para uma `const fn` com expressão única ou
com uma árvore bounded de statements. A projeção preserva a função owner, a
expressão frontend, o tipo inferido, o span, os operands, os ordinais de
parâmetros, a identidade de enum e case, o operador normalizado, as calls
locais, os arms de `switch`, os cases de membership, os locals normalizados e
as relações de `guard`, `if` e `for` com range half-open. Ela aceita Bool,
inteiros, enums payloadless, unary e binary tipados, calls locais, enum switch,
membership, `StaticList<enum>` caller-owned com `.count`/index e loops
bounded. A extensão D2 aceita `String` somente como literal simples, parâmetro
ou local e operand de `==`/`!=`; compara length e bytes UTF-8 borrowed, com
limite de 4.096 bytes e heap quota zero. O resultado da função permanece Bool,
integer ou enum nesta fatia. `String` result, escapes/interpolation, ordering,
concatenação, member/index, `Bytes`, heap values, errors, panic builtin,
generics e calls externas sem body ConstIR continuam fora da fatia.

Para D3, cada `TypedConstExpr` lowerable vira uma função sintética zero-arg com
origem `TYPED_CONST_EXPRESSION` e índice sentinel explícito para
`FRONTEND_FUNCTION`. O subset é uma árvore parentetizada fechada de literal,
grouping, unary e binary operator escalar com resultado Bool ou integer de
width/signedness explícitos. Calls, identifiers/named const, nested generic,
imported head/predicate e String computed result ficam `UNSUPPORTED`; origem,
mapping, application status, relação ou type shape incoerentes ficam
`INVALID`. O digest sintético exclui span/trivia/spelling e o valor calculado
usa a mesma codificação de fingerprint do immediate.

Para D4, `const name: Type = expression` e `export const` continuam sintaxe
append-only do parser. O frontend publica `w_seed_frontend_const_declaration`
caller-owned, com module/name/export, spans, declared type, initializer,
counts, capacities, ranges e relação explícita de identifier; ele resolve e
tipa, mas não avalia nem materializa `ConstValue`. Local/parameter lookup tem
precedência e forward reference no mesmo módulo é válida. Imports, associated
const e environment ficam fora. A forma lowerable exige `Bool` ou integer de
width/signedness explícitos e aceita literal, grouping, unary, binary e
referência a module const. Mismatch, unresolved ou relação corrompida é
`INVALID`; untyped, `String`, enum/list/quantity/size, call, member/index,
nested generic e imported const/head/predicate são `UNSUPPORTED`.

`W_SEED_CONSTIR_SCHEMA_VERSION` é `w-seed-constir-6`. Cada declaration vira
função sintética zero-arg com origem `FRONTEND_CONST_DECLARATION`; cada
identifier vira dependency `CALL`. A ordem é frontend functions, declarations
de module const em source order e `TypedConstExpr`. O body digest exclui
span/trivia/spelling e inclui estrutura e identity/digest de dependency. O
grafo é validado antes dos counters de cache e dos steps reais do evaluator:
corruption é `INVALID` zero-step, dependency fora do subset é `UNSUPPORTED`
zero-step com failure `function` e ciclo alcançável é `EVALUATION_FAILED` com
`W-CONST-0002`, counters zero e caminho causal fechado. Com capacidade de
receipt, o ciclo publica exatamente o `CONST_ARGUMENT` causal antes do retorno;
com capacidade zero, não publica receipt. O limite é 256 dependencies;
excedê-lo mantém `UNSUPPORTED` com failure `dependency-limit`; predicates
posteriores não executam.

D5 adiciona uma tabela de memoização local por invocação de
`w_seed_constir_evaluate`. A tabela é vazia, fixa, allocation-free e limitada a
256 declarations. A chave é a identity da declaration no programa fixo; o
primeiro acesso é `ACTIVE`/miss, e somente um resultado `ConstValue` completo e
válido vira `READY`. Um hit copia o valor e omite a avaliação do corpo, mas o
node `CALL` mantém seu step. Falha, panic, quota, resultado inválido e
`ACTIVE` nunca são cacheados; nova invocação começa vazia. Lookup linear tem
overhead adicional `O(E*R)`, com `R <= 256`, e espaço `O(R)`; isso não é o
custo total do evaluator, que também faz o lookup próprio de
`program_function_for_const`. Cada dependency de module const alcançada por
um `CALL` memoizado na avaliação generic D5 é avaliada no máximo uma vez. A
função usada diretamente como entry de `w_seed_constir_evaluate` não é
pré-semeada na tabela. Os
counters append-only `const_cache_hits`/`const_cache_misses` aparecem no eval
result e em cada receipt, não no fingerprint, body digest, type identity ou
cache key compartilhável. O preflight genérico continua rejeitando ciclos,
limite e corrupção antes dos counters de cache e dos steps reais; o ciclo
publica o `CONST_ARGUMENT` causal quando há capacidade de receipt e nenhum
receipt quando a capacidade é zero.

D6 mantém a tabela no evaluator público como uma sessão nova por chamada.
`src/w_seed_constir_session.h` define a sessão privada do seed compiler.
`w_seed_generic_validation_run` inicializa uma sessão imediatamente antes do
loop de argumentos e usa `w_seed_constir_evaluate_in_session` somente para
`TYPED_PENDING_CONST`. Immediate arguments continuam convertidos na mesma
posição. Predicates continuam usando `w_seed_constir_evaluate` e não partilham
a sessão. A sessão morre ao terminar ou falhar a fase de argumentos.

A tabela tem 256 entradas, sem heap e sem eviction. O limite é igual ao limite
de dependencies do preflight generic. Um `READY` bem-sucedido persiste entre
arguments irmãos da mesma aplicação. Falha, quota, panic, valor inválido e
`ACTIVE` não são reutilizáveis. Counters continuam por evaluation e receipt.
Steps, heap e result bytes continuam agregados por `quota_consume`, e
call-depth continua limitado por evaluation. A sessão não é pública, não cruza
applications, runs, threads, programs ou processes e não participa do
fingerprint.

O teste C de ConstIR chama o evaluator diretamente em um grafo cíclico, sem o
preflight generic. `ACTIVE` retorna `W-CONST-0002` com 2 misses, 0 hits, 3
steps e call depth 3; a segunda invocação repete os mesmos números. Essa é uma
defesa local do evaluator e não altera a causalidade generic nem os receipts
de ciclo.

`w_seed_constir_measure` calcula todas as capacidades. `w_seed_constir_run`
escreve somente quando cada array e o receipt possuem capacidade. Uma função
fora da fatia recebe um único root `W-CONST-0001` e não publica nodes parciais.
Cada função lowerable publica um digest SHA-256 do corpo semântico. O digest
exclui spans, trivia, offsets e nomes de parâmetros. Parênteses redundantes são
provenance do frontend: o ConstIR normalizado não publica um node para eles.

O evaluator recebe uma função ConstIR e argumentos tipados. Ele executa Bool,
inteiros, enums e listas estáticas borrowed com a mesma policy checked. Ele
percorre a árvore de statements, avalia bounds uma vez e usa quotas de steps,
heap, call depth e result bytes. Heap scalar usa zero bytes. Short-circuit não avalia
o RHS. Overflow, divisão inválida e divisão por zero emitem exatamente
`W-CONST-0006`. Excesso de quota emite exatamente `W-CONST-0003`. Entrada
estrutural, arity, tipo ou enum inválidos retornam `INVALID` sem execução.
Listas borrowed aceitam somente elementos enum/enum-subset payloadless nesta
fatia e têm um teto determinístico de 4096 elementos antes da avaliação; essa
é uma limitação da implementação D1, não uma regra completa da linguagem.
O depth da função de entrada é 1; `call_depth=1` aceita uma função folha e
`call_depth=2` aceita uma call aninhada. Um limite de implementação de 256 para
call depth e de 64 para slots de uma aplicação impede recursão C não limitada.
Uma quota finita acima de 256 é `INVALID`; `SIZE_MAX` pede a mesma política
limitada, sem clamp silencioso. Workspace ausente ou pequeno para uma call é
entrada estrutural `INVALID`, sem diagnóstico W.
`w_seed_constir_value` é zerado quando qualquer diagnóstico runtime W3/W6
ocorre, inclusive quando a quota de result bytes falha.
Result bytes usa o encoding D1 versionado: prefixo explícito de version, kind,
type e enum/value fields, seguido por payload Bool de um byte ou integer de 16
bytes. Literals frontend usam magnitude não-negativa little-endian canônica com
bytes altos zero; nodes/values ConstIR usam little-endian canônico em
two's-complement sign-extended para signed e zero-extended para unsigned,
limitado a 128 bits. O encoding não usa `sizeof`, layout ou endianness do host.

## Validação seed C de predicates genéricos

`include/w_seed_generic_validation.h` e
`src/w_seed_generic_validation.c` formam uma camada caller-owned separada do
frontend. `w_seed_generic_validation_run` recebe o
`w_seed_frontend_output`/`w_seed_frontend_result`, um `w_seed_constir_program`,
o índice da aplicação, quotas, workspace e arenas caller-owned de receipts,
conversão e bytes de evidência. O frontend não inclui nem chama ConstIR. A
camada não reparseia source, não
modifica os arrays do frontend e não publica type identity final ou
monomorphization.

`W_SEED_GENERIC_VALIDATION_SCHEMA_VERSION` é
`w-seed-generic-validation-8`. O fingerprint legado continua em
`w-seed-generic-fingerprint-1`. A identidade semântica D9 usa o schema
`w-seed-generic-specialization-2` e recebe um receipt opcional de origem
nominal.

`BOUND_IMMEDIATE` e `TYPED_PENDING_CONST` são elegíveis. O predicate é
localizado pela relação `frontend_function == predicate_function_index`; uma
expression pending usa uma função sintética pela origem e índice
`TypedConstExpr`. O preflight read-only chama
`w_seed_constir_validate_program` uma vez e depois
`w_seed_constir_validate_invocations_in_validated_program` para todas as
relações, funções sintéticas, predicates e capacities antes da primeira
avaliação. Quando um predicate precisa receber o value, a
conversão D1 fechada aceita `Bool`, integers com
width/signedness, enum cases payloadless (inclusive enum subset) e
`StaticList` destes enum cases. Bytes integer são little-endian canônicos.
`String` simples usa offsets/counts da arena `const_bytes` no frontend e value
borrowed no ConstIR; literal ou argumento acima de 4.096 bytes, escape,
interpolation e categorias fora desta lista que precisem dessa conversão são
`UNSUPPORTED`; função ausente/não lowerable também é `UNSUPPORTED`. Índices, spans, relations,
signature, arity ou tipo de retorno malformados são `INVALID`. Cada lista D1
limita 4.096 elementos. A travessia e a validação estrutural caller-owned têm
depth máximo 256. Listas aninhadas continuam `UNSUPPORTED`.

O validador ConstIR canônico também aceita um programa estruturalmente vazio:
zero functions e zero em todos os outros counts. Counts órfãos continuam
`INVALID`. A camada generic usa esse validador canônico uma vez; ela não tem
um bypass local para o caso vazio.

Para `CONCRETE`, o domínio efetivo é `parameter->domain_type`. Para
`DEPENDENT`, o resolver read-only exige uma referência estritamente anterior a
um parâmetro `TYPE`, cujo argumento na mesma aplicação seja `TYPE`,
`BOUND_IMMEDIATE` e tenha `type_index` válido. Esse `type_index` é usado na
assinatura, conversão e fingerprint; ordem, kind, status, índice incoerente ou
`ConstValue.type_index` divergente retornam `INVALID` antes do evaluator. Um
dependent válido não é `UNSUPPORTED` por si. String source-backed sem predicate
é validável e fingerprintável; o predicate D2 simples usa a conversão borrowed
bounded, enquanto over-limit, escape, interpolation e outras formas não
lowerable continuam `UNSUPPORTED`.

D3 avalia expressions parentetizadas fechadas de literal, grouping, unary e
binary operator com resultado Bool ou integer explícito; heap scalar permanece
zero. Calls, identifiers/named const, String computed result, nested generic,
imported head/predicate e graph dependencies/cycles permanecem fora. A
validação não muta frontend/ConstIR.

D4 adiciona somente referências a module const locais explicitamente tipadas.
O frontend mantém a aplicação `TYPED_PENDING_CONST` e o ConstIR baixa
declarations como funções zero-arg com dependency `CALL`; graph preflight,
cycles e capacities ocorrem antes de evaluation. Forward references são
válidas, mas imports, associated const, inference, calls, member/index,
untyped/String/enum/list/quantity/size e nested generic permanecem
`UNSUPPORTED`.

D5 adiciona memoização somente dentro de cada invocação de
`w_seed_constir_evaluate`: a tabela é vazia, fixa e bounded a 256 declarations;
um acesso novo é miss/`ACTIVE`, um resultado válido completo vira `READY`, e um
hit copia o valor sem reavaliar o corpo. O `CALL` do hit ainda consome seu step.
Falha, quota, panic, valor inválido e `ACTIVE` não são reutilizáveis. Os
counters `const_cache_hits`/`const_cache_misses` são evidence por evaluation e
receipt, fora do fingerprint; cache compartilhável, cross-argument/session,
imports, associated const, inference, identity final, runtime e self-host
continuam fora.

D6 adiciona uma sessão somente dentro do loop de argumentos de uma aplicação.
No witness `AnswerPair`, o membro estático `agrees = left == right` demonstra a
intenção do contrato; o primeiro calculated argument tem 7 steps, 4 misses
e 1 hit. O segundo irmão tem 1 step, 0 misses e 1 hit. Quota total 8 aceita os
dois. Quota 7 aceita o primeiro e falha o segundo antes do lookup, com 0 steps,
0 misses e 0 hits nessa segunda evaluation. Uma nova aplicação ou run reinicia
a sessão. Falha no primeiro calculated argument impede o segundo. O preflight
mantém ciclos, corrupção e dependency-limit antes de counters e steps.

Um `TypedConstExpr` retido em aplicação `INVALID` ou `UNSUPPORTED` é somente
audit: sua função sintética permanece não lowerable e não pode executar.

O estado público distingue `VERIFIED`, `REJECTED`, `UNSUPPORTED`, `INVALID`,
`EVALUATION_FAILED` e `CAPACITY`. `EVALUATION_FAILED` conserva o
`w_seed_constir_eval_result`, counters e o diagnostic W-CONST-0003/W-CONST-0006.
Quota não vira W-CONST-0004. `CAPACITY` não é ausência de feature e preserva
sentinels quando a arena caller-owned é pequena. `REJECTED` publica
W-CONST-0004 e facts de application/head, argumento, predicate, além de
`failure = "predicate:false"` e um array caller-owned
`rejection_trace = ["predicate:false"]`. A evidência é limitada a 64 records e
4.096 bytes UTF-8. Esta fatia D1 armazena e publica exatamente um item de
fallback. O item usa 15 bytes UTF-8 compartilhados para `failure` e
`rejection_trace` na arena caller-owned. O evaluator atual não guarda execution
dependencies para uma slice detalhada. Esta fatia D1 usa, portanto, o fallback
inteiro permitido. A capacidade da arena é medida antes da primeira avaliação.

`computed_argument_count` é publicado integralmente no preflight. Immediate não
gera receipt causal; cada pending gera `CONST_ARGUMENT` antes da avaliação e
depois o predicate gera `PREDICATE`. `required_receipts` é a soma dessas duas
contagens e a ordem é determinística por argumento e depois predicate. Uma
falha pending de quota/overflow/panic preserva seu receipt/evaluation antes de
`EVALUATION_FAILED`; todos os estados não-verificados mantêm fingerprint zero.

Depois da validação, o result também expõe
`W_SEED_GENERIC_VALIDATION_FINGERPRINT_SCHEMA_VERSION =
"w-seed-generic-fingerprint-1"` e um estado separado
`NOT_AVAILABLE`/`AVAILABLE`/`UNSUPPORTED`, com digest fixo de 32 bytes. Todos
os resultados não `VERIFIED` mantêm `NOT_AVAILABLE` e bytes zero. `VERIFIED`
encodable finaliza `AVAILABLE` somente depois que todos os predicates retornam
`Bool(true)`; `VERIFIED` fora do subconjunto encodable pode manter o resultado
principal e publicar `UNSUPPORTED`. O preflight constrói o SHA em estado local
antes da avaliação e valida cada relação consumida, sem counters, quotas,
workspace, receipts ou arena de evidence no preimage.

O preimage versionado começa com o prefixo ASCII
`w-seed-generic-fingerprint-1` e usa tags estáveis, integers/counts
big-endian, text UTF-8 length-prefixed, canonical type e `ConstValue` conforme
DESIGN §8.7.12. Ele exclui spans, source spelling, labels, índices,
allocation/layout e versões ambientais. O `body_digest` é evidence do lowering
ConstIR, não uma recomputação criptográfica nesta camada. O fingerprint é
evidence interna de comparação, não `TypeId`, `SemanticInterfaceKey`,
`WAbiKey`, wire/schema ID ou cache/instantiation key. Digests diferentes implicam
preimages diferentes; um digest igual isolado não prova preimages iguais nem
identidade collision-safe sem o preimage completo. O fingerprint-1 sozinho ainda
não contém o preimage completo de declaration/substitution/witness definido para
W-1467 e não é a identidade semântica. Target, profile, edition, toolchain,
compiler, bundle e ABI pertencem à recipe física; a identidade final depende da
declaração/interface e dos receipts canônicos definidos em DESIGN §8.7.8.

O result também expõe `specialization_state`, `specialization_bytes_written`,
`specialization_bytes_required` e `specialization_digest`. O input recebe um
buffer caller-owned de preimage e sua capacidade. Estados não `VERIFIED`
publicam `NOT_AVAILABLE`, `0/0` e digest zero. Um `VERIFIED` fora do encoder
publica `UNSUPPORTED` e zeros sem alterar o estado principal. Um buffer curto,
inclusive zero, publica `CAPACITY`, o tamanho exato em `bytes_required`, `0` em
`bytes_written`, digest zero e não toca o buffer. Capacidade suficiente publica
`AVAILABLE`, os bytes exatos e SHA-256 do preimage. `NULL` com capacidade
não-zero é `INVALID` antes de evaluation e mantém a projeção `NOT_AVAILABLE`;
`{nonnull,0}` é o caso `CAPACITY`. O buffer não pode aliasar frontend, ConstIR,
conversion values, evidence, receipts ou result, e esses inputs devem ficar
imutáveis entre measure/write. O measure pass ocorre antes do write/hash pass.
No D9, output/result também devem ser disjuntos da origin view, preimage,
digest, authority bytes e text arrays; o preflight rejeita esses aliases antes
de evaluation.

`NominalDeclarationOrigin` é caller-owned e contém a preimage completa da
authority autenticada pelo resolver, scoped package name, caminho canônico de
módulo (segmentos NFC), nominal kind, owner chain semântica e declared name.
Version, revision, mirror/source, dependency alias, workspace, checkout/file
path, source-set, feature, target, profile, edition, spans, docs, interface
digest e body ficam fora. Alias humano é apresentação. O seed não implementa
resolver de registry/Git; a authority receipt é trust input e `.registry("w")`
não é preimage suficiente. O builder aceita ASCII nesta fatia e publica
`UNSUPPORTED` para Unicode/NFC ainda não resolvido.

O package é exatamente `[a-z][a-z0-9-]{0,62}/[a-z][a-z0-9-]{0,62}`,
com no máximo 127 bytes. Segments de módulo, owners e declared name usam
`[A-Za-z_][A-Za-z0-9_]*`, sem NUL. Package não-ASCII, UTF-8 inválido e
identifier ASCII inválido são `INVALID`; UTF-8 válido não-ASCII e identifier
que excede somente o ceiling são `UNSUPPORTED` até NFC/ceiling resolver.
Os kinds são `STRUCT=1`, `TYPE=2`, `OBJECT=3`, `ENUM=4`,
`PROTOCOL=5`, `SERVICE=6`; `alias`, callable/function overload e const
não são type constructors D9.

O receipt `w-seed-nominal-origin-1` usa prefixo ASCII, root `0x4f`, tags
`0x41` authority, `0x50` package, `0x4d` module, `0x49` segment e `0x44`
declaration, com lengths/counts `u32` big-endian e sem terminador NUL:

```text
prefix, 0x4f,
0x41 u32(authority-length) authority-preimage,
0x50 text(package),
0x4d u32(segment-count) (0x49 text(segment))* ,
0x44 u8(kind) u32(owner-count) (u8(owner-kind) text(owner))* text(name)
```

O builder caller-owned possui measure/write, limites, overflow checks, exact
required/written, SHA-256 accelerator e não escreve parcialmente. Ele publica
no máximo 16.384 bytes de preimage. O parser aceita somente um envelope hard de
framing de 65.536 bytes: acima dele a view é `INVALID`; dentro dele, framing
completo acima do ceiling do feature pode ser `UNSUPPORTED`, mas framing
parseado como `AVAILABLE` ou `UNSUPPORTED` sempre exige SHA-256 correspondente.
Somente framing `INVALID` evita o hash. A view valida framing, digest e relação
frontend module/head/kind/owner antes da evaluation. Equality compara digest,
length e bytes completos. Receipt ausente permite `VERIFIED`, mas publica
`IDENTITY_REQUIRED` com `0/0` e digest zero.

O preimage D9 começa com `w-seed-generic-specialization-2` e root `0x49`.
Ele codifica uma vez `0x4f u32(origin-length) origin-preimage`, seguido de
`0x44 u32(parameter-count)`, os records de parâmetros/refinements D8, a
substitution vector normalizada e witness count zero (`0x57 u32(0)`). Module e
head não aparecem fora do receipt. Domain type, ConstValue e predicate body
digest usam a codificação canônica compartilhada com o fingerprint. Labels,
spans, source indices, annotation presence, counters, quota, session e source
spelling ficam fora. Target, profile, compiler, lowering plan e runtime facts
pertencem à recipe física futura e não são inputs deste encoder.
O predicate body digest ConstIR é somente um proxy bounded do lowering do seed,
não um receipt semântico autoritativo universal do predicate/construtor; esse
receipt do compiler completo continua gap.

`w_seed_generic_specialization_equal` rejeita views vazios ou com ponteiros
NULL e compara length, digest e os bytes completos do preimage. Digest igual
forçado com bytes diferentes, digest corrompido ou dois views indisponíveis não
produzem falso positivo. O digest não é `TypeId`, cache key ou identidade
persistente. `TypeId` runtime permanece fora deste seed.

O probe/gate source-backed usa `ServiceStage`, `canMove`, `isValidStagePath` e
`StagePath` de [domain.w](../../reference/last-light/domain.w). Ele prova o path
canônico como `VERIFIED` e vazio, salto e duplicata como `REJECTED`, repete o
probe para provar determinismo e verifica quota, relações inválidas, categorias
unsupported, Bool/integer/enum/list conversion e capacity. O witness usa o
package `last-light/restaurant` e o módulo `domain`: duas aplicações idênticas do standard path
publicam `AVAILABLE` com digest igual; a rota
`[.accepted, .cancelled]` também é `VERIFIED`, mas tem digest
diferente; vazio, salto e duplicata permanecem `NOT_AVAILABLE` com
bytes zero. O gate Bun reconstrói o preimage de forma independente e o probe
imprime module/head, estado do fingerprint, digest, estado/tamanho/digest da
specialization e `body_digest` do predicate.

O gate também lê `tooling/generic-fingerprint-cases.json` e exige os casos
únicos GPF0-W-1460/W-1461/W-1462/GPF0-W-1463-current/GPF0-W-1464-current/
GPF0-W-1465-current/GPF0-W-1466-current/GPF0-W-1467-current/
GPF0-W-1468-current,
suas decisões, sources e runner C+Bun. Ele verifica
em `reference/last-light/generics.w` os marcadores únicos da assinatura de
`StaticValue`, do body `export const expected = value`, dos aliases
`EnabledFeature`/`LastCallLabel`/`VerifiedFinalCall`, da função
`isFinalCallLabel` e do head `FinalCallValue`. O witness temporário usa a
assinatura real com body `{}` porque o body associado completo ainda está fora
da projeção seed; o gate prova os positivos String duplicados, `Mostly
harmless`/empty rejeitados, over-limit e corrupção de arena sem alegar que
`generics.w` inteiro compila.

Para W-1462, o gate extrai uma vez os markers reais de
`isUltimateAnswer`/`UltimateAnswer`, executa immediate `42`, computed `(6 * 7)`
e duplicate, rejeita `(6 * 6)`, deriva quota cumulativa, overflow, unsupported
call e corrupção de origem/relação/type/application/mapping. Bun reconstrói
independentemente o preimage i64 e SHA-256; a projeção não é compiler, runtime,
self-host ou identity final.

Para W-1463, o gate também lê `ultimateAnswer` e `UltimateAnswerNamed` reais,
prova named/duplicate `42`, forward chain, rejected, ciclos self/2/3 e caminho
fechado, ciclo inalcançável, mismatch, unresolved, unsupported, corruption,
zero capacity, quota, dependency graph ceiling de 257 declarations com failure
`dependency-limit` e named
const arithmetic overflow `i8` com `W-CONST-0006`. Bun reconstrói o preimage i64 e
verifica que immediate, D3 e D4 usam o schema
`w-seed-generic-fingerprint-1`; compiler completo, imports, associated const,
initializer inference, identity final, runtime e self-host continuam fora.

Para W-1464, o gate lê `answerSeed`, `firstAnswerHalf`, `secondAnswerHalf`,
`assembledUltimateAnswer` e `UltimateAnswerShared` reais. O probe C e Bun
reconstroem independentemente o diamond em source order: quatro misses, um hit,
sete steps, reset entre invocações e fingerprint igual a immediate, D3, D4 e a
aplicação D5 duplicada. O witness também prova D3/D4 linear sem hits, quota 7/6,
falha aritmética não cacheada e counters zero para ciclos, zero capacity,
dependency-limit e corrupção. A fatia fecha somente memoização local por
invocation; não é cache compartilhável, compiler, runtime ou self-host.

Para W-1465, o gate lê `AnswerPair`, seu membro `agrees` e as duas aliases
equivalentes do Restaurante. Cada aplicação possui dois calculated arguments com
`assembledUltimateAnswer` nos dois slots. O primeiro receipt prova 7 steps,
4 misses e 1 hit. O segundo prova 1 step, 0 misses e 1 hit. Quota total 8
aceita os dois. Quota 7 falha o segundo antes do lookup. Novo run e nova
aplicação repetem 7/1. Bun reconstrói a preimage dos dois i64 sem usar os
counters C. A sessão é privada ao seed compiler e não alcança predicates ou
outra aplicação.

Para W-1466, o gate mantém `ultimateAnswer: i64` explícito e verifica que
somente as quatro declarations do diamond são inferidas. Bun reconstrói os
records `declared_type=NONE`/`effective_type=i64`, a propagação de integer,
Bool, suffix e forward/reordered graph, além do preimage e da equivalência
entre source explícito e inferido. Ciclos anchored/unanchored e as barreiras
negativas continuam preflight evidence. O witness incompatível compara com a
reconstrução Bun o estado `EVALUATION_FAILED`, `W-CONST-0002`, path `0,1,0`,
count, receipt causal, counters zero e fingerprint indisponível; o witness
multi-slot prova count 2 com um receipt e count 2 com zero receipts quando a
capacidade é zero. A fatia não é compiler completo, identity final, imports,
associated const, cache compartilhável, runtime ou self-host.

Para W-1467, o gate reconstrói o preimage D8 de `StagePath`, `FinalCallValue`,
`UltimateAnswer` e `AnswerPair` usando os fragments reais do Restaurante e
também de `StaticValue<Bool,true>`/`StaticValue<String,"The final seating">`.
O probe publica o preimage AVAILABLE em hex e o gate compara esses bytes
escritos pelo C, length e SHA com a reconstrução Bun; não há bytes publicados
em estados não-AVAILABLE. Os casos immediate `42`, computed `6 * 7`, named
const, diamond e aliases equivalentes compartilham a mesma identity quando
head, module e refinement são iguais. Head, module ou predicate body diferentes
mudam a identity. Rejected, quota, overflow, cycle, invalid, corrupt e
unsupported não publicam identity. Os adversários head/module/refinement são
fixtures C sintéticos; os fragments reais sustentam somente os witnesses
source-backed e o gate não afirma que `generics.w` inteiro compila. C cobre
capacidade exata, zero e short-by-one com sentinels. O comparador cobre views
vazios/NULL, digest corrompido e digest forçado com preimages diferentes. A
receita física,
receipts autoritativos de package/interface, witness selection geral e
`TypeId` continuam gaps.

Para W-1468, o receipt nominal usa authority preimage, package, path de módulo,
kind, owners e name. A view precisa de digest íntegro e relação com module/head;
trailing, truncation, digest corrupto, module/head/kind/owner/process mismatch e
relação divergente falham antes de evaluation. O schema
`w-seed-generic-specialization-2` codifica o receipt uma
vez e não repete `module_id`/head. Sem receipt, o principal pode ser `VERIFIED`,
mas a identidade é `IDENTITY_REQUIRED`. O gate separa os witnesses de
`domain.w` e `generics.w`, usa a authority fixture synthetic declarada no
corpus e não afirma autorização de registry. Ele exige os markers literais
`authority: .registry("w")`, `name: "last-light/restaurant"` e o
moduleSet root/include/layout do `build.w`, e compara os bytes completos
escritos pelo C com a reconstrução Bun.

    bun tooling/check-seed-generic-validation.mjs

O probe source-backed e o gate dedicado executam o witness `ServiceStage`,
`canMove` e `isValidStagePath` de [domain.w](../../reference/last-light/domain.w).
Eles repetem o lowering e a avaliação para provar determinismo de receipt,
digest, valor e contadores, incluindo caminhos vazios, prefixos, cancelamento,
falhas de bounds e quotas:

    bun tooling/check-seed-constir.mjs

O gate scoped constrói o probe e os testes em diretório temporário, executa os
witnesses source-backed (`ServiceStage`/`DomainError` em `domain.w`, além de
`horizon_tool.w`, `formatting.w` e `numerics.w`), repete o probe para provar
receipt byte-idêntico e verifica os negativos semânticos e a barreira de recovery:

    bun tooling/check-seed-frontend.mjs

O classifier usa somente os dados oficiais vendorizados em `unicode/17.0.0`.
O check offline é executado com:

    bun tooling/check-seed-unicode.mjs

Uma atualização de dados é explícita e requer rede:

    bun tooling/generate-seed-unicode.mjs --update

Os headers include/w_seed_source.h, include/w_seed_lexer.h,
include/w_seed_parser.h, include/w_seed_formatter.h e
include/w_seed_diagnostic.h, include/w_seed_frontend.h e a biblioteca
w_seed_source são detalhes de
implementação do seed. A biblioteca, o parser, o formatter e o adapter não alocam,
não acessam paths, locale, clock ou environment e não assumem ownership dos
bytes de entrada. O probe de lexer é somente ferramenta de teste.

O source probe lê uma entrada limitada de stdin e devolve os bytes sem
alteração. O lexer probe devolve somente itens e spans; o parser probe devolve
CST, folhas e issues internos para o checker. O limite de 16 MiB pertence aos
probes de teste e ao perfil do target bootstrap. Esse limite não é contrato da
linguagem nem limite do source reader. NFC, resolver completo e build publication
continuam gaps intencionais desta fatia. O scanner C acima é somente source
validation. O formatter e o adapter D0 são fatias fechadas internas, não
frontend normativo. Os checkers Bun usam os probes sobre os casos
F0 e os witnesses FZ0 quando aplicável. Esses casos continuam oracles de design
e não são output de um compiler. A proveniência é mantida em
[formatter-cases.json (F0)](../../tooling/formatter-cases.json),
[frontend-freeze-cases.json (FZ0)](../../tooling/frontend-freeze-cases.json),
[formatting.w](../../reference/last-light/formatting.w) e nos
[check-seed-source-reader.mjs](../../tooling/check-seed-source-reader.mjs),
[check-seed-formatter.mjs](../../tooling/check-seed-formatter.mjs) e
[check-seed-diagnostic.mjs](../../tooling/check-seed-diagnostic.mjs),
[check-seed-module-scan.mjs](../../tooling/check-seed-module-scan.mjs),
[check-seed-check-driver.mjs](../../tooling/check-seed-check-driver.mjs) e
[check-seed-frontend.mjs](../../tooling/check-seed-frontend.mjs); os
checker lê essas fontes e não copia seus payloads. O checker do parser também
extrai slices delimitados por marcadores de bytes atuais de
`reference/last-light/generics.w`, `enum_contracts.w` e `allocation.w`; esses
witnesses são
somente entradas sintáticas do seed e não afirmam que o Last Light completo
compila. O parser/formatter/adapter seed não promove comportamento normativo de
compiler, AST/HIR, resolver completo ou runtime; o frontend acima é somente a
fatia semântica bounded explicitamente descrita nesta página.
