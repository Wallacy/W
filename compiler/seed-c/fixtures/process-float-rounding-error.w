// Expected exit: 1
// Expected stdout:
// Expected stderr:

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode throws NumericConversionError {
  let rounded = try i8(rounding: 256.0_f64, mode: .towardZero)
  return .success
}

entry(run)
