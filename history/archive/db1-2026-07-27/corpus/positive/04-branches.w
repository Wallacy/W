fn absolute(value: i32): i32 {
  if value < 0 {
    return -value
  } else {
    return value
  }
}

fn main() {
  print(absolute(-9))
}
