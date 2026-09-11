fn countTo(limit: i64): i64 {
  var count = 0
  while count < limit {
    count = count + 1
  }
  return count
}

entry {
  let served = countTo(limit: 3)
  print("Served ${served}")
}
