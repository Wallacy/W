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
| linhas de `DESIGN.md` | 23589 |
| tokens aproximados de `DESIGN.md` | 255200 |
| seções numeradas | 30 |
| seções terminais com evidência local | 304/304 |
| decisões | 814 (W-001–W-814) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| casos de ratificação comparativa | 52 |
| casos do corpus Tree-sitter | 87 |
| pares canônicos do formatter F0 | 11 |
| casos do corpus semântico S0 | 22 (11 positivos + 11 negativos) |
| outcomes SemanticResult S0 | 22 |
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
| 3 | 250–3038 | 26600 | Contratos estáticos e orçamento de símbolos |
| 4 | 3039–3101 | 400 | Superfície integrada |
| 5 | 3102–3377 | 2400 | Source, nomes e edição |
| 6 | 3378–3816 | 4100 | Módulos, imports e visibilidade |
| 7 | 3817–4437 | 5700 | Bindings, funções e closures |
| 8 | 4438–6776 | 18000 | Tipos e conversões |
| 9 | 6777–7957 | 12700 | Memória, layout e alocação |
| 10 | 7958–8001 | 400 | Property behaviors |
| 11 | 8002–8356 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 8357–10362 | 19500 | Concorrência, paralelismo e execução |
| 13 | 10363–12715 | 22500 | Módulos de execução, services e entries |
| 14 | 12716–13820 | 11000 | Prelude e SDK |
| 15 | 13821–14282 | 4600 | Números, ranges e unidades |
| 16 | 14283–15875 | 13500 | Texto, bytes e collections |
| 17 | 15876–16014 | 1200 | Matrizes, tensors e ML |
| 18 | 16015–16520 | 4800 | Performance e custo |
| 19 | 16521–16781 | 2700 | FFI, unsafe e ilhas de linguagem |
| 20 | 16782–17961 | 12100 | Compilador e bootstrap |
| 21 | 17962–19904 | 18100 | Packages, builds e releases |
| 22 | 19905–20349 | 4100 | Tooling e interface para máquinas |
| 23 | 20350–21756 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 21757–22062 | 7700 | Classificação de viabilidade |
| 25 | 22063–22273 | 1900 | Produto de referência Última Luz |
| 26 | 22274–22428 | 1600 | Protocolo de revisão |
| 27 | 22429–22733 | 3700 | Plano de implementação |
| 28 | 22734–22764 | 500 | Relação com a consolidação histórica |
| 29 | 22765–23589 | 34200 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–6776 | 60100 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 6777–12715 | 58000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 12716–16781 | 37800 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 16782–21756 | 50900 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 21757–23589 | 49600 | viabilidade, Última Luz, gates, roadmap e ledger |

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

