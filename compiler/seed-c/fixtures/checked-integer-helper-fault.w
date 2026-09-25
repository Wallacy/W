// Expected cases (user arguments => exit; stdout; stderr):
// [] => 0; "Begin 255\n"; ""
// ["x"] => 2; ""; ""
// 256 user arguments => 1; ""; ""
// Isolated checked-addition helper witness; conversion precedence remains in
// process-fixed-integer-arithmetic.w.

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

fn checkedOffset(value: u8): u8 {
  return value + 255_u8
}

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode throws NumericConversionError {
  let count = try u8(exactly: args.count)
  print("Begin ${checkedOffset(value: count)}")
  return .success
}

entry(run)
