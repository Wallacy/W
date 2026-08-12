import assert from "node:assert/strict"
import test from "node:test"
import { deriveOperationalTime } from "./operational-time-machine.mjs"

test("Duration keeps exact signed i128 nanoseconds", () => {
  const negative = deriveOperationalTime({
    subject: "duration",
    operation: "construct",
    nanoseconds: "-1",
  })
  const overflow = deriveOperationalTime({
    subject: "duration",
    operation: "add",
    left: "170141183460469231731687303715884105727",
    right: "1",
  })
  assert.equal(negative.nanoseconds, "-1")
  assert.equal(negative.physicalLayoutPublic, false)
  assert.equal(overflow.reason, "checkedOverflow")
})

test("Clock authority is projected only from a capable root", () => {
  const granted = deriveOperationalTime({
    subject: "clock",
    operation: "project",
    root: true,
    capabilities: ["clock"],
  })
  const denied = deriveOperationalTime({
    subject: "clock",
    operation: "project",
    root: true,
    capabilities: [],
  })
  assert.equal(granted.retainedOwner, true)
  assert.equal(granted.wallClock, false)
  assert.equal(denied.providerCalled, false)
})

test("provider samples are nondecreasing and expose honest clock facts", () => {
  const valid = deriveOperationalTime({
    subject: "clock",
    operation: "samples",
    factSource: "provider",
    samples: ["4", "4", "9"],
    resolutionNanoseconds: "1",
    hostSuspendPolicy: "unspecified",
  })
  const regressed = deriveOperationalTime({
    subject: "clock",
    operation: "samples",
    factSource: "provider",
    samples: ["9", "8"],
    resolutionNanoseconds: "1",
    hostSuspendPolicy: "included",
  })
  assert.equal(valid.nondecreasing, true)
  assert.equal(valid.steadyFrequencyPromised, false)
  assert.equal(regressed.reason, "clockRegressed")
})

test("Instant relations preserve signed differences within one root", () => {
  const reversed = deriveOperationalTime({
    subject: "relation",
    operation: "duration",
    clockRoot: "root-a",
    valueRoot: "root-a",
    earlierNanoseconds: "20",
    laterNanoseconds: "5",
  })
  const foreign = deriveOperationalTime({
    subject: "relation",
    operation: "duration",
    clockRoot: "root-a",
    valueRoot: "root-b",
    earlierNanoseconds: "5",
    laterNanoseconds: "20",
  })
  assert.equal(reversed.nanoseconds, "-15")
  assert.equal(foreign.providerCalled, false)
})

test("Deadline remaining clamps to zero without inventing infinity", () => {
  const remaining = deriveOperationalTime({
    subject: "relation",
    operation: "remaining",
    clockRoot: "root-a",
    valueRoot: "root-a",
    nowNanoseconds: "30",
    deadlineNanoseconds: "10",
  })
  assert.equal(remaining.nanoseconds, "0")
  assert.equal(remaining.clampedAtZero, true)
})

test("timer expiration cannot publish early and drains structured cleanup", () => {
  const late = deriveOperationalTime({
    subject: "timer",
    deadlineNanoseconds: "100",
    firedNanoseconds: "117",
    cancellationRequested: true,
    cleanupDrained: true,
  })
  const early = deriveOperationalTime({
    subject: "timer",
    deadlineNanoseconds: "100",
    firedNanoseconds: "99",
    cancellationRequested: true,
    cleanupDrained: true,
  })
  assert.equal(late.resumedLateNanoseconds, "17")
  assert.equal(late.threadKilled, false)
  assert.equal(early.reason, "earlyExpiration")
})

test("only Duration crosses a data boundary", () => {
  const duration = deriveOperationalTime({
    subject: "boundary",
    value: "duration",
    boundary: "service",
  })
  const instant = deriveOperationalTime({
    subject: "boundary",
    value: "instant",
    boundary: "service",
  })
  assert.equal(duration.portable, true)
  assert.equal(instant.reason, "rootLocalTimeValue")
})

test("virtual clocks advance only through explicit fixture input", () => {
  const result = deriveOperationalTime({
    subject: "virtual",
    provider: "virtual",
    factSource: "testFixture",
    advances: ["5", "0", "7"],
  })
  assert.deepEqual(result.samples, ["0", "5", "5", "12"])
  assert.equal(result.ambientReads, 0)
})
