// Expected exit: 1
// Expected stdout:
// Expected stderr:

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode throws NumericConversionError {
  let rounded = try i8(rounding: f64.fromBits(0x7ff8000000000000_u64), mode: .towardZero)
  return .success
}

entry(run)
