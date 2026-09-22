fn receiptDigits(value: i64): i64 {
  var remaining = value
  var digits = 0
  repeat {
    digits = digits + 1
    remaining = remaining / 10
  } while remaining > 0
  return digits
}

entry {
  let zero = receiptDigits(value: 0)
  let cosmic = receiptDigits(value: 42424)
  print("Receipt digits ${zero}/${cosmic}")
}
