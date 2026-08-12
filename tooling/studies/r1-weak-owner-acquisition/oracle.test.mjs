import { describe, expect, test } from "bun:test"

const forms = ["contextual-type", "upgrade", "strong-property", "strong-method"]

function acquireOwner(input, form) {
  if (form !== "contextual-type") {
    return { status: "rejected", reason: "retired-weak-acquisition-spelling", trace: [`${form}:rejected`] }
  }
  if (input.payloadAccess) {
    return { status: "rejected", reason: "weak-cannot-access-payload", trace: [] }
  }
  if (input.strongCount <= 0 || input.linearization === "after-final-release") {
    return { status: "accepted", owner: null, newOwner: false, trace: [`${form}:none`] }
  }
  if (!["before-final-release", "before-unrelated-release"].includes(input.linearization)) {
    return { status: "rejected", reason: "missing-linearization-point", trace: [] }
  }
  return {
    status: "accepted",
    owner: { kind: "shared", payload: input.payload },
    newOwner: true,
    trace: [`${form}:acquire-strong`, "payload-remains-live"],
  }
}

describe("R1 weak owner acquisition host oracle", () => {
  test("live contextual weak binding returns a new optional strong owner", () => {
    const input = { strongCount: 1, payload: "Dinner", linearization: "before-final-release" }
    expect(acquireOwner(input, "contextual-type")).toMatchObject({ status: "accepted", newOwner: true, owner: { kind: "shared", payload: "Dinner" } })
    for (const form of forms.slice(1)) expect(acquireOwner(input, form).status).toBe("rejected")
  })

  test("expired handles return none after the final strong release", () => {
    const input = { strongCount: 0, payload: "Expired", linearization: "after-final-release" }
    expect(acquireOwner(input, "contextual-type")).toMatchObject({ status: "accepted", owner: null, newOwner: false })
    for (const form of forms.slice(1)) expect(acquireOwner(input, form).status).toBe("rejected")
  })

  test("the race boundary is linearizable and never revives the payload", () => {
    const before = { strongCount: 1, payload: "Live", linearization: "before-final-release" }
    const after = { strongCount: 1, payload: "Gone", linearization: "after-final-release" }
    expect(acquireOwner(before, "contextual-type").newOwner).toBe(true)
    expect(acquireOwner(after, "contextual-type").owner).toBeNull()
  })

  test("weak has no payload access even when an owner exists", () => {
    expect(acquireOwner({ strongCount: 1, payload: "Hidden", payloadAccess: true }, "contextual-type")).toEqual({ status: "rejected", reason: "weak-cannot-access-payload", trace: [] })
  })

  test("all forms preserve the same logical result", () => {
    const inputs = [
      { strongCount: 2, payload: "A", linearization: "before-unrelated-release" },
      { strongCount: 0, payload: "B", linearization: "after-final-release" },
    ]
    for (const input of inputs) {
      const contextual = acquireOwner(input, "contextual-type")
      expect(contextual.status).toBe("accepted")
      for (const form of forms.slice(1)) expect(acquireOwner(input, form).status).toBe("rejected")
    }
  })

  test("retired property and method spellings are rejected", () => {
    const input = { strongCount: 1, payload: "Observed", linearization: "before-final-release" }
    const property = acquireOwner(input, "strong-property")
    const method = acquireOwner(input, "strong-method")
    expect(property).toEqual(expect.objectContaining({ status: "rejected", reason: "retired-weak-acquisition-spelling" }))
    expect(method).toEqual(expect.objectContaining({ status: "rejected", reason: "retired-weak-acquisition-spelling" }))
  })
})
