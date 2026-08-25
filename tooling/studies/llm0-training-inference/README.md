# LLM0: training and inference readiness

Status: **Complete design study** (W-1475). LLM0 reached its finite stop
condition and its ownership map is now a current roadmap direction. It does
not add a keyword, framework, kernel, compiler feature, or provider.

The inventory starts from W contracts that already exist: tensor shape and
value parameters, explicit broadcast/reduction/numeric mode, f16/bf16 and
quantization direction, views and strides, `Device`/`Queue`/`Launch`, DLPack,
ownership, streams/backpressure/services, deterministic RNG/profile, and
packages/receipts. It then records training and inference gaps.

Each gap has one owner class: core language, std/API, typed IR/compiler,
runtime/provider, tooling/evidence, or application framework. The default
recommendation is to avoid inflating the core. The two structured workloads
are a distributed training step and an inference service request. Their oracle
checks contracts and missing evidence. It does not measure performance.

## Evidence boundary

The primary URLs and access date are in `study.json`. They provide research
input, not W implementation evidence. A capability row is not a provider
receipt. The workload oracle does not claim model quality, kernel availability,
throughput, latency, or framework compatibility.
