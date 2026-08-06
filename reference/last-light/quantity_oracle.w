// Quantity/SI oracle. It records the closed W-903 contract.
// The JSON and wWire providers remain missing, so these tests are source oracles.

import si from std
import iec from std
import {
  Energy,
  MemorySize,
  PhysicalDuration,
  Temperature,
  TemperatureDelta,
} from units

enum QuantityJsonTokenOutcome {
  accepted
  rejectedAlternative
}

// Local schema oracle only. This helper is not a std.json decoder.
const fn expectedTickDurationTokenOutcome(token: ref String): QuantityJsonTokenOutcome {
  if token == "s" {
    return .accepted
  }
  return .rejectedAlternative
}

const fn durationBitsAreCanonical(): Bool {
  let fromSeconds: PhysicalDuration = 30<s>
  let fromMinutes: PhysicalDuration = 0.5<min>
  return fromSeconds.canonicalValue.toBits() == fromMinutes.canonicalValue.toBits()
}

const fn memoryBitsAreCanonical(): Bool {
  let memory: MemorySize = 64<KiB>
  return memory.canonicalValue == 524_288
}

// Fixed domain JSON examples. Their unit tokens do not vary with the runtime value.
export const tickDurationJsonExample: String = "{\"value\":30,\"unit\":\"s\"}"
export const energyUsedJsonExample: String = "{\"value\":12.5,\"unit\":\"J\"}"
export const memorySizeJsonExample: String = "{\"value\":\"524288\",\"unit\":\"bit\"}"

export fn normalizedTemperature(): Temperature {
  return 180<degC>
}

export fn temperatureDelta(): TemperatureDelta {
  let opening: Temperature = 180<degC>
  let closing: Temperature = 20<degC>
  return opening - closing
}

export fn energyDocumentValue(value: Energy): f64 {
  return value.canonicalValue
}

test "equivalent duration units have one canonical quantity and bit pattern" {
  let fromSeconds: PhysicalDuration = 30<s>
  let fromMinutes: PhysicalDuration = 0.5<min>

  expect fromSeconds.canonicalValue == fromMinutes.canonicalValue
  expect durationBitsAreCanonical()
}

test "affine points normalize and point subtraction produces a delta" {
  let point = normalizedTemperature()
  let delta = temperatureDelta()

  expect point.value(in: si.K) == 453.15
  expect delta.value(in: si.deltaK) == 160.0
  // `point + point` is a compile diagnostic. `point + delta` remains a point.
}

test "IEC information stores reference bits and converts exactly to bytes" {
  // `MemorySize` is Quantity<iec.Information, u64>; `bit` is its reference unit.
  let memory: MemorySize = 64<KiB>

  expect memoryBitsAreCanonical()
  expect memory.canonicalValue == 524_288
  expect (try memory.exactValue(in: B)) == 65_536
}

test "domain JSON fixes unit tokens and rejects an alternative token" {
  expect tickDurationJsonExample == "{\"value\":30,\"unit\":\"s\"}"
  expect energyUsedJsonExample == "{\"value\":12.5,\"unit\":\"J\"}"
  expect memorySizeJsonExample == "{\"value\":\"524288\",\"unit\":\"bit\"}"
  var secondsToken = "s"
  expect expectedTickDurationTokenOutcome(ref secondsToken) == .accepted

  // Provider-gated expectation: a future domain witness maps `unit: "ms"` to DecodeError.
  var millisecondsToken = "ms"
  expect expectedTickDurationTokenOutcome(ref millisecondsToken) == .rejectedAlternative
}
