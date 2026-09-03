// Immutable binary values for Web-compatible bodies.
//
// Blob is ordinary W composition: an immutable shared Bytes backing, a byte
// range, and normalized media metadata.  It grants no filesystem, URL,
// network, or registry authority.  Copying a Blob explicitly retains the
// backing; slicing does not copy payload bytes.

import { ByteSource, ReadStep, SnapshotByteSource, SnapshotReadStep } from std.io
import { ReadableStream } from std.stream

export enum BlobError: Error & Copy {
  invalidRange(start: usize, end: usize, size: usize)
  limitExceeded(maximumBytes: usize)
}

fn normalizeMediaType(input: ref String): String {
  var normalized = Bytes()

  for byte in input.bytes {
    if byte < 0x20_u8 || byte > 0x7e_u8 { return "" }

    if byte >= b'A' && byte <= b'Z' {
      normalized.append(byte + 0x20_u8)
    } else {
      normalized.append(byte)
    }
  }

  return switch String.adoptingUtf8(take normalized) {
    case .text(let value): value
    case .invalid(_, _): panic("ASCII media type invariant failed")
  }
}

struct BlobCursor: ByteSource<BlobError> {
  let blob: Blob
  let offset: usize

  mut async fn read(
    appendTo destination: inout Bytes,
    maximum: usize<(1...)>,
  ): ReadStep throws BlobError {
    switch try await blob.read(
      at: offset,
      appendTo: inout destination,
      maximum: maximum,
    ) {
      case .data(let count):
        offset += count
        return .data(count)
      case .end:
        return .end
    }
  }
}

export struct Blob: Duplicable & SnapshotByteSource<BlobError> {
  let backing: shared Bytes
  let start: usize
  let storedSize: usize
  let storedType: String

  export init(bytes: take Bytes, mediaType: String = "") {
    let size = bytes.count
    self.backing = take bytes
    self.start = 0
    self.storedSize = size
    self.storedType = normalizeMediaType(input: mediaType)
  }

  init(
    retaining backing: shared Bytes,
    start: usize,
    size: usize,
    mediaType: String,
  ) {
    self.backing = take backing
    self.start = start
    self.storedSize = size
    self.storedType = take mediaType
  }

  export let size: usize {
    get => storedSize
  }

  export let type: view String {
    get => storedType
  }

  export fn duplicate(): Blob {
    return Blob(
      retaining: copy backing,
      start: start,
      size: storedSize,
      mediaType: copy storedType,
    )
  }

  export fn slice(
    start relativeStart: usize = 0,
    end relativeEnd: usize? = .none,
    mediaType: String = "",
  ): Blob throws BlobError {
    let end = relativeEnd ?? storedSize
    guard relativeStart <= end && end <= storedSize else {
      throw .invalidRange(
        start: relativeStart,
        end: end,
        size: storedSize,
      )
    }

    return Blob(
      retaining: copy backing,
      start: start + relativeStart,
      size: end - relativeStart,
      mediaType: normalizeMediaType(input: mediaType),
    )
  }

  export fn bytes(
    maximumBytes: usize<(1...)>,
  ): Bytes throws BlobError {
    guard storedSize <= maximumBytes else {
      throw .limitExceeded(maximumBytes: maximumBytes)
    }

    return copy backing[start..<(start + storedSize)]
  }

  export fn text(
    maximumBytes: usize<(1...)>,
  ): String throws BlobError {
    let payload = try bytes(maximumBytes: maximumBytes)
    return String.replacingInvalidUtf8(payload)
  }

  export fn stream(
    chunkBytes: usize<(1...)>,
  ): ReadableStream<Bytes, BlobError> {
    let source = BlobCursor(blob: duplicate(), offset: 0)
    return ReadableStream.from(
      byteSource: take source,
      chunkBytes: chunkBytes,
    )
  }

  export fn byteCount(): u64 {
    return u64(storedSize)
  }

  export async fn read(
    at offset: u64,
    appendTo destination: inout Bytes,
    maximum: usize<(1...)>,
  ): SnapshotReadStep throws BlobError {
    guard offset < u64(storedSize) else return .end

    let localOffset = usize(offset)
    let remaining = storedSize - localOffset
    let count = if remaining < maximum { remaining } else { maximum }
    let first = start + localOffset
    destination.append(backing[first..<(first + count)])
    return .data(count)
  }
}

test "blob copies and slices retain immutable bytes" {
  let source = Blob(take b"Restaurant at the End", mediaType: "TEXT/PLAIN")
  let copy = copy source
  let suffix = try source.slice(start: 18)

  expect source.type == "text/plain"
  expect try copy.text(maximumBytes: 64) == "Restaurant at the End"
  expect try suffix.text(maximumBytes: 16) == "End"
}

test "blob reads preserve explicit bounds" {
  let source = Blob(take b"Violet Horizon")

  do {
    let _ = try source.bytes(maximumBytes: 4)
    panic("an oversized Blob materialization succeeded")
  } catch .limitExceeded(let maximumBytes) {
    expect maximumBytes == 4
  }

  expect source.size == 14
}
