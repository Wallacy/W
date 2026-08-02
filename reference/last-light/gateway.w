// Host-independent command dispatch and HTTP adapter.

import http from std
import { Command } from command
import { AppResponse } from presentation
import { RestaurantApi, RestaurantError, lastLight } from restaurant
import { SimulationError, simulateShift } from simulation
import { commandLimit } from units

enum DispatchError: Error {
  restaurant(RestaurantError)
  simulation(SimulationError)
  unauthorizedCommand
}

enum GatewayError: Error {
  decode(DecodeError)
  dispatch(DispatchError)
  response(ResponseError)
  service(ServiceFailure)
}

enum HostAuthority {
  localOperator
  remoteClient
}

const fn canDispatch(command: ref Command, authority: HostAuthority): Bool {
  return switch (authority, command) {
    case (.remoteClient, .shutdown): false
    case (_, _): true
  }
}

async fn dispatch(
  command: take Command,
  restaurant: ref ServiceRef<RestaurantApi>,
  authority: HostAuthority,
): AppResponse throws DispatchError {
  guard canDispatch(command, authority: authority) else throw .unauthorizedCommand

  return switch command {
    case .help:
      .help
    case .menu:
      .menu(try await restaurant.menu())
    case .place(let order):
      .placed(try await restaurant.place(take order))
    case .status(let orderId):
      .status(orderId, try await restaurant.status(orderId))
    case .cancel(let orderId):
      .cancelled(orderId, try await restaurant.cancel(orderId))
    case .dashboard:
      .dashboard(await restaurant.snapshot())
    case .simulate(let profile):
      .simulation(try simulateShift(profile))
    case .shutdown:
      .shuttingDown
  }
}

async fn fetch(request: take http.Request, ctx: http.Context): http.Response throws GatewayError {
  let command = try await request.decodeJson<Command>(maximumBytes: commandLimit)
  let response = try await dispatch(
    take command,
    restaurant: lastLight,
    authority: .remoteClient,
  )
  return try http.Response.json(response)
}

test "a remote command cannot stop the process" for canDispatch {
  let shutdown: Command = .shutdown
  let menu: Command = .menu

  expect canDispatch(shutdown, authority: .localOperator)
  expect !canDispatch(shutdown, authority: .remoteClient)
  expect canDispatch(menu, authority: .remoteClient)
}

export {
  DispatchError,
  GatewayError,
  fetch,
}
