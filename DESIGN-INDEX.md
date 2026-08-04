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
| linhas de `DESIGN.md` | 21485 |
| tokens aproximados de `DESIGN.md` | 231700 |
| seções numeradas | 30 |
| seções terminais com evidência local | 291/291 |
| decisões | 752 (W-001–W-752) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 1 |
| casos de ratificação comparativa | 37 |
| casos do corpus Tree-sitter | 59 |
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
| 3 | 250–1315 | 9400 | Contratos estáticos e orçamento de símbolos |
| 4 | 1316–1378 | 400 | Superfície integrada |
| 5 | 1379–1614 | 2000 | Source, nomes e edição |
| 6 | 1615–2055 | 4200 | Módulos, imports e visibilidade |
| 7 | 2056–2676 | 5700 | Bindings, funções e closures |
| 8 | 2677–5015 | 18000 | Tipos e conversões |
| 9 | 5016–6196 | 12700 | Memória, layout e alocação |
| 10 | 6197–6240 | 400 | Property behaviors |
| 11 | 6241–6595 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 6596–8605 | 19500 | Concorrência, paralelismo e execução |
| 13 | 8606–10958 | 22500 | Módulos de execução, services e entries |
| 14 | 10959–12063 | 11000 | Prelude e SDK |
| 15 | 12064–12525 | 4600 | Números, ranges e unidades |
| 16 | 12526–14118 | 13500 | Texto, bytes e collections |
| 17 | 14119–14257 | 1200 | Matrizes, tensors e ML |
| 18 | 14258–14763 | 4800 | Performance e custo |
| 19 | 14764–15024 | 2700 | FFI, unsafe e ilhas de linguagem |
| 20 | 15025–16203 | 12100 | Compilador e bootstrap |
| 21 | 16204–18146 | 18100 | Packages, builds e releases |
| 22 | 18147–18370 | 2100 | Tooling e interface para máquinas |
| 23 | 18371–19777 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 19778–20083 | 7600 | Classificação de viabilidade |
| 25 | 20084–20294 | 1900 | Produto de referência Última Luz |
| 26 | 20295–20386 | 1100 | Protocolo de revisão |
| 27 | 20387–20691 | 3700 | Plano de implementação |
| 28 | 20692–20722 | 500 | Relação com a consolidação histórica |
| 29 | 20723–21485 | 30800 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–5015 | 42600 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 5016–10958 | 58000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 10959–15024 | 37800 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 15025–19777 | 48900 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 19778–21485 | 45600 | viabilidade, Última Luz, gates, roadmap e ledger |

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

