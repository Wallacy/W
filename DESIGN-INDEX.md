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
| linhas de `DESIGN.md` | 20493 |
| tokens aproximados de `DESIGN.md` | 216900 |
| seções numeradas | 30 |
| seções terminais com evidência local | 281/281 |
| decisões | 709 (W-001–W-709) |
| famílias de viabilidade | 177 |
| comparações de revisão ainda previstas | 32 |
| casos do corpus Tree-sitter | 53 |
| sources W no root do Última Luz | 62 |
| sources W em todo o Última Luz | 70 |
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
| 9 | 4742–5852 | 11900 | Memória, layout e alocação |
| 10 | 5853–5896 | 400 | Property behaviors |
| 11 | 5897–6251 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 6252–8254 | 19400 | Concorrência, paralelismo e execução |
| 13 | 8255–10548 | 21700 | Módulos de execução, services e entries |
| 14 | 10549–11589 | 10100 | Prelude e SDK |
| 15 | 11590–12050 | 4600 | Números, ranges e unidades |
| 16 | 12051–13643 | 13500 | Texto, bytes e collections |
| 17 | 13644–13782 | 1200 | Matrizes, tensors e ML |
| 18 | 13783–14286 | 4800 | Performance e custo |
| 19 | 14287–14459 | 1800 | FFI, unsafe e ilhas de linguagem |
| 20 | 14460–15565 | 11100 | Compilador e bootstrap |
| 21 | 15566–17466 | 17600 | Packages, builds e releases |
| 22 | 17467–17674 | 1900 | Tooling e interface para máquinas |
| 23 | 17675–18993 | 15500 | Protocolos e pesquisas de ecossistema |
| 24 | 18994–19175 | 5500 | Classificação de viabilidade |
| 25 | 19176–19386 | 1900 | Produto de referência Última Luz |
| 26 | 19387–19450 | 800 | Protocolo de revisão |
| 27 | 19451–19742 | 3500 | Plano de implementação |
| 28 | 19743–19773 | 500 | Relação com a consolidação histórica |
| 29 | 19774–20493 | 27700 | Registro de decisões e alternativas |

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

