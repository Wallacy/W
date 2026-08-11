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
| linhas de `DESIGN.md` | 29670 |
| tokens aproximados de `DESIGN.md` | 316400 |
| linhas de `RATIONALE.md` | 2308 |
| tokens aproximados de `RATIONALE.md` | 81300 |
| seções numeradas | 27 |
| seções terminais com evidência local | 334/334 |
| decisões | 1176 (W-001–W-1176) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 68 |
| casos de substituição estruturados | 68/68 |
| decisões referenciadas por casos R0 | 123/1176 |
| decisões classificadas para design freeze | 361/1176 (123 source + 274 oracle + 8 explícitas; 44 overlaps) |
| decisões ainda sem classe de freeze | 815 |
| decisões com múltiplos eixos obrigatórios | 2 |
| formas R0 com baseline estática | 165 |
| surface lexemes das formas vigentes R0 | 1379 total; mediana 16; máximo 52 |
| bundles executáveis R1 | 20 |
| variantes/tarefas R1 | 48/80 |
| casos R0 promovidos a R1 | 31/68 |
| casos do corpus Tree-sitter | 101 |
| pares canônicos do formatter F0 | 21 |
| casos/operações do kernel de memória M1 | 165/580 (70 aceitos + 95 rejeitados) |
| casos/operações do kernel de allocation físico A0 | 48/123 (15 aceitos + 33 rejeitados) |
| casos/operações do kernel de layout e ABI L0 | 78/96 (27 aceitos + 51 rejeitados) |
| casos/operações do kernel de execução E0 | 57/527 (31 aceitos + 26 rejeitados; 10/10 origens happens-before) |
| casos/operações do kernel de runtime closure E1 | 41/473 (19 aceitos + 22 rejeitados; sete testes host) |
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
| casos do corpus semântico S0 | 102 (51 positivos + 51 negativos) |
| outcomes SemanticResult S0 | 102 |
| snapshots de diagnostic D0 | 51 |
| snapshots F0 no formato D0 | 21 |
| codes D0 catalogados | 206/174 |
| sources W no root do Última Luz | 86 |
| sources W em todo o Última Luz | 94 |
| sources W no rascunho da std | 21 |
| módulos/APIs catalogados da std SDK0 | 21/315 |
| superfícies qualificadas da std usadas pelo Última Luz | 78 |
| requisitos do Última Luz com contrato std SDK0 | 20/20 |
| requisitos do Última Luz ausentes na std SDK0 | 0/20 |

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
| EXPORT | 7 | 7 |
| EXPR | 4 | 4 |
| FLOW | 2 | 2 |
| FMT | 2 | 2 |
| GENERIC | 5 | 5 |
| INIT | 1 | 1 |
| JUPYTER | 8 | 8 |
| LABEL | 3 | 3 |
| LEX | 1 | 1 |
| MATCH | 3 | 3 |
| MOVE | 1 | 1 |
| OWNERSHIP | 3 | 3 |
| PARSE | 29 | 29 |
| PATTERN | 6 | 6 |
| PLACEMENT | 3 | 3 |
| PRESENTATION | 10 | 10 |
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
| 0 | 15–245 | 2800 | Como ler este documento |
| 1 | 246–269 | 300 | Limite da alegação |
| 2 | 270–294 | 400 | Invariantes |
| 3 | 295–3433 | 32100 | Contratos estáticos e orçamento de símbolos |
| 4 | 3434–3496 | 400 | Superfície integrada |
| 5 | 3497–3804 | 2700 | Source, nomes e edição |
| 6 | 3805–4244 | 4100 | Módulos, imports e visibilidade |
| 7 | 4245–4932 | 6500 | Bindings, funções e closures |
| 8 | 4933–7484 | 20500 | Tipos e conversões |
| 9 | 7485–9157 | 19300 | Memória, layout e alocação |
| 10 | 9158–9201 | 400 | Property behaviors |
| 11 | 9202–9546 | 2700 | Erros, panic, OOM e cleanup |
| 12 | 9547–12121 | 26800 | Concorrência, paralelismo e execução |
| 13 | 12122–14565 | 23600 | Módulos de execução, services e entries |
| 14 | 14566–18443 | 49500 | Prelude e SDK |
| 15 | 18444–19157 | 7300 | Números, ranges e unidades |
| 16 | 19158–20761 | 13700 | Texto, bytes e collections |
| 17 | 20762–21126 | 4400 | Matrizes, tensors e ML |
| 18 | 21127–21641 | 5000 | Performance e custo |
| 19 | 21642–22026 | 4300 | FFI, unsafe e ilhas de linguagem |
| 20 | 22027–23659 | 18000 | Compilador e bootstrap |
| 21 | 23660–25768 | 20200 | Packages, builds e releases |
| 22 | 25769–26278 | 4900 | Tooling e interface para máquinas |
| 23 | 26279–27707 | 16900 | Protocolos e pesquisas de ecossistema |
| 24 | 27708–29137 | 24900 | Classificação de viabilidade |
| 25 | 29138–29348 | 1900 | Produto de referência Última Luz |
| 26 | 29349–29670 | 4000 | Plano de implementação |

## Navegação compacta de RATIONALE

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 1 | 14–1084 | 14600 | Evidência comparativa |
| 2 | 1085–1115 | 500 | Proveniência |
| 3 | 1116–2308 | 66200 | Ledger |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 15–7484 | 69800 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 7485–14565 | 72800 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 14566–22026 | 84200 | módulos, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 22027–27707 | 60000 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26 | 27708–29670 | 30800 | viabilidade, Última Luz, gates e roadmap |

O bundle agrupa seções para planejamento; os intervalos não são uma nova autoridade.

## Classificação de viabilidade

| Classe | Famílias |
|---|---:|
| Possível agora | 81 |
| Possível por transport profile | 1 |
| Provável após C | 1 |
| Provável | 72 |
| Rejeitado na baseline | 2 |
| Rejeitado por enquanto | 6 |
| Rejeitado | 15 |

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

