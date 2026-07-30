// Bounded byte I/O for the Archive of Extinct Recipes.

import std.fs
import std.io

export enum RelayStep<ReadFailure: Error, WriteFailure: Error> {
  end
  copied(usize<(1...)>)
  readFailed(ReadFailure)
  writeFailed(
    cause: WriteAllError<WriteFailure>,
    sourceAdvanced: usize<(1...)>,
    payload: Bytes,
  )
}

export async fn relayRecipeChunk<
  ReadFailure: Error,
  WriteFailure: Error,
  Source: ByteSource<ReadFailure>,
  Destination: ByteSink<WriteFailure>,
>(
  source: inout Source,
  destination: inout Destination,
  chunkBytes: usize<(1...)>,
): RelayStep<ReadFailure, WriteFailure> {
  var scratch = Bytes()
  scratch.reserve(minimumCapacity: chunkBytes)

  var step: ReadStep

  do {
    step = try await source.read(
      appendTo: inout scratch,
      maximum: chunkBytes,
    )
  } catch error {
    return .readFailed(error)
  }

  switch step {
    case .data(let count):
      let chunk: view Bytes = scratch[0..<count]

      do {
        try await destination.writeAll(chunk)
        return .copied(count)
      } catch error {
        return .writeFailed(
          cause: error,
          sourceAdvanced: count,
          payload: take scratch,
        )
      }
    case .end:
      return .end
  }
}

export async fn readRecipeBlock(
  files: ref FileSystem,
  path: ref Path,
  offset: FileOffset,
  maximum: usize<(1...)>,
): Bytes throws IoError {
  let archive = try await files.open<[.read]>(path)
  var payload = Bytes()

  let step = try await archive.read(
    at: offset,
    appendTo: inout payload,
    maximum: maximum,
  )

  switch step {
    case .data(let count):
      expect count == payload.count
    case .end:
      expect payload.isEmpty
  }

  return payload
}

export async fn countBorrowedChunks<E: Error>(
  source: take some Stream<view Bytes, E>,
): usize throws E {
  var chunks = take source
  var count = 0_usize

  for try await chunk in chunks {
    if !chunk.isEmpty { count += 1 }
  }

  return count
}

// Compile-fail assays:
// let _ = Channel<view Bytes>.open(capacity: 1) // A view cannot leave its owner.
// async let pending = source.read(appendTo: inout scratch, maximum: 4096)
// scratch.reset()                              // `pending` still borrows `scratch`.
