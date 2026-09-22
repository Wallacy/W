fn settle(limit: i64): i64 {
  var served = 0
  var total = 0
  while served < limit {
    total = total + 2
    served = served + 1
  }
  total = total + served
  return total
}

entry {
  let result = settle(limit: 3)
  print("Final ${result}")
}
