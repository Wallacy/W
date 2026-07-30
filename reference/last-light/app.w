// Host bindings for CLI, a minimal ANSI TUI, line events, and HTTP.

import std.http
import std.io
import { Command, CommandError, decodeCommand } from restaurant.command
import {
  AppResponse,
  RenderMode,
  renderResponse,
  requestsShutdown,
} from restaurant.presentation
import { RestaurantApi, RestaurantError } from restaurant.restaurant
import { SimulationError, simulateShift } from restaurant.simulation
import { commandLimit } from restaurant.units

const restaurantService = ServiceBinding<RestaurantApi>(name: "last-light")

const nativeServerLimits = http.ServerLimits(
  activeRequests: 1_024,
  queuedRequests: 2_048,
  queuedBytes: 64<MiB>,
  connections: 8_192,
  message: http.MessageLimits(
    targetBytes: 16<KiB>,
    headerBytes: 64<KiB>,
    headerFields: 128,
    bodyBytes: commandLimit,
  ),
)

export enum AppError: Error {
  command(CommandError)
  conflictingLaunchModes
  decode(DecodeError)
  http(http.ServerError)
  io(IoError)
  restaurant(RestaurantError)
  response(ResponseError)
  service(ServiceFailure)
  simulation(SimulationError)
  unauthorizedCommand
}

enum HostAuthority {
  localOperator
  remoteClient
}

enum LaunchMode {
  cli
  tui
  serve
}

fn launchMode(args: ref ProcessArguments): LaunchMode throws AppError {
  let cli = args.contains("--cli")
  let tui = args.contains("--tui")
  let serve = args.contains("--serve")
  guard !(cli && tui) && !(cli && serve) && !(tui && serve)
    else throw .conflictingLaunchModes

  if tui {
    return .tui
  }

  if serve {
    return .serve
  }

  return .cli
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
): AppResponse throws AppError {
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

async fn runConsole(
  ctx: ProcessContext,
  mode: RenderMode,
): ExitCode throws AppError {
  let restaurant = try await ctx.services.get(restaurantService)
  let welcome = renderResponse(.help, mode: mode)
  try await ctx.stdout.write(welcome)

  for try await line in ctx.stdin.lines(maximumBytes: commandLimit) {
    let command = try decodeCommand(line)
    let response = try await dispatch(
      take command,
      restaurant: restaurant,
      authority: .localOperator,
    )
    let shouldStop = requestsShutdown(response)
    let output = renderResponse(take response, mode: mode)
    try await ctx.stdout.write(output)

    if shouldStop {
      return .success
    }
  }

  return .success
}

async fn runTui(args: ProcessArguments, ctx: ProcessContext): ExitCode throws AppError {
  return try await runConsole(ctx, mode: .ansi)
}

async fn runServer(ctx: ProcessContext): ExitCode throws AppError {
  try await http.serve(
    at: .loopback(port: 8_080),
    using: ctx.network,
    limits: nativeServerLimits,
    handler: fetch,
  )
  return .success
}

async fn runNative(args: ProcessArguments, ctx: ProcessContext): ExitCode throws AppError {
  return switch try launchMode(args) {
    case .cli:
      try await runConsole(ctx, mode: .plain)
    case .tui:
      try await runConsole(ctx, mode: .ansi)
    case .serve:
      try await runServer(ctx)
  }
}

async fn readCommand(line: String, ctx: CliContext): () throws AppError {
  let restaurant = try await ctx.services.get(restaurantService)
  let command = try decodeCommand(line)
  let response = try await dispatch(
    take command,
    restaurant: restaurant,
    authority: .localOperator,
  )
  let output = renderResponse(take response, mode: .plain)
  try await ctx.stdout.write(output)
}

package async fn fetch(
  request: take http.Request,
  ctx: http.Context,
): http.Response throws AppError {
  let restaurant = try await ctx.services.get(restaurantService)
  let command = try await request.decodeJson<Command>(
    maximumBytes: commandLimit,
  )
  let response = try await dispatch(
    take command,
    restaurant: restaurant,
    authority: .remoteClient,
  )
  return try http.Response.json(response)
}

async fn shutdown(signal: ProcessSignal, ctx: ProcessContext): () {
  print("Closing the final shift after ${signal}.")
  await ctx.services.drain(deadline: ctx.deadline)
}

entry(runNative) {
  process.signal = shutdown
}

entry LastLightTui(runTui)

entry LastLightLineHost {
  process.stdinLine = readCommand
}

test "a remote command cannot stop the process" for canDispatch {
  let shutdown: Command = .shutdown
  let menu: Command = .menu

  expect canDispatch(shutdown, authority: .localOperator)
  expect !canDispatch(shutdown, authority: .remoteClient)
  expect canDispatch(menu, authority: .remoteClient)
}
