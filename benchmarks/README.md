# WBench/1 e desenvolvimento orientado por benchmark

`WBench/1` define o protocolo de desenvolvimento orientado por benchmark para
W. O protocolo separa workloads de linguagem, compiler lifecycle e
product-runtime. BMD1 executa somente o ponto source-backed ready do compiler
lifecycle. Ele não produz result de language ou de product-runtime.

O programa BMD1 fica em [`program.json`](program.json). O schema fica em
[`wbench-1.schema.json`](wbench-1.schema.json). O manifesto do seed fica em
[`seed-check-lifecycle.manifest.json`](seed-check-lifecycle.manifest.json).
Os descriptors [`seed-check-graph.json`](seed-check-graph.json) e
[`seed-check-input.json`](seed-check-input.json) são source-backed. O checker
valida os bytes e os digests antes de aceitar o manifesto.

## Perfis

Todo workload de linguagem usa exatamente três perfis:

- `learner` contém código correto e plausível de quem transfere patterns de
  outra linguagem e subutiliza W. O perfil não usa sleep, trabalho inútil,
  flags piores de propósito ou bypass.
- `idiomatic` é a forma recomendada para produção. Ele é a métrica primária e
  a base de regressão.
- `frontier` declara o teto de desempenho. O record declara unsafe, FFI,
  target specialization, manual layout, algoritmo e qualquer perda de
  legibilidade.

As lacunas `learner → idiomatic` medem performance cliffs. As lacunas
`idiomatic → frontier` medem specialization burden.

## Lanes

A lane `equivalent` exige o mesmo algoritmo, representação, validação,
numeric contract e input. A lane `open` permite um algoritmo melhor, mas o
resultado não mede a qualidade do compiler. O record deve declarar cada
diferença semântica ou física.

O default usa baselines independentes C/Clang e Rust quando razoável. Uma
exceção registra sua razão. O Computer Language Benchmarks Game é exploratório.
Ele nunca é authority de W.

O catálogo de language reserva 21 unidades de workload. Essa contagem pertence
à track de language. Ela não é a matriz de 27 células do compiler lifecycle.
Cada unidade usa os perfis e as lanes que o manifesto declarar. Uma unidade sem
backend, runtime ou provider permanece blocked.

## Compiler lifecycle

O seed usa a fixture source-backed
`reference/last-light/checker_bootstrap.w`, símbolo
`export fn canAcceptOrder(`, que o `w check` público valida sem imports de
`std`. O manifesto fixa os digests de source, graph e input. A matriz tem
27 células. Ela cruza os cenários `clean`, `no-op` e `edit` com os estágios
`check-end-to-end`, `source`, `lex`, `parse`, `semantic`, `hir`,
`lowering`, `codegen` e `link`. `startup` e `execution` pertencem a
product-runtime e não aparecem nessa matriz.

Somente `clean × check-end-to-end` está ready. No-op e edit são blocked por
`incremental-cache`. Os estágios source, lex, parse e semantic são blocked
por `stage-instrumentation`. HIR, lowering, codegen e link são blocked pelos
componentes homônimos. Não chame wall time externo de tempo de estágio interno.
O manifesto usa `languageProfiles.applicability: not-applicable` porque esta
track não compara os três profiles de source.
No compiler lifecycle, C/Clang e Rust são baselines contextuais e non-ranking.
A regressão primária futura usa W histórico com recipe equivalente.

O corpus mantém `benchmark_app.w` como matriz source-backed para futuros
workloads de composition do Restaurant. Essa matriz é blocked por
runtime/provider. Ela não cria três variantes artificiais do app e não é o
workload do runner BMD1.

## Runner BMD1

Use um output path explícito. Crie o parent e execute o runner. O CLI recusa
overwrite:

```text
mkdir benchmarks/results
bun tooling/benchmark-driven-development-runner.mjs --output benchmarks/results/seed-check.local.json
```

O default é exatamente 1 warmup e 9 samples raw. Overrides de `--warmup` e
`--samples` exigem warmup >= 1 e samples raw ímpares >= 9. O runner constrói
`compiler/seed-c` em Release em diretório temporário. Esse build fica fora da
medição. Depois ele executa o `w check` source-backed e exige exit 0 com
stdout/stderr vazios. Cada warmup e cada sample inicia processo novo e usa
monotonic wall clock em ns. O escopo inclui startup do processo e estado de
cache do filesystem e do OS.

O result atual é `exploratory`, `measurement-only` e `single-series`.
Comparison e regression estão bloqueadas por
`interleaved-comparison-runner`. O record valida antes da publicação e usa
provenance real de source, artifact, input, recipe, runner e toolchain. Os
controles de ruído conhecidos e desconhecidos ficam explícitos. A track language
e a track product-runtime continuam sem result.

Outputs são evidência local explícita. Não rastreie automaticamente os arquivos
gerados. O diretório `benchmarks/results/` é ignorado. O runner recusa target
existente e publica somente um JSON completo por operação atômica fail-if-exists.

## Metodologia externa

As referências abaixo são evidência metodológica sobre medição. Elas não são
autoridade semântica para W:

- [Computer Language Benchmarks Game — how programs are measured](https://benchmarksgame-team.pages.debian.net/benchmarksgame/how-programs-are-measured.html)
- [LLVM — Benchmarking](https://llvm.org/docs/Benchmarking.html)
- [Google Benchmark — User Guide](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
- [Google Benchmark — Random Interleaving](https://github.com/google/benchmark/blob/main/docs/random_interleaving.md)
- [rustc-perf — tests/perf](https://rustc-dev-guide.rust-lang.org/tests/perf.html)

Execute os checks focais com:

```text
bun run check:bmd
bun run check:bmd:smoke
```

O primeiro check valida protocolo, matriz, corpus, schema e runner host-side. O
smoke constrói o seed e executa uma medição real em diretório temporário. Os
checks não publicam resultados no repositório.
