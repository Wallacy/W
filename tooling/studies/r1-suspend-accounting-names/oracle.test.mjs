import { describe, expect, test } from "bun:test"

const policyPairs = [
  ["included", "counted"],
  ["excluded", "paused"],
  ["unspecified", "unknown"],
]

function deadlineOutcome(input) {
  if (typeof input.policy === "boolean") {
    return { status: "rejected", reason: "boolean-cannot-express-three-states" }
  }
  if (!new Set(["included", "excluded", "unspecified", "counted", "paused", "unknown"]).has(input.policy)) {
    return { status: "rejected", reason: "unknown-suspend-policy" }
  }
  if (input.suspensionKind !== "host-so") {
    return { status: "rejected", reason: "only-host-so-suspension-is-a-clock-fact" }
  }
  if (input.activeMs < 0 || input.hostSuspendMs < 0 || input.deadlineMs < 0) {
    return { status: "rejected", reason: "negative-time-fact" }
  }

  const included = input.policy === "included" || input.policy === "counted"
  const unknown = input.policy === "unspecified" || input.policy === "unknown"
  if (unknown) {
    return {
      status: "accepted",
      inferred: false,
      elapsedMs: null,
      deadlineReached: null,
      hostSuspended: input.hostSuspendMs > 0,
    }
  }

  const elapsedMs = input.activeMs + (included ? input.hostSuspendMs : 0)
  return {
    status: "accepted",
    inferred: true,
    elapsedMs,
    deadlineReached: elapsedMs >= input.deadlineMs,
    hostSuspended: input.hostSuspendMs > 0,
  }
}

describe("R1 SuspendAccounting naming host oracle", () => {
  test("the 60/50/100 scenario preserves the three outcomes", () => {
    expect(deadlineOutcome({ policy: "included", suspensionKind: "host-so", activeMs: 60, hostSuspendMs: 50, deadlineMs: 100 })).toMatchObject({ elapsedMs: 110, deadlineReached: true })
    expect(deadlineOutcome({ policy: "excluded", suspensionKind: "host-so", activeMs: 60, hostSuspendMs: 50, deadlineMs: 100 })).toMatchObject({ elapsedMs: 60, deadlineReached: false })
    expect(deadlineOutcome({ policy: "unspecified", suspensionKind: "host-so", activeMs: 60, hostSuspendMs: 50, deadlineMs: 100 })).toMatchObject({ elapsedMs: null, deadlineReached: null, inferred: false })
  })

  test("the explicit alternative maps one-to-one to the baseline states", () => {
    for (const [baseline, alternative] of policyPairs) {
      const facts = { suspensionKind: "host-so", activeMs: 60, hostSuspendMs: 50, deadlineMs: 100 }
      expect(deadlineOutcome({ ...facts, policy: baseline })).toEqual(deadlineOutcome({ ...facts, policy: alternative }))
    }
  })

  test("await, task, and coroutine are not host suspension facts", () => {
    for (const suspensionKind of ["await", "task", "coroutine"]) {
      expect(deadlineOutcome({ policy: "included", suspensionKind, activeMs: 60, hostSuspendMs: 50, deadlineMs: 100 })).toEqual({ status: "rejected", reason: "only-host-so-suspension-is-a-clock-fact" })
    }
  })

  test("a Boolean and an inferred policy are rejected", () => {
    expect(deadlineOutcome({ policy: true, suspensionKind: "host-so", activeMs: 60, hostSuspendMs: 50, deadlineMs: 100 }).reason).toBe("boolean-cannot-express-three-states")
    expect(deadlineOutcome({ policy: "always-steady", suspensionKind: "host-so", activeMs: 60, hostSuspendMs: 50, deadlineMs: 100 }).reason).toBe("unknown-suspend-policy")
  })

  test("the restaurant deadline includes eight minutes of host sleep", () => {
    const facts = { suspensionKind: "host-so", activeMs: 1, hostSuspendMs: 8, deadlineMs: 5 }
    expect(deadlineOutcome({ ...facts, policy: "included" })).toMatchObject({ elapsedMs: 9, deadlineReached: true })
    expect(deadlineOutcome({ ...facts, policy: "excluded" })).toMatchObject({ elapsedMs: 1, deadlineReached: false })
    expect(deadlineOutcome({ ...facts, policy: "unspecified" })).toMatchObject({ elapsedMs: null, deadlineReached: null, inferred: false })
  })
})
