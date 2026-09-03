// Deterministic collections for the last service window.

import { Course, Money, OrderId } from domain

export struct ArrivalTicket {
  orderId: OrderId
  priority: u8
  sequence: u64
}

export struct CollisionKey: Hashable {
  raw: u64

  fn hash(into hasher: inout Hasher) {
    // The test hasher puts every key in one bucket.
    hasher.append(0)
  }
}

export fn stableServiceOrder(tickets: take Array<ArrivalTicket>): Array<ArrivalTicket> {
  var sorted = take tickets
  sorted.sort(by: (left, right) => left.priority.compare(right.priority))
  return sorted
}

// A zero skips the remaining cells in its row. A value outside the five-bit
// carrier stops the complete scan. Labels keep both exits explicit.
export fn foldDiagnosticBits(rows: ref Array<Array<u8>>): u32 {
  var bits: u32 = 0

  assembleWord: {
    scanRows: for ref row in rows {
      for value in row {
        // A zero skips this row. An invalid carrier stops the complete scan.
        if value == 0 { continue scanRows }
        if value > 31 { break assembleWord }
        bits <<= 5
        bits |= value
      }
    }
  }

  return bits
}

test "Map iteration is independent from its randomized hash seed" {
  var prices: Map<String, Money> = [
    "nebula broth": Money(minorUnits: 1_200, currency: .ww),
    "horizon cake": Money(minorUnits: 4_242, currency: .ww),
  ]

  prices["nebula broth"] = Money(minorUnits: 1_300, currency: .ww)
  expect prices.keys.collect() == ["nebula broth", "horizon cake"]

  guard let removed = prices.removeValue(for: "nebula broth") else panic("fixture key is missing")
  prices["nebula broth"] = removed

  expect prices.keys.collect() == ["horizon cake", "nebula broth"]
  expect prices["nebula broth"]?.minorUnits == 1_300
}

test "Map confirms equality after a forced hash collision" {
  var values = Map<CollisionKey, String>()
  values[CollisionKey(raw: 7)] = "seven"
  values[CollisionKey(raw: 9)] = "nine"

  expect values.count == 2
  expect values[CollisionKey(raw: 7)] == "seven"
  expect values[CollisionKey(raw: 9)] == "nine"
}

test "borrowed lookup updates a value without a second insertion" {
  var counts: Map<Course, u32> = [.horizonCake: 1]

  if let mut ref count = counts[.horizonCake] {
    count += 1
  }

  if let ref count = counts[.horizonCake] {
    expect count == 2
  }
}

test "Set keeps the first insertion and never mutates a key in place" {
  var courses = Set([.horizonCake, .nebulaBroth, .horizonCake])

  expect courses.collect() == [.horizonCake, .nebulaBroth]
  expect !courses.add(.horizonCake)

  let removed = courses.remove(.horizonCake)
  expect removed == .horizonCake
  expect courses.add(.horizonCake)
  expect courses.collect() == [.nebulaBroth, .horizonCake]

  // Compile-fail assay: Set does not provide MutableIterable.
  // for mut ref course in courses { course.rename() }
}

test "fixed repeat and slice contracts keep storage explicit" {
  let digest: [u8; 32] = [0; 32]
  var temperatures = [270.0, 271.0, 272.0, 273.0]
  let middle: view Array<f64> = temperatures[1..<3]

  expect digest.count == 32
  expect middle == [271.0, 272.0]

  // Compile-fail assay: append can relocate storage borrowed by middle.
  // temperatures.append(274.0)
  print(middle[0])
}

test "iteration states borrow copy and consumption" {
  let codes: Array<u16> = [200, 202]
  var mutableCodes = [1, 2]
  var borrowedSum: u32 = 0

  for ref code in codes {
    borrowedSum += code
  }

  for mut ref code in mutableCodes {
    code += 40
  }

  var copiedSum: u32 = 0
  for copy code in codes {
    copiedSum += code
  }

  var consumed: u32 = 0
  for code in take mutableCodes {
    consumed += code
  }

  expect borrowedSum == copiedSum
  expect consumed == 83
}

test "stable sort preserves the order of equal priorities" {
  let tickets = stableServiceOrder(tickets: [
    ArrivalTicket(orderId: 7, priority: 1, sequence: 0),
    ArrivalTicket(orderId: 9, priority: 1, sequence: 1),
    ArrivalTicket(orderId: 11, priority: 0, sequence: 2),
  ])

  expect tickets.map((ticket) => ticket.orderId) == [11, 7, 9]

  let urgent: Array<OrderId> = tickets.lazy
    .filter((ticket) => ticket.priority == 1)
    .map((ticket) => ticket.orderId)
    .take(1)
    .collect()
  expect urgent == [7]
}

test "labeled flow leaves one deterministic diagnostic word" for foldDiagnosticBits {
  let bits = foldDiagnosticBits(rows: [
    [1, 2, 0, 31],
    [3, 4],
    [32, 5],
  ])

  expect bits == 0x0000_8864
}
