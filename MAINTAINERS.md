# Guia para maintainers

Este guia define o trabalho recorrente de manutenção.
[`GOVERNANCE.md`](GOVERNANCE.md) define quem possui autoridade.
[`CONTRIBUTING.md`](CONTRIBUTING.md) define o fluxo compartilhado com
contributors e ferramentas.

## Responsabilidade humana

Somente uma pessoa pode ser maintainer. Automação e IA podem localizar,
implementar, testar e revisar mudanças. O maintainer continua responsável pelo
merge, pela licença, pela segurança e pela conclusão declarada.

Não aprove uma mudança porque ela parece humana. Não rejeite uma mudança porque
ela usa IA. Exija o mesmo contrato e a mesma evidência.

## Ordem de revisão

Revise nesta ordem:

1. escopo e classe da mudança;
2. arquivo que possui a autoridade;
3. estado da decisão e alternativas;
4. semântica e efeitos observáveis;
5. erros, cleanup, cancelamento, FFI e targets;
6. exemplos, testes e oracles;
7. projeções e arquivos gerados;
8. licença, provenance e supply chain;
9. diff final e checks.

Pare a revisão quando um ponto anterior falhar. Isso reduz comentários sobre
código que ainda não possui contrato válido.

## Gates de merge

| Mudança | Gate |
|---|---|
| editorial | links locais, escrita controlada e `git diff --check` |
| `DESIGN.md` | exemplo por seção, índice regenerado e `check:docs` |
| `RATIONALE.md` | links locais, ledger/evidence checks e índice regenerado |
| grammar ou source `.w` | corpus, parse dos produtos e check integrado |
| tooling ou oracle | teste positivo, falha preparada e check integrado |
| CI ou dependência | versão imutável, menor permissão e execução verde |
| governança ou segurança | processo classe D e revisão de conflito |

Um check verde confirma somente o que o check mede. O maintainer deve declarar
o risco que continua sem prova.

## Revisão de design

Confirme estes pontos:

- o problema é observável;
- a camada escolhida é necessária;
- a forma comum permanece curta;
- custo e efeitos continuam visíveis;
- a baseline funciona sem a pesquisa opcional;
- a alternativa mais simples foi tratada;
- o produto Última Luz consegue demonstrar a decisão;
- o plano de implementação possui um gate verificável.

Não promova uma **Pesquisa** para **Forma vigente** somente por plausibilidade.
Quando o resultado puder mudar source, ABI ou runtime, exija um spike.

## Artefatos derivados

- `DESIGN-INDEX.md` sai de `tooling/design-index.mjs` e separa métricas e
  navegação de `DESIGN.md` e `RATIONALE.md`.
- `tooling/tree-sitter-w/src/` sai de `tree-sitter generate`.
- O portal permanece congelado até o design freeze.
- `history/` recebe provenance, não correções da decisão vigente.

Nunca corrija uma projeção para esconder conflito com sua fonte.

## Git e branch principal

`main` deve permanecer reproduzível e verde. Prefira commits pequenos com um
resultado observável. Não misture reorganização, mudança semântica e geração
em um commit sem necessidade.

O CI deve falhar quando um gerador altera arquivos rastreados. O check local
deve terminar sem diff gerado inesperado.

Antes do W 1.0, corrija o design em vez de preservar compatibilidade obsoleta.
Depois do 1.0, toda compatibilidade temporária precisa de substituição, janela
de depreciação, data de remoção e migração.

## Triagem

Classifique uma issue por área e resultado esperado:

- design de linguagem;
- compiler e bootstrap;
- memória e runtime;
- concorrência e services;
- packages e supply chain;
- std e targets;
- tooling e experiência;
- documentação e comunidade;
- segurança.

Feche duplicatas com um link para a decisão canônica. Preserve evidência nova.
Não use `wontfix` para esconder uma alternativa que continua tecnicamente
plausível.

## Manutenção periódica

Revise periodicamente:

- checks do branch principal;
- versões LTS e dependências do tooling;
- pins de GitHub Actions;
- permissões de maintainers e bots;
- canal privado de segurança;
- pesquisas que bloqueiam o design freeze;
- arquivos gerados e caches rastreados por engano;
- links e instruções de bootstrap.

Uma atualização de dependência deve ter diff do lock, fonte oficial e check
integrado. Não aceite uma atualização automática sem revisão do resultado.

## Antes de tornar o repositório público

1. Ative private vulnerability reporting.
2. Proteja `main` contra force push e remoção.
3. Exija o check `Validate` antes do merge.
4. Exija CODEOWNER review quando existir outro maintainer independente.
5. Crie os labels usados pelos templates e pelo Dependabot.
6. Verifique contato privado, recuperação de acesso e permissões de bots.
7. Defina política de nome, domínio e trademark.
8. Confirme que nenhum path, segredo ou contexto privado permanece no Git.

## Handoff

Um maintainer que pausa trabalho deve registrar:

1. decisão ou objetivo vigente;
2. arquivos proprietários;
3. último check aprovado;
4. risco ou bloqueio restante;
5. próxima ação verificável.

O handoff não deve incluir logs completos quando um erro curto basta.
