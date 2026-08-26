// Directional service streaming for the Last Light restaurant.

export struct MenuSignal {
  sequence: u64
  message: String
}

export struct MenuSignalSummary {
  received: u64
}

export enum MenuStreamError: Error {
  invalidSequence(found: u64, expected: u64)
  service(ServiceFailure)
}

export protocol MenuExchangeApi {
  async fn summarize(
    signals: take some Stream<MenuSignal, MenuStreamError>,
  ): MenuSignalSummary throws MenuStreamError

  async fn exchange(
    signals: take some Stream<MenuSignal, MenuStreamError>,
  ): some Stream<MenuSignal, MenuStreamError>
}

export service menuExchange: MenuExchangeApi {
  async fn summarize(
    signals: take some Stream<MenuSignal, MenuStreamError>,
  ): MenuSignalSummary throws MenuStreamError {
    var input = take signals
    var received = 0_u64

    for try await signal in input {
      let _ = take signal
      received += 1
    }

    return MenuSignalSummary(received: received)
  }

  async fn exchange(
    signals: take some Stream<MenuSignal, MenuStreamError>,
  ): some Stream<MenuSignal, MenuStreamError> {
    return stream <[take signals]> {
      var input = take signals
      while let signal = try await input.next() {
        yield take signal
      }
    }
  }
}
