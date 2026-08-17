# CAP0 capability matrix

CAP0 is a design-oracle input. It compares common problems in C, Rust, and
Python. It does not compare maturity, popularity, or feature names.

The canonical matrix is [`tooling/capability-matrix-cases.json`](../../capability-matrix-cases.json).
The checker derives each route from subcapabilities marked `scope=problem`;
extension and foreign-mechanism subcapabilities do not reclassify the common
problem. The snapshot is an oracle output. The host tests mutate routes,
coverage, source references, and documentation fields.

Each axis records:

- the same problem and a Last Light scenario;
- three short original pseudocode examples with primary references for C, Rust,
  and Python; snippets are teaching aids, not copied quotations;
- the first W composition attempt;
- stable component ids that subcapability coverage must reference;
- the exact residual gap or intentional tradeoff;
- one global simplification or generalization target;
- memory, effects, interfaces, concurrency, FFI, tooling, and Last Light risks;
- design-level capability dimensions;
- a queued documentation block with a unique `guides/problems/...` target for
  a future problem guide. The foreign side is a paired pseudocode example;
  the W side is a source-backed Last Light reference and does not duplicate W
  code in a snippet.

Research subcapabilities carry the exact `nextStudyGate` id. Design gates are
used for unresolved language/design questions; evidence gates record provider
or execution evidence and are not promoted to language gaps.

Current evidence is source-backed and host-derived. DYN0 keeps its composable
route and records the DYN1 host design-oracle in durable `nextStudyGate.studyRefs`;
W compiler, runtime,
provider, human-study, and model-study evidence remain missing unless a later
gate closes them.

HRD0 is a downstream problem-first study for the same DYN0 route. It treats
hot reload as a development-only tooling runner, links DYN1/SYN1 evidence, and
does not change CAP0's composable classification or add a language/profile
surface.

Run `bun tooling/check-capability-matrix.mjs --write` from the repository root.
Run `bun test tooling/capability-matrix-reference.test.mjs` for host mutations.
The current snapshot records 8 axes, 17 subcapabilities, 149 source refs, and
8 queued documentation targets. DYN1 remains partial evidence: its provider,
security, compiler, runtime, and standard-library gates stay open.
