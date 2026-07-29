# tree-sitter-w

> **Status:** Candidato — tooling inicial; não é ainda o parser normativo do
> compilador W.

Esta pasta contém a gramática estrutural executável que deve alimentar
highlighting, navegação, folds e futuras projeções para editores. A baseline e
as alternativas estão em [`W/DESIGN.md`](../../DESIGN.md).

Tree-sitter é a única gramática **sintática** mantida neste experimento. Uma
extensão VS Code pode derivar uma projeção TextMate para fallback lexical, mas
essa projeção não deve aceitar constructs que `grammar.js` não reconhece. Isso
não decide ainda se o frontend do compilador consumirá a mesma CST.

## Cobertura deste corte

- imports lógicos, aliases e exports;
- `fn`, vários `init`, overloads estruturais, labels, generics e `throws`;
- `fn(...)`, `some fn(...)`, `any fn(...)` e callable modes;
- static records, `StaticList<T>` e payloads `<[...]>`;
- `struct`, `object`, `service`, `enum`, `protocol`, aliases refinados e `foreign c`;
- stored fields, computed properties e property requirements;
- bindings, patterns nominais, `mut fn`/`take fn`, ownership e behaviors;
- `try`/`await`, `async let`, `spawn let` e `Task.cancel()`;
- units/sufixos candidatos, raw hash-delimited e testes co-localizados;
- `if`, `guard`, loops, `switch`, `do`/`catch`, `defer` e retornos;
- calls, members, tuples, coleções, literais e precedência candidata;
- queries de highlights, locals e folds;
- casos estruturais internos, incluindo a superfície integrada da DB2.

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
npm run parse:fixture
npm run check:design
```

Ou execute todos os checks:

```sh
npm run check
```

`npm run check` executa corpus, restaurante, fixture do VS Code e cobertura de
exemplos do design. O CLI está fixado em `tree-sitter-cli` 0.26.11. Após
`generate`, `src/` e o parser C gerado tornam a gramática consumível sem copiar
regras para outro lexer. Um binding Node nativo é responsabilidade do
consumidor. Ele não é necessário para gerar ou testar esta pasta.

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
- `service` é keyword candidata; `worker`, `assistant` e `nanoservice` não são;
- raw com múltiplos hashes balanceados ainda precisa de scanner externo; este
  corte reconhece a forma canônica de um hash;
- patterns e captures de closure estão no subset estrutural; escape, mode e drop
  dependem do type checker;
- parser normativo e compartilhamento de CST seguem gates de protótipo; esta
  gramática continua uma projeção de tooling.

Quando uma forma mudar, altere nesta ordem: decisão canônica, `grammar.js`,
corpus, queries e consumidores gerados. Não mantenha uma lista de sintaxe
independente dentro da extensão ou do portal.
