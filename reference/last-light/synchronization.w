// Atomic publication, isolation domains, and the lock fallback at Last Light.

module synchronization<domains: [.serial(.apology)]>

import atomic from std
import { SnapshotCell } from std.sync

export enum SignState {
  dark
  announcing
  closed
}

export object EndOfUniverseSign {
  var atomic state: SignState = .dark
  var atomic announcements: u64 = 0

  export init() {}

  fn beginAnnouncement(): Bool {
    let result = state.compareExchange<
      success: .release,
      failure: .relaxed,
    >(
      expected: .dark,
      desired: .announcing,
    )

    return switch result {
      case .exchanged(_): true
      case .mismatch(_): false
    }
  }

  fn close(): AtomicExchange<SignState> {
    return state.compareExchange<
      success: .release,
      failure: .relaxed,
    >(
      expected: .announcing,
      desired: .closed,
    )
  }

  fn current(): SignState {
    return state
  }

  fn observe(): SignState {
    return state.load<.acquire>()
  }

  fn recordAnnouncement() {
    announcements.saturatingAdd<.relaxed>(1)
  }

  fn announcementCount(): u64 {
    return announcements.load<.relaxed>()
  }
}

export object HorizonTelemetryEpoch {
  var atomic revision: u64 = 0

  export init() {}

  fn publishFirst(): () {
    revision.store<.release>(1)
  }

  fn publish(_ next: u64): () {
    revision.store<.release>(next)
    revision.notifyAll()
  }

  async fn waitForChange(after observed: u64): u64 {
    return await revision.wait<.acquire>(whileEqual: observed)
  }

  fn notifyOneWithoutChange(): () {
    revision.notifyOne()
  }

  fn relay(): u64 {
    return revision.fetchWrappingAdd<.relaxed>(1)
  }

  fn observe(): u64 {
    return revision.load<.acquire>()
  }

  fn tryRelay(from expected: u64, to desired: u64): AtomicExchange<u64> {
    return revision.weakCompareExchange<
      success: .acquireRelease,
      failure: .acquire,
    >(
      expected: expected,
      desired: desired,
    )
  }

  fn resetBeforePublication(): () {
    revision.withExclusive((value: inout u64) => value = 0)
  }
}

export unsafe fn releaseTelemetryFence(): () {
  atomic.fence<.release>()
}

export unsafe fn acquireTelemetryFence(): () {
  atomic.fence<.acquire>()
}

export struct ApologyLedgerState: Duplicable {
  revision: u64
  messages: Array<String>
}

export object ThreadApologyLedger {
  state: shared ApologyLedgerState

  export init() {
    self.state = ApologyLedgerState(revision: 0, messages: [])
  }

  fn record(message: take String): u64 {
    return lock state as ledger {
      ledger.messages.append(take message)
      ledger.revision += 1
      ledger.revision
    }
  }

  fn snapshot(): ApologyLedgerState {
    return lock state as ledger { copy ledger }
  }

  fn trySnapshot(): LockAttempt<ApologyLedgerState> {
    return try lock state as ledger { copy ledger }
  }
}

fn recordApology(
  state: inout ApologyLedgerState,
  message: take String,
): u64 {
  state.messages.append(take message)
  state.revision += 1
  return state.revision
}

export async fn recordOnApologyDomain(
  state: inout ApologyLedgerState,
  message: take String,
): u64 {
  spawn<.apology> let revision = recordApology(
    inout state,
    message: take message,
  )
  return await revision
}

// This is the rare task fallback. State owned by the task subsystem should use
// its domain or a service instead of making every call await a lock.
export async fn snapshotForeignSharedState(
  state: shared ApologyLedgerState,
): ApologyLedgerState {
  return await lock state as ledger { copy ledger }
}

export struct PublishedMenu: Duplicable {
  revision: u64
  courses: Array<String>
}

export object HorizonMenuPublication {
  snapshots: SnapshotCell<PublishedMenu>

  export init(_ initial: take PublishedMenu) {
    self.snapshots = SnapshotCell(take initial)
  }

  fn courseCount(): usize {
    return snapshots.read(
      (menu: ref PublishedMenu) => menu.courses.count,
    )
  }

  fn revision(): u64 {
    return snapshots.read((menu: ref PublishedMenu) => menu.revision)
  }

  fn snapshot(): PublishedMenu {
    return snapshots.snapshot()
  }

  fn publish(_ next: take PublishedMenu) {
    snapshots.publish(take next)
  }
}

test "atomic enum transitions preserve the observed state" {
  let sign = EndOfUniverseSign()
  expect sign.observe() == .dark
  expect sign.beginAnnouncement()
  sign.recordAnnouncement()
  expect sign.observe() == .announcing
  expect sign.announcementCount() == 1
}

test "a relay updates the published telemetry epoch" {
  let epoch = HorizonTelemetryEpoch()
  epoch.resetBeforePublication()
  epoch.publishFirst()
  expect epoch.relay() == 1
  expect epoch.observe() == 2
}

test "an atomic wait observes a published telemetry generation" {
  let epoch = HorizonTelemetryEpoch()
  let observed = epoch.observe()
  async let changed = epoch.waitForChange(after: observed)

  epoch.publish(7)
  expect await changed == 7
}

test "a scoped synchronous lock returns an owned snapshot" {
  let ledger = ThreadApologyLedger()
  expect ledger.record("We regret the scheduling inconvenience") == 1

  let snapshot = ledger.snapshot()
  expect snapshot.revision == 1
  expect snapshot.messages == ["We regret the scheduling inconvenience"]

  let attempted = ledger.trySnapshot()
  expect switch attempted {
    case .acquired(let value): value.revision == 1
    case .busy: false
  }
}

test "a serial domain owns task state without an async mutex" {
  var ledger = ApologyLedgerState(revision: 0, messages: [])
  expect await recordOnApologyDomain(
    inout ledger,
    message: "Sorry for the temporal delay",
  ) == 1
  expect ledger.revision == 1
}

test "a published menu exposes one complete revision" {
  let menus = HorizonMenuPublication(PublishedMenu(
    revision: 1,
    courses: ["Pan-Galactic broth"],
  ))

  expect menus.revision() == 1
  expect menus.courseCount() == 1

  menus.publish(PublishedMenu(
    revision: 2,
    courses: ["Pan-Galactic broth", "End-of-universe cake"],
  ))

  let snapshot = menus.snapshot()
  expect snapshot.revision == 2
  expect snapshot.courses.count == 2
}

// Compile-fail assays:
// state.load<.release>()                  // LoadOrder rejects release.
// state.store<.acquire>(.closed)          // StoreOrder rejects acquire.
// state.compareExchange<success: .release, failure: .acquire>(...)
// announcements = announcements + 1      // Split load and store.
// await revision.wait<.release>(whileEqual: 1)
// take revision while a wait is registered
// revision.withExclusive(...) while a wait is registered
// (ref state).withExclusive((value: inout SignState) => value = .dark)
// lock state as value { await suspend(ref value) }
// lock state as value { ref value }
// lock state as value { lock state as nested { nested.revision } }
// copy state
// Mutex<State>(state)
// AsyncMutex<State>(state)
// ReadWriteLock<State>(state)
// snapshots.read((menu: ref PublishedMenu) => await inspect(menu))
// snapshots.read((menu: ref PublishedMenu) => ref menu)
// snapshots.publish(ref nextMenu)
// copy snapshots
// var atomic pointer: c.ptr<Menu> = rawMenu
// var atomic pair: (u64, u64) = (0, 0)
// var atomic owner: shared PublishedMenu = share(menu)
