# Contribuir com W

> **Joy for humans. Clarity for machines.**

W aceita contribuições humanas, assistidas por IA e produzidas por automação.
O projeto avalia o artefato, a evidência e a responsabilidade. A ferramenta usada
não concede preferência e não causa rejeição.

W ainda está na fase de design. O compiler, o runtime e o package manager não
existem. Não apresente uma proposta ou um source `.w` como comportamento
implementado.

Leia o [Código de Conduta](CODE_OF_CONDUCT.md) e a
[governança](GOVERNANCE.md) antes de participar.

## Responsabilidade pela contribuição

A pessoa que envia uma contribuição deve:

- compreender e revisar a mudança;
- ter o direito de publicar todo conteúdo enviado;
- identificar código, dados ou assets de terceiros;
- executar os checks aplicáveis;
- informar limites, riscos e evidência ausente;
- responder aos comentários de revisão.

O uso de IA não exige um rótulo. Informe a ferramenta quando seu uso afetar
licença, provenance, reprodução, segurança ou interpretação da evidência.
Uma resposta de modelo não é evidência. Um teste, uma fonte primária, um oracle
ou um experimento reproduzível pode ser evidência.

## Antes de alterar

Escolha a rota pelo tipo de mudança:

| Mudança | Rota |
|---|---|
| typo, link ou explicação local | pull request direto |
| bug no tooling ou no produto de referência | issue com reprodução e pull request |
| syntax, semântica, ABI, runtime, SDK ou package | proposta de design antes da implementação |
| governança, segurança ou confiança de release | discussão e revisão definidas em `GOVERNANCE.md` |
| vulnerabilidade | relato privado conforme `SECURITY.md` |

Pesquise o registro W e as issues antes de criar uma proposta. Uma forma
rejeitada pode exigir evidência nova, não uma nova descrição da mesma ideia.

## Fontes e autoridade

Use esta ordem:

1. `DESIGN.md` define os contratos normativos, o estado, pesquisas que mudam o
   contrato e a ordem de implementação.
2. `RATIONALE.md` guarda justificativas, evidência, alternativas e provenance;
   é complementar e não normativa.
3. `DESIGN-INDEX.md` localiza seções. Ele é gerado e não define semântica.
4. `reference/last-light/` demonstra o design e fornece oracles.
5. `tooling/tree-sitter-w/grammar.js` e o corpus projetam a syntax vigente.
6. `std/` projeta os contratos atuais da standard library.
7. `portal/` é um protótipo visual congelado.
8. `history/` preserva provenance obsoleta. Ele não decide o W atual.

Se dois artefatos divergem, corrija primeiro a fonte de maior autoridade.
Depois atualize as projeções afetadas.

## Propostas de design

Uma proposta deve informar:

1. o problema observável;
2. a camada correta para a solução;
3. o efeito no source, no tipo e no runtime;
4. o comportamento de erro, cancelamento, cleanup e FFI;
5. o comportamento em dois targets relevantes;
6. a alternativa mais simples;
7. o teste, o oracle e o critério de remoção.

Use os estados **Direção**, **Forma vigente**, **Alternativa**, **Pesquisa** e
**Rejeitado por enquanto**. Não crie sinônimos para esses estados.

Uma mudança de linguagem deve atualizar o ID W aplicável no ledger de
`RATIONALE.md`. Ela também deve
mostrar um exemplo real. Quando a forma for estrutural, atualize grammar,
corpus e produto de referência na mesma mudança ou explique a ordem planejada.

## Fluxo de edição

1. Leia `DESIGN-INDEX.md`.
2. Recorte somente a seção necessária com `tooling/design-slice.mjs`.
3. Verifique `git status --short`.
4. Altere uma fonte canônica para cada conceito.
5. Atualize as projeções depois que o contrato estiver estável.
6. Execute o menor check relevante.
7. Execute o check integrado antes do merge.
8. Revise o diff e execute `git diff --check`.

Não edite `DESIGN-INDEX.md` nem `tooling/tree-sitter-w/src/` manualmente.
Use o gerador proprietário de cada arquivo.

## Escrita e idiomas

Contribuições em português e inglês são aceitas. Preserve o idioma da seção
quando uma tradução completa não fizer parte da mudança.

Use `.codex/WRITING.md` para texto técnico. Use ASD-STE100 Issue 9 em inglês.
Use a adaptação controlada do repositório em português. Prefira frases curtas,
termos estáveis e exemplos verificáveis.

## Código e formatação

`.editorconfig` define encoding, newline e indentação comum.

Para source W, siga a forma canônica já definida:

- UTF-8 sem BOM;
- LF e um newline final;
- dois espaços, sem tabs;
- 120 colunas como limite preferido;
- uma linha quando a construção cabe;
- um item por linha e trailing comma quando a lista quebra;
- nenhum whitespace ou comment entre um head e seu `<...>`;
- header, imports e declarations comuns nesta ordem;
- source order preservada;
- semicolon somente quando sua remoção mudaria a statement partition.

O formatter W ainda não existe. Preserve manualmente essa forma até `w fmt`
substituir a verificação humana.

Para JavaScript, C, JSON, YAML, HTML e CSS, siga o arquivo vizinho. Não faça
reformatação fora do escopo.

## Ambiente e checks

Use Bun 1.4.0 ou uma versão compatível. Instale o tooling local uma vez:

```powershell
bun run tooling:install
```

Para documentação e decisões:

```powershell
bun run check:docs
```

Para grammar, corpus, std, tooling ou qualquer source `.w`:

```powershell
bun run check
```

Para atualizar o índice depois de alterar `DESIGN.md` ou `RATIONALE.md`:

```powershell
bun run design:index
bun run check:docs
```

O CI executa o check integrado. Um check aprovado não substitui revisão
semântica.

## Commits e pull requests

Mantenha cada commit revisável. Use uma mensagem curta em inglês técnico:

```text
design: clarify service admission
docs: add contribution policy
tooling: validate design index
ci: run the integrated check
```

Um pull request deve informar resultado, autoridade alterada, evidência,
riscos e checks executados. Não use volume de texto como substituto de uma
decisão clara.

## Licença

W usa a licença MIT. Ao enviar uma contribuição, você confirma que possui os
direitos necessários e aceita publicar a contribuição sob essa licença.

Uma contribuição pode ser recusada mesmo quando os checks passam. A decisão
considera coerência, custo permanente, segurança, escopo e evidência.
