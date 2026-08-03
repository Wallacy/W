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
| linhas de `DESIGN.md` | 20520 |
| tokens aproximados de `DESIGN.md` | 217400 |
| seções numeradas | 30 |
| seções terminais com evidência local | 281/281 |
| decisões | 711 (W-001–W-711) |
| famílias de viabilidade | 177 |
| comparações de revisão ainda previstas | 32 |
| casos do corpus Tree-sitter | 53 |
| sources W no root do Última Luz | 63 |
| sources W em todo o Última Luz | 71 |
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
| 13 | 8273–10566 | 21700 | Módulos de execução, services e entries |
| 14 | 10567–11607 | 10100 | Prelude e SDK |
| 15 | 11608–12068 | 4600 | Números, ranges e unidades |
| 16 | 12069–13661 | 13500 | Texto, bytes e collections |
| 17 | 13662–13800 | 1200 | Matrizes, tensors e ML |
| 18 | 13801–14304 | 4800 | Performance e custo |
| 19 | 14305–14477 | 1800 | FFI, unsafe e ilhas de linguagem |
| 20 | 14478–15590 | 11200 | Compilador e bootstrap |
| 21 | 15591–17491 | 17600 | Packages, builds e releases |
| 22 | 17492–17699 | 1900 | Tooling e interface para máquinas |
| 23 | 17700–19018 | 15500 | Protocolos e pesquisas de ecossistema |
| 24 | 19019–19200 | 5500 | Classificação de viabilidade |
| 25 | 19201–19411 | 1900 | Produto de referência Última Luz |
| 26 | 19412–19475 | 800 | Protocolo de revisão |
| 27 | 19476–19767 | 3500 | Plano de implementação |
| 28 | 19768–19798 | 500 | Relação com a consolidação histórica |
| 29 | 19799–20520 | 27900 | Registro de decisões e alternativas |

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

