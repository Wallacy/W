import assert from "node:assert/strict"
import test from "node:test"
import { runAllocatorScope } from "./allocator-scope-machine.mjs"

test("direct construction uses lexical plan and explicit override changes origin", () => {
  const local = runAllocatorScope({ plan: "fixed", scope: "scratch", operations: [{ op: "construct", explicitAllocator: "none" }] })
  const other = runAllocatorScope({ plan: "fixed", operations: [{ op: "construct", explicitAllocator: "other" }] })
  assert.equal(local.origin, "scratch")
  assert.equal(other.origin, "other")
})

test("rehome changes origin before a boundary", () => {
  const bypass = runAllocatorScope({ operations: [{ op: "boundary", kind: "channel", rehome: true }] })
  assert.equal(bypass.reason, "localOriginBoundary")
  const result = runAllocatorScope({ operations: [
    { op: "rehome", destination: "process", mobility: "crossDomain" },
    { op: "boundary", kind: "channel" },
  ] })
  assert.equal(result.origin, "process")
  assert.equal(result.mobility, "crossDomain")
})

test("scope close drains before typed drops and reclaim", () => {
  const result = runAllocatorScope({ operations: [{ op: "close", typedDrops: 2 }] })
  assert.equal(result.closed, true)
  assert.equal(result.typedDrops, 2)
  assert.equal(result.typedDropsBeforeReclaim, true)
  assert.equal(result.reclaimed, true)
})

test("custom plans use fixed descriptor facts and one lease close", () => {
  const descriptor = {
    op: "customContract",
    providerDigest: Array.from({ length: 32 }, (_, index) => index + 1),
    version: 1,
    failure: "fallible",
    deallocator: "provider",
    mobility: "crossDomain",
    backingOutlivesLease: true,
  }
  const result = runAllocatorScope({
    plan: "custom",
    operations: [descriptor, { op: "open", outcome: "success" }, { op: "close" }],
  })
  assert.equal(result.leaseCreated, true)
  assert.equal(result.leaseClosed, true)
  assert.equal(result.providerCloseCount, 1)
  assert.equal(result.deinitCount, 1)

  const failed = runAllocatorScope({
    plan: "custom",
    operations: [descriptor, { op: "open", outcome: "failure" }],
  })
  assert.equal(failed.reason, "planAdmissionFailed")
  assert.equal(failed.bodyEntered, false)
  assert.equal(failed.bindingCreated, false)
  assert.equal(failed.leaseCreated, false)
  assert.equal(failed.providerCloseCount, 0)
  assert.equal(failed.deinitCount, 0)

  const secondClose = runAllocatorScope({
    plan: "custom",
    operations: [descriptor, { op: "open", outcome: "success" }, { op: "close" }, { op: "close" }],
  })
  assert.equal(secondClose.reason, "customLeaseClosedTwice")
})

test("contextual calls use product root or reject an explicit none root", () => {
  const fallback = runAllocatorScope({
    rootAllocator: "system",
    operations: [{ op: "contextualCall", slot: { contextual: true, identity: "standard" } }],
  })
  assert.equal(fallback.resolutionSource, "productDefault")
  assert.equal(fallback.origin, "system")

  const none = runAllocatorScope({
    rootAllocator: ".none",
    operations: [{ op: "contextualCall", slot: { contextual: true, identity: "standard" } }],
  })
  assert.equal(none.code, "W-ALLOCATOR-0010")
  assert.deepEqual(none.available, [])
})

test("allocator resolution keeps explicit, lexical, parameter, and root priority", () => {
  const lexical = runAllocatorScope({
    rootAllocator: "root",
    contextualParameter: { identity: "parameter", mobility: "local" },
    lexicalBlocks: [{ identity: "outer", name: "outer" }, { identity: "inner", name: "inner" }],
    operations: [{ op: "contextualCall", slot: { contextual: true, identity: "standard" } }],
  })
  assert.equal(lexical.origin, "inner")
  assert.equal(lexical.resolutionSource, "lexicalBlock")

  const parameter = runAllocatorScope({
    rootAllocator: "root",
    contextualParameter: { identity: "parameter", mobility: "local" },
    operations: [{ op: "contextualCall", slot: { contextual: true, identity: "standard" } }],
  })
  assert.equal(parameter.origin, "parameter")
  assert.equal(parameter.resolutionSource, "contextualParameter")

  const root = runAllocatorScope({
    rootAllocator: "root",
    operations: [{ op: "contextualCall", slot: { contextual: true, identity: "standard" } }],
  })
  assert.equal(root.origin, "root")
  assert.equal(root.resolutionSource, "productDefault")

  const result = runAllocatorScope({
    rootAllocator: "root",
    contextualParameter: { identity: "parameter", mobility: "local" },
    lexicalBlocks: [
      { identity: "outer", name: "outer", mobility: "local" },
      { identity: "inner", name: "inner", mobility: "local" },
    ],
    operations: [
      { op: "contextualCall", slot: { contextual: true, identity: "standard" } },
      { op: "popContext" },
      { op: "contextualCall", explicitAllocator: "outer", slot: { contextual: true, identity: "standard" } },
    ],
  })
  assert.equal(result.resolutionSource, "explicit")
  assert.equal(result.origin, "outer")
  assert.equal(result.mobility, "local")
})

test("contextual chain restores the caller stack before the next call", () => {
  const result = runAllocatorScope({
    rootAllocator: "system",
    lexicalBlocks: [{ identity: "caller", name: "caller", mobility: "local" }],
    operations: [
      {
        op: "contextualChain",
        links: [
          { declaration: "stage", slot: { contextual: true, identity: "standard" } },
          { declaration: "decode", slot: { contextual: true, identity: "standard" } },
        ],
      },
      { op: "contextualCall", slot: { contextual: true, identity: "standard" } },
    ],
  })
  assert.equal(result.origin, "caller")
  assert.equal(result.resolutionSource, "lexicalBlock")
})

test("explicit external allocator needs mobility facts before a boundary", () => {
  const accepted = runAllocatorScope({
    operations: [
      { op: "construct", explicitAllocator: "process", explicitMobility: "crossDomain" },
      { op: "boundary", kind: "spawn" },
    ],
  })
  assert.equal(accepted.origin, "process")
  assert.equal(accepted.mobility, "crossDomain")

  const unknown = runAllocatorScope({
    operations: [
      { op: "construct", explicitAllocator: "external" },
      { op: "boundary", kind: "spawn" },
    ],
  })
  assert.equal(unknown.code, "W-ALLOCATOR-0003")
  assert.equal(unknown.reason, "unknownMobilityBoundary")
})
