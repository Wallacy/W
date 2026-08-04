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
| linhas de `DESIGN.md` | 22737 |
| tokens aproximados de `DESIGN.md` | 244400 |
| seções numeradas | 30 |
| seções terminais com evidência local | 295/295 |
| decisões | 775 (W-001–W-775) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 5 |
| casos de ratificação comparativa | 52 |
| casos do corpus Tree-sitter | 80 |
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
| 3 | 250–2447 | 19900 | Contratos estáticos e orçamento de símbolos |
| 4 | 2448–2510 | 400 | Superfície integrada |
| 5 | 2511–2786 | 2400 | Source, nomes e edição |
| 6 | 2787–3225 | 4100 | Módulos, imports e visibilidade |
| 7 | 3226–3846 | 5700 | Bindings, funções e closures |
| 8 | 3847–6185 | 18000 | Tipos e conversões |
| 9 | 6186–7366 | 12700 | Memória, layout e alocação |
| 10 | 7367–7410 | 400 | Property behaviors |
| 11 | 7411–7765 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 7766–9771 | 19500 | Concorrência, paralelismo e execução |
| 13 | 9772–12124 | 22500 | Módulos de execução, services e entries |
| 14 | 12125–13229 | 11000 | Prelude e SDK |
| 15 | 13230–13691 | 4600 | Números, ranges e unidades |
| 16 | 13692–15284 | 13500 | Texto, bytes e collections |
| 17 | 15285–15423 | 1200 | Matrizes, tensors e ML |
| 18 | 15424–15929 | 4800 | Performance e custo |
| 19 | 15930–16190 | 2700 | FFI, unsafe e ilhas de linguagem |
| 20 | 16191–17369 | 12100 | Compilador e bootstrap |
| 21 | 17370–19312 | 18100 | Packages, builds e releases |
| 22 | 19313–19536 | 2100 | Tooling e interface para máquinas |
| 23 | 19537–20943 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 20944–21249 | 7700 | Classificação de viabilidade |
| 25 | 21250–21460 | 1900 | Produto de referência Última Luz |
| 26 | 21461–21615 | 1600 | Protocolo de revisão |
| 27 | 21616–21920 | 3700 | Plano de implementação |
| 28 | 21921–21951 | 500 | Relação com a consolidação histórica |
| 29 | 21952–22737 | 32100 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–6185 | 53400 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 6186–12124 | 58000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 12125–16190 | 37800 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 16191–20943 | 48900 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 20944–22737 | 47500 | viabilidade, Última Luz, gates, roadmap e ledger |

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

