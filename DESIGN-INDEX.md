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
| linhas de `DESIGN.md` | 28385 |
| tokens aproximados de `DESIGN.md` | 295700 |
| linhas de `RATIONALE.md` | 3796 |
| tokens aproximados de `RATIONALE.md` | 107900 |
| seções numeradas | 27 |
| seções terminais com evidência local | 327/327 |
| decisões | 1209 (W-001–W-1209) |
| famílias de viabilidade | 182 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 69 |
| casos de substituição estruturados | 69/69 |
| decisões referenciadas por casos R0 | 129/1209 |
| decisões classificadas para design freeze | 409/1209 (129 source + 319 oracle + 8 explícitas; 47 overlaps) |
| decisões ainda sem classe de freeze | 800 |
| decisões com múltiplos eixos obrigatórios | 2 |
| formas R0 com baseline estática | 169 |
| surface lexemes das formas vigentes R0 | 1404 total; mediana 16; máximo 52 |
| bundles executáveis R1 | 21 |
| variantes/tarefas R1 | 51/84 |
| casos R0 promovidos a R1 | 32/69 |
| casos do corpus Tree-sitter | 101 |
| pares canônicos do formatter F0 | 21 |
| casos/operações do kernel de memória M1 | 184/603 (82 aceitos + 102 rejeitados) |
| casos/operações do kernel de allocation físico A0 | 48/123 (15 aceitos + 33 rejeitados) |
| casos/operações do kernel de layout e ABI L0 | 78/96 (27 aceitos + 51 rejeitados) |
| casos/operações do kernel de execução E0 | 73/677 (38 aceitos + 35 rejeitados; 10/10 origens happens-before) |
| casos/operações do kernel de runtime closure E1 | 41/473 (19 aceitos + 22 rejeitados; sete testes host) |
| casos/operações do behavior Lazy LZ0 | 40/118 (16 aceitos + 20 rejeitados + 4 fault; 12 testes host) |
| casos/operações da composição de ownership e execução MX0 | 46/274 (23 aceitos + 23 rejeitados; 14 testes host) |
| casos/operações de channel bounded CH0 | 47/333 (28 aceitos + 19 rejeitados; 12 testes host) |
| casos/operações de locks escopados LM0 | 42/171 (25 aceitos + 16 rejeitados + 1 fault; oito testes host) |
| casos/operações do carrier de snapshot SP0 | 27/82 (14 aceitos + 12 rejeitados + 1 fault; sete testes host) |
| casos/operações do kernel de boundary effects B0 | 39/320 (25 aceitos + 14 rejeitados) |
| casos/operações do kernel de packages e releases P0 | 44/379 (22 aceitos + 22 rejeitados) |
| casos/operações do workflow single-file PYN1 | 95/546 (23 aceitos + 72 rejeitados) |
| casos/operações da sessão transacional PYN2 | 67/287 (53 aceitos + 14 rejeitados) |
| casos/operações de apresentação PYN3 | 24/69 (8 aceitos + 16 rejeitados; host oracle não executa W) |
| casos/operações do adapter Jupyter PYN3 | 30/98 (16 aceitos + 14 rejeitados; host oracle não executa W) |
| casos/operações do export notebook PYN3 | 18/49 (5 aceitos + 13 rejeitados; host oracle não executa W) |
| casos do container WMeta1 W0 | 42 (5 aceitos + 37 rejeitados; 2 readers independentes) |
| casos/operações do carrier tabular TAB0 | 64/155 (22 aceitos + 42 rejeitados; host oracle não executa W) |
| casos/operações dos adapters tabulares TAB1 | 84/184 (35 aceitos + 49 rejeitados; host oracle não executa W) |
| casos/operações do carrier DLPack PYN4 | 74/325 (25 aceitos + 49 rejeitados; host oracle não executa W) |
| casos do corpus semântico S0 | 108 (54 positivos + 54 negativos) |
| outcomes SemanticResult S0 | 108 |
| snapshots de diagnostic D0 | 54 |
| snapshots F0 no formato D0 | 21 |
| codes D0 catalogados | 237/153 |
| sources W no root do Última Luz | 88 |
| sources W em todo o Última Luz | 96 |
| sources W no rascunho da std | 22 |
| módulos/APIs catalogados da std SDK0 | 22/320 |
| superfícies qualificadas da std usadas pelo Última Luz | 78 |
| requisitos do Última Luz com contrato std SDK0 | 24/24 |
| requisitos do Última Luz ausentes na std SDK0 | 0/24 |

A estimativa de tokens usa bytes divididos por quatro. Use o valor somente para planejar leitura.

## Cobertura do catálogo D0

| Família | Catalogados | Referenciados |
|---|---:|---:|
| BORROW | 10 | 10 |
| CAPABILITY | 1 | 1 |
| CONST | 7 | 7 |
| CONTRACT | 5 | 5 |
| DIAGNOSTIC | 1 | 1 |
| DOC | 2 | 2 |
| EFFECT | 2 | 2 |
| EXPR | 4 | 4 |
| FLOW | 2 | 2 |
| FMT | 2 | 2 |
| GENERIC | 5 | 5 |
| INIT | 1 | 1 |
| LABEL | 3 | 3 |
| LEX | 1 | 1 |
| MATCH | 3 | 3 |
| MEMORY | 1 | 1 |
| MOVE | 1 | 1 |
| OWNERSHIP | 6 | 6 |
| PARSE | 29 | 29 |
| PATTERN | 6 | 6 |
| PLACEMENT | 3 | 3 |
| PROCESS | 3 | 3 |
| SCRIPT | 16 | 16 |
| SEM | 1 | 1 |
| SESSION | 28 | 28 |
| STD | 1 | 1 |
| SUSPEND | 4 | 4 |
| TYPE | 3 | 3 |
| USE | 1 | 1 |
| WIRE | 1 | 1 |

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 15–219 | 2400 | Como ler este documento |
| 1 | 220–243 | 300 | Limite da alegação |
| 2 | 244–269 | 400 | Invariantes |
| 3 | 270–3298 | 30800 | Contratos estáticos e orçamento de símbolos |
| 4 | 3299–3361 | 400 | Superfície integrada |
| 5 | 3362–3669 | 2700 | Source, nomes e edição |
| 6 | 3670–4091 | 3900 | Módulos, imports e visibilidade |
| 7 | 4092–4768 | 6300 | Bindings, funções e closures |
| 8 | 4769–7290 | 20000 | Tipos e conversões |
| 9 | 7291–8814 | 17300 | Memória, layout e alocação |
| 10 | 8815–8937 | 1400 | Property behaviors |
| 11 | 8938–9283 | 2700 | Erros, panic, OOM e cleanup |
| 12 | 9284–12042 | 29800 | Concorrência, paralelismo e execução |
| 13 | 12043–14236 | 22100 | Módulos de execução, services e entries |
| 14 | 14237–17960 | 47300 | Prelude e SDK |
| 15 | 17961–18670 | 7200 | Números, ranges e unidades |
| 16 | 18671–20272 | 13700 | Texto, bytes e collections |
| 17 | 20273–20617 | 4000 | Matrizes, tensors e ML |
| 18 | 20618–21132 | 5000 | Performance e custo |
| 19 | 21133–21514 | 4300 | FFI, unsafe e ilhas de linguagem |
| 20 | 21515–23085 | 17100 | Compilador e bootstrap |
| 21 | 23086–25121 | 19300 | Packages, builds e releases |
| 22 | 25122–25631 | 4900 | Tooling e interface para máquinas |
| 23 | 25632–27040 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 27041–27850 | 11100 | Design freeze e pendências |
| 25 | 27851–28061 | 1900 | Produto de referência Última Luz |
| 26 | 28062–28385 | 4000 | Plano de implementação |

## Navegação compacta de RATIONALE

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 1 | 14–2539 | 38800 | Evidência comparativa |
| 2 | 2540–2570 | 500 | Proveniência |
| 3 | 2571–3796 | 68600 | Ledger |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 15–7290 | 67200 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 7291–14236 | 73300 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 14237–21514 | 81500 | módulos, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 21515–27040 | 57900 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26 | 27041–28385 | 17000 | freeze, Última Luz, gates e roadmap |

O bundle agrupa seções para planejamento; os intervalos não são uma nova autoridade.

## Classificação de viabilidade

| Classe | Famílias |
|---|---:|
| Pesquisa | 1 |
| Possível agora | 86 |
| Possível por transport profile | 1 |
| Provável após C | 1 |
| Provável | 69 |
| Rejeitado na baseline | 2 |
| Rejeitado por enquanto | 6 |
| Rejeitado | 16 |

## Pesquisas explícitas

- barreira cíclica/reutilizável — Pesquisa

## Comandos de leitura

```powershell
bun tooling/design-slice.mjs --section 12
bun tooling/design-slice.mjs --heading 12.13
bun tooling/design-slice.mjs --id W-711 --context 2
bun tooling/design-slice.mjs --rationale-heading 1.3
rg -n -C 4 'transaction' DESIGN.md
bun tooling/design-index.mjs --check
```

