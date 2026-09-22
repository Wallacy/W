async fn prepare(value: i64): i64 {
  await execution#yield()
  await execution#yield()
  return value
}

entry {
  let firstTask = spawn<.main> prepare(value: 20)
  let secondTask = spawn<.main> prepare(value: 22)
  let thirdTask = spawn<.main> prepare(value: 24)
  let fourthTask = spawn<.main> prepare(value: 26)
  let first = await firstTask
  let second = await secondTask
  let third = await thirdTask
  let fourth = await fourthTask
  print("Dispatched ${first + second + third + fourth}")
}
