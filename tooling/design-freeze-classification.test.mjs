import { expect, setDefaultTimeout, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

const rootDirectory = path.resolve(import.meta.dir, "..");
const checkerPath = path.join(rootDirectory, "tooling", "check-design-freeze-audit.mjs");
const sourcePath = path.join(rootDirectory, "tooling", "design-freeze-classification.json");

setDefaultTimeout(120000);

function runMutation(mutator) {
  const temporaryDirectory = fs.mkdtempSync(path.join(os.tmpdir(), "w-freeze-classification-"));
  try {
    const value = JSON.parse(fs.readFileSync(sourcePath, "utf8"));
    mutator(value);
    const mutatedPath = path.join(temporaryDirectory, "classification.json");
    fs.writeFileSync(mutatedPath, `${JSON.stringify(value)}\n`);
    return Bun.spawnSync([process.execPath, checkerPath, "--require-complete"], {
      cwd: rootDirectory,
      env: { ...process.env, W_DESIGN_FREEZE_CLASSIFICATION: mutatedPath },
      stdout: "pipe",
      stderr: "pipe",
    });
  } finally {
    fs.rmSync(temporaryDirectory, { recursive: true, force: true });
  }
}

test("rejects a missing ledger entry", () => {
  const result = runMutation((value) => value.entries.pop());
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("classification is missing");
});

test("rejects a stale authority digest", () => {
  const result = runMutation((value) => {
    value.entries[0].authorityRef.sha256 = "sha256:stale";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("sha256 is stale");
});

test("rejects a stale DESIGN section digest", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.decisionId === "W-1418");
    entry.authorityRef.sectionDigest = "sha256:stale";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("sectionDigest is stale");
});

test("rejects a DESIGN gate that does not match its authority", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.decisionId === "W-1418");
    entry.gap.gate = "DESIGN.md §13 / effect and workflow gates";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("gap.gate must equal its design-contract authority");
});

test("rejects a self-referencing superseding decision", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.category === "superseded");
    entry.authorityRef.decisionId = entry.decisionId;
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("self-references");
});

test("rejects a swapped supersession target claim", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.decisionId === "W-281");
    entry.supersessionClaim.decisionId = "W-1160";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("supersessionClaim.decisionId must equal authorityRef.decisionId");
});

test("rejects expected echo in evidence", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.evidence.length > 0);
    entry.evidence[0].expected = "caller-owned";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("must not echo expected");
});

test("rejects a history authority", () => {
  const result = runMutation((value) => {
    value.entries[0].authorityRef.path = "history/archive/pyn1-workflow/README.md";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("must not use history");
});

test("rejects mass default selection", () => {
  const result = runMutation((value) => {
    for (const entry of value.entries) entry.selection = "default";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("selection must be explicit-ledger-id");
});

test("rejects swapped canonical claims", () => {
  const result = runMutation((value) => {
    const firstClaim = value.entries[0].canonicalClaim;
    value.entries[0].canonicalClaim = value.entries[1].canonicalClaim;
    value.entries[1].canonicalClaim = firstClaim;
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("canonicalClaim does not match");
});

test("rejects a ledger addition", () => {
  const result = runMutation((value) => {
    value.entries[0].decisionId = "W-9999";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("ledger addition");
});

test("rejects a duplicate decision", () => {
  const result = runMutation((value) => {
    value.entries.push({ ...value.entries[0] });
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("duplicates");
});

test("rejects a missing category", () => {
  const result = runMutation((value) => {
    delete value.entries[0].category;
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("category is not closed");
});

test("rejects a conflicting category and authority", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.category === "implementation-evidence-gap");
    entry.category = "oracle-backed-current";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("basis does not match");
});

test("rejects a missing implementation gap record", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.category === "implementation-evidence-gap");
    delete entry.gap;
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("gap must name the concrete component and gate");
});

test("rejects fake implementation evidence", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.category === "implementation-evidence-gap");
    entry.reason = `${entry.reason} compiler is implemented`;
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("claims implementation");
});

test("rejects a non-later superseding decision", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.category === "superseded");
    entry.authorityRef.decisionId = "W-001";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("must be a later decision");
});

test("rejects a missing research gate", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.category === "research-gated");
    delete entry.researchGate;
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("researchGate must name the concrete research gate");
});

test("rejects BRX2 being relabeled as an implementation gap", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.decisionId === "W-1381");
    entry.category = "implementation-evidence-gap";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("fixed category assertion W-1381");
});

test("rejects a baseline without its explicit Research extension", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.decisionId === "W-1436");
    delete entry.researchExtension;
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("fixed baseline assertion W-1436");
});

test("rejects a stale BRX0 decision bridge", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.decisionId === "W-1436");
    entry.authorityRef.decisionBridge.claimDigest = "sha256:stale";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("decisionBridge.claimDigest is stale");
});

test("rejects the protocol-default mapping drift", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.decisionId === "W-1418");
    entry.gap.component = "execution";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("fixed gap assertion W-1418");
});

test("rejects the retired-label successor drift", () => {
  const result = runMutation((value) => {
    const entry = value.entries.find((candidate) => candidate.decisionId === "W-281");
    entry.authorityRef.decisionId = "W-1160";
    entry.supersessionClaim.decisionId = "W-1160";
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("fixed supersession assertion W-281");
});

test("rejects a weak audit sample", () => {
  const result = runMutation((value) => {
    value.auditSamples.byCategory["research-gated"] = ["W-001"];
  });
  expect(result.exitCode).not.toBe(0);
  expect(result.stderr.toString()).toContain("must contain at least five IDs");
});
