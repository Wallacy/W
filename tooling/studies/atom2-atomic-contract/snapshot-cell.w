// ATOM2 current read-heavy route.

module atom2_snapshot_cell

import { SnapshotCell } from std.sync

export struct MenuSnapshot: Duplicable {
  let revision: u64
  let courses: Array<String>
}

export object MenuPublication {
  let snapshots: SnapshotCell<MenuSnapshot>

  export init(_ initial: take MenuSnapshot) {
    self.snapshots = SnapshotCell(take initial)
  }

  fn courseCount(): usize {
    return snapshots.read((menu: ref MenuSnapshot) => menu.courses.count)
  }
}
