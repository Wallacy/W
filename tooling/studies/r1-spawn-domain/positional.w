// R1 Last Light spawn-domain study source.

struct MixResult {
  port: Int
  starboard: Int
}

fn mix(value: Int): Int {
  return value * value + 1
}

async fn planPair(left: Int, named right: Int): MixResult {
  let port = spawn<.compute> mix(left)
  let starboard = spawn<.compute> mix(right)
  let (portValue, starboardValue) = await (port, starboard)
  return MixResult(port: portValue, starboard: starboardValue)
}

test "spawn selects a bounded logical domain" for planPair {
  let result = await planPair(2, right: 3)
  expect result == MixResult(port: 5, starboard: 10)
}
