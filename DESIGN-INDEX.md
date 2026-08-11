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
| linhas de `DESIGN.md` | 28426 |
| tokens aproximados de `DESIGN.md` | 296200 |
| linhas de `RATIONALE.md` | 4330 |
| tokens aproximados de `RATIONALE.md` | 117800 |
| seções numeradas | 27 |
| seções terminais com evidência local | 335/335 |
| decisões | 1260 (W-001–W-1260) |
| famílias de viabilidade | 182 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 69 |
| casos de substituição estruturados | 69/69 |
| decisões referenciadas por casos R0 | 130/1260 |
| decisões classificadas para design freeze | 433/1260 (130 source + 343 oracle + 8 explícitas; 48 overlaps) |
| decisões ainda sem classe de freeze | 827 |
| decisões com múltiplos eixos obrigatórios | 2 |
| formas R0 com baseline estática | 170 |
| surface lexemes das formas vigentes R0 | 1404 total; mediana 16; máximo 52 |
| bundles executáveis R1 | 21 |
| variantes/tarefas R1 | 51/84 |
| casos R0 promovidos a R1 | 32/69 |
| casos do corpus Tree-sitter | 102 |
| pares canônicos do formatter F0 | 22 |
| casos/operações do kernel de memória M1 | 184/603 (82 aceitos + 102 rejeitados) |
| casos/operações do kernel de allocation físico A0 | 48/123 (15 aceitos + 33 rejeitados) |
| casos/operações do kernel de layout e ABI L0 | 78/96 (27 aceitos + 51 rejeitados) |
| casos/operações do kernel de execução E0 | 73/677 (38 aceitos + 35 rejeitados; 10/10 origens happens-before) |
| casos/operações do kernel de runtime closure E1 | 41/473 (19 aceitos + 22 rejeitados; sete testes host) |
| casos/operações do behavior Lazy LZ0 | 40/118 (16 aceitos + 20 rejeitados + 4 fault; 12 testes host) |
| casos/operações da composição de ownership e execução MX0 | 46/274 (23 aceitos + 23 rejeitados; 14 testes host) |
| casos/operações de channel bounded CH0 | 47/333 (28 aceitos + 19 rejeitados; 12 testes host) |
| casos/operações de lock da linguagem LM1 | 39/86 (21 aceitos + 17 rejeitados + 1 fault; 11 testes host) |
| casos/operações do carrier de snapshot SP0 | 27/82 (14 aceitos + 12 rejeitados + 1 fault; sete testes host) |
| casos/operações do kernel de boundary effects B0 | 39/320 (25 aceitos + 14 rejeitados) |
| casos/operações de service recovery SR0 | 48/392 (18 aceitos + 30 rejeitados; 17 testes host) |
| casos/operações do kernel de packages e releases P0 | 44/379 (22 aceitos + 22 rejeitados) |
| casos/operações do workflow single-file PYN1 | 95/546 (23 aceitos + 72 rejeitados) |
| casos/operações da sessão transacional PYN2 | 70/298 (56 aceitos + 14 rejeitados) |
| casos/operações de apresentação PYN3 | 26/75 (9 aceitos + 17 rejeitados; host oracle não executa W) |
| casos/operações do adapter Jupyter PYN3 | 32/104 (16 aceitos + 16 rejeitados; host oracle não executa W) |
| casos/operações do export notebook PYN3 | 18/49 (5 aceitos + 13 rejeitados; host oracle não executa W) |
| casos do container WMeta1 W0 | 42 (5 aceitos + 37 rejeitados; 2 readers independentes) |
| casos/operações do carrier tabular TAB0 | 64/155 (22 aceitos + 42 rejeitados; host oracle não executa W) |
| casos/operações dos adapters tabulares TAB1 | 86/193 (36 aceitos + 50 rejeitados; host oracle não executa W) |
| casos/operações do carrier DLPack PYN4 | 75/326 (26 aceitos + 49 rejeitados; host oracle não executa W) |
| casos/operações de device execution DEV0 | 39/181 (17 aceitos + 22 rejeitados; host oracle não executa W) |
| casos do corpus semântico S0 | 108 (54 positivos + 54 negativos) |
| outcomes SemanticResult S0 | 108 |
| snapshots de diagnostic D0 | 54 |
| snapshots F0 no formato D0 | 22 |
| codes D0 catalogados | 240/154 |
| sources W no root do Última Luz | 91 |
| sources W em todo o Última Luz | 99 |
| sources W no rascunho da std | 23 |
| módulos/APIs catalogados da std SDK0 | 23/324 |
| superfícies qualificadas da std usadas pelo Última Luz | 84 |
| requisitos do Última Luz com contrato std SDK0 | 23/23 |
| requisitos do Última Luz ausentes na std SDK0 | 1/23 |

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
| EXPR | 4 | 4 |
| FLOW | 2 | 2 |
| FMT | 2 | 2 |
| GENERIC | 5 | 5 |
| INIT | 1 | 1 |
| JUPYTER | 1 | 1 |
| LABEL | 3 | 3 |
| LEX | 1 | 1 |
| MATCH | 3 | 3 |
| MEMORY | 1 | 1 |
| MOVE | 1 | 1 |
| OWNERSHIP | 6 | 6 |
| PARSE | 29 | 29 |
| PATTERN | 6 | 6 |
| PLACEMENT | 3 | 3 |
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
| 0 | 15–228 | 2600 | Como ler este documento |
| 1 | 229–252 | 300 | Limite da alegação |
| 2 | 253–278 | 400 | Invariantes |
| 3 | 279–3277 | 30400 | Contratos estáticos e orçamento de símbolos |
| 4 | 3278–3340 | 400 | Superfície integrada |
| 5 | 3341–3648 | 2700 | Source, nomes e edição |
| 6 | 3649–4054 | 3700 | Módulos, imports e visibilidade |
| 7 | 4055–4712 | 6000 | Bindings, funções e closures |
| 8 | 4713–7149 | 19000 | Tipos e conversões |
| 9 | 7150–8655 | 17100 | Memória, layout e alocação |
| 10 | 8656–8778 | 1400 | Property behaviors |
| 11 | 8779–9124 | 2700 | Erros, panic, OOM e cleanup |
| 12 | 9125–12007 | 31400 | Concorrência, paralelismo e execução |
| 13 | 12008–14280 | 22900 | Módulos de execução, services e entries |
| 14 | 14281–17952 | 46600 | Prelude e SDK |
| 15 | 17953–18663 | 7200 | Números, ranges e unidades |
| 16 | 18664–20236 | 13200 | Texto, bytes e collections |
| 17 | 20237–20586 | 4100 | Matrizes, tensors e ML |
| 18 | 20587–21040 | 4400 | Performance e custo |
| 19 | 21041–21575 | 6100 | FFI, unsafe e ilhas de linguagem |
| 20 | 21576–23110 | 16600 | Compilador e bootstrap |
| 21 | 23111–25137 | 19200 | Packages, builds e releases |
| 22 | 25138–25644 | 4900 | Tooling e interface para máquinas |
| 23 | 25645–27048 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 27049–27889 | 11500 | Design freeze e pendências |
| 25 | 27890–28100 | 1900 | Produto de referência Última Luz |
| 26 | 28101–28426 | 4000 | Plano de implementação |

## Navegação compacta de RATIONALE

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 1 | 14–3022 | 46000 | Evidência comparativa |
| 2 | 3023–3053 | 500 | Proveniência |
| 3 | 3054–4330 | 71300 | Ledger |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 15–7149 | 65500 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 7150–14280 | 75500 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 14281–21575 | 81600 | módulos, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 21576–27048 | 57300 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26 | 27049–28426 | 17400 | freeze, Última Luz, gates e roadmap |

O bundle agrupa seções para planejamento; os intervalos não são uma nova autoridade.

## Classificação de viabilidade

| Classe | Famílias |
|---|---:|
| Design comum fechado; providers missing | 1 |
| Design fechado | 1 |
| Design fechado; provider missing | 1 |
| Possível agora | 86 |
| Possível por transport profile | 1 |
| Provável | 65 |
| Rejeitado na baseline | 3 |
| Rejeitado por enquanto | 6 |
| Rejeitado | 18 |

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

