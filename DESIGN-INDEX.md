# Índice gerado do design W

> Gerado por `tooling/design-index.mjs`. Não edite este arquivo.
> `DESIGN.md` continua sendo a única fonte normativa. `RATIONALE.md` é complementar e não normativo.

## Contexto mínimo

1. Leia este índice para localizar a seção necessária.
2. Leia somente o intervalo correspondente em `DESIGN.md`.
3. Use `RATIONALE.md` somente para IDs, evidência, alternativas e proveniência.
4. Busque o ID W quando a tarefa alterar uma decisão.
5. Abra o produto Última Luz somente para o exemplo afetado.
6. Não leia `tooling/tree-sitter-w/src/` como source. Essa pasta é gerada.

## Snapshot calculado

| Métrica | Valor |
|---|---:|
| linhas de `DESIGN.md` | 40404 |
| tokens aproximados de `DESIGN.md` | 468800 |
| linhas de `RATIONALE.md` | 12656 |
| tokens aproximados de `RATIONALE.md` | 309900 |
| seções numeradas | 27 |
| seções terminais com evidência local | 445/455 |
| decisões | 1599 (W-001–W-1599) |
| famílias de viabilidade | 183 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 155 |
| casos de substituição estruturados | 155/155 |
| decisões referenciadas por casos R0 | 265/1599 |
| decisões classificadas para design freeze | 1599/1599 (825 implementation-evidence-gap; 83 superseded; 177 source-backed-current; 506 oracle-backed-current; 8 rejected) |
| decisões com evidência legada de fonte/oráculo | 629/1599 (265 source + 413 oracle + 8 explícitas; 57 overlaps) |
| decisões ainda sem classe de freeze | 0 |
| decisões com múltiplos eixos obrigatórios | 2 |
| formas R0 com baseline estática | 545 |
| surface lexemes das formas vigentes R0 | 5760 total; mediana 31; máximo 151 |
| bundles executáveis R1 | 57 |
| variantes/tarefas R1 | 162/228 |
| casos R0 promovidos a R1 | 69/155 |
| protocolo HUM0 | 8 slices/32 tasks; 0 human records/0 model records; structure-only |
| casos do corpus Tree-sitter | 136 |
| pares canônicos do formatter F0 | 31 |
| casos/operações do kernel de memória M1 | 185/606 (79 aceitos + 106 rejeitados) |
| casos/operações do control block shared SHC0 | 45/84 (16 aceitos + 6 errors + 3 faults + 20 rejeitados) |
| casos/operações do kernel de allocation físico A0 | 48/123 (15 aceitos + 33 rejeitados) |
| casos/operações do kernel de layout e ABI L0 | 78/96 (27 aceitos + 51 rejeitados) |
| casos/operações do kernel de execução E0 | 73/677 (38 aceitos + 35 rejeitados; 10/10 origens happens-before) |
| casos/operações do kernel de runtime closure E1 | 41/473 (19 aceitos + 22 rejeitados; sete testes host) |
| casos/operações do behavior Lazy LZ0 | 40/118 (16 aceitos + 20 rejeitados + 4 fault; 12 testes host) |
| casos/operações da composição de ownership e execução MX0 | 46/274 (23 aceitos + 23 rejeitados; 14 testes host) |
| casos/operações de channel bounded CH0 | 79/586 (40 aceitos + 39 rejeitados; oracle host) |
| casos/operações de contexto local CTX0 | 25/94 (10 aceitos + 15 rejeitados; seis testes host) |
| casos/operações de layout de interferência IL0 | 30/140 (22 aceitos + 8 rejeitados; nove testes host) |
| casos/operações de lock da linguagem LM1 | 39/86 (20 aceitos + 18 rejeitados + 1 fault; 11 testes host) |
| casos/operações do carrier de snapshot SP0 | 27/82 (14 aceitos + 12 rejeitados + 1 fault; sete testes host) |
| casos/operações do kernel de boundary effects B0 | 39/320 (25 aceitos + 14 rejeitados) |
| casos/operações de service recovery SR0 | 48/392 (18 aceitos + 30 rejeitados; 17 testes host) |
| casos/operações do kernel de packages e releases P0 | 44/379 (22 aceitos + 22 rejeitados) |
| casos/operações do workflow module-run RU0 | 15/74 (5 aceitos + 10 rejeitados) |
| casos/operações da sessão transacional PYN2 | 70/298 (56 aceitos + 14 rejeitados) |
| casos/operações de apresentação PYN3 | 26/75 (9 aceitos + 17 rejeitados; host oracle não executa W) |
| casos/operações do adapter Jupyter PYN3 | 32/104 (16 aceitos + 16 rejeitados; host oracle não executa W) |
| casos/operações do export notebook PYN3 | 18/49 (5 aceitos + 13 rejeitados; host oracle não executa W) |
| casos do container WMeta1 W0 | 42 (5 aceitos + 37 rejeitados; 2 readers independentes) |
| casos/operações do carrier tabular TAB0 | 64/155 (22 aceitos + 42 rejeitados; host oracle não executa W) |
| casos/operações dos adapters tabulares TAB1 | 86/193 (36 aceitos + 50 rejeitados; host oracle não executa W) |
| casos/operações do carrier DLPack PYN4 | 75/326 (26 aceitos + 49 rejeitados; host oracle não executa W) |
| casos/operações de device execution DEV0 | 42/186 (17 aceitos + 25 rejeitados; host oracle não executa W) |
| casos/operações da síntese de kernel KM0 | 32/218 (6 aceitos + 26 rejeitados; host oracle não executa W) |
| casos/operações de body estrangeiro FB0 | 45/90 (15 aceitos + 28 rejeitados + 2 informações; host oracle não executa adapter) |
| casos/operações de Web bodies WB0 | 27/160 (12 aceitos + 15 rejeitados; host oracle não executa compiler/provider) |
| casos/operações do root de processo PR0 | 48/251 (27 aceitos + 21 rejeitados; host oracle não executa W/provider) |
| casos/operações do filesystem FS0 | 99/665 (40 aceitos + 59 rejeitados; host oracle não executa syscalls/provider) |
| casos/operações de erro portátil de I/O IOE0 | 44/219 (32 aceitos + 12 rejeitados; host oracle não executa W/provider) |
| casos/operações de tempo operacional TIME0 | 52/277 (27 aceitos + 25 rejeitados; host oracle não executa clock/timer/provider) |
| casos do corpus semântico S0 | 158 (79 positivos + 79 negativos) |
| matriz host SDM0 | 30 (8 oracle aceitos + 22 oracle rejeitados; 3 outcomes aceitos + 5 rejeitados; 24 decisões) |
| outcomes SemanticResult S0 | 158 |
| snapshots de diagnostic D0 | 79 |
| snapshots F0 no formato D0 | 31 |
| codes D0 catalogados | 331/239 |
| sources W no root do Última Luz | 107 |
| sources W em todo o Última Luz | 112 |
| sources W no rascunho da std | 32 |
| módulos/APIs catalogados da std | 32/436 |
| superfícies qualificadas da std usadas pelo Última Luz | 90 |
| requisitos do Última Luz com contrato std | 35/35 |
| requisitos do Última Luz ausentes na std | 0/35 |

A estimativa de tokens usa bytes divididos por quatro. Use o valor somente para planejar leitura.

## Cobertura do catálogo D0

| Família | Catalogados | Referenciados |
|---|---:|---:|
| ALLOCATOR | 11 | 11 |
| ATOMIC | 16 | 16 |
| BEHAVIOR | 5 | 6 |
| BORROW | 12 | 12 |
| CAPABILITY | 1 | 1 |
| CONST | 7 | 7 |
| CONTEXT | 7 | 7 |
| CONTRACT | 5 | 5 |
| DIAGNOSTIC | 1 | 1 |
| DOC | 2 | 2 |
| EFFECT | 2 | 2 |
| EXECUTION | 3 | 3 |
| EXPR | 4 | 4 |
| FACET | 6 | 6 |
| FLOW | 2 | 2 |
| FMT | 2 | 2 |
| FOREIGN | 1 | 1 |
| GENERIC | 5 | 5 |
| INIT | 1 | 1 |
| JUPYTER | 1 | 1 |
| LABEL | 3 | 3 |
| LEX | 1 | 1 |
| MATCH | 3 | 3 |
| MEMORY | 1 | 1 |
| MOVE | 1 | 1 |
| OWNERSHIP | 8 | 8 |
| PARSE | 29 | 29 |
| PATTERN | 6 | 7 |
| PIPE | 4 | 4 |
| PIPELINE | 5 | 5 |
| PLACEMENT | 4 | 4 |
| RUN | 16 | 16 |
| SEM | 1 | 1 |
| SESSION | 28 | 28 |
| STD | 1 | 1 |
| STREAM | 1 | 1 |
| SUSPEND | 5 | 5 |
| TIME | 2 | 2 |
| TLS | 3 | 3 |
| TYPE | 7 | 7 |
| UNIT | 1 | 1 |
| USE | 1 | 1 |
| WIRE | 1 | 1 |
| YIELD | 11 | 11 |

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 15–234 | 2700 | Como ler este documento |
| 1 | 235–258 | 300 | Limite da alegação |
| 2 | 259–284 | 400 | Invariantes |
| 3 | 285–3796 | 38600 | Contratos estáticos e orçamento de símbolos |
| 4 | 3797–3859 | 400 | Superfície integrada |
| 5 | 3860–4236 | 3700 | Source, nomes e edição |
| 6 | 4237–4660 | 4000 | Módulos, imports e visibilidade |
| 7 | 4661–5618 | 10000 | Bindings, funções e closures |
| 8 | 5619–9206 | 36200 | Tipos e conversões |
| 9 | 9207–11400 | 26500 | Memória, layout e alocação |
| 10 | 11401–11849 | 5900 | Property behaviors |
| 11 | 11850–12218 | 3000 | Erros, panic, OOM e cleanup |
| 12 | 12219–15814 | 42000 | Concorrência, paralelismo e execução |
| 13 | 15815–18095 | 23200 | Módulos de execução, services e entries |
| 14 | 18096–22150 | 51600 | Prelude e standard library |
| 15 | 22151–22998 | 8900 | Números, ranges e unidades |
| 16 | 22999–24711 | 14800 | Texto, bytes e collections |
| 17 | 24712–25061 | 4100 | Matrizes, tensors e ML |
| 18 | 25062–26037 | 12200 | Performance e custo |
| 19 | 26038–26612 | 6900 | FFI, unsafe e ilhas de linguagem |
| 20 | 26613–28322 | 19300 | Compilador e bootstrap |
| 21 | 28323–30858 | 26700 | Packages, builds e releases |
| 22 | 30859–31496 | 6800 | Tooling e interface para máquinas |
| 23 | 31497–33047 | 19100 | Protocolos e pesquisas de ecossistema |
| 24 | 33048–35206 | 33600 | Design freeze e pendências |
| 25 | 35207–35417 | 1900 | Produto de referência Última Luz |
| 26 | 35418–40404 | 67100 | Plano de implementação |

## Navegação compacta de RATIONALE

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 1 | 15–6265 | 101100 | Evidência comparativa |
| 2 | 6266–6295 | 500 | Proveniência |
| 3 | 6296–12656 | 208300 | Ledger |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 15–9206 | 96300 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 9207–18095 | 100600 | ownership, errors, tasks, domains, services e entries |
| std e performance | 14, 15, 16, 17, 18, 19 | 18096–26612 | 98500 | módulos, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 26613–33047 | 71900 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26 | 33048–40404 | 102600 | freeze, Última Luz, gates e roadmap |

O bundle agrupa seções para planejamento; os intervalos não são uma nova autoridade.

## Classificação de viabilidade

| Classe | Famílias |
|---|---:|
| Alternativa | 1 |
| Design comum fechado; providers missing | 1 |
| Design fechado | 1 |
| Design fechado; provider missing | 1 |
| Possível agora | 89 |
| Possível por transport profile | 1 |
| Provável | 61 |
| Rejeitado na baseline | 3 |
| Rejeitado por enquanto | 7 |
| Rejeitado | 18 |

## Pesquisas explícitas

- Nenhuma família sem classificação de viabilidade.

## Comandos de leitura

```powershell
bun tooling/design-slice.mjs --section 12
bun tooling/design-slice.mjs --heading 12.13
bun tooling/design-slice.mjs --id W-711 --context 2
bun tooling/design-slice.mjs --rationale-heading 1.3
rg -n -C 4 'transaction' DESIGN.md
bun tooling/design-index.mjs --check
```

