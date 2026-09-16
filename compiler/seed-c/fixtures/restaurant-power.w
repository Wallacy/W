fn signedPower(base: Int, exponent: UInt): Int {
  return base ** exponent
}

fn unsignedPower(base: UInt, exponent: UInt): UInt {
  return base ** exponent
}

entry {
  let signed = signedPower(base: -3, exponent: 3_u64)
  let unsigned = unsignedPower(base: 2_u64, exponent: 10_u64)
  let zero = signedPower(base: 0, exponent: 0_u64)
  let associated = 2 ** 3_u64 ** 2_u64
  print("Power ${signed}/${unsigned}/${zero}/${associated}")
}
