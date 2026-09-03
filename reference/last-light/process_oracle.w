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
  let mode = if requestedCheck(args: ref args) { "check" } else { "serve" }
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

// Contextual-execution assays:
// `execution` is target-neutral and `std.process` remains an explicit host API.
// let clock = execution.clock()
// let started = clock.now()
// let deadline = execution.deadline
// let elapsed = clock.duration(from: started, to: clock.now())
// `execution.clock()` keeps identity, origin, authority, and lifetime from the
// host-granted owner. `execution.deadline` keeps value identity, origin, and
// lifetime without expanding authority. Each projection is availability-gated.
// `execution.clock()` selects the product default and may report
// `.unspecified`; it is nonthrowing when the Context capability is available.
// `try execution.clock(hostSuspend: .included)` selects an active policy.
// Reusable code with an explicit Context uses `ctx.clock(...)`.
// Restaurant reservation leases require `.included`; kitchen active-work
// budgets require `.excluded`; unsupported or unspecified active requests are
// rejected before work starts.
// entry { serialize(execution) }
// entry { service.send(execution) }
// let moduleClock = execution.clock()
