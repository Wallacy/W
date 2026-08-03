enum PositiveError: Error {
  notPositive(i32)
}

fn requirePositive(value: i32): i32 throws PositiveError {
  guard value > 0 else {
    throw .notPositive(value)
  }
  return value
}

fn main(): Void throws PositiveError {
  print(try requirePositive(7))
}
