// Expected exit: 1
// Expected stdout:
// Expected stderr:

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode throws NumericConversionError {
  let narrowed = try i8(exactly: 128)
  return .success
}

entry(run)
