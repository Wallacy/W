# Codex workflow for W

> **Status:** operational guidance, not part of the language
> **Manager:** principal model selected by the user
> **Default executor:** Luna Max

This file defines W-specific gates for long or delegated work. `AGENTS.md`
defines general agent policy. `CONTRIBUTING.md` defines contribution.
`MAINTAINERS.md` defines review and merge. `GOVERNANCE.md` reserves human
authority. Agent instructions grant no authority.

## Optional bundle handoff

Use a compact handoff when delegation helps. Include only the information the
executor needs:

```text
Role: Luna Max executor.
Objective: observable end-to-end result.
Facts: confirmed evidence that need not be rediscovered.
Contract: closed form, semantics, invariants, and alternatives.
Inputs: indexes, W IDs, slices, and starting files.
Writing: authorized paths and concepts.
Restrictions: contracts and surfaces that must not change.
Done when: acceptance and risk-appropriate checks pass.
Return: result, files, checks, risks, and open decisions.
```

Luna may investigate alternatives, diagnose bounded problems, and recommend a
form. The manager ratifies material product, architecture, syntax, semantic,
or API decisions. If execution reveals a material premise or contradiction,
return the fact to the manager and continue safe work that does not depend on
the decision.

## Task-routed W policy

Use `.codex/W.md` for the canonical artifact map, compatibility policy, design
reading order, generated-file exceptions, abstraction guidance, and definition
of done. Use `CONTRIBUTING.md`, `MAINTAINERS.md`, and `GOVERNANCE.md` for their
respective contribution, maintenance, and authority rules. Use
`.codex/WRITING.md` for controlled technical prose.

This file retains only W-specific cleanup and benchmark contracts below. Do not
copy source-of-record rules into this workflow.

## Workspace cleanup contract

Before a large build, measure free space on workspace and temporary volumes.
Repeat the measure at the next checkpoint and when closing the bundle.

Keep only one active build directory per bundle and reuse it. The checkpoint
reports the retained path and measured free space.

`bun run cleanup` performs a dry run for permitted workspace outputs. Use
`--apply` to confirm removal. Use `--keep <path>` to retain an active build.
The flag may occur more than once.

Legacy temporary cleanup requires `--temp --legacy-temp`. It accepts only
exact repository-created prefixes with their modeled `mkdtemp` suffix. Every
candidate must resolve under `os.tmpdir()` and be unchanged for at least
24 hours. Use `--temp-age-hours N` to increase this limit; values below 24 are
invalid. The default remains a dry run. Explicit `--apply` revalidates the
closed allowlist, physical tree, mount proof, fingerprint, and age before
removal.

The legacy calculation uses the newest `mtime` in the tree. The exact W prefix
and `mkdtemp` suffix are the ownership evidence currently modeled; unknown
names, nested paths, and escapes remain refused. A future change may use a
checkout namespace and a verifiable owner record.

Workspace apply requires local mount proof. Linux uses `/proc/self/mountinfo`.
Windows rejects junctions and reparse points during the scan. A platform
without this proof rejects apply.

Apply requires an editor and no concurrent mutation. The two revalidations
reduce the local window but do not remove the race before recursive `rm`.

No flow performs global automatic cleanup. The routine does not select
`node_modules`, source, history, sessions, data, logs, dumps, generated files,
or benchmark results.

## Benchmark-driven development

Every future behavioral bundle declares exactly one
`benchmarkDisposition`: `required`, `compiler-lifecycle`, `deferred`, or
`not-applicable`. `required` belongs to the language track and requires the
`learner`, `idiomatic`, and `frontier` profiles, with a correctness/oracle
record before any sample. It does not require timing before correctness or
before a backend exists. `compiler-lifecycle` uses its own track, one
source/graph/input identity, and separate phases. `deferred` requires an
explicit blocker, taskId, and stop condition. `not-applicable` requires a
reason and is reserved for documentation or digest-only changes. A disposition
is not a performance claim and does not turn a protocol oracle into a research
gate.

For a WBench/1 result, use `kind: result` and link the validation-oracle digest
to later samples. The record includes raw samples, warmup, stop rule,
randomized/interleaved order, environment, complete provenance, derived
metrics/summary, semantic deviations, and disclosures. The BMD checker rejects
undeclared claims without a backend, best-only output, precomputed output,
compiler recognition, hidden FFI, removed validation, numeric mode, or input
specialization.

## Sources

- [OpenAI — AGENTS.md instructions](https://learn.chatgpt.com/docs/agent-configuration/agents-md)
- [OpenAI — subagents](https://learn.chatgpt.com/docs/agent-configuration/subagents)
