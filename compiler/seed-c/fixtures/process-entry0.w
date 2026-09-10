import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process
async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode { return .success }
entry(run)
