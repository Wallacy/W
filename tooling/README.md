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
bun check
bun check --target compiler
bun check --target docs
bun check --target studies
bun check --target all
bun check --list
bun check --target quick --dry-run
bun demo --list
bun bootstrap --target host
bun dev run compiler/seed-c/fixtures/hlo0-hello.w
bun run check:suite-manifest
bun run check:bmd:executable
bun run check:study-registry
bun run study:registry
```

The short facade in [`dev-cli.mjs`](dev-cli.mjs) reads the small catalog in
[`dev-cli.json`](dev-cli.json). `bun check` selects `quick` by default;
`compiler`, `docs`, `studies`, and `all` point to the already ordered suites in
[`check-suites.json`](check-suites.json), without maintaining a second order.
`--list` only lists and `--dry-run` only expands the plan.

`bun demo` executes a named fixture with the public `w run` binary through the
current MLIR route; it does not call `demo:seed-hello` or HLO1 C. `bun bootstrap --target host`
uses the local development recipe, validates `w.exe` and
`receipt.json`, and never downloads a toolchain. In this first cut, bootstrap,
demo, and `dev run` are native Windows x64 operations; Linux is explicitly
unsupported. `dev run` requires an explicit existing regular `.w` path and may
accept one outside the checkout, while catalog, fixture, binary, and receipt
paths remain contained. Only arguments after `--` are forwarded.
Timers appear on stderr as non-benchmark DX metadata. The earlier
`check:quick`, `check:compiler`, `check:docs`, `check:studies`, and
`demo:seed-hello` aliases remain internal compatibility aliases during this
transition.

Use o runner para inspecionar uma suíte antes de executá-la:

```sh
bun tooling/check-suite.mjs --list
bun tooling/check-suite.mjs --dry-run --suite root-quick
bun tooling/check-suite.mjs --dry-run --suite root-compiler
```

`check:quick` validates manifests, the BMD/executable catalogs, projections,
documentation, and maintained parsing without heavy C builds.
`check:bmd:executable` is a separate Hello correctness smoke: it never runs W,
records no timing, and compiles C23/c2x and Rust when toolchains are available.
The executable catalog uses `windows-x64` as the shared platform class; GCC C
is recorded as `x86_64-w64-mingw32` and remains contextual/non-ranking across
ABI, while W and Rust use `x86_64-pc-windows-msvc`. Rust uses edition 2024.
Future measured records are exploratory, measurement-only, and not-evaluated;
the catalog's source/oracle readiness does not make W performance-ready.
Raw wall/RSS samples and artifact sizes are strictly positive; CPU counters may
be zero at their disclosed microsecond resolution, and arithmetic means use
integer-floor rounding. Result host identities are derived from normalized
redacted environment classes, never from hostnames, users, or paths. The
best-known contract is defined even while its empty index is not-established.
`check:compiler` executa uma vez os gates do compilador seed,
ACQ0, OWN0, MAN0, HIR0, HLO0, HLO1 e do `w run` público bounded. O RUN0
interno permanece um gate focal separado (`bun run check:run0`). Os leaves
`root/check:acquisition`, `root/check:owner-guard` e
`root/check:seed-manifest` aparecem uma vez em `root-compiler`; MAN0 fica
imediatamente depois de OWN0. O gate MAN0 atravessa o caminho OWN0 que consome,
mas não repete a suíte OWN0 inteira. `tree-check` e `root-check` recebem os
mesmos leaves por composição. `check:w-cli` continua depois deles como
regressão pública.
`check --target all` mantém a suíte integrada histórica. Use os targets
`docs` e `studies` para escopos menores; os aliases com dois-pontos acima são
compatibilidade interna.

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

After an intentional change to sources referenced by studies, run
`bun run study:refresh-digests`. The command updates only well-formed digests
for existing repository-contained paths. It installs each dependency wave as
one transaction and repeats until metadata dependencies reach a fixed point.
Missing paths, invalid JSON, duplicates, and cycles fail before each wave.

After changing normative text or classified evidence, run
`bun run design:refresh-evidence`. The command updates only mechanical
identities in the freeze classification: ledger text and hashes, file hashes,
and exact `DESIGN.md` section hashes. It fails if decision order or reviewed
classification fields would change.

`bun run hum0:refresh-evidence` applies the same rule to the HUM0 review
protocol. It updates only file hashes and derived stimuli. Symbols, windows,
tasks, prompts, and other human decisions remain unchanged.

`bun run capability:refresh-evidence` refreshes only local file hashes in the
CAP0 capability matrix and regenerates its result snapshot. Capability levels,
routes, gaps, source symbols, and study questions remain reviewed inputs.
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
seed, adapter D0, ACQ0, OWN0 e as fatias verificadas HIR0/HLO0/HLO1/RUN0. Consulte
[`compiler/seed-c/README.md`](../compiler/seed-c/README.md) para a superfície
local. Execute `bun run check:compiler` para os gates do bundle.

O caminho C23 `source → parser → frontend → HIR0 verificada → HLO0 → HLO1`
continua limitado aos subset e witnesses documentados. A rota nativa primária
W-1522 é independente: `source → parser/frontend → HIR0 verificada → MLIR0 →
mlir-opt → mlir-translate → llc → native host link`; HLO0, HLO1 e RUN0 são bootstrap,
auditoria e recovery, não pré-requisitos dessa rota. MLIR0 v3 aceita somente a
sequência NAT1 linear bounded como contrato histórico de W-1522. O adapter
MLIR0 v5 corrente também aceita interpolação signed-`i64` com helpers
internos de Display e texto counted; os HLO0/HLO1/RUN0 continuam single-print.
ACQ0 executa CHK6 em
storage caller-owned, com retry bounded e sem frontend, policy de filesystem ou
CLI. Execute `bun run check:acquisition` para compilar os cinco targets focais,
rodar o CTest ancorado e exigir duas saídas ACQ0 exatas. OWN0 observa e
reconfirma candidates `build.w` em uma sessão guarded sem selecionar owner ou
autorizar fallback; o gate executa Linux nativo e, em host Windows, exige WSL
Ubuntu. O adapter Windows permanece incondicionalmente fail-closed neste
bundle; seus probes são somente diagnósticos. Execute
`bun run check:owner-guard`. RUN0 consome o plano
HLO0 pelo verifier compartilhado em um gate interno, bounded e test-only.
Execute `bun run check:run0` para esse gate. O subset público W-1521 usa
`w run <explicit-path.w> [-- <args...>]` em Linux x86_64 com a rota nativa
explicitamente habilitada, ou o binário Linux por WSL Ubuntu no host Windows;
o basename explícito é uma source identity
opaca, não um identifier de módulo. Execute `bun run check:w-run` para o
produto; a extensão NAT1 é definida por W-1522.

The public Linux gate uses `llc` for a PIC object and an absolute host C
driver for `-pie` linking with native CRT/libc. It generates no C source
and does not require Clang. LLVM version checks remain separate from host
driver provenance. The older `check:mlir0` recipe is unchanged.
`bun run check:w-run --ci` requires Linux x64 and the separately acquired
23.1.0 toolchain. Missing prerequisites fail instead of SKIP. Local WSL gates
passed with LLVM 20.1.2 and 23.1.0, host GCC/cc 13.3.0, and Bun 1.3.4.
The mandatory Linux and Windows hosted jobs use Bun 1.4.0 and have not run.

ICMP0/W-1537 extends the comparison fixtures for six signed-`i64` operators.
The HIR9/MLIR12/Windows3 labels describe Bool-producing comparisons through
real `llvm.icmp` operations. Native0 stays v6. Six focused C23 suites passed.
The Windows LLVM 23.1.0 gate passed with MSVC C11 recovery and `/WX` intact.
The Linux/WSL LLVM 20.1.2 gate passed with the explicit GCC 13.3 host link driver.
Both verify exact admission output, signed boundaries, Bool composition, and type rejection.
ICMP0 did not rerun Linux LLVM 23 or the hosted jobs.
Existing syntax, ownership, nesting, stdout limits, and native
recipes remain unchanged. The gates make no timing or cross-target claim.

Isso não é frontend normativo
completo, typechecker, contexto público/geral de aquisição, manifest parsing,
owner selection, backend, linker, runtime ou o runner `w run` geral.

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
