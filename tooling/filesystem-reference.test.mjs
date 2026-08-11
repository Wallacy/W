import assert from "node:assert/strict"
import test from "node:test"
import { deriveFilesystem } from "./filesystem-machine.mjs"

const rootPath = {
  factSource: "provider",
  absolute: false,
  steps: [],
  containment: "inside",
}

test("provider resolution rejects ambient and forged filesystem authority", () => {
  const forged = deriveFilesystem({
    subject: "path",
    operation: "resolve",
    rootId: "menu-root",
    path: { ...rootPath, factSource: "caller" },
  })
  const escaped = deriveFilesystem({
    subject: "path",
    operation: "resolve",
    rootId: "menu-root",
    path: { ...rootPath, steps: [{ kind: "parent" }] },
  })
  assert.equal(forged.reason, "providerContainmentRequired")
  assert.equal(forged.providerCalled, false)
  assert.equal(escaped.reason, "outsideRoot")
})

test("provider resolution stops at root-profile bounds", () => {
  const exhausted = deriveFilesystem({
    subject: "path",
    operation: "resolve",
    rootId: "menu-root",
    rootLimits: {
      maximumPathUnits: 8,
      maximumComponents: 4,
      maximumSymlinkTraversals: 0,
    },
    path: {
      ...rootPath,
      steps: [
        {
          kind: "symlink",
          absolute: false,
          target: [{ kind: "name", value: { kind: "unixBytes", bytes: [0x61] } }],
        },
      ],
    },
  })
  assert.equal(exhausted.reason, "symlinkLimitExceeded")
  assert.equal(exhausted.providerCalled, true)
})

test("a child filesystem can only narrow provider-proved authority", () => {
  const childPath = {
    ...rootPath,
    steps: [{ kind: "name", value: { kind: "unixBytes", bytes: [114] } }],
  }
  const child = deriveFilesystem({
    subject: "scope",
    rootId: "restaurant-root",
    path: childPath,
    childRootFactSource: "provider",
    childRootId: "recipes-root",
    parentRights: ["read", "write", "metadata", "scope"],
    childRights: ["read", "metadata"],
    providerDisposition: "opened",
  })
  const forged = deriveFilesystem({
    subject: "scope",
    rootId: "restaurant-root",
    path: rootPath,
    childRootFactSource: "caller",
    childRootId: "unbounded-root",
    providerDisposition: "opened",
  })
  assert.equal(child.authorityNarrowed, true)
  assert.equal(child.authorityExpanded, false)
  assert.equal(forged.reason, "providerChildRootRequired")

  const amplified = deriveFilesystem({
    subject: "scope",
    rootId: "restaurant-root",
    path: childPath,
    childRootFactSource: "provider",
    childRootId: "recipes-root",
    parentRights: ["read", "scope"],
    childRights: ["read", "write"],
    providerDisposition: "opened",
  })
  assert.equal(amplified.reason, "authorityAmplification")
  assert.equal(amplified.providerCalled, false)
})

test("native path conversion is strict and lossy display is presentation-only", () => {
  const invalidUnix = deriveFilesystem({
    subject: "path",
    operation: "toUtf8",
    native: { kind: "unixBytes", bytes: [0xff] },
  })
  const invalidWindows = deriveFilesystem({
    subject: "path",
    operation: "toUtf8",
    native: { kind: "windowsUnits", units: [0xd800] },
  })
  const invalidAfterPrefix = deriveFilesystem({
    subject: "path",
    operation: "toUtf8",
    native: { kind: "unixBytes", bytes: [0x6d, 0xe2, 0x28, 0xa1] },
  })
  const leadingBom = deriveFilesystem({
    subject: "path",
    operation: "toUtf8",
    native: { kind: "unixBytes", bytes: [0xef, 0xbb, 0xbf, 0x6d] },
  })
  const display = deriveFilesystem({
    subject: "path",
    operation: "displayLossy",
    native: { kind: "unixBytes", bytes: [0xff] },
  })
  assert.equal(invalidUnix.reason, "invalidUnixUtf8")
  assert.equal(invalidUnix.byteOffset, 0)
  assert.equal(invalidWindows.reason, "unpairedWindowsUnit")
  assert.equal(invalidAfterPrefix.byteOffset, 1)
  assert.equal(leadingBom.text.codePointAt(0), 0xfeff)
  assert.equal(display.presentationOnly, true)
  assert.equal(display.lookupUsed, false)
})

test("static rights reject an operation before provider execution", () => {
  const result = deriveFilesystem({
    subject: "open",
    rootId: "menu-root",
    rights: ["read"],
    creation: "createNew",
    path: rootPath,
    providerDisposition: "opened",
  })
  assert.equal(result.reason, "creationRequiresWrite")
  assert.equal(result.phase, "static")
  assert.equal(result.providerCalled, false)
})

test("creation disposition never disguises atomic publication", () => {
  const replaced = deriveFilesystem({
    subject: "open",
    rootId: "menu-root",
    rights: ["write"],
    creation: "replace",
    path: rootPath,
    providerDisposition: "opened",
    fileKind: "regular",
  })
  const existing = deriveFilesystem({
    subject: "open",
    rootId: "menu-root",
    rights: ["write"],
    creation: "createNew",
    path: rootPath,
    providerDisposition: "alreadyExists",
  })
  assert.equal(replaced.truncatesExisting, true)
  assert.equal(replaced.atomicPublication, false)
  assert.equal(existing.reason, "alreadyExists")
})

test("positional reads share safely while cursors require unique ownership", () => {
  const read = deriveFilesystem({
    subject: "file",
    operation: "read",
    rights: ["read"],
    bytes: [10, 20, 30],
    offset: 1,
    maximum: 8,
  })
  const cursor = deriveFilesystem({
    subject: "file",
    operation: "reader",
    rights: ["read"],
    shared: true,
    startOffset: 0,
    steps: [],
  })
  assert.deepEqual(read.data, [20, 30])
  assert.equal(read.positionChanged, false)
  assert.equal(read.sharedAllowed, true)
  assert.equal(cursor.reason, "cursorNotShareable")
})

test("append selects its offset inside the provider operation", () => {
  const result = deriveFilesystem({
    subject: "file",
    operation: "append",
    rights: ["append"],
    bytes: [1],
    concurrentPrefix: [2, 3],
    source: [4],
    callerObservedLength: 1,
  })
  assert.equal(result.selectedOffset, 3)
  assert.equal(result.ignoredCallerLength, 1)
  assert.deepEqual(result.output, [1, 2, 3, 4])
})

test("shared positional I/O exposes conflicts without inserting a lock", () => {
  const disjoint = deriveFilesystem({
    subject: "interference",
    operations: [
      { id: "left", kind: "write", offset: 0, length: 8 },
      { id: "right", kind: "write", offset: 8, length: 8 },
    ],
  })
  const overlapping = deriveFilesystem({
    subject: "interference",
    operations: [
      { id: "reader", kind: "read", offset: 0, length: 8 },
      { id: "writer", kind: "write", offset: 4, length: 8 },
    ],
  })
  const append = deriveFilesystem({
    subject: "interference",
    operations: [
      { id: "first", kind: "append", length: 8 },
      { id: "second", kind: "append", length: 8 },
    ],
  })
  const mixed = deriveFilesystem({
    subject: "interference",
    operations: [
      { id: "writer", kind: "write", offset: 8, length: 8 },
      { id: "append", kind: "append", length: 8 },
    ],
  })
  assert.equal(disjoint.deterministic, true)
  assert.deepEqual(overlapping.providerConflicts, [["reader", "writer"]])
  assert.equal(overlapping.warningEligible, true)
  assert.equal(overlapping.lockInserted, false)
  assert.equal(append.appendOffsetSelectionAtomic, true)
  assert.equal(append.appendPayloadOrderDeterministic, false)
  assert.equal(append.warningEligible, true)
  assert.deepEqual(mixed.providerConflicts, [["writer", "append"]])
  assert.equal(mixed.deterministic, false)
})

test("snapshots publish only after bounds and version validation", () => {
  const bounded = deriveFilesystem({
    subject: "file",
    operation: "snapshot",
    rights: ["read"],
    bytes: [1, 2, 3],
    maximumBytes: 2,
    versionBefore: "v1",
    versionAfter: "v1",
  })
  const changed = deriveFilesystem({
    subject: "file",
    operation: "snapshot",
    rights: ["read"],
    bytes: [1],
    maximumBytes: 2,
    versionBefore: "v1",
    versionAfter: "v2",
  })
  assert.equal(bounded.reason, "limitExceeded")
  assert.equal(bounded.allocatedBeforeValidation, false)
  assert.equal(changed.reason, "changedDuringRead")
  assert.equal(changed.published, false)
})

test("directory streams preserve provider order without implicit work", () => {
  const result = deriveFilesystem({
    subject: "directory",
    operation: "entries",
    rootId: "menu-root",
    path: rootPath,
    limits: { maximumEntries: 2, maximumNameUnits: 4, maximumTotalNameUnits: 8 },
    entries: [
      { name: { kind: "unixBytes", bytes: [98] }, kindHint: null },
      { name: { kind: "unixBytes", bytes: [97] }, kindHint: "regular" },
    ],
    recursive: false,
    sorted: false,
  })
  assert.deepEqual(result.entries.map((entry) => entry.name.bytes[0]), [98, 97])
  assert.equal(result.order, "provider")
  assert.equal(result.singlePass, true)
})

test("namespace atomicity remains separate from durability and outcome certainty", () => {
  const renamed = deriveFilesystem({
    subject: "namespace",
    operation: "rename",
    rootId: "menu-root",
    destinationRootId: "menu-root",
    sourcePath: rootPath,
    destinationPath: rootPath,
    replacement: "replaceFile",
    destinationExists: true,
    destinationKind: "regular",
    providerOutcome: "committed",
  })
  const unknown = deriveFilesystem({
    subject: "namespace",
    operation: "rename",
    rootId: "menu-root",
    destinationRootId: "menu-root",
    sourcePath: rootPath,
    destinationPath: rootPath,
    replacement: "replaceFile",
    destinationExists: true,
    destinationKind: "regular",
    providerOutcome: "unknown",
  })
  const crossMount = deriveFilesystem({
    subject: "namespace",
    operation: "rename",
    rootId: "menu-root",
    destinationRootId: "menu-root",
    sourceMountId: "cache",
    destinationMountId: "data",
    sourcePath: rootPath,
    destinationPath: rootPath,
    replacement: "keepExisting",
    providerOutcome: "committed",
  })
  assert.equal(renamed.namespaceAtomic, true)
  assert.equal(renamed.durable, false)
  assert.equal(unknown.reason, "unknownOutcome")
  assert.equal(unknown.retrySafe, false)
  assert.equal(crossMount.reason, "crossMount")
})

test("durability is explicit and none never calls the provider", () => {
  const noOp = deriveFilesystem({
    subject: "file",
    operation: "sync",
    rights: ["write"],
    durability: "none",
    providerSupports: false,
  })
  const dropped = deriveFilesystem({ subject: "file", operation: "drop" })
  assert.equal(noOp.providerCalled, false)
  assert.equal(noOp.providerConfirmed, false)
  assert.equal(dropped.durabilityInserted, false)
})

test("cancellation retains borrowed storage until provider completion", () => {
  const cancelFirst = deriveFilesystem({
    subject: "cancellation",
    operation: "drain",
    events: ["cancel", "providerCompletion", "releaseBorrow"],
  })
  const completionFirst = deriveFilesystem({
    subject: "cancellation",
    operation: "drain",
    events: ["providerCompletion", "cancel", "releaseBorrow"],
  })
  const earlyRelease = deriveFilesystem({
    subject: "cancellation",
    operation: "drain",
    events: ["cancel", "releaseBorrow", "providerCompletion"],
  })
  assert.equal(cancelFirst.providerDrained, true)
  assert.equal(cancelFirst.completionWonRace, false)
  assert.equal(completionFirst.completionWonRace, true)
  assert.equal(earlyRelease.reason, "releaseBeforeDrain")
})
