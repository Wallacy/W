// Portable readable-stream carrier.
//
// ReadableStream owns the only cursor and conforms directly to Stream. The
// intrinsic is a private adapter seam. It cannot grant authority or create an
// unbounded queue. A conforming provider keeps at most one upstream pull in
// flight, delegates explicit cancellation to Stream.cancel(), and belongs to
// the structured scope that owns the source.

import * from std.io

export enum ReadableStreamUseError: Error {
  disturbed
  locked
}

// The handle is opaque because it can contain any concrete Stream or
// ByteSource. It carries a private type witness for the exact Item, Failure,
// and source specialization. The provider may use an erasure box, small-buffer
// storage, or monomorphized lowering. None of these choices creates public
// `any` or permits a dynamic Item/Failure mismatch.
//
// A consuming operation marks the logical handle inert before it can suspend
// or fail. The later deinit call observes that state and cannot repeat cancel
// or cleanup. The provider is not a second public stream protocol.
foreign intrinsic from "std.readable-stream@1" {
  type ReadableStreamHandle

  fn stdReadableStreamFrom<Item, Failure: Error, Source: Stream<Item, Failure>>(
    source: take Source,
  ): ReadableStreamHandle

  fn stdReadableStreamFromByteSource<
    Failure: Error,
    Source: ByteSource<Failure>,
  >(
    named source: take Source,
    named chunkBytes: usize<(1...)>,
  ): ReadableStreamHandle

  async fn stdReadableStreamNext<Item, Failure: Error>(
    handle: inout ReadableStreamHandle,
  ): Item? throws Failure

  // The provider commits inert state before suspension. It drains the owned
  // root before returning or throwing. Failure reports the source cancel or
  // cleanup failure; it never restores the logical stream owner.
  async fn stdReadableStreamCancel<Failure: Error>(
    handle: inout ReadableStreamHandle,
  ): () throws Failure

  fn stdReadableStreamTeeItems<
    Item: Duplicable,
    Failure: Error & Duplicable,
  >(
    named handle: inout ReadableStreamHandle,
    named maximumBufferedItems: usize<(1...)>,
  ): (ReadableStreamHandle, ReadableStreamHandle) throws ReadableStreamUseError

  fn stdReadableStreamTeeBytes<Failure: Error & Duplicable>(
    named handle: inout ReadableStreamHandle,
    named maximumBufferedBytes: usize<(1...)>,
  ): (ReadableStreamHandle, ReadableStreamHandle) throws ReadableStreamUseError

  // Read and next share one cursor. A short read retains at most one remainder
  // owner and serves it before another upstream pull.
  async fn stdReadableStreamReadBytes<Failure: Error>(
    named handle: inout ReadableStreamHandle,
    appendTo destination: inout Bytes,
    named maximum: usize<(1...)>,
  ): ReadStep throws Failure

  // Drop is idempotent and best-effort: it requests cancellation for a live
  // handle without awaiting, or does nothing for a terminal/inert handle. The
  // structured root owns the physical drain in both cases.
  fn stdReadableStreamDrop(handle: inout ReadableStreamHandle)
}

// The phantom parameters keep the erased provider handle tied to the public
// Item and Failure at every safe call site. Only the intrinsic provider sees
// the raw handle and must validate its matching runtime witness.
struct TypedReadableStreamHandle<Item, Failure: Error> {
  raw: ReadableStreamHandle

  init(validatedRaw: ReadableStreamHandle) {
    self.raw = validatedRaw
  }
}

export struct ReadableStream<Item, Failure: Error>: Stream<Item, Failure> {
  handle: TypedReadableStreamHandle<Item, Failure>

  export static fn from<Source: Stream<Item, Failure>>(
    _ source: take Source,
  ): ReadableStream<Item, Failure> {
    let raw = unsafe { stdReadableStreamFrom(take source) }
    let handle = TypedReadableStreamHandle<Item, Failure>(validatedRaw: raw)
    return ReadableStream(validatedHandle: handle)
  }

  init(validatedHandle: TypedReadableStreamHandle<Item, Failure>) {
    self.handle = validatedHandle
  }

  export mut async fn next(): Item? throws Failure {
    return unsafe { try await stdReadableStreamNext(inout handle.raw) }
  }

  export take async fn cancel(): () throws Failure {
    // W-330: success, Failure, and task cancellation all consume `self`.
    // The provider commits `handle` to inert before propagating any outcome.
    unsafe { try await stdReadableStreamCancel(inout handle.raw) }
  }

  deinit {
    unsafe { stdReadableStreamDrop(inout handle.raw) }
  }
}

extension<Item: Duplicable, Failure: Error & Duplicable> ReadableStream<Item, Failure> {
  // This positive bound limits lag in item count only. Retained memory still
  // depends on each duplicated graph and the runtime allocation budget.
  export take fn tee(
    items maximumBufferedItems: usize<(1...)>,
  ): (
    ReadableStream<Item, Failure>,
    ReadableStream<Item, Failure>,
  ) throws ReadableStreamUseError {
    let (left, right) = unsafe {
      try stdReadableStreamTeeItems(
        handle: inout handle.raw,
        maximumBufferedItems: maximumBufferedItems,
      )
    }
    let leftHandle = TypedReadableStreamHandle<Item, Failure>(validatedRaw: left)
    let rightHandle = TypedReadableStreamHandle<Item, Failure>(validatedRaw: right)

    return (
      ReadableStream(validatedHandle: leftHandle),
      ReadableStream(validatedHandle: rightHandle),
    )
  }
}

extension<Failure: Error> ReadableStream<Bytes, Failure>: ByteSource<Failure> {
  export static fn from<Source: ByteSource<Failure>>(
    byteSource source: take Source,
    named chunkBytes: usize<(1...)>,
  ): ReadableStream<Bytes, Failure> {
    let raw = unsafe {
      stdReadableStreamFromByteSource(
        source: take source,
        chunkBytes: chunkBytes,
      )
    }

    let handle = TypedReadableStreamHandle<Bytes, Failure>(validatedRaw: raw)
    return ReadableStream(validatedHandle: handle)
  }

  export mut async fn read(
    appendTo destination: inout Bytes,
    named maximum: usize<(1...)>,
  ): ReadStep throws Failure {
    // maximum bounds the appended delta. The provider may grow destination
    // when its spare capacity is smaller than the confirmed progress.
    return unsafe {
      try await stdReadableStreamReadBytes(
        handle: inout handle.raw,
        appendTo: inout destination,
        maximum: maximum,
      )
    }
  }
}

extension<Failure: Error & Duplicable> ReadableStream<Bytes, Failure> {
  // This positive bound limits logical byte lag exactly. Shared or COW backing
  // must still preserve independent branch values.
  export take fn tee(
    bytes maximumBufferedBytes: usize<(1...)>,
  ): (
    ReadableStream<Bytes, Failure>,
    ReadableStream<Bytes, Failure>,
  ) throws ReadableStreamUseError {
    let (left, right) = unsafe {
      try stdReadableStreamTeeBytes(
        handle: inout handle.raw,
        maximumBufferedBytes: maximumBufferedBytes,
      )
    }
    let leftHandle = TypedReadableStreamHandle<Bytes, Failure>(validatedRaw: left)
    let rightHandle = TypedReadableStreamHandle<Bytes, Failure>(validatedRaw: right)

    return (
      ReadableStream(validatedHandle: leftHandle),
      ReadableStream(validatedHandle: rightHandle),
    )
  }
}

// Compile-fail assays:
// let second = copy stream              // The cursor is not Duplicable.
// let _ = stream.tee(items: 8)
// // Rejected when Item or Failure does not conform to Duplicable.
// async let first = stream.next()
// async let second = stream.next()      // The two calls need the same inout owner.
// do {
//   try await (take stream).cancel()
// } catch error {
//   inspect(stream)                     // Rejected: throws does not restore `stream`.
// }

// Provider-gated runtime assays (not executable while the provider is missing):
// - cancel failure leaves the handle inert. Deinit cannot repeat cleanup.
// - item tee checks only item lag, never a transitive byte bound.
// - byte tee respects its byte bound, and dropping one branch preserves the other.
