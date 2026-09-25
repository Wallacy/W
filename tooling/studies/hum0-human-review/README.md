# HUM0 — human and model review

HUM0 is a cross-cutting ergonomics protocol for problems exercised by Last
Light. It is not an R1 bundle: it has no `bundle.json`, creates no syntax
variants, and selects no normative form.

The protocol keeps exactly eight problem-first slices and four tasks per slice:
`explain`, `recall`, `repair`, and `change`. Each slice uses real
`reference/last-light` sources, independent host oracles with digests, one
primary input, one adversarial input with the same problem and outcome,
counterbalanced ordering, and blinding. A Last Light source is bound by a
repository-contained path and a symbolic selector that occurs exactly once.
Each stimulus is a bounded UTF-8 line-aligned window whose displayed bytes are
digest-checked independently; unrelated edits elsewhere in the source file do
not stale it. The mutation and expected repair remain observer-only. Internal
IDs, paths, digests, oracles, expected values, and implementation facts do not
enter participant-visible input. The renderer returns only `scenario`, `task`,
`instruction`, `source`, and `blindedLabel`. Deterministic facts may appear only
in `w explain`, through `explainableFacts`.

The snapshot measures protocol readiness only: eight slices, 32 tasks, and no
human or model records. It does not claim a score, preference, ergonomic win,
comprehension result, or W implementation. No participant or model was run.

Future record contracts distinguish:

- human records: SHA-256 `participantIdHash`, non-empty C/Rust/Python/W
  background, non-negative time/query counts, confidence from 1–5, and
  oracle-verified semantic outcomes, with no PII;
- model records: provider, model, version, tokenizer, closed JSON parameters,
  SHA-256 digests, consistent input/output/total token counts, and
  oracle-verified outcomes.

In the FFI slice, `BellLease` demonstrates optional registration and guarded
unsubscribe. Draining in-flight callbacks is an external oracle obligation, not
a guarantee inferred from the source.

Collection stops at the first expected-value echo, forged outcome, missing or
stale digest/selector, leaked internal identity, problem/outcome divergence,
duplicate record, or oracle disagreement. The case remains Research and
requires independent evidence before any normative revision.

Scoped checks:

```sh
bun test tooling/hum0-human-review-reference.test.mjs tooling/studies/hum0-human-review/oracle.test.mjs
bun tooling/check-hum0-human-review.mjs
```

The gate neither compiles nor executes W, collects no human/model data, and
never promotes design automatically.
