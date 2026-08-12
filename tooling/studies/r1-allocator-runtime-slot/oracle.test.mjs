import { describe, expect, test } from "bun:test"

function allocateNames(input, form) {
  if (!input.capabilityIdentity || !input.capabilityLifetime) {
    return { status: "rejected", reason: "allocator-capability-facts-missing", trace: [] }
  }
  if (input.typeIdentity) {
    return { status: "rejected", reason: "allocator-capability-cannot-be-type-identity", trace: [] }
  }
  if (input.comptime) {
    return { status: "rejected", reason: "allocator-capability-cannot-be-comptime", trace: [] }
  }
  if (form === "generic-envelope") {
    return { status: "rejected", reason: "allocator-control-argument-not-generic-slot", trace: [] }
  }
  const slot = "initializer-allocator-control-argument"
  if (input.failAt === "allocation") {
    return {
      status: "allocation-error",
      origin: input.origin,
      output: null,
      trace: ["resolve-static-element-contract", slot, "allocation-failed", "release-partial-buffer"],
    }
  }
  return {
    status: "accepted",
    origin: input.origin,
    output: ["Dinner"],
    trace: ["resolve-static-element-contract", slot, "allocate-with-origin", "append", "return-owner"],
  }
}

function resolveConstruction(input) {
  if (input.ordinaryUsingLabel) {
    return { status: "accepted", semantics: "nominal-label" }
  }
  if (input.allocatorMeaning === "ordinary-field") {
    return { status: "rejected", reason: "allocator-label-reserved" }
  }
  if (input.argumentOrder?.indexOf("allocator") > 0) {
    return { status: "rejected", reason: "allocator-control-argument-must-be-first" }
  }
  if (!input.publishesAllocationSite) {
    return { status: "rejected", reason: "allocator-argument-inapplicable" }
  }
  return { status: "accepted", semantics: "published-allocation-site" }
}

describe("R1 allocator runtime slot host oracle", () => {
  test("the initializer control argument keeps static contracts separate", () => {
    const input = { capabilityIdentity: true, capabilityLifetime: true, origin: "request.memory" }
    const baseline = allocateNames(input, "construction-allocator")
    expect(baseline).toMatchObject({ status: "accepted", output: ["Dinner"], origin: "request.memory" })
    expect(allocateNames(input, "generic-envelope")).toMatchObject({ status: "rejected", reason: "allocator-control-argument-not-generic-slot" })
  })

  test("allocation failure preserves origin and releases partial state", () => {
    const input = { capabilityIdentity: true, capabilityLifetime: true, origin: "request.memory", failAt: "allocation" }
    const result = allocateNames(input, "construction-allocator")
    expect(result).toMatchObject({ status: "allocation-error", origin: "request.memory", output: null })
    expect(result.trace).toContain("release-partial-buffer")
    expect(allocateNames(input, "generic-envelope")).toMatchObject({ status: "rejected", reason: "allocator-control-argument-not-generic-slot" })
  })

  test("type identity remains rejected for either spelling", () => {
    for (const form of ["construction-allocator", "generic-envelope"]) {
      expect(allocateNames({ capabilityIdentity: true, capabilityLifetime: true, typeIdentity: true, origin: "request.memory" }, form)).toEqual({ status: "rejected", reason: "allocator-capability-cannot-be-type-identity", trace: [] })
    }
  })

  test("comptime capability treatment remains rejected for either spelling", () => {
    for (const form of ["construction-allocator", "generic-envelope"]) {
      expect(allocateNames({ capabilityIdentity: true, capabilityLifetime: true, comptime: true, origin: "request.memory" }, form)).toEqual({ status: "rejected", reason: "allocator-capability-cannot-be-comptime", trace: [] })
    }
  })

  test("allocator is valid only when construction publishes a storage site", () => {
    expect(resolveConstruction({ publishesAllocationSite: true })).toMatchObject({
      status: "accepted",
      semantics: "published-allocation-site",
    })
    expect(resolveConstruction({ publishesAllocationSite: false })).toEqual({
      status: "rejected",
      reason: "allocator-argument-inapplicable",
    })
  })

  test("reserved position and using labels are explicit semantic boundaries", () => {
    expect(resolveConstruction({
      publishesAllocationSite: false,
      allocatorMeaning: "ordinary-field",
    }).reason).toBe("allocator-label-reserved")
    expect(resolveConstruction({
      publishesAllocationSite: true,
      argumentOrder: ["a", "allocator", "b"],
    }).reason).toBe("allocator-control-argument-must-be-first")
    expect(resolveConstruction({ ordinaryUsingLabel: true })).toEqual({
      status: "accepted",
      semantics: "nominal-label",
    })
  })

  test("a capability must carry identity and lifetime", () => {
    expect(allocateNames({ capabilityIdentity: false, capabilityLifetime: true, origin: "request.memory" }, "construction-allocator").reason).toBe("allocator-capability-facts-missing")
    expect(allocateNames({ capabilityIdentity: true, capabilityLifetime: false, origin: "request.memory" }, "construction-allocator").reason).toBe("allocator-capability-facts-missing")
  })
})
