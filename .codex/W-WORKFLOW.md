# Fluxo do Codex para W

> **Status:** orientação operacional para agentes, não parte da linguagem
> **Sessão principal recomendada:** **GPT-5.6 Sol · High**

`CONTRIBUTING.md` define o fluxo compartilhado. `MAINTAINERS.md` define revisão,
merge e manutenção. Este arquivo acrescenta somente roteamento de modelos,
delegação e controle de contexto.

Uma instrução de agente não concede autoridade. `GOVERNANCE.md` reserva merge,
release e segurança para pessoas responsáveis.

## Escolha de modelo

Use GPT-5.6 Sol em High para arquitetura integrada, memória, concorrência,
services, ABI e supply chain. Use o menor perfil que conclua a tarefa.

| Trabalho | Perfil inicial | Uso |
|---|---|---|
| arquitetura e integração difícil | Sol · High | sessão principal |
| revisão semântica excepcional | Sol · XHigh | pergunta estreita e critério de saída |
| implementação delimitada | Terra · Medium | tooling e mudanças com testes claros |
| inventário e leitura seletiva | Terra · Low ou Medium | síntese curta para a sessão principal |
| extração e transformação mecânica | Luna · Low ou Medium | resultado determinístico e verificável |

Não use Max, XHigh ou Ultra como padrão. Promova o esforço somente após erro
semântico, ambiguidade real ou falha de qualidade.

Fast mode reduz latência e aumenta consumo. Use-o somente quando a urgência
justificar esse custo. Não use Ultra para obter delegação automática.

### Endurecimento integral

Uma revisão integral pode usar Sol · Ultra quando o usuário priorizar qualidade
sobre custo. Nesse caso:

1. leia o ledger uma vez;
2. produza uma matriz única;
3. devolva um questionário único de ratificação;
4. não crie subagentes que releiam o mesmo contexto;
5. volte a Sol · High após a síntese.

## Delegação

Não delegue por padrão. Um subagente deve economizar mais contexto do que sua
coordenação consome.

Antes de delegar, defina:

```text
Objetivo: resultado observável.
Entradas: arquivos e decisões necessários.
Escrita autorizada: paths exclusivos.
Restrições: o que não pode mudar.
Concluído quando: teste ou inspeção verificável.
Retorno: síntese, arquivos, validação e bloqueios.
```

Não delegue quando duas tarefas alteram `DESIGN.md` ou dependem da mesma decisão.
Não peça logs brutos. Não permita escrita sobreposta.

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

- [OpenAI — GPT-5.6 model guidance](https://developers.openai.com/api/docs/guides/model-guidance?model=gpt-5.6)
- [OpenAI — Customization e AGENTS.md](https://developers.openai.com/codex/concepts/customization#agents-guidance)
- [OpenAI — modelos no Codex](https://learn.chatgpt.com/docs/models#recommended-models)
- [OpenAI — subagentes](https://learn.chatgpt.com/docs/agent-configuration/subagents)
- [OpenAI — Speed](https://learn.chatgpt.com/docs/agent-configuration/speed)
- [Artificial Analysis — Sol, Terra e Luna](https://artificialanalysis.ai/articles/gpt-5-6-intelligence-vs-cost-across-sol-terra-luna)

Reavalie o roteamento quando modelos, limites ou avaliações mudarem.
