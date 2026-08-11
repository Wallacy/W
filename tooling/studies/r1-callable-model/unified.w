// R1 alternative: one callable spelling erases representation and call mode.

struct Arrival {
  orderId: u64
}

struct Welcome {
  orderId: u64
  gate: usize
}

struct CallableObservation {
  firstGate: usize
  routedGate: usize
  tickets: (usize, usize)
  manifestCount: usize
  manifestAvailable: Bool
}

alias ErasedWelcomeRoute = fn(Arrival): Welcome

fn standardWelcome(arrival: Arrival = Arrival(orderId: 0)): Welcome {
  return Welcome(orderId: arrival.orderId, gate: 1)
}

fn route(gate: usize): fn(Arrival): Welcome {
  return capture(copy gate) (arrival) => Welcome(
    orderId: arrival.orderId,
    gate: gate,
  )
}

fn ticketSequence(initial: usize): fn(): usize {
  var next = initial

  return capture(take next) () => {
    next += 1
    return next
  }
}

fn finalManifest(orderIds: take Array<u64>): fn(): Array<u64> {
  return capture(take orderIds) () => take orderIds
}

fn recoverableRoute(gate: usize): ErasedWelcomeRoute {
  return capture(copy gate) (arrival) => Welcome(
    orderId: arrival.orderId,
    gate: gate,
  )
}

fn observeCallableModel(
  gate: usize,
  initial: usize,
  orderIds: take Array<u64>,
): CallableObservation {
  let greeter: fn(Arrival): Welcome = standardWelcome
  let first = greeter(Arrival(orderId: 42))

  let handler: fn(Arrival): Welcome = route(gate: gate)
  let routed = handler(Arrival(orderId: 43))

  let next: fn(): usize = ticketSequence(initial: initial)
  let firstTicket = next()
  let secondTicket = next()

  let manifest: fn(): Array<u64> = finalManifest(take orderIds)
  let restored = manifest()

  return CallableObservation(
    firstGate: first.gate,
    routedGate: routed.gate,
    tickets: (firstTicket, secondTicket),
    manifestCount: restored.count,
    manifestAvailable: true,
  )
}

test "one callable spelling leaves ownership policy implicit" for observeCallableModel {
  let observed = observeCallableModel(gate: 2, initial: 40, orderIds: [7, 8, 9])
  expect observed.firstGate == 1
  expect observed.routedGate == 2
  expect observed.tickets == (41, 42)
  expect observed.manifestCount == 3
  expect observed.manifestAvailable
}
