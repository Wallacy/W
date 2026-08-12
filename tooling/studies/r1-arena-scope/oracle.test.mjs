import { describe, expect, test } from "bun:test"

const forms = ["fixed-arena", "reserved-region", "scope-closure"]

function runArena(input, form) {
  const trace = ["construct-fixed-storage", "construct-arena"]
  const reject = (reason) => ({ status: "rejected", reason, trace })

  if (input.escape) return reject("arena-storage-escape")
  if (input.resetBeforeRehome) return reject("reset-with-live-arena-value")
  if (input.osAllocation) return reject("fixed-arena-calls-no-OS-allocation")

  trace.push("allocate-W-value")
  if (input.failAt === "parse") {
    trace.push("drop-W-value")
    trace.push("drop-arena-ledger")
    trace.push("bulk-release-arena")
    trace.push("release-fixed-storage")
    return { status: "unwind", reason: "parse-failed", cleanupTrace: trace }
  }

  trace.push("rehome-value")
  if (input.failAt === "rehome") {
    trace.push("drop-W-value")
    trace.push("drop-arena-ledger")
    trace.push("bulk-release-arena")
    trace.push("release-fixed-storage")
    return { status: "unwind", reason: "rehome-failed", cleanupTrace: trace }
  }

  trace.push("drop-W-value")
  trace.push("reset-arena")
  trace.push("bulk-release-arena")
  trace.push("release-fixed-storage")
  return { status: "accepted", result: input.payloadBytes, cleanupTrace: trace, form }
}

describe("R1 Arena scope host oracle", () => {
  test("problem-first Arena forms preserve output and cleanup order", () => {
    const input = { payloadBytes: 12 }
    const outcomes = forms.map((form) => runArena(input, form))
    for (const outcome of outcomes) {
      expect(outcome.status).toBe("accepted")
      expect(outcome.result).toBe(12)
      expect(outcome.cleanupTrace.indexOf("drop-W-value")).toBeLessThan(outcome.cleanupTrace.indexOf("bulk-release-arena"))
    }
    expect(outcomes[1].cleanupTrace.slice(0, -1)).toEqual(outcomes[0].cleanupTrace.slice(0, -1))
    expect(outcomes[2].cleanupTrace.slice(0, -1)).toEqual(outcomes[0].cleanupTrace.slice(0, -1))
  })

  test("unwind drops W values before bulk release", () => {
    for (const form of forms) {
      const outcome = runArena({ payloadBytes: 4, failAt: "parse" }, form)
      expect(outcome.status).toBe("unwind")
      expect(outcome.cleanupTrace.indexOf("drop-W-value")).toBeLessThan(outcome.cleanupTrace.indexOf("bulk-release-arena"))
    }
  })

  test("reset requires rehome and escape is rejected", () => {
    for (const form of forms) {
      expect(runArena({ payloadBytes: 3, resetBeforeRehome: true }, form).reason).toBe("reset-with-live-arena-value")
      expect(runArena({ payloadBytes: 3, escape: true }, form).reason).toBe("arena-storage-escape")
    }
  })

  test("fixed storage has no OS allocation path", () => {
    for (const form of forms) {
      expect(runArena({ payloadBytes: 3, osAllocation: true }, form).reason).toBe("fixed-arena-calls-no-OS-allocation")
    }
  })
})
