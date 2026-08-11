import { describe, expect, test } from "bun:test"

function constructShared(input) {
  const trace = []
  const reject = (reason) => ({ status: "rejected", reason, trace })
  const declarative = input.form === "shared-declaration"

  if (declarative && !new Set(["binding", "stored-field"]).has(input.context)) {
    return reject("context-cannot-promote")
  }
  if (declarative && !input.explicitSharedType) return reject("shared-type-not-written")
  if (input.source === "binding" && !input.take) return reject("missing-take")
  if (!input.lifetimeIndependent) return reject("lifetime-dependent")

  const recoverable = input.form === "share-using" || input.form === "tryShare"
  if (input.allocator !== "product.default" && !recoverable) {
    return reject("allocator-requires-fallible-operation")
  }

  trace.push({ operation: "consume-source", source: input.source })
  trace.push({
    operation: declarative ? "resolve-written-shared-type" : "resolve-share-operation",
    context: input.context,
  })
  trace.push({ operation: "allocate-control", allocator: input.allocator })

  if (input.failure) {
    trace.push({ operation: "drop-source", count: 1 })
    trace.push({ operation: "release-partial-control", count: 1 })
    return {
      status: recoverable ? "allocation-error" : "panic",
      reason: input.failure,
      published: false,
      trace,
    }
  }

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
      form: "share-using",
      context: "expression",
      source: "binding",
      take: true,
      allocator: "request.arena",
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
  })

  test("default share remains equivalent in expression contexts", () => {
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
    expect(expression.resultType).toBe(declaration.resultType)
    expect(expression.failurePolicy).toBe(declaration.failurePolicy)
    expect(expression.published).toBe(true)
  })

  test("failure cleans source and partial control exactly once", () => {
    const result = constructShared({
      form: "share-using",
      context: "expression",
      source: "binding",
      take: true,
      allocator: "request.arena",
      lifetimeIndependent: true,
      failure: "budgetExceeded",
    })
    expect(result.status).toBe("allocation-error")
    expect(result.published).toBe(false)
    expect(result.trace.filter((event) => event.operation === "drop-source")).toEqual([
      { operation: "drop-source", count: 1 },
    ])
    expect(result.trace.filter((event) => event.operation === "release-partial-control")).toEqual([
      { operation: "release-partial-control", count: 1 },
    ])
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
      form: "share-using",
      context: "expression",
      source: "binding",
      take: true,
      allocator: "request.arena",
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
