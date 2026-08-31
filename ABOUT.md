# Sobre o W

W é uma linguagem experimental para aplicações nativas, sistemas,
concorrência, paralelismo e computação científica.

O projeto busca uma combinação difícil: uma forma agradável para pessoas e
uma forma explícita para ferramentas. O lema resume essa meta:

> Joy for humans. Clarity for machines.

## Origem

O Git fornece a primeira evidência dedicada ao W em 22 de dezembro de 2020.
O commit `9410b5c` já contém funções, tipos, bindings e uma exploração de
lowering para C. Esta data é a primeira evidência verificável no repositório.
Ela não nega que a ideia possa ser anterior ou ter existido fora do Git.

Em 2021, o projeto passou de uma coleção de formas para uma investigação de
sistema. Módulos, memória, chamadas, concorrência, interoperabilidade e build
apareceram em cadernos e protótipos. O exemplo do restaurante surgiu nesse
período e continua sendo uma referência útil para explicar o método.

## O restaurante como método

O restaurante modela um domínio que possui estados, recursos, serviços e
trabalho concorrente. Uma preparação sequencial pode cortar ingredientes,
marinar a carne e aquecer o forno em ordem. Uma preparação concorrente pode
iniciar essas tarefas de forma estruturada e aguardar seus resultados antes de
montar o prato.

O exemplo começa no domínio. Depois, passa por menu, pedidos, cozinha,
pagamento e atendimento. Cada parte expõe uma pergunta concreta sobre tipos,
ownership, errors, services, tasks e boundaries. O mesmo exemplo ajuda a
avaliar ergonomia, sem fingir que um sketch é um runtime pronto.

Essa abordagem é top-down. Primeiro, o problema e as invariantes ficam claros.
Depois, a linguagem recebe uma forma que pode ser testada. O compilador só
ganha uma implementação quando o contrato e a evidência justificam a camada.

## Reconstrução do design

Em 2026, o projeto foi reconstruído com estados explícitos. O documento
[`DESIGN.md`](DESIGN.md) é a autoridade normativa. O
[`RATIONALE.md`](RATIONALE.md) registra evidência, alternativas e decisões
históricas sem substituir o design.

O produto de referência [`Última Luz`](reference/last-light/README.md) coloca
as formas em um conjunto coerente de fontes `.w`. Os oracles e o tooling
testam contratos, fronteiras e diagnósticos. A gramática e o atlas verificam a
superfície sintática. O seed compiler percorre uma rota limitada de source,
parser, frontend, HIR0 verificada, HLO0 e HLO1; o artefato C conservador é
compilado em modo C23, e C11 fica somente na lane explícita de
recovery/compatibilidade.

Essa rota é evidência de uma fatia. Ela não é o compiler completo, o runtime,
o package manager ou o provider da standard library. O estado atual permanece
parcial por escolha. Uma lacuna declarada é mais útil do que uma promessa sem
teste.

## Princípios

- A forma comum deve ser clara para pessoas e precisa para ferramentas.
- Ownership, borrow, views e lifetime devem compor sem anotações excessivas.
- Concorrência e paralelismo devem permanecer estruturados e explícitos no
  call site.
- Performance deve seguir benchmark-driven development, com correção e
  equivalência antes de ranking.
- ABI, artifacts, provenance e security checks devem permitir auditoria.
- Uma proposta deve distinguir direção, forma vigente, pesquisa e rejeição.
- Um exemplo não prova uma implementação. Um oracle não se apresenta como
  runtime.

## Proveniência

Os documentos substituídos não definem o W atual. O Git é o arquivo da
proveniência e preserva autoria, datas e diffs sem manter uma árvore histórica
no checkout. A narrativa pública fica aqui. A autoridade continua em
`DESIGN.md` e a evidência detalhada fica em `RATIONALE.md`.

O commit imutável `4964d1f` contém os fontes brutos retirados na limpeza. O
commit imutável `25ef412` registra o último estado que ainda continha a árvore
`history/`. Use `git show <commit>:<path>` quando uma auditoria histórica
precisar de um arquivo.

O About é uma apresentação curta. Para contratos, estado, exemplos e gates,
use as fontes canônicas indicadas no [`README.md`](README.md).
