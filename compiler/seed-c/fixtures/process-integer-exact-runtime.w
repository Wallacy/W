// Expected with 0 user arguments: exit 0, stdout "Exact 0\n", stderr <empty>.
// Expected with 127 user arguments: exit 0, stdout "Exact 127\n", stderr <empty>.
// Expected with 128 user arguments: exit 1, stdout <empty>, stderr <empty>.

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode throws NumericConversionError {
  let narrowed = try i8(exactly: args.count)
  print("Exact ${narrowed}")
  return .success
}

entry(run)
