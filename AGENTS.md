# Repository guidance

W is an experimental language and compiler project. Do not present a proposal
as implemented behavior. Do not start a broad compiler implementation without
a request.

## Agent policy

The manager is the principal model selected by the user. The default executor
for substantive work is Luna Max. This split preserves the principal model's
available quota and accounts for rework, context, tools, and review. Do not
replace useful delegation with direct manager execution only because the
manager is more capable.

The manager interprets the objective, closes decisions, and defines scope,
risks, permissions, and completion evidence. The manager reviews critical
diffs and results and reports the real state. Luna reads and investigates in
detail, implements the package, validates the affected surface, fixes its own
issues, and returns a verifiable summary. Luna may reason technically within
the accepted scope. The manager ratifies material product, architecture,
syntax, semantic, or API decisions.

For substantive work, Luna performs detailed inventory and preparation when
delegation is useful. The manager inspects critical boundaries without
repeating the full investigation.

When the user pauses work, stop safely and preserve the worktree. An explicit
continuation or explicit resumption of the active objective ends the pause
without another confirmation or repeated discovery. Do not resume a paused
scope for a new task.

Small tasks, conversations, and bounded decisions may stay with the manager
when delegation costs more work than it saves. Do not create a subagent for
ritual.

The normal substantive workflow may use a pool of up to four Luna Max
executors. Use fewer executors when the work does not have independent bounded
packages. The tool's lower concurrency limit takes precedence. Do not create
conflicting writers only to fill the pool.

Keep one persistent Luna Max executor per related line of work. Reuse each
executor while its context remains valid. When changing an executor, send only
the objective, confirmed facts, scope, restrictions, acceptance, checks, and
pending decisions. Do not require percentage targets, irrelevant fields,
fixed review rounds, or a universal status cadence.

Set a status cadence that fits the package. At a status boundary or manager
request, the executor answers before more work. If the expected response is
absent, check liveness once. Do not poll or start a recovery loop. If context
is lost, use one compact recovery handoff to a confirmed Luna Max and report a
remaining blocker.

Use Luna Max according to the tool's actual selection. Do not invent model
identifiers, parameters, or metadata. Verify the model when the tool allows
it. Report unavailability, an unconfirmed selection, or a fallback. Do not
silently move a heavy package to the manager or claim that Luna performed work
without confirmation. A model mismatch is not a reason to discard useful
changes. Preserve the patch and assess execution policy separately from patch
quality.

Do not create recursive agents. Use parallelism only for independent work with
a clear benefit and no conflicting writes. Prefer completion events and
available notifications to status or transcript polling. A task must have an
observable result and a finite stop condition.

## Sources and scope

This file is the canonical source for general agent policy. Local instructions
may add only area-specific restrictions. They do not replace product contracts
or expand permissions.

Before a W task, read [`.codex/W.md`](.codex/W.md) and follow its routing. Read
only the additional policy needed:

- [`CONTRIBUTING.md`](CONTRIBUTING.md) for public contribution and pull requests;
- [`MAINTAINERS.md`](MAINTAINERS.md) for review, merge, and maintenance;
- [`GOVERNANCE.md`](GOVERNANCE.md) for authority and governance decisions;
- [`.codex/WRITING.md`](.codex/WRITING.md) for technical text;
- [`.codex/W-WORKFLOW.md`](.codex/W-WORKFLOW.md) for W-specific gates in long or
  delegated work, including the applicable cleanup and benchmark contracts.

`DESIGN.md`, `RATIONALE.md`, Last Light, grammar, tooling, security, ownership,
and other domain contracts remain in their canonical sources. Do not copy
their content into this policy. If sources conflict, fix the source of record
or state the real configuration limit.

Agent-instruction loading depends on the tool. A precedence statement here
cannot prevent another loaded instruction from applying. Inspect and fix the
source file when that conflict matters.

Use the repository instructions and available tools. Do not create or install
skills for this workflow.

## Execution and validation

Confirm the repository, branch, and `git status --short` before editing.
Preserve pre-existing changes. For design work, start with the index and read
only the required slices. Keep one canonical home for each concept and update
projections after the decision is stable.

Use Bun for repository tooling, scripts, and lockfiles. Do not create an npm
lockfile.

Do not read generated files without a task need. In
`tooling/tree-sitter-w/src/`, generated files remain out of normal language
work. `scanner.c` is authored and versioned. Follow the exceptions in
`.codex/W.md` and `MAINTAINERS.md`.

Run focused checks during implementation. Expand validation for affected
contracts, risk, existing requirements, or observed failures. Reuse a result
when the relevant input and environment have not changed. A green check proves
only what it measures.

Review correctness, security, ownership, complexity, and relevant cost. Do not
make cosmetic changes or measurements unrelated to the task. Visual, integration,
hardware, and production changes require their corresponding evidence.

## Authority and delivery

Simplified instructions do not grant access to credentials, production, data,
equipment, or publication. Keep local editing, commit, push, pull request, and
publication distinct. Perform each step only under existing authorization and
report only confirmed results.

Complete the authorized objective. Confirm that requested files exist, contain
the expected result, and remain in scope. Record checks as passed, failed,
blocked, or not run. Finish with `git diff --check` and a risk-proportionate
diff review.
