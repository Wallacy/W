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
| linhas de `DESIGN.md` | 20889 |
| tokens aproximados de `DESIGN.md` | 223300 |
| seções numeradas | 30 |
| seções terminais com evidência local | 283/283 |
| decisões | 730 (W-001–W-730) |
| famílias de viabilidade | 177 |
| comparações de revisão ainda previstas | 32 |
| casos do corpus Tree-sitter | 53 |
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
| 3 | 250–1171 | 8000 | Contratos estáticos e orçamento de símbolos |
| 4 | 1172–1234 | 400 | Superfície integrada |
| 5 | 1235–1364 | 1000 | Source, nomes e edição |
| 6 | 1365–1805 | 4200 | Módulos, imports e visibilidade |
| 7 | 1806–2426 | 5700 | Bindings, funções e closures |
| 8 | 2427–4764 | 17900 | Tipos e conversões |
| 9 | 4765–5945 | 12700 | Memória, layout e alocação |
| 10 | 5946–5989 | 400 | Property behaviors |
| 11 | 5990–6344 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 6345–8352 | 19500 | Concorrência, paralelismo e execução |
| 13 | 8353–10700 | 22400 | Módulos de execução, services e entries |
| 14 | 10701–11741 | 10100 | Prelude e SDK |
| 15 | 11742–12202 | 4600 | Números, ranges e unidades |
| 16 | 12203–13795 | 13500 | Texto, bytes e collections |
| 17 | 13796–13934 | 1200 | Matrizes, tensors e ML |
| 18 | 13935–14438 | 4800 | Performance e custo |
| 19 | 14439–14611 | 1800 | FFI, unsafe e ilhas de linguagem |
| 20 | 14612–15792 | 12100 | Compilador e bootstrap |
| 21 | 15793–17735 | 18100 | Packages, builds e releases |
| 22 | 17736–17959 | 2100 | Tooling e interface para máquinas |
| 23 | 17960–19364 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 19365–19546 | 5500 | Classificação de viabilidade |
| 25 | 19547–19757 | 1900 | Produto de referência Última Luz |
| 26 | 19758–19821 | 800 | Protocolo de revisão |
| 27 | 19822–20117 | 3500 | Plano de implementação |
| 28 | 20118–20148 | 500 | Relação com a consolidação histórica |
| 29 | 20149–20889 | 29400 | Registro de decisões e alternativas |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 14–4764 | 40100 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 4765–10700 | 57900 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 10701–14611 | 36000 | tiers, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 14612–19364 | 48900 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26, 27, 28, 29 | 19365–20889 | 41600 | viabilidade, Última Luz, gates, roadmap e ledger |

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
node tooling/design-slice.mjs --section 12
node tooling/design-slice.mjs --heading 12.13
node tooling/design-slice.mjs --id W-711 --context 2
rg -n -C 4 'transaction' DESIGN.md
node tooling/design-index.mjs --check
```

