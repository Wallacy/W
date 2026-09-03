// GEN2 current-composition fixtures. SourceRefs bind these problems to
// real Last Light symbols; this file is a paired human-facing witness.

export fn gen2Fixture(
  _ source: take some Stream<Order, Never>,
): some Stream<Order, Never> {
  return (take source).map(using: identity)
}

export async fn currentMenuCopy(
  _ source: take some Stream<view String, MenuError>,
): Array<String> throws MenuError {
  var lines = take source
  var result: Array<String> = []
  for try await line in lines {
    result.append(line.materialize())
  }
  return result
}

export async fn currentDialogue(
  _ first: take Order,
  _ second: take Order,
): Array<Order> throws ChannelSendError<Order><[.closed]> {
  return try await runBoundedOrderWindow(take first, take second)
}
