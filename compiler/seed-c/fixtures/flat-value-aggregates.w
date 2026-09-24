// Expected: exit 0; stdout: "7,5,26\n"; stderr: ""
type TuplePair = (i64, i64)

struct ValuePair { let left: i64 let right: i64 }

fn makeTuplePair(left: i64, right: i64): TuplePair {
  let pair: TuplePair = (left, right)
  return pair
}

fn combineTuplePair(pair: TuplePair, scale: i64): i64 {
  let product = pair.0 * scale
  return product + pair.1
}

fn makeValuePair(left: i64, right: i64): ValuePair {
  let pair: ValuePair = ValuePair(right: right, left: left)
  return pair
}

fn combineValuePair(pair: ValuePair, scale: i64): i64 {
  let product = pair.left * scale
  return product + pair.right
}

entry {
  let tuple = makeTuplePair(left: 7, right: 5)
  let value = makeValuePair(left: tuple.0, right: tuple.1)
  let tupleResult = combineTuplePair(pair: tuple, scale: 3)
  let valueResult = combineValuePair(pair: value, scale: 3)
  let result = tupleResult + valueResult - 26
  print("${value.left},${value.right},${result}")
}
