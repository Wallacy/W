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
| linhas de `DESIGN.md` | 27099 |
| tokens aproximados de `DESIGN.md` | 309400 |
| seções numeradas | 30 |
| seções terminais com evidência local | 325/325 |
| decisões | 906 (W-001–W-906) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 54 |
| casos de substituição estruturados | 54/54 |
| formas R0 com baseline estática | 121 |
| surface lexemes das formas vigentes R0 | 1029 total; mediana 15.5; máximo 50 |
| bundles executáveis R1 | 4 |
| variantes/tarefas R1 | 8/16 |
| casos do corpus Tree-sitter | 87 |
| pares canônicos do formatter F0 | 17 |
| casos/operações do kernel de memória M0 | 21/61 (8 aceitos + 13 rejeitados) |
| casos/operações do kernel de execução E0 | 28/280 (17 aceitos + 11 rejeitados; 8/8 origens happens-before) |
| casos/operações do kernel de boundary effects B0 | 39/320 (25 aceitos + 14 rejeitados) |
| casos/operações do kernel de packages e releases P0 | 44/379 (22 aceitos + 22 rejeitados) |
| casos do corpus semântico S0 | 84 (42 positivos + 42 negativos) |
| outcomes SemanticResult S0 | 84 |
| snapshots de diagnostic D0 | 42 |
| snapshots F0 no formato D0 | 17 |
| codes D0 catalogados | 73/73 |
| sources W no root do Última Luz | 76 |
| sources W em todo o Última Luz | 84 |
| sources W no rascunho da std | 14 |
| módulos/APIs catalogados da std SDK0 | 14/161 |
| superfícies qualificadas da std usadas pelo Última Luz | 42 |
| requisitos do Última Luz com contrato std SDK0 | 9/9 |
| requisitos do Última Luz ausentes na std SDK0 | 0/9 |

A estimativa de tokens usa bytes divididos por quatro. Use o valor somente para planejar leitura.

## Cobertura do catálogo D0

| Família | Catalogados | Referenciados |
|---|---:|---:|
| BORROW | 1 | 1 |
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
| OWNERSHIP | 1 | 1 |
| PARSE | 27 | 27 |
| PATTERN | 6 | 6 |
| SEM | 1 | 1 |
| TYPE | 3 | 3 |
| USE | 1 | 1 |
| WIRE | 1 | 1 |

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 14–200 | 2200 | Como ler este documento |
| 1 | 201–224 | 300 | Limite da alegação |
| 2 | 225–249 | 400 | Invariantes |
| 3 | 250–3164 | 28200 | Contratos estáticos e orçamento de símbolos |
| 4 | 3165–3227 | 400 | Superfície integrada |
| 5 | 3228–3519 | 2500 | Source, nomes e edição |
| 6 | 3520–3958 | 4100 | Módulos, imports e visibilidade |
| 7 | 3959–4579 | 5700 | Bindings, funções e closures |
| 8 | 4580–6957 | 18400 | Tipos e conversões |
| 9 | 6958–8138 | 12700 | Memória, layout e alocação |
| 10 | 8139–8182 | 400 | Property behaviors |
| 11 | 8183–8537 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 8538–10615 | 20500 | Concorrência, paralelismo e execução |
| 13 | 10616–12999 | 22900 | Módulos de execução, services e entries |
| 14 | 13000–16533 | 44000 | Prelude e SDK |
| 15 | 16534–17247 | 7200 | Números, ranges e unidades |
| 16 | 17248–18840 | 13500 | Texto, bytes e collections |
| 17 | 18841–18979 | 1200 | Matrizes, tensors e ML |
| 18 | 18980–19485 | 4800 | Performance e custo |
| 19 | 19486–19817 | 3700 | FFI, unsafe e ilhas de linguagem |
| 20 | 19818–21024 | 12400 | Compilador e bootstrap |
| 21 | 21025–23100 | 19800 | Packages, builds e releases |
| 22 | 23101–23573 | 4400 | Tooling e interface para máquinas |
| 23 | 23574–25002 | 16900 | Protocolos e pesquisas de ecossistema |
| 24 | 25003–25345 | 8500 | Classificação de viabilidade |
| 25 | 25346–25556 | 1900 | Produto de referência Última Luz |
| 26 | 25557–25846 | 3300 | Protocolo de revisão |
| 27 | 25847–26151 | 3700 | Plano de implementação |
| 28 | 26152–26182 | 500 | Relação com a consolidação histórica |
| 29 | 26183–27099 | 43200 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–6957 | 62200 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 6958–12999 | 59400 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 13000–19817 | 74400 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 19818–25002 | 53500 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 25003–27099 | 61100 | viabilidade, Última Luz, gates, roadmap e ledger |

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

