# Orientações do repositório

W é uma linguagem experimental em fase de projeto. Este repositório é a fonte
de trabalho do W depois da extração do monorepo `wallacy.com`.

Antes de uma tarefa sobre W, leia `.codex/W.md`. Leia `CONTRIBUTING.md` para
mudança pública, preparação de pull request ou política de contribuição. Para
revisão ou merge, leia `MAINTAINERS.md`. `GOVERNANCE.md` define autoridade e
decisão.

Para trabalho longo, leia `.codex/W-WORKFLOW.md`. Para texto novo ou revisado,
leia `.codex/WRITING.md`. Use ASD-STE100 Issue 9 no texto técnico em inglês e
as regras equivalentes para português.

## Orquestração padrão

Toda tarefa substantiva segue `.codex/W-WORKFLOW.md`. A tarefa principal usa
Sol High ou superior para design e revisão. O Sol delega a execução fechada a
um único `w_luna_worker` com Luna Max. Não crie agentes paralelos.

Mantenha o Sol principal durante a jornada contínua. Reuse o mesmo Luna somente
no bundle corrente. Use um Luna novo para um bundle independente. Confirme o
modelo nos metadados do filho. O nome da tarefa não comprova o modelo. Não
substitua modelo ou effort silenciosamente.

Uma pausa termina quando o usuário pede para continuar ou quando o objetivo
ativo retoma a execução. Retome o bundle imediatamente nesse caso. Não repita
mensagens de espera sem mudança. Um timeout não é progresso e não justifica uma
mensagem ao usuário. Siga os limites de espera e checkpoint de
`.codex/W-WORKFLOW.md`.

## Artefatos canônicos

- `DESIGN.md` é a autoridade normativa para contratos correntes, estado,
  pesquisas que mudam o contrato e ordem de implementação.
- `RATIONALE.md` é complementar e não normativa: guarda justificativas,
  evidência, alternativas e proveniência histórica.
- `DESIGN-INDEX.md` é uma projeção gerada para navegação e leitura seletiva.
- `reference/last-light/` é o produto de referência e o alvo da especificação
  executável.
- `portal/` e `tooling/` são projeções e ferramentas. Não definem a semântica.
- `history/` preserva proveniência obsoleta, ideias antigas e material
  arquivado. Não use o histórico como decisão corrente.

Não apresente uma proposta como comportamento implementado. Antes do W 1.0,
não preserve compatibilidade por inércia. Depois do 1.0, toda compatibilidade
temporária precisa de depreciação, substituição, data de remoção e caminho de
migração.

## Execução eficiente

Use Bun para tooling JavaScript, scripts e lockfiles. Não crie um lockfile npm.
Comece por `DESIGN-INDEX.md` e use `tooling/design-slice.mjs` para ler apenas o
trecho necessário. Não leia fontes geradas em
`tooling/tree-sitter-w/src/` durante o trabalho normal. Mantenha uma única
fonte para cada conceito e atualize suas projeções somente depois de estabilizar
a decisão canônica.

Antes de editar, verifique `git status --short`. Use saídas de comandos curtas,
não repita leituras ou testes cujas entradas não mudaram e valide apenas a
superfície afetada antes da revisão final. Termine com `git diff --check`, um
diff resumido e os checks relevantes.
