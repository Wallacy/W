fn add(left: UInt, right: UInt): UInt {
  return left + right
}

fn subtract(left: UInt, right: UInt): UInt {
  return left - right
}

fn multiply(left: UInt, right: UInt): UInt {
  return left * right
}

fn divide(left: UInt, right: UInt): UInt {
  return left / right
}

fn remainder(left: UInt, right: UInt): UInt {
  return left % right
}

fn equal(left: UInt, right: UInt): Bool {
  return left == right
}

fn notEqual(left: UInt, right: UInt): Bool {
  return left != right
}

fn less(left: UInt, right: UInt): Bool {
  return left < right
}

fn lessEqual(left: UInt, right: UInt): Bool {
  return left <= right
}

fn greater(left: UInt, right: UInt): Bool {
  return left > right
}

fn greaterEqual(left: UInt, right: UInt): Bool {
  return left >= right
}

entry {
  let high = 9223372036854775808_u64
  let maximum = 18446744073709551615_u64
  let sum = add(left: high, right: 2_u64)
  let difference = subtract(left: sum, right: 1_u64)
  let product = multiply(left: 3_u64, right: 7_u64)
  let quotient = divide(left: 23_u64, right: 3_u64)
  let remaining = remainder(left: 23_u64, right: 3_u64)
  let comparison0 = greater(left: high, right: 9223372036854775807_u64)
  let comparison1 = greaterEqual(left: high, right: high)
  let comparison2 = greater(left: maximum, right: high)
  let comparison3 = less(left: high, right: maximum)
  let comparison4 = equal(left: high, right: 9223372036854775807_u64)
  let comparison5 = notEqual(left: high, right: 9223372036854775807_u64)
  let comparison6 = lessEqual(left: high, right: high)
  print("UInt ${sum}/${difference}/${product}; div ${quotient}; rem ${remaining}; cmp ${comparison0}/${comparison1}/${comparison2}/${comparison3}/${comparison4}/${comparison5}/${comparison6}")
}
