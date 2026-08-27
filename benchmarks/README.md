# WBench/1 e desenvolvimento orientado por benchmark

`WBench/1` define o protocolo de desenvolvimento orientado por benchmark para
W. O protocolo separa workloads de linguagem de lifecycle do compiler. Ele não
produz resultados de runtime enquanto o BMD0 runner não existir.

O programa BMD0 fica em [`program.json`](program.json). O schema fica em
[`wbench-1.schema.json`](wbench-1.schema.json). O manifesto inicial do seed fica
em [`seed-check-lifecycle.manifest.json`](seed-check-lifecycle.manifest.json).
Os descriptors [`seed-check-graph.json`](seed-check-graph.json) e
[`seed-check-input.json`](seed-check-input.json) são source-backed: o checker
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

## Compiler lifecycle

O seed usa a fixture source-backed
`reference/last-light/checker_bootstrap.w`, símbolo
`export fn canAcceptOrder(`, que o `w check` público valida sem imports de
`std`. O digest do source, do graph de uma fonte e do input invocation/edit fica
no manifesto e deve mudar quando os bytes mudarem. Cada fase usa o mesmo source,
graph e input identity. O corpus de workloads de linguagem pode usar
`benchmark_app.w` como source de design, mas o seed executável corrente não o
usa.

O ponto `edit` usa uma receita fixa de substituição de whitespace, sem alterar a
fixture canônica: o runner aplica a receita a uma cópia temporária, verifica uma
única ocorrência e confirma que o texto normalizado permanece igual.

As fases são `clean`, `no-op`, `edit`, `frontend`, `hir`, `lowering`, `codegen`,
`link`, `startup` e `execution`. O seed `w check` pode validar a entrada e o
frontend. Codegen, link, startup e execution permanecem pendentes ou blocked
até o BMD0 runner existir.

O lifecycle separa frontend, HIR, lowering, codegen, link, startup e execution.
Não cria três sources artificiais. Comparações com clang/rustc são contextuais
até haver equivalência de project shape e semantics. O frontend e `w check` estão
disponíveis; native backend e runtime não estão. Por isso HIR depende de `hir`,
lowering de `lowering`, codegen/link de `codegen`, startup de `runtime` e
execution de `runtime` e `provider`. O manifesto declara
`languageProfiles.applicability: not-applicable`; compiler lifecycle não usa os
três profiles de source.

Cada bundle comportamental registra exatamente uma disposição: `required` para
workload de linguagem, `compiler-lifecycle` para esta track, `deferred` com
blocker, task e stop condition, ou `not-applicable` com razão para documentação
ou digest-only. `required` não força timing antes de correção ou antes de o
BMD0 runner existir.

## Evidence boundary

O oracle de correção deve existir antes de qualquer amostra. Um record posterior
deve incluir raw samples, warmup, stop rule, ordem randomized/interleaved,
environment (hardware, kernel, toolchain, flags, target e provider) e
provenance separada (source, artifact, input, recipe, runner e toolchain
digests). O record identifica o
workload por manifest digest, track, lane, phase, baseline, variant e profile
quando aplicável. Registre latency, throughput, memory, allocations e artifact
size quando aplicável.

Registre semantic deviations e safety/specialization disclosures. O protocolo
rejeita best-only, output precomputed, compiler recognition, hidden FFI,
validation removida, numeric mode ou input specialization não declarados e
claims sem BMD0 runner.

Quando existir um record WBench/1, seu `kind` é `result`. Ele carrega o digest do
oracle de validação antes das amostras, raw samples, warmup, stop rule e ordem
randomized/interleaved. O record também exige hardware, kernel, toolchain, flags,
target e provider em `environment`, além de digests de source/artifact/input,
recipe/runner/toolchain em `provenance`; `metrics` e `summary` devem ser
derivados das amostras; semantic
deviations e disclosures ficam explícitos. BMD0 não cria nenhum result, timing
ou ranking enquanto native backend/runtime estiverem indisponíveis.

## Metodologia externa

As referências abaixo são evidência metodológica sobre medição. Elas não são
autoridade semântica para W:

- [Computer Language Benchmarks Game — how programs are measured](https://benchmarksgame-team.pages.debian.net/benchmarksgame/how-programs-are-measured.html)
- [LLVM — Benchmarking](https://llvm.org/docs/Benchmarking.html)
- [Google Benchmark — User Guide](https://github.com/google/benchmark/blob/main/docs/user_guide.md)

Execute o check focal com:

```text
bun run check:bmd0
```

O check valida o protocolo e os manifests host-side. Ele não executa W, não
mede tempo e não cria `results/`.
