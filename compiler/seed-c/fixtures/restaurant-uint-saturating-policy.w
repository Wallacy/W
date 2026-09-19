// Expected exit: 0
// Expected stdout:
// Saturating policy add 18446744073709551615/11; subtract 0/10; multiply 18446744073709551615/42; negate 0/0; power 8/18446744073709551615/1

fn saturatingAdd(left: UInt, right: UInt): UInt {
  return u64.saturatingAdd(left, right)
}

fn saturatingSubtract(value: UInt, amount: UInt): UInt {
  return u64.saturatingSubtract(value, amount)
}

fn saturatingMultiply(left: UInt, right: UInt): UInt {
  return u64.saturatingMultiply(left, right)
}

fn saturatingNegate(value: UInt): UInt {
  return u64.saturatingNegate(value)
}

fn saturatingPower(base: UInt, exponent: UInt): UInt {
  return u64.saturatingPower(base, exponent)
}

entry {
  let maximumAdd = saturatingAdd(left: 18446744073709551615_u64, right: 1_u64)
  let ordinaryAdd = saturatingAdd(left: 10_u64, right: 1_u64)
  let underflowSubtract = saturatingSubtract(value: 0_u64, amount: 1_u64)
  let ordinarySubtract = saturatingSubtract(value: 11_u64, amount: 1_u64)
  let overflowMultiply = saturatingMultiply(left: 18446744073709551615_u64, right: 2_u64)
  let ordinaryMultiply = saturatingMultiply(left: 6_u64, right: 7_u64)
  let negZero = saturatingNegate(value: 0_u64)
  let negMaximum = saturatingNegate(value: 18446744073709551615_u64)
  let ordinary = saturatingPower(base: 2_u64, exponent: 3_u64)
  let clamped = saturatingPower(base: 2_u64, exponent: 64_u64)
  let zeroPowerZero = saturatingPower(base: 0_u64, exponent: 0_u64)
  print("Saturating policy add ${maximumAdd}/${ordinaryAdd}; subtract ${underflowSubtract}/${ordinarySubtract}; multiply ${overflowMultiply}/${ordinaryMultiply}; negate ${negZero}/${negMaximum}; power ${ordinary}/${clamped}/${zeroPowerZero}")
}
