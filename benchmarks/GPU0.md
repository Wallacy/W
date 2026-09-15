# GPU0 device-linkage diagnostic

Experimental compiler/linkage evidence on NVIDIA RTX A400 (sm_86, driver 581.42). It is not source-backed W, a W runtime/provider, homogeneous toolchain support, or a product ranking.

| Observation | Value |
| --- | ---: |
| Correct result | 42 |
| H2D p50 / p95 | 2.9 us / 13.4 us |
| Dispatch + synchronize p50 / p95 | 6.7 us / 9.5 us |
| D2H p50 / p95 | 7.3 us / 17.7 us |
| End-to-end p50 / p95 | 19.5 us / 29.1 us |
| Host adapter | 14848 B |
| Host / device MLIR | 814 B / 428 B |
| PTX | 502 B |
| Protocol | 101 warmups, 1001 in-process samples |
| Toolchain | MLIR 23.1.1 + Clang 22.1.8 |

Reproduce correctness with `bun check --target gpu0` and refresh this snapshot with `bun benchmark gpu0`. The recipe compiles the C23 adapter with Clang `-O3 -flto=full /OPT:REF /OPT:ICF` and lowers the device artifact to `sm_86` PTX with `-O3`; all produced artifacts are temporary.
