// R1 expression-core study variant.

fn countDigits(value: u64): u8 {
  var remaining = value
  var digits: u8 = 0

  while true {
    digits += 1
    remaining /= 10
    if remaining == 0 { break }
  }

  return digits
}

fn countUntil(value: u64, limit: u8): u8 {
  var remaining = value
  var digits: u8 = 0

  while true {
    digits += 1
    if digits == limit { break }
    remaining /= 10
    if remaining == 0 { break }
  }

  return digits
}

fn countWithContinue(value: u64): u8 {
  var remaining = value
  var digits: u8 = 0

  while true {
    digits += 1
    remaining /= 10
    if remaining > 0 { continue }
    break
  }

  return digits
}

test "while true and break preserve the loop outcomes" for countDigits {
  expect countDigits(0) == 1
  expect countDigits(9) == 1
  expect countDigits(42_424) == 5
}
