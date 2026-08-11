// Atomic publication and scoped locks at the Last Light restaurant.

import atomic from std

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

export struct ApologyLedgerState {
  revision: u64
  messages: Array<String>
}

export object ThreadApologyLedger {
  state: Mutex<ApologyLedgerState>

  export init() {
    self.state = Mutex(ApologyLedgerState(revision: 0, messages: []))
  }

  fn record(message: take String): u64 {
    return state.withLock((ledger: inout ApologyLedgerState) => {
      ledger.messages.append(take message)
      ledger.revision += 1
      return ledger.revision
    })
  }

  fn snapshot(): ApologyLedgerState {
    return state.withLock(
      (ledger: ref ApologyLedgerState) => copy ledger,
    )
  }
}

export object TaskApologyLedger {
  state: AsyncMutex<ApologyLedgerState>

  export init() {
    self.state = AsyncMutex(ApologyLedgerState(revision: 0, messages: []))
  }

  async fn record(message: take String): u64 {
    return await state.withLock((ledger: inout ApologyLedgerState) => {
      ledger.messages.append(take message)
      ledger.revision += 1
      return ledger.revision
    })
  }

  async fn snapshot(): ApologyLedgerState {
    return await state.withLock(
      (ledger: ref ApologyLedgerState) => copy ledger,
    )
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

test "a scoped synchronous lock returns an owned snapshot" {
  let ledger = ThreadApologyLedger()
  expect ledger.record("We regret the scheduling inconvenience") == 1

  let snapshot = ledger.snapshot()
  expect snapshot.revision == 1
  expect snapshot.messages == ["We regret the scheduling inconvenience"]
}

// Compile-fail assays:
// state.load<.release>()                  // LoadOrder rejects release.
// state.store<.acquire>(.closed)          // StoreOrder rejects acquire.
// state.compareExchange<success: .release, failure: .acquire>(...)
// announcements = announcements + 1      // Split load and store.
// (ref state).withExclusive((value: inout SignState) => value = .dark)
// state.withLock((value: inout State) => await suspend(value))
// state.withLock((value: ref State) => ref value)
