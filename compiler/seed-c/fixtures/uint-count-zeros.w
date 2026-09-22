fn count(value: UInt): UInt {
  return u64.countZeros(value)
}

entry {
  let zeros = count(value: 0xf0f0f0f00f0f0f0f_u64)
  print("Zeros ${zeros}")
}
