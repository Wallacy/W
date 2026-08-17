# ASIC0 — implementation evidence gap closure

ASIC0 is a reuse-only design-oracle bundle for W-1355, W-1359, W-1420,
W-1425, and W-1435. The implementation gaps map to W-1448, W-1449, and
W-1450. It links one current route and one adversarial route to each original
decision.
It does not copy corpus payloads and it does not claim conformance.

The current routes close these design boundaries:

- W-1355 and W-1359 IPC adapters and providers use explicit receipts. Durable snapshots require
  data and selector receipts. Bounded mapped channels remain volatile.
- W-1420 and W-1425 availability facts and provider bindings use typed target and generation
  evidence. They do not add a keyword or runtime authority.
- W-1435 security profiles, side-channel budgets, patch attestation, and deployment
  receipts are evidence and admission contracts. They do not prove a sandbox.

Each implementation-evidence gap remains explicit. The existing IPC1, AVF0,
and SEC0 studies remain the authority for source cases, machines, snapshots,
official references, and missing evidence.

Run the scoped check from the repository root:

```text
bun test tooling/studies/asic0-evidence-gap-closure/oracle.test.mjs
bun tooling/check-asic0-evidence-gap-closure.mjs
bun run --cwd tooling/tree-sitter-w parse:asic0
```

The nested parser only parses the two thin W witnesses. It does not parse or
replace the reused study payloads.
