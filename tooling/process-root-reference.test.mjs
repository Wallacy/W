import assert from "node:assert/strict"
import test from "node:test"
import { deriveProcessRoot } from "./process-root-machine.mjs"

test("process projections reuse one root owner without source bindings", () => {
  const result = deriveProcessRoot({
    subject: "arguments",
    operation: "project",
    root: true,
    profile: "native-process",
    ownerId: "args@root-1",
  })
  assert.equal(result.accepted, true)
  assert.equal(result.ownerId, result.repeatedOwnerId)
  assert.equal(result.copies, 0)
})

test("native arguments preserve invalid UTF-8 without lossy text matching", () => {
  const values = [{ kind: "unixBytes", bytes: [0xff] }]
  const text = deriveProcessRoot({ subject: "arguments", operation: "containsText", values, text: "�" })
  const native = deriveProcessRoot({
    subject: "arguments",
    operation: "containsNative",
    values,
    value: { kind: "unixBytes", bytes: [0xff] },
  })
  assert.equal(text.contains, false)
  assert.equal(text.lossyDecode, false)
  assert.equal(native.contains, true)
})

test("text lookup encodes to each host-native representation", () => {
  const unix = deriveProcessRoot({
    subject: "arguments",
    operation: "containsText",
    values: [{ kind: "unixBytes", bytes: [226, 152, 149] }],
    text: "☕",
  })
  const windows = deriveProcessRoot({
    subject: "arguments",
    operation: "containsText",
    values: [{ kind: "windowsUnits", units: [0xd83c, 0xdf74] }],
    text: "🍴",
  })
  assert.equal(unix.contains, true)
  assert.equal(windows.contains, true)
  assert.equal(unix.encodedToNative, true)
  assert.equal(windows.encodedToNative, true)
})

test("Context projections require their exact product capability", () => {
  const denied = deriveProcessRoot({
    subject: "context",
    operation: "project",
    root: true,
    profile: "native-process",
    member: "network",
    capabilities: ["stdio"],
  })
  const allowed = deriveProcessRoot({
    subject: "context",
    operation: "project",
    root: true,
    profile: "native-process",
    member: "network",
    capabilities: ["network"],
  })
  assert.equal(denied.reason, "capabilityMissing")
  assert.equal(denied.providerCalled, false)
  assert.equal(allowed.requirement, "network")
  assert.equal(allowed.authorityExpanded, false)
})

test("filesystem projection keeps the granted root authority", () => {
  const denied = deriveProcessRoot({
    subject: "context",
    operation: "project",
    root: true,
    profile: "native-process",
    member: "filesystem",
    capabilities: [],
  })
  const allowed = deriveProcessRoot({
    subject: "context",
    operation: "project",
    root: true,
    profile: "native-process",
    member: "filesystem",
    capabilities: ["filesystem"],
  })
  assert.equal(denied.providerCalled, false)
  assert.equal(allowed.requirement, "filesystem")
  assert.equal(allowed.rootBound, true)
  assert.equal(allowed.authorityExpanded, false)
})

test("line decoding is bounded, strict, and accepts LF plus CRLF", () => {
  const bytes = [...new TextEncoder().encode("alpha\r\nbeta\nlast")]
  const result = deriveProcessRoot({
    subject: "input",
    operation: "lines",
    bytes,
    maximumBytes: 8,
    activeReaders: 1,
  })
  assert.deepEqual(result.lines, ["alpha", "beta", "last"])
  assert.equal(result.maximumConcurrentReads, 1)

  const invalid = deriveProcessRoot({
    subject: "input",
    operation: "lines",
    bytes: [0xff, 0x0a],
    maximumBytes: 8,
  })
  assert.equal(invalid.reason, "invalidUtf8")
})

test("output tickets never interleave calls and preserve failure progress", () => {
  const ordered = deriveProcessRoot({
    subject: "output",
    operation: "write",
    calls: [
      { ticket: 2, text: "B" },
      { ticket: 1, text: "AA" },
    ],
  })
  assert.deepEqual(ordered.ticketOrder, [1, 2])
  assert.deepEqual(ordered.output, [65, 65, 66])
  assert.equal(ordered.noInterleaving, true)

  const failed = deriveProcessRoot({
    subject: "output",
    operation: "write",
    calls: [{ ticket: 1, text: "ABC", failureAfterBytes: 2 }],
  })
  assert.equal(failed.reason, "io")
  assert.equal(failed.publishedBytes, 2)
})

test("signal replacement preserves each accepted generation", () => {
  const result = deriveProcessRoot({
    subject: "signals",
    signals: ["interrupt", "terminate"],
    maximumSignals: 2,
    events: [
      { op: "emit", signal: "interrupt" },
      { op: "replace" },
      { op: "emit", signal: "terminate" },
      { op: "complete" },
      { op: "complete" },
      { op: "cancel" },
    ],
  })
  assert.deepEqual(result.acceptedGenerations, [1, 2])
  assert.equal(result.pendingCallbacks, 0)
  assert.equal(result.maximumConcurrentHandlers, 1)
  assert.equal(result.structured, true)
})

test("service drain never claims rollback or process termination", () => {
  const result = deriveProcessRoot({
    subject: "services",
    operation: "drain",
    roots: 2,
    completedRoots: 1,
    deadlineTicks: 0,
  })
  assert.equal(result.state, "draining")
  assert.equal(result.hostDecisionRequired, true)
  assert.equal(result.rollback, false)
  assert.equal(result.processExit, false)
})

test("normal exit codes remain separate from faults", () => {
  const success = deriveProcessRoot({ subject: "exit", outcome: "return", code: 0 })
  const failure = deriveProcessRoot({ subject: "exit", outcome: "return", code: 7 })
  const panic = deriveProcessRoot({ subject: "exit", outcome: "panic" })
  assert.equal(success.kind, "success")
  assert.equal(failure.kind, "failure")
  assert.equal(panic.exitCodePublished, false)
  assert.equal(panic.distinctFromNormalReturn, true)
})
