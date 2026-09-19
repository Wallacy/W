// Expected exit: 0
// Expected stdout:
// Overflowing family add 0/true,11/false; subtract 41/false,18446744073709551615/true; multiply 42/false,18446744073709551614/true; negate 0/false,18446744073709551615/true; power 9223372036854775808/false,0/true,1/true,1/false

entry {
  let maximumAdd = u64.overflowingAdd(18446744073709551615_u64, 1_u64)
  let ordinaryAdd = u64.overflowingAdd(10_u64, 1_u64)
  let ordinarySubtract = u64.overflowingSubtract(42_u64, 1_u64)
  let underflowSubtract = u64.overflowingSubtract(0_u64, 1_u64)
  let ordinaryMultiply = u64.overflowingMultiply(6_u64, 7_u64)
  let overflowMultiply = u64.overflowingMultiply(18446744073709551615_u64, 2_u64)
  let zeroNegate = u64.overflowingNegate(0_u64)
  let oneNegate = u64.overflowingNegate(1_u64)
  let ordinaryPower = u64.overflowingPower(2_u64, 63_u64)
  let overflowPower = u64.overflowingPower(2_u64, 64_u64)
  let wrappedPower = u64.overflowingPower(18446744073709551615_u64, 2_u64)
  let identityPower = u64.overflowingPower(0_u64, 0_u64)
  print("Overflowing family add ${maximumAdd.0}/${maximumAdd.1},${ordinaryAdd.0}/${ordinaryAdd.1}; subtract ${ordinarySubtract.0}/${ordinarySubtract.1},${underflowSubtract.0}/${underflowSubtract.1}; multiply ${ordinaryMultiply.0}/${ordinaryMultiply.1},${overflowMultiply.0}/${overflowMultiply.1}; negate ${zeroNegate.0}/${zeroNegate.1},${oneNegate.0}/${oneNegate.1}; power ${ordinaryPower.0}/${ordinaryPower.1},${overflowPower.0}/${overflowPower.1},${wrappedPower.0}/${wrappedPower.1},${identityPower.0}/${identityPower.1}")
}
