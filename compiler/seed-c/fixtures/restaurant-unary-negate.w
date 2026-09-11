fn reverse(value: i64): i64 {
  return -value
}

entry {
  let balance = reverse(value: 7)
  print("Balance ${balance}")
}
