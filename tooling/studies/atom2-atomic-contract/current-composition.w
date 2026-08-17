// ATOM2 current composition witness.

module atom2_current_composition

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

export object CurrentComposition {
  // Existing scalar/snapshot composition stays allocation-free per atomic operation.
  var atomic epoch: u64 = 0
  snapshots: SnapshotCell<SignEpochWord>

  export init(_ initial: take SignEpochWord) {
    self.snapshots = SnapshotCell(take initial)
  }

  fn publishScalar(_ next: SignEpochWord) {
    epoch.store<.release>(u64(next.generation))
  }

  fn publishSnapshot(_ next: take SignEpochWord) {
    snapshots.publish(take next)
  }
}
