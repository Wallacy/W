# Arquivo de design DB1

> **Status:** histórico · 27 de julho de 2026

Este diretório preserva a documentação e o corpus que antecederam a baseline
integrada atual. Os arquivos mantêm contexto, alternativas, links e proveniência.
Eles não definem o W atual.

Use [`W/DESIGN.md`](../../../../W/DESIGN.md) para linguagem, runtime, SDK,
compilador, packages, distribuição, tooling, plano e decisões. Use o
[`Restaurante Última Luz`](../../../../W/examples/restaurant/README.md) para o
ensaio visual atual.

## Conteúdo

- `STATUS.md`, `DB1_REVIEW.md` e `DB1_ADDENDUM.md`: ledger e rodadas de decisão;
- `spec/` e `design/`: contratos anteriores distribuídos por assunto;
- `ARCHITECTURE.md`, `techspec.md`, `VISION.md` e `ROADMAP.md`: mapas anteriores;
- `research/` e `ecosystem/`: hipóteses e extensões;
- `corpus/`: runner e snapshots do subset DB1;
- `examples/restaurant/`: primeira versão do ensaio top-down.

## Destino no design atual

| Material DB1 | Seções de `W/DESIGN.md` |
|---|---|
| visão, status, fechamento e revisões | 0–4, 27 e 28 |
| `spec/syntax.md` | 3–8 e 15–18 |
| `spec/types-and-memory.md` | 8–11 |
| `spec/concurrency.md` | 12 |
| `spec/modules.md` | 6 e 13 |
| compiler e bootstrap | 19 |
| memória e tagged values | 9–11 e 23 |
| módulos, services e runtime | 12–13 |
| SDK e stdlib | 14 |
| numéricos, units e tensor | 15–17 |
| packages, verificação e releases | 20 |
| formatter, documentação, testes e recursos | 21 |
| ecossistema e pesquisa de longo prazo | 22–23 |
| roadmap | 26 |

Consulte estes arquivos somente para auditoria histórica ou para recuperar uma
alternativa que não esteja no registro D2 de `W/DESIGN.md`.
