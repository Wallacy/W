// Host bindings for CLI, a minimal ANSI TUI, line events, and HTTP.

import std.http
import std.io
import { Command, CommandError, decodeCommand } from restaurant.command
import {
  DispatchError,
  GatewayError,
  HostAuthority,
  dispatch,
  fetch,
  restaurantService,
} from restaurant.gateway
import {
  RenderMode,
  renderResponse,
  requestsShutdown,
} from restaurant.presentation
import { NativeTerminalBackend } from restaurant.platform
import { nativeTerminalBackend } from restaurant.platform.native
import { commandLimit } from restaurant.units

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
  dispatch(DispatchError)
  gateway(GatewayError)
  http(http.ServerError)
  io(IoError)
  service(ServiceFailure)
}

enum LaunchMode {
  cli
  tui
  serve
}

const fn terminalBackendLabel(backend: NativeTerminalBackend): String {
  return switch backend {
    case .ansi: "ANSI"
    case .windowsVirtualTerminal: "Windows Virtual Terminal"
  }
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

async fn runConsole(ctx: ProcessContext, mode: RenderMode): ExitCode throws AppError {
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
  let backend = terminalBackendLabel(nativeTerminalBackend())
  print("Opening the final terminal with the ${backend} adapter.")
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
      try await runTui(args, ctx: ctx)
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
