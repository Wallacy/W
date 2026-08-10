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
| linhas de `DESIGN.md` | 29453 |
| tokens aproximados de `DESIGN.md` | 313500 |
| linhas de `RATIONALE.md` | 2076 |
| tokens aproximados de `RATIONALE.md` | 76800 |
| seções numeradas | 27 |
| seções terminais com evidência local | 331/331 |
| decisões | 1159 (W-001–W-1159) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 68 |
| casos de substituição estruturados | 68/68 |
| decisões referenciadas por casos R0 | 120/1159 |
| decisões classificadas para design freeze | 356/1159 (120 source + 269 oracle + 8 explícitas; 41 overlaps) |
| decisões ainda sem classe de freeze | 803 |
| decisões com múltiplos eixos obrigatórios | 2 |
| formas R0 com baseline estática | 165 |
| surface lexemes das formas vigentes R0 | 1379 total; mediana 16; máximo 52 |
| bundles executáveis R1 | 20 |
| variantes/tarefas R1 | 48/80 |
| casos R0 promovidos a R1 | 31/68 |
| casos do corpus Tree-sitter | 100 |
| pares canônicos do formatter F0 | 20 |
| casos/operações do kernel de memória M1 | 165/580 (70 aceitos + 95 rejeitados) |
| casos/operações do kernel de allocation físico A0 | 48/123 (15 aceitos + 33 rejeitados) |
| casos/operações do kernel de layout e ABI L0 | 78/96 (27 aceitos + 51 rejeitados) |
| casos/operações do kernel de execução E0 | 50/451 (28 aceitos + 22 rejeitados; 8/8 origens happens-before) |
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
| snapshots F0 no formato D0 | 20 |
| codes D0 catalogados | 189/157 |
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
| EFFECT | 2 | 2 |
| EXPORT | 7 | 7 |
| EXPR | 4 | 4 |
| FLOW | 2 | 2 |
| FMT | 2 | 2 |
| GENERIC | 5 | 5 |
| INIT | 1 | 1 |
| JUPYTER | 8 | 8 |
| LEX | 1 | 1 |
| MATCH | 3 | 3 |
| MOVE | 1 | 1 |
| OWNERSHIP | 2 | 2 |
| PARSE | 29 | 29 |
| PATTERN | 6 | 6 |
| PRESENTATION | 10 | 10 |
| SCRIPT | 16 | 16 |
| SEM | 1 | 1 |
| SESSION | 28 | 28 |
| TYPE | 3 | 3 |
| USE | 1 | 1 |
| WIRE | 1 | 1 |

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 19–216 | 2400 | Como ler este documento |
| 1 | 217–240 | 300 | Limite da alegação |
| 2 | 241–265 | 400 | Invariantes |
| 3 | 266–3405 | 32100 | Contratos estáticos e orçamento de símbolos |
| 4 | 3406–3468 | 400 | Superfície integrada |
| 5 | 3469–3760 | 2500 | Source, nomes e edição |
| 6 | 3761–4199 | 4100 | Módulos, imports e visibilidade |
| 7 | 4200–4863 | 6200 | Bindings, funções e closures |
| 8 | 4864–7400 | 20300 | Tipos e conversões |
| 9 | 7401–9214 | 21000 | Memória, layout e alocação |
| 10 | 9215–9258 | 400 | Property behaviors |
| 11 | 9259–9617 | 3000 | Erros, panic, OOM e cleanup |
| 12 | 9618–11989 | 24400 | Concorrência, paralelismo e execução |
| 13 | 11990–14393 | 23200 | Módulos de execução, services e entries |
| 14 | 14394–18279 | 49300 | Prelude e SDK |
| 15 | 18280–18993 | 7200 | Números, ranges e unidades |
| 16 | 18994–20597 | 13700 | Texto, bytes e collections |
| 17 | 20598–20962 | 4400 | Matrizes, tensors e ML |
| 18 | 20963–21477 | 5000 | Performance e custo |
| 19 | 21478–21862 | 4400 | FFI, unsafe e ilhas de linguagem |
| 20 | 21863–23495 | 18000 | Compilador e bootstrap |
| 21 | 23496–25605 | 20200 | Packages, builds e releases |
| 22 | 25606–26096 | 4700 | Tooling e interface para máquinas |
| 23 | 26097–27525 | 16900 | Protocolos e pesquisas de ecossistema |
| 24 | 27526–28920 | 24400 | Classificação de viabilidade |
| 25 | 28921–29131 | 1900 | Produto de referência Última Luz |
| 26 | 29132–29453 | 4000 | Plano de implementação |

## Navegação compacta de RATIONALE

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 1 | 9–875 | 11500 | Evidência comparativa |
| 2 | 876–906 | 500 | Proveniência |
| 3 | 907–2076 | 64800 | Ledger |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 19–7400 | 68700 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 7401–14393 | 72000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 14394–21862 | 84000 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 21863–27525 | 59800 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26 | 27526–29453 | 30300 | viabilidade, Última Luz, gates e roadmap |

O bundle agrupa seções para planejamento; os intervalos não são uma nova autoridade.

## Classificação de viabilidade

| Classe | Famílias |
|---|---:|
| Possível agora | 81 |
| Possível por transport profile | 1 |
| Provável T1 | 5 |
| Provável T2 | 7 |
| Provável após C | 1 |
| Provável | 60 |
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

