import assert from "node:assert/strict"
import test from "node:test"
import {
  logicalChannelProjection,
  runChannelOperations,
} from "./channel-machine.mjs"

function open(capacity = 1, strategy = "ring") {
  return { op: "open", capacity, strategy, sender: "out", receiver: "in" }
}

function item(id, kind = "owned") {
  return { op: "createItem", item: id, kind }
}

function send(id, sender = "out") {
  return { op: "beginSend", sender, item: id, waiter: `send-${id}` }
}

function receive(id) {
  return { op: "beginReceive", receive: `receive-${id}` }
}

function committedFrame(capacity, strategy) {
  const operations = [open(capacity, strategy), item("frame")]
  if (capacity === 0) {
    operations.push(
      receive("frame"),
      send("frame"),
      { op: "progressAdmission" },
    )
  } else {
    operations.push(
      send("frame"),
      { op: "progressAdmission" },
      receive("frame"),
      { op: "progressReceive" },
    )
  }
  return operations
}

function closeConsumed(ids, senders = ["out"]) {
  return [
    ...senders.map((sender) => ({ op: "dropSender", sender })),
    { op: "releaseReceiver" },
    { op: "finish" },
  ]
}

test("a buffered item has one owner at every handoff", () => {
  const result = runChannelOperations([
    open(1),
    item("cake"),
    send("cake"),
    { op: "progressAdmission" },
    receive("cake"),
    { op: "progressReceive" },
    { op: "consumeReceive", receive: "receive-cake" },
    ...closeConsumed(["cake"]),
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.terminalItems.cake, "consumed")
  assert.equal(result.state.drops.length, 0)
  assert.equal(result.state.lifecycle, "drained")
})

test("capacity zero commits send and receive as one rendezvous", () => {
  const result = runChannelOperations([
    open(0),
    item("tea"),
    receive("tea"),
    send("tea"),
    { op: "progressAdmission" },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.buffer.length, 0)
  assert.equal(result.state.frames["receive-tea"], "owned")
  assert.deepEqual(result.state.happensBefore, [
    "send:send-tea:tea->receive:receive-tea:tea",
    "send:send-tea:tea->send-complete:send-tea",
  ])
})

test("a graceful close preserves an issued permit", () => {
  const result = runChannelOperations([
    open(1),
    { op: "beginReserve", sender: "out", waiter: "reserve", permit: "seat" },
    { op: "progressAdmission" },
    { op: "closeReceiver" },
    item("meal"),
    { op: "permitSend", permit: "seat", item: "meal" },
    receive("meal"),
    { op: "progressReceive" },
    { op: "consumeReceive", receive: "receive-meal" },
    ...closeConsumed(["meal"]),
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.permits.seat, "used")
  assert.equal(result.state.terminalItems.meal, "consumed")
})

test("trySend cannot bypass the oldest admission ticket", () => {
  const result = runChannelOperations([
    open(1),
    item("first"),
    item("barger"),
    send("first"),
    { op: "trySend", sender: "out", item: "barger" },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.waitingTickets.length, 1)
  assert.equal(result.state.terminalItems.barger, "returned:full")
  assert.equal(result.state.itemLocations.first, "sendWaiter:send-first")

  const rendezvousFull = runChannelOperations([
    open(0),
    item("alone"),
    { op: "trySend", sender: "out", item: "alone" },
  ])
  assert.equal(rendezvousFull.state.terminalItems.alone, "returned:full")

  const rendezvousAccepted = runChannelOperations([
    open(0),
    receive("paired"),
    item("paired"),
    { op: "trySend", sender: "out", item: "paired" },
  ])
  assert.equal(rendezvousAccepted.state.frames["receive-paired"], "owned")
})

test("cancellation on either side of send commit has different ownership", () => {
  const before = runChannelOperations([
    open(1),
    item("before"),
    send("before"),
    { op: "cancelAdmission", waiter: "send-before" },
  ])
  assert.equal(before.state.terminalItems.before, "dropped:cancel:send-before")

  const after = runChannelOperations([
    open(1),
    item("after"),
    send("after"),
    { op: "progressAdmission" },
    { op: "cancelAdmission", waiter: "send-after" },
  ])
  assert.equal(after.state.terminalItems.after, null)
  assert.equal(after.state.itemLocations.after, "buffer")
  assert.equal(
    after.state.outcomes.some((outcome) => outcome.outcome === "commitWonCancellation"),
    true,
  )

  const middleTicket = runChannelOperations([
    open(1),
    item("first"),
    item("middle"),
    item("last"),
    send("first"),
    send("middle"),
    send("last"),
    { op: "cancelAdmission", waiter: "send-middle" },
  ])
  assert.deepEqual(middleTicket.state.waitingTickets, [1, 3])
  assert.equal(middleTicket.state.terminalItems.middle, "dropped:cancel:send-middle")
})

test("receive cancellation preserves a queued item before commit and drops it after commit", () => {
  const before = runChannelOperations([
    open(1),
    receive("first"),
    { op: "cancelReceive", receive: "receive-first" },
    item("meal"),
    send("meal"),
    { op: "progressAdmission" },
  ])
  assert.equal(before.state.itemLocations.meal, "buffer")
  assert.equal(before.state.terminalItems.meal, null)

  const after = runChannelOperations([
    open(1),
    item("meal"),
    send("meal"),
    { op: "progressAdmission" },
    receive("meal"),
    { op: "progressReceive" },
    { op: "cancelReceive", receive: "receive-meal" },
  ])
  assert.equal(after.state.terminalItems.meal, "dropped:receive-cancel:receive-meal")
  assert.equal(after.state.frames["receive-meal"], "dropped")
})

test("receiver abort distinguishes accepted and waiting payloads", () => {
  const result = runChannelOperations([
    open(1),
    item("accepted"),
    item("waiting"),
    send("accepted"),
    { op: "progressAdmission" },
    send("waiting"),
    { op: "abortReceiver" },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.terminalItems.accepted, "dropped:receiver-abort")
  assert.equal(result.state.terminalItems.waiting, "returned:closed")
  assert.deepEqual(result.state.drops, [{ item: "accepted", reason: "receiver-abort" }])
})

test("ring and mutex strategies have the same logical result", () => {
  const run = (strategy) => runChannelOperations([
    open(1, strategy),
    item("meal"),
    send("meal"),
    { op: "progressAdmission" },
    receive("meal"),
    { op: "progressReceive" },
    { op: "consumeReceive", receive: "receive-meal" },
    ...closeConsumed(["meal"]),
  ])
  const ring = run("ring")
  const mutex = run("mutex")
  assert.deepEqual(logicalChannelProjection(ring), logicalChannelProjection(mutex))
  assert.notDeepEqual(ring.physical.trace, mutex.physical.trace)
})

test("FIFO is the selected interleaving, not a global producer order", () => {
  const run = (order) => {
    const operations = [
      open(2),
      { op: "copySender", from: "out", as: "other" },
      item("left"),
      item("right"),
      ...order.map((id) => send(id, id === "left" ? "out" : "other")),
      { op: "progressAdmission" },
      { op: "progressAdmission" },
    ]
    return runChannelOperations(operations)
  }
  const leftFirst = run(["left", "right"])
  const rightFirst = run(["right", "left"])
  assert.deepEqual(leftFirst.state.buffer, ["left", "right"])
  assert.deepEqual(rightFirst.state.buffer, ["right", "left"])
})

test("capacity accounting includes permits and never exceeds the bound", () => {
  const result = runChannelOperations([
    open(2),
    { op: "beginReserve", sender: "out", waiter: "reserve", permit: "seat" },
    { op: "progressAdmission" },
    item("meal"),
    send("meal"),
    { op: "progressAdmission" },
    item("extra"),
    { op: "trySend", sender: "out", item: "extra" },
  ])
  assert.equal(result.status, "accepted")
  assert.equal(result.state.highWaterMark, 2)
  assert.equal(result.state.terminalItems.extra, "returned:full")
})

test("borrowed payloads and a copied receiver fail before publication", () => {
  const borrowed = runChannelOperations([
    open(1),
    item("line", "view"),
    send("line"),
  ])
  assert.equal(borrowed.error, "payloadNotTransferable")
  assert.equal(borrowed.state.buffer.length, 0)

  const copiedReceiver = runChannelOperations([
    open(1),
    { op: "copyReceiver", as: "other" },
  ])
  assert.equal(copiedReceiver.error, "receiverNotShareable")
})

test("an aborted permit remains a linear resource until used or dropped", () => {
  const unresolved = runChannelOperations([
    open(1),
    { op: "beginReserve", sender: "out", waiter: "reserve", permit: "seat" },
    { op: "progressAdmission" },
    { op: "abortReceiver" },
    { op: "dropSender", sender: "out" },
    { op: "finish" },
  ])
  assert.equal(unresolved.error, "channelObligationsRemain")

  const released = runChannelOperations([
    open(1),
    { op: "beginReserve", sender: "out", waiter: "reserve", permit: "seat" },
    { op: "progressAdmission" },
    { op: "abortReceiver" },
    { op: "dropPermit", permit: "seat" },
    { op: "dropSender", sender: "out" },
    { op: "finish" },
  ])
  assert.equal(released.status, "accepted")
})

test("a canceled rendezvous permit closes only its own send and frees the receiver", () => {
  for (const strategy of ["ring", "mutex"]) {
    const oldPermit = runChannelOperations([
      open(0, strategy),
      receive("first"),
      { op: "beginReserve", sender: "out", waiter: "reserve-first", permit: "permit-first" },
      { op: "progressAdmission" },
      { op: "cancelReceive", receive: "receive-first" },
      item("old"),
      { op: "permitSend", permit: "permit-first", item: "old" },
      { op: "dropReturnedItem", item: "old", reason: "caller" },
    ])
    assert.equal(oldPermit.status, "accepted")
    assert.equal(oldPermit.state.lifecycle, "open")
    assert.equal(oldPermit.state.permits["permit-first"], "usedAfterAbort")
    assert.equal(oldPermit.state.terminalItems.old, "dropped:caller")

    const canceledWhileReplacementWaits = runChannelOperations([
      open(0, strategy),
      receive("first"),
      { op: "beginReserve", sender: "out", waiter: "reserve-first", permit: "permit-first" },
      { op: "progressAdmission" },
      { op: "cancelReceive", receive: "receive-first" },
      receive("second"),
      item("old"),
      { op: "permitSend", permit: "permit-first", item: "old" },
      { op: "dropReturnedItem", item: "old", reason: "caller" },
    ])
    assert.equal(canceledWhileReplacementWaits.status, "accepted")
    assert.equal(canceledWhileReplacementWaits.state.receiveWaiter, "receive-second")
    assert.equal(
      canceledWhileReplacementWaits.state.outcomes.some(
        (outcome) => outcome.operation === "send" &&
          outcome.outcome === "closed" &&
          outcome.item === "old",
      ),
      true,
    )
    assert.equal(canceledWhileReplacementWaits.state.terminalItems.old, "dropped:caller")

    const replacement = runChannelOperations([
      open(0, strategy),
      receive("first"),
      { op: "beginReserve", sender: "out", waiter: "reserve-first", permit: "permit-first" },
      { op: "progressAdmission" },
      { op: "cancelReceive", receive: "receive-first" },
      receive("second"),
      item("old"),
      { op: "permitSend", permit: "permit-first", item: "old" },
      { op: "dropReturnedItem", item: "old", reason: "caller" },
      item("new"),
      send("new"),
      { op: "progressAdmission" },
      { op: "consumeReceive", receive: "receive-second" },
      { op: "dropSender", sender: "out" },
      { op: "releaseReceiver" },
      { op: "finish" },
    ])
    assert.equal(replacement.status, "accepted")
    assert.equal(replacement.state.frames["receive-second"], "consumed")
    assert.equal(
      replacement.state.outcomes.some(
        (outcome) => outcome.operation === "send" &&
          outcome.outcome === "closed" &&
          outcome.item === "old",
      ),
      true,
    )
    assert.equal(replacement.state.terminalItems.old, "dropped:caller")
    assert.equal(replacement.state.terminalItems.new, "consumed")
    assert.equal(replacement.state.receiveWaiter, null)
  }
})

test("a paired rendezvous receiver remains busy until cancellation", () => {
  for (const strategy of ["ring", "mutex"]) {
    const waitingClose = runChannelOperations([
      open(0, strategy),
      receive("first"),
      { op: "closeReceiver" },
    ])
    assert.equal(waitingClose.status, "rejected")
    assert.equal(waitingClose.error, "receiverBusy")
    assert.equal(waitingClose.state.receiveWaiter, "receive-first")

    const waitingAbort = runChannelOperations([
      open(0, strategy),
      receive("first"),
      { op: "abortReceiver" },
    ])
    assert.equal(waitingAbort.status, "rejected")
    assert.equal(waitingAbort.error, "receiverBusy")
    assert.equal(waitingAbort.state.receiverLive, true)

    const waitingDuplicate = runChannelOperations([
      open(0, strategy),
      receive("first"),
      receive("second"),
    ])
    assert.equal(waitingDuplicate.status, "rejected")
    assert.equal(waitingDuplicate.error, "receiverAlreadyWaiting")
    assert.equal(waitingDuplicate.state.receiveWaiter, "receive-first")
    assert.equal(waitingDuplicate.state.events.includes("receive:receive-second:waiting"), false)
    assert.equal(
      waitingDuplicate.state.outcomes.some((outcome) => outcome.receive === "receive-second"),
      false,
    )

    const close = runChannelOperations([
      open(0, strategy),
      receive("first"),
      { op: "beginReserve", sender: "out", waiter: "reserve-first", permit: "permit-first" },
      { op: "progressAdmission" },
      { op: "closeReceiver" },
    ])
    assert.equal(close.status, "rejected")
    assert.equal(close.error, "receiverBusy")
    assert.equal(close.state.receiveWaiter, null)
    assert.equal(close.state.permits["permit-first"], "issued")

    const abort = runChannelOperations([
      open(0, strategy),
      receive("first"),
      { op: "beginReserve", sender: "out", waiter: "reserve-first", permit: "permit-first" },
      { op: "progressAdmission" },
      { op: "abortReceiver" },
    ])
    assert.equal(abort.status, "rejected")
    assert.equal(abort.error, "receiverBusy")
    assert.equal(abort.state.receiverLive, true)
    assert.equal(abort.state.permits["permit-first"], "issued")

    const duplicate = runChannelOperations([
      open(0, strategy),
      receive("first"),
      { op: "beginReserve", sender: "out", waiter: "reserve-first", permit: "permit-first" },
      { op: "progressAdmission" },
      receive("second"),
    ])
    assert.equal(duplicate.status, "rejected")
    assert.equal(duplicate.error, "receiverAlreadyWaiting")
    assert.equal(duplicate.state.receiveWaiter, null)
    assert.equal(duplicate.state.events.includes("receive:receive-second:waiting"), false)
    assert.equal(
      duplicate.state.outcomes.some((outcome) => outcome.receive === "receive-second"),
      false,
    )
  }
})

test("canceling a paired permit allows graceful drain without resurrecting a waiter", () => {
  for (const strategy of ["ring", "mutex"]) {
    const replacement = runChannelOperations([
      open(0, strategy),
      receive("first"),
      { op: "beginReserve", sender: "out", waiter: "reserve-first", permit: "permit-first" },
      { op: "progressAdmission" },
      { op: "cancelReceive", receive: "receive-first" },
      receive("second"),
      { op: "dropPermit", permit: "permit-first" },
      item("new"),
      send("new"),
      { op: "progressAdmission" },
      { op: "consumeReceive", receive: "receive-second" },
      { op: "dropSender", sender: "out" },
      { op: "releaseReceiver" },
      { op: "finish" },
    ])
    assert.equal(replacement.status, "accepted")
    assert.equal(replacement.state.permits["permit-first"], "dropped")
    assert.equal(replacement.state.terminalItems.new, "consumed")
    assert.equal(replacement.state.frames["receive-second"], "consumed")

    const result = runChannelOperations([
      open(0, strategy),
      receive("first"),
      { op: "beginReserve", sender: "out", waiter: "reserve-first", permit: "permit-first" },
      { op: "progressAdmission" },
      { op: "cancelReceive", receive: "receive-first" },
      { op: "closeReceiver" },
      { op: "dropSender", sender: "out" },
      { op: "dropPermit", permit: "permit-first" },
      { op: "releaseReceiver" },
      { op: "finish" },
    ])
    assert.equal(result.status, "accepted")
    assert.equal(result.state.lifecycle, "drained")
    assert.equal(result.state.permits["permit-first"], "dropped")
    assert.equal(result.state.receiveWaiter, null)
    assert.equal(result.state.outcomes.some((outcome) => outcome.outcome === "none"), false)
  }
})

test("last-sender close preserves an unrevoked rendezvous permit", () => {
  for (const strategy of ["ring", "mutex"]) {
    const result = runChannelOperations([
      open(0, strategy),
      receive("first"),
      { op: "beginReserve", sender: "out", waiter: "reserve-first", permit: "permit-first" },
      { op: "progressAdmission" },
      { op: "dropSender", sender: "out" },
      item("meal"),
      { op: "permitSend", permit: "permit-first", item: "meal" },
      { op: "consumeReceive", receive: "receive-first" },
      { op: "releaseReceiver" },
      { op: "finish" },
    ])
    assert.equal(result.status, "accepted")
    assert.equal(result.state.lifecycle, "drained")
    assert.equal(result.state.permits["permit-first"], "used")
    assert.equal(result.state.terminalItems.meal, "consumed")
  }
})

test("finish reports an active rendezvous receive independently of permit state", () => {
  for (const strategy of ["ring", "mutex"]) {
    const result = runChannelOperations([
      open(0, strategy),
      receive("first"),
      { op: "beginReserve", sender: "out", waiter: "reserve-first", permit: "permit-first" },
      { op: "progressAdmission" },
      { op: "dropSender", sender: "out" },
      { op: "finish" },
    ])
    assert.equal(result.status, "rejected")
    assert.equal(result.error, "receiverObligationsRemain")
  }
})

test("rendezvous item commit keeps receiver-owned cancellation cleanup", () => {
  for (const strategy of ["ring", "mutex"]) {
    const result = runChannelOperations([
      open(0, strategy),
      item("meal"),
      receive("meal"),
      send("meal"),
      { op: "progressAdmission" },
      { op: "cancelReceive", receive: "receive-meal" },
    ])
    assert.equal(result.status, "accepted")
    assert.equal(result.state.frames["receive-meal"], "dropped")
    assert.equal(result.state.terminalItems.meal, "dropped:receive-cancel:receive-meal")
    assert.equal(
      result.state.outcomes.some((outcome) => outcome.outcome === "commitWonCancellation"),
      true,
    )
  }
})

test("an owned receive frame keeps the receiver cursor busy without state mutation", () => {
  for (const capacity of [0, 1]) {
    for (const strategy of ["ring", "mutex"]) {
      const prefix = committedFrame(capacity, strategy)
      const before = runChannelOperations(prefix)
      assert.equal(before.status, "accepted")
      assert.equal(before.state.frames["receive-frame"], "owned")

      const blocked = [
        ["beginReceive", receive("second"), "receiverAlreadyWaiting"],
        ["closeReceiver", { op: "closeReceiver" }, "receiverBusy"],
        ["abortReceiver", { op: "abortReceiver" }, "receiverBusy"],
        ["releaseReceiver", { op: "releaseReceiver" }, "receiverBusy"],
      ]
      for (const [operationName, operation, expectedError] of blocked) {
        const result = runChannelOperations([...prefix, operation])
        assert.equal(result.status, "rejected", `${capacity}/${strategy}/${operationName}`)
        assert.equal(result.error, expectedError, `${capacity}/${strategy}/${operationName}`)
        assert.deepEqual(result.state, before.state, `${capacity}/${strategy}/${operationName}`)
      }
    }
  }
})

test("receiver authority resumes after consume, frame drop, or post-commit cancellation", () => {
  for (const capacity of [0, 1]) {
    for (const strategy of ["ring", "mutex"]) {
      for (const cleanup of ["consumeReceive", "dropReceiveFrame", "cancelReceive"]) {
        const operation = { op: cleanup, receive: "receive-frame" }
        const result = runChannelOperations([
          ...committedFrame(capacity, strategy),
          operation,
          receive("next"),
          { op: "cancelReceive", receive: "receive-next" },
          { op: "closeReceiver" },
          { op: "dropSender", sender: "out" },
          { op: "releaseReceiver" },
          { op: "finish" },
        ])
        assert.equal(result.status, "accepted", `${capacity}/${strategy}/${cleanup}`)
        assert.notEqual(result.state.frames["receive-frame"], "owned")
        assert.equal(result.state.frames["receive-next"], undefined)
        assert.equal(result.state.lifecycle, "drained")
        assert.equal(result.state.receiverLive, false)
      }
    }
  }
})

test("last-sender close drains around an owned frame but release waits for cleanup", () => {
  for (const capacity of [0, 1]) {
    for (const strategy of ["ring", "mutex"]) {
      const drained = runChannelOperations([
        ...committedFrame(capacity, strategy),
        { op: "dropSender", sender: "out" },
      ])
      assert.equal(drained.status, "accepted", `${capacity}/${strategy}/dropSender`)
      assert.equal(drained.state.lifecycle, "drained")
      assert.equal(drained.state.receiverLive, true)
      assert.equal(drained.state.frames["receive-frame"], "owned")

      const release = runChannelOperations([
        ...committedFrame(capacity, strategy),
        { op: "dropSender", sender: "out" },
        { op: "releaseReceiver" },
      ])
      assert.equal(release.status, "rejected", `${capacity}/${strategy}/releaseReceiver`)
      assert.equal(release.error, "receiverBusy")
      assert.deepEqual(release.state, drained.state)
    }
  }
})
