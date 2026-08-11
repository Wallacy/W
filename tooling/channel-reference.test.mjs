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
