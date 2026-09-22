// Expected output cases (argv => exit; stdout):
// [] => 7; "arguments-missing count=0 amount=17 over-limit=false\n"
// [""] => 0; "arguments-present count=1 amount=17 over-limit=false\n"
// ["alpha", "beta"] => 0; "arguments-present count=2 amount=17 over-limit=false\n"
// ["alpha", "beta", "gamma"] => 0; "arguments-present count=3 amount=17 over-limit=true\n"
import {
  Arguments as InputArgs,
  Context as InputContext,
  ExitCode as InputExit,
} from std.process

enum ArgumentState {
  unavailable
  observed(missing: Bool, overLimit: Bool, amount: i64)
}

fn buildArgumentState(missing: Bool, overLimit: Bool): ArgumentState {
  return .observed(amount: 17, overLimit: overLimit, missing: missing)
}

fn argumentStateIsMissing(state: ArgumentState): Bool {
  return switch state {
    case .unavailable: false
    case .observed(missing: let missing, overLimit: _, amount: _): missing
  }
}

fn argumentStateIsOverLimit(state: ArgumentState): Bool {
  return switch state {
    case .unavailable: false
    case .observed(missing: _, overLimit: let overLimit, amount: _): overLimit
  }
}

fn argumentStateAmount(state: ArgumentState): i64 {
  return switch state {
    case .unavailable: 0
    case .observed(missing: _, overLimit: _, amount: let amount): amount
  }
}

async fn dispatch(input: InputArgs, environment: InputContext): InputExit {
  let state = buildArgumentState(missing: input.isEmpty, overLimit: input.count > 2)
  let missing = argumentStateIsMissing(state: state)
  let overLimit = argumentStateIsOverLimit(state: state)
  let amount = argumentStateAmount(state: state)
  if missing {
    print("arguments-missing count=${input.count} amount=${amount} over-limit=${overLimit}")
    return .failure(7)
  } else {
    print("arguments-present count=${input.count} amount=${amount} over-limit=${overLimit}")
    return .success
  }
}

entry(dispatch)
