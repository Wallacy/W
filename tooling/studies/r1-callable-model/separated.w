// R1 Last Light callable-model study source.

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

alias ErasedWelcomeRoute = any fn(Arrival): Welcome

fn standardWelcome(arrival: Arrival = Arrival(orderId: 0)): Welcome {
  return Welcome(orderId: arrival.orderId, gate: 1)
}

fn route(named gate: usize): any fn(Arrival): Welcome {
  return capture(copy gate) (arrival) => Welcome(
    orderId: arrival.orderId,
    gate: gate,
  )
}

fn ticketSequence(named initial: usize): some mut fn(): usize {
  var next = initial

  return capture(take next) () => {
    next += 1
    return next
  }
}

fn finalManifest(orderIds: take Array<u64>): some take fn(): Array<u64> {
  return capture(take orderIds) () => take orderIds
}

fn recoverableRoute(
  gate: usize,
  memory: ref Allocator,
): ErasedWelcomeRoute throws AllocationError {
  let concrete = capture(copy gate) (arrival) => Welcome(
    orderId: arrival.orderId,
    gate: gate,
  )
  return try erase(take concrete, using: memory)
}

fn observeCallableModel(
  named gate: usize,
  named initial: usize,
  named orderIds: take Array<u64>,
): CallableObservation {
  let greeter: fn(Arrival): Welcome = standardWelcome
  let first = greeter(Arrival(orderId: 42))

  let handler = route(gate: gate)
  let routed = handler(Arrival(orderId: 43))

  var next = ticketSequence(initial: initial)
  let firstTicket = next()
  let secondTicket = next()

  let manifest = finalManifest(take orderIds)
  let restored = (take manifest)()

  return CallableObservation(
    firstGate: first.gate,
    routedGate: routed.gate,
    tickets: (firstTicket, secondTicket),
    manifestCount: restored.count,
    manifestAvailable: false,
  )
}

test "separated callables expose representation and ownership" for observeCallableModel {
  let observed = observeCallableModel(gate: 2, initial: 40, orderIds: [7, 8, 9])
  expect observed.firstGate == 1
  expect observed.routedGate == 2
  expect observed.tickets == (41, 42)
  expect observed.manifestCount == 3
  expect !observed.manifestAvailable
}
