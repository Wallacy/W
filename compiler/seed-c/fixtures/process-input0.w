import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode {
  if args.isEmpty {
    print("missing")
    return .failure(2)
  } else {
    print("received")
    return .success
  }
}

entry(run)
