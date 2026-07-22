# W Portal

POC autocontida do portal oficial de W: uma visão visual da proposta, um livro
progressivo em português, mapa da referência canônica, status e playground de
especificação.

> **Working Draft:** W ainda não possui compilador, runtime ou stdlib
> implementados. Os exemplos são candidatos para prototipação; os rótulos de
> maturidade seguem o [ledger canônico](../STATUS.md).

## Executar

Requer [Bun](https://bun.sh/) 1.4 ou posterior. Não há dependências para
instalar.

```powershell
cd W\portal
bun run dev
```

Abra <http://127.0.0.1:3000>. Para executar sem watch:

```powershell
bun run start
```

`PORT` e `HOST` podem ser configurados por ambiente:

```powershell
$env:PORT = "4300"
$env:HOST = "127.0.0.1"
bun run start
```

## Áreas

| Rota | Conteúdo |
| --- | --- |
| `/` | Visão visual e referência explorável da direção atual |
| `/book` | Livro progressivo, com capítulos e links canônicos |
| `/reference` | Mapa dos documentos que possuem fatos e decisões |
| `/playground` | Editor com análise lexical local, sem execução |
| `/status` | Snapshot visual da maturidade do projeto |
| `/docs/DB1_REVIEW.md` | Questionário e matriz das 97 questões originais da Baseline de Design 1 |
| `/docs/DB1_ADDENDUM.md` | Rodada recuperada do WIP: execução, entries, tensores/ML e parâmetros compile-time |
| `/docs/ARCHITECTURE.md` | Fronteiras de longo prazo do sistema W |
| `/docs/research/long-term-program.md` | Trilhas, gates e ondas de pesquisa pós-v0 |
| `/docs/design/documentation-and-tests.md` | Contrato candidato de `///`, doctests, testes co-localizados e runner |
| `/examples/restaurant/README.md` | Índice do exemplo top-down “O restaurante W”; as dezesseis fontes `.w`, requisitos, ensaio DB1 e experimento multilíngue também possuem rotas explícitas |
| `/examples/restaurant/DB1_ASSAY.md` | Double-check integrado das decisões H01–H14 e ponte para o adendo aberto |
| `/corpus/README.md` | Contrato da Fase 0; manifest e schema também possuem rotas explícitas |
| `/health` | Health check JSON |

O tema claro/escuro é compartilhado entre páginas e persistido localmente. A
navegação funciona por teclado, os layouts são responsivos e o conteúdo central
continua legível sem JavaScript.

## Playground e contrato de compilação

O playground e os exemplos estáticos usam `w-syntax.js`: um scanner lexical
local que preserva offsets e reconhece comentários (`//`, `///`, `/* */`),
strings comuns/raw/multiline, números, delimitadores, operadores e as keywords
candidatas atuais.
Ele produz um realce seguro via nós DOM e `textContent`, sem `eval` ou HTML de
origem. É um fallback **não normativo** até que uma gramática única de
Tree-sitter/WASM possa atender o editor e o portal; ele não contém parser, type
checker ou compilador, e suas anotações não validam um programa W.

A API global mínima, carregada antes de `app.js` e `playground.js`, é exposta
como `window.WSyntax`: `scan(source)` retorna tokens com offsets e posição,
`appendHighlightedCode(element, source)` cria spans com `textContent`, e
`highlightAll()` cobre os blocos `.code-shell` já presentes no documento.

`POST /api/playground/compile` documenta o ponto de integração e sempre responde
`501 Not Implemented` em JSON:

```json
{
  "ok": false,
  "error": {
    "code": "compiler_unavailable",
    "message": "W ainda não possui compilador; nenhum código foi executado."
  }
}
```

Um adapter futuro poderá usar WASM ou um serviço remoto isolado. Nenhum está
configurado nesta POC e o portal não afirma que W rode no browser.

## Servidor e segurança

`server.ts` usa apenas `Bun.serve` e módulos nativos. Rotas de páginas, assets e
Markdown canônicos são enumeradas explicitamente; nenhum caminho arbitrário do
filesystem é aceito. `GET` e `HEAD` servem recursos estáticos, o endpoint do
playground aceita somente `POST`, e métodos incompatíveis retornam `405` com
`Allow`.

As respostas incluem CSP sem scripts inline, bloqueio de framing, `nosniff`,
política de permissões e política de referrer.

## Verificação rápida

```powershell
bun run check
bun run start
```

O check transpila client e servidor em memória; ele não grava bundles, caches ou
arquivos `NUL` na árvore.

Com o servidor rodando, confirme `GET`/`HEAD` nas páginas, `501` no contrato,
`404` para uma rota desconhecida e `405` para métodos incompatíveis.

## Arquivos

- `index.html`: landing/Visão preservada e rebatizada;
- `book.html` + `book.js`: livro, sidebar e progresso de leitura;
- `reference.html` e `status.html`: mapas estáticos;
- `w-syntax.js`: scanner lexical e realçador DOM local, fallback para a futura gramática Tree-sitter/WASM;
- `playground.html` + `playground.js`: editor, pré-visualização realçada e análise local;
- `../examples/restaurant/`: pseudocódigo narrativo servido por rotas explícitas;
- `../corpus/`: manifest e documentação do corpus servidos para inspeção;
- `styles.css` e `app.js`: identidade, acessibilidade, tema e interações comuns;
- `server.ts`: servidor Bun, headers de segurança e contrato `501`;
- `check.ts`: parse/build em memória, sem artefatos;
- `package.json`: scripts locais, sem dependências.
