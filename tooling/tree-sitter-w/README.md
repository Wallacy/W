# tree-sitter-w

> **Status:** Candidato — tooling inicial; não é ainda o parser normativo do
> compilador W.

Esta pasta contém a gramática estrutural executável que deve alimentar
highlighting, navegação, folds e futuras projeções para editores. A baseline e
as alternativas estão em [`DESIGN.md`](../../DESIGN.md).

Tree-sitter é a única gramática **sintática** mantida neste experimento. Uma
extensão VS Code pode derivar uma projeção TextMate para fallback lexical, mas
essa projeção não deve aceitar constructs que `grammar.js` não reconhece. Isso
não decide ainda se o frontend do compilador consumirá a mesma CST.

## Cobertura deste corte

- imports de pacote, módulo, símbolo e wildcard, aliases e exports coletivos;
- `fn`, vários `init`, overloads estruturais, labels, generics e `throws`;
- `fn(...)`, `some fn(...)`, `any fn(...)` e callable modes;
- static records, `StaticList<T>` e payloads `<[...]>`;
- `struct`, `object`, `service`, `enum`, `protocol`, aliases refinados e `foreign c`;
- stored fields, computed properties e property requirements;
- bindings, patterns nominais, `mut fn`/`take fn`, ownership e behaviors;
- borrowed result types, optional bindings owned e iteration
  `ref`/`inout`/`copy`/`take`;
- array repeat `[value; count]` e literals de Array/Map;
- `try`/`await`, `for try await`, `async let`, `spawn let` e `Task.cancel()`;
- `Stream<view T, E>` e contracts direcionais `Channel<T><.send/.receive>`;
- units/sufixos candidatos, raw hash-delimited e testes co-localizados;
- `entry { ... }`, implicit entry bodies finais, descriptors nomeados, service declarations e `import service`;
- header contextual `script { ... }` como root standalone antes de module/imports;
- `hostBindings` data-only nos manifests, sem assignments de slots no source;
- manifests `package`, `workspace`, `lock` e `deployment` com values data-only;
- `if`, `guard`, loops, `switch`, `do`/`catch`, `defer` e retornos;
- calls, members, tuples, coleções, literais e precedência candidata;
- queries de highlights, locals e folds;
- casos estruturais internos, incluindo a superfície integrada vigente.

Na raiz de módulo, a projeção Tree-sitter mantém declarations e statements em
um repeat de top-level items para compartilhar estados e recovery. O checker
localiza o primeiro statement, forma o suffix `implicit_entry_body` e rejeita
declarations posteriores; essa fatoração não amplia a sintaxe normativa.

O corpus usa snippets autocontidos para que compiler, formatter, portal e
extensão possam futuramente reutilizar os mesmos fixtures. Os arquivos do
produto Última Luz também são um smoke test. Eles continuam source de design
até existir type checker e runtime.

## Executar

Requer Bun 1.3.14 ou uma versão compatível e instala somente dependências locais
desta pasta:

```sh
bun install
bun run generate
bun run test
bun run parse:reference
bun run parse:platforms
bun run parse:packages
bun run parse:deployments
bun run parse:std
bun run parse:fixture
bun run check:wire
bun run check:wire:c
bun run check:hir
bun run check:design
```

Ou execute todos os checks:

```sh
bun run check
```

`bun run check` executa corpus, produto, variantes de plataforma, deployments,
std, fixture do VS Code, oracles de wire e HIR e cobertura de exemplos. O CLI
está fixado em
`tree-sitter-cli` 0.26.11.

Após `generate`, `src/` e o parser C gerado tornam a gramática consumível sem
copiar regras para outro lexer. Um binding Node nativo é responsabilidade do
consumidor. Ele não é necessário para gerar ou testar esta pasta.

## Próximos consumidores

1. `wls` ou adapter de extensão usando a gramática para estrutura; semantic
   tokens dependentes de resolução vêm da HIR e refinam o fallback TextMate;
2. portal/playground usando o parser WASM quando esse artefato tiver build e
   teste reproduzíveis;
3. formatter e testes de diagnóstico consumindo o mesmo corpus.

O VS Code não consome Tree-sitter automaticamente, e não há adapter ou WASM
neste corte. Eles só devem entrar depois de
`tree-sitter generate`, corpus e smoke test do produto de referência passarem
no host.

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
