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
| linhas de `DESIGN.md` | 20724 |
| tokens aproximados de `DESIGN.md` | 220800 |
| seções numeradas | 30 |
| seções terminais com evidência local | 282/282 |
| decisões | 724 (W-001–W-724) |
| famílias de viabilidade | 177 |
| comparações de revisão ainda previstas | 32 |
| casos do corpus Tree-sitter | 53 |
| sources W no root do Última Luz | 67 |
| sources W em todo o Última Luz | 75 |
| sources W no rascunho da std | 9 |

A estimativa de tokens usa bytes divididos por quatro. Use o valor somente para planejar leitura.

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 14–177 | 2000 | Como ler este documento |
| 1 | 178–201 | 300 | Limite da alegação |
| 2 | 202–226 | 400 | Invariantes |
| 3 | 227–1148 | 8000 | Contratos estáticos e orçamento de símbolos |
| 4 | 1149–1211 | 400 | Superfície integrada |
| 5 | 1212–1341 | 1000 | Source, nomes e edição |
| 6 | 1342–1782 | 4200 | Módulos, imports e visibilidade |
| 7 | 1783–2403 | 5700 | Bindings, funções e closures |
| 8 | 2404–4741 | 17900 | Tipos e conversões |
| 9 | 4742–5870 | 12200 | Memória, layout e alocação |
| 10 | 5871–5914 | 400 | Property behaviors |
| 11 | 5915–6269 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 6270–8272 | 19400 | Concorrência, paralelismo e execução |
| 13 | 8273–10615 | 22300 | Módulos de execução, services e entries |
| 14 | 10616–11656 | 10100 | Prelude e SDK |
| 15 | 11657–12117 | 4600 | Números, ranges e unidades |
| 16 | 12118–13710 | 13500 | Texto, bytes e collections |
| 17 | 13711–13849 | 1200 | Matrizes, tensors e ML |
| 18 | 13850–14353 | 4800 | Performance e custo |
| 19 | 14354–14526 | 1800 | FFI, unsafe e ilhas de linguagem |
| 20 | 14527–15670 | 11600 | Compilador e bootstrap |
| 21 | 15671–17596 | 17900 | Packages, builds e releases |
| 22 | 17597–17804 | 1900 | Tooling e interface para máquinas |
| 23 | 17805–19209 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 19210–19391 | 5500 | Classificação de viabilidade |
| 25 | 19392–19602 | 1900 | Produto de referência Última Luz |
| 26 | 19603–19666 | 800 | Protocolo de revisão |
| 27 | 19667–19958 | 3500 | Plano de implementação |
| 28 | 19959–19989 | 500 | Relação com a consolidação histórica |
| 29 | 19990–20724 | 28900 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–4741 | 39900 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 4742–10615 | 57200 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 10616–14526 | 36000 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 14527–19209 | 48000 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 19210–20724 | 41100 | viabilidade, Última Luz, gates, roadmap e ledger |

O bundle agrupa seções para planejamento; os intervalos não são uma nova autoridade.

## Classificação de viabilidade

| Classe | Famílias |
|---|---:|
| Pesquisa por transport profile | 1 |
| Pesquisa | 18 |
| Possível agora | 80 |
| Provável T2 | 7 |
| Provável | 58 |
| Rejeitado na baseline | 1 |
| Rejeitado por enquanto | 2 |
| Rejeitado | 10 |

## Pesquisas explícitas

- `InlineString` com layout público — Pesquisa
- Posit, Unum e decimal float — Pesquisa
- packs heterogêneos e GAT — Pesquisa
- wWire `exact` e `compatible` — Pesquisa
- introdução direta entre três services — Pesquisa
- theorem prover ou SMT geral no build — Pesquisa
- obrigação linear de async close — Pesquisa
- encoding publicável de metadata — Pesquisa
- ABI W resiliente entre releases — Pesquisa
- ASIC/FPGA como target geral — Pesquisa
- high-bit addresses e NaN boxing — Pesquisa
- deadline remoto strict — Pesquisa por transport profile
- MPMC, broadcast, watch e weighted channel — Pesquisa
- scatter read — Pesquisa
- file/device zero-copy — Pesquisa
- RCU e snapshot cell — Pesquisa
- facts trusted para FFI e synchronization customizada — Pesquisa
- QoS na syntax de `spawn` — Pesquisa
- `fn<Rust>`/`fn<Swift>` — Pesquisa

## Comandos de leitura

```powershell
node W/tooling/design-slice.mjs --section 12
node W/tooling/design-slice.mjs --heading 12.13
node W/tooling/design-slice.mjs --id W-711 --context 2
rg -n -C 4 'transaction' W/DESIGN.md
node W/tooling/design-index.mjs --check
```

