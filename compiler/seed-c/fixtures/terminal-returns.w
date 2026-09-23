// Expected exit: 0
// Expected stdout:
// -1,0,1

fn sign(value: i64): i64 {
  if value < 0 { return -1 } else if value == 0 { return 0 }
  return 1
}

fn main() {
  let negative = sign(value: -5)
  let zero = sign(value: 0)
  let positive = sign(value: 7)
  print("${negative},${zero},${positive}")
}

entry(main)
