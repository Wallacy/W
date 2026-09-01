// Cross-axis ownership and structured execution at the Last Light restaurant.

import { OrderId } from domain

export object LifetimeLedger {
  var atomic dropped: u64 = 0
  var atomic cleaned: u64 = 0

  export init() {}

  fn recordDrop() {
    dropped.saturatingAdd<.relaxed>(1)
  }

  fn recordCleanup() {
    cleaned.saturatingAdd<.relaxed>(1)
  }

  fn dropCount(): u64 {
    return dropped.load<.relaxed>()
  }

  fn cleanupCount(): u64 {
    return cleaned.load<.relaxed>()
  }
}

export object TrackedCourse {
  orderId: OrderId
  revision: u64
  ledger: shared LifetimeLedger

  export init(
    orderId: OrderId,
    revision: u64,
    ledger: shared LifetimeLedger,
  ) {
    self.orderId = orderId
    self.revision = revision
    self.ledger = ledger
  }

  deinit {
    ledger.recordDrop()
  }
}

export struct OwnershipBatch {
  direct: TrackedCourse
  awaited: TrackedCourse
  local: TrackedCourse
  parallel: TrackedCourse
  checksum: u64
}

export struct RevisionLedger {
  revision: u64
}

fn inspectDirect(course: ref TrackedCourse): u64 {
  return course.orderId ^ course.revision
}

fn inspectAfterYield(course: ref TrackedCourse): u64 {
  await execution#yield()
  return course.orderId ^ course.revision
}

fn finishCourse(course: take TrackedCourse): TrackedCourse {
  return take course
}

fn finishCourseAfterYield(course: take TrackedCourse): TrackedCourse {
  let cleanupLedger = copy course.ledger
  defer { cleanupLedger.recordCleanup() }

  await execution#yield()
  execution#checkCancellation()
  course.revision += 1
  return take course
}

fn discardCourseAfterYield(course: take TrackedCourse) {
  let cleanupLedger = copy course.ledger
  defer { cleanupLedger.recordCleanup() }

  await execution#yield()
  execution#checkCancellation()
}

fn incrementRevision(ledger: inout RevisionLedger): u64 {
  ledger.revision += 1
  return ledger.revision
}

export fn fourOwnershipForms(
  named direct: take TrackedCourse,
  named awaited: take TrackedCourse,
  named local: take TrackedCourse,
  named parallel: take TrackedCourse,
): OwnershipBatch {
  let directCode = inspectDirect(ref direct)
  let awaitedCode = await inspectAfterYield(ref awaited)

  let localTask = async finishCourse(take local)
  let parallelTask = spawn<.compute> finishCourse(take parallel)
  let (localResult, parallelResult) = await (localTask, parallelTask)

  return OwnershipBatch(
    direct: take direct,
    awaited: take awaited,
    local: take localResult,
    parallel: take parallelResult,
    checksum: directCode ^ awaitedCode,
  )
}

export fn updateWithChild(ledger: inout RevisionLedger): u64 {
  let update = async incrementRevision(inout ledger)
  let childRevision = await update
  return childRevision + ledger.revision
}

export fn inspectInCompute(course: ref TrackedCourse): u64 {
  let code = spawn<.compute> inspectDirect(ref course)
  return await code
}

export fn cancelTrackedCourse(
  course: take TrackedCourse,
): TaskOutcome<TrackedCourse, Never> {
  let task = async finishCourseAfterYield(take course)
  task#cancel(reason: .shutdown)
  return await (take task)#outcome()
}

export fn cancelDiscardedCourse(
  course: take TrackedCourse,
): TaskOutcome<(), Never> {
  let task = async discardCourseAfterYield(take course)
  task#cancel(reason: .shutdown)
  return await (take task)#outcome()
}

test "the four forms preserve explicit ownership" for fourOwnershipForms {
  let ledgerOwner = LifetimeLedger()
  let ledger: shared LifetimeLedger = take ledgerOwner
  let result = await fourOwnershipForms(
    direct: TrackedCourse(orderId: 1, revision: 1, ledger: copy ledger),
    awaited: TrackedCourse(orderId: 2, revision: 1, ledger: copy ledger),
    local: TrackedCourse(orderId: 3, revision: 1, ledger: copy ledger),
    parallel: TrackedCourse(orderId: 4, revision: 1, ledger: copy ledger),
  )

  expect result.direct.orderId == 1
  expect result.awaited.orderId == 2
  expect result.local.orderId == 3
  expect result.parallel.orderId == 4
}
