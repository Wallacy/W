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
| linhas de `DESIGN.md` | 27956 |
| tokens aproximados de `DESIGN.md` | 325000 |
| seções numeradas | 30 |
| seções terminais com evidência local | 330/330 |
| decisões | 940 (W-001–W-940) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 55 |
| casos de substituição estruturados | 55/55 |
| decisões referenciadas por casos R0 | 93/940 |
| decisões classificadas para design freeze | 142/940 (93 source + 63 oracle + 6 explícitas; 20 overlaps) |
| decisões ainda sem classe de freeze | 798 |
| decisões com múltiplos eixos obrigatórios | 2 |
| formas R0 com baseline estática | 124 |
| surface lexemes das formas vigentes R0 | 1070 total; mediana 16; máximo 50 |
| bundles executáveis R1 | 8 |
| variantes/tarefas R1 | 17/32 |
| casos R0 promovidos a R1 | 17/55 |
| casos do corpus Tree-sitter | 87 |
| pares canônicos do formatter F0 | 19 |
| casos/operações do kernel de memória M1 | 164/579 (70 aceitos + 94 rejeitados) |
| casos/operações do kernel de execução E0 | 50/451 (28 aceitos + 22 rejeitados; 8/8 origens happens-before) |
| casos/operações do kernel de boundary effects B0 | 39/320 (25 aceitos + 14 rejeitados) |
| casos/operações do kernel de packages e releases P0 | 44/379 (22 aceitos + 22 rejeitados) |
| casos do corpus semântico S0 | 92 (46 positivos + 46 negativos) |
| outcomes SemanticResult S0 | 92 |
| snapshots de diagnostic D0 | 46 |
| snapshots F0 no formato D0 | 19 |
| codes D0 catalogados | 83/83 |
| sources W no root do Última Luz | 77 |
| sources W em todo o Última Luz | 85 |
| sources W no rascunho da std | 14 |
| módulos/APIs catalogados da std SDK0 | 14/161 |
| superfícies qualificadas da std usadas pelo Última Luz | 42 |
| requisitos do Última Luz com contrato std SDK0 | 9/9 |
| requisitos do Última Luz ausentes na std SDK0 | 0/9 |

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
| PARSE | 27 | 27 |
| PATTERN | 6 | 6 |
| SEM | 1 | 1 |
| TYPE | 3 | 3 |
| USE | 1 | 1 |
| WIRE | 1 | 1 |

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 14–204 | 2300 | Como ler este documento |
| 1 | 205–228 | 300 | Limite da alegação |
| 2 | 229–253 | 400 | Invariantes |
| 3 | 254–3180 | 28400 | Contratos estáticos e orçamento de símbolos |
| 4 | 3181–3243 | 400 | Superfície integrada |
| 5 | 3244–3535 | 2500 | Source, nomes e edição |
| 6 | 3536–3974 | 4100 | Módulos, imports e visibilidade |
| 7 | 3975–4638 | 6200 | Bindings, funções e closures |
| 8 | 4639–7064 | 19000 | Tipos e conversões |
| 9 | 7065–8563 | 17400 | Memória, layout e alocação |
| 10 | 8564–8607 | 400 | Property behaviors |
| 11 | 8608–8962 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 8963–11197 | 22400 | Concorrência, paralelismo e execução |
| 13 | 11198–13581 | 22900 | Módulos de execução, services e entries |
| 14 | 13582–17115 | 44000 | Prelude e SDK |
| 15 | 17116–17829 | 7200 | Números, ranges e unidades |
| 16 | 17830–19430 | 13600 | Texto, bytes e collections |
| 17 | 19431–19569 | 1200 | Matrizes, tensors e ML |
| 18 | 19570–20080 | 4900 | Performance e custo |
| 19 | 20081–20423 | 3900 | FFI, unsafe e ilhas de linguagem |
| 20 | 20424–21694 | 13500 | Compilador e bootstrap |
| 21 | 21695–23770 | 19800 | Packages, builds e releases |
| 22 | 23771–24243 | 4400 | Tooling e interface para máquinas |
| 23 | 24244–25672 | 16900 | Protocolos e pesquisas de ecossistema |
| 24 | 25673–26037 | 8900 | Classificação de viabilidade |
| 25 | 26038–26248 | 1900 | Produto de referência Última Luz |
| 26 | 26249–26659 | 4800 | Protocolo de revisão |
| 27 | 26660–26974 | 3900 | Plano de implementação |
| 28 | 26975–27005 | 500 | Relação com a consolidação histórica |
| 29 | 27006–27956 | 47200 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–7064 | 63600 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 7065–13581 | 66000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 13582–20423 | 74800 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 20424–25672 | 54600 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 25673–27956 | 67200 | viabilidade, Última Luz, gates, roadmap e ledger |

O bundle agrupa seções para planejamento; os intervalos não são uma nova autoridade.

## Classificação de viabilidade

| Classe | Famílias |
|---|---:|
| Possível agora | 80 |
| Possível por transport profile | 1 |
| Provável T1 | 5 |
| Provável T2 | 7 |
| Provável após C | 1 |
| Provável | 61 |
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

