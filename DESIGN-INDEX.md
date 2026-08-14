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
| linhas de `DESIGN.md` | 29683 |
| tokens aproximados de `DESIGN.md` | 313700 |
| linhas de `RATIONALE.md` | 6071 |
| tokens aproximados de `RATIONALE.md` | 156000 |
| seções numeradas | 27 |
| seções terminais com evidência local | 339/339 |
| decisões | 1399 (W-001–W-1399) |
| famílias de viabilidade | 182 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 75 |
| casos de substituição estruturados | 75/75 |
| decisões referenciadas por casos R0 | 170/1399 |
| decisões classificadas para design freeze | 547/1399 (170 source + 432 oracle + 8 explícitas; 63 overlaps) |
| decisões ainda sem classe de freeze | 852 |
| decisões com múltiplos eixos obrigatórios | 2 |
| formas R0 com baseline estática | 190 |
| surface lexemes das formas vigentes R0 | 1595 total; mediana 17; máximo 58 |
| bundles executáveis R1 | 42 |
| variantes/tarefas R1 | 117/168 |
| casos R0 promovidos a R1 | 69/75 |
| casos do corpus Tree-sitter | 121 |
| pares canônicos do formatter F0 | 27 |
| casos/operações do kernel de memória M1 | 185/606 (82 aceitos + 103 rejeitados) |
| casos/operações do control block shared SHC0 | 45/84 (16 aceitos + 6 errors + 3 faults + 20 rejeitados) |
| casos/operações do kernel de allocation físico A0 | 48/123 (15 aceitos + 33 rejeitados) |
| casos/operações do kernel de layout e ABI L0 | 78/96 (27 aceitos + 51 rejeitados) |
| casos/operações do kernel de execução E0 | 73/677 (38 aceitos + 35 rejeitados; 10/10 origens happens-before) |
| casos/operações do kernel de runtime closure E1 | 41/473 (19 aceitos + 22 rejeitados; sete testes host) |
| casos/operações do behavior Lazy LZ0 | 40/118 (16 aceitos + 20 rejeitados + 4 fault; 12 testes host) |
| casos/operações da composição de ownership e execução MX0 | 46/274 (23 aceitos + 23 rejeitados; 14 testes host) |
| casos/operações de channel bounded CH0 | 47/333 (28 aceitos + 19 rejeitados; 12 testes host) |
| casos/operações de contexto local CTX0 | 25/94 (10 aceitos + 15 rejeitados; seis testes host) |
| casos/operações de layout de interferência IL0 | 30/140 (22 aceitos + 8 rejeitados; nove testes host) |
| casos/operações de lock da linguagem LM1 | 39/86 (20 aceitos + 18 rejeitados + 1 fault; 11 testes host) |
| casos/operações do carrier de snapshot SP0 | 27/82 (14 aceitos + 12 rejeitados + 1 fault; sete testes host) |
| casos/operações do kernel de boundary effects B0 | 39/320 (25 aceitos + 14 rejeitados) |
| casos/operações de service recovery SR0 | 48/392 (18 aceitos + 30 rejeitados; 17 testes host) |
| casos/operações do kernel de packages e releases P0 | 44/379 (22 aceitos + 22 rejeitados) |
| casos/operações do workflow single-file PYN1 | 95/546 (23 aceitos + 72 rejeitados) |
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
| casos/operações do root de processo PR0 | 48/249 (27 aceitos + 21 rejeitados; host oracle não executa W/provider) |
| casos/operações do filesystem FS0 | 99/665 (40 aceitos + 59 rejeitados; host oracle não executa syscalls/provider) |
| casos/operações de erro portátil de I/O IOE0 | 44/219 (32 aceitos + 12 rejeitados; host oracle não executa W/provider) |
| casos/operações de tempo operacional TIME0 | 52/277 (27 aceitos + 25 rejeitados; host oracle não executa clock/timer/provider) |
| casos do corpus semântico S0 | 140 (70 positivos + 70 negativos) |
| matriz host SDM0 | 30 (8 oracle aceitos + 22 oracle rejeitados; 3 outcomes aceitos + 5 rejeitados; 24 decisões) |
| outcomes SemanticResult S0 | 140 |
| snapshots de diagnostic D0 | 70 |
| snapshots F0 no formato D0 | 27 |
| codes D0 catalogados | 277/183 |
| sources W no root do Última Luz | 100 |
| sources W em todo o Última Luz | 108 |
| sources W no rascunho da std | 31 |
| módulos/APIs catalogados da std | 31/422 |
| superfícies qualificadas da std usadas pelo Última Luz | 92 |
| requisitos do Última Luz com contrato std | 31/31 |
| requisitos do Última Luz ausentes na std | 0/31 |

A estimativa de tokens usa bytes divididos por quatro. Use o valor somente para planejar leitura.

## Cobertura do catálogo D0

| Família | Catalogados | Referenciados |
|---|---:|---:|
| ALLOCATOR | 11 | 11 |
| BORROW | 10 | 10 |
| CAPABILITY | 1 | 1 |
| CONST | 7 | 7 |
| CONTEXT | 7 | 7 |
| CONTRACT | 5 | 5 |
| DIAGNOSTIC | 1 | 1 |
| DOC | 2 | 2 |
| EFFECT | 2 | 2 |
| EXPR | 4 | 4 |
| FLOW | 2 | 2 |
| FMT | 2 | 2 |
| FOREIGN | 1 | 1 |
| GENERIC | 5 | 5 |
| INIT | 1 | 1 |
| JUPYTER | 1 | 1 |
| LABEL | 4 | 4 |
| LEX | 1 | 1 |
| MATCH | 3 | 3 |
| MEMORY | 1 | 1 |
| MOVE | 1 | 1 |
| OWNERSHIP | 8 | 8 |
| PARSE | 29 | 29 |
| PATTERN | 6 | 6 |
| PLACEMENT | 3 | 3 |
| PROCESS | 3 | 3 |
| SCRIPT | 16 | 16 |
| SEM | 1 | 1 |
| SESSION | 28 | 28 |
| STD | 1 | 1 |
| SUSPEND | 4 | 4 |
| TIME | 2 | 2 |
| TLS | 3 | 3 |
| TYPE | 4 | 4 |
| UNIT | 1 | 1 |
| USE | 1 | 1 |
| WIRE | 1 | 1 |

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 15–230 | 2700 | Como ler este documento |
| 1 | 231–254 | 300 | Limite da alegação |
| 2 | 255–280 | 400 | Invariantes |
| 3 | 281–3352 | 31600 | Contratos estáticos e orçamento de símbolos |
| 4 | 3353–3415 | 400 | Superfície integrada |
| 5 | 3416–3723 | 2700 | Source, nomes e edição |
| 6 | 3724–4129 | 3700 | Módulos, imports e visibilidade |
| 7 | 4130–4878 | 7200 | Bindings, funções e closures |
| 8 | 4879–7375 | 19700 | Tipos e conversões |
| 9 | 7376–9321 | 23100 | Memória, layout e alocação |
| 10 | 9322–9444 | 1400 | Property behaviors |
| 11 | 9445–9809 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 9810–12843 | 33600 | Concorrência, paralelismo e execução |
| 13 | 12844–15120 | 23000 | Módulos de execução, services e entries |
| 14 | 15121–19067 | 50200 | Prelude e standard library |
| 15 | 19068–19797 | 7400 | Números, ranges e unidades |
| 16 | 19798–21397 | 13500 | Texto, bytes e collections |
| 17 | 21398–21747 | 4100 | Matrizes, tensors e ML |
| 18 | 21748–22251 | 5100 | Performance e custo |
| 19 | 22252–22826 | 6900 | FFI, unsafe e ilhas de linguagem |
| 20 | 22827–24332 | 16100 | Compilador e bootstrap |
| 21 | 24333–26349 | 19100 | Packages, builds e releases |
| 22 | 26350–26860 | 4900 | Tooling e interface para máquinas |
| 23 | 26861–28283 | 16900 | Protocolos e pesquisas de ecossistema |
| 24 | 28284–29146 | 11900 | Design freeze e pendências |
| 25 | 29147–29357 | 1900 | Produto de referência Última Luz |
| 26 | 29358–29683 | 4000 | Plano de implementação |

## Navegação compacta de RATIONALE

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 1 | 14–4624 | 71100 | Evidência comparativa |
| 2 | 4625–4655 | 500 | Proveniência |
| 3 | 4656–6071 | 84300 | Ledger |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 15–7375 | 68700 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 7376–15120 | 84000 | ownership, errors, tasks, domains, services e entries |
| std e performance | 14, 15, 16, 17, 18, 19 | 15121–22826 | 87200 | módulos, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 22827–28283 | 57000 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26 | 28284–29683 | 17800 | freeze, Última Luz, gates e roadmap |

O bundle agrupa seções para planejamento; os intervalos não são uma nova autoridade.

## Classificação de viabilidade

| Classe | Famílias |
|---|---:|
| Alternativa | 1 |
| Design comum fechado; providers missing | 1 |
| Design fechado | 1 |
| Design fechado; provider missing | 1 |
| Possível agora | 86 |
| Possível por transport profile | 1 |
| Provável | 63 |
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

