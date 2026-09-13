import std.process

async fn run(args: Arguments, ctx: Context): ExitCode {
  if 2 > args.count {
    print("Kitchen seats ${args.count} guests")
    return .success
  } else {
    print("Banquet seats ${args.count} guests")
    return .success
  }
}

entry(run)
