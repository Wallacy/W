struct Batch {
  value: i32
}

fn duplicate(batch: ref Batch): Batch {
  return copy batch
}

fn consume(batch: take Batch): i32 {
  return batch.value
}

fn main() {
  let original = Batch(value: 42)
  let copied = duplicate(original)
  print(consume(take copied))
}
