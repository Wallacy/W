// R1 Last Light spawn-domain study source.

struct MixResult {
  port: Int
  starboard: Int
}

fn mix(value: Int): Int {
  return value * value + 1
}

async fn planPair(left: Int, right: Int): MixResult {
  spawn<.compute> let port = mix(left)
  spawn<.compute> let starboard = mix(right)
  let (portValue, starboardValue) = await (port, starboard)
  return MixResult(port: portValue, starboard: starboardValue)
}

test "spawn selects a bounded logical domain" for planPair {
  let result = await planPair(2, right: 3)
  expect result == MixResult(port: 5, starboard: 10)
}
