// Native handlers for CLI, a minimal ANSI TUI, signals, and HTTP.

import http from std
import std.io
import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
  Signal as ProcessSignal,
  SignalError as ProcessSignalError,
} from std.process
import { CommandError, decodeCommand } from command
import {
  DispatchError,
  GatewayError,
  HostAuthority,
  dispatch,
  fetch,
} from gateway
import { lastLight } from restaurant
import {
  RenderMode,
  renderResponse,
  requestsShutdown,
} from presentation
import { NativeTerminalBackend } from platform
import { nativeTerminalBackend } from platform.native
import { commandLimit } from units

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
  signal(ProcessSignalError)
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

async fn runConsole(ctx: ProcessContext, mode renderMode: RenderMode): ProcessExitCode throws AppError {
  let welcome = renderResponse(.help, mode: renderMode)
  try await ctx.stdout.write(welcome)

  for try await line in ctx.stdin.lines(maximumBytes: commandLimit) {
    let command = try decodeCommand(line)
    let response = try await dispatch(
      take command,
      restaurant: lastLight,
      authority: .localOperator,
    )
    let shouldStop = requestsShutdown(response)
    let output = renderResponse(take response, mode: renderMode)
    try await ctx.stdout.write(output)

    if shouldStop {
      return .success
    }
  }

  return .success
}

async fn runTui(args: ProcessArguments, ctx context: ProcessContext): ProcessExitCode throws AppError {
  let backend = terminalBackendLabel(nativeTerminalBackend())
  print("Opening the final terminal with the ${backend} adapter.")
  return try await runConsole(context, mode: .ansi)
}

async fn runServer(ctx: ProcessContext): ProcessExitCode throws AppError {
  try await http.serve(
    at: .loopback(port: 8_080),
    using: ctx.network,
    limits: nativeServerLimits,
    handler: fetch,
  )
  return .success
}

async fn runNative(
  args: ProcessArguments,
  ctx: ProcessContext,
): ProcessExitCode throws AppError {
  let shutdownSignals = try ctx.signals.register(
    [.interrupt, .terminate],
    handler: shutdown,
  )
  defer { shutdownSignals.cancel() }

  return switch try launchMode(args) {
    case .cli:
      try await runConsole(ctx, mode: .plain)
    case .tui:
      try await runTui(args, ctx: ctx)
    case .serve:
      try await runServer(ctx)
  }
}

async fn runTuiEntry(
  args: ProcessArguments,
  ctx: ProcessContext,
): ProcessExitCode throws AppError {
  let shutdownSignals = try ctx.signals.register(
    [.interrupt, .terminate],
    handler: shutdown,
  )
  defer { shutdownSignals.cancel() }
  return try await runTui(args, ctx: ctx)
}

async fn shutdown(signal: ProcessSignal, ctx: ProcessContext): () {
  print("Closing the final shift after ${signal}.")
  await ctx.services.drain(deadline: ctx.deadline)
}

entry(runNative)

entry LastLightTui(runTuiEntry)
