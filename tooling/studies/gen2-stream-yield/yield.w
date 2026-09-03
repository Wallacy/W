// GEN2 promoted narrow form. The compiler owns suspension storage. The
// explicit capture list moves `source` at construction; the source contract
// remains Stream pull, capacity zero, and one owned cursor.

export fn gen2Fixture(
  _ source: take some Stream<Order, Never>,
): some Stream<Order, Never> {
  return stream <[take source]> {
    var input = take source
    while let order = await input.next() {
      yield take order
    }
  }
}

export fn gen2MenuFixture(
  _ source: take some Stream<String, MenuError>,
): some Stream<String, MenuError> {
  return stream <[take source]> {
    var input = take source
    while let line = try await input.next() {
      yield copy line
    }
  }
}

export fn yieldDelegation(
  _ source: take some Stream<Order, BrigadeError>,
): some Stream<Order, BrigadeError> {
  return stream <[take source]> {
    for try await order in source {
      yield take order
    }
  }
}
