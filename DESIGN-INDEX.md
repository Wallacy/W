# Índice gerado do design W

> Gerado por `tooling/design-index.mjs`. Não edite este arquivo.
> `DESIGN.md` continua sendo a única fonte normativa. `RATIONALE.md` é complementar e não normativo.

## Contexto mínimo

1. Leia este índice para localizar a seção necessária.
2. Leia somente o intervalo correspondente em `DESIGN.md`.
3. Use `RATIONALE.md` somente para IDs, evidência, alternativas e proveniência.
4. Busque o ID W quando a tarefa alterar uma decisão.
5. Abra o produto Última Luz somente para o exemplo afetado.
6. Não leia `tooling/tree-sitter-w/src/` como source. Essa pasta é gerada.

## Snapshot calculado

| Métrica | Valor |
|---|---:|
| linhas de `DESIGN.md` | 29394 |
| tokens aproximados de `DESIGN.md` | 314900 |
| linhas de `RATIONALE.md` | 2375 |
| tokens aproximados de `RATIONALE.md` | 82600 |
| seções numeradas | 27 |
| seções terminais com evidência local | 335/335 |
| decisões | 1180 (W-001–W-1180) |
| famílias de viabilidade | 178 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 69 |
| casos de substituição estruturados | 69/69 |
| decisões referenciadas por casos R0 | 129/1180 |
| decisões classificadas para design freeze | 368/1180 (129 source + 278 oracle + 8 explícitas; 47 overlaps) |
| decisões ainda sem classe de freeze | 812 |
| decisões com múltiplos eixos obrigatórios | 2 |
| formas R0 com baseline estática | 169 |
| surface lexemes das formas vigentes R0 | 1404 total; mediana 16; máximo 52 |
| bundles executáveis R1 | 21 |
| variantes/tarefas R1 | 51/84 |
| casos R0 promovidos a R1 | 32/69 |
| casos do corpus Tree-sitter | 101 |
| pares canônicos do formatter F0 | 21 |
| casos/operações do kernel de memória M1 | 165/580 (70 aceitos + 95 rejeitados) |
| casos/operações do kernel de allocation físico A0 | 48/123 (15 aceitos + 33 rejeitados) |
| casos/operações do kernel de layout e ABI L0 | 78/96 (27 aceitos + 51 rejeitados) |
| casos/operações do kernel de execução E0 | 57/527 (31 aceitos + 26 rejeitados; 10/10 origens happens-before) |
| casos/operações do kernel de runtime closure E1 | 41/473 (19 aceitos + 22 rejeitados; sete testes host) |
| casos/operações do carrier de snapshot SP0 | 27/82 (14 aceitos + 12 rejeitados + 1 fault; sete testes host) |
| casos/operações do kernel de boundary effects B0 | 39/320 (25 aceitos + 14 rejeitados) |
| casos/operações do kernel de packages e releases P0 | 44/379 (22 aceitos + 22 rejeitados) |
| casos/operações do workflow single-file PYN1 | 95/546 (23 aceitos + 72 rejeitados) |
| casos/operações da sessão transacional PYN2 | 67/287 (53 aceitos + 14 rejeitados) |
| casos/operações de apresentação PYN3 | 24/69 (8 aceitos + 16 rejeitados; host oracle não executa W) |
| casos/operações do adapter Jupyter PYN3 | 30/98 (16 aceitos + 14 rejeitados; host oracle não executa W) |
| casos/operações do export notebook PYN3 | 18/49 (5 aceitos + 13 rejeitados; host oracle não executa W) |
| casos do container WMeta1 W0 | 42 (5 aceitos + 37 rejeitados; 2 readers independentes) |
| casos/operações do carrier tabular TAB0 | 64/155 (22 aceitos + 42 rejeitados; host oracle não executa W) |
| casos/operações dos adapters tabulares TAB1 | 84/184 (35 aceitos + 49 rejeitados; host oracle não executa W) |
| casos/operações do carrier DLPack PYN4 | 74/325 (25 aceitos + 49 rejeitados; host oracle não executa W) |
| casos do corpus semântico S0 | 104 (52 positivos + 52 negativos) |
| outcomes SemanticResult S0 | 104 |
| snapshots de diagnostic D0 | 52 |
| snapshots F0 no formato D0 | 21 |
| codes D0 catalogados | 217/175 |
| sources W no root do Última Luz | 86 |
| sources W em todo o Última Luz | 94 |
| sources W no rascunho da std | 22 |
| módulos/APIs catalogados da std SDK0 | 22/316 |
| superfícies qualificadas da std usadas pelo Última Luz | 78 |
| requisitos do Última Luz com contrato std SDK0 | 21/21 |
| requisitos do Última Luz ausentes na std SDK0 | 0/21 |

A estimativa de tokens usa bytes divididos por quatro. Use o valor somente para planejar leitura.

## Cobertura do catálogo D0

| Família | Catalogados | Referenciados |
|---|---:|---:|
| BORROW | 10 | 10 |
| CAPABILITY | 1 | 1 |
| CONST | 7 | 7 |
| CONTRACT | 5 | 5 |
| DIAGNOSTIC | 1 | 1 |
| DOC | 2 | 2 |
| EFFECT | 2 | 2 |
| EXPORT | 7 | 7 |
| EXPR | 4 | 4 |
| FLOW | 2 | 2 |
| FMT | 2 | 2 |
| GENERIC | 5 | 5 |
| INIT | 1 | 1 |
| JUPYTER | 8 | 8 |
| LABEL | 3 | 3 |
| LEX | 1 | 1 |
| MATCH | 3 | 3 |
| MOVE | 1 | 1 |
| OWNERSHIP | 4 | 4 |
| PARSE | 29 | 29 |
| PATTERN | 6 | 6 |
| PLACEMENT | 3 | 3 |
| PRESENTATION | 10 | 10 |
| PROCESS | 3 | 3 |
| SCRIPT | 16 | 16 |
| SEM | 1 | 1 |
| SESSION | 28 | 28 |
| STD | 1 | 1 |
| SUSPEND | 4 | 4 |
| TYPE | 3 | 3 |
| USE | 1 | 1 |
| WIRE | 1 | 1 |

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 15–245 | 2800 | Como ler este documento |
| 1 | 246–269 | 300 | Limite da alegação |
| 2 | 270–294 | 400 | Invariantes |
| 3 | 295–3436 | 32100 | Contratos estáticos e orçamento de símbolos |
| 4 | 3437–3499 | 400 | Superfície integrada |
| 5 | 3500–3807 | 2700 | Source, nomes e edição |
| 6 | 3808–4247 | 4100 | Módulos, imports e visibilidade |
| 7 | 4248–4935 | 6500 | Bindings, funções e closures |
| 8 | 4936–7487 | 20500 | Tipos e conversões |
| 9 | 7488–9140 | 19000 | Memória, layout e alocação |
| 10 | 9141–9184 | 400 | Property behaviors |
| 11 | 9185–9529 | 2700 | Erros, panic, OOM e cleanup |
| 12 | 9530–12116 | 27500 | Concorrência, paralelismo e execução |
| 13 | 12117–14341 | 22500 | Módulos de execução, services e entries |
| 14 | 14342–18167 | 48700 | Prelude e SDK |
| 15 | 18168–18881 | 7300 | Números, ranges e unidades |
| 16 | 18882–20485 | 13700 | Texto, bytes e collections |
| 17 | 20486–20850 | 4400 | Matrizes, tensors e ML |
| 18 | 20851–21365 | 5000 | Performance e custo |
| 19 | 21366–21750 | 4300 | FFI, unsafe e ilhas de linguagem |
| 20 | 21751–23383 | 18000 | Compilador e bootstrap |
| 21 | 23384–25492 | 20200 | Packages, builds e releases |
| 22 | 25493–26002 | 4900 | Tooling e interface para máquinas |
| 23 | 26003–27431 | 16900 | Protocolos e pesquisas de ecossistema |
| 24 | 27432–28861 | 24900 | Classificação de viabilidade |
| 25 | 28862–29072 | 1900 | Produto de referência Última Luz |
| 26 | 29073–29394 | 4000 | Plano de implementação |

## Navegação compacta de RATIONALE

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 1 | 14–1147 | 15500 | Evidência comparativa |
| 2 | 1148–1178 | 500 | Proveniência |
| 3 | 1179–2375 | 66500 | Ledger |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 15–7487 | 69800 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 7488–14341 | 72100 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 14342–21750 | 83400 | módulos, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 21751–27431 | 60000 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26 | 27432–29394 | 30800 | viabilidade, Última Luz, gates e roadmap |

O bundle agrupa seções para planejamento; os intervalos não são uma nova autoridade.

## Classificação de viabilidade

| Classe | Famílias |
|---|---:|
| Possível agora | 82 |
| Possível por transport profile | 1 |
| Provável após C | 1 |
| Provável | 71 |
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
bun tooling/design-slice.mjs --rationale-heading 1.3
rg -n -C 4 'transaction' DESIGN.md
bun tooling/design-index.mjs --check
```

