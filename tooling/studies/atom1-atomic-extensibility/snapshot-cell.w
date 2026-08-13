// ATOM1 current route for read-heavy menu state.

module atom1_snapshot_cell

import { SnapshotCell } from std.sync

export struct MenuSnapshot: Duplicable {
  revision: u64
  courses: Array<String>
}

export object MenuPublication {
  snapshots: SnapshotCell<MenuSnapshot>

  export init(_ initial: take MenuSnapshot) {
    self.snapshots = SnapshotCell(take initial)
  }

  fn courseCount(): usize {
    return snapshots.read((menu: ref MenuSnapshot) => menu.courses.count)
  }

  fn publish(_ next: take MenuSnapshot) {
    snapshots.publish(take next)
  }
}
