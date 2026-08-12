import { describe, expect, test } from "bun:test"

const forms = ["upgrade", "strong-property", "strong-method"]

function acquireOwner(input, form) {
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
  test("live handles return a new optional strong owner in every form", () => {
    const input = { strongCount: 1, payload: "Dinner", linearization: "before-final-release" }
    for (const form of forms) {
      expect(acquireOwner(input, form)).toMatchObject({ status: "accepted", newOwner: true, owner: { kind: "shared", payload: "Dinner" } })
    }
  })

  test("expired handles return none after the final strong release", () => {
    const input = { strongCount: 0, payload: "Expired", linearization: "after-final-release" }
    for (const form of forms) {
      expect(acquireOwner(input, form)).toMatchObject({ status: "accepted", owner: null, newOwner: false })
    }
  })

  test("the race boundary is linearizable and never revives the payload", () => {
    const before = { strongCount: 1, payload: "Live", linearization: "before-final-release" }
    const after = { strongCount: 1, payload: "Gone", linearization: "after-final-release" }
    for (const form of forms) {
      expect(acquireOwner(before, form).newOwner).toBe(true)
      expect(acquireOwner(after, form).owner).toBeNull()
    }
  })

  test("weak has no payload access even when an owner exists", () => {
    for (const form of forms) {
      expect(acquireOwner({ strongCount: 1, payload: "Hidden", payloadAccess: true }, form)).toEqual({ status: "rejected", reason: "weak-cannot-access-payload", trace: [] })
    }
  })

  test("all forms preserve the same logical result", () => {
    const inputs = [
      { strongCount: 2, payload: "A", linearization: "before-unrelated-release" },
      { strongCount: 0, payload: "B", linearization: "after-final-release" },
    ]
    for (const input of inputs) {
      const outcomes = forms.map((form) => acquireOwner(input, form).owner)
      expect(outcomes[1]).toEqual(outcomes[0])
      expect(outcomes[2]).toEqual(outcomes[0])
    }
  })

  test("property and method spellings keep acquisition observable", () => {
    const input = { strongCount: 1, payload: "Observed", linearization: "before-final-release" }
    const property = acquireOwner(input, "strong-property")
    const method = acquireOwner(input, "strong-method")
    expect(property).toEqual(expect.objectContaining({ status: "accepted", newOwner: true }))
    expect(method).toEqual(expect.objectContaining({ status: "accepted", newOwner: true }))
    expect(property.owner).toEqual(method.owner)
  })
})
