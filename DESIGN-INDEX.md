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
| linhas de `DESIGN.md` | 24993 |
| tokens aproximados de `DESIGN.md` | 277000 |
| seções numeradas | 30 |
| seções terminais com evidência local | 314/314 |
| decisões | 895 (W-001–W-895) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 54 |
| casos de substituição estruturados | 54/54 |
| formas R0 com baseline estática | 121 |
| surface lexemes das formas vigentes R0 | 1023 total; mediana 15.5; máximo 50 |
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
| sources W no root do Última Luz | 69 |
| sources W em todo o Última Luz | 77 |
| sources W no rascunho da std | 9 |
| módulos/APIs catalogados da std SDK0 | 9/68 |
| superfícies qualificadas da std usadas pelo Última Luz | 27 |
| requisitos do Última Luz com contrato std SDK0 | 9/9 |
| requisitos do Última Luz ausentes na std SDK0 | 6/9 |

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
| 8 | 4580–6957 | 18300 | Tipos e conversões |
| 9 | 6958–8138 | 12700 | Memória, layout e alocação |
| 10 | 8139–8182 | 400 | Property behaviors |
| 11 | 8183–8537 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 8538–10573 | 19900 | Concorrência, paralelismo e execução |
| 13 | 10574–12957 | 22900 | Módulos de execução, services e entries |
| 14 | 12958–14814 | 20100 | Prelude e SDK |
| 15 | 14815–15276 | 4600 | Números, ranges e unidades |
| 16 | 15277–16869 | 13500 | Texto, bytes e collections |
| 17 | 16870–17008 | 1200 | Matrizes, tensors e ML |
| 18 | 17009–17514 | 4800 | Performance e custo |
| 19 | 17515–17775 | 2700 | FFI, unsafe e ilhas de linguagem |
| 20 | 17776–18982 | 12400 | Compilador e bootstrap |
| 21 | 18983–21012 | 19200 | Packages, builds e releases |
| 22 | 21013–21485 | 4400 | Tooling e interface para máquinas |
| 23 | 21486–22907 | 16800 | Protocolos e pesquisas de ecossistema |
| 24 | 22908–23250 | 8500 | Classificação de viabilidade |
| 25 | 23251–23461 | 1900 | Produto de referência Última Luz |
| 26 | 23462–23751 | 3300 | Protocolo de revisão |
| 27 | 23752–24056 | 3700 | Plano de implementação |
| 28 | 24057–24087 | 500 | Relação com a consolidação histórica |
| 29 | 24088–24993 | 39600 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–6957 | 62100 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 6958–12957 | 58800 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 12958–17775 | 46900 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 17776–22907 | 52800 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 22908–24993 | 57500 | viabilidade, Última Luz, gates, roadmap e ledger |

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

