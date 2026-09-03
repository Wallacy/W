// R1 alternative: callable behavior is expressed with nominal protocols.

protocol Callable<Input, Output> {
  fn call(_ input: Input): Output
}

protocol MutableCallable<Input, Output> {
  mut fn call(_ input: Input): Output
}

protocol ConsumingCallable<Input, Output> {
  take fn call(_ input: Input): Output
}

struct Arrival {
  let orderId: u64
}

struct Welcome {
  let orderId: u64
  let gate: usize
}

struct CallableObservation {
  let firstGate: usize
  let routedGate: usize
  let tickets: (usize, usize)
  let manifestCount: usize
  let manifestAvailable: Bool
}

alias ErasedWelcomeRoute = any Callable<Arrival, Welcome>

fn standardWelcome(_ arrival: Arrival = Arrival(orderId: 0)): Welcome {
  return Welcome(orderId: arrival.orderId, gate: 1)
}

fn route(gate: usize): any Callable<Arrival, Welcome> {
  return <[copy gate]> (arrival) => Welcome(
    orderId: arrival.orderId,
    gate: gate,
  )
}

fn ticketSequence(initial: usize): some MutableCallable<(), usize> {
  var next = initial

  return <[take next]> () => {
    next += 1
    return next
  }
}

fn finalManifest(_ orderIds: take Array<u64>): some ConsumingCallable<(), Array<u64>> {
  return <[take orderIds]> () => take orderIds
}

fn recoverableRoute(
  _ gate: usize,
  _ memory: ref Allocator,
): ErasedWelcomeRoute throws AllocationError {
  let concrete: some Callable<Arrival, Welcome> =
    (arrival) => Welcome(orderId: arrival.orderId, gate: gate)
  return try erase(take concrete, allocator: memory)
}

fn observeCallableModel(
  gate: usize,
  initial: usize,
  orderIds: take Array<u64>,
): CallableObservation {
  let greeter: some Callable<Arrival, Welcome> = (arrival) => standardWelcome(arrival)
  let first = greeter.call(Arrival(orderId: 42))

  let handler = route(gate: gate)
  let routed = handler.call(Arrival(orderId: 43))

  var next = ticketSequence(initial: initial)
  let firstTicket = next.call(())
  let secondTicket = next.call(())

  let manifest = finalManifest(take orderIds)
  let restored = (take manifest).call(())

  return CallableObservation(
    firstGate: first.gate,
    routedGate: routed.gate,
    tickets: (firstTicket, secondTicket),
    manifestCount: restored.count,
    manifestAvailable: false,
  )
}

test "protocol callables expose behavior through nominal requirements" for observeCallableModel {
  let observed = observeCallableModel(gate: 2, initial: 40, orderIds: [7, 8, 9])
  expect observed.firstGate == 1
  expect observed.routedGate == 2
  expect observed.tickets == (41, 42)
  expect observed.manifestCount == 3
  expect !observed.manifestAvailable
}
