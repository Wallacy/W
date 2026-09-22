// Expected with 0 user arguments: exit 0, stdout "Arithmetic 0/4\n", stderr <empty>.
// Expected with 127 user arguments: exit 0, stdout "Arithmetic 127/10\n", stderr <empty>.
// Expected with 128 user arguments: exit 1, stdout <empty>, stderr <empty>.

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode throws NumericConversionError {
  let narrowed = try i8(exactly: args.count)
  print("Arithmetic ${narrowed}/${narrowed % 11_i8 * 2_i8 / 2_i8 + 7_i8 - 3_i8}")
  return .success
}

entry(run)
