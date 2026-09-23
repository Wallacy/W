// Expected: exit 0; stdout: "7,5,26\n"; stderr: ""
type Pair = (i64, i64)

fn makePair(left: i64, right: i64): Pair {
  let pair: Pair = (left, right)
  return pair
}

fn combine(pair: Pair, scale: i64): i64 {
  let product = pair.0 * scale
  return product + pair.1
}

entry {
  let original = makePair(left: 7, right: 5)
  let result = combine(pair: original, scale: 3)
  print("${original.0},${original.1},${result}")
}
