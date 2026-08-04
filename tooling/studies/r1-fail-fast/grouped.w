// R1 Last Light fail-fast study variant.

enum MixingError: Error {
  port
  starboard
}

async fn controlledDelay(ticks: u16): () {
  for _ in 0..<ticks {
    await Task.yield()
  }
}

async fn mixPort(delay: u16, fails: Bool): u32 throws MixingError {
  await controlledDelay(delay)
  if fails { throw .port }
  return 1
}

async fn mixStarboard(delay: u16, fails: Bool): u32 throws MixingError {
  await controlledDelay(delay)
  if fails { throw .starboard }
  return 2
}

async fn mixGalleyPair(
  portDelay: u16,
  portFails: Bool,
  starboardDelay: u16,
  starboardFails: Bool,
): (u32, u32) throws MixingError {
  async let port = mixPort(portDelay, fails: portFails)
  async let starboard = mixStarboard(starboardDelay, fails: starboardFails)
  return try await (port, starboard)
}
