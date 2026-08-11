const TASK_STATES = new Set([
  "reserved",
  "published",
  "bodySettled",
  "cleanup",
  "outcomeCommitted",
  "joined",
]);
const OUTCOMES = new Set(["success", "error", "canceled"]);
const ACCESS_KINDS = new Set(["read", "write"]);
const ATOMIC_ORDERS = new Set([
  "relaxed",
  "acquire",
  "release",
  "acquireRelease",
  "sequential",
]);
const ATOMIC_LOAD_ORDERS = new Set(["relaxed", "acquire", "sequential"]);
const ATOMIC_STORE_ORDERS = new Set(["relaxed", "release", "sequential"]);
const ATOMIC_FENCE_ORDERS = new Set([
  "acquire",
  "release",
  "acquireRelease",
  "sequential",
]);
const ATOMIC_EXCLUSIVE_AUTHORITIES = new Set(["ref", "inout", "consumed"]);
const COMPARE_EXCHANGE_RESULTS = new Set(["exchanged", "mismatch"]);
const DISPATCH_MODES = new Set(["ordinary", "barrier"]);

export class ExecutionConcurrencyError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

function clone(value) {
  return structuredClone(value);
}

function requireTask(state, name) {
  const task = state.tasks[name];
  if (!task) throw new ExecutionConcurrencyError("unknownTask");
  return task;
}

function requireState(task, expected, code = "invalidTaskTransition") {
  if (task.state !== expected) throw new ExecutionConcurrencyError(code);
}

function requireDomainJob(state, name) {
  for (const [domainName, domain] of Object.entries(state.domains)) {
    if (domain.jobs[name]) return { domainName, domain, job: domain.jobs[name] };
  }
  throw new ExecutionConcurrencyError("domainJobNotAdmitted");
}

function addEdge(state, from, to, kind) {
  if (!state.events[from] || !state.events[to]) {
    throw new ExecutionConcurrencyError("edgeReferencesUnknownEvent");
  }
  if (!state.edges.some((edge) => edge.from === from && edge.to === to && edge.kind === kind)) {
    state.edges.push({ from, to, kind });
  }
}

function addEvent(state, operation, event) {
  if (state.events[operation.id]) {
    throw new ExecutionConcurrencyError("duplicateEvent");
  }
  if (event.task) requireTask(state, event.task);
  state.events[operation.id] = event;
  const previous = event.task ? state.lastEventByTask[event.task] : undefined;
  if (previous) addEdge(state, previous, operation.id, "sequencedBefore");
  if (event.task) state.lastEventByTask[event.task] = operation.id;
  return operation.id;
}

function memoryRange(value) {
  return {
    offset: value.offset ?? 0,
    bytes: value.bytes ?? null,
  };
}

function rangesOverlap(left, right) {
  const leftRange = memoryRange(left);
  const rightRange = memoryRange(right);
  if (leftRange.bytes === null || rightRange.bytes === null) return true;
  return (
    leftRange.offset < rightRange.offset + rightRange.bytes &&
    rightRange.offset < leftRange.offset + leftRange.bytes
  );
}

function rangesMatch(left, right) {
  const leftRange = memoryRange(left);
  const rightRange = memoryRange(right);
  return leftRange.offset === rightRange.offset && leftRange.bytes === rightRange.bytes;
}

function atomicLocationKey(value) {
  const range = memoryRange(value);
  return `${value.storage}@${range.offset}:${range.bytes ?? "all"}`;
}

function requireLiveStorage(state, storage) {
  const lifetime = state.storageLifetimes[storage];
  if (lifetime && !lifetime.live) {
    throw new ExecutionConcurrencyError("storageNotLive");
  }
  return lifetime;
}

function checkMemoryAccess(state, candidate) {
  const lifetime = requireLiveStorage(state, candidate.storage);
  if (
    lifetime?.mode &&
    lifetime.mode !== (candidate.atomic ? "atomic" : "ordinary") &&
    !candidate.exclusiveAtomic
  ) {
    throw new ExecutionConcurrencyError("mixedAtomicOrdinaryAccess");
  }

  for (const previous of Object.values(state.events)) {
    if (!previous.access || previous.storage !== candidate.storage) continue;
    if (!rangesOverlap(previous, candidate)) continue;
    if (previous.atomic && candidate.atomic && !rangesMatch(previous, candidate)) {
      throw new ExecutionConcurrencyError("atomicWidthMismatch");
    }
    if (
      previous.atomic !== candidate.atomic &&
      !previous.exclusiveAtomic &&
      !candidate.exclusiveAtomic
    ) {
      throw new ExecutionConcurrencyError("mixedAtomicOrdinaryAccess");
    }
  }
}

function checkAtomicAvailability(state, operation) {
  const lifetime = requireLiveStorage(state, operation.storage);
  if (lifetime?.mode === "ordinary") {
    throw new ExecutionConcurrencyError("mixedAtomicOrdinaryAccess");
  }
  if (state.atomicExclusive[operation.storage]) {
    throw new ExecutionConcurrencyError("atomicAccessDuringExclusivePayload");
  }
}

function atomicEvent(operation, kind, access, order, extra = {}) {
  return {
    task: operation.task,
    kind,
    storage: operation.storage,
    access,
    atomic: true,
    order,
    atomicLocation: atomicLocationKey(operation),
    ...(operation.offset === undefined ? {} : { offset: operation.offset }),
    ...(operation.bytes === undefined ? {} : { bytes: operation.bytes }),
    ...extra,
  };
}

function atomicModificationOrder(state, location) {
  state.atomicModificationOrder[location] ??= [];
  return state.atomicModificationOrder[location];
}

function requireObservedModification(state, operation) {
  const source = state.events[operation.observes];
  if (
    !source ||
    !source.atomicModification ||
    source.atomicLocation !== atomicLocationKey(operation)
  ) {
    throw new ExecutionConcurrencyError("atomicObservationMismatch");
  }
  return source;
}

function requireLatestModification(state, operation) {
  requireObservedModification(state, operation);
  const order = atomicModificationOrder(state, atomicLocationKey(operation));
  if (order.at(-1) !== operation.observes) {
    throw new ExecutionConcurrencyError("atomicRmwMustObserveLatest");
  }
}

function appendAtomicModification(state, eventId) {
  const event = state.events[eventId];
  atomicModificationOrder(state, event.atomicLocation).push(eventId);
}

function recordSequentialOperation(state, eventId) {
  if (state.events[eventId].order === "sequential") {
    state.sequentialOrder.push(eventId);
  }
}

function cancellationRecord() {
  return {
    requestedReasons: [],
    exceededBudgets: [],
    deadlineExceeded: false,
    scopeExited: false,
    siblingFailed: false,
    nearestAncestor: null,
  };
}

function addUnique(values, value) {
  if (value !== undefined && !values.includes(value)) values.push(value);
}

function mergeCancellation(task, request) {
  addUnique(task.cancellation.requestedReasons, request.reason);
  addUnique(task.cancellation.exceededBudgets, request.budget);
  if (request.deadline) task.cancellation.deadlineExceeded = true;
  if (request.scopeExit) task.cancellation.scopeExited = true;
  if (request.siblingFailure) task.cancellation.siblingFailed = true;
  if (request.nearestAncestor && !task.cancellation.nearestAncestor) {
    task.cancellation.nearestAncestor = request.nearestAncestor;
  }
}

function descendantsOf(state, ancestor) {
  const descendants = [];
  const pending = [...requireTask(state, ancestor).children];
  while (pending.length > 0) {
    const child = pending.shift();
    descendants.push(child);
    pending.push(...requireTask(state, child).children);
  }
  return descendants;
}

function requestCancellation(state, target, request, authority = "owner") {
  if (authority !== "owner") {
    throw new ExecutionConcurrencyError("cancelRequiresOwnerAuthority");
  }
  const task = requireTask(state, target);
  if (task.state === "joined") {
    throw new ExecutionConcurrencyError("cancelAfterJoin");
  }
  mergeCancellation(task, request);
  for (const descendantName of descendantsOf(state, target)) {
    const descendant = requireTask(state, descendantName);
    if (descendant.state !== "joined") {
      mergeCancellation(descendant, { nearestAncestor: target });
    }
  }
}

function reachable(state, from, to) {
  const pending = [from];
  const visited = new Set();
  while (pending.length > 0) {
    const current = pending.pop();
    if (current === to) return true;
    if (visited.has(current)) continue;
    visited.add(current);
    for (const edge of state.edges) {
      if (edge.from === current) pending.push(edge.to);
    }
  }
  return false;
}

function racePair(state) {
  const accesses = Object.entries(state.events).filter(([, event]) => event.access);
  for (let leftIndex = 0; leftIndex < accesses.length; leftIndex += 1) {
    const [leftId, left] = accesses[leftIndex];
    for (let rightIndex = leftIndex + 1; rightIndex < accesses.length; rightIndex += 1) {
      const [rightId, right] = accesses[rightIndex];
      if (left.storage !== right.storage || left.task === right.task) continue;
      if (!rangesOverlap(left, right)) continue;
      if (left.access === "read" && right.access === "read") continue;
      if (left.atomic && right.atomic) continue;
      if (!reachable(state, leftId, rightId) && !reachable(state, rightId, leftId)) {
        return [leftId, rightId];
      }
    }
  }
  return null;
}

function releaseOrder(order) {
  return ["release", "acquireRelease", "sequential"].includes(order);
}

function acquireOrder(order) {
  return ["acquire", "acquireRelease", "sequential"].includes(order);
}

function compareExchangeFailureAllowed(success, failure) {
  const allowed = {
    relaxed: ["relaxed"],
    acquire: ["relaxed", "acquire"],
    release: ["relaxed"],
    acquireRelease: ["relaxed", "acquire"],
    sequential: ["relaxed", "acquire", "sequential"],
  };
  return allowed[success]?.includes(failure) ?? false;
}

function latestReleaseFenceBefore(state, eventId) {
  const target = state.events[eventId];
  if (!target?.task) return null;
  const events = Object.entries(state.events);
  for (let index = events.findIndex(([id]) => id === eventId) - 1; index >= 0; index -= 1) {
    const [candidateId, candidate] = events[index];
    if (
      candidate.task === target.task &&
      candidate.kind === "atomicFence" &&
      releaseOrder(candidate.order) &&
      reachable(state, candidateId, eventId)
    ) {
      return candidateId;
    }
  }
  return null;
}

function releaseSourceForObservation(state, observedId) {
  const observed = state.events[observedId];
  if (!observed?.atomicModification) return null;
  const order = atomicModificationOrder(state, observed.atomicLocation);
  const observedIndex = order.indexOf(observedId);
  if (observedIndex < 0) return null;

  for (let index = observedIndex; index >= 0; index -= 1) {
    const eventId = order[index];
    const event = state.events[eventId];
    if (releaseOrder(event.order)) return eventId;
    if (!event.rmw) return latestReleaseFenceBefore(state, eventId);
  }
  return null;
}

function synchronizeAcquire(state, observedId, acquireId, order) {
  if (!observedId || !acquireOrder(order)) return;
  const source = releaseSourceForObservation(state, observedId);
  if (source) addEdge(state, source, acquireId, "atomicReleaseAcquire");
}

function lifecycleEvent(state, operation, kind) {
  return addEvent(state, operation, { task: operation.task, kind });
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "reserveTask": {
      if (state.tasks[operation.task]) {
        throw new ExecutionConcurrencyError("taskAlreadyReserved");
      }
      if (operation.parent) requireTask(state, operation.parent);
      state.tasks[operation.task] = {
        state: "reserved",
        parent: operation.parent ?? null,
        children: [],
        outcome: null,
        everPublished: false,
        cancellation: cancellationRecord(),
      };
      if (operation.parent) state.tasks[operation.parent].children.push(operation.task);
      return;
    }
    case "publishTask": {
      const task = requireTask(state, operation.task);
      requireState(task, "reserved");
      task.state = "published";
      task.everPublished = true;
      const start = lifecycleEvent(state, operation, "taskStart");
      if (operation.after) addEdge(state, operation.after, start, "parentToChild");
      return;
    }
    case "budgetReject": {
      const task = requireTask(state, operation.task);
      requireState(task, "reserved");
      task.state = "outcomeCommitted";
      task.outcome = "canceled";
      addUnique(task.cancellation.exceededBudgets, operation.budget);
      lifecycleEvent(state, operation, "inlineCanceledOutcome");
      return;
    }
    case "settleTask": {
      const task = requireTask(state, operation.task);
      requireState(task, "published");
      if (operation.outcome === "canceled" && !hasCancellation(task)) {
        throw new ExecutionConcurrencyError("canceledWithoutSignal");
      }
      task.state = "bodySettled";
      task.outcome = operation.outcome;
      lifecycleEvent(state, operation, "bodySettled");
      return;
    }
    case "runCleanup": {
      const task = requireTask(state, operation.task);
      requireState(task, "bodySettled");
      task.state = "cleanup";
      lifecycleEvent(state, operation, "cleanupCompleted");
      return;
    }
    case "commitOutcome": {
      const task = requireTask(state, operation.task);
      requireState(task, "cleanup", "outcomeBeforeCleanup");
      task.state = "outcomeCommitted";
      lifecycleEvent(state, operation, "outcomeCommitted");
      return;
    }
    case "joinTask": {
      const task = requireTask(state, operation.task);
      if (task.state === "joined") {
        throw new ExecutionConcurrencyError("taskHandleAlreadyConsumed");
      }
      requireState(task, "outcomeCommitted", "joinBeforeOutcome");
      task.state = "joined";
      const join = addEvent(state, operation, { task: operation.owner, kind: "taskJoin" });
      const committed = state.lastCommittedByTask[operation.task];
      if (!committed) {
        const candidates = Object.entries(state.events).filter(
          ([, event]) =>
            event.task === operation.task &&
            ["outcomeCommitted", "inlineCanceledOutcome"].includes(event.kind),
        );
        state.lastCommittedByTask[operation.task] = candidates.at(-1)?.[0];
      }
      addEdge(state, state.lastCommittedByTask[operation.task], join, "childToJoin");
      return;
    }
    case "requestCancel": {
      requestCancellation(state, operation.task, operation, operation.authority ?? "owner");
      return;
    }
    case "scopeExit": {
      for (const childName of requireTask(state, operation.task).children) {
        const child = requireTask(state, childName);
        if (child.state !== "joined") {
          requestCancellation(state, childName, { scopeExit: true });
        }
      }
      return;
    }
    case "createStorage": {
      requireTask(state, operation.task);
      if (state.storageLifetimes[operation.storage]) {
        throw new ExecutionConcurrencyError("storageIdentityReused");
      }
      addEvent(state, operation, { task: operation.task, kind: "storageCreate" });
      state.storageLifetimes[operation.storage] = {
        live: true,
        owner: operation.task,
        mode: operation.mode,
      };
      return;
    }
    case "destroyStorage": {
      requireTask(state, operation.task);
      const lifetime = state.storageLifetimes[operation.storage];
      if (!lifetime?.live) throw new ExecutionConcurrencyError("storageNotLive");
      if (lifetime.owner !== operation.task) {
        throw new ExecutionConcurrencyError("storageDestroyRequiresOwner");
      }
      if (state.atomicExclusive[operation.storage]) {
        throw new ExecutionConcurrencyError("storageDestroyedDuringExclusivePayload");
      }
      addEvent(state, operation, { task: operation.task, kind: "storageDestroy" });
      lifetime.live = false;
      return;
    }
    case "access": {
      const event = {
        task: operation.task,
        kind: "memoryAccess",
        storage: operation.storage,
        access: operation.access,
        atomic: false,
        ...(operation.offset === undefined ? {} : { offset: operation.offset }),
        ...(operation.bytes === undefined ? {} : { bytes: operation.bytes }),
      };
      checkMemoryAccess(state, event);
      addEvent(state, operation, event);
      return;
    }
    case "domainAdmit": {
      requireTask(state, operation.task);
      requireTask(state, operation.jobTask);
      let domain = state.domains[operation.domain];
      if (!domain) {
        domain = { nextTicket: 0, jobs: {} };
        state.domains[operation.domain] = domain;
      }
      if (Object.values(state.domains).some((candidate) => candidate.jobs[operation.job])) {
        throw new ExecutionConcurrencyError("domainJobAlreadyAdmitted");
      }
      if (operation.ticket !== domain.nextTicket) {
        throw new ExecutionConcurrencyError("domainTicketOutOfOrder");
      }
      if (operation.mode === "barrier" && operation.maySuspend) {
        throw new ExecutionConcurrencyError("barrierMaySuspend");
      }
      const admission = addEvent(state, operation, {
        task: operation.task,
        kind: "domainAdmission",
        domain: operation.domain,
        job: operation.job,
        ticket: operation.ticket,
        mode: operation.mode,
      });
      domain.jobs[operation.job] = {
        task: operation.jobTask,
        ticket: operation.ticket,
        mode: operation.mode,
        state: "admitted",
        admission,
        start: null,
        completion: null,
        outcome: null,
        groupParent: null,
      };
      domain.nextTicket += 1;
      return;
    }
    case "domainAttachChild": {
      requireTask(state, operation.task);
      requireTask(state, operation.jobTask);
      const { domainName, domain, job: parent } = requireDomainJob(state, operation.parentJob);
      if (domainName !== operation.domain) {
        throw new ExecutionConcurrencyError("domainParentMismatch");
      }
      if (parent.task !== operation.task) {
        throw new ExecutionConcurrencyError("domainJobTaskMismatch");
      }
      if (parent.state !== "started") {
        throw new ExecutionConcurrencyError("domainParentNotActive");
      }
      if (parent.mode === "barrier") {
        throw new ExecutionConcurrencyError("domainBarrierCannotCreateChild");
      }
      if (Object.values(state.domains).some((candidate) => candidate.jobs[operation.job])) {
        throw new ExecutionConcurrencyError("domainJobAlreadyAdmitted");
      }
      const attachment = addEvent(state, operation, {
        task: operation.task,
        kind: "domainStructuredChild",
        domain: operation.domain,
        job: operation.job,
        parentJob: operation.parentJob,
        ticket: parent.ticket,
      });
      domain.jobs[operation.job] = {
        task: operation.jobTask,
        ticket: parent.ticket,
        mode: "ordinary",
        state: "admitted",
        admission: attachment,
        start: null,
        completion: null,
        outcome: null,
        groupParent: operation.parentJob,
      };
      return;
    }
    case "domainStart": {
      const { domain, job } = requireDomainJob(state, operation.job);
      if (job.task !== operation.task) {
        throw new ExecutionConcurrencyError("domainJobTaskMismatch");
      }
      if (job.state !== "admitted") {
        throw new ExecutionConcurrencyError("domainJobAlreadyStarted");
      }
      const earlier = Object.values(domain.jobs).filter((candidate) => candidate.ticket < job.ticket);
      if (job.mode === "barrier" && earlier.some((candidate) => candidate.state !== "completed")) {
        throw new ExecutionConcurrencyError("domainBarrierBeforePriorCompletion");
      }
      const earlierBarriers = earlier.filter((candidate) => candidate.mode === "barrier");
      if (job.mode === "ordinary" && earlierBarriers.some((candidate) => candidate.state !== "completed")) {
        throw new ExecutionConcurrencyError("domainStartBeforeBarrierCompletion");
      }
      const start = addEvent(state, operation, {
        task: operation.task,
        kind: "domainJobStart",
        job: operation.job,
        ticket: job.ticket,
        mode: job.mode,
      });
      if (job.mode === "barrier") {
        for (const prior of earlier) addEdge(state, prior.completion, start, "domainPriorToBarrier");
      } else {
        for (const barrier of earlierBarriers) {
          addEdge(state, barrier.completion, start, "domainBarrierToLater");
        }
      }
      job.state = "started";
      job.start = start;
      if (job.groupParent) addEdge(state, job.admission, start, "parentToChild");
      return;
    }
    case "domainComplete": {
      const { job } = requireDomainJob(state, operation.job);
      if (job.task !== operation.task) {
        throw new ExecutionConcurrencyError("domainJobTaskMismatch");
      }
      if (job.state !== "started") {
        throw new ExecutionConcurrencyError("domainJobNotStarted");
      }
      const completion = addEvent(state, operation, {
        task: operation.task,
        kind: "domainJobCommitted",
        job: operation.job,
        ticket: job.ticket,
        mode: job.mode,
        outcome: operation.outcome ?? "success",
      });
      job.state = "completed";
      job.completion = completion;
      job.outcome = operation.outcome ?? "success";
      return;
    }
    case "channelSend":
    case "channelClose": {
      const event = addEvent(state, operation, {
        task: operation.task,
        kind: operation.op,
        channel: operation.channel,
      });
      if (operation.op === "channelSend" && operation.afterCapacity) {
        const source = state.events[operation.afterCapacity];
        if (
          !source ||
          source.kind !== "channelReceive" ||
          source.channel !== operation.channel
        ) {
          throw new ExecutionConcurrencyError("channelCapacityMismatch");
        }
        addEdge(state, operation.afterCapacity, event, "channelCapacityRelease");
      }
      return;
    }
    case "channelReceive":
    case "channelReceiveNone": {
      const receive = addEvent(state, operation, {
        task: operation.task,
        kind: operation.op,
        channel: operation.channel,
      });
      const source = state.events[operation.observes];
      const expectedKind = operation.op === "channelReceive" ? "channelSend" : "channelClose";
      if (!source || source.kind !== expectedKind || source.channel !== operation.channel) {
        throw new ExecutionConcurrencyError("channelObservationMismatch");
      }
      addEdge(
        state,
        operation.observes,
        receive,
        operation.op === "channelReceive" ? "channelSendCommit" : "channelCloseCommit",
      );
      return;
    }
    case "unlock": {
      addEvent(state, operation, { task: operation.task, kind: "unlock", lock: operation.lock });
      return;
    }
    case "lockAcquire": {
      const acquire = addEvent(state, operation, {
        task: operation.task,
        kind: "lockAcquire",
        lock: operation.lock,
      });
      const source = state.events[operation.observes];
      if (!source || source.kind !== "unlock" || source.lock !== operation.lock) {
        throw new ExecutionConcurrencyError("lockObservationMismatch");
      }
      addEdge(state, operation.observes, acquire, "lockReleaseAcquire");
      return;
    }
    case "serviceCall": {
      addEvent(state, operation, {
        task: operation.task,
        kind: "serviceCall",
        service: operation.service,
      });
      return;
    }
    case "serviceTurn": {
      const turn = addEvent(state, operation, {
        task: operation.task,
        kind: "serviceTurn",
        service: operation.service,
      });
      const source = state.events[operation.observes];
      if (!source || source.kind !== "serviceCall" || source.service !== operation.service) {
        throw new ExecutionConcurrencyError("serviceObservationMismatch");
      }
      addEdge(state, operation.observes, turn, "serviceCallToTurn");
      return;
    }
    case "atomicStore": {
      if (!ATOMIC_STORE_ORDERS.has(operation.order)) {
        throw new ExecutionConcurrencyError("invalidAtomicOrder");
      }
      checkAtomicAvailability(state, operation);
      const candidate = atomicEvent(operation, "atomicStore", "write", operation.order, {
        atomicModification: true,
        rmw: false,
      });
      checkMemoryAccess(state, candidate);
      const event = addEvent(state, operation, candidate);
      appendAtomicModification(state, event);
      recordSequentialOperation(state, event);
      return;
    }
    case "atomicLoad": {
      if (!ATOMIC_LOAD_ORDERS.has(operation.order)) {
        throw new ExecutionConcurrencyError("invalidAtomicOrder");
      }
      checkAtomicAvailability(state, operation);
      if (operation.observes) requireObservedModification(state, operation);
      const candidate = atomicEvent(operation, "atomicLoad", "read", operation.order, {
        observes: operation.observes ?? null,
      });
      checkMemoryAccess(state, candidate);
      const event = addEvent(state, operation, candidate);
      recordSequentialOperation(state, event);
      synchronizeAcquire(state, operation.observes, event, operation.order);
      return;
    }
    case "atomicRmw": {
      if (!ATOMIC_ORDERS.has(operation.order)) {
        throw new ExecutionConcurrencyError("invalidAtomicOrder");
      }
      checkAtomicAvailability(state, operation);
      requireLatestModification(state, operation);
      const candidate = atomicEvent(operation, "atomicRmw", "write", operation.order, {
        atomicModification: true,
        rmw: true,
        observes: operation.observes,
      });
      checkMemoryAccess(state, candidate);
      const event = addEvent(state, operation, candidate);
      synchronizeAcquire(state, operation.observes, event, operation.order);
      appendAtomicModification(state, event);
      recordSequentialOperation(state, event);
      return;
    }
    case "atomicCompareExchange": {
      if (!compareExchangeFailureAllowed(operation.successOrder, operation.failureOrder)) {
        throw new ExecutionConcurrencyError("invalidAtomicFailureOrder");
      }
      checkAtomicAvailability(state, operation);
      requireObservedModification(state, operation);
      const exchanged = operation.result === "exchanged";
      if (exchanged) requireLatestModification(state, operation);
      const order = exchanged ? operation.successOrder : operation.failureOrder;
      const candidate = atomicEvent(
        operation,
        exchanged ? "atomicCompareExchangeSuccess" : "atomicCompareExchangeFailure",
        exchanged ? "write" : "read",
        order,
        {
          observes: operation.observes,
          atomicModification: exchanged,
          rmw: exchanged,
          weak: operation.weak ?? false,
        },
      );
      checkMemoryAccess(state, candidate);
      const event = addEvent(state, operation, candidate);
      synchronizeAcquire(state, operation.observes, event, order);
      if (exchanged) appendAtomicModification(state, event);
      recordSequentialOperation(state, event);
      return;
    }
    case "atomicFence": {
      if (!ATOMIC_FENCE_ORDERS.has(operation.order)) {
        throw new ExecutionConcurrencyError("invalidAtomicFenceOrder");
      }
      if (operation.observes) {
        const observedLoad = state.events[operation.observes];
        if (
          !observedLoad?.atomic ||
          !["read", "write"].includes(observedLoad.access) ||
          !observedLoad.observes ||
          observedLoad.task !== operation.task
        ) {
          throw new ExecutionConcurrencyError("atomicFenceObservationMismatch");
        }
      }
      const event = addEvent(state, operation, {
        task: operation.task,
        kind: "atomicFence",
        order: operation.order,
      });
      recordSequentialOperation(state, event);
      if (operation.observes && acquireOrder(operation.order)) {
        const observedLoad = state.events[operation.observes];
        const source = releaseSourceForObservation(state, observedLoad.observes);
        if (source) addEdge(state, source, event, "atomicReleaseAcquire");
      }
      return;
    }
    case "beginAtomicExclusive": {
      requireTask(state, operation.task);
      const lifetime = requireLiveStorage(state, operation.storage);
      if (lifetime?.mode === "ordinary") {
        throw new ExecutionConcurrencyError("mixedAtomicOrdinaryAccess");
      }
      if (lifetime && lifetime.owner !== operation.task) {
        throw new ExecutionConcurrencyError("atomicExclusiveRequiresOwner");
      }
      if (operation.authority !== "inout" && operation.authority !== "consumed") {
        throw new ExecutionConcurrencyError("atomicExclusiveRequiresInout");
      }
      if (state.atomicExclusive[operation.storage]) {
        throw new ExecutionConcurrencyError("atomicPayloadAlreadyExclusive");
      }
      addEvent(state, operation, { task: operation.task, kind: "atomicExclusiveBegin" });
      state.atomicExclusive[operation.storage] = {
        task: operation.task,
        token: operation.token,
        authority: operation.authority,
      };
      return;
    }
    case "atomicPayloadAccess": {
      const exclusive = state.atomicExclusive[operation.storage];
      if (
        !exclusive ||
        exclusive.task !== operation.task ||
        exclusive.token !== operation.token
      ) {
        throw new ExecutionConcurrencyError("atomicPayloadRequiresExclusive");
      }
      const candidate = {
        task: operation.task,
        kind: "atomicPayloadAccess",
        storage: operation.storage,
        access: operation.access,
        atomic: false,
        exclusiveAtomic: true,
        ...(operation.offset === undefined ? {} : { offset: operation.offset }),
        ...(operation.bytes === undefined ? {} : { bytes: operation.bytes }),
      };
      checkMemoryAccess(state, candidate);
      addEvent(state, operation, candidate);
      return;
    }
    case "endAtomicExclusive": {
      const exclusive = state.atomicExclusive[operation.storage];
      if (
        !exclusive ||
        exclusive.task !== operation.task ||
        exclusive.token !== operation.token
      ) {
        throw new ExecutionConcurrencyError("atomicExclusiveTokenMismatch");
      }
      addEvent(state, operation, { task: operation.task, kind: "atomicExclusiveEnd" });
      delete state.atomicExclusive[operation.storage];
      return;
    }
    case "verifyModificationOrder": {
      const actual = atomicModificationOrder(state, atomicLocationKey(operation));
      if (JSON.stringify(actual) !== JSON.stringify(operation.events)) {
        throw new ExecutionConcurrencyError("unexpectedAtomicModificationOrder");
      }
      return;
    }
    case "verifySequentialOrder": {
      if (JSON.stringify(state.sequentialOrder) !== JSON.stringify(operation.events)) {
        throw new ExecutionConcurrencyError("unexpectedSequentialOrder");
      }
      return;
    }
    case "requestFailFast": {
      const tasks = operation.tasks.map((name) => requireTask(state, name));
      const triggerIndex = operation.tasks.indexOf(operation.trigger);
      if (
        triggerIndex < 0 ||
        tasks[triggerIndex].outcome !== "error" ||
        tasks[triggerIndex].state === "published"
      ) {
        throw new ExecutionConcurrencyError("failFastWithoutSettledTrigger");
      }
      for (const [index, task] of tasks.entries()) {
        if (index !== triggerIndex && task.state === "published") {
          requestCancellation(state, operation.tasks[index], { siblingFailure: true });
        }
      }
      state.lastFailFastTrigger = operation.trigger;
      return;
    }
    case "arbitrateTuple": {
      const tasks = operation.tasks.map((name) => requireTask(state, name));
      if (tasks.some((task) => task.state !== "outcomeCommitted")) {
        throw new ExecutionConcurrencyError("arbitrationBeforeDrain");
      }
      const primaryIndex = tasks.findIndex((task) => task.outcome === "error");
      if (primaryIndex < 0) throw new ExecutionConcurrencyError("arbitrationWithoutError");
      state.lastArbitration = operation.tasks[primaryIndex];
      return;
    }
    case "verifyRaceFreedom": {
      const race = racePair(state);
      if (race) {
        state.lastRace = race;
        throw new ExecutionConcurrencyError("dataRace");
      }
      return;
    }
    case "verifyTask": {
      const task = requireTask(state, operation.task);
      if (operation.state && task.state !== operation.state) {
        throw new ExecutionConcurrencyError("unexpectedTaskState");
      }
      if (operation.outcome && task.outcome !== operation.outcome) {
        throw new ExecutionConcurrencyError("unexpectedTaskOutcome");
      }
      if (operation.reason && !task.cancellation.requestedReasons.includes(operation.reason)) {
        throw new ExecutionConcurrencyError("missingCancellationReason");
      }
      if (operation.budget && !task.cancellation.exceededBudgets.includes(operation.budget)) {
        throw new ExecutionConcurrencyError("missingCancellationBudget");
      }
      for (const field of ["deadlineExceeded", "scopeExited", "siblingFailed"]) {
        if (operation[field] !== undefined && task.cancellation[field] !== operation[field]) {
          throw new ExecutionConcurrencyError("unexpectedCancellationCause");
        }
      }
      if (
        operation.nearestAncestor !== undefined &&
        task.cancellation.nearestAncestor !== operation.nearestAncestor
      ) {
        throw new ExecutionConcurrencyError("unexpectedCancellationAncestor");
      }
      if (
        operation.everPublished !== undefined &&
        task.everPublished !== operation.everPublished
      ) {
        throw new ExecutionConcurrencyError("unexpectedPublicationState");
      }
      return;
    }
    case "verifyArbitration": {
      if (state.lastArbitration !== operation.primary) {
        throw new ExecutionConcurrencyError("unexpectedErrorArbitration");
      }
      return;
    }
    case "verifyChildrenJoined": {
      const task = requireTask(state, operation.task);
      if (task.children.some((child) => requireTask(state, child).state !== "joined")) {
        throw new ExecutionConcurrencyError("unjoinedChild");
      }
      return;
    }
    default:
      throw new ExecutionConcurrencyError("unknownExecutionOperation");
  }
}

function hasCancellation(task) {
  const cancellation = task.cancellation;
  return (
    cancellation.requestedReasons.length > 0 ||
    cancellation.exceededBudgets.length > 0 ||
    cancellation.deadlineExceeded ||
    cancellation.scopeExited ||
    cancellation.siblingFailed ||
    cancellation.nearestAncestor !== null
  );
}

export function validateExecutionOperation(operation) {
  if (!operation || typeof operation !== "object" || typeof operation.op !== "string") {
    return false;
  }
  const string = (field) => typeof operation[field] === "string" && operation[field].length > 0;
  const idTask = () => string("id") && string("task");
  const memoryRange = () =>
    (operation.offset === undefined ||
      (Number.isInteger(operation.offset) && operation.offset >= 0)) &&
    (operation.bytes === undefined ||
      (Number.isInteger(operation.bytes) && operation.bytes > 0));
  switch (operation.op) {
    case "reserveTask":
      return string("task") && (operation.parent === undefined || string("parent"));
    case "publishTask":
      return idTask() && (operation.after === undefined || string("after"));
    case "budgetReject":
      return idTask() && string("budget");
    case "settleTask":
      return idTask() && OUTCOMES.has(operation.outcome);
    case "runCleanup":
    case "commitOutcome":
      return idTask();
    case "joinTask":
      return idTask() && string("owner");
    case "requestCancel":
      return string("task") && [undefined, "owner", "shared"].includes(operation.authority);
    case "scopeExit":
    case "verifyChildrenJoined":
      return string("task");
    case "createStorage":
      return (
        idTask() &&
        string("storage") &&
        ["ordinary", "atomic"].includes(operation.mode)
      );
    case "destroyStorage":
      return idTask() && string("storage");
    case "access":
      return (
        idTask() &&
        string("storage") &&
        ACCESS_KINDS.has(operation.access) &&
        memoryRange()
      );
    case "domainAdmit":
      return (
        idTask() &&
        string("job") &&
        string("jobTask") &&
        string("domain") &&
        Number.isInteger(operation.ticket) &&
        operation.ticket >= 0 &&
        DISPATCH_MODES.has(operation.mode) &&
        (operation.maySuspend === undefined || typeof operation.maySuspend === "boolean")
      );
    case "domainStart":
      return idTask() && string("job");
    case "domainAttachChild":
      return (
        idTask() &&
        string("job") &&
        string("jobTask") &&
        string("domain") &&
        string("parentJob")
      );
    case "domainComplete":
      return (
        idTask() &&
        string("job") &&
        (operation.outcome === undefined || OUTCOMES.has(operation.outcome))
      );
    case "channelSend":
      return (
        idTask() &&
        string("channel") &&
        (operation.afterCapacity === undefined || string("afterCapacity"))
      );
    case "channelClose":
      return idTask() && string("channel");
    case "channelReceive":
    case "channelReceiveNone":
      return idTask() && string("channel") && string("observes");
    case "unlock":
      return idTask() && string("lock");
    case "lockAcquire":
      return idTask() && string("lock") && string("observes");
    case "serviceCall":
      return idTask() && string("service");
    case "serviceTurn":
      return idTask() && string("service") && string("observes");
    case "atomicStore":
      return (
        idTask() &&
        string("storage") &&
        ATOMIC_ORDERS.has(operation.order) &&
        memoryRange()
      );
    case "atomicLoad":
      return (
        idTask() &&
        string("storage") &&
        ATOMIC_ORDERS.has(operation.order) &&
        (operation.observes === undefined || string("observes")) &&
        memoryRange()
      );
    case "atomicRmw":
      return (
        idTask() &&
        string("storage") &&
        string("observes") &&
        ATOMIC_ORDERS.has(operation.order) &&
        memoryRange()
      );
    case "atomicCompareExchange":
      return (
        idTask() &&
        string("storage") &&
        string("observes") &&
        ATOMIC_ORDERS.has(operation.successOrder) &&
        ATOMIC_ORDERS.has(operation.failureOrder) &&
        COMPARE_EXCHANGE_RESULTS.has(operation.result) &&
        (operation.weak === undefined || typeof operation.weak === "boolean") &&
        memoryRange()
      );
    case "atomicFence":
      return (
        idTask() &&
        ATOMIC_ORDERS.has(operation.order) &&
        (operation.observes === undefined || string("observes"))
      );
    case "beginAtomicExclusive":
      return (
        idTask() &&
        string("storage") &&
        string("token") &&
        ATOMIC_EXCLUSIVE_AUTHORITIES.has(operation.authority)
      );
    case "atomicPayloadAccess":
      return (
        idTask() &&
        string("storage") &&
        string("token") &&
        ACCESS_KINDS.has(operation.access) &&
        memoryRange()
      );
    case "endAtomicExclusive":
      return idTask() && string("storage") && string("token");
    case "verifyModificationOrder":
      return (
        string("storage") &&
        Array.isArray(operation.events) &&
        operation.events.every((event) => typeof event === "string" && event.length > 0) &&
        memoryRange()
      );
    case "verifySequentialOrder":
      return (
        Array.isArray(operation.events) &&
        operation.events.every((event) => typeof event === "string" && event.length > 0)
      );
    case "requestFailFast":
      return (
        Array.isArray(operation.tasks) &&
        operation.tasks.length > 1 &&
        operation.tasks.every((task) => typeof task === "string" && task.length > 0) &&
        string("trigger")
      );
    case "arbitrateTuple":
      return (
        Array.isArray(operation.tasks) &&
        operation.tasks.length > 1 &&
        operation.tasks.every((task) => typeof task === "string" && task.length > 0)
      );
    case "verifyRaceFreedom":
      return true;
    case "verifyTask":
      return (
        string("task") &&
        (operation.state === undefined || TASK_STATES.has(operation.state)) &&
        (operation.outcome === undefined || OUTCOMES.has(operation.outcome))
      );
    case "verifyArbitration":
      return string("primary");
    default:
      return false;
  }
}

export function runExecutionProgram(operations) {
  const state = {
    tasks: {},
    events: {},
    edges: [],
    lastEventByTask: {},
    lastCommittedByTask: {},
    storageLifetimes: {},
    atomicModificationOrder: {},
    sequentialOrder: [],
    atomicExclusive: {},
    domains: {},
    lastFailFastTrigger: null,
    lastArbitration: null,
    lastRace: null,
  };
  const trace = [];

  for (const [index, operation] of operations.entries()) {
    const before = clone(state);
    try {
      applyOperation(state, operation);
      if (operation.op === "commitOutcome" || operation.op === "budgetReject") {
        state.lastCommittedByTask[operation.task] = operation.id;
      }
      trace.push({ index, operation: clone(operation), before, after: clone(state) });
    } catch (error) {
      if (!(error instanceof ExecutionConcurrencyError)) throw error;
      trace.push({ index, operation: clone(operation), before, rejected: error.code });
      return { status: "rejected", code: error.code, operation: index, state, trace };
    }
  }

  return { status: "accepted", state, trace };
}
