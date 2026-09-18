entry {
  let ordinarySubtract = u64.overflowingSubtract(42_u64, 1_u64)
  let underflowSubtract = u64.overflowingSubtract(0_u64, 1_u64)
  let ordinaryMultiply = u64.overflowingMultiply(6_u64, 7_u64)
  let overflowMultiply = u64.overflowingMultiply(18446744073709551615_u64, 2_u64)
  let zeroNegate = u64.overflowingNegate(0_u64)
  let oneNegate = u64.overflowingNegate(1_u64)
  print("Overflowing family ${ordinarySubtract.0}/${ordinarySubtract.1}/${underflowSubtract.0}/${underflowSubtract.1}/${ordinaryMultiply.0}/${ordinaryMultiply.1}/${overflowMultiply.0}/${overflowMultiply.1}/${zeroNegate.0}/${zeroNegate.1}/${oneNegate.0}/${oneNegate.1}")
}
