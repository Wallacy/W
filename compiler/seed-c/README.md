# Source reader e lexer do seed C

**Status:** componente real e interno do w-seed-c.

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
colisões no resolver, confusables, scripts mistos, parser, owner, CST, recovery,
formatter e scanner de foreign não pertencem a esta fatia. CRLF é um único item
NEWLINE; CR isolado é UNSUPPORTED_CONTROL interno. Erros internos não são
diagnósticos D0.

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

O classifier usa somente os dados oficiais vendorizados em `unicode/17.0.0`.
O check offline é executado com:

    bun tooling/check-seed-unicode.mjs

Uma atualização de dados é explícita e requer rede:

    bun tooling/generate-seed-unicode.mjs --update

Os headers include/w_seed_source.h e include/w_seed_lexer.h e a biblioteca
w_seed_source são detalhes de implementação do seed. A biblioteca não aloca,
não acessa paths, locale, clock ou environment e não assume ownership dos
bytes de entrada. O probe de lexer é somente ferramenta de teste.

O source probe lê uma entrada limitada de stdin e devolve os bytes sem
alteração. O lexer probe devolve somente itens e spans para o checker. O limite
de 16 MiB pertence somente aos probes de teste; não é contrato da linguagem
nem limite do source reader. NFC, resolver e o scanner de foreign continuam
gaps intencionais desta fatia. O checker Bun usa o source
probe sobre os casos
F0 e os witnesses FZ0. Esses casos continuam oracles de design e não são output
de um compiler. A proveniência é mantida em
[formatter-cases.json (F0)](../../tooling/formatter-cases.json),
[frontend-freeze-cases.json (FZ0)](../../tooling/frontend-freeze-cases.json),
[formatting.w](../../reference/last-light/formatting.w) e no
[check-seed-source-reader.mjs](../../tooling/check-seed-source-reader.mjs); o
checker lê essas fontes e não copia seus payloads.
