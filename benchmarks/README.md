# WBench/1 e desenvolvimento orientado por benchmark

`WBench/1` define o protocolo de desenvolvimento orientado por benchmark para
W. O protocolo separa workloads de linguagem, compiler lifecycle e
product-runtime. BMD1 executa somente o ponto source-backed ready do compiler
lifecycle como série única. BMD2 adiciona comparação source-backed entre dois
commits locais do mesmo seed. Nenhum bundle produz result de language ou de
product-runtime.

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

## Runner BMD1 e comparação BMD2

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

O result BMD1 é `exploratory`, `measurement-only` e `single-series`, com
`comparison: null`. Ele preserva a execução de um único seed.

Para BMD2, os dois refs devem ser SHAs completos de 40 hex e existir no
repositório local:

```text
bun tooling/benchmark-driven-development-runner.mjs --baseline <40-hex-sha> --candidate <40-hex-sha> --output benchmarks/results/seed-check-comparison.local.json
```

O runner extrai somente `compiler/seed-c` por `git archive` para diretórios
temporários próprios e faz builds Release independentes com CMake/Ninja fora
da medição. Não usa working tree suja, rede ou worktree Git. Os digests de
commit, closure, artifact, recipe, recipe-class e toolchain são registrados por
papel. Recipe-class, toolchain e workload divergentes falham antes de samples.
Os dois oracles exigem exit 0 com stdout/stderr vazios antes de warmup e raw.

Warmup usa pelo menos um par, com rounds próprios de `1..warmupPairCount` na
mesma orientação do primeiro round raw. Raw usa número ímpar fixo de pelo
menos nove pares. Cada round executa baseline e candidate uma vez. A ordem é
gerada pelo runner com `balanced-paired-interleaved-sha256-v1`, registrada com
seed, e a máquina recompõe e valida o schedule. O caller não escolhe seed. A
máquina recalcula as estatísticas, deltas candidate-baseline, ppm com sinal,
counts e calibration com `BigInt` e arredondamento explícito; ela valida também
o workload corrente e a consistência entre as identidades de papel duplicadas.
O runner deriva a proveniência de archive, build, artifact, recipe e toolchain e
executa os oracles. Um result isolado não permite à máquina recomputar essa
proveniência nem reexecutar o oracle.

O result BMD2 é `exploratory`, `comparison-only`, lane `equivalent`, cenário
`clean`, estágio `check-end-to-end` e `verdict: not-evaluated`. Ele não é claim
de performance. Regression continua bloqueada por
`managed-regression-runner`, que exige provider controlado, repetição,
uncertainty e threshold. O record valida antes da publicação e os controles de
ruído conhecidos e desconhecidos ficam explícitos.

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
bun run check:bmd:comparison-smoke
```

O primeiro check valida protocolo, matriz, corpus, schema e runner host-side. O
smoke BMD1 constrói o seed e executa uma medição real em diretório temporário.
O smoke de comparação faz HEAD×HEAD com dois builds independentes, um warmup
pair e nove raw pairs, e verifica apenas a estrutura do result sem gravá-lo.
Os checks não publicam resultados no repositório.
