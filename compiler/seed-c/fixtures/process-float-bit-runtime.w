// Expected:
// args=[] -> exit 0; stdout "Bits 0\n"; stderr ""
// args=["x"] -> exit 0; stdout "Bits 1\n"; stderr ""

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode throws NumericConversionError {
  let bits: u64 = try u64(exactly: args.count)
  let value: f64 = f64.fromBits(bits)
  let roundTrip: u64 = value.toBits()
  print("Bits ${roundTrip}")
  return .success
}

entry(run)
