// Expected exit: 0; stdout: "8\n"; stderr: "".
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
  let result = scan(limit: 9)
  print("${result}")
}
