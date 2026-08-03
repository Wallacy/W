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
| linhas de `DESIGN.md` | 20432 |
| tokens aproximados de `DESIGN.md` | 216000 |
| seções numeradas | 30 |
| seções terminais com evidência local | 281/281 |
| decisões | 707 (W-001–W-707) |
| famílias de viabilidade | 177 |
| comparações de revisão ainda previstas | 31 |
| casos do corpus Tree-sitter | 53 |
| sources W no root do Última Luz | 61 |
| sources W em todo o Última Luz | 69 |
| sources W no rascunho da std | 9 |

A estimativa de tokens usa bytes divididos por quatro. Use o valor somente para planejar leitura.

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 14–177 | 2000 | Como ler este documento |
| 1 | 178–201 | 300 | Limite da alegação |
| 2 | 202–226 | 400 | Invariantes |
| 3 | 227–1094 | 7400 | Contratos estáticos e orçamento de símbolos |
| 4 | 1095–1157 | 400 | Superfície integrada |
| 5 | 1158–1287 | 1000 | Source, nomes e edição |
| 6 | 1288–1728 | 4200 | Módulos, imports e visibilidade |
| 7 | 1729–2349 | 5700 | Bindings, funções e closures |
| 8 | 2350–4687 | 17900 | Tipos e conversões |
| 9 | 4688–5798 | 11900 | Memória, layout e alocação |
| 10 | 5799–5842 | 400 | Property behaviors |
| 11 | 5843–6197 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 6198–8200 | 19400 | Concorrência, paralelismo e execução |
| 13 | 8201–10494 | 21700 | Módulos de execução, services e entries |
| 14 | 10495–11535 | 10100 | Prelude e SDK |
| 15 | 11536–11996 | 4600 | Números, ranges e unidades |
| 16 | 11997–13589 | 13500 | Texto, bytes e collections |
| 17 | 13590–13728 | 1200 | Matrizes, tensors e ML |
| 18 | 13729–14232 | 4800 | Performance e custo |
| 19 | 14233–14405 | 1800 | FFI, unsafe e ilhas de linguagem |
| 20 | 14406–15511 | 11100 | Compilador e bootstrap |
| 21 | 15512–17412 | 17600 | Packages, builds e releases |
| 22 | 17413–17616 | 1800 | Tooling e interface para máquinas |
| 23 | 17617–18935 | 15500 | Protocolos e pesquisas de ecossistema |
| 24 | 18936–19117 | 5500 | Classificação de viabilidade |
| 25 | 19118–19328 | 1900 | Produto de referência Última Luz |
| 26 | 19329–19391 | 800 | Protocolo de revisão |
| 27 | 19392–19683 | 3500 | Plano de implementação |
| 28 | 19684–19714 | 500 | Relação com a consolidação histórica |
| 29 | 19715–20432 | 27600 | Registro de decisões e alternativas |

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
rg -n '^## 12\.|^### 12\.' W/DESIGN.md
rg -n 'W-688' W/DESIGN.md
rg -n -C 4 'transaction' W/DESIGN.md
node W/tooling/design-index.mjs --check
```

