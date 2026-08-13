import { describe, expect, test } from "bun:test"

function constructShared(input) {
  const trace = []
  const reject = (reason) => ({ status: "rejected", reason, trace })
  const declarative = input.form === "shared-declaration"

  if ((input.typeSpelling ?? "prefix") !== "prefix") {
    return reject("shared-type-is-prefix")
  }
  if (input.allocatorTypeSlot) return reject("allocator-is-origin-not-type")

  if (!declarative) return reject("retired-shared-construction-call")

  if (declarative && !new Set(["binding", "stored-field"]).has(input.context)) {
    return reject("context-cannot-promote")
  }
  if (declarative && !input.explicitSharedType) return reject("shared-type-not-written")
  if (["binding", "existing"].includes(input.source) && !input.take) return reject("missing-take")
  if (!input.lifetimeIndependent) return reject("lifetime-dependent")

  const recoverable = input.allocator !== "product.default"
  if (recoverable && input.try !== true) return reject("missing-try")
  if (recoverable && input.providerProfileJoined !== true) return reject("provider-profile-join-missing")

  trace.push({ operation: "consume-source", source: input.source })
  trace.push({
    operation: declarative ? "resolve-written-shared-type" : "resolve-share-operation",
    context: input.context,
  })
  trace.push({ operation: "stage-payload-and-control", allocator: input.allocator, physicalOrder: "unspecified" })

  if (input.failure) {
    trace.push({ operation: "cleanup", payload: 1, partialControlBlock: 1 })
    return {
      status: recoverable ? "allocation-error" : "panic",
      reason: input.failure,
      published: false,
      cleanup: { payloadDropCount: 1, partialControlBlockDropCount: 1 },
      trace,
    }
  }

  trace.push({ operation: "initialize-staged-values", strong: 1, weak: 0 })
  trace.push({ operation: "publish-shared-owner", strong: 1, weak: 0 })
  return {
    status: "accepted",
    resultType: "shared MenuSection",
    allocationVisible: true,
    failurePolicy: recoverable ? "throws AllocationError" : "normal OOM",
    published: true,
    trace,
  }
}

function lowerSharedType({ payload, optionalHandle = false }) {
  const owner = `shared ${payload}`
  return optionalHandle ? `Option<${owner}>` : owner
}

describe("R1 shared-construction host oracle", () => {
  test("the selected declaration and custom allocator path are both explicit", () => {
    const normal = constructShared({
      form: "shared-declaration",
      context: "binding",
      explicitSharedType: true,
      source: "temporary",
      take: false,
      allocator: "product.default",
      lifetimeIndependent: true,
    })
    const recoverable = constructShared({
      form: "shared-declaration",
      context: "binding",
      explicitSharedType: true,
      source: "temporary",
      allocator: "request.arena",
      try: true,
      providerProfileJoined: true,
      lifetimeIndependent: true,
    })
    expect(normal).toMatchObject({
      status: "accepted",
      resultType: "shared MenuSection",
      allocationVisible: true,
      failurePolicy: "normal OOM",
    })
    expect(recoverable).toMatchObject({
      status: "accepted",
      resultType: "shared MenuSection",
      failurePolicy: "throws AllocationError",
    })
    expect(recoverable.resultType).toBe(normal.resultType)
  })

  test("prefix ownership and optionality lower to distinct type shapes", () => {
    expect(lowerSharedType({ payload: "MenuSection", optionalHandle: true })).toBe(
      "Option<shared MenuSection>",
    )
    expect(lowerSharedType({ payload: "Option<MenuSection>" })).toBe(
      "shared Option<MenuSection>",
    )
  })

  test("container spelling and allocator type slots remain rejected", () => {
    const container = constructShared({
      form: "shared-declaration",
      context: "binding",
      explicitSharedType: true,
      typeSpelling: "generic-container",
      source: "temporary",
      allocator: "product.default",
      lifetimeIndependent: true,
    })
    const allocatorSlot = constructShared({
      form: "shared-declaration",
      context: "binding",
      explicitSharedType: true,
      allocatorTypeSlot: "request",
      source: "temporary",
      allocator: "request.arena",
      lifetimeIndependent: true,
    })
    expect(container.reason).toBe("shared-type-is-prefix")
    expect(allocatorSlot.reason).toBe("allocator-is-origin-not-type")
    expect(container.trace).toEqual([])
    expect(allocatorSlot.trace).toEqual([])
  })

  test("retired share calls do not allocate", () => {
    const declaration = constructShared({
      form: "shared-declaration",
      context: "binding",
      explicitSharedType: true,
      source: "temporary",
      allocator: "product.default",
      lifetimeIndependent: true,
    })
    const expression = constructShared({
      form: "share-default",
      context: "expression",
      source: "temporary",
      allocator: "product.default",
      lifetimeIndependent: true,
    })
    expect(expression.status).toBe("rejected")
    expect(expression.reason).toBe("retired-shared-construction-call")
  })

  test("failure cleans source and partial control exactly once", () => {
    const result = constructShared({
      form: "shared-declaration",
      context: "binding",
      explicitSharedType: true,
      source: "temporary",
      allocator: "request.arena",
      try: true,
      providerProfileJoined: true,
      lifetimeIndependent: true,
      failure: "budgetExceeded",
    })
    expect(result.status).toBe("allocation-error")
    expect(result.reason).toBe("budgetExceeded")
    expect(result.cleanup).toEqual({ payloadDropCount: 1, partialControlBlockDropCount: 1 })
  })

  test("existing owners require take and borrowed payloads remain rejected", () => {
    const missingTake = constructShared({
      form: "shared-declaration",
      context: "binding",
      explicitSharedType: true,
      source: "binding",
      take: false,
      allocator: "product.default",
      lifetimeIndependent: true,
    })
    const dependent = constructShared({
      form: "shared-declaration",
      context: "binding",
      explicitSharedType: true,
      source: "binding",
      take: true,
      allocator: "product.default",
      lifetimeIndependent: false,
    })
    expect(missingTake.reason).toBe("missing-take")
    expect(dependent.reason).toBe("lifetime-dependent")
  })

  test("argument return and inferred contexts never insert shared allocation", () => {
    for (const context of ["argument", "return", "inferred-binding"]) {
      const result = constructShared({
        form: "shared-declaration",
        context,
        explicitSharedType: context !== "inferred-binding",
        source: "temporary",
        allocator: "product.default",
        lifetimeIndependent: true,
      })
      expect(result.status).toBe("rejected")
      expect(result.trace).toEqual([])
    }
  })
})
