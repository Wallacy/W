# Fluxo do Codex para W

> **Status:** orientação operacional, não parte da linguagem
> **Coordenador principal:** **GPT-5.6 Sol · High ou superior**
> **Executor:** **GPT-5.6 Luna · Max**

Este arquivo define roteamento de modelos e controle de contexto.
`CONTRIBUTING.md` define contribuição. `MAINTAINERS.md` define revisão e merge.
`GOVERNANCE.md` reserva autoridade humana. Uma instrução de agente não concede
autoridade.

## Contrato dos modelos

O Sol principal mantém a conversa. Ele pesquisa, compara alternativas, decide
design, fecha o bundle e revisa o resultado. Ele evita trabalho operacional que
o Luna pode executar com um contrato completo.

O `w_luna_worker` aplica o bundle. Ele usa Luna Max, edita, valida, refatora e
faz o commit após revisão. Ele não decide uma questão arquitetural aberta e não
cria subagentes.

Use somente um Luna ativo. Não crie agentes paralelos. Mantenha pelo menos 95%
do consumo de tokens da tarefa no Luna. Essa porcentagem é uma meta de
comportamento, não uma métrica do Codex. Não gaste contexto para calculá-la.

## Quando usar Luna

Use Luna quando o Sol já definiu estes itens:

- objetivo e resultado observável;
- forma, semântica e invariantes;
- escopo de escrita e contratos vizinhos;
- checks e condição de parada.

Não use Luna para uma destas ações:

- decidir sintaxe, semântica, API ou arquitetura;
- comparar alternativas abertas;
- interpretar uma contradição nova;
- responder um status ou uma pergunta curta.

Se o contrato não cabe em um pacote compacto, Sol continua o design. Ele não
transfere uma pergunta aberta ao Luna.

## Fluxo de um bundle

### 1. Delimitar e decidir

Sol lê o pedido e as instruções uma vez. Ele começa por `DESIGN-INDEX.md` e abre
somente os slices necessários. Ele verifica `git status --short` antes do
spawn.

Um bundle fecha uma decisão que pode ser revisada e revertida como unidade. Ele
pode incluir `DESIGN.md`, `RATIONALE.md`, contratos dependentes, Última Luz,
grammar, tooling, projeções e checks. Não divida o bundle por arquivo.

Sol decide a forma recomendada, as alternativas preservadas, os invariantes,
os contratos vizinhos, as condições de rejeição e a aceitação. Depois, ele
prepara este pacote:

```text
Papel: worker W.
Objetivo: resultado observável de ponta a ponta.
Fatos: evidência confirmada que não deve ser redescoberta.
Contrato: forma, semântica, invariantes e alternativas fechadas.
Entradas: índices, W IDs, slices e arquivos iniciais.
Escrita: paths e conceitos autorizados.
Restrições: contratos e superfícies que não podem mudar.
Concluído quando: checks, diff, commit e estado Git esperados.
Retorno: resultado, arquivos, checks, riscos e diff resumido.
```

O pacote é finito. Ele não usa objetivos como “continue pesquisando” ou “melhore
até ficar pronto”. Ele define milestones ordenados, condições observáveis de
conclusão e uma cadência de status. A cadência padrão é dez minutos. Um bundle
pode durar horas quando os checkpoints mostram avanço verificável.

Cada checkpoint do Luna contém somente:

- milestone atual e último milestone concluído;
- último check executado e seu resultado;
- próximo passo finito;
- bloqueio ou nova premissa, quando existir.

O Luna envia um checkpoint ao concluir cada milestone ou quando a cadência
expira, o que ocorrer primeiro. Se o Sol pedir status, o Luna responde no próximo
boundary de mensagem antes de executar mais trabalho. A resposta não repete o
contrato do bundle.

### 2. Criar o worker

Crie exatamente um `w_luna_worker` com contexto novo. Selecione o perfil quando
a interface expuser `agent_type`. Caso contrário, informe explicitamente
`model: "gpt-5.6-luna"` e `reasoning_effort: "max"`, e inclua o contrato do
perfil no pacote. Use `fork_context: false` ou `fork_turns: "none"`, conforme a
interface.

Confirme nos metadados que o filho usa Luna Max. O nome do perfil ou da tarefa
não comprova o modelo. Se o runtime selecionar outro modelo, interrompa o filho
e informe o usuário. Não use fallback silencioso.

Sol usa espera orientada a evento. Ele não lê o contexto privado do Luna e não
duplica a execução. Um timeout não é progresso, bloqueio ou informação para o
usuário. Sol não publica mensagens que apenas dizem que o worker continua
ativo.

Use a maior espera prática que a interface permite. Não faça busy polling. Sol
fica silencioso entre eventos do Luna. Quando a cadência expira sem checkpoint,
Sol envia um único pedido de status. O Luna responde antes de continuar.

Se o pedido não receber resposta em cinco minutos, Sol consulta a liveness uma
vez. Se o Luna ainda estiver executando, Sol o interrompe somente para obter o
checkpoint e retoma o mesmo agente depois da resposta. Essa interrupção não
abandona o bundle e não cria outro worker.

Duração isolada não é motivo para parar. Três checkpoints consecutivos no mesmo
milestone, sem novo diff, check ou fato, indicam ausência de progresso. Nesse
caso, Sol interrompe o worker, preserva o worktree e decide a causa antes de
retomar. Sol informa o usuário uma vez, com evidência concreta.

### 3. Aplicar

Luna atualiza primeiro a fonte canônica. Depois, ele atualiza somente as
superfícies afetadas. Se encontrar uma nova premissa, ele para e devolve o fato
ao Sol.

Luna valida a menor superfície após cada fatia coerente. Ele amplia os checks
uma vez no limite de integração. Um resultado verde permanece válido até uma
entrada relacionada mudar.

### 4. Revisar

O primeiro handoff permanece sem commit, salvo ordem explícita. Ele contém:

- resultado observável;
- `git diff --stat` e lista completa de arquivos;
- hunks que alteram contrato ou comportamento;
- checks executados e primeiro erro útil;
- riscos, pendências e mudanças remotas.

Sol inspeciona o diff de forma adversarial. Ele verifica composição,
previsibilidade, ergonomia, clareza, implementação, performance, segurança e
testes. Ele não relê arquivos inteiros nem repete checks verdes.

Sol envia uma lista consolidada ao mesmo Luna. Luna corrige e repete somente os
checks invalidados. Evite revisão cerimonial. Se duas rodadas não fecharem o
contrato, pare e informe o usuário.

### 5. Fechar

Luna faz a revisão final de manutenção. Ele remove duplicação, artefatos
temporários e complexidade acidental. Depois, ele executa os gates finais,
faz o commit e confirma `git status --short`.

Sol confirma o hash, o estado Git e as evidências. O resultado final contém
commit, arquivos, checks e riscos restantes. Não repita logs ou o histórico da
tarefa.

## Jornada contínua

Mantenha o Sol principal entre bundles relacionados. Use os artefatos canônicos
como memória durável. Não releia uma entrada que não mudou.

Reuse o mesmo Luna para draft, revisão, correção e commit do bundle corrente.
Feche o Luna após o commit. Use um Luna novo para um bundle independente. Essa
rotação limita contexto antigo sem perder a continuidade decisória do Sol.

Quando o usuário acrescentar informação, Sol incorpora o delta. Ele envia o
delta ao Luna somente quando a informação preserva o contrato ativo. Se o delta
mudar uma premissa, Sol interrompe o worker, decide novamente e só então retoma.

Uma pausa solicitada pelo usuário interrompe o worker e preserva o worktree. Uma
mensagem posterior para continuar, ou a retomada explícita do objetivo ativo,
encerra a pausa. Sol retoma o bundle sem pedir confirmação e sem narrar a pausa
anterior como bloqueio.

Se uma interrupção remover o contexto do Luna, Sol faz no máximo uma recuperação
com outro Luna do mesmo modelo e effort. O pacote de recuperação contém somente
o contrato fechado, o estado Git e o delta necessário. Se essa recuperação não
entregar checkpoint no limite acima, Sol interrompe o worker e devolve o estado
ao usuário. Ele não cria uma sequência de recovery workers.

Depois de uma compactação, retome o plano e o estado Git. Não repita descoberta
sem evidência de mudança no workspace.

## Design da linguagem

Sol responde por decisões amplas. Cada decisão contém:

- forma recomendada e semântica exata;
- alternativas preservadas e motivo da rejeição atual;
- contratos vizinhos e impacto de implementação;
- ergonomia humana e clareza para modelos;
- performance, segurança e capacidade de teste;
- condições que invalidam a recomendação.

Luna aplica o contrato normativo em `DESIGN.md` e registra a justificativa e a
evidência em `RATIONALE.md`. Depois, ele atualiza Última Luz, grammar, tooling e
projeções quando necessário. Uma aplicação coerente não ratifica uma
decisão incompleta. O contrato e o estado devem estar explícitos em
`DESIGN.md`; justificativa e evidência devem estar em `RATIONALE.md` antes do
commit.

## Leitura e saída econômicas

Use esta ordem:

1. `DESIGN-INDEX.md` para localização.
2. `tooling/design-slice.mjs` para seção ou W ID.
3. `reference/last-light/README.md` para localizar source.
4. `RATIONALE.md` para evidência, alternativas e ledger.
5. `history/HISTORY.md` somente para proveniência obsoleta.

Não leia `DESIGN.md` integralmente sem pedido de revisão integral. Não leia
`tooling/tree-sitter-w/src/` fora de uma falha de geração.

Use saídas curtas:

- inventário: paths e contagens;
- busca: somente correspondências relevantes;
- leitura: intervalo necessário;
- sucesso: código de saída e resumo;
- falha: primeiro erro e até 80 linhas úteis;
- diff: estatística, lista de arquivos e hunks de contrato;
- pesquisa: uma fonte primária por afirmação técnica.

Não narre buscas, leituras, checks ou waits rotineiros. Comunique somente escopo
inicial, decisão material, bloqueio, operação remota, checkpoint solicitado e
resultado. Não crie um plano visível quando uma lista interna curta for
suficiente.

Depois de editar, prefira o diff à releitura integral. Agrupe comandos de
leitura independentes somente quando todas as saídas forem necessárias. Não
execute um check verde novamente sem uma entrada relacionada alterada.

Não crie arquivo de status por padrão. Um arquivo temporário é permitido quando
uma ferramenta longa não produz eventos. Luna remove o arquivo antes do commit.

## Fontes

- [OpenAI — instruções com AGENTS.md](https://developers.openai.com/codex/concepts/customization#agents-guidance)
- [OpenAI — subagentes](https://learn.chatgpt.com/docs/agent-configuration/subagents)
- [OpenAI Codex — contorno de roteamento Sol para Luna](https://github.com/openai/codex/issues/31814)
- [OpenAI Codex — Luna está classificado como V1](https://github.com/openai/codex/issues/35097)
