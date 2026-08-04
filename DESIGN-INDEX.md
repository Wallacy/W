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
| linhas de `DESIGN.md` | 23543 |
| tokens aproximados de `DESIGN.md` | 254400 |
| seções numeradas | 30 |
| seções terminais com evidência local | 304/304 |
| decisões | 809 (W-001–W-809) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| casos de ratificação comparativa | 52 |
| casos do corpus Tree-sitter | 87 |
| pares canônicos do formatter F0 | 11 |
| casos do corpus semântico S0 | 13 |
| snapshots de diagnostic D0 | 11 |
| snapshots F0 no formato D0 | 11 |
| codes D0 catalogados | 12/73 |
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
| 3 | 250–2997 | 26100 | Contratos estáticos e orçamento de símbolos |
| 4 | 2998–3060 | 400 | Superfície integrada |
| 5 | 3061–3336 | 2400 | Source, nomes e edição |
| 6 | 3337–3775 | 4100 | Módulos, imports e visibilidade |
| 7 | 3776–4396 | 5700 | Bindings, funções e closures |
| 8 | 4397–6735 | 18000 | Tipos e conversões |
| 9 | 6736–7916 | 12700 | Memória, layout e alocação |
| 10 | 7917–7960 | 400 | Property behaviors |
| 11 | 7961–8315 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 8316–10321 | 19500 | Concorrência, paralelismo e execução |
| 13 | 10322–12674 | 22500 | Módulos de execução, services e entries |
| 14 | 12675–13779 | 11000 | Prelude e SDK |
| 15 | 13780–14241 | 4600 | Números, ranges e unidades |
| 16 | 14242–15834 | 13500 | Texto, bytes e collections |
| 17 | 15835–15973 | 1200 | Matrizes, tensors e ML |
| 18 | 15974–16479 | 4800 | Performance e custo |
| 19 | 16480–16740 | 2700 | FFI, unsafe e ilhas de linguagem |
| 20 | 16741–17920 | 12100 | Compilador e bootstrap |
| 21 | 17921–19863 | 18100 | Packages, builds e releases |
| 22 | 19864–20308 | 4100 | Tooling e interface para máquinas |
| 23 | 20309–21715 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 21716–22021 | 7700 | Classificação de viabilidade |
| 25 | 22022–22232 | 1900 | Produto de referência Última Luz |
| 26 | 22233–22387 | 1600 | Protocolo de revisão |
| 27 | 22388–22692 | 3700 | Plano de implementação |
| 28 | 22693–22723 | 500 | Relação com a consolidação histórica |
| 29 | 22724–23543 | 33900 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–6735 | 59600 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 6736–12674 | 58000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 12675–16740 | 37800 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 16741–21715 | 50900 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 21716–23543 | 49300 | viabilidade, Última Luz, gates, roadmap e ledger |

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

