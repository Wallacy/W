// R1 Last Light fail-fast study variant.

enum MixingError: Error {
  port
  starboard
}

async fn controlledDelay(_ ticks: u16): () {
  for _ in 0..<ticks {
    await execution#yield()
  }
}

async fn mixPort(_ delay: u16, fails: Bool): u32 throws MixingError {
  await controlledDelay(delay)
  if fails { throw .port }
  return 1
}

async fn mixStarboard(_ delay: u16, fails: Bool): u32 throws MixingError {
  await controlledDelay(delay)
  if fails { throw .starboard }
  return 2
}

async fn mixGalleyPair(
  _ portDelay: u16,
  _ portFails: Bool,
  _ starboardDelay: u16,
  _ starboardFails: Bool,
): (u32, u32) throws MixingError {
  let port = async mixPort(portDelay, fails: portFails)
  let starboard = async mixStarboard(starboardDelay, fails: starboardFails)
  let portResult = try await port
  let starboardResult = try await starboard
  return (portResult, starboardResult)
}
