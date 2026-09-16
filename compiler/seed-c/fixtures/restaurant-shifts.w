fn signedRight(value: Int, count: UInt): Int {
  return value >> count
}

fn unsignedRight(value: UInt, count: UInt): UInt {
  return value >> count
}

fn signedLeft(value: Int, count: UInt): Int {
  return value << count
}

fn unsignedLeft(value: UInt, count: UInt): UInt {
  return value << count
}

entry {
  let signedRightValue = signedRight(value: -16, count: 2_u64)
  let unsignedRightValue = unsignedRight(
    value: 18446744073709551615_u64,
    count: 60_u64,
  )
  let signedLeftValue = signedLeft(value: -3, count: 4_u64)
  let unsignedLeftValue = unsignedLeft(value: 3_u64, count: 4_u64)
  print(
    "Shifts ${signedRightValue}/${unsignedRightValue}/${signedLeftValue}/${unsignedLeftValue}",
  )
}
