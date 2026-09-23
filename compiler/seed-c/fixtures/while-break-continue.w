// Expected exit: 0
// Expected stdout:
// 0,4,8
fn scan(limit: i64): i64 {
  var index = 0
  var total = 0
  while index < limit {
    index = index + 1
    if index == 2 { continue }
    if index == 5 { break }
    total = total + index
  }
  return total
}

entry {
  let first = scan(limit: 0)
  let second = scan(limit: 3)
  let third = scan(limit: 9)
  print("${first},${second},${third}")
}
