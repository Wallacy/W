# Source reader, lexer, parser, formatter, frontend seed e D0/D1 do seed C

**Status:** componente real e interno do w-seed-c. O parser seed abaixo é uma
fatia incremental de CST/recovery. O formatter, o adapter D0 e o frontend seed
semântico são fatias fechadas caller-owned; o frontend é interno e não é um
compiler driver normativo.

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

O lexer é uma fatia interna destinada a SH0. Palavra é sempre crua: a tabela de
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

`include/w_seed_parser.h` e `src/w_seed_parser.c` adicionam uma API C11 sem
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
sintáticos `copy`/`take`/`pin`/`inout`/`ref`, a expressão estruturada
`transaction identifier = expression { ... }` e o statement `commit` com
expression opcional. O parser Pratt
delimitado também reconhece tuple types e tuple expressions com dois ou mais
itens, inclusive trailing comma, e o statement
`spawn<.domain>` ou `spawn<domain: .domain> let name = expression`. O parser
mantém `()` e `(expression)` como formas unitária e parenthesized. `(T)` e
`(T,)` não são aceitos como tuple type, e `(expression,)` não é aceito como
tuple expression. O parser Pratt é usado pelos vinte e sete casos F0
selecionados. A tabela de
reconhecimento inclui atribuições compostas, coalescing, operadores lógicos e
bitwise, comparações, ranges, shifts, aritmética, `@`, potência e `in`/`is`;
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
preserva policy de label (`positional`, `named`, `external` ou `optional`), normaliza o
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
`W_SEED_FRONTEND_SCHEMA_VERSION` é `w-seed-frontend-7`. Os campos D2/D3
anteriores permanecem append-only; a versão 6 acrescenta records, ranges,
counts/capacities e relações de module const; a versão 7 acrescenta
`effective_type` e preserva `declared_type` como annotation source-only para
inferência scalar D7.

O seed materializa `Bool`, inteiros bounded (incluindo `usize`), strings simples
sem escape, cases enum contextuais e `StaticList` caller-owned. Inteiros usam
bytes little-endian canônicos; strings usam offsets em `const_bytes`; listas
preservam ordem, vazio e duplicatas com `const_elements`. O teto explícito de
lista é 4096 elementos e o de slots de uma aplicação é 64. `_ value: T` é um
value domain dependente somente quando
`T` é type parameter anterior e resolve para `StaticArgumentRepresentable`.
Todos os slots continuam obrigatórios: `_` torna apenas o label externo opcional.
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
closures, semântica de effects/async/lock, contratos de transaction,
AST/HIR,
name/type resolution e formatter normativo permanecem fora; `foreign` falha fechado antes
do body. `unsafe fn<C>` e `export unsafe fn<C>` são aceitos somente pela ilha C
validada abaixo; `unsafe fn` sem tag de linguagem permanece STOP. Imports só
aparecem antes de qualquer declaration; `export` aceita `fn`, `const fn`,
`async fn`, `struct`, `enum`, `type` e `alias` nesta fatia. Enum generics são reconhecidos
sintaticamente, mas continuam unsupported no frontend.
`transaction` não aceita argumentos de contract nesta fatia. Statements
`commit` e transactions aninhadas são reconhecidos sintaticamente em qualquer
block. O parser não valida owner, provider, nesting, commit, rollback, effects
ou atomicidade. `const fn` e `export const fn` preservam o modifier no CST e
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

Corpos `fn<C>` e `fn<lang:.c>` usam um scanner C11 caller-owned com o profile
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
C11 sem heap, path, locale, clock ou environment. A API recebe buffers de
tokens, grupos e output do caller, mede antes de escrever e rejeita
CST recuperado/fatal. A renderização usa a estrutura CST e as folhas raw; não
carrega o oracle JSON nem procura IDs ou digests em runtime. O gate compara os
28 pares de [`formatter-cases.json`](../../tooling/formatter-cases.json),
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
catalog truth retornam `UNSUPPORTED`; não há claim semântico.

## Build local

Use C11, CMake e Ninja. Mantenha o diretório de build fora do repositório:

    $build = Join-Path $env:TEMP "w-seed-source-reader-build"
    cmake -S compiler/seed-c -B $build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build $build
    ctest --test-dir $build --output-on-failure

O corpus dirigido de lexer também pode ser executado com:

    bun tooling/check-seed-lexer.mjs

O parser seed e os vinte e oito IDs F0 completos (input e output) podem ser
validados com:

    bun tooling/check-seed-parser.mjs

O formatter seed compara os 28 outputs canônicos, reparses e prova a
idempotência, assinatura CST, capacidade e foreign body:

    bun tooling/check-seed-formatter.mjs

O adapter D0 compara os 28 records `W-FMT-0001` byte-a-byte ao snapshot e
valida records lex/parse com JSON.parse e schema/ordem determinísticos:

    bun tooling/check-seed-diagnostic.mjs

O gate dedicado do scanner C constrói o probe em diretório temporário e compara
32 operações de scan C do corpus FB0, o witness source-backed atual de
`hardware.w` (`unsafe fn<C>`), limites e digest adulterado; sem claim de build:

    bun tooling/check-seed-foreign.mjs

## Frontend seed interno (fatia semântica)

`include/w_seed_frontend.h` e `src/w_seed_frontend.c` formam a primeira fatia
caller-owned do frontend. A API C11 mede antes de emitir e não usa heap,
filesystem, locale, environment ou clock. Ela aceita somente documentos CST
`COMPLETE`; CST `RECOVERED`/fatal cruza uma barreira sem alterar nenhum buffer.
Logical source ID e module ID são entradas explícitas. Imports externos usam
somente stubs estruturados fornecidos pelo caller (símbolos exportados,
parâmetros, política de labels e retorno).

A normalização preserva módulo, imports e aliases de itens, structs/fields,
enums/cases/payloads, declarações de tipo/alias, funções, parâmetros, entry,
bindings, argumentos e expressions suportadas. Enum declarations produzem um
tipo nominal `ENUM`; conformance é uma superfície de tipo, e generics de enum
geram fato explícito `UNSUPPORTED_TYPE`. A projeção bounded de módulos/imports na ordem de input
detecta duplicate local, unresolved import/local e entry inválido, e registra
fatos explícitos para nodes, types e expressions fora do subset. O checker cobre
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
all-or-nothing. Esta fatia aceita um documento por module ID; contribuições de
vários documentos para o mesmo módulo são rejeitadas como `INVALID` em vez de
serem mescladas silenciosamente. Formas de import que o parser ainda recupera
(por exemplo, alias de item não reconhecido pelo CST) continuam unsupported.
Ownership/HIR completo, async/services/providers, avaliação de
initializers/dependencies, cache e materialização, generic calls completas,
heads importados e aplicações de enum/object/type/alias/function, tensor,
runtime, MLIR e WInterface permanecem fora desta fatia.

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
segundo é o nome interno (`from current: Stage`, `at index: u8`). `named name`
é required(name), `_ name` é optional(name), e um único nome é positional-only.

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
CST, folhas e issues internos para o checker. O limite
de 16 MiB pertence somente aos probes de teste; não é contrato da linguagem
nem limite do source reader. NFC, resolver completo e build publication continuam gaps
intencionais desta fatia; o scanner C acima é somente source validation. O
formatter e o adapter D0 são fatias fechadas internas, não frontend normativo. Os
checkers Bun usam os probes sobre os casos
F0 e os witnesses FZ0 quando aplicável. Esses casos continuam oracles de design
e não são output de um compiler. A proveniência é mantida em
[formatter-cases.json (F0)](../../tooling/formatter-cases.json),
[frontend-freeze-cases.json (FZ0)](../../tooling/frontend-freeze-cases.json),
[formatting.w](../../reference/last-light/formatting.w) e nos
[check-seed-source-reader.mjs](../../tooling/check-seed-source-reader.mjs),
[check-seed-formatter.mjs](../../tooling/check-seed-formatter.mjs) e
[check-seed-diagnostic.mjs](../../tooling/check-seed-diagnostic.mjs) e
[check-seed-frontend.mjs](../../tooling/check-seed-frontend.mjs); os
checker lê essas fontes e não copia seus payloads. O checker do parser também
extrai slices delimitados por marcadores de bytes atuais de
`reference/last-light/generics.w`, `enum_contracts.w` e `allocation.w`; esses
witnesses são
somente entradas sintáticas do seed e não afirmam que o Last Light completo
compila. O parser/formatter/adapter seed não promove comportamento normativo de
compiler, AST/HIR, resolver completo ou runtime; o frontend acima é somente a
fatia semântica bounded explicitamente descrita nesta página.
