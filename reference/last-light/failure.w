// Absence, recoverable failure, panic, OOM, and cleanup at the last service window.

import { Course, Guest, GuestId, GuestName, Order } from domain

export enum ServiceLookupError: Error {
  missingGuest(GuestId)
  allocation(AllocationError)
  corruptRecord(GuestId)
}

export enum CleanupStep {
  opened
  decoded
  closed
}

export protocol FallibleArchive {
  fn duplicate(source: ref Bytes): Result<Bytes, AllocationError>
}

export fn firstOrder(orders: ref Array<Order>): ref Order? {
  let ref first = orders.first?
  return .some(first)
}

export fn guestLabel(guest: ref Guest?): String {
  return guest?.name ?? "Unknown traveller"
}

export fn optionalGuestName(input: ref String): GuestName? {
  return try? GuestName(input)
}

export fn requireGuest(
  guests: ref Map<GuestId, Guest>,
  id guestId: GuestId,
): ref Guest throws ServiceLookupError {
  return try guests[guestId].orThrow(.missingGuest(guestId))
}

export fn archiveSnapshot(
  archive: ref any FallibleArchive,
  source: ref Bytes,
): Bytes throws ServiceLookupError {
  return try archive.duplicate(source)
    .mapError((error) => .allocation(error))
}

export fn captureLookup(
  guests: ref Map<GuestId, Guest>,
  id guestId: GuestId,
): Result<ref Guest, ServiceLookupError> {
  return Result.capture(() => try requireGuest(guests, id: guestId))
}

export fn recoverGuest(
  guests: ref Map<GuestId, Guest>,
  id guestId: GuestId,
): ref Guest throws ServiceLookupError {
  do {
    return try requireGuest(guests, id: guestId)
  } catch .corruptRecord(let recordId) if recordId == guestId {
    throw .missingGuest(guestId)
  } catch error {
    throw error
  }
}

export fn decodeWithCleanup(
  source: ref Bytes,
  trace cleanupTrace: inout Array<CleanupStep>,
): Course throws ServiceLookupError {
  cleanupTrace.append(.opened)
  defer { cleanupTrace.append(.closed) }

  guard source.count > 0 else throw .corruptRecord(0)
  cleanupTrace.append(.decoded)
  return .horizonCake
}

export fn invariantCourse(courses: ref Array<Course>, index: usize): Course {
  // A bad internal index causes panic .bounds. User input must use get().
  return courses[index]
}

test "Option propagation returns none before later work" for firstOrder {
  let orders = Array<Order>()
  expect firstOrder(orders) == .none
}

test "Result can cross a storage boundary without hidden control flow" for captureLookup {
  let guests = Map<GuestId, Guest>()
  let result = captureLookup(guests, id: 42)
  expect result.isError
}

test "structured failure executes cleanup once" for decodeWithCleanup {
  var trace = Array<CleanupStep>()

  do {
    let _ = try decodeWithCleanup(b"", trace: inout trace)
    panic("empty record was accepted")
  } catch .corruptRecord(_) {
    expect trace == [.opened, .closed]
  }
}
