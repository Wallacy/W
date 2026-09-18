fn saturatingNegate(value: UInt): UInt {
  return u64.saturatingNegate(value)
}

fn saturatingPower(base: UInt, exponent: UInt): UInt {
  return u64.saturatingPower(base, exponent)
}

entry {
  let negZero = saturatingNegate(value: 0_u64)
  let negMaximum = saturatingNegate(value: 18446744073709551615_u64)
  let ordinary = saturatingPower(base: 2_u64, exponent: 3_u64)
  let clamped = saturatingPower(base: 2_u64, exponent: 64_u64)
  let zeroPowerZero = saturatingPower(base: 0_u64, exponent: 0_u64)
  print("Saturating policy ${negZero}/${negMaximum}/${ordinary}/${clamped}/${zeroPowerZero}")
}
