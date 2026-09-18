entry {
  let ordinary = u64.overflowingPower(2_u64, 63_u64)
  let overflow = u64.overflowingPower(2_u64, 64_u64)
  let wrapped = u64.overflowingPower(18446744073709551615_u64, 2_u64)
  let identity = u64.overflowingPower(0_u64, 0_u64)
  print("Overflowing power ${ordinary.0}/${ordinary.1}; ${overflow.0}/${overflow.1}; ${wrapped.0}/${wrapped.1}; ${identity.0}/${identity.1}")
}
