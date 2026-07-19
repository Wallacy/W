fn clamp(value: i32, min: i32, max: i32): i32 {
  if value < min { return min }
  if value > max { return max }
  return value
}

fn main() {
  print(clamp(72, min: 0, max: 100))
}
