// Callable routes for the maître d' at the end of the shift.

import iec from std
import { GuestCount, OrderId } from domain
import * from std.memory

export struct Arrival {
  let orderId: OrderId
  let guests: GuestCount
}

export struct Welcome {
  let orderId: OrderId
  let gate: usize
}

fn standardWelcome(arrival: ref Arrival): Welcome {
  return Welcome(orderId: arrival.orderId, gate: 1)
}

export fn welcome(
  arrival: ref Arrival,
  using greeter: some fn(ref Arrival): Welcome,
): Welcome {
  return greeter(arrival)
}

export struct WelcomeRoute {
  let handler: any fn(Arrival): Welcome
}

export fn route(
  handler: take any fn(Arrival): Welcome,
): WelcomeRoute {
  return WelcomeRoute(handler: take handler)
}

export fn recoverableRoute(
  gate entryGate: usize,
  allocator memory: ref Allocator,
): WelcomeRoute throws AllocationError {
  let concrete = <[copy entryGate]> (arrival) => Welcome(
    orderId: arrival.orderId,
    gate: entryGate,
  )
  let handler: any fn(Arrival): Welcome =
    try erase(take concrete, allocator: allocator)
  return WelcomeRoute(handler: take handler)
}

export fn ticketSequence(
  initial: usize,
): some mut fn(): usize {
  var next = initial

  return <[take next]> () => {
    next += 1
    return next
  }
}

export fn finalManifest(
  orderIds: take Array<OrderId>,
): some take fn(): Array<OrderId> {
  return <[take orderIds]> () => take orderIds
}

test "a thin function pointer keeps calls positional" for standardWelcome {
  let arrival = Arrival(orderId: 42, guests: try GuestCount(2))
  let greeter: fn(ref Arrival): Welcome = standardWelcome
  let result = greeter(arrival)

  expect result.orderId == 42
  expect result.gate == 1
}

test "an opaque callable stays specialized" for welcome {
  let arrival = Arrival(orderId: 43, guests: try GuestCount(3))
  let result = welcome(arrival: arrival, using: standardWelcome)

  expect result.orderId == 43
}

test "an erased callable owns its invocation environment" for route {
  let gate = 2
  let handler: any fn(Arrival): Welcome =
    <[copy gate]> (arrival) => Welcome(orderId: arrival.orderId, gate: gate)
  let selected = route(handler: take handler)
  let result = selected.handler(Arrival(orderId: 44, guests: try GuestCount(4)))

  expect result.gate == 2
}

test "explicit erasure exposes allocation recovery" for recoverableRoute {
  allocator memory: .fixed<capacity: 1<iec.KiB>> {
    let selected = try recoverableRoute(gate: 3, allocator: ref memory)
    let result = selected.handler(Arrival(orderId: 45, guests: try GuestCount(5)))

    expect result.gate == 3
  }
}

test "callable modes expose mutation and consumption" {
  var nextTicket = ticketSequence(initial: 40)
  expect nextTicket() == 41
  expect nextTicket() == 42

  let manifest = finalManifest(orderIds: [7, 8, 9])
  let restored = (take manifest)()
  expect restored.count == 3
}
