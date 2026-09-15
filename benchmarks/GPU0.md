# GPU0 device-linkage diagnostic

Experimental compiler/linkage evidence on NVIDIA RTX A400 (sm_86, driver 581.42). Its device request is derived from the W fixture through ACCREQ0; it is not yet a complete W executable, a W runtime/provider, homogeneous toolchain support, or a product ranking.

| Observation | Value |
| --- | ---: |
| Correct result | 42 |
| H2D p50 / p95 | 2.9 us / 13.9 us |
| Dispatch + synchronize p50 / p95 | 7.0 us / 25.0 us |
| D2H p50 / p95 | 7.6 us / 17.7 us |
| End-to-end p50 / p95 | 17.9 us / 35.5 us |
| Host adapter | 14848 B |
| Device MLIR | 423 B |
| PTX | 502 B |
| Protocol | 101 warmups, 1001 in-process samples |
| Toolchain | MLIR 23.1.1 + Clang 22.1.8 |

Reproduce correctness with `bun check --target gpu0` and refresh this snapshot with `bun benchmark gpu0`. The recipe compiles the C23 adapter with Clang `-O3 -flto=full /OPT:REF /OPT:ICF` and lowers the device artifact to `sm_86` PTX with `-O3`; all produced artifacts are temporary.
