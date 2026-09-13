async fn prepare(value: i64): i64 {
  let staged = value + 1
  await execution#yield()
  return staged * 2
}

entry {
  let left = async prepare(value: 20)
  let right = async prepare(value: 22)
  let first = await left
  let second = await right
  print("Prepared ${first + second}")
}
