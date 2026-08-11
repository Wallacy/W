// Public byte I/O contracts.

export enum ReadStep {
  data(usize<(1...)>)
  end
}

// Positional reads use a separate step so a provider can report a short read
// without exposing a cursor.  A positive count is progress; .end is EOF.
export enum SnapshotReadStep {
  data(usize<(1...)>)
  end
}

export enum WriteStep {
  complete
  partial(usize<(1...)>)
}

export struct WriteAllError<Cause: Error>: Error {
  export cause: Cause
  export committed: usize
}

export protocol ByteSource<Failure: Error> {
  mut async fn read(
    appendTo destination: inout Bytes,
    named maximum: usize<(1...)>,
  ): ReadStep throws Failure
}

// SnapshotByteSource is a finite positional source. It has no cursor.
// The owner keeps byteCount and the logical content stable until release.
// Positional reads are safe to run in parallel; a provider may return a short
// positive read and the caller must issue another offset read. A decoder must
// consume the source and must not infer a path, provider, or ambient file.
export protocol SnapshotByteSource<Failure: Error> {
  fn byteCount(): u64

  async fn read(
    at offset: u64,
    appendTo destination: inout Bytes,
    named maximum: usize<(1...)>,
  ): SnapshotReadStep throws Failure
}

export protocol ByteSink<Failure: Error> {
  mut async fn write(source: view Bytes): WriteStep throws Failure

  mut async fn writeAll(
    source: view Bytes,
  ): () throws WriteAllError<Failure> {
    var committed: usize = 0

    while committed < source.count {
      let remaining: view Bytes = source[committed...]

      do {
        switch try await self.write(remaining) {
          case .complete:
            committed = source.count
          case .partial(let count):
            committed += count
        }
      } catch error {
        throw WriteAllError(cause: error, committed: committed)
      }
    }
  }

  mut async fn writeMany(
    _ sources: view Bytes...,
  ): WriteStep throws Failure {
    var index: usize = 0

    while index < sources.count {
      let source: view Bytes = sources[index]
      index += 1

      if source.isEmpty { continue }

      switch try await self.write(source) {
        case .partial(let count):
          return .partial(count)
        case .complete:
          while index < sources.count {
            if !sources[index].isEmpty {
              return .partial(source.count)
            }

            index += 1
          }

          return .complete
      }
    }

    return .complete
  }
}

// The provider is intentionally missing.  This marker keeps the intrinsic
// carrier visible in the SDK catalog without inventing a production runtime
// implementation or a public handle.
foreign intrinsic from "std.io@1" {
  type SnapshotByteSourceProviderMarker
}

test "write progress is distinct from completion" {
  let complete: WriteStep = .complete
  let partial: WriteStep = .partial(1)
  expect complete != partial
}

test "EOF is distinct from positive read progress" {
  let end: ReadStep = .end
  let data: ReadStep = .data(1)
  expect end != data
}
