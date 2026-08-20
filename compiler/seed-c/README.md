# Source reader do seed C

**Status:** componente real e interno do w-seed-c.

Este componente fornece uma view de bytes sem cópia. Ele valida UTF-8 estrito,
detecta o BOM inicial, conta linhas por LF, valida spans half-open e converte
offsets de bytes para pontos determinísticos. Neste primeiro seed, cada
conversão de ponto faz um scan O(bytes) a partir do início da view.

A ordem de bootstrap e o pipeline que motivam este componente estão em
[DESIGN §20.5 — Bootstrap](../../DESIGN.md#205-bootstrap) e
[DESIGN §20.2 — Pipeline](../../DESIGN.md#202-pipeline). O gate SH0 continua
ausente, conforme [DESIGN §26.6.1 — Gates internos do self-host](../../DESIGN.md#2661-gates-internos-do-self-host).

O componente não é lexer, parser, CST, recovery, formatter ou compiler. Ele não
define tokens, diagnostics D0, display columns ou digest de source. O core
compiler/core-w0 e o gate SH0 ainda não existem.

## Build local

Use C11, CMake e Ninja. Mantenha o diretório de build fora do repositório:

    $build = Join-Path $env:TEMP "w-seed-source-reader-build"
    cmake -S compiler/seed-c -B $build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build $build
    ctest --test-dir $build --output-on-failure

O header include/w_seed_source.h e a biblioteca w_seed_source são detalhes
de implementação do seed. A biblioteca não aloca, não acessa paths, locale,
clock ou environment e não assume ownership dos bytes de entrada.

O probe lê uma entrada limitada de stdin e devolve os bytes sem alteração. O
limite de 16 MiB pertence somente ao probe de teste; não é contrato da
linguagem nem limite do source reader. O checker Bun usa o probe sobre os casos
F0 e os witnesses FZ0. Esses casos continuam oracles de design e não são output
de um compiler. A proveniência é mantida em
[formatter-cases.json (F0)](../../tooling/formatter-cases.json),
[frontend-freeze-cases.json (FZ0)](../../tooling/frontend-freeze-cases.json),
[formatting.w](../../reference/last-light/formatting.w) e no
[check-seed-source-reader.mjs](../../tooling/check-seed-source-reader.mjs); o
checker lê essas fontes e não copia seus payloads.
