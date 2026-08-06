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
objetivo, pesquisa as fontes necessárias, compara alternativas, decide a
semântica, a API e os invariantes, define o critério de saída, delega, espera e
revisa. Ele não executa o volume mecânico nem repete a aplicação do worker.

O worker é `w_luna_worker`, configurado em `.codex/agents/` com Luna Max. Use
somente um worker ativo. Reuse sua thread no mesmo bundle para draft, correções,
gate final e commit. Depois de um commit limpo, use um worker novo para o próximo
bundle independente. Não use Ultra ou agentes paralelos.

Somente o coordenador cria o worker. O próprio worker executa o pacote sozinho:
ele não cria, delega ou coordena subagentes, nem mesmo durante correções ou
checks longos. Se precisar de outra autoridade ou premissa, ele devolve o
bloqueio ao coordenador.

O alvo é deixar pelo menos 95% do volume operacional no worker. Esse volume
inclui edição, atualização de source de referência, grammar, tooling, projeções,
checks e relato do diff. A meta não se aplica ao raciocínio arquitetural:
pesquisa, comparação e decisão pertencem ao coordenador. O valor é uma meta,
não uma métrica garantida pelo Codex. Não gaste contexto tentando medi-lo
durante a tarefa. Contadores brutos, principalmente input em cache, não medem
diretamente custo, quota ou qualidade. Audite o fluxo somente em checkpoints.

O coordenador age diretamente para:

- conversar sobre o próprio fluxo;
- pedir uma clarificação que muda materialmente o resultado;
- pesquisar fontes e contratos vizinhos;
- comparar alternativas e decidir semântica, API e invariantes;
- relatar status, bloqueio ou resultado final;
- revisar instruções de agente;
- operar quando o usuário autorizar um fallback após Luna ficar indisponível.

Depois do spawn, o coordenador limita a operação direta a revisão estratégica:
status Git, diff resumido, hunks semânticos, resumo dos checks e confirmação do
commit. Termine pesquisa e leitura canônica antes de delegar. Não refaça checks
ou aplicação do worker.

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
Contrato decidido: forma exata, invariantes, alternativas e limites de escopo.
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

Antes de enviar trabalho substantivo, confirme uma vez nos metadados do runtime
ou da interface que a thread usa `gpt-5.6-luna` com effort `max`.
O nome `w_luna_worker` sozinho não comprova o modelo. Essa confirmação não é
polling. Se os metadados faltarem, Luna Max não estiver disponível ou o
modelo divergir, pare e informe o usuário. Não use Sol, Terra ou outro effort
como fallback silencioso.

Depois de alterar `.codex/config.toml` ou `.codex/agents/`, inicie uma tarefa
nova antes do teste. Uma tarefa existente pode preservar a configuração antiga.

## Granularidade do bundle

Um bundle operacional fecha uma unidade semântica que pode ser revisada e
revertida como uma decisão: contrato canônico, contratos dependentes, Última
Luz, grammar quando necessária, tooling, projeções e checks. Não divida trabalho
por arquivo ou por alteração pequena.

Separe bundles quando duas decisões puderem ser aceitas ou revertidas de forma
independente. Separe também uma mudança de infraestrutura habilitadora quando
ela introduzir um formato novo de checker ou snapshot além do contrato da
linguagem. O número de arquivos e o churn gerado não são limites por si só.

O fluxo esperado usa dois turnos do worker quando o draft é aprovado: draft e
gate final com commit. Quando há correções, usa três: draft, correção consolidada
e gate final com commit. Mais de duas rodadas de correção indicam que o contrato
chegou aberto demais ou que a estratégia precisa de autorização do usuário.

## Trabalho de design da linguagem

Sol não entrega uma pergunta arquitetural aberta ao worker. Antes do spawn, o
coordenador pesquisa fontes primárias, compara alternativas e fecha o bundle de
decisão: forma recomendada, semântica, API, invariantes, contratos vizinhos,
alternativas preservadas, limites de escopo, evidência esperada e condições de
rejeição.

Luna Max aplica esse pacote. O worker lê somente os slices necessários,
atualiza `DESIGN.md`, source de referência, grammar quando necessária, tooling,
projeções e Última Luz, executa os checks afetados e relata o diff. Ele não cria
design novo, não muda alternativas, não amplia o escopo e não faz pesquisa
comparativa aberta. Ele não usa pesquisa Web, salvo quando o pacote indicar uma
fonte e uma verificação operacional específica. Ele não cria ou atualiza um
plano visível, salvo pedido explícito do coordenador. Se a aplicação revelar uma
contradição que exige nova decisão, ele para e devolve o bloqueio objetivo ao
coordenador.

O coordenador revisa o resultado com critérios fixos: composição com o
restante da linguagem, previsibilidade runtime, ergonomia humana, clareza para
modelos, implementabilidade, performance, segurança e capacidade de teste. Se
a revisão exigir uma nova premissa central, o coordenador volta à pesquisa e à
decisão. Ele não transfere a decisão para Luna por meio de uma pergunta aberta.

Uma decisão ampla não fica ratificada apenas porque Luna aplicou um pacote
coerente. O coordenador responde pela decisão. O estado, as alternativas e a
evidência precisam estar explícitos em `DESIGN.md`, e o Última Luz precisa
tornar o contrato visível antes do commit.

## Espera e interação

Depois de delegar, o coordenador fica idle. Use espera orientada a evento. Não
leia a memória do worker, não faça polling de logs e não crie um arquivo de
status por padrão.

Se o usuário complementar a tarefa, envie somente o delta ao mesmo worker. Não
abra outro agente e não reenvie o pacote completo. Interrompa o worker somente
quando a nova instrução invalida o trabalho em curso.

Reuse o worker enquanto as tarefas pertencem ao mesmo bundle ou dependem do
contexto acumulado. Não preserve uma thread indefinidamente por princípio. Após
um commit limpo, rotacione o worker para um bundle independente. Rotacione antes
se aparecerem repetição, esquecimento de decisões recentes ou releitura
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
