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
// let clock = process.clock()
// let started = clock.now()
// let deadline = process.deadline
// let elapsed = clock.duration(from: started, to: clock.now())
// `process.clock()` keeps identity, origin, authority, and lifetime from
// `process.context.clock()`. `process.deadline` keeps value identity, origin,
// and lifetime from `process.context.deadline`; Deadline is not authority, so
// the short projection keeps `authorityExpanded: false`.
// Each short projection has the availability of its long projection.
// `process.clock()` selects the product default and may report
// `.unspecified`; it is nonthrowing when the Context capability is available.
// `try process.clock(hostSuspend: .included)` and the long
// `try process.context.clock(hostSuspend: .excluded)` select an active policy.
// Restaurant reservation leases require `.included`; kitchen active-work
// budgets require `.excluded`; unsupported or unspecified active requests are
// rejected before work starts.
// entry { serialize(process.context) }
// entry { service.send(process.context) }
// entry { let hidden = process.ctx }
// let libraryContext = process.context
