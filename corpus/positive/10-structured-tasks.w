fn fetchValue(id: i32): i32 async {
  return id
}

fn checksum(value: i32): i32 {
  return value * 2
}

fn main(): Void async {
  async let left = fetchValue(20)
  async let right = fetchValue(22)
  spawn let digest = checksum(21)
  let (left, right) = await (left, right)
  print(left + right + await digest)
}
