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
| linhas de `DESIGN.md` | 22952 |
| tokens aproximados de `DESIGN.md` | 246600 |
| seções numeradas | 30 |
| seções terminais com evidência local | 296/296 |
| decisões | 783 (W-001–W-783) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| casos de ratificação comparativa | 52 |
| casos do corpus Tree-sitter | 87 |
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
| 3 | 250–2654 | 21700 | Contratos estáticos e orçamento de símbolos |
| 4 | 2655–2717 | 400 | Superfície integrada |
| 5 | 2718–2993 | 2400 | Source, nomes e edição |
| 6 | 2994–3432 | 4100 | Módulos, imports e visibilidade |
| 7 | 3433–4053 | 5700 | Bindings, funções e closures |
| 8 | 4054–6392 | 18000 | Tipos e conversões |
| 9 | 6393–7573 | 12700 | Memória, layout e alocação |
| 10 | 7574–7617 | 400 | Property behaviors |
| 11 | 7618–7972 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 7973–9978 | 19500 | Concorrência, paralelismo e execução |
| 13 | 9979–12331 | 22500 | Módulos de execução, services e entries |
| 14 | 12332–13436 | 11000 | Prelude e SDK |
| 15 | 13437–13898 | 4600 | Números, ranges e unidades |
| 16 | 13899–15491 | 13500 | Texto, bytes e collections |
| 17 | 15492–15630 | 1200 | Matrizes, tensors e ML |
| 18 | 15631–16136 | 4800 | Performance e custo |
| 19 | 16137–16397 | 2700 | FFI, unsafe e ilhas de linguagem |
| 20 | 16398–17576 | 12100 | Compilador e bootstrap |
| 21 | 17577–19519 | 18100 | Packages, builds e releases |
| 22 | 19520–19743 | 2100 | Tooling e interface para máquinas |
| 23 | 19744–21150 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 21151–21456 | 7700 | Classificação de viabilidade |
| 25 | 21457–21667 | 1900 | Produto de referência Última Luz |
| 26 | 21668–21822 | 1600 | Protocolo de revisão |
| 27 | 21823–22127 | 3700 | Plano de implementação |
| 28 | 22128–22158 | 500 | Relação com a consolidação histórica |
| 29 | 22159–22952 | 32600 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–6392 | 55200 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 6393–12331 | 58000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 12332–16397 | 37800 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 16398–21150 | 48900 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 21151–22952 | 48000 | viabilidade, Última Luz, gates, roadmap e ledger |

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

