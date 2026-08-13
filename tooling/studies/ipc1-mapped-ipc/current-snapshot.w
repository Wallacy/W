// IPC1 current baseline: bounded snapshots and typed wire handoff.

module ipc1.current.snapshot

import * from std.io

export struct Ipc1MappedIpcSnapshot: Duplicable {
  generation: u64
  bytes: Bytes
}

export async fn Ipc1MappedIpc(
  source: take SnapshotByteSource<IoError>,
): Ipc1MappedIpcSnapshot throws IoError {
  let snapshot = try await source.snapshot(maximumBytes: 1_048_576)
  return Ipc1MappedIpcSnapshot(
    generation: 1,
    bytes: snapshot.bytes,
  )
}
