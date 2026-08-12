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
  const slot = form === "envelope-using" ? "contextual-envelope-runtime-slot" : "runtime-using-slot"
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

describe("R1 allocator runtime slot host oracle", () => {
  test("both spellings keep static contracts and runtime capability separate", () => {
    const input = { capabilityIdentity: true, capabilityLifetime: true, origin: "request.memory" }
    const baseline = allocateNames(input, "runtime-using")
    const alternative = allocateNames(input, "envelope-using")
    expect(baseline).toMatchObject({ status: "accepted", output: ["Dinner"], origin: "request.memory" })
    expect(alternative).toMatchObject({ status: "accepted", output: ["Dinner"], origin: "request.memory" })
    expect(alternative.output).toEqual(baseline.output)
    expect(alternative.origin).toBe(baseline.origin)
    expect(alternative.trace.slice(0, 1)).toEqual(baseline.trace.slice(0, 1))
    expect(alternative.trace.slice(2)).toEqual(baseline.trace.slice(2))
  })

  test("allocation failure preserves origin and releases partial state", () => {
    const input = { capabilityIdentity: true, capabilityLifetime: true, origin: "request.memory", failAt: "allocation" }
    const result = allocateNames(input, "runtime-using")
    const alternative = allocateNames(input, "envelope-using")
    expect(result).toMatchObject({ status: "allocation-error", origin: "request.memory", output: null })
    expect(result.trace).toContain("release-partial-buffer")
    expect(alternative).toMatchObject({ status: "allocation-error", origin: "request.memory", output: null })
    expect(alternative.trace).toContain("release-partial-buffer")
    expect(alternative.trace.slice(0, 1)).toEqual(result.trace.slice(0, 1))
    expect(alternative.trace.slice(2)).toEqual(result.trace.slice(2))
  })

  test("type identity remains rejected for either spelling", () => {
    for (const form of ["runtime-using", "envelope-using"]) {
      expect(allocateNames({ capabilityIdentity: true, capabilityLifetime: true, typeIdentity: true, origin: "request.memory" }, form)).toEqual({ status: "rejected", reason: "allocator-capability-cannot-be-type-identity", trace: [] })
    }
  })

  test("comptime capability treatment remains rejected for either spelling", () => {
    for (const form of ["runtime-using", "envelope-using"]) {
      expect(allocateNames({ capabilityIdentity: true, capabilityLifetime: true, comptime: true, origin: "request.memory" }, form)).toEqual({ status: "rejected", reason: "allocator-capability-cannot-be-comptime", trace: [] })
    }
  })

  test("a capability must carry identity and lifetime", () => {
    expect(allocateNames({ capabilityIdentity: false, capabilityLifetime: true, origin: "request.memory" }, "runtime-using").reason).toBe("allocator-capability-facts-missing")
    expect(allocateNames({ capabilityIdentity: true, capabilityLifetime: false, origin: "request.memory" }, "runtime-using").reason).toBe("allocator-capability-facts-missing")
  })
})
