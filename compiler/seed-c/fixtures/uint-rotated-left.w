fn rotate(value: UInt, count: UInt): UInt {
  return u64.rotatedLeft(value, count)
}

entry {
  let rotated = rotate(value: 9223372036854775809_u64, count: 1_u64)
  print("Rotated ${rotated}")
}
