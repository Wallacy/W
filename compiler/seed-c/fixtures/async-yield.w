fn stage(value: i64): i64 {
  return value + 1
}

async fn prepare(value: i64): i64 {
  let staged = stage(value: value)
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
