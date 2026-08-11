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
| linhas de `DESIGN.md` | 28064 |
| tokens aproximados de `DESIGN.md` | 291500 |
| linhas de `RATIONALE.md` | 4087 |
| tokens aproximados de `RATIONALE.md` | 112600 |
| seções numeradas | 27 |
| seções terminais com evidência local | 329/329 |
| decisões | 1218 (W-001–W-1218) |
| famílias de viabilidade | 182 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 69 |
| casos de substituição estruturados | 69/69 |
| decisões referenciadas por casos R0 | 129/1218 |
| decisões classificadas para design freeze | 409/1218 (129 source + 319 oracle + 8 explícitas; 47 overlaps) |
| decisões ainda sem classe de freeze | 809 |
| decisões com múltiplos eixos obrigatórios | 2 |
| formas R0 com baseline estática | 169 |
| surface lexemes das formas vigentes R0 | 1404 total; mediana 16; máximo 52 |
| bundles executáveis R1 | 21 |
| variantes/tarefas R1 | 51/84 |
| casos R0 promovidos a R1 | 32/69 |
| casos do corpus Tree-sitter | 101 |
| pares canônicos do formatter F0 | 21 |
| casos/operações do kernel de memória M1 | 184/603 (82 aceitos + 102 rejeitados) |
| casos/operações do kernel de allocation físico A0 | 48/123 (15 aceitos + 33 rejeitados) |
| casos/operações do kernel de layout e ABI L0 | 78/96 (27 aceitos + 51 rejeitados) |
| casos/operações do kernel de execução E0 | 73/677 (38 aceitos + 35 rejeitados; 10/10 origens happens-before) |
| casos/operações do kernel de runtime closure E1 | 41/473 (19 aceitos + 22 rejeitados; sete testes host) |
| casos/operações do behavior Lazy LZ0 | 40/118 (16 aceitos + 20 rejeitados + 4 fault; 12 testes host) |
| casos/operações da composição de ownership e execução MX0 | 46/274 (23 aceitos + 23 rejeitados; 14 testes host) |
| casos/operações de channel bounded CH0 | 47/333 (28 aceitos + 19 rejeitados; 12 testes host) |
| casos/operações de locks escopados LM0 | 42/171 (25 aceitos + 16 rejeitados + 1 fault; oito testes host) |
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
| casos/operações de device execution DEV0 | 39/181 (17 aceitos + 22 rejeitados; host oracle não executa W) |
| casos do corpus semântico S0 | 108 (54 positivos + 54 negativos) |
| outcomes SemanticResult S0 | 108 |
| snapshots de diagnostic D0 | 54 |
| snapshots F0 no formato D0 | 21 |
| codes D0 catalogados | 237/153 |
| sources W no root do Última Luz | 89 |
| sources W em todo o Última Luz | 97 |
| sources W no rascunho da std | 23 |
| módulos/APIs catalogados da std SDK0 | 23/326 |
| superfícies qualificadas da std usadas pelo Última Luz | 84 |
| requisitos do Última Luz com contrato std SDK0 | 26/26 |
| requisitos do Última Luz ausentes na std SDK0 | 1/26 |

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
| 0 | 15–221 | 2500 | Como ler este documento |
| 1 | 222–245 | 300 | Limite da alegação |
| 2 | 246–271 | 400 | Invariantes |
| 3 | 272–3268 | 30400 | Contratos estáticos e orçamento de símbolos |
| 4 | 3269–3331 | 400 | Superfície integrada |
| 5 | 3332–3639 | 2700 | Source, nomes e edição |
| 6 | 3640–4045 | 3700 | Módulos, imports e visibilidade |
| 7 | 4046–4703 | 6000 | Bindings, funções e closures |
| 8 | 4704–7140 | 19000 | Tipos e conversões |
| 9 | 7141–8631 | 17000 | Memória, layout e alocação |
| 10 | 8632–8754 | 1400 | Property behaviors |
| 11 | 8755–9100 | 2700 | Erros, panic, OOM e cleanup |
| 12 | 9101–11952 | 30800 | Concorrência, paralelismo e execução |
| 13 | 11953–14092 | 21200 | Módulos de execução, services e entries |
| 14 | 14093–17762 | 46500 | Prelude e SDK |
| 15 | 17763–18462 | 7100 | Números, ranges e unidades |
| 16 | 18463–20035 | 13200 | Texto, bytes e collections |
| 17 | 20036–20380 | 4000 | Matrizes, tensors e ML |
| 18 | 20381–20834 | 4400 | Performance e custo |
| 19 | 20835–21216 | 4300 | FFI, unsafe e ilhas de linguagem |
| 20 | 21217–22751 | 16600 | Compilador e bootstrap |
| 21 | 22752–24778 | 19200 | Packages, builds e releases |
| 22 | 24779–25285 | 4900 | Tooling e interface para máquinas |
| 23 | 25286–26689 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 26690–27527 | 11500 | Design freeze e pendências |
| 25 | 27528–27738 | 1900 | Produto de referência Última Luz |
| 26 | 27739–28064 | 4000 | Plano de implementação |

## Navegação compacta de RATIONALE

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 1 | 14–2821 | 42900 | Evidência comparativa |
| 2 | 2822–2852 | 500 | Proveniência |
| 3 | 2853–4087 | 69200 | Ledger |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 15–7140 | 65400 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 7141–14092 | 73100 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 14093–21216 | 79500 | módulos, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 21217–26689 | 57300 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26 | 26690–28064 | 17400 | freeze, Última Luz, gates e roadmap |

O bundle agrupa seções para planejamento; os intervalos não são uma nova autoridade.

## Classificação de viabilidade

| Classe | Famílias |
|---|---:|
| Pesquisa | 1 |
| Possível agora | 86 |
| Possível por transport profile | 1 |
| Provável após C | 1 |
| Provável | 69 |
| Rejeitado na baseline | 2 |
| Rejeitado por enquanto | 6 |
| Rejeitado | 16 |

## Pesquisas explícitas

- barreira cíclica/reutilizável — Pesquisa

## Comandos de leitura

```powershell
bun tooling/design-slice.mjs --section 12
bun tooling/design-slice.mjs --heading 12.13
bun tooling/design-slice.mjs --id W-711 --context 2
bun tooling/design-slice.mjs --rationale-heading 1.3
rg -n -C 4 'transaction' DESIGN.md
bun tooling/design-index.mjs --check
```

