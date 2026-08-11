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
| linhas de `DESIGN.md` | 28661 |
| tokens aproximados de `DESIGN.md` | 299400 |
| linhas de `RATIONALE.md` | 4642 |
| tokens aproximados de `RATIONALE.md` | 123700 |
| seções numeradas | 27 |
| seções terminais com evidência local | 335/335 |
| decisões | 1289 (W-001–W-1289) |
| famílias de viabilidade | 182 |
| slices normativos de grammar | 6 |
| requisitos de ratificação comparativa | 70 |
| casos de substituição estruturados | 70/70 |
| decisões referenciadas por casos R0 | 141/1289 |
| decisões classificadas para design freeze | 466/1289 (141 source + 365 oracle + 8 explícitas; 48 overlaps) |
| decisões ainda sem classe de freeze | 823 |
| decisões com múltiplos eixos obrigatórios | 2 |
| formas R0 com baseline estática | 174 |
| surface lexemes das formas vigentes R0 | 1443 total; mediana 16; máximo 52 |
| bundles executáveis R1 | 24 |
| variantes/tarefas R1 | 59/96 |
| casos R0 promovidos a R1 | 36/70 |
| casos do corpus Tree-sitter | 108 |
| pares canônicos do formatter F0 | 23 |
| casos/operações do kernel de memória M1 | 184/603 (82 aceitos + 102 rejeitados) |
| casos/operações do kernel de allocation físico A0 | 48/123 (15 aceitos + 33 rejeitados) |
| casos/operações do kernel de layout e ABI L0 | 78/96 (27 aceitos + 51 rejeitados) |
| casos/operações do kernel de execução E0 | 73/677 (38 aceitos + 35 rejeitados; 10/10 origens happens-before) |
| casos/operações do kernel de runtime closure E1 | 41/473 (19 aceitos + 22 rejeitados; sete testes host) |
| casos/operações do behavior Lazy LZ0 | 40/118 (16 aceitos + 20 rejeitados + 4 fault; 12 testes host) |
| casos/operações da composição de ownership e execução MX0 | 46/274 (23 aceitos + 23 rejeitados; 14 testes host) |
| casos/operações de channel bounded CH0 | 47/333 (28 aceitos + 19 rejeitados; 12 testes host) |
| casos/operações de contexto local CTX0 | 25/94 (10 aceitos + 15 rejeitados; seis testes host) |
| casos/operações de layout de interferência IL0 | 30/140 (22 aceitos + 8 rejeitados; nove testes host) |
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
| casos/operações de device execution DEV0 | 42/186 (17 aceitos + 25 rejeitados; host oracle não executa W) |
| casos/operações da síntese de kernel KM0 | 32/218 (6 aceitos + 26 rejeitados; host oracle não executa W) |
| casos/operações de body estrangeiro FB0 | 45/90 (15 aceitos + 28 rejeitados + 2 informações; host oracle não executa adapter) |
| casos/operações de Web bodies WB0 | 27/160 (12 aceitos + 15 rejeitados; host oracle não executa compiler/provider) |
| casos do corpus semântico S0 | 116 (58 positivos + 58 negativos) |
| outcomes SemanticResult S0 | 116 |
| snapshots de diagnostic D0 | 58 |
| snapshots F0 no formato D0 | 23 |
| codes D0 catalogados | 260/166 |
| sources W no root do Última Luz | 93 |
| sources W em todo o Última Luz | 101 |
| sources W no rascunho da std | 25 |
| módulos/APIs catalogados da std SDK0 | 25/334 |
| superfícies qualificadas da std usadas pelo Última Luz | 87 |
| requisitos do Última Luz com contrato std SDK0 | 25/25 |
| requisitos do Última Luz ausentes na std SDK0 | 0/25 |

A estimativa de tokens usa bytes divididos por quatro. Use o valor somente para planejar leitura.

## Cobertura do catálogo D0

| Família | Catalogados | Referenciados |
|---|---:|---:|
| BORROW | 10 | 10 |
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
| TLS | 3 | 3 |
| TYPE | 4 | 4 |
| USE | 1 | 1 |
| WIRE | 1 | 1 |

## Navegação por seção

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 0 | 15–230 | 2700 | Como ler este documento |
| 1 | 231–254 | 300 | Limite da alegação |
| 2 | 255–280 | 400 | Invariantes |
| 3 | 281–3301 | 30700 | Contratos estáticos e orçamento de símbolos |
| 4 | 3302–3364 | 400 | Superfície integrada |
| 5 | 3365–3672 | 2700 | Source, nomes e edição |
| 6 | 3673–4078 | 3700 | Módulos, imports e visibilidade |
| 7 | 4079–4736 | 6000 | Bindings, funções e closures |
| 8 | 4737–7221 | 19500 | Tipos e conversões |
| 9 | 7222–8761 | 17500 | Memória, layout e alocação |
| 10 | 8762–8884 | 1400 | Property behaviors |
| 11 | 8885–9230 | 2700 | Erros, panic, OOM e cleanup |
| 12 | 9231–12143 | 31900 | Concorrência, paralelismo e execução |
| 13 | 12144–14416 | 22900 | Módulos de execução, services e entries |
| 14 | 14417–18176 | 47900 | Prelude e SDK |
| 15 | 18177–18881 | 7100 | Números, ranges e unidades |
| 16 | 18882–20481 | 13500 | Texto, bytes e collections |
| 17 | 20482–20831 | 4100 | Matrizes, tensors e ML |
| 18 | 20832–21323 | 4900 | Performance e custo |
| 19 | 21324–21890 | 6800 | FFI, unsafe e ilhas de linguagem |
| 20 | 21891–23396 | 16100 | Compilador e bootstrap |
| 21 | 23397–25409 | 19000 | Packages, builds e releases |
| 22 | 25410–25916 | 4900 | Tooling e interface para máquinas |
| 23 | 25917–27320 | 16600 | Protocolos e pesquisas de ecossistema |
| 24 | 27321–28124 | 10900 | Design freeze e pendências |
| 25 | 28125–28335 | 1900 | Produto de referência Última Luz |
| 26 | 28336–28661 | 4000 | Plano de implementação |

## Navegação compacta de RATIONALE

| Seção | Linhas | Tokens aproximados | Tema |
|---:|---:|---:|---|
| 1 | 14–3305 | 50300 | Evidência comparativa |
| 2 | 3306–3336 | 500 | Proveniência |
| 3 | 3337–4642 | 72900 | Ledger |

## Bundles de leitura

Use um bundle para uma revisão de domínio. Depois leia somente os headings e IDs ligados à pergunta; não copie o bundle para outro documento.

| Bundle | Seções | Linhas | Tokens aproximados | Foco |
|---|---:|---:|---:|---|
| orientação e superfície | 0, 1, 2, 3, 4, 5, 6, 7, 8 | 15–7221 | 66400 | promessa, símbolos, source, módulos, funções e tipos |
| segurança e execução | 9, 10, 11, 12, 13 | 7222–14416 | 76400 | ownership, errors, tasks, domains, services e entries |
| SDK e performance | 14, 15, 16, 17, 18, 19 | 14417–21890 | 84300 | módulos, números, texto, tensors, custo, C e unsafe |
| compiler e distribuição | 20, 21, 22, 23 | 21891–27320 | 56600 | frontend, HIR, packages, releases, tooling e protocolos |
| validação e decisões | 24, 25, 26 | 27321–28661 | 16800 | freeze, Última Luz, gates e roadmap |

O bundle agrupa seções para planejamento; os intervalos não são uma nova autoridade.

## Classificação de viabilidade

| Classe | Famílias |
|---|---:|
| Design comum fechado; providers missing | 1 |
| Design fechado | 1 |
| Design fechado; provider missing | 1 |
| Possível agora | 86 |
| Possível por transport profile | 1 |
| Provável | 64 |
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

