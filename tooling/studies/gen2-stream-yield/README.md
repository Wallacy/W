# GEN2 — bloco `stream { yield }`

GEN2 fecha a decisão de design sobre a forma estreita de produção de
`Stream`. O estudo compara a composição vigente com um bloco compiler-owned
que retorna `some Stream<Item, Failure>` e faz apenas
`stream <[take source, copy config, ref stable]> { ... yield (take|copy) value }`.

O problema é concreto: alguns producers lineares precisam repetir o mesmo
boilerplate de cursor, terminal e transferência de ownership. O estudo não
trata `yield` como uma segunda semântica de task, como callback push ou como
frame público. `Channel`, `buffer(capacity:)`, `TaskGroup` e `Stream<view T,E>`
continuam sendo as formas para diálogo, backpressure, trabalho paralelo e
views borrowed.

O corpus usa símbolos reais de `reference/last-light/` como âncoras e fixtures
pareados para a superfície corrente e a superfície candidata. A métrica é
human-first: conta decisões e obrigações visíveis (cursor, ownership, efeito,
terminal, cleanup, cancelamento e capacity), não escolhe por LOC.

Cada caso aceito é reduzido por duas máquinas independentes:
`switched-frame` e `returned-state`. Elas devem derivar o mesmo owner graph,
commit/happens-before, resultado tipado, cancelamento e cleanup/drop. O trace
físico e o packing podem divergir.

O fechamento atual do estudo é:

- promover o contrato estreito como forma normativa de producer pull;
- exigir `yield take value` ou `yield copy value`; `take` move e `copy` exige
  `Duplicable` e preserva o binding; bare `yield value` e copy de item não
  `Duplicable` são rejeitados;
- exigir uma capture list explícita (também `stream <[]>` quando vazia). Cada
  capture é avaliada, copiada, referenciada ou movida na construção, antes de
  o `Stream` ser retornado; `next` não escolhe uma capture ambiental;
- manter generator geral, frame/resume público, `send`/`throw`/`close`,
  `yield-from`, scheduler yield, push/unbounded, borrowed yield, `inout`,
  yield em `defer`, `next` concorrente/reentrante, `return value`, failure
  untyped, prefetch oculto e FFI resume fora do contrato;
- registrar separadamente a lacuna futura de compiler, runtime, provider, ABI,
  debug e estudos humano/modelo.

O corpus atual tem 20 casos (7 positivos e 13 negativos): cinco cenários
reduzem decisões visíveis e dois cenários de cancelamento aberto mostram o
custo adicional da capture explícita. O resultado e o cleanup continuam iguais
nas duas lowerings.

O estudo é evidência de design. Ele não compila ou executa W.

## Checks

```sh
bun test tooling/gen2-stream-yield-reference.test.mjs
bun tooling/check-gen2-stream-yield.mjs
bun run --cwd tooling/tree-sitter-w generate
bun run --cwd tooling/tree-sitter-w test
```

O parser gerado em `tooling/tree-sitter-w/src/` é uma projeção. Não o leia ou
edite manualmente.
