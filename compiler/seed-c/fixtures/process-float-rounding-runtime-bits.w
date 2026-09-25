// Expected:
// args=[] -> exit 0; stdout "Rounded 42\n"; stderr ""
// args=["x"] -> exit 1; stdout ""; stderr ""
// args=["x", "y"] -> exit 1; stdout ""; stderr ""

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode throws NumericConversionError {
  let rounded = try i8(rounding: f64.fromBits(
    if args.count == 0 { 0x4045600000000000_u64 } else { 0x7ff8000000000000_u64 }),
    mode: .towardZero)
  print("Rounded ${rounded}")
  return .success
}

entry(run)
