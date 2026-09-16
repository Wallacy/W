fn combineFlags(left: i64, right: i64): i64 {
  return left | right ^ left & right
}

entry {
  let combined = combineFlags(left: 10, right: 12)
  print("Flags ${combined}")
}
