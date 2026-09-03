// R1 expression-core semantic-negative variant.

struct Counter {
  var value: u32
}

// Parseable syntax. Assignment has Unit type and cannot satisfy this return.
fn assignUnit(_ counter: inout Counter, _ next: u32): u32 {
  return counter.value = next
}

fn increment(_ counter: inout Counter): () {
  counter.value += 1
}

test "value-producing assignment is rejected in a value context" {
  var counter = Counter(value: 3)
  // Semantic oracle rejects the return assignment before execution.
  expect counter.value == 3
}
