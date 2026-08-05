# Fluxo do Codex para W

> **Status:** orientação operacional para agentes, não parte da linguagem
> **Coordenador recomendado:** **GPT-5.6 Sol · High ou superior**
> **Worker padrão:** **GPT-5.6 Luna · Max**

`CONTRIBUTING.md` define o fluxo compartilhado. `MAINTAINERS.md` define revisão,
merge e manutenção. Este arquivo acrescenta somente roteamento de modelos,
delegação e controle de contexto.

Uma instrução de agente não concede autoridade. `GOVERNANCE.md` reserva merge,
release e segurança para pessoas responsáveis.

## Arquitetura coordinator-worker

O modelo principal é coordenador. Ele representa o mantenedor humano: entende o
objetivo, define o critério de saída, delega, espera e revisa. Ele não implementa
a tarefa nem repete a exploração do worker.

O worker é `w_luna_worker`, configurado em `.codex/agents/` com Luna Max. Use
somente um worker por sessão. Reuse sua thread para todas as correções e tarefas
relacionadas. Não use Ultra ou agentes paralelos.

Somente o coordenador cria o worker. O próprio worker executa o pacote sozinho:
ele não cria, delega ou coordena subagentes, nem mesmo durante correções ou
checks longos. Se precisar de outra autoridade ou premissa, ele devolve o
bloqueio ao coordenador.

O alvo operacional é deixar pelo menos 95% do trabalho de modelo no worker.
Esse valor é uma meta, não uma métrica garantida pelo Codex. Não gaste contexto
tentando medi-lo durante a tarefa.

O coordenador pode agir diretamente somente para:

- conversar sobre o próprio fluxo;
- pedir uma clarificação que muda materialmente o resultado;
- relatar status, bloqueio ou resultado final;
- revisar instruções de agente;
- operar quando o usuário autorizar um fallback após Luna ficar indisponível.

## Início da tarefa

O coordenador lê o pedido e as instruções ativas uma vez. Ele pensa no resultado
de ponta a ponta. Antes do spawn, ele fixa um plano de interação: contato inicial,
espera, inspeção final e, somente se necessário, uma correção consolidada. O
plano também define quais evidências justificam redirecionamento durante a
execução. Status periódico não é uma evidência.

Depois, o coordenador cria um pacote curto:

```text
Papel: worker W.
Objetivo: resultado observável de ponta a ponta.
Contexto já decidido: fatos necessários que não devem ser redescobertos.
Entradas: índices, seções e arquivos iniciais.
Escopo de escrita: paths ou conceitos autorizados.
Restrições: o que não pode mudar.
Concluído quando: checks, inspeções e estado Git esperados.
Commit: não no primeiro draft; aguarde revisão, salvo ordem contrária.
Retorno: resultado, arquivos, checks, riscos e diff resumido.
```

Crie o worker com o nome `w_luna_worker` e sem copiar o histórico completo
quando a interface permitir, como `fork_turns: none`. O pacote deve conter o
contexto mínimo suficiente. Configuração e instruções do repositório fornecem o
restante. Não faça uma segunda leitura da codebase antes de delegar.

Se Luna Max não estiver disponível no seletor ou runtime atual, pare e informe
o usuário. Não use Sol, Terra ou outro effort como fallback silencioso.

## Trabalho de design da linguagem

Sol não deve entregar uma pergunta arquitetural aberta ao worker. Antes do
spawn, o coordenador define o bundle de decisão: objetivo, invariantes que não
podem regredir, contratos vizinhos, evidência esperada e condições de rejeição.
Ele define a moldura, não a solução.

Luna Max executa o volume: lê o índice e slices necessários, pesquisa fontes
primárias, compara alternativas, verifica viabilidade, atualiza os artefatos e
expande o Última Luz. O primeiro draft deve separar:

- forma recomendada e motivo;
- alternativas preservadas e condição para reconsiderá-las;
- impacto para humanos, máquinas, implementação e performance;
- contratos afetados e exemplos adversariais;
- dúvidas que nenhuma evidência atual resolve.

O coordenador revisa design com critérios fixos: composição com o restante da
linguagem, previsibilidade runtime, ergonomia humana, clareza para modelos,
implementabilidade, performance, segurança e capacidade de teste. Se a revisão
exigir uma nova premissa central, não transforme a solução de Sol em uma longa
lista de microcorreções para Luna. Pare e peça ao usuário autorização para usar
um worker mais forte naquele bundle.

Uma decisão ampla não fica ratificada apenas porque Luna produziu um documento
coerente. O estado, as alternativas e a evidência precisam estar explícitos em
`DESIGN.md`, e o Última Luz precisa tornar o contrato visível antes do commit.

## Espera e interação

Depois de delegar, o coordenador fica idle. Use espera orientada a evento. Não
leia a memória do worker, não faça polling de logs e não crie um arquivo de
status por padrão.

Se o usuário complementar a tarefa, envie somente o delta ao mesmo worker. Não
abra outro agente e não reenvie o pacote completo. Interrompa o worker somente
quando a nova instrução invalida o trabalho em curso.

Reuse o worker enquanto as tarefas pertencem ao mesmo bundle ou dependem do
contexto acumulado. Não preserve uma thread indefinidamente por princípio. Após
um commit limpo, rotacione o worker quando o próximo bundle for independente ou
quando aparecerem repetição, esquecimento de decisões recentes ou releitura
excessiva. O novo worker parte dos artefatos canônicos, não do histórico do chat.

Um arquivo temporário de status é aceitável somente quando uma ferramenta longa
não produz eventos e o estado é necessário para recuperação. O worker é dono do
arquivo e o remove antes do handoff.

## Revisão adversarial

Quando o worker terminar, o coordenador inspeciona estrategicamente:

1. `git status --short` e `git diff --stat`;
2. hunks que mudam contratos, autoridade ou comportamento;
3. resumo dos checks e o primeiro erro útil, se houver;
4. aderência ao pedido, às fontes canônicas e ao escopo;
5. regressões, omissões, artefatos temporários e trabalho não relacionado.

Não releia arquivos inteiros que o diff não afetou. Não refaça a implementação.
Envie ao mesmo worker uma única lista consolidada de correções. Depois do novo
handoff, repita somente as verificações atingidas. Se o trabalho continuar ruim
após duas rodadas, informe o usuário e peça autorização para mudar o modelo ou a
estratégia.

Quando a revisão for aprovada, peça ao worker para executar o gate final, fazer
o commit e devolver hash e estado Git. O coordenador confirma o resultado e
responde ao usuário.

## Contexto

Entre pelo menor artefato:

1. `DESIGN-INDEX.md` para localização;
2. `tooling/design-slice.mjs` para seção ou ID;
3. `reference/last-light/README.md` para localizar o source afetado;
4. `history/HISTORY.md` somente para provenance.

Não leia `DESIGN.md` integralmente sem uma revisão integral explícita.
Não leia `tooling/tree-sitter-w/src/` fora de uma falha de geração.

O cache humano é:

- `DESIGN.md` para contratos e decisões;
- `DESIGN-INDEX.md` para métricas e intervalos;
- `reference/last-light/` para especificação executável;
- `CONTRIBUTING.md` para fluxo;
- `MAINTAINERS.md` para revisão;
- `history/` para provenance.

Não copie uma explicação canônica para este arquivo.

## Fechamento de pendências

Quando o usuário pedir todas as pendências:

1. leia o índice e a seção 29 uma vez;
2. consulte somente as seções ligadas aos bundles;
3. dê recomendação, status e fallback para cada ID;
4. agrupe perguntas independentes em um único formulário;
5. atualize o design somente depois da ratificação;
6. valide as projeções uma vez no final.

## Saída de comandos

Use estes defaults:

- inventário: paths e contagens;
- busca: poucas dezenas de correspondências;
- leitura: intervalo necessário;
- teste aprovado: código de saída e resumo;
- teste falho: primeiro erro e até 80 linhas úteis;
- diff durante edição: `--stat` e arquivos;
- pesquisa: fonte primária por afirmação técnica.

Não repita um check quando nenhuma entrada mudou.

## Fontes desta política

- [OpenAI — Customization e AGENTS.md](https://developers.openai.com/codex/concepts/customization#agents-guidance)
- [OpenAI — modelos no Codex](https://learn.chatgpt.com/docs/models#recommended-models)
- [OpenAI — subagentes](https://learn.chatgpt.com/docs/agent-configuration/subagents)

Reavalie o roteamento quando modelos, limites ou avaliações mudarem.
