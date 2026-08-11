import assert from "node:assert/strict"
import test from "node:test"
import { runInterferenceLayoutOperations } from "./interference-layout-machine.mjs"

function candidate(overrides = {}) {
  return {
    op: "declareCandidate",
    id: "restaurant.performance::InterferenceCounters",
    storage: "atomic",
    layoutVisibility: "privateOpaque",
    layoutScope: "allocation",
    aggregateSize: 16,
    instanceCount: 1,
    places: [
      { name: "completed", offset: 0, size: 8 },
      { name: "horizonSamples", offset: 8, size: 8 },
    ],
    boundaries: [],
    ...overrides,
  }
}

function accesses() {
  return [
    { op: "recordAccess", place: "completed", group: "kitchen", phase: "service", mode: "write", concurrent: true },
    { op: "recordAccess", place: "horizonSamples", group: "observatory", phase: "service", mode: "write", concurrent: true },
  ]
}

function target(overrides = {}) {
  return {
    op: "targetProfile",
    digest: "sha256:target",
    interferenceSize: 64,
    maximumAlignment: 64,
    recipeInput: true,
    ...overrides,
  }
}

function evidence(overrides = {}) {
  return {
    op: "contentionEvidence",
    digest: "sha256:evidence",
    kind: "measurement",
    contentionObserved: true,
    hot: true,
    recipeInput: true,
    ...overrides,
  }
}

test("private measured places may receive an automatic physical layout", () => {
  const result = runInterferenceLayoutOperations([
    candidate(),
    ...accesses(),
    target(),
    evidence(),
    { op: "footprintBudget", maximumAdditionalBytes: 112 },
    { op: "selectLayout" },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.selection.status, "applied")
  assert.equal(result.state.selection.layout.additionalBytes, 112)
  assert.equal(result.state.selection.guaranteedExclusiveCacheLine, false)
})

test("an unknown interference size chooses the semantic fallback", () => {
  const result = runInterferenceLayoutOperations([
    candidate(),
    ...accesses(),
    target({ interferenceSize: null }),
    evidence(),
    { op: "footprintBudget", maximumAdditionalBytes: 1024 },
    { op: "selectLayout" },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.selection.status, "notApplied")
  assert.equal(result.state.selection.reason, "interferenceSizeUnknown")
})

test("published layout cannot be rewritten by the optimizer", () => {
  const result = runInterferenceLayoutOperations([
    candidate({ layoutVisibility: "published", boundaries: ["abi"] }),
    ...accesses(),
    target(),
    evidence(),
    { op: "footprintBudget", maximumAdditionalBytes: 1024 },
    { op: "selectLayout" },
  ])
  assert.equal(result.state.selection.status, "notApplied")
  assert.equal(result.state.selection.reason, "publishedLayout")
})

test("relaxed order does not remove a destructive access pair", () => {
  const relaxed = accesses().map((operation) => ({ ...operation, order: "relaxed" }))
  const result = runInterferenceLayoutOperations([
    candidate(),
    ...relaxed,
    target(),
    evidence(),
    { op: "footprintBudget", maximumAdditionalBytes: 112 },
    { op: "selectLayout" },
  ])
  assert.equal(result.state.selection.harmfulPairs.length, 1)
  assert.equal(result.state.selection.status, "applied")
})

test("partitioning remains an explicit ownership and join decision", () => {
  const result = runInterferenceLayoutOperations([{
    op: "partitionAndJoin",
    ownershipDisjoint: true,
    joinExplicit: true,
    overflowPolicyPreserved: true,
    snapshotPolicyDeclared: true,
    shards: 2,
    join: "combineBrigadeCounts",
  }])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.partition.status, "explicit")
})

test("a global atomic is never silently sharded", () => {
  const result = runInterferenceLayoutOperations([{ op: "requestSilentSharding" }])
  assert.equal(result.status, "rejected")
  assert.equal(result.error, "interferenceSilentShardingChangesSemantics")
})

test("a physical adapter publishes bytes but not a performance guarantee", () => {
  const result = runInterferenceLayoutOperations([{
    op: "publishPhysicalLayout",
    targetRecordPinned: true,
    offsetsVerified: true,
    targetRecord: "last-light-shm-v1",
    claimsExclusiveCacheLine: false,
  }])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.physicalBoundary.performanceGuarantee, false)
})

test("forcing an optimization past the footprint budget is rejected", () => {
  const result = runInterferenceLayoutOperations([
    candidate(),
    ...accesses(),
    target(),
    evidence(),
    { op: "footprintBudget", maximumAdditionalBytes: 111 },
    { op: "selectLayout", force: true },
  ])
  assert.equal(result.status, "rejected")
  assert.equal(result.error, "interferenceForcedLayoutBlocked")
})

test("lock-free progress remains separate from cache mitigation", () => {
  const result = runInterferenceLayoutOperations([
    candidate({ lockFreeRequired: true }),
    ...accesses(),
    target(),
    evidence(),
    { op: "footprintBudget", maximumAdditionalBytes: 112 },
    { op: "selectLayout" },
  ])
  assert.equal(result.state.selection.progressContract, "lockFree")
  assert.equal(result.state.selection.guaranteedExclusiveCacheLine, false)
})
