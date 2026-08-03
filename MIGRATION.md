# Extração do repositório W

Este repositório foi extraído de `Wallacy/wallacy.com` para permitir que W
evolua com seu próprio ciclo de revisão e lançamento.

## Proveniência

- A árvore corrente foi criada com `git subtree split --prefix=W`.
- O histórico de `Y/W` foi importado em `history/`.
- Os commits anteriores à extração permanecem no histórico filtrado.
- O monorepo mantém `W/` como submódulo apontado para este repositório.

O histórico preserva a origem dos artefatos. Ele não altera os contratos atuais
em `DESIGN.md`.

## Instruções de trabalho

As instruções de IA estão presentes enquanto o repositório permanecer privado.
Antes de publicar W, revise `AGENTS.md` e `.codex/` e remova o material que não
deve ser público. A remoção não deve apagar a proveniência de linguagem em
`history/`.

## Estado da migração

O nome remoto canônico é `git@github.com:Wallacy/W.git`. O primeiro branch
publicado é `main`. O parent repository aponta o submódulo para um commit
imutável deste branch; atualize o ponteiro do submódulo quando uma nova versão
for validada.
