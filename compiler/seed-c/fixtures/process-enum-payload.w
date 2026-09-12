import {
  Arguments as InputArgs,
  Context as InputContext,
  ExitCode as InputExit,
} from std.process

enum AdmissionState {
  unavailable
  observed(missing: Bool, amount: i64)
}

fn buildAdmission(missing: Bool): AdmissionState {
  return .observed(amount: 17, missing: missing)
}

fn admissionIsMissing(state: AdmissionState): Bool {
  return switch state {
    case .unavailable: false
    case .observed(missing: let missing, amount: _): missing
  }
}

async fn dispatch(input: InputArgs, environment: InputContext): InputExit {
  let state = buildAdmission(missing: input.isEmpty)
  let missing = admissionIsMissing(state: state)
  let repeated = input.isEmpty
  if missing {
    print("enum-missing ${repeated}")
    return .failure(7)
  } else {
    print("enum-received ${repeated}")
    return .success
  }
}

entry(dispatch)
