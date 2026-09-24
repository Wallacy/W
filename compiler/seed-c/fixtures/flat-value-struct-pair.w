// Focused verified-HIR witness; public native coverage is provided by the
// flat-value-aggregates.w family fixture.
// Expected: exit 0; stdout: "7,5,26\n"; stderr: ""
struct Pair { let left: i64 let right: i64 }

fn makePair(left: i64, right: i64): Pair {
  let pair: Pair = Pair(right: right, left: left)
  return pair
}

fn combine(pair: Pair, scale: i64): i64 {
  let product = pair.left * scale
  return product + pair.right
}

entry {
  let original = makePair(left: 7, right: 5)
  let result = combine(pair: original, scale: 3)
  print("${original.left},${original.right},${result}")
}
