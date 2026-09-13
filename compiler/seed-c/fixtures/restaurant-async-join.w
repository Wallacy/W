async fn prepare(value: i64): i64 {
  return value
}

entry {
  let left = async prepare(value: 20)
  let right = async prepare(value: 22)
  let first = await left
  let second = await right
  print("Prepared ${first + second}")
}
