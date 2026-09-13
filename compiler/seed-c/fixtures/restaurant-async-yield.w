async fn prepare(value: i64): i64 {
  let staged = value + 1
  await execution#yield()
  let doubled = staged * 2
  await execution#yield()
  return doubled
}

entry {
  let left = async prepare(value: 20)
  let right = async prepare(value: 22)
  let first = await left
  let second = await right
  print("Prepared ${first + second}")
}
