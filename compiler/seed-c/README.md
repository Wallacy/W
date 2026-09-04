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
result)`, sem ranking final C23. Não há fallback silencioso para C11. MSVC sem
C23 gera SKIP no gate principal ou roda recovery quando isso for solicitado.

O código continua compilável em C11 recovery. Essa escolha não cria requisito
C23 para uma ABI C externa. C permanece backend de validation, differential e
recovery; MLIR0 é a rota nativa primária somente para o subset fechado, e W/MLIR
geral continua futuro.

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
`W_SEED_FRONTEND_SCHEMA_VERSION` is `w-seed-frontend-12`. Earlier D2/D3 fields
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
integer interpolation to canonical signed `i64`. HIR0 and MLIR0 lower only the
bounded signed-`i64` subset. Escape decoding, Boolean/String value Display,
general Display conformance, and native lowering outside that subset remain
gaps.

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
Este D0 não implementa conversão explícita `try Subset(base)`, subsets
importados, aliases genéricos ou empilhados, payload patterns/captures, guards,
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

O schema HIR0 aceita exatamente um document, um module e um entry em `.default`,
e rejeita nomes de function duplicados. As ranges de function/entry do module,
parameters, blocks, host parameters/requirements e call arguments/values são
partições densas: não há gap, overlap ou record órfão. `symbols` é um índice
auxiliar validado na ordem module → parâmetros → function → entry; ele não é
autoridade para o lowering. Famílias frontend sem record HIR0 falham fechadas.
Labels host required copiam o nome público, positional usa label vazio e
qualquer outra policy permanece fora deste subset.

W-1519 introduced the binding records in HIR0 schema `w-seed-hir0-2`.
Current schema `w-seed-hir0-7` generalizes that contract. The caller-owned
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
schema `w-seed-hir0-7` gives binding initializers explicit roots in that graph,
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
bun run check:hlo0
bun run check:seed-frontend
bun run parse:hlo0
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
bun run check:hlo1
```

Este gate é correctness-only e pertence à classificação
`compiler-lifecycle`. Não publica timing ou resultado de performance;
`hlo3-hello-world-runtime-benchmark` continua deferred até existir um runner W
público/pinado com fases separáveis e reproduzíveis. C11 é recovery explícita.

## MLIR0 ponte nativa terminal para LLVM

`include/w_seed_mlir0.h` e `src/w_seed_mlir0.c` formam um adapter seed-only que
consome somente `w_seed_mlir0_input { program, hir_result }`. O header inclui
HIR0 e a implementação não inclui, chama ou cria HLO0. W-1530 advances MLIR0
to `w-seed-mlir0-9`; W-1531 advances it to `w-seed-mlir0-10`; Native0 remains
`w-seed-native0-6`. MLIR0 re-verifies HIR
through the private `native_subset0` helper. The current path retains the
linear NAT1 form and adds actual labeled LLVM-dialect blocks for bounded
top-level Unit `if` diamonds using `llvm.cond_br`/`llvm.br`. It accepts bounded
String interpolation with panic-free
signed-`i64` arithmetic, constant Bool, compile-time-known String values, and
later reads of typed immutable binding initializers. The multi-function path
also emits real internal `llvm.call` operations for bounded acyclic Unit calls
with `i64`/Bool parameters. It also emits typed scalar `llvm.return` and
result-producing `llvm.call` operations for direct call-result bindings.
Source argument evaluation order and declaration slot order remain distinct.
Bool uses exact lowercase ASCII; String values keep their counted bytes,
including NUL. The single selector still supports HLO0/HLO1/RUN0.

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

O gate `bun run check:mlir0` comprova source → parser/frontend → HIR0 → MLIR0 →
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
payloads, one post-join body, and real calls. It
also executes all five integer binary operators, a negative
result, a literal percent sign, NUL in both text and a String value, and
multiple ordered integer fields. It
requires byte-identical MLIR for the equivalent static Restaurant forms,
exact stdout, empty stderr, and exit zero. It preserves
MLIR em trivia e rejeita comentário com `print`, noop, limits excedidos e formas
fora do subset sem artifact parcial. O manifest `tooling/mlir0-toolchain.json` fixa
MLIR/LLVM/Clang/LLVM-config 20.1.2 e a recipe; sua evidência tem status
`update-required`. Linux usa ferramentas diretas; no checkout Windows
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
bun run acquire:mlir0-windows                                      # network is an explicit opt-in
bun run build:w-windows                                           # release, primary C23
bun run build:w-windows --c11-recovery                            # release, explicit C11 recovery
bun run build:w-windows --profile development --c11-recovery      # Debug, explicit C11 recovery
bun run build:w-windows --profile size-experimental --c11-recovery # MinSizeRel
```

`build:w-windows` discovers Visual Studio through `vswhere`, probes the Windows
SDK explicitly, and does not copy the heavy toolchain. The default `release`
profile maps to CMake `Release`. The `development` profile maps to `Debug`.
The `benchmark` profile maps to a recipe-constrained `Release`, requires a
clean Git worktree, records HEAD, and probes `/Brepro` and `/pathmap` before the
build. This bounded recipe evidence does not claim a reproducible binary or a
double-build result. The `size-experimental` profile maps to `MinSizeRel` for
size comparison only.
These are toolchain profiles. They do not add a profile option to `w run`,
`w check`, or another W command. The
`bun run check:w-run-windows` gate proves Hello, Restaurant/if, interpolation,
linear output, a forwarded empty argument, invalid source without stdout, and
an x64 PE. The cache has role `development-and-release-only`,
`bundledWithW: false`, and its extracted size is not a W package budget. This is
candidate evidence, not general support. Unicode source paths, the general
ABI/runtime, packaging, CI, cross-compilation, and other targets remain gaps.
The builder tries C23 first; this host's MSVC/CMake rejects that dialect, so
the current local evidence uses the explicit `--c11-recovery` option. There is
no implicit standard fallback. The builder reads each fixture before execution,
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
bun run check:mlir0
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
mlir-translate → clang/native`; HLO0, HLO1 and RUN0 are not prerequisites.

The gate checks the absolute pinned tool paths and factual version 20.1.2. A
private `/tmp/w-run-XXXXXX` directory uses mode 0700 and fixed files use modes
0600/0700. The runner uses `execv` without a shell and cleans every path on
all returns. Arguments after `--` are forwarded byte-for-byte, and the child
inherits stdout/stderr. Normal exit is propagated; signal exit is `128 +
signal`. Invocation, source, unsupported and missing-tool errors return 2;
internal, I/O and cleanup errors return 3. `--entry` and `--offline` are
rejected. The bounded native Windows route is a separate W-1532 candidate;
macOS, the general runner and performance remain gaps. The gate is
compiler-lifecycle correctness evidence only.

On Linux x86_64, run:

```text
bun run check:w-run
```

On a Windows host, the same gate builds and runs the Linux binary in WSL
Ubuntu. It does not claim general native Windows support. The separate
bounded candidate gate is:

```text
bun run check:w-run-windows
```

The versioned fixtures can be run directly from the repository root after the
build:

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

For a manual Linux or WSL smoke from the repository root:

```sh
cmake -S compiler/seed-c -B build/seed-c-run -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/gcc
cmake --build build/seed-c-run --target w
./build/seed-c-run/w run compiler/seed-c/fixtures/hlo0-hello.w
# Hello, world!
./build/seed-c-run/w run compiler/seed-c/fixtures/restaurant-linear.w
# Table 42 remains open
# Kitchen is ready
rm -rf -- ./build/seed-c-run
```

The automated reproduction remains `bun run check:w-run`; it also checks
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
bun run check:run0
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
