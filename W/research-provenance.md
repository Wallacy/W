# Proveniência do catálogo de pesquisa do W

> **Arquivo histórico · 21 de julho de 2026**

Este documento mantém fora da árvore publicável `W/` a ligação entre pesquisas
atuais e os cadernos que as originaram. Ele não é especificação nem decisão. Os
caminhos abaixo existem nesta árvore histórica; o inventário byte a byte está em
[consolidation-manifest.md](consolidation-manifest.md) e os links externos em
[historical-references.md](historical-references.md).

A auditoria integral por família, incluindo itens cobertos, parciais e ausentes,
está em [WIP-audit.md](WIP-audit.md). Este arquivo curto continua sendo apenas o
índice das pesquisas que possuem protótipos ou documentos satélite.

| Tema | Fontes de origem |
|---|---|
| tagged values/pointers | `Y/WIP.MD`; `Y/_w_/C/tagged_pointer.c`, `tagged.c`, `tagged_ops.c`, `tagged_8k.c`, `tbytes.c`, `type128.c` |
| arenas, mimalloc e heap por módulo | `Y/WIP.MD` (`module.memory`, `process.flush`); `Y/_w_/WC.MD` |
| previsão de recursos no import/tooling | `Y/WIP.MD:164-225`, `517-527`, `1542` e `1667`: min/max/current, call tree, paralelismo estruturado e metadata de alocação |
| WC/EmitC/bootstrap | `Y/_w_/WC.MD`; trechos de bootstrap e geração WC em `Y/WIP.MD` |
| `fn<lang>` e GPU | exemplos `fn<C>`, `fn<JS>`, `fn<TS>`, `fn<Rust>`, `fn<WC>`, `fn<Zig>` e `fn<Bend, .gpu>` em `Y/WIP.MD`/`Y/_w_/WC.MD` |
| WLO/WLON | seção “Dics, json, wlon, comptime e profile” de `Y/WIP.MD` |
| wQL/wRPC/RestQL | seções `WRPC` e `wRPC / wQL` de `Y/WIP.MD`; `TK/wQL.MD`; `TK/RestQL.MD` |
| V6/Computer Units | `TK/V6.MD`; seção “Computer Unit” de `Y/WIP.MD` |
| tree strings | `TK/tree_string.md` |
| SQLite/storage | seção “Storage” de `Y/WIP.MD`; `Y/_w_/WC.MD`; `TK/wQL.MD` |
| GPU/HDL | perguntas e exemplos OpenCL, OpenMP, HIP, CUDA, Bend e HDL em `Y/WIP.MD`/`Y/_w_/WC.MD` |
| snapshots, PGO e autotests | trechos `snapshot`, `.test.w`, `.debug.w`, `profile`, PGO e `.w/.autotest` de `Y/WIP.MD` |
| executors/queues por módulo, instância e task | `Y/WIP.MD:470-472`, `723-734`, `2402-2450`, `2561-2624`, `2703-2776`, `3958-3999` |
| entrypoints e host events | `Y/WIP.MD:136-138`, `1224-1258`, `1481-1535`, `1718-1760`, `3167-3210` |
| type modifiers e tipos parametrizados por valores | `Y/WIP.MD:1549-1568`, `1678-1706`, `2778-2806`, `3824-3835` |
| matrices/tensors/ML | accelerators, SIMD/GPU e ciência em `Y/WIP.MD:1694`, `2157`, `3064-3069`, `3353-3362`; a notação matricial foi acrescentada como nova hipótese, não encontrada pronta no caderno |
| callables, captures e callbacks | `Y/WIP.MD:805-835`, `1404-1477`, `2181-2233`, `3577-3654`; a DB2 substitui `CallbackType` por `fn`, `some fn` e `any fn` |

## Protocolos e framing

As notas de WRPC/wRPC alternam discriminadores de um ou quatro bytes e enums
`int64`, sem fechar endianness, versão, call ID, partial reads, limites, erros ou
multiplexação. O que vale preservar é a separação entre tipo, operação e corpo;
os números não formam um wire format adotado.

As palavras `select`, `insert`, `update`, `delete` e `call`, query em header,
GET com body, batch, sequência PUT+GET por ID e módulos remotos implícitos foram
alternativas exploradas, não baseline.

## Tagged values

Os spikes são gerações incompatíveis de uma pesquisa, não partes de um runtime:

| Spike | Pergunta investigada |
|---|---|
| `tagged.c` | combinar endereço, small tags, null e estado shared numa palavra e promover para estrutura indireta |
| `tagged_8k.c` | bits disponíveis com endereços virtuais de 48, 52, 56 ou 57 bits |
| `tagged_pointer.c` | distinguir scalar, compound, error e shared e decidir quando usar estrutura auxiliar |
| `tagged_ops.c` | operações e transições atômicas diretamente no encoding compacto |
| `tbytes.c` | perda ao retirar bits de um `double` IEEE-754 |
| `type128.c` | uso de 128 bits para intermediários, checks ou payloads maiores |

O catálogo detalhado dos protótipos e seus problemas está em
[legacy-spikes.md](research/legacy-spikes.md).
