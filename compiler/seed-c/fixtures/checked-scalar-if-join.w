// Expected output cases (argv => exit; stdout):
// [] => 0; "Joined -1\n"
// ["x"] => 0; "Joined 0\n"
// ["x", "x"] => 2; ""
// ["x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x"] => 1; ""

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

fn choose(value: i8): i8 {
  return if value == 2_i8 { value * 127_i8 } else { value - 1_i8 }
}

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode throws NumericConversionError {
  let count = try i8(exactly: args.count)
  print("Joined ${choose(value: count)}")
  return .success
}

entry(run)
