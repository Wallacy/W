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
| linhas de `DESIGN.md` | 27467 |
| tokens aproximados de `DESIGN.md` | 316600 |
| seções numeradas | 30 |
| seções terminais com evidência local | 327/327 |
| decisões | 921 (W-001–W-921) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 54 |
| casos de substituição estruturados | 54/54 |
| decisões referenciadas por casos R0 | 88/921 |
| formas R0 com baseline estática | 121 |
| surface lexemes das formas vigentes R0 | 1029 total; mediana 15.5; máximo 50 |
| bundles executáveis R1 | 5 |
| variantes/tarefas R1 | 10/20 |
| casos R0 promovidos a R1 | 11/54 |
| casos do corpus Tree-sitter | 87 |
| pares canônicos do formatter F0 | 17 |
| casos/operações do kernel de memória M1 | 135/442 (60 aceitos + 75 rejeitados) |
| casos/operações do kernel de execução E0 | 28/280 (17 aceitos + 11 rejeitados; 8/8 origens happens-before) |
| casos/operações do kernel de boundary effects B0 | 39/320 (25 aceitos + 14 rejeitados) |
| casos/operações do kernel de packages e releases P0 | 44/379 (22 aceitos + 22 rejeitados) |
| casos do corpus semântico S0 | 86 (43 positivos + 43 negativos) |
| outcomes SemanticResult S0 | 86 |
| snapshots de diagnostic D0 | 43 |
| snapshots F0 no formato D0 | 17 |
| codes D0 catalogados | 81/81 |
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
| BORROW | 9 | 9 |
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
| 7 | 3959–4590 | 5900 | Bindings, funções e closures |
| 8 | 4591–6974 | 18500 | Tipos e conversões |
| 9 | 6975–8364 | 16000 | Memória, layout e alocação |
| 10 | 8365–8408 | 400 | Property behaviors |
| 11 | 8409–8763 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 8764–10864 | 20800 | Concorrência, paralelismo e execução |
| 13 | 10865–13248 | 22900 | Módulos de execução, services e entries |
| 14 | 13249–16782 | 44000 | Prelude e SDK |
| 15 | 16783–17496 | 7200 | Números, ranges e unidades |
| 16 | 17497–19097 | 13600 | Texto, bytes e collections |
| 17 | 19098–19236 | 1200 | Matrizes, tensors e ML |
| 18 | 19237–19742 | 4800 | Performance e custo |
| 19 | 19743–20085 | 3900 | FFI, unsafe e ilhas de linguagem |
| 20 | 20086–21328 | 13100 | Compilador e bootstrap |
| 21 | 21329–23404 | 19800 | Packages, builds e releases |
| 22 | 23405–23877 | 4400 | Tooling e interface para máquinas |
| 23 | 23878–25306 | 16900 | Protocolos e pesquisas de ecossistema |
| 24 | 25307–25649 | 8500 | Classificação de viabilidade |
| 25 | 25650–25860 | 1900 | Produto de referência Última Luz |
| 26 | 25861–26189 | 3800 | Protocolo de revisão |
| 27 | 26190–26504 | 3900 | Plano de implementação |
| 28 | 26505–26535 | 500 | Relação com a consolidação histórica |
| 29 | 26536–27467 | 45000 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–6974 | 62500 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 6975–13248 | 63000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 13249–20085 | 74700 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 20086–25306 | 54200 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 25307–27467 | 63600 | viabilidade, Última Luz, gates, roadmap e ledger |

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

