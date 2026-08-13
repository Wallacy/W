import assert from "node:assert/strict"
import test from "node:test"
import { runSharedControl } from "./shared-control-machine.mjs"

const map = { "$storage": "process", "$controlBlock": "process" }
const profile = {
  providerDigest: "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  version: 1,
  failure: "fallible",
  deallocator: "provider",
  mobility: "crossDomain",
  lifetime: "process",
  progress: "bounded",
  adoptionFamily: "process",
  limits: { maxObjectBytes: 4096 },
  recipeJoined: true,
  bulkReleaseOwner: "process-bulk",
}

test("SHC0 publishes the payload and hidden control-block origins atomically", () => {
  const result = runSharedControl({ operations: [{
    op: "openPlan",
    allocator: "custom",
    outcome: "success",
    providerProfile: profile,
  }, {
    op: "construct",
    context: "binding",
    typeSpelling: "shared T",
    source: "temporary",
    lifetimeIndependent: true,
    allocator: "custom",
    failure: "fallible",
    try: true,
    analysisFacts: { payloadShareable: true, counterThreadSafe: true },
    payloadOrigin: "process",
    controlBlockOrigin: "process",
    originMap: map,
  }] })
  assert.equal(result.status, "accepted")
  assert.deepEqual({
    "$storage": result.facts.allocationOriginMap["$storage"],
    "$controlBlock": result.facts.allocationOriginMap["$controlBlock"],
  }, map)
  assert.equal(result.facts.allocationOriginMap.controlBlock.mobility, "crossDomain")
  assert.equal(result.facts.published, true)
})

test("pre-publication failures consume and clean once", () => {
  const result = runSharedControl({ operations: [{
    op: "openPlan",
    allocator: "custom",
    outcome: "success",
    providerProfile: profile,
  }, {
    op: "construct",
    context: "binding",
    typeSpelling: "shared T",
    source: "existing",
    take: true,
    lifetimeIndependent: true,
    allocator: "custom",
    failure: "fallible",
    try: true,
    analysisFacts: { payloadShareable: true, counterThreadSafe: true },
    controlReserve: "outOfMemory",
    originMap: { "$storage": "process", "$controlBlock": "process" },
  }] })
  assert.equal(result.code, "SHC0-CONTROL-RESERVE-FAILURE")
  assert.deepEqual(result.facts.cleanup, { payloadDropCount: 1, partialControlBlockDropCount: 0 })
  assert.equal(result.facts.sourceConsumed, true)
})

test("weak zero retains the block until its final release", () => {
  const result = runSharedControl({ operations: [
    { op: "construct", context: "binding", typeSpelling: "shared T", source: "temporary", allocator: "product.default", lifetimeIndependent: true, originMap: { "$storage": "product.default", "$controlBlock": "product.default" } },
    { op: "makeWeak" },
    { op: "strongDrop" },
    { op: "weakRead" },
    { op: "weakDrop" },
  ] })
  assert.equal(result.facts.payloadDeinitCount, 1)
  assert.equal(result.facts.controlBlockDeinitCount, 1)
  assert.equal(result.facts.blockAlive, false)
})

test("weak acquisition creates a strong owner only while payload is live", () => {
  const live = runSharedControl({ operations: [
    { op: "construct", context: "binding", typeSpelling: "shared T", source: "temporary", allocator: "product.default", lifetimeIndependent: true, originMap: { "$storage": "product.default", "$controlBlock": "product.default" } },
    { op: "makeWeak" },
    { op: "weakAcquire" },
  ] })
  assert.equal(live.status, "accepted")
  assert.equal(live.facts.strong, 2)

  const expired = runSharedControl({ operations: [
    { op: "construct", context: "binding", typeSpelling: "shared T", source: "temporary", allocator: "product.default", lifetimeIndependent: true, originMap: { "$storage": "product.default", "$controlBlock": "product.default" } },
    { op: "makeWeak" },
    { op: "strongDrop" },
    { op: "weakAcquire" },
  ] })
  assert.equal(expired.status, "accepted")
  assert.equal(expired.facts.strong, 0)
  assert.equal(expired.facts.payloadAlive, false)
})

test("cross-domain construction requires rehome before shared publication", () => {
  const wrongOrder = runSharedControl({ operations: [{
    op: "openPlan", allocator: "custom", outcome: "success", providerProfile: profile,
  }, {
    op: "construct", context: "binding", typeSpelling: "shared T", source: "existing", take: true,
    allocator: "custom", lifetimeIndependent: true, failure: "fallible", try: true, crossDomain: true,
    analysisFacts: { payloadShareable: true, counterThreadSafe: true },
    payloadOrigin: "local", controlBlockOrigin: "process",
    originMap: { "$storage": "local", "$controlBlock": "process" },
  }] })
  assert.equal(wrongOrder.code, "SHC0-REHOME-BEFORE-SHARED")
})

test("custom failure mode and try syntax use the same provider fact", () => {
  const mismatch = runSharedControl({ operations: [{
    op: "openPlan", allocator: "custom", outcome: "success", providerProfile: { ...profile, failure: "fallible" },
  }, {
    op: "construct",
    context: "binding",
    typeSpelling: "shared T",
    source: "temporary",
    lifetimeIndependent: true,
    allocator: "custom",
    failure: "infallible",
    try: false,
    originMap: { "$storage": "request", "$controlBlock": "request" },
  }] })
  assert.equal(mismatch.code, "SHC0-PROVIDER-FAILURE-MISMATCH")
})

test("equal initializer and allocation errors collapse to one edge", () => {
  const result = runSharedControl({ operations: [{
    op: "openPlan", allocator: "custom", outcome: "success", providerProfile: profile,
  }, {
    op: "construct",
    context: "binding",
    typeSpelling: "shared T",
    source: "temporary",
    lifetimeIndependent: true,
    allocator: "custom",
    failure: "fallible",
    initializerThrows: true,
    initializerError: "AllocationError",
    allocationError: "AllocationError",
    errorSet: ["AllocationError"],
    try: true,
    analysisFacts: { payloadShareable: true, counterThreadSafe: true },
    originMap: { "$storage": "process", "$controlBlock": "process" },
  }] })
  assert.equal(result.status, "accepted")
  assert.deepEqual(result.facts.errorEdges, ["AllocationError"])
})
