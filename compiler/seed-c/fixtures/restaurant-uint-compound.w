entry {
  var value = 18446744073709551615_u64
  value &= 240_u64
  value ^= 170_u64
  value |= 5_u64
  print("UInt compound ${value}")
}
