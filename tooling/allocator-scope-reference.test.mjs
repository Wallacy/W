import assert from "node:assert/strict"
import test from "node:test"
import { runAllocatorScope } from "./allocator-scope-machine.mjs"

test("direct construction uses lexical plan and explicit override changes origin", () => {
  const local = runAllocatorScope({ plan: "fixed", operations: [{ op: "construct", explicitAllocator: "none" }] })
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
