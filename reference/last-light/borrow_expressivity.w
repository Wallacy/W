// Higher-order borrowed values at the Last Light restaurant.
//
// This file is a source-visible fixture for BRX0. It uses the current W
// surface. The structured oracle supplies the bodyless problem traces and
// relation candidates; no lifetime name is written here.

import streaming from std.stream

export struct MenuLine {
  text: String
}

export struct MenuLineCursor {
  lines: Array<String>
  position: usize

  init(lines: Array<String>) {
    self.lines = take lines
    self.position = 0
  }
}

export protocol MenuCursor<Item, Failure: Error> {
  mut async fn next(): view Item? throws Failure
}

// Bodyless protocol requirement used by BRX0. One compatible input is the
// unique source; two independent compatible inputs reject with
// W-BORROW-0011 when no receiver or body is authoritative.
export protocol MenuSelection {
  static fn select(primary: ref String, fallback: ref String): view String
}

export fn freshGreeting(line: ref String): view String {
  return line
}

export fn selectPrimary(
  primary: ref String,
  fallback: ref String,
): view String {
  return primary
}

export fn selectFromCursor(cursor: ref MenuLineCursor): view String? {
  guard cursor.position < cursor.lines.count else return .none
  return cursor.lines[cursor.position]
}

export fn mapBorrowedLines(
  source: take some Stream<view String, Never>,
): some Stream<view String, Never> {
  // Source-shaped adapter direction. The current std Stream contract does
  // not publish map/filter declarations; BRX0 traces their OriginSet in the
  // host oracle without claiming an adapter implementation.
  return (take source).map(using: (line) => line)
}

export fn filterBorrowedLines(
  source: take some Stream<view String, Never>,
): some Stream<view String, Never> {
  return (take source).filter(using: (line) => !line.isEmpty)
}

export fn mapThenFilterBorrowedLines(
  source: take some Stream<view String, Never>,
): some Stream<view String, Never> {
  return (take source)
    .map(using: (line) => line)
    .filter(using: (line) => !line.isEmpty)
}

// The adapter factory above moves an owned source into the adapter. The next
// operation is a separate receiver-shaped borrow: its item is tied to the
// adapter's reusable storage, not to the factory parameter.
export async fn nextMappedLine(cursor: inout MenuLineCursor): view String? {
  return selectFromCursor(ref cursor)
}

export fn callableBorrow(
  line: ref String,
  using handler: some fn(ref String): view String,
): view String {
  return handler(line)
}

export fn erasedHandler(line: ref String): view String {
  return line
}

export struct BorrowedHandlerStorage {
  handler: any fn(ref String): view String
}

// The callable owner may be stored. A later invocation still applies the
// returned-view escape rule to the actual input owner.
export fn storeErasedHandler(
  handler: take any fn(ref String): view String,
): BorrowedHandlerStorage {
  return BorrowedHandlerStorage(handler: take handler)
}

export async fn awaitBorrowedLine(line: ref String): view String {
  // The invocation trace supplies the await boundary. This source fixture
  // does not claim an async implementation or a stable frame.
  return line
}

export fn storeBorrowedHandler(line: ref String): view String {
  // The host assay supplies closure storage and escape facts for this source
  // shape. No closure is stored by this parse-only fixture.
  return line
}

export fn escapeErasedResult(
  line: ref String,
  using handler: any fn(ref String): view String,
): view String {
  // The returned view is the negative: the caller cannot retain it after the
  // borrowed `line` owner ends, even though the erased handler itself stores.
  return handler(line)
}

// Storing an erased callable is a valid owner operation. The returned view
// from an invocation still cannot escape its input owner.
// The following comments are boundary assays for the source fixture. The
// host oracle records the corresponding ownership diagnostics.
// next() while a view is live and storage is reused: W-BORROW-0006
// await with an unstable referent: W-BORROW-0007
// closure storage or a channel cannot retain a dynamic view: W-BORROW-0003
