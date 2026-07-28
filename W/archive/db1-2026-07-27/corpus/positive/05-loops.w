fn sumThrough(limit: i32): i32 {
  var total = 0
  var number = 1
  while number <= limit {
    total += number
    number += 1
  }
  return total
}

fn main() {
  print(sumThrough(5))
}
