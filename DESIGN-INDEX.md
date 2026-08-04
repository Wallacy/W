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
| linhas de `DESIGN.md` | 23641 |
| tokens aproximados de `DESIGN.md` | 256300 |
| seções numeradas | 30 |
| seções terminais com evidência local | 304/304 |
| decisões | 824 (W-001–W-824) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| casos de ratificação comparativa | 52 |
| casos do corpus Tree-sitter | 87 |
| pares canônicos do formatter F0 | 11 |
| casos do corpus semântico S0 | 32 (16 positivos + 16 negativos) |
| outcomes SemanticResult S0 | 32 |
| snapshots de diagnostic D0 | 16 |
| snapshots F0 no formato D0 | 11 |
| codes D0 catalogados | 44/80 |
| sources W no root do Última Luz | 69 |
| sources W em todo o Última Luz | 77 |
| sources W no rascunho da std | 9 |

A estimativa de tokens usa bytes divididos por quatro. Use o valor somente para planejar leitura.

## Cobertura do catálogo D0

| Família | Catalogados | Referenciados |
|---|---:|---:|
| BORROW | 1 | 1 |
| BUILD | 0 | 1 |
| CAPABILITY | 1 | 1 |
| CONST | 0 | 7 |
| CONTRACT | 5 | 5 |
| DIAGNOSTIC | 1 | 1 |
| DOC | 0 | 1 |
| EFFECT | 1 | 3 |
| EXPR | 0 | 7 |
| FFI | 0 | 1 |
| FLOW | 2 | 2 |
| FMT | 2 | 2 |
| GENERIC | 0 | 2 |
| INIT | 1 | 1 |
| LEX | 1 | 1 |
| MATCH | 0 | 3 |
| MOVE | 1 | 1 |
| OWNERSHIP | 0 | 1 |
| PARSE | 25 | 25 |
| PATTERN | 0 | 7 |
| SEM | 1 | 1 |
| TYPE | 1 | 4 |
| USE | 1 | 1 |
| WIRE | 0 | 1 |

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 14–200 | 2200 | Como ler este documento |
| 1 | 201–224 | 300 | Limite da alegação |
| 2 | 225–249 | 400 | Invariantes |
| 3 | 250–3058 | 26800 | Contratos estáticos e orçamento de símbolos |
| 4 | 3059–3121 | 400 | Superfície integrada |
| 5 | 3122–3397 | 2400 | Source, nomes e edição |
| 6 | 3398–3836 | 4100 | Módulos, imports e visibilidade |
| 7 | 3837–4457 | 5700 | Bindings, funções e closures |
| 8 | 4458–6796 | 18000 | Tipos e conversões |
| 9 | 6797–7977 | 12700 | Memória, layout e alocação |
| 10 | 7978–8021 | 400 | Property behaviors |
| 11 | 8022–8376 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 8377–10382 | 19500 | Concorrência, paralelismo e execução |
| 13 | 10383–12735 | 22500 | Módulos de execução, services e entries |
| 14 | 12736–13840 | 11000 | Prelude e SDK |
| 15 | 13841–14302 | 4600 | Números, ranges e unidades |
| 16 | 14303–15895 | 13500 | Texto, bytes e collections |
| 17 | 15896–16034 | 1200 | Matrizes, tensors e ML |
| 18 | 16035–16540 | 4800 | Performance e custo |
| 19 | 16541–16801 | 2700 | FFI, unsafe e ilhas de linguagem |
| 20 | 16802–17981 | 12100 | Compilador e bootstrap |
| 21 | 17982–19924 | 18100 | Packages, builds e releases |
| 22 | 19925–20391 | 4300 | Tooling e interface para máquinas |
| 23 | 20392–21798 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 21799–22104 | 7700 | Classificação de viabilidade |
| 25 | 22105–22315 | 1900 | Produto de referência Última Luz |
| 26 | 22316–22470 | 1600 | Protocolo de revisão |
| 27 | 22471–22775 | 3700 | Plano de implementação |
| 28 | 22776–22806 | 500 | Relação com a consolidação histórica |
| 29 | 22807–23641 | 34800 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–6796 | 60300 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 6797–12735 | 58000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 12736–16801 | 37800 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 16802–21798 | 51100 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 21799–23641 | 50200 | viabilidade, Última Luz, gates, roadmap e ledger |

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

