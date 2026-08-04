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
| linhas de `DESIGN.md` | 21334 |
| tokens aproximados de `DESIGN.md` | 230000 |
| seções numeradas | 30 |
| seções terminais com evidência local | 290/290 |
| decisões | 749 (W-001–W-749) |
| famílias de viabilidade | 178 |
| casos de ratificação comparativa | 35 |
| casos do corpus Tree-sitter | 55 |
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
| 5 | 1234–1468 | 2000 | Source, nomes e edição |
| 6 | 1469–1909 | 4200 | Módulos, imports e visibilidade |
| 7 | 1910–2530 | 5700 | Bindings, funções e closures |
| 8 | 2531–4869 | 18000 | Tipos e conversões |
| 9 | 4870–6050 | 12700 | Memória, layout e alocação |
| 10 | 6051–6094 | 400 | Property behaviors |
| 11 | 6095–6449 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 6450–8459 | 19500 | Concorrência, paralelismo e execução |
| 13 | 8460–10812 | 22500 | Módulos de execução, services e entries |
| 14 | 10813–11917 | 11000 | Prelude e SDK |
| 15 | 11918–12379 | 4600 | Números, ranges e unidades |
| 16 | 12380–13972 | 13500 | Texto, bytes e collections |
| 17 | 13973–14111 | 1200 | Matrizes, tensors e ML |
| 18 | 14112–14617 | 4800 | Performance e custo |
| 19 | 14618–14878 | 2700 | FFI, unsafe e ilhas de linguagem |
| 20 | 14879–16057 | 12100 | Compilador e bootstrap |
| 21 | 16058–18000 | 18100 | Packages, builds e releases |
| 22 | 18001–18224 | 2100 | Tooling e interface para máquinas |
| 23 | 18225–19631 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 19632–19937 | 7600 | Classificação de viabilidade |
| 25 | 19938–20148 | 1900 | Produto de referência Última Luz |
| 26 | 20149–20238 | 1100 | Protocolo de revisão |
| 27 | 20239–20543 | 3700 | Plano de implementação |
| 28 | 20544–20574 | 500 | Relação com a consolidação histórica |
| 29 | 20575–21334 | 30600 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–4869 | 41200 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 4870–10812 | 58000 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 10813–14878 | 37800 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 14879–19631 | 48900 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 19632–21334 | 45400 | viabilidade, Última Luz, gates, roadmap e ledger |

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

