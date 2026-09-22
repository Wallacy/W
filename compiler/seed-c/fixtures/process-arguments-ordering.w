// Expected output cases (argv => exit; stdout):
// [] => 0; "Argument mode compact: count=0\n"
// [""] => 0; "Argument mode compact: count=1\n"
// ["alpha", "beta"] => 0; "Argument mode extended: count=2\n"
// ["alpha", "beta", "gamma"] => 0; "Argument mode extended: count=3\n"
import std.process

async fn run(args: Arguments, ctx: Context): ExitCode {
  if args.count < 2 {
    print("Argument mode compact: count=${args.count}")
    return .success
  } else {
    print("Argument mode extended: count=${args.count}")
    return .success
  }
}

entry(run)
