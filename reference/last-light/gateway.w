// Host-independent command dispatch and HTTP adapter.

import http from std
import json from std.json
import { Command } from command
import { AppResponse } from presentation
import {
  AppResponseDocument,
  CommandDocument,
  CommandDocumentError,
  ProblemCode,
  problemResponse,
} from http_documents
import { RestaurantApi, RestaurantError, lastLight } from restaurant
import { SimulationError, simulateShift } from simulation
import { commandLimit } from units

enum DispatchError: Error {
  restaurant(RestaurantError)
  simulation(SimulationError)
  unauthorizedCommand
}

enum GatewayError: Error {
  decode(http.BodyDecodeError<json.DecodeError>)
  schema(CommandDocumentError)
  dispatch(DispatchError)
  response(http.ResponseError)
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

fn gatewayProblemCode(error: ref GatewayError): ProblemCode? {
  return switch error {
    case .decode(_): .some(.malformedJson)
    case .schema(_): .some(.invalidCommand)
    case .dispatch(.unauthorizedCommand):
      .some(.forbiddenShutdown)
    case _: .none
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

async fn decodeCommandBody(
  request: take http.Request,
): Command throws GatewayError {
  // The consuming receiver makes a second body read a compile-time ownership error.
  let document: CommandDocument
  do {
    document = try await (take request).json<CommandDocument>(maximumBytes: commandLimit)
  } catch error {
    throw .decode(error)
  }
  do {
    return try (take document).command()
  } catch error {
    throw .schema(error)
  }
}

async fn handle(
  request: take http.Request,
  ctx: http.Context,
): AppResponse throws GatewayError {
  let command = try await decodeCommandBody(take request)
  return try await dispatch(
    take command,
    restaurant: lastLight,
    authority: .remoteClient,
  )
}

fn gatewayProblemResponse(code: ProblemCode): http.Response throws GatewayError {
  do {
    return try problemResponse(code: code, maximumBytes: commandLimit)
  } catch error {
    throw .response(error)
  }
}

// Compile-surface oracle only. The production route does not clone its request.
// Runtime evidence waits for std.http@1 and its bounded body tee.
fn boundedRequestCloneCompileOracle(
  request: take http.Request,
): (http.Request, http.Request) throws http.BodyCloneError {
  return try (take request).clone(maximumBufferedBytes: commandLimit)
}

async fn fetch(request: take http.Request, ctx: http.Context): http.Response throws GatewayError {
  guard request.url.pathname == "/commands" else {
    return try http.Response(status: http.StatusCode.notFound)
  }

  do {
    let response = try await handle(take request, ctx)
    let document = AppResponseDocument(response: ref response)
    do {
      return try http.Response.json(value: ref document, maximumBytes: commandLimit)
    } catch error {
      throw .response(error)
    }
  } catch error {
    switch error {
      case .decode(_):
        return try gatewayProblemResponse(.malformedJson)
      case .schema(_):
        return try gatewayProblemResponse(.invalidCommand)
      case .dispatch(.unauthorizedCommand):
        return try gatewayProblemResponse(.forbiddenShutdown)
      case _:
        throw error
    }
  }
}

test "a remote command cannot stop the process" for canDispatch {
  let shutdown: Command = .shutdown
  let menu: Command = .menu

  expect canDispatch(shutdown, authority: .localOperator)
  expect !canDispatch(shutdown, authority: .remoteClient)
  expect canDispatch(menu, authority: .remoteClient)
}

test "gateway maps only documented boundary failures" {
  let malformed = GatewayError.decode(.codec(.invalidNumber(location: json.Location(byteOffset: 0))))
  let invalid = GatewayError.schema(.invalidKind(.kind))
  let forbidden = GatewayError.dispatch(.unauthorizedCommand)
  let unclassified = GatewayError.dispatch(.restaurant(.domain(.overflow)))

  expect gatewayProblemCode(ref malformed) == .some(.malformedJson)
  expect gatewayProblemCode(ref invalid) == .some(.invalidCommand)
  expect gatewayProblemCode(ref forbidden) == .some(.forbiddenShutdown)
  expect gatewayProblemCode(ref unclassified) == .none
}

export {
  DispatchError,
  GatewayError,
  fetch,
}
