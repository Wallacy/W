// Expected: exit 0; stdout: "-1,1,3\n"; stderr: ""
fn walk(limit: i64): i64 {
  var outer = 0
  var inner = 0
  var total = 0
  outerLoop: while outer < limit {
    outer = outer + 1
    inner = 0
    while inner < limit {
      inner = inner + 1
      if inner == 2 { continue }
      if inner == 3 { break }
      if outer == 4 { continue outerLoop }
      if outer == 5 { break outerLoop }
      total = total + 1
    }
  }
  if total == 0 { return -1 } else if total == 1 { return 1 }
  return total
}

entry {
  let zero = walk(limit: 0)
  let one = walk(limit: 1)
  let six = walk(limit: 6)
  print("${zero},${one},${six}")
}
