// Native-process contract oracle.
//
// The provider remains missing. This file fixes the source surface and keeps
// process authority in explicit Arguments and Context owners.

import process from std
import std.io

fn requestedCheck(args: ref process.Arguments): Bool {
  return args.contains("--check")
}

async fn runProcessOracle(
  args: process.Arguments,
  ctx: process.Context,
): process.ExitCode throws WriteAllError<IoError> {
  let output = ctx.stdout
  let mode = if requestedCheck(ref args) { "check" } else { "serve" }
  try await output.writeAll(text: "Process oracle mode: ${mode}.\n")
  return .success
}

entry LastLightProcessOracle(runProcessOracle)

test "portable process outcomes separate success and failure" {
  let success = process.ExitCode.success
  let failure = process.ExitCode.failure(2)
  expect success != failure
}

test "portable signals are explicit values" {
  let signals = [process.Signal.interrupt, process.Signal.terminate]
  expect signals.count == 2
  expect signals[0] != signals[1]
}

// Compile-fail assays:
// In a native-process entry body, the short projections are equivalent:
// let started = process.clock.now()
// let deadline = process.deadline
// let elapsed = process.clock.duration(from: started, to: process.clock.now())
// `process.clock` keeps identity, origin, authority, and lifetime from
// `process.context.clock`. `process.deadline` keeps value identity, origin,
// and lifetime from `process.context.deadline`; Deadline is not authority, so
// the short projection keeps `authorityExpanded: false`.
// Each short projection has the availability of its long projection.
// entry { serialize(process.context) }
// entry { service.send(process.context) }
// entry { let hidden = process.ctx }
// let libraryContext = process.context
