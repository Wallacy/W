entry {
  var value = 8
  value += 2
  value -= 1
  value *= 4
  value /= 3
  value %= 5
  value **= 3_u64
  value <<= 2_u64
  value >>= 1_u64
  value &= 15
  value ^= 3
  value |= 8
  print("Compound ${value}")
}
