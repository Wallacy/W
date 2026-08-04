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
  const accesses = Object.entries(state.events).filter(([, event]) => event.storage);
  for (let leftIndex = 0; leftIndex < accesses.length; leftIndex += 1) {
    const [leftId, left] = accesses[leftIndex];
    for (let rightIndex = leftIndex + 1; rightIndex < accesses.length; rightIndex += 1) {
      const [rightId, right] = accesses[rightIndex];
      if (left.storage !== right.storage || left.task === right.task) continue;
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
    case "access": {
      addEvent(state, operation, {
        task: operation.task,
        kind: "memoryAccess",
        storage: operation.storage,
        access: operation.access,
        atomic: false,
      });
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
    case "atomicStore":
    case "atomicLoad": {
      const event = addEvent(state, operation, {
        task: operation.task,
        kind: operation.op,
        storage: operation.storage,
        access: operation.op === "atomicStore" ? "write" : "read",
        atomic: true,
        order: operation.order,
      });
      if (operation.op === "atomicLoad" && operation.observes) {
        const source = state.events[operation.observes];
        if (
          !source ||
          source.kind !== "atomicStore" ||
          source.storage !== operation.storage
        ) {
          throw new ExecutionConcurrencyError("atomicObservationMismatch");
        }
        if (releaseOrder(source.order) && acquireOrder(operation.order)) {
          addEdge(state, operation.observes, event, "atomicReleaseAcquire");
        }
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
    case "access":
      return idTask() && string("storage") && ACCESS_KINDS.has(operation.access);
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
      return idTask() && string("storage") && ATOMIC_ORDERS.has(operation.order);
    case "atomicLoad":
      return (
        idTask() &&
        string("storage") &&
        ATOMIC_ORDERS.has(operation.order) &&
        (operation.observes === undefined || string("observes"))
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
