// Mobility proofs for work outside the Last Light isolation domain.

export protocol Inspectable {
  fn inspectionCode(): u64
}

export protocol Consumable {
  take fn finish(): u64
}

export struct MenuSnapshot: Inspectable {
  revision: u64
  courses: Array<String>

  fn inspectionCode(): u64 {
    return revision
  }
}

export struct TemperatureBatch: Consumable {
  samples: Array<f64>

  take fn finish(): u64 {
    return u64(samples.count)
  }
}

export object SafeCounter: Inspectable {
  var atomic value: u64 = 0

  fn increment() {
    value += 1
  }

  fn inspectionCode(): u64 {
    return value
  }
}

export async fn inspectElsewhere<T: Inspectable>(
  value: ref T<(.shareable)>,
): u64 {
  spawn<.compute> let code = value.inspectionCode()
  return await code
}

export async fn consumeElsewhere<T: Consumable>(
  value: take T<(.transferable)>,
): u64 {
  spawn<.compute> let code = (take value).finish()
  return await code
}

export async fn commandWidth(command: ref String): usize {
  guard let word = command.scalars
    .split(where: (scalar) => scalar.isWhitespace)
    .first
  else return 0

  // Compile-fail assay: a detached task cannot retain this borrowed view.
  // Task.detached(() => print(word))

  spawn<.compute> let count = word.bytes.count
  return await count
}

test "mobility facts do not change value behavior" {
  let snapshot = MenuSnapshot(revision: 42, courses: ["cake"])
  expect snapshot.inspectionCode() == 42

  let batch = TemperatureBatch(samples: [42.0, 273.15])
  expect (take batch).finish() == 2

  // Compile-fail assay: raw pointers are local without a trusted binding fact.
  // spawn<.compute> let invalid = usePointer(take rawPointer)
}
