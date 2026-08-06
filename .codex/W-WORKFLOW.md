# Fluxo do Codex para W

> **Status:** orientação operacional, não parte da linguagem
> **Executor principal:** **GPT-5.6 Luna · Max**
> **Arquiteto e revisor:** **GPT-5.6 Sol · High**

Este arquivo define roteamento de modelos e controle de contexto.
`CONTRIBUTING.md` define contribuição. `MAINTAINERS.md` define revisão e merge.
`GOVERNANCE.md` reserva autoridade humana. Uma instrução de agente não concede
autoridade.

## Contrato dos modelos

O Luna principal mantém a conversa e o contexto operacional. Ele localiza
fontes, coleta fatos, edita, valida, refatora e faz commits. Ele não decide uma
questão arquitetural aberta.

O `w_sol_architect` decide design e faz revisão adversarial. Ele usa Sol High
com acesso somente para leitura. Ele não edita, não valida mecanicamente, não
faz commit e não cria subagentes.

Use somente um Sol ativo. Não crie agentes paralelos. Mantenha pelo menos 95%
do consumo de tokens da tarefa no Luna. Essa porcentagem é uma meta de
comportamento, não uma métrica do Codex. Não gaste contexto para calculá-la.

## Quando usar Sol

Use Sol quando a tarefa exigir uma destas ações:

- decidir sintaxe, semântica, API ou invariantes;
- comparar alternativas com impacto de longo prazo;
- resolver uma contradição entre contratos;
- revisar uma mudança material antes do commit.

Não use Sol para uma destas ações:

- responder um status ou uma pergunta factual curta;
- localizar um W ID ou um arquivo;
- corrigir texto ou tooling sem mudar contrato;
- executar checks já definidos;
- aplicar uma decisão ratificada.

Se a classificação não estiver clara, Luna faz a descoberta mínima. Luna chama
Sol somente quando a descoberta revela uma decisão real.

## Fluxo de um bundle

### 1. Delimitar

Luna lê o pedido e as instruções uma vez. Ele começa por `DESIGN-INDEX.md` e
abre somente os slices necessários. Ele verifica `git status --short` antes de
editar.

Um bundle fecha uma decisão que pode ser revisada e revertida como unidade. Ele
pode incluir `DESIGN.md`, contratos dependentes, Última Luz, grammar, tooling,
projeções e checks. Não divida o bundle por arquivo.

### 2. Preparar a decisão

Quando o bundle exige design, Luna coleta somente os fatos necessários. Ele não
decide a solução. Depois, ele cria um Sol com contexto novo e este pacote:

```text
Papel: arquiteto W.
Objetivo: decisão observável de ponta a ponta.
Pergunta: escolha arquitetural que precisa de resposta.
Fatos: evidência já confirmada e slices canônicos.
Restrições: contratos, compatibilidade, escopo e autoridade.
Alternativas: candidatas conhecidas, sem decisão implícita.
Saída: recomendação, semântica, invariantes, riscos, rejeições e aceitação.
```

Use `fork_turns: "none"` quando a interface permitir. Selecione
`w_sol_architect`. Se o perfil não estiver disponível, use explicitamente
`gpt-5.6-sol` com effort `high` e inclua o contrato do perfil no pacote. O nome
da tarefa, sozinho, não seleciona modelo ou perfil.

Luna espera um evento longo. Ele não faz polling, não lê o contexto privado do
Sol e não executa trabalho paralelo. Uma atualização de status sem nova decisão
não justifica interromper a espera.

### 3. Aplicar

Luna aplica a decisão sem ampliá-la. Ele atualiza primeiro a fonte canônica.
Depois, ele atualiza somente as superfícies afetadas. Se a aplicação revelar
uma nova premissa, Luna para e devolve o fato ao mesmo Sol.

Luna valida a menor superfície após cada fatia coerente. Ele amplia os checks
uma vez no limite de integração. Um resultado verde permanece válido até uma
entrada relacionada mudar.

### 4. Revisar

Antes da revisão, Luna executa `git diff --check`. Depois, ele envia ao mesmo Sol:

- resultado observável;
- `git diff --stat` e lista completa de arquivos;
- hunks que alteram contrato ou comportamento;
- checks executados e primeiro erro útil;
- riscos, pendências e mudanças remotas.

Sol revisa composição, previsibilidade, ergonomia, clareza, implementação,
performance, segurança e testes. Ele retorna uma lista consolidada de achados
materiais. Luna corrige e repete somente os checks invalidados.

Evite uma revisão cerimonial. Se duas rodadas não fecharem o contrato, pare e
informe o usuário. Não esconda a falha com outro modelo.

### 5. Fechar

Luna faz a revisão final de manutenção. Ele remove duplicação, artefatos
temporários e complexidade acidental. Depois, ele executa os gates finais
afetados, faz o commit e confirma `git status --short`.

O handoff final contém resultado, commit, arquivos, checks e riscos restantes.
Não repita logs ou o histórico da tarefa.

## Jornada contínua

Mantenha o Luna principal entre bundles relacionados. Use os artefatos
canônicos como memória durável. Não releia uma entrada que não mudou.

Reuse o mesmo Sol para decisão, revisão e correções do bundle corrente. Feche o
Sol após o commit. Use um Sol novo para um bundle independente. Essa rotação
limita contexto antigo sem perder a continuidade operacional do Luna.

Quando o usuário acrescentar informação, Luna incorpora o delta. Ele envia o
delta ao Sol somente quando a informação muda a decisão ativa. Ele interrompe o
Sol somente se o objetivo for substituído ou a continuação gerar trabalho
inválido.

Depois de uma compactação, retome o plano e o estado Git. Não repita descoberta
sem evidência de mudança no workspace.

## Design da linguagem

Sol responde por decisões amplas. A decisão deve conter:

- forma recomendada e semântica exata;
- alternativas preservadas e motivo da rejeição atual;
- contratos vizinhos e impacto de implementação;
- ergonomia humana e clareza para modelos;
- performance, segurança e capacidade de teste;
- condições que invalidam a recomendação.

Luna aplica a decisão em `DESIGN.md`. Depois, ele atualiza Última Luz, grammar,
tooling e projeções quando necessário. Uma aplicação coerente não ratifica uma
decisão incompleta. O estado e a evidência devem estar explícitos em
`DESIGN.md` antes do commit.

## Leitura e saída econômicas

Use esta ordem:

1. `DESIGN-INDEX.md` para localização.
2. `tooling/design-slice.mjs` para seção ou W ID.
3. `reference/last-light/README.md` para localizar source.
4. `history/HISTORY.md` somente para proveniência.

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

Não crie arquivo de status por padrão. Um arquivo temporário é permitido quando
uma ferramenta longa não produz eventos. Luna remove o arquivo antes do commit.

Não narre buscas, leituras ou checks rotineiros. Comunique somente escopo
inicial, decisão material, bloqueio, operação remota e resultado. Não crie um
plano visível quando uma lista interna curta for suficiente.

Depois de editar, prefira o diff à releitura integral. Agrupe comandos de
leitura independentes somente quando todas as saídas forem necessárias. Não
execute um check verde novamente sem uma entrada relacionada alterada.

## Runtime do Codex

Em 6 de agosto de 2026, Multi-Agent V2 rejeitou Luna como filho de Sol. Uma
tarefa Luna Max conseguiu criar Sol High. Por isso, W usa Luna como tarefa
principal e Sol como filho de design.

Esta topologia é um contorno temporário. Reavalie quando
[OpenAI Codex issue 34909](https://github.com/openai/codex/issues/34909) for
resolvida. Retorne para Sol como pai somente quando uma tarefa real confirmar
Luna Max nos metadados do filho.

Depois de alterar `.codex/config.toml` ou `.codex/agents/`, inicie uma tarefa
nova. Uma tarefa existente pode manter a configuração anterior.

## Fontes

- [OpenAI — instruções com AGENTS.md](https://developers.openai.com/codex/concepts/customization#agents-guidance)
- [OpenAI — subagentes](https://learn.chatgpt.com/docs/agent-configuration/subagents)
- [OpenAI Codex — Luna rejeitado no spawn](https://github.com/openai/codex/issues/34909)
