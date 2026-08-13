// ATOM1 current composition witness for the Last Light restaurant.

module atom1_current_composition

import atomic from std
import { SnapshotCell } from std.sync

export enum SignState {
  dark
  announcing
  closed
}

export struct SignEpochWord: Duplicable {
  state: SignState
  generation: u32
}

export object ScalarSignEpoch {
  var atomic packed: u64 = 0

  export init() {}

  fn encode(_ state: SignState, named generation: u32): u64 {
    return (u64(generation) << 2) | u64(state.code)
  }

  fn decode(_ packed: u64): SignEpochWord? {
    let stateCode = packed & 3
    guard stateCode < 3 else return .none
    let generationValue = packed >> 2
    guard generationValue <= u64(0xffffffff) else return .none
    return SignEpochWord(
      state: SignState.from(code: u8(stateCode)),
      generation: u32(generationValue),
    )
  }

  fn publish(_ next: SignEpochWord) {
    let encoded = encode(next.state, generation: next.generation)
    packed.store<.release>(encoded)
  }

  fn observe(): SignEpochWord? {
    return decode(packed.load<.acquire>())
  }

  fn publishRaw(_ encoded: u64) {
    packed.store<.release>(encoded)
  }
}

export fn publishOwnerLocal(
  state: inout SignEpochWord,
  named next: SignEpochWord,
): SignEpochWord {
  state = next
  return state
}

fn assignOnApologyDomain(
  state: inout SignEpochWord,
  named next: SignEpochWord,
): SignEpochWord {
  state = next
  return state
}

export async fn publishOnApologyDomain(
  state: inout SignEpochWord,
  named next: SignEpochWord,
): SignEpochWord {
  spawn<.apology> let published = assignOnApologyDomain(
    inout state,
    next: next,
  )
  return await published
}

export fn publishWithLock(
  state: shared SignEpochWord,
  named next: SignEpochWord,
): SignEpochWord {
  return lock state as value {
    value = next
    copy value
  }
}

export object MenuVersions {
  snapshots: SnapshotCell<SignEpochWord>

  export init(_ initial: take SignEpochWord) {
    self.snapshots = SnapshotCell(take initial)
  }

  fn publish(_ next: take SignEpochWord) {
    snapshots.publish(take next)
  }

  fn snapshot(): SignEpochWord {
    return snapshots.snapshot()
  }
}
