# tree-sitter-w

> **Status:** Candidato — tooling inicial; não é ainda o parser normativo do
> compilador W.

Esta pasta contém a gramática estrutural executável que deve alimentar
highlighting, navegação, folds e futuras projeções para editores. A baseline é
[`W/spec/syntax.md`](../../spec/syntax.md); decisões e ambiguidades continuam em
[`W/STATUS.md`](../../STATUS.md), especialmente W-O007 e W-O008.

Tree-sitter é a única gramática **sintática** mantida neste experimento. Uma
extensão VS Code pode derivar uma projeção TextMate para fallback lexical, mas
essa projeção não deve aceitar constructs que `grammar.js` não reconhece. Isso
não decide ainda se o frontend do compilador consumirá a mesma CST.

## Cobertura deste corte

- imports lógicos, aliases e exports;
- `fn`, parâmetros rotulados, generics, efeitos `async throws` e blocos;
- `struct`, `object`, `enum`, `protocol`, aliases refinados e `foreign c`;
- bindings, ownership no call site, `try`/`await`, `async let` e `spawn let`;
- `if`, `guard`, loops, `switch`, `do`/`catch`, `defer` e retornos;
- calls, members, tuples, coleções, literais e precedência candidata;
- queries de highlights, locals e folds;
- nove casos estruturais internos e o
  [corpus compartilhado](../../corpus/README.md), com 12 positivos e 11
  negativos versionados.

O corpus usa snippets autocontidos para que compiler, formatter, portal e
extensão possam futuramente reutilizar os mesmos fixtures. Os arquivos do
restaurante são também um smoke test, mas continuam pseudocódigo pedagógico.

## Executar

Requer uma versão LTS ativa do Node.js e instala somente dependências locais
desta pasta:

```sh
npm install
npm run generate
npm test
npm run parse:restaurant
npm run corpus
```

Ou execute todos os checks:

```sh
npm run check
```

`npm run check` também executa o corpus compartilhado. O CLI está fixado em
`tree-sitter-cli` 0.26.11. Após `generate`, `src/` e o
parser C gerado tornam a gramática consumível sem copiar regras para outro
lexer. Um binding Node nativo é responsabilidade do consumidor e não é exigido
para gerar/testar esta pasta.

## Próximos consumidores

1. `wls` ou adapter de extensão usando a gramática para estrutura; semantic
   tokens dependentes de resolução vêm da HIR e refinam o fallback TextMate;
2. portal/playground usando o parser WASM quando esse artefato tiver build e
   teste reproduzíveis;
3. formatter e testes de diagnóstico consumindo o mesmo corpus.

O VS Code não consome Tree-sitter automaticamente, e não há adapter ou WASM
neste corte. Eles só devem entrar depois de
`tree-sitter generate`, corpus e smoke test do restaurante passarem no host.

## Lacunas deliberadas

- interpolação `${...}` é destacada como parte da string, não uma subárvore;
- comentários de bloco ainda não são aninhados;
- newline é trivia; a regra exata de separação/formatter ainda precisa de casos
  negativos, portanto `;` permanece aceito;
- `service`, `worker`, `assistant` e `nanoservice` não são keywords;
- patterns, captures de closure e annotations estão apenas no subset mínimo;
- a posição final de `async`, parser normativo e compartilhamento de CST seguem
  abertos no status.

Quando uma forma mudar, altere nesta ordem: decisão canônica, `grammar.js`,
corpus, queries e consumidores gerados. Não mantenha uma lista de sintaxe
independente dentro da extensão ou do portal.
