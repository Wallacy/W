fn rotate(value: UInt, count: UInt): UInt {
  return u64.rotatedRight(value, count)
}

entry {
  let rotated = rotate(value: 3_u64, count: 1_u64)
  print("Rotated ${rotated}")
}
