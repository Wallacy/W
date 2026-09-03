// R1 expression-core study variant.

struct Counter {
  var value: u32
}

fn assignUnit(_ counter: inout Counter, _ next: u32): () {
  let replacement = next
  counter.value = replacement
}

fn replaceAndRead(_ counter: inout Counter, _ next: u32): u32 {
  let old = counter.value
  let replacement = next
  counter.value = replacement
  return old
}

fn increment(_ counter: inout Counter): () {
  let delta = 1
  counter.value += delta
}

test "an explicit temporary does not make assignment produce a value" {
  var counter = Counter(value: 3)
  assignUnit(inout counter, 8)
  expect counter.value == 8
  let old = replaceAndRead(inout counter, 13)
  expect old == 8
  expect counter.value == 13
  increment(inout counter)
  expect counter.value == 14
}
