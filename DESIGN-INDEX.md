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
| linhas de `DESIGN.md` | 22021 |
| tokens aproximados de `DESIGN.md` | 237200 |
| seções numeradas | 30 |
| seções terminais com evidência local | 293/293 |
| decisões | 763 (W-001–W-763) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 3 |
| casos de ratificação comparativa | 47 |
| casos do corpus Tree-sitter | 70 |
| sources W no root do Última Luz | 69 |
| sources W em todo o Última Luz | 77 |
| sources W no rascunho da std | 9 |

A estimativa de tokens usa bytes divididos por quatro. Use o valor somente para planejar leitura.

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 14–200 | 2200 | Como ler este documento |
| 1 | 201–224 | 300 | Limite da alegação |
| 2 | 225–249 | 400 | Invariantes |
| 3 | 250–1836 | 14200 | Contratos estáticos e orçamento de símbolos |
| 4 | 1837–1899 | 400 | Superfície integrada |
| 5 | 1900–2135 | 2000 | Source, nomes e edição |
| 6 | 2136–2574 | 4100 | Módulos, imports e visibilidade |
| 7 | 2575–3195 | 5700 | Bindings, funções e closures |
| 8 | 3196–5534 | 18000 | Tipos e conversões |
| 9 | 5535–6715 | 12700 | Memória, layout e alocação |
| 10 | 6716–6759 | 400 | Property behaviors |
| 11 | 6760–7114 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 7115–9120 | 19500 | Concorrência, paralelismo e execução |
| 13 | 9121–11473 | 22500 | Módulos de execução, services e entries |
| 14 | 11474–12578 | 11000 | Prelude e SDK |
| 15 | 12579–13040 | 4600 | Números, ranges e unidades |
| 16 | 13041–14633 | 13500 | Texto, bytes e collections |
| 17 | 14634–14772 | 1200 | Matrizes, tensors e ML |
| 18 | 14773–15278 | 4800 | Performance e custo |
| 19 | 15279–15539 | 2700 | FFI, unsafe e ilhas de linguagem |
| 20 | 15540–16718 | 12100 | Compilador e bootstrap |
| 21 | 16719–18661 | 18100 | Packages, builds e releases |
| 22 | 18662–18885 | 2100 | Tooling e interface para máquinas |
| 23 | 18886–20292 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 20293–20598 | 7600 | Classificação de viabilidade |
| 25 | 20599–20809 | 1900 | Produto de referência Última Luz |
| 26 | 20810–20911 | 1300 | Protocolo de revisão |
| 27 | 20912–21216 | 3700 | Plano de implementação |
| 28 | 21217–21247 | 500 | Relação com a consolidação histórica |
| 29 | 21248–22021 | 31400 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–5534 | 47300 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 5535–11473 | 58000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 11474–15539 | 37800 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 15540–20292 | 48900 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 20293–22021 | 46400 | viabilidade, Última Luz, gates, roadmap e ledger |

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

