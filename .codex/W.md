# Working guidance for the W repository

W is an experimental language repository. Do not present proposals as
implemented behavior. Do not start a broad compiler implementation without a
request.

Read [`CONTRIBUTING.md`](../CONTRIBUTING.md) for public contribution changes,
pull request preparation, or contribution policy. Read
[`MAINTAINERS.md`](../MAINTAINERS.md) for review or merge work.
[`GOVERNANCE.md`](../GOVERNANCE.md) defines human authority. Agent instructions
do not grant governance rights.

W has no backward-compatibility obligation before its first stable 1.0
release. Prefer correcting the design over preserving an obsolete pre-1.0
surface, artifact, or behavior. After 1.0, add a compatibility path only with
an explicit deprecation, replacement, removal milestone, and migration path.
Do not retain compatibility code without an exit condition.

## Canonical entry points

1. `README.md` is the short project entry point.
2. `CONTRIBUTING.md` is the shared contribution workflow.
3. `GOVERNANCE.md` and `MAINTAINERS.md` define authority and maintenance.
4. `DESIGN-INDEX.md` is the generated navigation and metrics projection.
   It never defines semantics.
5. `DESIGN.md` is the only source of truth for current language and system
   design, decisions, alternatives, status, and implementation order.
6. `reference/last-light/` is the official reference product and executable
   specification target. It does not override `DESIGN.md`.
7. `portal/` and `tooling/` project the current design for people and
   editors. They do not define semantics.
8. `history/` owns provenance. `history/archive/db1-2026-07-27/` contains the
   replaced 2026-07-27 consolidation and corpus. Do not treat archived files as
   current.

Use exactly these labels: **Direção**, **Forma vigente**, **Alternativa**,
**Pesquisa**, and **Rejeitado por enquanto**. A historical idea is not a
  decision. If current artifacts conflict, fix `DESIGN.md` first, then update
its projections.

## Read and write economically

- Follow [`.codex/WRITING.md`](WRITING.md).
- Read `DESIGN-INDEX.md` before any section of `DESIGN.md`.
- Use its line interval, then search headings and W IDs. Do not read the full
  design unless the task explicitly requires an integral review.
- Use `bun tooling/design-slice.mjs --heading N.N` or `--id W-NNN` when a
  bounded slice is enough. The command is read-only and never becomes a second
  authority.
- Read archived material only for a provenance question or a suspected omitted
  alternative.
- One concept has one canonical home. Link to `DESIGN.md` instead of copying
  its explanation into another document.
- Do not read `tooling/tree-sitter-w/src/` during language work. It is
  generated. Inspect it only for a parser-generation or distribution problem.
- Search the file table in `reference/last-light/README.md`. Open only the
  affected `.w` source and its local documentation range.
- Update the grammar, reference product, and tests when the visible surface
  changes. Keep the frozen portal noncanonical.
- Verify the smallest changed surface first. Run broader link, app, and diff
  checks once at the end.
- For every substantive task, follow the coordinator-worker protocol in
  `.codex/W-WORKFLOW.md`.

## Definition of done

A design change is done only when:

- its state and alternatives are explicit in `DESIGN.md`;
- the reference product and grammar agree with the selected form;
- local links resolve;
- `bun tooling/design-index.mjs --check` passes;
- generated or runtime artifacts are not left untracked;
- scoped tests and `git diff --check` pass.
