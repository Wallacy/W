# Atlas sintático de W

O atlas mostra a superfície sintática atual de W para leitura humana. Ele é
separado do produto funcional Última Luz: o Atlas inventaria as formas
atômicas; [`../last-light/STORY.md`](../last-light/STORY.md) liga todas as
famílias a uma narrativa de aplicação. Os exemplos usam nomes de cidade e
território somente para tornar a leitura local.

## Como navegar

Comece por [`language.w`](language.w). Ele reúne roots de módulo, imports,
declarations, tipos, contratos, patterns, literals e entry explícito.

Depois leia [`execution.w`](execution.w). Ele reúne o módulo de execução com
entry explícito, bindings, allocator scopes, controle, execução, effects,
streams e channels.

Para a superfície fechada de operadores, leia [`operators.w`](operators.w).
O bloco demonstra precedência, bitwise, shifts, power, coalescing, `in`, `is`,
`as?`, `type of`, `info of`, ranges, `@`, compound assignment e APIs numéricas nomeadas, incluindo
`bitWidth`, counts, `reversedBits` e `reversedBytes`. Essas APIs são nomes
portáveis, não novos tokens de operador. O bloco é
parse-only e não prova type-check, runtime ou provider.
O parser mantém uma supergrammar para recuperação: o atlas aceita as formas
parentetizadas `(x |> f()) ?? fallback` e `(x ?? fallback) |> f()`, enquanto o
adversarial `x |> f() ?? fallback`/`x |> f() || fallback` é rejeitado
semanticamente por `W-PIPE-0001` porque o RHS deve ser um call template único.
For usage examples, read [`Operators and pipe-forward`](../../CHEATSHEET.md#operators-and-pipe-forward).
For explicit overflow and bit APIs, read [`Numeric policies and bit primitives`](../../CHEATSHEET.md#numeric-policies-and-bit-primitives).

The type-and-contract block also demonstrates the configured low-precision
heads `f8<.e4m3fn>`, `f8<.e5m2>`, and `f4<.e2m1fn>`, plus fixed and dynamic
`BigFloat<precision: ...>`. These reuse ordinary type arguments; the atlas
rejects bare `f4`/`f6`/`f8` because W-1651 defines no default encoding.

`>..` e `>..<` continuam formas current do contrato e das tabelas seed
lexer/parser. O witness direto Tree-sitter dessas formas está em um gap
conhecido do parser.
`a = b = c` também tem um gap de conformance: o seed Pratt e a grammar formam
uma árvore right-associative, mas DESIGN rejeita assignment encadeada. O atlas
e seu checker registram esses fatos sem convertê-los em claims semânticos.

[`build.w`](build.w) is the atlas's single manifest document. It contains two
direct `package` records and one local-only `build { schema: "w.build/1" }`
coordinator. Each package has an exact local `root`; package-authored
requirements, profiles, and recipes remain inside the independently
publishable package. The coordinator owns local resolution and deployments,
plus the exact default selector. There is no workspace record or workspace
identity. Multiple-package builds require an explicit package selector, and
`w build all` is only used when requested.

[`atlas-manifest.json`](atlas-manifest.json) inventaria cada bloco marcado,
cada família e cada regra pública nomeada da grammar. Ele registra design status,
status de validação, refs de design, refs da Última Luz e digests.
O manifest é a fonte de metadados: os markers nos arquivos `.w` somente ligam
um trecho legível ao ID. Ele também fecha o inventário de variantes aceitas;
uma variante marcada como `current` ainda pode ter somente evidência de parse.
Cada regra pública da grammar recebe uma classificação `direct`, `composed`,
`root`, `lexical` ou `recovery`; uma regra nova sem classificação falha o
checker.

The manifest also carries a small explicit `editorialCoverage` inventory for
the contract carriers. The projection check requires each listed carrier to
remain represented in `CHEATSHEET.md`; update that inventory together with the
accepted and rejected atlas witnesses when an approved contract spelling
changes.

O checker também exige que os fontes exercitem de fato toda regra pública
observável da grammar. `behavior_identifier` e `function_signature` são as
duas exceções explícitas: seus spellings existem, mas o CST os publica como
`identifier` e `function_declaration`. A lista de variantes cobre alternativas
atômicas dentro de uma regra, como as quatro formas de `entry`, imports,
strings ordinary/raw, ABI/foreign, borrows e pipe relativo. Combinações
cartesianas não são novas formas e não são duplicadas.

[`SYNTAX-COVERAGE.md`](SYNTAX-COVERAGE.md) é gerado dos blocos reais dos arquivos
`.w`. Ele é uma cobertura técnica parse-only, não um guia editorial e não uma
promessa de execução. Não edite o arquivo. Use o gerador para manter os
snippets idênticos.

O guia editorial fica na raiz em [`../../CHEATSHEET.md`](../../CHEATSHEET.md).
Ele explica contexto, rotas de uso e trade-offs. O guia editorial não é uma
projeção do atlas e não deve ser usado como fonte de sintaxe gerada.

## Estados e limites

`designStatus: current` identifica forma sintática vigente. Isso não prova
compiler, runtime ou provider.

`evidenceStatus: tree-sitter-parse-only` prova somente o parse Tree-sitter sem
recovery. `tree-sitter-parse-only-provider-missing` e
`tree-sitter-parse-only-compiler-runtime-missing` registram superfícies aceitas
que ainda não possuem a rota de implementação correspondente. Este campo não
é uma alegação de implementação.

`// atlas:value-tail` marks only a direct final expression inside a value block.
The tail rule is the same for single-expression and multi-statement blocks and
braced closure bodies; it never turns an ordinary function-body tail into an
implicit return. Atlas evidence remains parse-only, not type-check or execution proof.

Research, reserved, and rejected spellings stay in the companions
[`reserved.w-reserved.txt`](reserved.w-reserved.txt) and
[`rejected.w-rejected.txt`](rejected.w-rejected.txt). These files are not W
source, have explicit manifest status, and are not parsed by the checker. The
reserved companion has `designStatus: reserved`; the rejected companion has
`designStatus: rejected`. The arbitrary multi-hash raw-literal form is
explicitly rejected by W-218 in the rejected companion; it is not a Research
extension.

O atlas não altera a semântica normativa. Para contratos, use
[`DESIGN.md`](../../DESIGN.md) e os refs do manifest. Para evidência integrada,
use o mapa de [`reference/last-light/README.md`](../last-light/README.md).

## Verificação

Execute no root do repositório:

```text
bun tooling/syntax-atlas.mjs --check
```

O checker rejeita marker ausente, duplicado ou não listado, digest ou snippet
stale, regra pública não classificada, bloco inválido, root incompatível e
syntax coverage stale. Ele também parseia todos os `.w` do atlas sem recovery,
exige a ocorrência de cada regra observável e rejeita values soltos.

Para atualizar os artefatos depois de uma alteração aprovada:

```text
bun tooling/syntax-atlas.mjs --write
```

O atlas ajuda a preparar futuras tarefas HUM0 e documentação final. Ele não é
um tutorial completo, um formatter ou uma alegação de execução de W.
