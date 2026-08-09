# Índice gerado do design W

> Gerado por `tooling/design-index.mjs`. Não edite este arquivo.
> `DESIGN.md` continua sendo a única fonte de verdade.

## Contexto mínimo

1. Leia este índice para localizar a seção necessária.
2. Leia somente o intervalo correspondente em `DESIGN.md`.
3. Busque o ID W quando a tarefa alterar uma decisão.
4. Abra o produto Última Luz somente para o exemplo afetado.
5. Não leia `tooling/tree-sitter-w/src/` como source. Essa pasta é gerada.

## Snapshot calculado

| Métrica | Valor |
|---|---:|
| linhas de `DESIGN.md` | 30519 |
| tokens aproximados de `DESIGN.md` | 373300 |
| seções numeradas | 30 |
| seções terminais com evidência local | 347/347 |
| decisões | 1106 (W-001–W-1106) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 68 |
| casos de substituição estruturados | 68/68 |
| decisões referenciadas por casos R0 | 120/1106 |
| decisões classificadas para design freeze | 314/1106 (120 source + 227 oracle + 8 explícitas; 41 overlaps) |
| decisões ainda sem classe de freeze | 792 |
| decisões com múltiplos eixos obrigatórios | 2 |
| formas R0 com baseline estática | 164 |
| surface lexemes das formas vigentes R0 | 1381 total; mediana 16; máximo 52 |
| bundles executáveis R1 | 15 |
| variantes/tarefas R1 | 34/60 |
| casos R0 promovidos a R1 | 25/68 |
| casos do corpus Tree-sitter | 93 |
| pares canônicos do formatter F0 | 20 |
| casos/operações do kernel de memória M1 | 165/580 (70 aceitos + 95 rejeitados) |
| casos/operações do kernel de allocation físico A0 | 48/123 (15 aceitos + 33 rejeitados) |
| casos/operações do kernel de layout e ABI L0 | 78/96 (27 aceitos + 51 rejeitados) |
| casos/operações do kernel de execução E0 | 50/451 (28 aceitos + 22 rejeitados; 8/8 origens happens-before) |
| casos/operações do kernel de runtime closure E1 | 41/473 (19 aceitos + 22 rejeitados; sete testes host) |
| casos/operações do kernel de boundary effects B0 | 39/320 (25 aceitos + 14 rejeitados) |
| casos/operações do kernel de packages e releases P0 | 44/379 (22 aceitos + 22 rejeitados) |
| casos/operações do workflow single-file PYN1 | 91/533 (22 aceitos + 69 rejeitados) |
| casos/operações da sessão transacional PYN2 | 67/287 (53 aceitos + 14 rejeitados) |
| casos do container WMeta1 W0 | 42 (5 aceitos + 37 rejeitados; 2 readers independentes) |
| casos/operações do carrier tabular TAB0 | 64/155 (22 aceitos + 42 rejeitados; host oracle não executa W) |
| casos/operações dos adapters tabulares TAB1 | 84/184 (35 aceitos + 49 rejeitados; host oracle não executa W) |
| casos do corpus semântico S0 | 92 (46 positivos + 46 negativos) |
| outcomes SemanticResult S0 | 92 |
| snapshots de diagnostic D0 | 46 |
| snapshots F0 no formato D0 | 20 |
| codes D0 catalogados | 129/129 |
| sources W no root do Última Luz | 84 |
| sources W em todo o Última Luz | 92 |
| sources W no rascunho da std | 18 |
| módulos/APIs catalogados da std SDK0 | 18/285 |
| superfícies qualificadas da std usadas pelo Última Luz | 67 |
| requisitos do Última Luz com contrato std SDK0 | 14/14 |
| requisitos do Última Luz ausentes na std SDK0 | 0/14 |

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
| EXPR | 4 | 4 |
| FLOW | 2 | 2 |
| FMT | 2 | 2 |
| GENERIC | 2 | 2 |
| INIT | 1 | 1 |
| LEX | 1 | 1 |
| MATCH | 3 | 3 |
| MOVE | 1 | 1 |
| OWNERSHIP | 2 | 2 |
| PARSE | 29 | 29 |
| PATTERN | 6 | 6 |
| SCRIPT | 16 | 16 |
| SEM | 1 | 1 |
| SESSION | 28 | 28 |
| TYPE | 3 | 3 |
| USE | 1 | 1 |
| WIRE | 1 | 1 |

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 14–211 | 2400 | Como ler este documento |
| 1 | 212–235 | 300 | Limite da alegação |
| 2 | 236–260 | 400 | Invariantes |
| 3 | 261–3356 | 31400 | Contratos estáticos e orçamento de símbolos |
| 4 | 3357–3419 | 400 | Superfície integrada |
| 5 | 3420–3711 | 2500 | Source, nomes e edição |
| 6 | 3712–4150 | 4100 | Módulos, imports e visibilidade |
| 7 | 4151–4814 | 6200 | Bindings, funções e closures |
| 8 | 4815–7245 | 19100 | Tipos e conversões |
| 9 | 7246–9038 | 20700 | Memória, layout e alocação |
| 10 | 9039–9082 | 400 | Property behaviors |
| 11 | 9083–9441 | 3000 | Erros, panic, OOM e cleanup |
| 12 | 9442–11802 | 24200 | Concorrência, paralelismo e execução |
| 13 | 11803–14187 | 23000 | Módulos de execução, services e entries |
| 14 | 14188–18073 | 49300 | Prelude e SDK |
| 15 | 18074–18787 | 7200 | Números, ranges e unidades |
| 16 | 18788–20391 | 13700 | Texto, bytes e collections |
| 17 | 20392–20537 | 1300 | Matrizes, tensors e ML |
| 18 | 20538–21052 | 5000 | Performance e custo |
| 19 | 21053–21437 | 4400 | FFI, unsafe e ilhas de linguagem |
| 20 | 21438–23040 | 17500 | Compilador e bootstrap |
| 21 | 23041–25150 | 20200 | Packages, builds e releases |
| 22 | 25151–25636 | 4600 | Tooling e interface para máquinas |
| 23 | 25637–27065 | 16900 | Protocolos e pesquisas de ecossistema |
| 24 | 27066–28135 | 20000 | Classificação de viabilidade |
| 25 | 28136–28346 | 1900 | Produto de referência Última Luz |
| 26 | 28347–29056 | 9100 | Protocolo de revisão |
| 27 | 29057–29371 | 3900 | Plano de implementação |
| 28 | 29372–29402 | 500 | Relação com a consolidação histórica |
| 29 | 29403–30519 | 61000 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–7245 | 66800 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 7246–14187 | 71300 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 14188–21437 | 80900 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 21438–27065 | 59200 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 27066–30519 | 96400 | viabilidade, Última Luz, gates, roadmap e ledger |

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
rg -n -C 4 'transaction' DESIGN.md
bun tooling/design-index.mjs --check
```

