// String storage and mutation for the Last Light restaurant.

import * from std.memory
import * from std.text

export type HorizonSignLabel = String<(.bytes.count <= 64)>

export fn appendToHorizonSign(
  label: inout HorizonSignLabel,
  suffix addition: view String,
): Bool {
  guard addition.bytes.count <= 64 - label.bytes.count else return false
  label.append(addition)
  return true
}

export fn joinAnnouncements(
  lines: ref Array<String>,
  memory allocator: ref Allocator,
): String throws AllocationError {
  var required: usize = 0
  var isFirst = true

  for ref line in lines {
    if !isFirst {
      required = try usize.checkedAdd(required, 1)
        .mapError((_) => .sizeOverflow)
    }

    required = try usize.checkedAdd(required, line.bytes.count)
      .mapError((_) => .sizeOverflow)
    isFirst = false
  }

  var output = String(using: allocator)
  try output.tryReserve(minimumBytes: required)
  isFirst = true

  for ref line in lines {
    if !isFirst { output.append("\n") }
    output.append(line)
    isFirst = false
  }

  return output
}

export object AnnouncementBuffer {
  var text: String

  export init(memory: ref Allocator) {
    self.text = String(using: memory)
  }

  mut fn push(chunk: view String) {
    text.append(chunk)
  }

  mut fn finishLine() {
    text.append('\n')
  }

  mut fn drain(): String {
    return text.takeAll()
  }

  mut fn clearForReuse() {
    text.clear()
  }

  mut fn releaseStorage() {
    text.reset()
  }
}

export fn encodeAnnouncement(value: take String): Bytes {
  return (take value).intoBytes()
}

export fn roundTripCarrier(value: take String): Utf8Adoption {
  let payload = (take value).intoBytes()
  return String.adoptingUtf8(take payload)
}

export fn joinPair(left: take String, right: view String): String {
  return (take left) + right
}

test "reservation makes incremental construction deterministic" for joinAnnouncements {
  var storage: [u8; 4<KiB>] = [0; 4<KiB>]
  let memory = Arena.fixed(inout storage)
  let lines = ["Do not panic", "Dessert remains available"]

  let result = try joinAnnouncements(ref lines, memory: ref memory)
  expect result == "Do not panic\nDessert remains available"
}

test "takeAll moves a frame and leaves a reusable String" {
  var storage: [u8; 4<KiB>] = [0; 4<KiB>]
  let memory = Arena.fixed(inout storage)
  let buffer = AnnouncementBuffer(memory: ref memory)

  buffer.push("Last")
  buffer.push(" orders")
  buffer.finishLine()

  let first = buffer.drain()
  expect first == "Last orders\n"

  buffer.push("Closed")
  let second = buffer.drain()
  expect second == "Closed"
}

test "a consuming conversion preserves the UTF-8 bytes" for encodeAnnouncement {
  let message = "Violet Horizon"
  let payload = encodeAnnouncement(take message)
  expect payload == b"Violet Horizon"
}

test "static and dynamic carriers keep the same text" for roundTripCarrier {
  let literal = roundTripCarrier("End of service")
  switch literal {
    case .text(let text): expect text == "End of service"
    case .invalid(_, _): panic("a UTF-8 literal became invalid")
  }

  var dynamic = String(reservingBytes: 128)
  dynamic.append("A table at the ")
  dynamic.append("observable edge\0")

  let rebuilt = roundTripCarrier(take dynamic)
  switch rebuilt {
    case .text(let text): expect text == "A table at the observable edge\0"
    case .invalid(_, _): panic("valid dynamic UTF-8 became invalid")
  }
}

test "String mutation never creates an implicit alias temporary" {
  var title = "Last Light"
  let suffix = title.scalars[title.scalars.start..<title.scalars.end]

  // Compile-fail assay: suffix borrows the destination owner.
  // title.append(suffix)

  let ownedSuffix = suffix.materialize()
  title.append(ownedSuffix)
  expect title == "Last LightLast Light"
}

test "a byte-bounded sign preserves its refinement during mutation" for appendToHorizonSign {
  var label: HorizonSignLabel = "Last Light"

  expect appendToHorizonSign(inout label, suffix: " remains open")
  expect label == "Last Light remains open"

  let before = copy label
  expect !appendToHorizonSign(
    inout label,
    suffix: " beyond the final observable edge of the universe",
  )
  expect label == before
}
