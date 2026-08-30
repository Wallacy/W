# Mapa do repositório W

Este arquivo descreve a organização operacional do repositório. Ele não define
a semântica da linguagem. Para decisões normativas, leia
[`DESIGN.md`](DESIGN.md); para justificativas e proveniência, leia
[`RATIONALE.md`](RATIONALE.md).

## Autoridade

1. `DESIGN.md` — contratos correntes, estado, pesquisas que alteram o contrato
   e sequência de implementação;
2. `RATIONALE.md` — justificativas, alternativas, evidência e ledger;
3. `reference/last-light/` — produto de referência e fontes `.w`;
4. `tooling/tree-sitter-w/grammar.js` e corpus — projeção sintática;
5. `std/`, `portal/`, benchmarks e tooling — contratos, interfaces e evidência
   derivados.

`DESIGN-INDEX.md`, `DIAGNOSTICS.md`, `STUDIES.md` e outros índices são
projeções. Quando uma projeção divergir, corrija sua fonte e regenere a saída.

## Mapa de diretórios

| Diretório | Função |
|---|---|
| `compiler/seed-c/` | leitor de fonte, lexer, parser, formatter, frontend seed, HIR0/HLO0/HLO1 e CLI de bootstrap |
| `reference/last-light/` | produto de referência, fontes `.w`, contratos e fixtures do design |
| `reference/syntax-atlas/` | atlas de sintaxe e cobertura de parsing gerados |
| `std/` | rascunho dos módulos da biblioteca padrão |
| `tooling/` | máquinas, oracles, checkers, snapshots, índices e suítes |
| `tooling/tree-sitter-w/` | grammar.js, corpus, queries e CLI local do Tree-sitter |
| `benchmarks/` | catálogo e receitas de benchmark; resultados só existem quando explicitamente gerados |
| `portal/` | protótipo de editor/highlighting e seus contratos de fallback |
| `.github/` | fluxos de trabalho e configuração de automação do repositório |
| `.codex/` | instruções operacionais para trabalho assistido e revisão |

O diretório físico `history/` não faz mais parte do checkout. O Git é o arquivo
da proveniência removida: os commits imutáveis `4964d1f` (material bruto
anterior) e `25ef412` (último estado com `history/` no checkout) preservam a
narrativa para auditoria. Eles não são autoridade do W atual.

## Versionado, gerado e local

Fontes canônicas, testes, fixtures, manifests e snapshots necessários para
reprodução são versionados. Entre as projeções geradas e verificadas estão:

- `DESIGN-INDEX.md`, `DIAGNOSTICS.md` e `STUDIES.md`;
- `tooling/study-registry.json` e `reference/syntax-atlas/*`;
- snapshots e bundles declarados pelos seus manifests.

`tooling/tree-sitter-w/src/` é a saída de `tree-sitter generate`; não é código-fonte e
não deve ser editado manualmente. `src/scanner.c` é escrito manualmente e versionado.
Compilações locais, `node_modules/` e outras saídas temporárias ficam fora do
commit conforme `.gitignore`.

## Dependências e comandos

O pacote raiz declara Bun `1.4.0` como gerenciador de pacotes e versão mínima. O
pacote Tree-sitter declara somente `tree-sitter-cli` `0.26.13`. O seed C usa
CMake e um compilador C11 disponível no host; a matriz e os limites atuais
estão em [`compiler/seed-c/README.md`](compiler/seed-c/README.md).

Instale e gere as ferramentas com:

```sh
bun run tooling:install
```

Comandos públicos:

```sh
bun run check:quick
bun run check:compiler
bun run check:docs
bun run check:studies
bun run check
bun run check:suite-manifest
bun run check:study-registry
bun run study:registry
```

`check:quick` é a validação rápida de manifests, docs, projeções e parsing
mantido. `check:compiler` executa os gates do compilador seed uma vez. O
manifesto e a ordem podem ser inspecionados com:

```sh
bun tooling/check-suite.mjs --list
bun tooling/check-suite.mjs --dry-run --suite root-quick
```

## Regra de aliases

O `package.json` da raiz é a superfície pública para checks em todo o repositório. O
`tooling/tree-sitter-w/package.json` mantém somente `generate`, `test`,
`check:injections` e `parse:*` locais. Não adicione o mesmo check nas duas
superfícies. Um `parse:*` pode usar `--cwd tooling/tree-sitter-w` quando o CLI
precisar do diretório da grammar.

## Fluxo de manutenção

1. confirme `git status --short`;
2. escolha a fonte canônica e leia somente o trecho necessário;
3. atualize as projeções geradas pelo seu comando;
4. rode o menor gate relevante e depois a suíte focal da área;
5. revise links, `git diff --check` e o inventário final.

Não apresente um oracle ou uma fonte `.w` como comportamento implementado. O
frontend normativo completo, typechecker, backend, linker, runtime, provider e
gerenciador de pacotes continuam lacunas declaradas.
