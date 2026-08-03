# Fluxo de trabalho eficiente com Codex

> **Status:** orientação operacional do projeto, não parte da linguagem W
> **Escolha recomendada para a sessão principal:** **GPT-5.6 Sol · High**

Este documento existe para maximizar trabalho concluído por janela de execução.
O objetivo não é reduzir raciocínio necessário, mas impedir que descoberta,
logs, repetição e coordenação consumam o contexto que deveria guardar requisitos
e decisões.

## Escolha de modelo

Para a fase atual de W, use **GPT-5.6 Sol em High** na sessão principal enquanto
ela estiver integrando decisões de arquitetura, memória, concorrência e supply
chain. O default oficial do Codex é Sol em Medium; volte a ele quando a tarefa
principal tiver contrato e testes claros. High é uma promoção deliberada para
esta etapa de desenho, não o baseline permanente do repositório.

Use níveis ou modelos diferentes por papel:

| Trabalho | Modelo e esforço inicial | Motivo |
|---|---|---|
| Sessão principal de arquitetura, integração e implementação | Sol · High | Rastreia decisões e dependências difíceis sem pagar `xhigh` continuamente. |
| Revisão semântica excepcional, bug intratável ou decisão irreversível | Sol · XHigh | Uso pontual, com pergunta e critério de saída estreitos. |
| Implementação cotidiana, integração moderada e tooling | Terra · Medium | Workhorse pragmático quando não é necessária a profundidade de Sol. |
| Inventário, busca e leitura grande com julgamento moderado | Terra · Low/Medium | Varredura rápida que devolve síntese ao contexto principal. |
| Extração, classificação, links, hashes e transformações repetíveis | Luna · Low/Medium | Melhor rota quando a resposta correta é clara e verificável em volume. |

Não use `max` ou `xhigh` como padrão. A orientação atual é usar o menor esforço
que entregue o resultado: Medium equilibra velocidade e profundidade; High e
Extra High servem a trabalho difícil com várias etapas, fontes ou trade-offs;
Max fica para os problemas mais duros. W começa excepcionalmente em High na
sessão principal durante esta fase arquitetural, e rebaixa tarefas internas pela
tabela acima.

Use velocidade **Standard** como padrão. O Fast mode reduz espera, mas não o
trabalho lógico: na configuração atual ele acelera GPT-5.6 em cerca de 1,5× e
consome 2,5× os créditos de ChatGPT. Portanto ele serve a uma urgência pontual,
não a estender a janela disponível. Pelo mesmo motivo, não use Ultra apenas para
obter delegação automática; delegue explicitamente as poucas tarefas que
realmente são independentes.

Os benchmarks indicados pelo projeto são um *prior*, não uma política automática.
O Artificial Analysis ajuda a comparar inteligência, velocidade e custo, mas o
roteamento começa pelos papéis documentados do produto: Sol para trabalho
complexo e aberto, Terra para o cotidiano e Luna para tarefas claras e
repetíveis. Disponibilidade real, qualidade do artefato e custo por tarefa
**concluída** vencem qualquer ranking agregado.

### Exceção: endurecimento integral do design

Uma revisão única de todas as decisões arquiteturais pode usar **Sol · Ultra**
quando o usuário optar explicitamente por priorizar qualidade sobre custo e
latência. Nessa rodada, o modelo principal lê o ledger uma vez, produz uma matriz
exaustiva e devolve um único questionário de ratificação. Não habilite subagentes
automaticamente: Ultra e multi-agent são decisões independentes, e releituras do
mesmo contexto anulam o ganho pretendido. Depois da síntese/ratificação, volte a
Sol · High para integração e a Terra/Luna para tarefas delimitadas.

A orientação atual do GPT-5.6 recomenda começar no effort necessário e medir
antes de usar os níveis máximos; também trata multi-agent como capacidade
opcional para workstreams realmente independentes. Portanto Ultra não vira novo
default permanente do repositório.

### Regra prática de escalonamento

1. Comece no menor perfil da tabela que comporte a tarefa.
2. Defina um teste observável antes de executar.
3. Suba um nível apenas após falha de qualidade, ambiguidade real ou revisão que
   encontre erro semântico.
4. Não repita a tarefa inteira: entregue ao perfil superior o diff, o erro e as
   decisões relevantes.
5. Depois de três tarefas do mesmo tipo, compare sucesso, tempo e consumo e fixe
   o perfil mais barato que mantém a qualidade.

## Unidade de trabalho

Toda etapa longa deve caber neste contrato:

```text
Objetivo: resultado observável.
Entradas: arquivos e decisões canônicas estritamente necessários.
Escrita autorizada: caminhos exclusivos.
Restrições: o que não pode mudar.
Concluído quando: comando, teste ou inspeção verificável.
Retorno: síntese, arquivos alterados, validação e bloqueios — sem log bruto.
```

Sem essas seis linhas, não crie um subagente. Se duas tarefas escrevem o mesmo
arquivo ou uma depende do resultado semântico da outra, execute-as em sequência.
Paralelize sobretudo inventário, pesquisa, testes independentes e artefatos com
proprietários distintos. Subagentes consomem contexto e tokens próprios; quatro
agentes não são uma economia se todos relerem as mesmas 4.000 linhas.

### Protocolo de fechamento em lote

Quando o usuário pedir revisão de todas as pendências:

1. leia `DESIGN-INDEX.md` e depois a seção 29 de `DESIGN.md` uma vez;
2. consulte somente as seções canônicas diretamente ligadas aos bundles;
3. dê a cada ID uma recomendação, destino de status e bundle humano;
4. escolha uma baseline conservadora mesmo quando uma otimização exigir spike;
5. apresente um único formulário em que o usuário aceita defaults e lista
   exceções;
6. só depois da resposta promova decisões e sincronize exemplo, grammar e portal.

O artefato corrente desse fluxo é `DESIGN.md`. O registro W substitui ciclos
de uma pergunta por turno.

## Ciclo de execução

### 1. Localizar

- Leia o `AGENTS.md` mais próximo e o estado curto do Git.
- Entre por `README.md` e `DESIGN-INDEX.md`.
- Use o intervalo do índice. Depois use títulos, IDs e termos para reduzir a
  leitura.
- Prefira `node tooling/design-slice.mjs --heading N.N` ou `--id W-NNN` para
  um recorte limitado. O comando lê o arquivo canônico e não escreve uma
  projeção.
- Não abra `tooling/tree-sitter-w/src/` fora de uma falha de geração ou
  distribuição. Esses arquivos são gerados.
- Consulte `history/HISTORY.md`, `history/consolidation-manifest.md` e
  `history/historical-references.md` antes do histórico Git ou das fontes brutas.

Saída da etapa: uma lista curta de artefatos canônicos e lacunas. Não produza uma
segunda narrativa completa do projeto.

### 2. Decidir

- Separe fato existente, inferência e nova proposta.
- Dê à proposta um estado do vocabulário de `DESIGN.md`.
- Resolva uma decisão no documento canônico; os demais recebem links ou exemplos
  mínimos.
- Registre alternativas plausíveis na decisão W correspondente. Use `history/`
  para notas históricas ou spikes extensos.

Saída da etapa: decisão ou pergunta explícita, arquivos proprietários e teste de
aceitação.

### 3. Alterar

- Faça uma passagem coerente por arquivo; não reescreva o mesmo texto em ciclos.
- Preserve mudanças alheias e evite reformatação fora do escopo.
- Agrupe patches relacionados, mas mantenha decisões independentes revisáveis.
- Não rode build ou servidor até que as entradas correspondentes parem de mudar.

### 4. Verificar

Valide do barato para o caro:

1. parser/lint/check focado no arquivo;
2. links, exemplos ou teste do componente;
3. integração relevante;
4. `git diff --check`, diff resumido e uma única revisão final.

Uma falha deve ser reduzida ao comando, código de saída e vizinhança do primeiro
erro acionável. Preserve o log integral apenas fora do contexto principal e só o
abra por buscas direcionadas. Não repita um teste aprovado se nenhuma de suas
entradas mudou.

### 5. Registrar

- Atualize `DESIGN.md` somente quando o contrato, o estado, a alternativa ou
  o plano mudou.
- Informe resultado, arquivos, testes e risco restante em poucas linhas.
- Deixe a árvore sem servidores, logs, screenshots ou caches temporários.

## Orçamento de saída de comandos

Defaults operacionais, não limites absolutos:

- inventário: caminhos e contagens, não conteúdo;
- busca: até algumas dezenas de correspondências, refinando o padrão se exceder;
- leitura: título/intervalo necessário, não arquivo inteiro;
- build/test aprovado: código de saída e resumo;
- build/test falho: primeiro erro útil e até cerca de 80 linhas de contexto;
- diff durante trabalho: `--stat` e arquivos; diff completo apenas na revisão;
- pesquisa externa: uma fonte primária por afirmação técnica, expandindo somente
  quando houver conflito ou decisão de alto risco.

## Cache de contexto humano

Os arquivos abaixo substituem releituras:

- `DESIGN.md`: contrato, decisões, alternativas e plano;
- `DESIGN-INDEX.md`: intervalos e métricas geradas para leitura seletiva;
- `history/`: história, proveniência, referências antigas e spikes arquivados;
- `reference/last-light/`: produto de referência e alvo de especificação executável;
- este documento: operação e roteamento de modelos.

Se a informação já está neles, use o link e confirme apenas a seção relevante.
Se estiver ausente, acrescente-a uma vez ao arquivo correto para que a próxima
sessão não precise redescobri-la.

## Fontes da política

- [OpenAI — GPT-5.6 model guidance](https://developers.openai.com/api/docs/guides/model-guidance?model=gpt-5.6)
- [OpenAI — Customization e AGENTS.md](https://developers.openai.com/codex/concepts/customization#agents-guidance)
- [OpenAI — modelos recomendados no Codex](https://learn.chatgpt.com/docs/models#recommended-models)
- [OpenAI — subagentes e escolha de modelo](https://learn.chatgpt.com/docs/agent-configuration/subagents)
- [OpenAI — Speed no Codex](https://learn.chatgpt.com/docs/agent-configuration/speed)
- [Artificial Analysis — Sol, Terra e Luna: inteligência por custo](https://artificialanalysis.ai/articles/gpt-5-6-intelligence-vs-cost-across-sol-terra-luna)
- [Artificial Analysis — benchmarks GPT-5.6](https://artificialanalysis.ai/articles/gpt-5-6-has-landed)

Reavalie esta matriz quando os modelos, limites da superfície ou avaliações do
projeto mudarem. Não transforme um benchmark datado em regra eterna.
