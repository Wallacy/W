fn invert(value: UInt): UInt {
  return ~value
}

entry {
  let inverted = invert(value: 0_u64)
  print("UInt not ${inverted}")
}
