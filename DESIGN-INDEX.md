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
| linhas de `DESIGN.md` | 21238 |
| tokens aproximados de `DESIGN.md` | 228700 |
| seções numeradas | 30 |
| seções terminais com evidência local | 289/289 |
| decisões | 745 (W-001–W-745) |
| famílias de viabilidade | 178 |
| casos de ratificação comparativa | 32 |
| casos do corpus Tree-sitter | 54 |
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
| 3 | 250–1170 | 8000 | Contratos estáticos e orçamento de símbolos |
| 4 | 1171–1233 | 400 | Superfície integrada |
| 5 | 1234–1429 | 1600 | Source, nomes e edição |
| 6 | 1430–1870 | 4200 | Módulos, imports e visibilidade |
| 7 | 1871–2491 | 5700 | Bindings, funções e closures |
| 8 | 2492–4830 | 18000 | Tipos e conversões |
| 9 | 4831–6011 | 12700 | Memória, layout e alocação |
| 10 | 6012–6055 | 400 | Property behaviors |
| 11 | 6056–6410 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 6411–8420 | 19500 | Concorrência, paralelismo e execução |
| 13 | 8421–10773 | 22500 | Módulos de execução, services e entries |
| 14 | 10774–11878 | 11000 | Prelude e SDK |
| 15 | 11879–12340 | 4600 | Números, ranges e unidades |
| 16 | 12341–13933 | 13500 | Texto, bytes e collections |
| 17 | 13934–14072 | 1200 | Matrizes, tensors e ML |
| 18 | 14073–14578 | 4800 | Performance e custo |
| 19 | 14579–14839 | 2700 | FFI, unsafe e ilhas de linguagem |
| 20 | 14840–16018 | 12100 | Compilador e bootstrap |
| 21 | 16019–17961 | 18100 | Packages, builds e releases |
| 22 | 17962–18185 | 2100 | Tooling e interface para máquinas |
| 23 | 18186–19592 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 19593–19873 | 7200 | Classificação de viabilidade |
| 25 | 19874–20084 | 1900 | Produto de referência Última Luz |
| 26 | 20085–20148 | 800 | Protocolo de revisão |
| 27 | 20149–20451 | 3700 | Plano de implementação |
| 28 | 20452–20482 | 500 | Relação com a consolidação histórica |
| 29 | 20483–21238 | 30300 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–4830 | 40800 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 4831–10773 | 58000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 10774–14839 | 37800 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 14840–19592 | 48900 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 19593–21238 | 44400 | viabilidade, Última Luz, gates, roadmap e ledger |

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

