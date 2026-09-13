import std.process

async fn run(args: Arguments, ctx: Context): ExitCode {
  if args.count == 2 {
    print("Exactly two arguments")
    return .success
  } else {
    print("Argument count ${args.count}")
    return .success
  }
}

entry(run)
