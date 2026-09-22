entry {
  let maximum = u64.overflowingAdd(18446744073709551615_u64, 1_u64)
  let ordinary = u64.overflowingAdd(10_u64, 1_u64)
  print("Overflowing ${maximum.0}/${maximum.1}/${ordinary.0}/${ordinary.1}")
}
