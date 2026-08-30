# Tooling do W

> **Status:** Working Draft. Este diretório contém a infraestrutura local e as
> projeções do W. Ele não define a semântica da linguagem.

O contrato normativo está em [`DESIGN.md`](../DESIGN.md). Use
[`DESIGN-INDEX.md`](../DESIGN-INDEX.md) para localizar uma seção e
[`RATIONALE.md`](../RATIONALE.md) para justificativas, evidência e proveniência.
O tooling valida essas fontes, gera índices humanos e oferece oracles de
design. Um oracle não é compiler, runtime, provider ou resultado de usuário.

## Comandos públicos

Os scripts públicos ficam somente no `package.json` da raiz. O manifesto
declarativo em [`check-suites.json`](check-suites.json) mantém a ordem e os
limites das suítes.

```sh
bun run tooling:install
bun run check:quick
bun run check:compiler
bun run check:docs
bun run check:studies
bun run check
bun run check:suite-manifest
bun run check:study-registry
bun run study:registry
```

Use o runner para inspecionar uma suíte antes de executá-la:

```sh
bun tooling/check-suite.mjs --list
bun tooling/check-suite.mjs --dry-run --suite root-quick
bun tooling/check-suite.mjs --dry-run --suite root-compiler
```

`check:quick` valida manifests, projeções, documentação e parsing mantido sem
builds C pesados. `check:compiler` executa uma vez os gates do compilador seed,
HIR0, HLO0, HLO1 e da CLI `w`. `check` mantém a suíte integrada histórica; use
`check:docs` e `check:studies` para escopos menores.

Não crie um alias equivalente em `tooling/tree-sitter-w/package.json`. O pacote
Tree-sitter mantém apenas comandos locais da gramática:

```sh
bun run --cwd tooling/tree-sitter-w generate
bun run --cwd tooling/tree-sitter-w test
bun run --cwd tooling/tree-sitter-w parse:reference
bun run --cwd tooling/tree-sitter-w parse:std
```

Os aliases repo-wide ficam na raiz. Um comando `parse:*` pode permanecer local
quando o CLI precisa do diretório da gramática.

## Projeções humanas e de máquina

O registro de estudos tem duas superfícies sincronizadas:

- [`STUDIES.md`](../STUDIES.md) é o catálogo humano, agrupado por status, com
  ID, função, path, gate e entrypoint principal;
- [`study-registry.json`](study-registry.json) é o índice de máquina com
  metadata, fixtures, referências, digests, dependências e scripts.

Ambos são gerados por `bun run study:registry`. O writer prepara as duas
saídas e tenta instalá-las transacionalmente, com rollback diante de erros
comuns do sistema de arquivos; isso não promete atomicidade entre arquivos
depois de crash ou perda de energia. `bun run check:study-registry` rejeita
JSON ou Markdown stale. Detalhes de cada estudo ficam no `README.md` local
quando existir e no catálogo gerado. Não mantenha uma segunda tabela manual
neste arquivo.

Outras projeções correntes são:

| Projeção | Gerador ou gate | Função |
|---|---|---|
| [`DIAGNOSTICS.md`](../DIAGNOSTICS.md) | `bun tooling/diagnostic-catalog.mjs --write` | busca humana por código, fatos, fixes e referência normativa |
| [`DESIGN-INDEX.md`](../DESIGN-INDEX.md) | `bun tooling/design-index.mjs --write` | navegação por heading, ID e seção |
| `reference/syntax-atlas/` | `bun tooling/syntax-atlas.mjs --write` | cobertura de snippets e parsing parse-only |
| `tooling/*-snapshot.*` | checker de cada unidade | bytes e resultados derivados reproduzíveis |
| `tooling/*-cases.json` | máquina/oracle da unidade | casos de design e barreiras adversariais |

Regenerar uma projeção não transforma sua saída em autoridade. Se uma
projeção divergir, corrija a fonte canônica e depois regenere os derivados.

## Tree-sitter e editores

[`tree-sitter-w/grammar.js`](tree-sitter-w/grammar.js) é a única gramática
sintática mantida neste corte. O corpus em `tree-sitter-w/test/corpus/` e as
queries em `tree-sitter-w/queries/` alimentam parsing incremental, highlights,
locals e folds.

`tree-sitter-w/src/` contém outputs gerados pelo comando `generate`; não edite
essa pasta manualmente. O scanner `src/scanner.c` é authored e versionado.
TextMate e o portal são fallbacks/projeções editoriais; não aceitam um programa
em nome da linguagem.

Para testar a grammar diretamente:

```sh
bun run tooling:install
bun run --cwd tooling/tree-sitter-w test
bun run --cwd tooling/tree-sitter-w parse:reference
bun run --cwd tooling/tree-sitter-w parse:std
```

Os checks de integração permanecem na raiz, por exemplo:
`bun run check:syntax-atlas`, `bun run check:maintained-parse`,
`bun run check:cheatsheet` e `bun run check:links`.

## Compiler seed

O seed C é uma implementação caller-owned e incremental de validação. Ele
contém source reader, lexer lossless, scanner C, parser, formatter, frontend
seed, adapter D0 e as fatias verificadas HIR0/HLO0/HLO1. Consulte
[`compiler/seed-c/README.md`](../compiler/seed-c/README.md) para a superfície
local. Execute `bun run check:compiler` para os gates do bundle.

O caminho source → parser → frontend → HIR0 verificada → HLO0 → HLO1 → C11
continua limitado aos subset e witnesses documentados. Isso não é ainda o
frontend normativo completo, typechecker, backend, linker, runtime ou `w run`.

## Estudos e oracles

Leia [`STUDIES.md`](../STUDIES.md) para o inventário dos 71 estudos, seus
status e entrypoints. Cada diretório em `studies/` pode conter corpus, máquina,
oracle, snapshot e documentação local. `bun run check:studies` executa a suíte
agregada; um estudo também pode ter um alias focal na raiz.

Os estudos preservam a separação entre:

1. contrato ou hipótese em `DESIGN.md`/`RATIONALE.md`;
2. casos e máquinas reproduzíveis;
3. evidência de parser, host oracle ou implementação limitada;
4. lacunas explícitas de compiler, runtime, provider, humano ou modelo.

Parsing Tree-sitter, testes Bun, snapshots e gates host não provam execução de
W. Não use um status de estudo como uma promessa de produto.

## Manutenção

Antes de uma mudança, leia `.codex/W.md`, `.codex/W-WORKFLOW.md` e
`.codex/WRITING.md`. Preserve uma única fonte por conceito:

1. altere a fonte canônica;
2. regenere somente as projeções afetadas;
3. execute o menor gate relevante;
4. execute `bun run check:quick` ou `bun run check:compiler` conforme a área;
5. termine com `bun run check:links` e `git diff --check`.

Não edite projeções geradas manualmente, não copie o catálogo de estudos para
outro README e não mantenha aliases duplicados entre raiz e Tree-sitter.
