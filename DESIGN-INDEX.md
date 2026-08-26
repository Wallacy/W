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
| linhas de `DESIGN.md` | 32589 |
| tokens aproximados de `DESIGN.md` | 359600 |
| linhas de `RATIONALE.md` | 7776 |
| tokens aproximados de `RATIONALE.md` | 197000 |
| seções numeradas | 27 |
| seções terminais com evidência local | 353/353 |
| decisões | 1484 (W-001–W-1484) |
| famílias de viabilidade | 183 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 75 |
| casos de substituição estruturados | 75/75 |
| decisões referenciadas por casos R0 | 170/1484 |
| decisões classificadas para design freeze | 1484/1484 (822 implementation-evidence-gap; 59 superseded; 91 source-backed-current; 504 oracle-backed-current; 8 rejected) |
| decisões com evidência legada de fonte/oráculo | 535/1484 (170 source + 409 oracle + 8 explícitas; 52 overlaps) |
| decisões ainda sem classe de freeze | 0 |
| decisões com múltiplos eixos obrigatórios | 2 |
| formas R0 com baseline estática | 190 |
| surface lexemes das formas vigentes R0 | 1646 total; mediana 18; máximo 58 |
| bundles executáveis R1 | 57 |
| variantes/tarefas R1 | 162/228 |
| casos R0 promovidos a R1 | 69/75 |
| protocolo HUM0 | 8 slices/32 tasks; 0 human records/0 model records; structure-only |
| casos do corpus Tree-sitter | 127 |
| pares canônicos do formatter F0 | 28 |
| casos/operações do kernel de memória M1 | 185/606 (79 aceitos + 106 rejeitados) |
| casos/operações do control block shared SHC0 | 45/84 (16 aceitos + 6 errors + 3 faults + 20 rejeitados) |
| casos/operações do kernel de allocation físico A0 | 48/123 (15 aceitos + 33 rejeitados) |
| casos/operações do kernel de layout e ABI L0 | 78/96 (27 aceitos + 51 rejeitados) |
| casos/operações do kernel de execução E0 | 73/677 (38 aceitos + 35 rejeitados; 10/10 origens happens-before) |
| casos/operações do kernel de runtime closure E1 | 41/473 (19 aceitos + 22 rejeitados; sete testes host) |
| casos/operações do behavior Lazy LZ0 | 40/118 (16 aceitos + 20 rejeitados + 4 fault; 12 testes host) |
| casos/operações da composição de ownership e execução MX0 | 46/274 (23 aceitos + 23 rejeitados; 14 testes host) |
| casos/operações de channel bounded CH0 | 47/333 (28 aceitos + 19 rejeitados; 12 testes host) |
| casos/operações de contexto local CTX0 | 25/94 (10 aceitos + 15 rejeitados; seis testes host) |
| casos/operações de layout de interferência IL0 | 30/140 (22 aceitos + 8 rejeitados; nove testes host) |
| casos/operações de lock da linguagem LM1 | 39/86 (20 aceitos + 18 rejeitados + 1 fault; 11 testes host) |
| casos/operações do carrier de snapshot SP0 | 27/82 (14 aceitos + 12 rejeitados + 1 fault; sete testes host) |
| casos/operações do kernel de boundary effects B0 | 39/320 (25 aceitos + 14 rejeitados) |
| casos/operações de service recovery SR0 | 48/392 (18 aceitos + 30 rejeitados; 17 testes host) |
| casos/operações do kernel de packages e releases P0 | 44/379 (22 aceitos + 22 rejeitados) |
| casos/operações do workflow module-run RU0 | 12/58 (3 aceitos + 9 rejeitados) |
| casos/operações da sessão transacional PYN2 | 70/298 (56 aceitos + 14 rejeitados) |
| casos/operações de apresentação PYN3 | 26/75 (9 aceitos + 17 rejeitados; host oracle não executa W) |
| casos/operações do adapter Jupyter PYN3 | 32/104 (16 aceitos + 16 rejeitados; host oracle não executa W) |
| casos/operações do export notebook PYN3 | 18/49 (5 aceitos + 13 rejeitados; host oracle não executa W) |
| casos do container WMeta1 W0 | 42 (5 aceitos + 37 rejeitados; 2 readers independentes) |
| casos/operações do carrier tabular TAB0 | 64/155 (22 aceitos + 42 rejeitados; host oracle não executa W) |
| casos/operações dos adapters tabulares TAB1 | 86/193 (36 aceitos + 50 rejeitados; host oracle não executa W) |
| casos/operações do carrier DLPack PYN4 | 75/326 (26 aceitos + 49 rejeitados; host oracle não executa W) |
| casos/operações de device execution DEV0 | 42/186 (17 aceitos + 25 rejeitados; host oracle não executa W) |
| casos/operações da síntese de kernel KM0 | 32/218 (6 aceitos + 26 rejeitados; host oracle não executa W) |
| casos/operações de body estrangeiro FB0 | 45/90 (15 aceitos + 28 rejeitados + 2 informações; host oracle não executa adapter) |
| casos/operações de Web bodies WB0 | 27/160 (12 aceitos + 15 rejeitados; host oracle não executa compiler/provider) |
| casos/operações do root de processo PR0 | 48/249 (27 aceitos + 21 rejeitados; host oracle não executa W/provider) |
| casos/operações do filesystem FS0 | 99/665 (40 aceitos + 59 rejeitados; host oracle não executa syscalls/provider) |
| casos/operações de erro portátil de I/O IOE0 | 44/219 (32 aceitos + 12 rejeitados; host oracle não executa W/provider) |
| casos/operações de tempo operacional TIME0 | 52/277 (27 aceitos + 25 rejeitados; host oracle não executa clock/timer/provider) |
| casos do corpus semântico S0 | 158 (79 positivos + 79 negativos) |
| matriz host SDM0 | 30 (8 oracle aceitos + 22 oracle rejeitados; 3 outcomes aceitos + 5 rejeitados; 24 decisões) |
| outcomes SemanticResult S0 | 158 |
| snapshots de diagnostic D0 | 79 |
| snapshots F0 no formato D0 | 28 |
| codes D0 catalogados | 309/217 |
| sources W no root do Última Luz | 107 |
| sources W em todo o Última Luz | 112 |
| sources W no rascunho da std | 32 |
| módulos/APIs catalogados da std | 32/436 |
| superfícies qualificadas da std usadas pelo Última Luz | 90 |
| requisitos do Última Luz com contrato std | 35/35 |
| requisitos do Última Luz ausentes na std | 0/35 |

A estimativa de tokens usa bytes divididos por quatro. Use o valor somente para planejar leitura.

## Cobertura do catálogo D0

| Família | Catalogados | Referenciados |
|---|---:|---:|
| ALLOCATOR | 11 | 11 |
| ATOMIC | 16 | 16 |
| BEHAVIOR | 0 | 1 |
| BORROW | 12 | 12 |
| CAPABILITY | 1 | 1 |
| CONST | 7 | 7 |
| CONTEXT | 7 | 7 |
| CONTRACT | 5 | 5 |
| DIAGNOSTIC | 1 | 1 |
| DOC | 2 | 2 |
| EFFECT | 2 | 2 |
| EXPR | 4 | 4 |
| FLOW | 2 | 2 |
| FMT | 2 | 2 |
| FOREIGN | 1 | 1 |
| GENERIC | 5 | 5 |
| INIT | 1 | 1 |
| JUPYTER | 1 | 1 |
| LABEL | 4 | 4 |
| LEX | 1 | 1 |
| MATCH | 3 | 3 |
| MEMORY | 1 | 1 |
| MOVE | 1 | 1 |
| OWNERSHIP | 8 | 8 |
| PARSE | 29 | 29 |
| PATTERN | 6 | 6 |
| PLACEMENT | 4 | 4 |
| PROCESS | 3 | 3 |
| RUN | 16 | 16 |
| SEM | 1 | 1 |
| SESSION | 28 | 28 |
| STD | 1 | 1 |
| STREAM | 1 | 1 |
| SUSPEND | 5 | 5 |
| TIME | 2 | 2 |
| TLS | 3 | 3 |
| TYPE | 4 | 5 |
| UNIT | 1 | 1 |
| USE | 1 | 1 |
| WIRE | 1 | 1 |
| YIELD | 11 | 11 |

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 15–231 | 2700 | Como ler este documento |
| 1 | 232–255 | 300 | Limite da alegação |
| 2 | 256–281 | 400 | Invariantes |
| 3 | 282–3680 | 36900 | Contratos estáticos e orçamento de símbolos |
| 4 | 3681–3743 | 400 | Superfície integrada |
| 5 | 3744–4062 | 2900 | Source, nomes e edição |
| 6 | 4063–4470 | 3700 | Módulos, imports e visibilidade |
| 7 | 4471–5279 | 7900 | Bindings, funções e closures |
| 8 | 5280–8604 | 32100 | Tipos e conversões |
| 9 | 8605–10631 | 24200 | Memória, layout e alocação |
| 10 | 10632–10798 | 2100 | Property behaviors |
| 11 | 10799–11163 | 2900 | Erros, panic, OOM e cleanup |
| 12 | 11164–14657 | 40100 | Concorrência, paralelismo e execução |
| 13 | 14658–16935 | 23100 | Módulos de execução, services e entries |
| 14 | 16936–20962 | 51100 | Prelude e standard library |
| 15 | 20963–21774 | 8700 | Números, ranges e unidades |
| 16 | 21775–23432 | 14200 | Texto, bytes e collections |
| 17 | 23433–23782 | 4100 | Matrizes, tensors e ML |
| 18 | 23783–24461 | 7900 | Performance e custo |
| 19 | 24462–25036 | 6900 | FFI, unsafe e ilhas de linguagem |
| 20 | 25037–26559 | 16400 | Compilador e bootstrap |
| 21 | 26560–28817 | 22300 | Packages, builds e releases |
| 22 | 28818–29349 | 5300 | Tooling e interface para máquinas |
| 23 | 29350–30900 | 19100 | Protocolos e pesquisas de ecossistema |
| 24 | 30901–31989 | 18100 | Design freeze e pendências |
| 25 | 31990–32200 | 1900 | Produto de referência Última Luz |
| 26 | 32201–32589 | 5000 | Plano de implementação |

## Navegação compacta de RATIONALE

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 1 | 14–5882 | 93600 | Evidência comparativa |
| 2 | 5883–5913 | 500 | Proveniência |
| 3 | 5914–7776 | 102900 | Ledger |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 15–8604 | 87300 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 8605–16935 | 92400 | ownership, errors, tasks, domains, services e entries |
| std e performance | 14, 15, 16, 17, 18, 19 | 16936–25036 | 92900 | módulos, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 25037–30900 | 63100 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26 | 30901–32589 | 25000 | freeze, Última Luz, gates e roadmap |

O bundle agrupa seções para planejamento; os intervalos não são uma nova autoridade.

## Classificação de viabilidade

| Classe | Famílias |
|---|---:|
| Alternativa | 1 |
| Design comum fechado; providers missing | 1 |
| Design fechado | 1 |
| Design fechado; provider missing | 1 |
| Possível agora | 89 |
| Possível por transport profile | 1 |
| Provável | 61 |
| Rejeitado na baseline | 3 |
| Rejeitado por enquanto | 7 |
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

