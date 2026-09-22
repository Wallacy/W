// Expected exit: 0
// Expected stdout:
// Rounded 2
// Expected stderr:

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode throws NumericConversionError {
  let rounded = try i8(rounding: 2.5_f64, mode: .nearestEven)
  print("Rounded ${rounded}")
  return .success
}

entry(run)
