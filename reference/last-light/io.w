// Bounded byte I/O for the Archive of Extinct Recipes.

import * from std.fs
import * from std.io

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

export async fn writeExtinctRecipeFrame<
  Failure: Error,
  Destination: ByteSink<Failure>,
>(
  destination: inout Destination,
  header: view Bytes,
  recipe: view Bytes,
  checksum: view Bytes,
): WriteStep throws Failure {
  return try await destination.writeMany(header, recipe, checksum)
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

export async fn readRecipeEnvelope<
  Failure: Error,
  Source: ByteSource<Failure>,
>(
  source: inout Source,
  batch: inout ReadBatch,
): ScatterReadStep throws Failure {
  return try await readMany(
    from: inout source,
    scatterInto: inout batch,
  )
}

export fn recipeTransferPlan(
  offset: FileOffset,
  byteCount: u64,
): TransferPlan throws TransferPlanError {
  return try TransferPlan(
    at: offset,
    maximumBytes: byteCount,
    chunkBytes: 64 * 1_024,
  )
}

export async fn transferRecipeArchiveStep<
  Failure: Error,
  Destination: ByteSink<Failure>,
>(
  archive: ref FileSnapshot,
  destination: inout Destination,
  plan: inout TransferPlan,
): TransferStep throws TransferError<IoError, Failure> {
  return try await transfer(
    from: ref archive,
    to: inout destination,
    using: inout plan,
  )
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
// let pending = async source.read(appendTo: inout scratch, maximum: 4096)
// scratch.reset()                              // `pending` still borrows `scratch`.
// let write = async output.writeMany(prefix, payloadOwner, checksum)
// payloadOwner.reset()                         // `write` still borrows the payload.
// let scatter = async readMany(from: inout source, scatterInto: inout batch)
// batch.reset()                                // `scatter` still borrows the batch.
// let direct = async transfer(
//   from: ref archive,
//   to: inout output,
//   using: inout plan,
// )
// plan.pendingBytes                            // `direct` still borrows the plan.
