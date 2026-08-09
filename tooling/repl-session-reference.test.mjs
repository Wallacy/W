import { expect, test } from "bun:test";
import { runReplSessionProgram } from "./repl-session-machine.mjs";

function identityProgram() {
  return [
    { op: "open", sessionId: "host-identities" },
    { op: "submit", requestId: "one", source: "let limit = 3", declarations: [{ name: "limit", value: 3 }] },
    { op: "submit", requestId: "two", source: "fn doubled(): i32 { limit * 2 }", declarations: [{ name: "doubled", kind: "compiled", body: "limit * 2", hardDependencies: [{ name: "limit", kind: "compiledLookup" }] }] },
    { op: "submit", requestId: "three", source: "let snapshot = limit * 2", declarations: [{ name: "snapshot", kind: "snapshot", expression: "limit * 2" }] },
    { op: "submit", requestId: "four", source: "let limit = 4", declarations: [{ name: "limit", value: 4 }] },
  ];
}

test("g0, opaque generation identity, and ordinal remain separate", () => {
  const result = runReplSessionProgram(identityProgram());
  expect(result.status).toBe("accepted");
  expect(result.state.session.executionOrdinal).toBe(4);
  expect(result.state.session.prompt).toBe("w[4]");
  expect(result.state.generation.display).toBe("g4");
  expect(result.state.generation.id).not.toBe("g4");
  expect(result.state.generationHistory[0].display).toBe("g0");
  expect(result.state.generationHistory[1].bindings[0].value).toBeUndefined();
  expect(result.state.receipts.map((receipt) => receipt.ordinal)).toEqual([1, 2, 3, 4]);
  const limits = result.state.generation.bindings.filter((binding) => binding.name === "limit").sort((left, right) => left.version - right.version);
  expect(limits[0].createdGenerationId).toBe(result.state.generationHistory[1].id);
  expect(limits[1].createdGenerationId).toBe(result.state.generation.id);
  expect(limits[0].createdGenerationId).not.toBe(limits[1].createdGenerationId);
});

test("snapshot stores a value and compiled hard edge invalidates by exact binding", () => {
  const result = runReplSessionProgram([...identityProgram(), { op: "inspect", name: "snapshot" }]);
  const snapshot = result.state.lastResult.binding;
  const dependent = result.state.generation.bindings.find((binding) => binding.name === "doubled");
  expect(snapshot.value).toBe(6);
  expect(snapshot.softProvenance[0].kind).toBe("softProvenance");
  expect(dependent.availability).toBe("invalidated");
  expect(dependent.hardDependencies[0].kind).toBe("compiledLookup");
  expect(result.state.invalidation[0].dependencyBindingId).toBe(dependent.hardDependencies[0].bindingId);
});

test("interactive forms are accepted without changing module top-level legality", () => {
  const result = runReplSessionProgram([
    { op: "open", sessionId: "host-forms" },
    { op: "submit", requestId: "expression", source: "1 + 2", form: "expression", outputs: [{ kind: "display", data: 3 }] },
    { op: "submit", requestId: "statement", source: "1 + 2;", form: "statement", tailExpression: false },
    { op: "submit", requestId: "loop", source: "for x in xs { print(x) }", form: "loop" },
    { op: "submit", requestId: "call", source: "call()", form: "call" },
    { op: "submit", requestId: "await", source: "await task()", form: "await", structuredEvents: [{ kind: "child", state: "settled" }, { kind: "join", outcome: "joined" }] },
    { op: "submit", requestId: "defer", source: "defer { cleanup() }", form: "defer", structuredEvents: [{ kind: "cleanup", outcome: "committed" }] },
  ]);
  expect(result.status).toBe("accepted");
  expect(result.state.session.executionOrdinal).toBe(6);
  expect(result.state.generation.display).toBe("g0");
  expect(result.state.receipts.every((receipt) => receipt.outcome === "committed")).toBe(true);
});

test("parser and semantic diagnostics are separate facts", () => {
  const result = runReplSessionProgram([
    { op: "open", sessionId: "host-facts" },
    { op: "classify", source: "fn broken(", parserFacts: { status: "invalid", code: "W-SESSION-0002", reason: "missing close" } },
    { op: "submit", requestId: "semantic", source: "var x: i32 = \"x\"", checkerFacts: { status: "invalid", code: "W-SESSION-0003", reason: "type mismatch" } },
  ]);
  expect(result.status).toBe("accepted");
  expect(result.state.lastReceipt.diagnostics[0].code).toBe("W-SESSION-0003");
  expect(result.state.lastReceipt.diagnostics[0].phase).toBe("source.semantic");
  expect(result.state.generation.display).toBe("g0");
});

test("runtime failure retains external invocation and provider outcomes", () => {
  const result = runReplSessionProgram([
    { op: "open", sessionId: "host-effects" },
    { op: "submit", requestId: "print", source: "print()", execution: { outcome: "failure", events: ["invocation-observed", "runtime-failure"] }, effects: [{ provider: "foreign", kind: "print", payload: "seen" }] },
    { op: "submit", requestId: "rollback", source: "tx()", execution: { outcome: "failure", events: ["invocation-observed", "runtime-failure"] }, effects: [{ provider: "db", kind: "write", transaction: { capability: "transaction", events: ["rolledBack"] } }] },
    { op: "submit", requestId: "unknown", source: "tx()", execution: { outcome: "failure", events: ["invocation-observed", "runtime-failure"] }, effects: [{ provider: "db", kind: "write", transaction: { capability: "transaction", events: ["unknown"] } }] },
  ]);
  expect(result.state.generation.display).toBe("g0");
  expect(result.state.receipts.map((receipt) => receipt.outcome)).toEqual(["runtime-error", "runtime-error", "runtime-error"]);
  expect(result.state.receipts[0].effects[0].durableOutcome).toBe("observed");
  expect(result.state.receipts[1].effects[0].durableOutcome).toBe("rolledBack");
  expect(result.state.receipts[2].effects[0].durableOutcome).toBe("unknown");
});

test("resource owner scopes survive independent graph publication and drain only on rebind", () => {
  const result = runReplSessionProgram([
    { op: "open", sessionId: "host-scope" },
    { op: "submit", requestId: "watch", source: "let watch = make()", declarations: [{ name: "watch", persistentResource: true, resource: "watch" }] },
    { op: "submit", requestId: "sibling", source: "let value = 1", declarations: [{ name: "value", value: 1 }] },
    { op: "submit", requestId: "replace", source: "let watch = make2()", declarations: [{ name: "watch", persistentResource: true, resource: "watch2" }], allowDrain: { confirmed: true }, resourceEvents: [{ resource: "watch", active: true, providerState: "replaceable", events: [{ kind: "close", outcome: "ready" }] }] },
  ]);
  expect(result.status).toBe("accepted");
  expect(result.state.ownerScopes).toHaveLength(1);
  expect(result.state.ownerScopes[0].resource).toBe("watch2");
  expect(result.state.receipts.at(-1).cleanup.drain).toBe("ready");
});

test("Copy staging is automatic while move-only ownership operations reject separately", () => {
  const copy = runReplSessionProgram([
    { op: "open", sessionId: "host-copy" },
    { op: "submit", requestId: "counter", source: "var counter = 0", declarations: [{ name: "counter", value: 0, copyable: true }] },
    { op: "submit", requestId: "update", source: "counter += 1", mutations: [{ name: "counter", mode: "inout", operation: "add", delta: 1 }] },
  ]);
  expect(copy.state.receipts.at(-1).outcome).toBe("committed");
  expect(copy.state.receipts.at(-1).phases.find((phase) => phase.phase === "staged").details.automaticCopyStaging).toBe(true);
  const counterVersions = copy.state.generation.bindings.filter((binding) => binding.name === "counter");
  expect(counterVersions.find((binding) => binding.availability === "available").value).toBe(1);
  expect(counterVersions.find((binding) => binding.availability === "available").version).toBe(2);
  expect(counterVersions[0].createdGenerationId).not.toBe(counterVersions[1].createdGenerationId);

  for (const [mode, code] of [["take", "W-SESSION-0007"], ["inout", "W-SESSION-0008"], ["ref", "W-SESSION-0009"], ["borrow", "W-SESSION-0010"], ["view", "W-SESSION-0011"]]) {
    const rejected = runReplSessionProgram([
      { op: "open", sessionId: `host-${mode}` },
      { op: "submit", requestId: "owner", source: "let owner = make()", declarations: [{ name: "owner", value: "opaque", copyable: false, moveOnly: true }] },
      { op: "submit", requestId: mode, source: `${mode} owner`, declarations: [{ name: "result", value: 1 }], mutations: [{ name: "owner", mode, operation: "escape", escape: true, typeFacts: { copyable: false, moveOnly: true } }] },
    ]);
    expect(rejected.state.lastReceipt.outcome).toBe("rejected");
    expect(rejected.state.lastReceipt.diagnostics[0].code).toBe(code);
  }
});

test("step machine allows a reader during staging and active cancellation", () => {
  const result = runReplSessionProgram([
    { op: "open", sessionId: "host-step" },
    { op: "beginSubmission", requestId: "active", source: "let staged = 1", declarations: [{ name: "staged", value: 1 }] },
    { op: "inspect", name: "staged" },
    { op: "advanceSubmission", requestId: "active" },
    { op: "cancel", requestId: "active" },
    { op: "finishSubmission", requestId: "active" },
  ]);
  expect(result.status).toBe("accepted");
  expect(result.state.lastReceipt.outcome).toBe("cancelled");
  expect(result.state.lastReceipt.ordinal).toBe(1);
  expect(result.state.lastResult.kind).toBe("receipt");
  expect(result.state.generation.display).toBe("g0");
});

test("stale display is not an identity and explicit reanalysis uses current opaque base", () => {
  const result = runReplSessionProgram([
    { op: "open", sessionId: "host-stale" },
    { op: "submit", requestId: "one", source: "let one = 1", declarations: [{ name: "one", value: 1 }] },
    { op: "submit", requestId: "display", baseGeneration: "g1", source: "let two = 2", declarations: [{ name: "two", value: 2 }] },
    { op: "submit", requestId: "reanalysis", baseGeneration: { id: "missing", incarnation: 1 }, reanalyse: true, source: "let fresh = 3", declarations: [{ name: "fresh", value: 3 }] },
  ]);
  expect(result.status).toBe("accepted");
  expect(result.state.receipts[1].outcome).toBe("rejected");
  expect(result.state.receipts[1].diagnostics[0].code).toBe("W-SESSION-0005");
  expect(result.state.lastReceipt.reanalysed).toBe(true);
  expect(result.state.lastReceipt.reanalysis.currentOpaqueBase.id).not.toBe("g1");
});

test("reset opens g0 in a new incarnation", () => {
  const result = runReplSessionProgram([
    { op: "open", sessionId: "host-reset" },
    { op: "submit", requestId: "one", source: "let one = 1", declarations: [{ name: "one", value: 1 }] },
    { op: "reset", requestId: "reset", force: true },
  ]);
  expect(result.state.session.incarnation).toBe(2);
  expect(result.state.session.executionOrdinal).toBe(2);
  expect(result.state.generation.display).toBe("g0");
  expect(result.state.generationHistory[0].display).toBe("g0");
  expect(result.state.lastReceipt.cleanup.forceBoundary).toBe(true);
  expect(result.state.lastReceipt.ordinal).toBe(2);
});

test("history raw source is redactable and bounded; output truncation is explicit", () => {
  const result = runReplSessionProgram([
    { op: "open", sessionId: "host-bounds", historyPolicy: { rawSource: "redact" }, limits: { historyCount: 1, historyBytes: 2000, outputBytes: 500 } },
    { op: "submit", requestId: "one", source: "let secret = 1", declarations: [{ name: "secret", value: 1 }], outputs: [{ kind: "display", bytes: 1000, data: "large" }], outputPolicy: "truncate" },
    { op: "submit", requestId: "two", source: "let two = 2", declarations: [{ name: "two", value: 2 }] },
  ]);
  expect(result.status).toBe("accepted");
  expect(result.state.history.records).toHaveLength(1);
  expect(result.state.history.records[0].rawSource).toBe(null);
  expect(result.state.outputs.committed.some((output) => output.truncated)).toBe(true);
});

test("false rollback claim is rejected", () => {
  const result = runReplSessionProgram([
    { op: "open", sessionId: "host-false-rollback" },
    { op: "submit", requestId: "false", source: "print()", execution: { outcome: "failure", events: ["invocation-observed", "runtime-failure"] }, effects: [{ provider: "foreign", kind: "print", rollbackClaim: "rolledBack" }] },
  ]);
  expect(result.status).toBe("accepted");
  expect(result.state.lastReceipt.outcome).toBe("rejected");
  expect(result.state.lastReceipt.diagnostics[0].code).toBe("W-SESSION-0018");
  expect(result.state.effects).toHaveLength(0);
});

test("transitive invalidation keeps the immediate predecessor edge", () => {
  const result = runReplSessionProgram([
    { op: "open", sessionId: "host-transitive" },
    { op: "submit", requestId: "limit", source: "let limit = 3", declarations: [{ name: "limit", value: 3 }] },
    { op: "submit", requestId: "doubled", source: "fn doubled() { limit * 2 }", declarations: [{ name: "doubled", kind: "compiled", hardDependencies: [{ name: "limit", kind: "compiledLookup" }] }] },
    { op: "submit", requestId: "menu", source: "fn menu() { doubled() }", declarations: [{ name: "menu", kind: "compiled", hardDependencies: [{ name: "doubled", kind: "compiledLookup" }] }] },
    { op: "submit", requestId: "rebind", source: "let limit = 4", declarations: [{ name: "limit", value: 4 }] },
  ]);
  const menu = result.state.invalidation.find((entry) => entry.name === "menu");
  const doubled = result.state.invalidation.find((entry) => entry.name === "doubled");
  expect(menu.dependencyName).toBe("doubled");
  expect(menu.dependencyBindingId).toBe(doubled.bindingId);
  expect(doubled.dependencyName).toBe("limit");
});

test("owner registry retains draining scopes on degraded publication and reset failure", () => {
  const degraded = runReplSessionProgram([
    { op: "open", sessionId: "host-owner-degraded" },
    { op: "submit", requestId: "owner", source: "let watcher = watch()", declarations: [{ name: "watcher", persistentResource: true, resource: "watcher" }] },
    { op: "submit", requestId: "replace", source: "let watcher = watch2()", declarations: [{ name: "watcher", persistentResource: true, resource: "watcher2" }], allowDrain: { confirmed: true }, resourceEvents: [{ resource: "watcher", providerState: "replaceable" }], drainEvents: [{ phase: "post-publish", outcome: "deadline" }] },
  ]);
  expect(degraded.state.ownerScopes.map((scope) => scope.state).sort()).toEqual(["faulted", "owned"]);
  const reset = runReplSessionProgram([
    { op: "open", sessionId: "host-reset-failure" },
    { op: "submit", requestId: "owner", source: "let watcher = watch()", declarations: [{ name: "watcher", persistentResource: true, resource: "watcher" }] },
    { op: "reset", requestId: "reset", allowDrain: { confirmed: true }, resourceEvents: [] },
  ]);
  expect(reset.state.session.incarnation).toBe(1);
  expect(reset.state.generation.display).toBe("g1");
  expect(reset.state.phase).toBe("ready");
  expect(reset.state.mutationBlocked).toBe(false);
  expect(reset.state.ownerScopes[0].state).toBe("owned");
  expect(reset.state.effects).toHaveLength(0);
  expect(reset.state.lastReceipt.effects).toHaveLength(0);
  expect(reset.state.lastReceipt.diagnostics[0].code).toBe("W-SESSION-0012");
  const resetTrace = reset.trace.find((entry) => entry.operation.op === "reset");
  const stable = (state) => ({
    incarnation: state.session.incarnation,
    generation: state.generation,
    phase: state.phase,
    mutationBlocked: state.mutationBlocked,
    ownerScopes: state.ownerScopes,
    admission: { writer: state.admission.writer, active: state.admission.active, queue: state.admission.queue },
    effects: state.effects,
  });
  expect(stable(resetTrace.before)).toEqual(stable(resetTrace.after));
});

test("close keeps blocked registry and records force boundary", () => {
  const blocked = runReplSessionProgram([
    { op: "open", sessionId: "host-close-blocked" },
    { op: "submit", requestId: "owner", source: "let watcher = watch()", declarations: [{ name: "watcher", persistentResource: true, resource: "foreign" }] },
    { op: "quit", requestId: "quit", allowDrain: { confirmed: true }, resourceEvents: [] },
  ]);
  expect(blocked.state.phase).toBe("closing");
  expect(blocked.state.ownerScopes[0].state).toBe("closing");
  const forced = runReplSessionProgram([
    { op: "open", sessionId: "host-close-force" },
    { op: "submit", requestId: "owner", source: "let watcher = watch()", declarations: [{ name: "watcher", persistentResource: true, resource: "foreign" }] },
    { op: "quit", requestId: "quit", force: true, allowDrain: { confirmed: true }, resourceEvents: [{ resource: "foreign", providerState: "foreign-retained", events: [{ outcome: "foreign-retained" }] }] },
  ]);
  expect(forced.state.phase).toBe("closed");
  expect(forced.state.ownerScopes[0].state).toBe("force-boundary");
  expect(forced.state.lastReceipt.ordinal).toBe(2);
});

test("structured async owner and defer outcome are event-derived", () => {
  const result = runReplSessionProgram([
    { op: "open", sessionId: "host-lifecycle" },
    { op: "submit", requestId: "await", source: "await task()", form: "await", structuredEvents: [{ kind: "child", state: "settled" }, { kind: "join", outcome: "joined" }] },
    { op: "submit", requestId: "defer", source: "defer { cleanup() }", form: "defer", structuredEvents: [{ kind: "cleanup", outcome: "failure" }] },
  ]);
  expect(result.state.receipts.map((receipt) => receipt.outcome)).toEqual(["committed", "runtime-error"]);
  expect(result.state.generation.display).toBe("g0");
});

test("output policy preserves delivered bytes and bounded effects", () => {
  const result = runReplSessionProgram([
    { op: "open", sessionId: "host-output-bound", limits: { outputBytes: 100, outputCount: 2, effectCount: 1 } },
    { op: "submit", requestId: "partial", source: "print()", execution: { outcome: "failure", events: ["invocation-observed", "runtime-failure"] }, effects: [{ provider: "stdio", kind: "print", payload: "x" }], outputs: [{ kind: "display", bytes: 150, data: "x", external: true }], outputPolicy: "truncate" },
    { op: "submit", requestId: "next", source: "print()", effects: [{ provider: "stdio", kind: "print", payload: "y" }] },
  ]);
  expect(result.state.receipts[0].effects[0].durableOutcome).toBe("observed");
  expect(result.state.outputs.committed[0].deliveredBytes).toBe(100);
  expect(result.state.outputs.committed[0].budget).toBe("truncated");
  expect(result.state.effects).toHaveLength(1);
  expect(result.state.effectsEvicted).toBe(1);
});
