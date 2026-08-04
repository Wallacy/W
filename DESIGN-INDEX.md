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
| linhas de `DESIGN.md` | 23862 |
| tokens aproximados de `DESIGN.md` | 261100 |
| seções numeradas | 30 |
| seções terminais com evidência local | 306/306 |
| decisões | 864 (W-001–W-864) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 52 |
| casos de substituição estruturados | 52/52 |
| casos do corpus Tree-sitter | 87 |
| pares canônicos do formatter F0 | 17 |
| casos do corpus semântico S0 | 84 (42 positivos + 42 negativos) |
| outcomes SemanticResult S0 | 84 |
| snapshots de diagnostic D0 | 42 |
| snapshots F0 no formato D0 | 17 |
| codes D0 catalogados | 73/73 |
| sources W no root do Última Luz | 69 |
| sources W em todo o Última Luz | 77 |
| sources W no rascunho da std | 9 |

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
| 5 | 3228–3503 | 2400 | Source, nomes e edição |
| 6 | 3504–3942 | 4100 | Módulos, imports e visibilidade |
| 7 | 3943–4563 | 5700 | Bindings, funções e closures |
| 8 | 4564–6941 | 18300 | Tipos e conversões |
| 9 | 6942–8122 | 12700 | Memória, layout e alocação |
| 10 | 8123–8166 | 400 | Property behaviors |
| 11 | 8167–8521 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 8522–10527 | 19500 | Concorrência, paralelismo e execução |
| 13 | 10528–12880 | 22500 | Módulos de execução, services e entries |
| 14 | 12881–13985 | 11000 | Prelude e SDK |
| 15 | 13986–14447 | 4600 | Números, ranges e unidades |
| 16 | 14448–16040 | 13500 | Texto, bytes e collections |
| 17 | 16041–16179 | 1200 | Matrizes, tensors e ML |
| 18 | 16180–16685 | 4800 | Performance e custo |
| 19 | 16686–16946 | 2700 | FFI, unsafe e ilhas de linguagem |
| 20 | 16947–18126 | 12100 | Compilador e bootstrap |
| 21 | 18127–20069 | 18100 | Packages, builds e releases |
| 22 | 20070–20542 | 4400 | Tooling e interface para máquinas |
| 23 | 20543–21964 | 16800 | Protocolos e pesquisas de ecossistema |
| 24 | 21965–22270 | 7800 | Classificação de viabilidade |
| 25 | 22271–22481 | 1900 | Produto de referência Última Luz |
| 26 | 22482–22651 | 1900 | Protocolo de revisão |
| 27 | 22652–22956 | 3700 | Plano de implementação |
| 28 | 22957–22987 | 500 | Relação com a consolidação histórica |
| 29 | 22988–23862 | 37300 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–6941 | 62000 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 6942–12880 | 58000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 12881–16946 | 37800 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 16947–21964 | 51400 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 21965–23862 | 53100 | viabilidade, Última Luz, gates, roadmap e ledger |

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

