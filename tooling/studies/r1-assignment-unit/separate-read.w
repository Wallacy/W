// R1 expression-core study variant.

struct Counter {
  var value: u32
}

fn assignUnit(_ counter: inout Counter, _ next: u32): () {
  counter.value = next
}

fn replaceAndRead(_ counter: inout Counter, _ next: u32): u32 {
  let old = counter.value
  counter.value = next
  return old
}

fn increment(_ counter: inout Counter): () {
  counter.value += 1
}

test "assignment returns Unit and reads a place separately" {
  var counter = Counter(value: 3)
  assignUnit(inout counter, 8)
  expect counter.value == 8
  let old = replaceAndRead(inout counter, 13)
  expect old == 8
  expect counter.value == 13
  increment(inout counter)
  expect counter.value == 14
}
