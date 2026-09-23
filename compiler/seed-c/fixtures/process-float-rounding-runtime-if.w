// Expected:
// args=[] -> exit 0; stdout "Rounded 2\n"; stderr ""
// args=["x"] -> exit 0; stdout "Rounded 4\n"; stderr ""

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode throws NumericConversionError {
  let rounded = try i8(rounding: if args.count == 0 { 2.5_f64 } else { 3.5_f64 }, mode: .nearestEven)
  print("Rounded ${rounded}")
  return .success
}

entry(run)
