# Source reader, lexer e parser do seed C

**Status:** componente real e interno do w-seed-c. O parser seed abaixo é uma
fatia incremental de CST/recovery; ele não é um compiler frontend.

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
colisões no resolver, confusables, scripts mistos, formatter e scanner de
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
`entry(name)`, `struct` simples exportável com fields, `test "..." for name`
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
tuple expression. O parser Pratt é usado pelos vinte e seis casos F0
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

A fatia incremental adiciona `generic_parameters` append-only em `struct`,
`fn`, `type` e `alias`, declarações de `type`/`alias` com ordem de origem, e
envelopes de contract sequenciais em tipos e em postfix de expressão. Os argumentos de
contract aceitam somente formas sintáticas: tipo/path WORD, membro contextual
`.id`, argumento nomeado `id: static_value`, predicado `(expression)`, lista
`[static_value, ...]`, número, literal, bool ou quantity. `switch expression`
aceita pelo menos um arm `case .id|literal: expression`. Listas vazias,
duplicatas, nomes desconhecidos e exaustividade não são avaliados; não há
inferência, resolução ou avaliação de constantes nesta fatia.

Esta fatia de `for` não inclui `async`, patterns de destructuring ou `take` como
marcador de iteração; um rótulo aplicado a `while` permanece STOP. O prefixo
`async` só é aceito no owner root de `async fn` ou `export async fn`; o parser
preserva `try`/`await` como folhas raw, sem validar a ordem semântica dos
efeitos.

O lexer continua emitindo `>>` como uma folha raw de dois bytes. Um owner de
type cria duas `w_seed_parse_token_view` virtuais sem duplicar a folha; um owner
de expression mantém `>>` como shift. Newline continua trivia. Recovery só cria
`ERROR` com os bytes ignorados e `MISSING` zero-width. Os `w_seed_parse_issue`
internos têm mapping futuro para D0, mas não são diagnósticos D0. `manifest`,
declarations além de `fn`/`struct`/`type`/`alias`/`test`/`entry`, patterns,
closures, semântica de effects/async/lock, contratos de transaction,
AST/HIR,
name/type resolution, formatter e foreign scanner permanecem fora; `foreign`
falha fechado antes do body. Imports só aparecem antes de qualquer
declaration; `export` aceita `fn`, `async fn`, `struct`, `type` e `alias` nesta
fatia.
`transaction` não aceita argumentos de contract nesta fatia. Statements
`commit` e transactions aninhadas são reconhecidos sintaticamente em qualquer
block. O parser não valida owner, provider, nesting, commit, rollback, effects
ou atomicidade. Outros
modificadores de função (`static`, `const`, `unsafe` e receiver modifiers)
permanecem fora. `expect` fora de `test` falha fechado.

Statements `allocator [binding:] expression { ... }` são reconhecidos em
qualquer block, inclusive de forma aninhada. O owner `allocator_block` preserva
o keyword `allocator`, a binding WORD opcional e seu `:`, uma única expressão de
plan e um único block na ordem dos bytes; o CST não adquire leases, valida
capacidades ou resolve chamadas contextuais. `try allocator` e `allocator` na
raiz continuam STOP, e o parser não afirma a semântica de providers, contexto
ou recuperação de allocation.

Corpos foreign usam handshake dinâmico. O harness chama `require_opaque` no
cursor atual, `claim_opaque` com um span pinado e então `next` emite um único
FOREIGN_BODY. Um `next` sem claim é uma falha terminal OPAQUE_UNCLAIMED. O
lexer não calcula profile ou digest do corpo; os digests dos spans pinados são
evidence local do checker.

## Build local

Use C11, CMake e Ninja. Mantenha o diretório de build fora do repositório:

    $build = Join-Path $env:TEMP "w-seed-source-reader-build"
    cmake -S compiler/seed-c -B $build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build $build
    ctest --test-dir $build --output-on-failure

O corpus dirigido de lexer também pode ser executado com:

    bun tooling/check-seed-lexer.mjs

O parser seed e os vinte e cinco IDs F0 completos (input e output) podem ser
validados com:

    bun tooling/check-seed-parser.mjs

O classifier usa somente os dados oficiais vendorizados em `unicode/17.0.0`.
O check offline é executado com:

    bun tooling/check-seed-unicode.mjs

Uma atualização de dados é explícita e requer rede:

    bun tooling/generate-seed-unicode.mjs --update

Os headers include/w_seed_source.h, include/w_seed_lexer.h e
include/w_seed_parser.h e a biblioteca w_seed_source são detalhes de
implementação do seed. A biblioteca e o parser não alocam,
não acessa paths, locale, clock ou environment e não assume ownership dos
bytes de entrada. O probe de lexer é somente ferramenta de teste.

O source probe lê uma entrada limitada de stdin e devolve os bytes sem
alteração. O lexer probe devolve somente itens e spans; o parser probe devolve
CST, folhas e issues internos para o checker. O limite
de 16 MiB pertence somente aos probes de teste; não é contrato da linguagem
nem limite do source reader. NFC, resolver e o scanner de foreign continuam
gaps intencionais desta fatia. Os checkers Bun usam os probes sobre os casos
F0 e os witnesses FZ0 quando aplicável. Esses casos continuam oracles de design
e não são output de um compiler. A proveniência é mantida em
[formatter-cases.json (F0)](../../tooling/formatter-cases.json),
[frontend-freeze-cases.json (FZ0)](../../tooling/frontend-freeze-cases.json),
[formatting.w](../../reference/last-light/formatting.w) e no
[check-seed-source-reader.mjs](../../tooling/check-seed-source-reader.mjs); o
checker lê essas fontes e não copia seus payloads. O checker do parser também
extrai slices delimitados por marcadores de bytes atuais de
`reference/last-light/generics.w`, `enum_contracts.w` e `allocation.w`; esses
witnesses são
somente entradas sintáticas do seed e não afirmam que o Last Light completo
compila. O parser seed não promove
nenhum comportamento de compiler, AST/HIR, checker semântico ou formatter.
