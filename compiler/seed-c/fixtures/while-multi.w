fn serve(limit: i64): i64 {
  var served = 0
  var total = 0
  while served < limit {
    total = total + 2
    served = served + 1
  }
  return served + total
}

entry {
  let summary = serve(limit: 3)
  print("Served ${summary}")
}
