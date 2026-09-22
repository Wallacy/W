// Expected exit: 0
// Expected stdout:
// Not 18446744073709551615
// And 0
// Or 18446744073709551615
// Xor 18446744073709551615
// Ones 32
// Zeros 32
// Leading 56
// Leading zero 64
// Trailing 12
// Trailing zero 64

fn invert(): UInt {
  return ~0_u64
}

fn intersect(): UInt {
  return 9223372036854775808_u64 & 9223372036854775807_u64
}

fn unite(): UInt {
  return 9223372036854775808_u64 | 9223372036854775807_u64
}

fn differ(): UInt {
  return 9223372036854775808_u64 ^ 9223372036854775807_u64
}

fn countOnes(): UInt {
  return u64.countOnes(0xf0f0f0f00f0f0f0f_u64)
}

fn countZeros(): UInt {
  return u64.countZeros(0xf0f0f0f00f0f0f0f_u64)
}

fn countLeadingZeros(value: UInt): UInt {
  return u64.countLeadingZeros(value)
}

fn countTrailingZeros(value: UInt): UInt {
  return u64.countTrailingZeros(value)
}

entry {
  var value = invert()
  print("Not ${value}")
  value = intersect()
  print("And ${value}")
  value = unite()
  print("Or ${value}")
  value = differ()
  print("Xor ${value}")
  value = countOnes()
  print("Ones ${value}")
  value = countZeros()
  print("Zeros ${value}")
  value = countLeadingZeros(value: 0x00000000000000f0_u64)
  print("Leading ${value}")
  value = countLeadingZeros(value: 0_u64)
  print("Leading zero ${value}")
  value = countTrailingZeros(value: 0x000000000000f000_u64)
  print("Trailing ${value}")
  value = countTrailingZeros(value: 0_u64)
  print("Trailing zero ${value}")
}
