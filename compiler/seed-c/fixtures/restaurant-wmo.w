fn bill(total: i64): i64 {
  return total + 2
}

export fn unusedReceipt(total: i64): i64 {
  return total / 2
}

fn hiddenMenu() {
  print("Never served")
}

entry {
  let total = bill(total: 40)
  print("Bill ${total}")
}
