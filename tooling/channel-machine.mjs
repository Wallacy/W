const VALID_STRATEGIES = new Set(["ring", "mutex"])

function freshState() {
  return {
    initialized: false,
    lifecycle: "uninitialized",
    capacity: null,
    strategy: null,
    nextTicket: 1,
    senders: {},
    receiver: null,
    items: {},
    admission: [],
    admissionHistory: {},
    permits: {},
    receiveWaiter: null,
    receiveHistory: {},
    frames: {},
    slotTokens: [],
    outcomes: [],
    drops: [],
    events: [],
    happensBefore: [],
    highWaterMark: 0,
  }
}

function clone(value) {
  return JSON.parse(JSON.stringify(value))
}

function activeSenders(state) {
  return Object.values(state.senders).filter((sender) => sender.active).length
}

function activePermits(state) {
  return Object.values(state.permits).filter((permit) => permit.state === "issued")
}

function unresolvedPermitResources(state) {
  return Object.values(state.permits).filter((permit) =>
    new Set(["issued", "aborted"]).has(permit.state),
  )
}

function occupancy(state) {
  return state.buffer.length + activePermits(state).filter(
    (permit) => permit.capacity > 0,
  ).length
}

function updateHighWaterMark(state) {
  state.highWaterMark = Math.max(state.highWaterMark, occupancy(state))
}

function dropItem(state, itemId, reason, fail) {
  const item = state.items[itemId]
  if (!item) return fail("unknownItem")
  if (item.terminal !== null) return fail("itemAlreadyTerminal")
  item.location = "dropped"
  item.terminal = `dropped:${reason}`
  state.drops.push({ item: itemId, reason })
  state.events.push(`drop:${itemId}:${reason}`)
  return true
}

function returnItem(state, itemId, reason, fail) {
  const item = state.items[itemId]
  if (!item) return fail("unknownItem")
  if (item.terminal !== null) return fail("itemAlreadyTerminal")
  item.location = "returned"
  item.terminal = `returned:${reason}`
  state.outcomes.push({ operation: "send", outcome: reason, item: itemId })
  state.events.push(`return:${itemId}:${reason}`)
  return true
}

function consumeSlot(state, event, fail) {
  if (state.capacity === 0) return null
  const token = state.slotTokens.shift()
  if (token === undefined) return fail("capacityUnavailable")
  if (token.startsWith("release:")) {
    state.happensBefore.push(`${token.slice(8)}->${event}`)
  }
  return token
}

function releaseSlot(state, event) {
  if (state.capacity === 0) return
  state.slotTokens.push(`release:${event}`)
}

function channelObligations(state) {
  return state.admission.length + state.buffer.length + activePermits(state).length
}

function completePendingReceiveWithNone(state) {
  if (!state.receiveWaiter) return
  const receiveId = state.receiveWaiter
  state.receiveWaiter = null
  state.receiveHistory[receiveId].state = "none"
  state.outcomes.push({ operation: "receive", outcome: "none", receive: receiveId })
  state.events.push(`receive:${receiveId}:none`)
  state.happensBefore.push(`close->receive-none:${receiveId}`)
}

function updateDrained(state) {
  if (state.lifecycle !== "closing" || channelObligations(state) !== 0) return
  state.lifecycle = "drained"
  state.events.push("channel:drained")
  completePendingReceiveWithNone(state)
}

function failAdmission(state, admission, reason, fail) {
  admission.state = reason
  state.admissionHistory[admission.waiter].state = reason
  if (admission.kind === "send") {
    if (!returnItem(state, admission.item, "closed", fail)) return false
  } else {
    state.outcomes.push({ operation: "reserve", outcome: "closed", waiter: admission.waiter })
    state.events.push(`reserve:${admission.waiter}:closed`)
  }
  return true
}

function beginClosing(state, source, fail) {
  if (state.lifecycle === "aborted" || state.lifecycle === "drained") return true
  if (state.lifecycle === "open") {
    state.lifecycle = "closing"
    state.events.push(`channel:closing:${source}`)
    state.happensBefore.push("close->admission-closed")
  }
  const waiting = state.admission.splice(0)
  for (const admission of waiting) {
    if (!failAdmission(state, admission, "closed", fail)) return false
  }
  updateDrained(state)
  return true
}

function commitReceive(state, receiveId, itemId, sendEvent, fail) {
  const history = state.receiveHistory[receiveId]
  const item = state.items[itemId]
  if (!history || history.state !== "waiting") return fail("receiveNotWaiting")
  if (!item || item.terminal !== null) return fail("itemUnavailable")
  state.receiveWaiter = null
  history.state = "committed"
  history.item = itemId
  item.location = `receiveFrame:${receiveId}`
  state.frames[receiveId] = { item: itemId, state: "owned" }
  state.events.push(`receive:${receiveId}:commit:${itemId}`)
  state.happensBefore.push(`${sendEvent}->receive:${receiveId}:${itemId}`)
  return true
}

function commitBufferedSend(state, admission, fail) {
  const sendEvent = `send:${admission.waiter}:${admission.item}`
  const slot = consumeSlot(state, sendEvent, fail)
  if (slot === false) return false
  const item = state.items[admission.item]
  if (!item || item.terminal !== null) return fail("itemUnavailable")
  item.location = "buffer"
  admission.state = "committed"
  admission.slot = slot
  state.admissionHistory[admission.waiter].state = "committed"
  state.buffer.push({ item: admission.item, sendEvent })
  state.outcomes.push({ operation: "send", outcome: "sent", item: admission.item })
  state.events.push(`${sendEvent}:commit`)
  updateHighWaterMark(state)
  return true
}

function commitRendezvousSend(state, admission, fail) {
  if (!state.receiveWaiter) return fail("rendezvousReceiverMissing")
  const receiveId = state.receiveWaiter
  const sendEvent = `send:${admission.waiter}:${admission.item}`
  admission.state = "committed"
  state.admissionHistory[admission.waiter].state = "committed"
  state.outcomes.push({ operation: "send", outcome: "sent", item: admission.item })
  state.events.push(`${sendEvent}:commit`)
  if (!commitReceive(state, receiveId, admission.item, sendEvent, fail)) return false
  state.happensBefore.push(`${sendEvent}->send-complete:${admission.waiter}`)
  return true
}

function issuePermit(state, admission, fail) {
  if (state.permits[admission.permit]) return fail("duplicatePermit")
  const permit = {
    id: admission.permit,
    state: "issued",
    capacity: state.capacity,
    receiver: null,
    slot: null,
  }
  if (state.capacity === 0) {
    if (!state.receiveWaiter) return fail("rendezvousReceiverMissing")
    permit.receiver = state.receiveWaiter
    state.receiveHistory[permit.receiver].state = "paired"
    state.receiveWaiter = null
  } else {
    const slot = consumeSlot(state, `permit:${permit.id}`, fail)
    if (slot === false) return false
    permit.slot = slot
  }
  admission.state = "committed"
  state.admissionHistory[admission.waiter].state = "committed"
  state.permits[permit.id] = permit
  state.outcomes.push({ operation: "reserve", outcome: "permit", permit: permit.id })
  state.events.push(`permit:${permit.id}:issued`)
  updateHighWaterMark(state)
  return true
}

function projectedState(state) {
  const terminalItems = Object.fromEntries(
    Object.entries(state.items).map(([id, item]) => [id, item.terminal]),
  )
  const itemLocations = Object.fromEntries(
    Object.entries(state.items).map(([id, item]) => [id, item.location]),
  )
  const permitStates = Object.fromEntries(
    Object.entries(state.permits).map(([id, permit]) => [id, permit.state]),
  )
  const frameStates = Object.fromEntries(
    Object.entries(state.frames).map(([id, frame]) => [id, frame.state]),
  )
  return {
    lifecycle: state.lifecycle,
    capacity: state.capacity,
    activeSenders: activeSenders(state),
    receiverLive: state.receiver?.live ?? false,
    buffer: state.buffer.map((entry) => entry.item),
    waitingTickets: state.admission.map((entry) => entry.ticket),
    permits: permitStates,
    receiveWaiter: state.receiveWaiter,
    frames: frameStates,
    itemLocations,
    terminalItems,
    outcomes: state.outcomes,
    drops: state.drops,
    highWaterMark: state.highWaterMark,
    happensBefore: state.happensBefore,
    events: state.events,
  }
}

export function logicalChannelProjection(result) {
  const logical = clone(result.state)
  return {
    ...logical,
    events: logical.events.filter((event) => !event.startsWith("physical:")),
  }
}

export function runChannelOperations(operations) {
  const state = freshState()
  state.buffer = []
  const physical = { strategy: null, trace: [] }
  let error = null

  const fail = (reason) => {
    if (error === null) error = reason
    return false
  }
  const requireInitialized = () => state.initialized || fail("channelNotOpen")
  const requireSender = (senderId) => {
    if (!state.senders[senderId]?.active) return fail("senderUnavailable")
    return true
  }
  const requireReceiver = () => {
    if (!state.receiver?.live) return fail("receiverUnavailable")
    return true
  }
  const requireOwnedItem = (itemId) => {
    const item = state.items[itemId]
    if (!item) return fail("unknownItem")
    if (item.terminal !== null || item.location !== "sender") {
      return fail("itemNotOwnedBySender")
    }
    if (item.kind !== "owned") return fail("payloadNotTransferable")
    return true
  }

  for (const operation of operations) {
    if (error !== null) break

    switch (operation.op) {
      case "open": {
        if (state.initialized) {
          fail("channelAlreadyOpen")
          break
        }
        if (!Number.isSafeInteger(operation.capacity) || operation.capacity < 0) {
          fail("capacityInvalid")
          break
        }
        if (!VALID_STRATEGIES.has(operation.strategy)) {
          fail("strategyInvalid")
          break
        }
        if (typeof operation.sender !== "string" || typeof operation.receiver !== "string") {
          fail("endpointMissing")
          break
        }
        state.initialized = true
        state.lifecycle = "open"
        state.capacity = operation.capacity
        state.strategy = operation.strategy
        state.senders[operation.sender] = { active: true }
        state.receiver = { id: operation.receiver, live: true }
        state.slotTokens = Array.from(
          { length: operation.capacity },
          (_, index) => `initial:${index}`,
        )
        state.events.push(`channel:open:${operation.capacity}`)
        physical.strategy = operation.strategy
        physical.trace.push(
          operation.strategy === "ring" ? "ring:allocate-bounded" : "mutex:allocate-bounded",
        )
        break
      }

      case "createItem": {
        if (!requireInitialized()) break
        if (state.items[operation.item]) {
          fail("duplicateItem")
          break
        }
        if (!new Set(["owned", "view"]).has(operation.kind)) {
          fail("itemKindInvalid")
          break
        }
        state.items[operation.item] = {
          kind: operation.kind,
          location: "sender",
          terminal: null,
        }
        state.events.push(`item:${operation.item}:created`)
        break
      }

      case "copySender": {
        if (!requireInitialized() || !requireSender(operation.from)) break
        if (state.senders[operation.as]) {
          fail("duplicateSender")
          break
        }
        state.senders[operation.as] = { active: true }
        state.events.push(`sender:${operation.as}:copied`)
        break
      }

      case "copyReceiver": {
        if (!requireInitialized() || !requireReceiver()) break
        fail("receiverNotShareable")
        break
      }

      case "beginSend": {
        if (!requireInitialized() || !requireSender(operation.sender)) break
        if (!requireOwnedItem(operation.item)) break
        if (state.admissionHistory[operation.waiter]) {
          fail("duplicateAdmissionWaiter")
          break
        }
        if (state.lifecycle !== "open") {
          state.admissionHistory[operation.waiter] = { state: "closed", kind: "send" }
          returnItem(state, operation.item, "closed", fail)
          break
        }
        const admission = {
          ticket: state.nextTicket++,
          kind: "send",
          waiter: operation.waiter,
          sender: operation.sender,
          item: operation.item,
          state: "waiting",
        }
        state.items[operation.item].location = `sendWaiter:${operation.waiter}`
        state.admission.push(admission)
        state.admissionHistory[operation.waiter] = admission
        state.events.push(`admission:${admission.ticket}:send:${operation.item}`)
        break
      }

      case "beginReserve": {
        if (!requireInitialized() || !requireSender(operation.sender)) break
        if (state.admissionHistory[operation.waiter]) {
          fail("duplicateAdmissionWaiter")
          break
        }
        if (typeof operation.permit !== "string") {
          fail("permitMissing")
          break
        }
        if (state.lifecycle !== "open") {
          state.admissionHistory[operation.waiter] = { state: "closed", kind: "reserve" }
          state.outcomes.push({ operation: "reserve", outcome: "closed", waiter: operation.waiter })
          state.events.push(`reserve:${operation.waiter}:closed`)
          break
        }
        const admission = {
          ticket: state.nextTicket++,
          kind: "reserve",
          waiter: operation.waiter,
          sender: operation.sender,
          permit: operation.permit,
          state: "waiting",
        }
        state.admission.push(admission)
        state.admissionHistory[operation.waiter] = admission
        state.events.push(`admission:${admission.ticket}:reserve:${operation.permit}`)
        break
      }

      case "progressAdmission": {
        if (!requireInitialized()) break
        if (state.lifecycle !== "open") {
          fail("admissionClosed")
          break
        }
        const admission = state.admission[0]
        if (!admission) {
          fail("admissionQueueEmpty")
          break
        }
        const canProgress = state.capacity === 0
          ? state.receiveWaiter !== null
          : state.slotTokens.length > 0
        if (!canProgress) {
          state.events.push(`admission:${admission.ticket}:blocked`)
          break
        }
        state.admission.shift()
        if (admission.kind === "send") {
          if (state.capacity === 0) commitRendezvousSend(state, admission, fail)
          else commitBufferedSend(state, admission, fail)
        } else {
          issuePermit(state, admission, fail)
        }
        physical.trace.push(
          `${state.strategy}:${admission.kind}:ticket-${admission.ticket}`,
        )
        break
      }

      case "trySend": {
        if (!requireInitialized() || !requireSender(operation.sender)) break
        if (!requireOwnedItem(operation.item)) break
        if (state.lifecycle !== "open") {
          returnItem(state, operation.item, "closed", fail)
          break
        }
        if (state.admission.length > 0) {
          returnItem(state, operation.item, "full", fail)
          break
        }
        if (state.capacity === 0) {
          if (!state.receiveWaiter) {
            returnItem(state, operation.item, "full", fail)
            break
          }
          const admission = {
            kind: "send",
            waiter: `try-${operation.item}`,
            item: operation.item,
            state: "waiting",
          }
          state.admissionHistory[admission.waiter] = admission
          commitRendezvousSend(state, admission, fail)
          break
        }
        if (state.slotTokens.length === 0) {
          returnItem(state, operation.item, "full", fail)
          break
        }
        const admission = {
          kind: "send",
          waiter: `try-${operation.item}`,
          item: operation.item,
          state: "waiting",
        }
        state.admissionHistory[admission.waiter] = admission
        commitBufferedSend(state, admission, fail)
        break
      }

      case "beginReceive": {
        if (!requireInitialized() || !requireReceiver()) break
        if (state.receiveWaiter !== null) {
          fail("receiverAlreadyWaiting")
          break
        }
        if (state.receiveHistory[operation.receive]) {
          fail("duplicateReceive")
          break
        }
        if (state.lifecycle === "aborted") {
          fail("receiverAborted")
          break
        }
        state.receiveHistory[operation.receive] = { state: "waiting" }
        state.receiveWaiter = operation.receive
        state.events.push(`receive:${operation.receive}:waiting`)
        if (state.lifecycle === "drained" ||
            (state.lifecycle === "closing" && channelObligations(state) === 0)) {
          completePendingReceiveWithNone(state)
        }
        break
      }

      case "progressReceive": {
        if (!requireInitialized() || !requireReceiver()) break
        if (!state.receiveWaiter) {
          fail("receiveWaiterMissing")
          break
        }
        if (state.buffer.length === 0) {
          if (state.lifecycle === "closing" || state.lifecycle === "drained") {
            updateDrained(state)
          } else {
            state.events.push(`receive:${state.receiveWaiter}:blocked`)
          }
          break
        }
        const buffered = state.buffer.shift()
        const receiveId = state.receiveWaiter
        if (!commitReceive(state, receiveId, buffered.item, buffered.sendEvent, fail)) break
        releaseSlot(state, `receive:${receiveId}:${buffered.item}`)
        physical.trace.push(`${state.strategy}:receive:${buffered.item}`)
        updateDrained(state)
        break
      }

      case "cancelAdmission": {
        if (!requireInitialized()) break
        const history = state.admissionHistory[operation.waiter]
        if (!history) {
          fail("admissionUnknown")
          break
        }
        if (history.state === "waiting") {
          const index = state.admission.findIndex(
            (admission) => admission.waiter === operation.waiter,
          )
          if (index < 0) {
            fail("admissionQueueCorrupt")
            break
          }
          const [admission] = state.admission.splice(index, 1)
          history.state = "canceled"
          if (admission.kind === "send") {
            dropItem(state, admission.item, `cancel:${operation.waiter}`, fail)
          } else {
            state.outcomes.push({
              operation: "reserve",
              outcome: "canceled",
              waiter: operation.waiter,
            })
          }
          state.events.push(`admission:${operation.waiter}:canceled`)
        } else if (history.state === "committed") {
          state.events.push(`admission:${operation.waiter}:late-cancel`)
          state.outcomes.push({
            operation: history.kind,
            outcome: "commitWonCancellation",
            waiter: operation.waiter,
          })
        } else {
          state.events.push(`admission:${operation.waiter}:cancel-idempotent`)
        }
        break
      }

      case "cancelReceive": {
        if (!requireInitialized()) break
        const history = state.receiveHistory[operation.receive]
        if (!history) {
          fail("receiveUnknown")
          break
        }
        if (history.state === "waiting") {
          if (state.receiveWaiter !== operation.receive) {
            fail("receiveWaiterCorrupt")
            break
          }
          state.receiveWaiter = null
          history.state = "canceled"
          state.outcomes.push({
            operation: "receive",
            outcome: "canceledBeforeCommit",
            receive: operation.receive,
          })
        } else if (history.state === "committed") {
          const frame = state.frames[operation.receive]
          if (!frame || frame.state !== "owned") {
            fail("receiveFrameUnavailable")
            break
          }
          dropItem(state, frame.item, `receive-cancel:${operation.receive}`, fail)
          frame.state = "dropped"
          history.state = "canceledAfterCommit"
          state.outcomes.push({
            operation: "receive",
            outcome: "commitWonCancellation",
            receive: operation.receive,
          })
        } else {
          state.events.push(`receive:${operation.receive}:cancel-idempotent`)
        }
        updateDrained(state)
        break
      }

      case "consumeReceive": {
        if (!requireInitialized()) break
        const frame = state.frames[operation.receive]
        if (!frame || frame.state !== "owned") {
          fail("receiveFrameUnavailable")
          break
        }
        const item = state.items[frame.item]
        if (!item || item.terminal !== null) {
          fail("itemUnavailable")
          break
        }
        item.location = "consumer"
        item.terminal = "consumed"
        frame.state = "consumed"
        state.events.push(`consume:${frame.item}`)
        break
      }

      case "dropReceiveFrame": {
        if (!requireInitialized()) break
        const frame = state.frames[operation.receive]
        if (!frame || frame.state !== "owned") {
          fail("receiveFrameUnavailable")
          break
        }
        dropItem(state, frame.item, `receive-frame:${operation.receive}`, fail)
        frame.state = "dropped"
        break
      }

      case "permitSend": {
        if (!requireInitialized()) break
        const permit = state.permits[operation.permit]
        if (!permit || !new Set(["issued", "aborted"]).has(permit.state)) {
          fail("permitUnavailable")
          break
        }
        if (!requireOwnedItem(operation.item)) break
        if (permit.state === "aborted" || state.lifecycle === "aborted") {
          permit.state = "usedAfterAbort"
          returnItem(state, operation.item, "closed", fail)
          break
        }
        permit.state = "used"
        const sendEvent = `permit-send:${permit.id}:${operation.item}`
        state.outcomes.push({ operation: "permitSend", outcome: "sent", item: operation.item })
        state.events.push(`${sendEvent}:commit`)
        if (permit.capacity === 0) {
          const receiveId = permit.receiver
          const history = state.receiveHistory[receiveId]
          if (!history || history.state !== "paired") {
            fail("pairedReceiverUnavailable")
            break
          }
          history.state = "waiting"
          if (!commitReceive(state, receiveId, operation.item, sendEvent, fail)) break
        } else {
          const item = state.items[operation.item]
          item.location = "buffer"
          state.buffer.push({ item: operation.item, sendEvent })
        }
        updateDrained(state)
        break
      }

      case "dropPermit": {
        if (!requireInitialized()) break
        const permit = state.permits[operation.permit]
        if (!permit || !new Set(["issued", "aborted"]).has(permit.state)) {
          fail("permitUnavailable")
          break
        }
        const wasIssued = permit.state === "issued"
        permit.state = "dropped"
        state.events.push(`permit:${permit.id}:dropped`)
        if (wasIssued && permit.capacity > 0) {
          releaseSlot(state, `permit-drop:${permit.id}`)
        } else if (wasIssued && permit.capacity === 0) {
          const history = state.receiveHistory[permit.receiver]
          if (!history || history.state !== "paired") {
            fail("pairedReceiverUnavailable")
            break
          }
          history.state = "waiting"
          state.receiveWaiter = permit.receiver
        }
        updateDrained(state)
        break
      }

      case "closeReceiver": {
        if (!requireInitialized() || !requireReceiver()) break
        const pairedReceive = Object.values(state.receiveHistory).some(
          (history) => history.state === "paired",
        )
        if (state.receiveWaiter !== null || pairedReceive) {
          fail("receiverBusy")
          break
        }
        beginClosing(state, "receiver", fail)
        break
      }

      case "abortReceiver": {
        if (!requireInitialized() || !requireReceiver()) break
        if (state.receiveWaiter !== null) {
          fail("receiverBusy")
          break
        }
        if (state.lifecycle === "drained") {
          state.receiver.live = false
          state.events.push("receiver:released")
          break
        }
        state.lifecycle = "aborted"
        state.receiver.live = false
        state.events.push("channel:aborted")
        const waiting = state.admission.splice(0)
        for (const admission of waiting) {
          if (!failAdmission(state, admission, "closed", fail)) break
        }
        for (const buffered of state.buffer.splice(0)) {
          if (!dropItem(state, buffered.item, "receiver-abort", fail)) break
        }
        for (const permit of activePermits(state)) permit.state = "aborted"
        state.slotTokens = []
        break
      }

      case "dropSender": {
        if (!requireInitialized() || !requireSender(operation.sender)) break
        if (state.admission.some((entry) => entry.sender === operation.sender)) {
          fail("senderHasPendingAdmission")
          break
        }
        state.senders[operation.sender].active = false
        state.events.push(`sender:${operation.sender}:dropped`)
        if (activeSenders(state) === 0) beginClosing(state, "last-sender", fail)
        break
      }

      case "closeSender": {
        if (!requireInitialized() || !requireSender(operation.sender)) break
        fail("senderCannotCloseGlobally")
        break
      }

      case "releaseReceiver": {
        if (!requireInitialized() || !requireReceiver()) break
        if (state.lifecycle !== "drained") {
          fail("receiverReleaseWouldAbort")
          break
        }
        state.receiver.live = false
        state.events.push("receiver:released")
        break
      }

      case "dropReturnedItem": {
        if (!requireInitialized()) break
        const item = state.items[operation.item]
        if (!item || !item.terminal?.startsWith("returned:")) {
          fail("returnedItemUnavailable")
          break
        }
        item.location = "dropped"
        item.terminal = `dropped:${operation.reason ?? "caller"}`
        state.drops.push({ item: operation.item, reason: operation.reason ?? "caller" })
        state.events.push(`drop:${operation.item}:${operation.reason ?? "caller"}`)
        break
      }

      case "finish": {
        if (!requireInitialized()) break
        if (!["drained", "aborted"].includes(state.lifecycle)) {
          fail("channelNotTerminal")
          break
        }
        if (state.admission.length !== 0 || unresolvedPermitResources(state).length !== 0) {
          fail("channelObligationsRemain")
          break
        }
        if (activeSenders(state) !== 0 || state.receiver?.live) {
          fail("endpointResourcesRemain")
          break
        }
        if (Object.values(state.frames).some((frame) => frame.state === "owned")) {
          fail("receiveFrameOwned")
          break
        }
        if (Object.values(state.items).some((item) => item.terminal === null)) {
          fail("itemWithoutTerminal")
          break
        }
        state.events.push("model:finished")
        break
      }

      default:
        fail("unknownOperation")
    }
  }

  if (state.initialized && occupancy(state) > state.capacity) {
    fail("capacityExceeded")
  }
  const duplicateDrops = new Set()
  for (const drop of state.drops) {
    if (duplicateDrops.has(drop.item)) fail("duplicateDrop")
    duplicateDrops.add(drop.item)
  }

  return {
    status: error === null ? "accepted" : "rejected",
    error,
    state: projectedState(state),
    physical,
  }
}
