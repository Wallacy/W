entry {
  let divideBy = 2_u64
  let remainderBy = 18446744073709551615_u64
  var value = 4611686018427387904_u64
  value += 3_u64
  let afterAdd = value
  value -= 1_u64
  let afterSubtract = value
  value *= 2_u64
  let afterMultiply = value
  value /= divideBy
  let afterDivide = value
  value %= remainderBy
  let afterRemainder = value
  value **= 1_u64
  let afterPower = value
  value <<= 1_u64
  let afterShiftLeft = value
  value >>= 1_u64
  let afterShiftRight = value
  value &= 255_u64
  let afterAnd = value
  value ^= 85_u64
  let afterXor = value
  value |= 10_u64
  print("UInt compound ${afterAdd}/${afterSubtract}/${afterMultiply}/${afterDivide}/${afterRemainder}/${afterPower}/${afterShiftLeft}/${afterShiftRight}/${afterAnd}/${afterXor}/${value}")
}
