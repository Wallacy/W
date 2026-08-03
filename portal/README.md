# Portal W

> **Status:** protótipo visual congelado. Não é compilador nem fonte normativa.

O portal registra uma direção inicial para o Livro, a referência, o status e o
playground lexical. Ele não precisa acompanhar cada mudança antes do design
freeze. [`DESIGN.md`](../DESIGN.md) continua sendo a única fonte de verdade.

Após o freeze, as páginas serão geradas de `DESIGN.md`, do registro W e dos
arquivos `.w`. Astro é uma alternativa de renderização, não uma escolha atual.
Consulte W-096 antes de retomar o desenvolvimento.

## Executar

Requer Bun 1.4 ou posterior:

```sh
bun run check
bun run start
```

Abra `http://127.0.0.1:3000`.

## Rotas principais

| Rota | Conteúdo |
|---|---|
| `/` | visão visual da linguagem |
| `/book` | percurso top-down pela baseline integrada |
| `/reference` | mapa do design e dos sources atuais |
| `/playground` | edição e análise lexical local |
| `/status` | maturidade das camadas |
| `/docs/DESIGN.md` | contrato integral canônico |
| `/reference/last-light/README.md` | ensaio e oracles |

Os seis arquivos `.w` do restaurante também possuem rotas diretas.

## Limites

- O botão **Analisar** não executa W.
- `w-syntax.js` é um scanner lexical temporário.
- Tree-sitter/WASM deve substituir esse scanner quando o build browser for
  reproduzível.
- Cores não validam tipos, ownership, effects ou semântica.
- O servidor usa somente arquivos locais e não possui dependências runtime
  externas.

## Verificação

`bun run check` compila os scripts do browser e o servidor em memória. A
verificação final também deve testar links, tema, navegação por teclado e os
snippets do design vigente.
