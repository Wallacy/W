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
binding; cada argumento preserva ordinal, span, label, parâmetro, kind e o índice
de type ou `ConstValue`. O root liga à aplicação por
`generic_application_index`. `W_SEED_FRONTEND_SCHEMA_VERSION` é
`w-seed-frontend-3`.

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
inclui ou depende do componente ConstIR. Generic calls e heads importados,
enum/object/type/alias/function, quantity/size, `Bytes`, listas aninhadas,
expressions calculadas e avaliação de predicate permanecem `UNSUPPORTED` ou
fora do seed conforme a forma. O seed não apresenta esta fatia como compiler W
completo.

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

## ConstIR D1 seed

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
bounded. O resultado da função permanece Bool, integer ou enum nesta fatia.
String, Bytes, heap values, errors, panic builtin, generics e calls externas
sem body ConstIR continuam fora da fatia.

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

Somente `BOUND_IMMEDIATE` é elegível. O predicate é localizado pela relação
`frontend_function == predicate_function_index`. O preflight read-only chama
`w_seed_constir_validate_program` uma vez e depois
`w_seed_constir_validate_invocations_in_validated_program` para todas as
relações. Ele verifica todos os predicates e relações
antes da primeira avaliação. A conversão D1 fechada aceita `Bool`, integers com
width/signedness, enum cases payloadless (inclusive enum subset) e
`StaticList` destes enum cases. Bytes integer são little-endian canônicos.
String, `TYPED_PENDING_CONST`, computed values, função ausente/não lowerable e
categorias fora desta lista são `UNSUPPORTED`. Índices, spans, relations,
signature, arity ou tipo de retorno malformados são `INVALID`. Cada lista D1
limita 4.096 elementos. A travessia e a validação estrutural caller-owned têm
depth máximo 256. Listas aninhadas continuam `UNSUPPORTED`.

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

O probe/gate source-backed usa `ServiceStage`, `canMove`, `isValidStagePath` e
`StagePath` de [domain.w](../../reference/last-light/domain.w). Ele prova o path
canônico como `VERIFIED` e vazio, salto e duplicata como `REJECTED`, repete o
probe para provar determinismo e verifica quota, relações inválidas, categorias
unsupported, Bool/integer/enum/list conversion e capacity:

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
