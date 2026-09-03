// R1 expression-core study variant.

fn countDigits(_ value: u64): u8 {
  var remaining = value
  var digits: u8 = 0

  repeat {
    digits += 1
    remaining /= 10
  } while remaining > 0

  return digits
}

fn countUntil(_ value: u64, _ limit: u8): u8 {
  var remaining = value
  var digits: u8 = 0

  repeat {
    digits += 1
    if digits == limit { break }
    remaining /= 10
  } while remaining > 0

  return digits
}

fn countWithContinue(_ value: u64): u8 {
  var remaining = value
  var digits: u8 = 0

  repeat {
    digits += 1
    remaining /= 10
    if remaining > 0 { continue }
  } while remaining > 0

  return digits
}

test "repeat counts zero, nine, and multidigit values" for countDigits {
  expect countDigits(0) == 1
  expect countDigits(9) == 1
  expect countDigits(42_424) == 5
}
