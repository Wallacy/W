// Source oracle for S0's expected-use, owner, effect, and control assay.
// The host matrix covers the complete compiler CheckerContext schema; this
// source does not reify that internal interface as a public W type.

module semantic_matrix

export fn checkTicket(
  expected: Ticket,
  owner: take Ticket,
): Ticket throws KitchenError {
  if expected == owner { return owner }
  return try kitchenFallback(owner)
}

export fn fixedPoint(values: ref Array<Int>): Int {
  var index = 0
  var total = 0

  fixed: while index < values.count {
    total += values[index]
    index += 1
    if total < 0 { continue fixed }
    if index == values.count { break fixed }
  }

  return total
}

// Adversarial compile-fail assay: moving the owner through the back edge is unsafe.
// fn unsafeBackEdge(values: take Array<Int>): Int {
//   while values.count > 0 {
//     take values
//     continue
//   }
//   return values.count
// }
