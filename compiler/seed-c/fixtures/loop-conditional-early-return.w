// Expected: exit 0; stdout: "13,2,0\n"; stderr: ""
fn firstAt(limit: i64): i64 {
  var index: i64 = 0
  while index < limit {
    index = index + 1
    if index == 3 { return index + 10 }
  }
  return index
}

entry {
  let early = firstAt(limit: 5)
  let exhausted = firstAt(limit: 2)
  let empty = firstAt(limit: 0)
  print("${early},${exhausted},${empty}")
}
