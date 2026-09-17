fn count(value: UInt): UInt {
  return u64.countOnes(value)
}

entry {
  let ones = count(value: 0xf0f0f0f00f0f0f0f_u64)
  print("Ones ${ones}")
}
