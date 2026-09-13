import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode {
  if args.count == 2 {
    print("Exactly two arguments")
    return .success
  } else {
    print("Argument count ${args.count}")
    return .success
  }
}

entry(run)
