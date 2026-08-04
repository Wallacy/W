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
| linhas de `DESIGN.md` | 23257 |
| tokens aproximados de `DESIGN.md` | 251000 |
| seções numeradas | 30 |
| seções terminais com evidência local | 297/297 |
| decisões | 791 (W-001–W-791) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| casos de ratificação comparativa | 52 |
| casos do corpus Tree-sitter | 87 |
| casos do corpus semântico S0 | 13 |
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
| 3 | 250–2951 | 25700 | Contratos estáticos e orçamento de símbolos |
| 4 | 2952–3014 | 400 | Superfície integrada |
| 5 | 3015–3290 | 2400 | Source, nomes e edição |
| 6 | 3291–3729 | 4100 | Módulos, imports e visibilidade |
| 7 | 3730–4350 | 5700 | Bindings, funções e closures |
| 8 | 4351–6689 | 18000 | Tipos e conversões |
| 9 | 6690–7870 | 12700 | Memória, layout e alocação |
| 10 | 7871–7914 | 400 | Property behaviors |
| 11 | 7915–8269 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 8270–10275 | 19500 | Concorrência, paralelismo e execução |
| 13 | 10276–12628 | 22500 | Módulos de execução, services e entries |
| 14 | 12629–13733 | 11000 | Prelude e SDK |
| 15 | 13734–14195 | 4600 | Números, ranges e unidades |
| 16 | 14196–15788 | 13500 | Texto, bytes e collections |
| 17 | 15789–15927 | 1200 | Matrizes, tensors e ML |
| 18 | 15928–16433 | 4800 | Performance e custo |
| 19 | 16434–16694 | 2700 | FFI, unsafe e ilhas de linguagem |
| 20 | 16695–17873 | 12100 | Compilador e bootstrap |
| 21 | 17874–19816 | 18100 | Packages, builds e releases |
| 22 | 19817–20040 | 2100 | Tooling e interface para máquinas |
| 23 | 20041–21447 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 21448–21753 | 7700 | Classificação de viabilidade |
| 25 | 21754–21964 | 1900 | Produto de referência Última Luz |
| 26 | 21965–22119 | 1600 | Protocolo de revisão |
| 27 | 22120–22424 | 3700 | Plano de implementação |
| 28 | 22425–22455 | 500 | Relação com a consolidação histórica |
| 29 | 22456–23257 | 33000 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–6689 | 59200 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 6690–12628 | 58000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 12629–16694 | 37800 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 16695–21447 | 48900 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 21448–23257 | 48400 | viabilidade, Última Luz, gates, roadmap e ledger |

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

