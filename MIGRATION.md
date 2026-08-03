# Extração do repositório W

Este repositório foi extraído de `Wallacy/wallacy.com` para permitir que W
evolua com seu próprio ciclo de revisão e lançamento.

## Proveniência

- A árvore corrente foi criada com `git subtree split --prefix=W`.
- O histórico curado de `Y/W` foi importado em `history/`.
- A árvore completa de `Y` foi importada sem alterações em
  `history/legacy-y/`, incluindo `WIP.MD` e `_w_`.
- Os commits anteriores à extração permanecem no histórico filtrado.
- O monorepo mantém `W/` como submódulo apontado para este repositório.

O histórico preserva a origem dos artefatos. Ele não altera os contratos atuais
em `DESIGN.md`.

## Instruções de trabalho

`CONTRIBUTING.md` e `MAINTAINERS.md` definem o fluxo público para pessoas e
ferramentas. `AGENTS.md` e `.codex/` acrescentam somente instruções operacionais
para agentes. Eles não concedem autoridade nem substituem a governança.

Antes de publicar W, revise esses arquivos para remover paths, credenciais ou
contexto privado. Não remova instruções somente porque citam IA. A política do
projeto aceita contribuições humanas, assistidas por IA e automatizadas sob o
mesmo padrão verificável.

## Estado da migração

O nome remoto canônico é `git@github.com:Wallacy/W.git`. O primeiro branch
publicado é `main`. O parent repository aponta o submódulo para um commit
imutável deste branch; atualize o ponteiro do submódulo quando uma nova versão
for validada.
