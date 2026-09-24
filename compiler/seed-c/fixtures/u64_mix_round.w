// Expected output cases (user arguments => exit; stdout; stderr):
// [] => 0; "Mix 16743387770561346575\n"; ""
// ["x"] => 0; "Mix 553619412775969103\n"; ""
// ["alpha", "beta", "gamma"] => 0; "Mix 5608831001354178255\n"; ""

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

fn rotateAndFold(value: u64): u64 {
  let rotated = u64.rotatedLeft(value, 23_u64)
  let shifted = u64.logicalShiftRight(rotated, 17_u64)
  return rotated ^ shifted
}

fn mixRound(state: u64, lane: u64): u64 {
  let added = u64.wrappingAdd(state, lane)
  let folded = rotateAndFold(value: added)
  return u64.wrappingMultiply(folded, 0x9e3779b185ebca87_u64)
}

async fn run(args: ProcessArguments, ctx: ProcessContext): ProcessExitCode throws NumericConversionError {
  let lane = try u64(exactly: args.count)
  print("Mix ${mixRound(state: 0x0206f2a26db56cfd_u64, lane: lane)}")
  return .success
}

entry(run)
