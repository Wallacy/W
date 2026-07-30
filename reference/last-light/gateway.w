// Host-independent command dispatch and HTTP adapter.

import std.http
import { Command } from restaurant.command
import { AppResponse } from restaurant.presentation
import { RestaurantApi, RestaurantError } from restaurant.restaurant
import { SimulationError, simulateShift } from restaurant.simulation
import { commandLimit } from restaurant.units

package const restaurantService = ServiceBinding<RestaurantApi>(name: "last-light")

export enum DispatchError: Error {
  restaurant(RestaurantError)
  simulation(SimulationError)
  unauthorizedCommand
}

export enum GatewayError: Error {
  decode(DecodeError)
  dispatch(DispatchError)
  response(ResponseError)
  service(ServiceFailure)
}

package enum HostAuthority {
  localOperator
  remoteClient
}

package const fn canDispatch(command: ref Command, authority: HostAuthority): Bool {
  return switch (authority, command) {
    case (.remoteClient, .shutdown): false
    case (_, _): true
  }
}

package async fn dispatch(
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

package async fn fetch(request: take http.Request, ctx: http.Context): http.Response throws GatewayError {
  let restaurant = try await ctx.services.get(restaurantService)
  let command = try await request.decodeJson<Command>(maximumBytes: commandLimit)
  let response = try await dispatch(
    take command,
    restaurant: restaurant,
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
