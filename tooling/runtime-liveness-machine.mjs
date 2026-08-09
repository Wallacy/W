const BODY_PHASES = new Set(["reserved", "published", "active", "settled"]);
const CLOSURE_PHASES = new Set([
  "open",
  "closing",
  "childWaitDrain",
  "explicitCleanup",
  "typedDrop",
  "runtimeQuiescent",
  "committed",
]);
const OBSERVATION_PHASES = new Set(["unavailable", "committed", "joined", "retained"]);
const STORAGE_PHASES = new Set(["live", "retired", "reclaimable"]);
const WAIT_PHASES = new Set(["registered", "submitted", "completing", "terminal", "drained"]);
const SHUTDOWN_PHASES = new Set([
  "ready",
  "admissionClosed",
  "cancellationRequested",
  "draining",
  "quiescent",
  "stopped",
  "terminating",
  "terminated",
]);
const OUTCOMES = new Set(["success", "error", "canceled"]);
const CLEANUP_KINDS = new Set(["defer", "deferAsync"]);
const RESOURCE_KINDS = new Set(["registration", "queue", "timer", "waker"]);
const COMPLETION_OUTCOMES = new Set(["success", "error", "canceled"]);

export class RuntimeLivenessError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

function clone(value) {
  return structuredClone(value);
}

function fail(code) {
  throw new RuntimeLivenessError(code);
}

function requireTask(state, name) {
  const task = state.tasks[name];
  if (!task) fail("unknownTask");
  return task;
}

function requirePhase(value, expected, code) {
  if (value !== expected) fail(code);
}

function requireWait(state, operationId, generation) {
  const key = `${operationId}@${generation}`;
  const wait = state.waits[key];
  if (!wait) fail("unknownWait");
  return wait;
}

function activeWaits(state, taskName, includeCleanup = true) {
  return Object.values(state.waits).filter(
    (wait) =>
      wait.task === taskName &&
      (includeCleanup || wait.owner !== "cleanup") &&
      !["drained"].includes(wait.phase),
  );
}

function taskHasFault(task) {
  return task.faultBoundary !== null;
}

function addUnique(list, value) {
  if (!list.includes(value)) list.push(value);
}

function allResourcesDrained(state, taskName) {
  return Object.values(state.resources).every(
    (resource) => resource.task !== taskName || resource.drained,
  );
}

function allChildrenDrained(state, task) {
  return task.children.every((childName) => {
    const child = requireTask(state, childName);
    return child.closure === "committed";
  });
}

function completeWait(state, operation) {
  const wait = requireWait(state, operation.operationId, operation.generation);
  if (!COMPLETION_OUTCOMES.has(operation.outcome)) fail("invalidProviderOutcome");
  if (wait.phase === "terminal" || wait.phase === "drained") {
    wait.disposition = "lateDrained";
    wait.completionDisposition = "lateDrained";
    wait.callbackSuppressed = true;
    wait.callback = "suppressed";
    wait.phase = "drained";
    return;
  }
  requirePhase(wait.phase, "completing", "waitCompletionOutOfOrder");

  const task = requireTask(state, wait.task);
  if (wait.generation !== operation.generation) {
    wait.disposition = "staleGeneration";
    wait.completionDisposition = "staleGeneration";
    wait.callbackSuppressed = true;
  } else if (operation.generation !== state.generations[wait.service]) {
    wait.disposition = "staleGeneration";
    wait.completionDisposition = "staleGeneration";
    wait.callbackSuppressed = true;
  } else {
    wait.outcome = operation.outcome;
    wait.providerOutcome = operation.outcome;
    wait.disposition =
      operation.outcome === "success"
        ? "selectedSuccess"
        : operation.outcome === "error"
          ? "selectedError"
          : "selectedCanceled";
    wait.completionDisposition = wait.disposition;
    wait.callbackSuppressed = task.closure !== "open";
  }
  wait.phase = "terminal";
  wait.owner = operation.ownerDisposition ?? wait.owner;
  if (wait.callbackSuppressed) wait.callback = "suppressed";
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "createTask": {
      if (state.tasks[operation.task]) fail("taskAlreadyCreated");
      if (operation.parent !== undefined) requireTask(state, operation.parent);
      state.tasks[operation.task] = {
        body: "reserved",
        closure: "open",
        observation: "unavailable",
        storage: "live",
        frame: "live",
        parent: operation.parent ?? null,
        children: [],
        childrenHistory: [],
        initialization: [],
        drops: [],
        cleanup: [],
        cleanupExecuted: [],
        cleanupNode: null,
        cleanupHistory: [],
        cleanupErrors: [],
        cleanupEvidence: [],
        cancellationRequested: false,
        cancellationReasons: [],
        cancellationDisposition: null,
        outcomeCandidate: null,
        outcome: null,
        outcomeMoved: false,
        faultBoundary: null,
        runtimeRefs: 0,
        outcomeRefs: 0,
        diagnostics: [],
        admissionClosed: false,
        blocking: false,
        killableBoundary: false,
        graceFinite: true,
        cleanupAllocation: 0,
        cleanupAllocationLimit: operation.cleanupAllocationLimit ?? 0,
        service: operation.service ?? "default",
      };
      state.generations[operation.service ?? "default"] ??= 1;
      if (operation.parent !== undefined) state.tasks[operation.parent].children.push(operation.task);
      return;
    }
    case "publishTask": {
      const task = requireTask(state, operation.task);
      requirePhase(task.body, "reserved", "invalidBodyPublication");
      task.body = "published";
      return;
    }
    case "startBody": {
      const task = requireTask(state, operation.task);
      requirePhase(task.body, "published", "invalidBodyStart");
      task.body = "active";
      return;
    }
    case "initialize": {
      const task = requireTask(state, operation.task);
      if (task.body !== "active" || task.closure !== "open") fail("invalidInitialization");
      if (typeof operation.label !== "string" || operation.label.length === 0) fail("invalidInitialization");
      task.initialization.push(operation.label);
      return;
    }
    case "settleBody": {
      const task = requireTask(state, operation.task);
      requirePhase(task.body, "active", "invalidBodySettlement");
      if (!OUTCOMES.has(operation.outcome)) fail("invalidBodyOutcome");
      task.body = "settled";
      task.outcomeCandidate = operation.outcome;
      return;
    }
    case "beginClosure": {
      const task = requireTask(state, operation.task);
      requirePhase(task.body, "settled", "closureBeforeBodySettlement");
      requirePhase(task.closure, "open", "closureAlreadyStarted");
      task.closure = "closing";
      task.admissionClosed = true;
      return;
    }
    case "requestCancellation": {
      const task = requireTask(state, operation.task);
      if (task.closure === "committed") {
        task.cancellationDisposition = "late";
        return;
      }
      task.cancellationRequested = true;
      task.cancellationDisposition = "requested";
      addUnique(task.cancellationReasons, operation.reason ?? "shutdown");
      if (task.cleanupNode && operation.reason !== "local") {
        task.cleanupNode.cancellationReceived = true;
      }
      return;
    }
    case "createChild": {
      const parent = requireTask(state, operation.task);
      const childName = operation.child;
      if (parent.children.includes(childName) || state.tasks[childName]) fail("childAlreadyCreated");
      const cleanupChild = operation.owner === "cleanup";
      if (parent.closure === "open") {
        if (parent.body !== "active") fail("childBodyNotActive");
        if (cleanupChild) fail("cleanupChildRequiresNode");
      } else if (
        !(
          parent.closure === "explicitCleanup" &&
          cleanupChild &&
          parent.cleanupNode &&
          parent.cleanupNode.label === operation.cleanupNode &&
          parent.cleanupNode.kind === "deferAsync"
        )
      ) {
        fail("childAdmissionClosed");
      }
      state.tasks[childName] = {
        body: "reserved",
        closure: "open",
        observation: "unavailable",
        storage: "live",
        frame: "live",
        parent: operation.task,
        children: [],
        childrenHistory: [],
        initialization: [],
        drops: [],
        cleanup: [],
        cleanupExecuted: [],
        cleanupNode: null,
        cleanupHistory: [],
        cleanupErrors: [],
        cleanupEvidence: [],
        cancellationRequested: false,
        cancellationReasons: [],
        cancellationDisposition: null,
        outcomeCandidate: null,
        outcome: null,
        outcomeMoved: false,
        faultBoundary: null,
        runtimeRefs: 0,
        outcomeRefs: 0,
        diagnostics: [],
        admissionClosed: false,
        blocking: false,
        killableBoundary: false,
        graceFinite: true,
        cleanupAllocation: 0,
        cleanupAllocationLimit: 0,
        service: parent.service,
        cleanupOwner: cleanupChild,
        cleanupNodeLabel: cleanupChild ? operation.cleanupNode : null,
      };
      parent.children.push(childName);
      return;
    }
    case "drainChildren": {
      const task = requireTask(state, operation.task);
      requirePhase(task.closure, "closing", "childrenDrainOutOfOrder");
      if (!allChildrenDrained(state, task)) fail("childrenNotDrained");
      if (activeWaits(state, operation.task, false).length > 0) fail("preexistingWaitNotDrained");
      task.childrenHistory.push(...task.children.map((child) => ({ name: child, owner: "task" })));
      task.children = [];
      task.closure = "childWaitDrain";
      return;
    }
    case "beginCleanup": {
      const task = requireTask(state, operation.task);
      requirePhase(task.closure, "childWaitDrain", "cleanupOutOfOrder");
      task.closure = "explicitCleanup";
      return;
    }
    case "registerCleanup": {
      const task = requireTask(state, operation.task);
      if (task.body !== "active" || task.closure !== "open") fail("cleanupRegistrationClosed");
      if (!CLEANUP_KINDS.has(operation.kind)) fail("invalidCleanupKind");
      task.cleanup.push({ label: operation.label, kind: operation.kind });
      return;
    }
    case "startCleanupNode": {
      const task = requireTask(state, operation.task);
      requirePhase(task.closure, "explicitCleanup", "cleanupNodeOutOfOrder");
      if (task.cleanupNode) fail("cleanupNodeAlreadyActive");
      const cleanup = task.cleanup.at(-1);
      if (!cleanup || cleanup.label !== operation.label) fail("cleanupNotLifo");
      if (cleanup.kind !== "deferAsync") fail("cleanupNodeRequiresAsyncDefer");
      task.cleanup.pop();
      task.cleanupNode = {
        label: cleanup.label,
        kind: cleanup.kind,
        cancellationReceived: false,
        mask: "active",
      };
      return;
    }
    case "finishCleanupNode": {
      const task = requireTask(state, operation.task);
      requirePhase(task.closure, "explicitCleanup", "cleanupNodeOutOfOrder");
      const node = task.cleanupNode;
      if (!node || node.label !== operation.label) fail("cleanupNodeNotActive");
      if (activeWaits(state, operation.task, true).some(
        (wait) => wait.owner === "cleanup" && wait.cleanupNodeLabel === node.label,
      )) {
        fail("cleanupWaitNotDrained");
      }
      if (task.children.some((childName) => {
        const child = requireTask(state, childName);
        return child.cleanupOwner && child.cleanupNodeLabel === node.label && child.closure !== "committed";
      })) {
        fail("cleanupChildNotCommitted");
      }
      const drainedCleanupChildren = task.children.filter((childName) => {
        const child = requireTask(state, childName);
        return child.cleanupOwner && child.cleanupNodeLabel === node.label;
      });
      task.childrenHistory.push(...drainedCleanupChildren.map((child) => ({ name: child, owner: "cleanup", node: node.label })));
      task.children = task.children.filter((childName) => !drainedCleanupChildren.includes(childName));
      task.cleanupExecuted.push({ label: node.label, kind: node.kind });
      task.cleanupHistory.push({ ...node });
      task.cleanupNode = null;
      return;
    }
    case "runCleanup": {
      const task = requireTask(state, operation.task);
      requirePhase(task.closure, "explicitCleanup", "cleanupOutOfOrder");
      if (task.cleanupNode) fail("cleanupNodeAlreadyActive");
      const cleanup = task.cleanup.at(-1);
      if (!cleanup || cleanup.label !== operation.label) fail("cleanupNotLifo");
      if (cleanup.kind !== "defer") fail("cleanupNodeRequiresStart");
      if (activeWaits(state, operation.task, true).some((wait) => wait.owner === "cleanup")) {
        fail("cleanupWaitNotDrained");
      }
      task.cleanup.pop();
      task.cleanupExecuted.push(cleanup);
      return;
    }
    case "finishCleanup": {
      const task = requireTask(state, operation.task);
      requirePhase(task.closure, "explicitCleanup", "cleanupOutOfOrder");
      if (task.cleanup.length > 0) fail("cleanupNotComplete");
      if (task.cleanupNode) fail("cleanupNodeNotFinished");
      if (activeWaits(state, operation.task, true).length > 0) fail("cleanupWaitNotDrained");
      task.closure = "typedDrop";
      return;
    }
    case "cleanupAllocate": {
      const task = requireTask(state, operation.task);
      if (task.closure !== "explicitCleanup") fail("cleanupAllocationOutOfOrder");
      task.cleanupAllocation += operation.bytes;
      if (task.cleanupAllocationLimit !== null && task.cleanupAllocation > task.cleanupAllocationLimit) {
        fail("cleanupAllocationBudgetExceeded");
      }
      return;
    }
    case "cleanupError": {
      const task = requireTask(state, operation.task);
      if (task.closure !== "explicitCleanup") fail("cleanupErrorOutOfOrder");
      if (operation.kind === "captured") {
        if (task.cleanupEvidence.length >= 4) fail("cleanupEvidenceBoundExceeded");
        task.cleanupEvidence.push(operation.error ?? "cleanup");
      } else if (operation.kind === "uncaught") {
        task.cleanupErrors.push(operation.error ?? "cleanup");
        task.diagnostics.push("uncaughtCleanupError");
      } else {
        fail("invalidCleanupErrorKind");
      }
      return;
    }
    case "cleanupPanic": {
      const task = requireTask(state, operation.task);
      if (task.closure !== "explicitCleanup") fail("cleanupPanicOutOfOrder");
      if (task.cleanupNode) task.cleanupNode.mask = "faulted";
      task.faultBoundary = "cleanupPanic";
      return;
    }
    case "cleanupTimeout": {
      const task = requireTask(state, operation.task);
      if (task.closure !== "explicitCleanup") fail("cleanupTimeoutOutOfOrder");
      if (task.cleanupNode) task.cleanupNode.mask = "expired";
      task.faultBoundary = "cleanupDeadline";
      return;
    }
    case "typedDrop": {
      const task = requireTask(state, operation.task);
      requirePhase(task.closure, "typedDrop", "typedDropOutOfOrder");
      if (task.cleanup.length > 0) fail("cleanupNotComplete");
      if (activeWaits(state, operation.task, true).length > 0) fail("cleanupWaitNotDrained");
      const expected = task.initialization.at(-1);
      if (expected !== operation.label) fail("typedDropOrder");
      task.initialization.pop();
      task.drops.push(operation.label);
      return;
    }
    case "drainRuntime": {
      const task = requireTask(state, operation.task);
      requirePhase(task.closure, "typedDrop", "runtimeDrainOutOfOrder");
      if (task.initialization.length > 0) fail("typedDropsNotComplete");
      if (!allResourcesDrained(state, operation.task)) fail("runtimeResourcesNotDrained");
      if (activeWaits(state, operation.task, true).length > 0) fail("waitsNotDrained");
      task.closure = "runtimeQuiescent";
      return;
    }
    case "commitOutcome": {
      const task = requireTask(state, operation.task);
      requirePhase(task.closure, "runtimeQuiescent", "outcomeBeforeRuntimeQuiescence");
      if (task.faultBoundary) fail("faultBoundaryHasNoNormalOutcome");
      task.closure = "committed";
      task.observation = "committed";
      task.outcome = task.outcomeCandidate;
      return;
    }
    case "registerWait": {
      const task = requireTask(state, operation.task);
      if (!Number.isInteger(operation.generation) || operation.generation < 1) fail("invalidGeneration");
      state.generations[operation.service ?? task.service] ??= 1;
      if (task.closure === "open" && operation.owner === "cleanup") fail("cleanupWaitRequiresNode");
      if (task.closure === "open" && task.body !== "active") fail("waitBodyNotActive");
      if (task.closure === "explicitCleanup") {
        if (operation.owner !== "cleanup" || !task.cleanupNode || task.cleanupNode.kind !== "deferAsync") {
          fail("cleanupWaitRequiresNode");
        }
      }
      const key = `${operation.operationId}@${operation.generation}`;
      if (state.waits[key]) fail("waitAlreadyRegistered");
      if (!["open", "explicitCleanup"].includes(task.closure)) fail("waitRegistrationClosed");
      state.waits[key] = {
        operationId: operation.operationId,
        generation: operation.generation,
        service: operation.service ?? task.service,
        task: operation.task,
        owner: operation.owner ?? "frame",
        cleanupNodeLabel: operation.owner === "cleanup" ? task.cleanupNode?.label ?? null : null,
        phase: "registered",
        cancelRequested: false,
        cancelDisposition: null,
        outcome: null,
        providerOutcome: null,
        disposition: null,
        completionDisposition: null,
        callback: "pending",
        callbackSuppressed: false,
      };
      return;
    }
    case "submitWait": {
      const wait = requireWait(state, operation.operationId, operation.generation);
      requirePhase(wait.phase, "registered", "waitSubmitOutOfOrder");
      wait.phase = "submitted";
      return;
    }
    case "startWait": {
      const wait = requireWait(state, operation.operationId, operation.generation);
      if (!["submitted"].includes(wait.phase)) fail("waitStartOutOfOrder");
      wait.phase = "completing";
      return;
    }
    case "cancelWait": {
      const wait = requireWait(state, operation.operationId, operation.generation);
      if (wait.phase === "registered") {
        wait.cancelRequested = true;
        wait.cancelDisposition = "localDrained";
        wait.phase = "drained";
        return;
      }
      if (["terminal", "drained"].includes(wait.phase)) {
        wait.cancelRequested = true;
        wait.cancelDisposition = "late";
        return;
      }
      wait.cancelRequested = true;
      wait.cancelDisposition = "requested";
      return;
    }
    case "completeWait": {
      completeWait(state, operation);
      return;
    }
    case "drainWait": {
      const wait = requireWait(state, operation.operationId, operation.generation);
      if (wait.phase === "drained") return;
      requirePhase(wait.phase, "terminal", "waitDrainBeforeCompletion");
      wait.phase = "drained";
      if (wait.owner === "cleanup") wait.cleanupOwner = "released";
      return;
    }
    case "registerResource": {
      const task = requireTask(state, operation.task);
      if (task.closure !== "explicitCleanup") fail("resourceRegistrationOutOfOrder");
      if (!RESOURCE_KINDS.has(operation.kind)) fail("invalidResourceKind");
      if (!task.cleanupNode || task.cleanupNode.kind !== "deferAsync") fail("resourceRequiresCleanupNode");
      const key = `${operation.kind}:${operation.resource}`;
      if (state.resources[key]) fail("resourceAlreadyRegistered");
      state.resources[key] = {
        task: operation.task,
        kind: operation.kind,
        drained: false,
        cleanupNodeLabel: task.cleanupNode.label,
      };
      return;
    }
    case "drainResource": {
      const key = `${operation.kind}:${operation.resource}`;
      const resource = state.resources[key];
      if (!resource) fail("unknownResource");
      if (resource.task !== operation.task) fail("resourceOwnerMismatch");
      resource.drained = true;
      return;
    }
    case "moveOutcome": {
      const task = requireTask(state, operation.task);
      requirePhase(task.observation, "committed", "outcomeNotCommitted");
      if (task.outcomeMoved) fail("outcomeAlreadyMoved");
      task.outcomeMoved = true;
      task.outcomeRefs += 1;
      return;
    }
    case "retireFrame": {
      const task = requireTask(state, operation.task);
      requirePhase(task.closure, "committed", "frameRetireBeforeClosure");
      if (!task.outcomeMoved) fail("frameRetireBeforeOutcomeMove");
      requirePhase(task.frame, "live", "frameAlreadyRetired");
      task.frame = "retired";
      return;
    }
    case "retainRuntimeRef": {
      const task = requireTask(state, operation.task);
      task.runtimeRefs += 1;
      return;
    }
    case "releaseRuntimeRef": {
      const task = requireTask(state, operation.task);
      if (task.runtimeRefs === 0) fail("runtimeRefUnderflow");
      task.runtimeRefs -= 1;
      return;
    }
    case "reclaimFrame": {
      const task = requireTask(state, operation.task);
      requirePhase(task.frame, "retired", "frameReclaimBeforeRetirement");
      if (task.children.length > 0) fail("frameChildrenLive");
      if (activeWaits(state, operation.task, true).length > 0) fail("frameRegistrationsLive");
      if (!allResourcesDrained(state, operation.task)) fail("frameResourcesLive");
      if (task.runtimeRefs > 0) fail("frameRuntimeRefsLive");
      task.frame = "reclaimed";
      return;
    }
    case "joinObservation": {
      const task = requireTask(state, operation.task);
      requirePhase(task.observation, "committed", "joinBeforeOutcome");
      task.observation = "joined";
      if (task.outcomeRefs > 0) task.outcomeRefs -= 1;
      return;
    }
    case "retainObservation": {
      const task = requireTask(state, operation.task);
      requirePhase(task.observation, "committed", "retainBeforeOutcome");
      task.observation = "retained";
      task.outcomeRefs += 1;
      return;
    }
    case "expireRetention": {
      const task = requireTask(state, operation.task);
      requirePhase(task.observation, "retained", "retentionNotActive");
      task.observation = "unavailable";
      if (task.outcomeRefs > 0) task.outcomeRefs -= 1;
      return;
    }
    case "retireStorage": {
      const task = requireTask(state, operation.task);
      requirePhase(task.storage, "live", "storageAlreadyRetired");
      if (task.frame !== "reclaimed") fail("storageRetireBeforeFrameReclaim");
      task.storage = "retired";
      return;
    }
    case "reclaimStorage": {
      const task = requireTask(state, operation.task);
      requirePhase(task.storage, "retired", "storageReclaimBeforeRetirement");
      if (task.frame !== "reclaimed") fail("storageFrameStillLive");
      if (task.outcomeRefs > 0) fail("outcomeCellStillReferenced");
      task.storage = "reclaimable";
      return;
    }
    case "enterBlocking": {
      const task = requireTask(state, operation.task);
      if (task.body !== "active") fail("blockingOutsideBody");
      if (operation.graceFinite === false && operation.killable !== true) {
        fail("unboundedBlockingAdapter");
      }
      task.blocking = true;
      task.killableBoundary = operation.killable === true;
      task.graceFinite = operation.graceFinite !== false;
      return;
    }
    case "returnBlocking": {
      const task = requireTask(state, operation.task);
      if (!task.blocking) fail("blockingNotActive");
      task.blocking = false;
      return;
    }
    case "killBlocking": {
      const task = requireTask(state, operation.task);
      if (!task.blocking || !task.killableBoundary) fail("blockingBoundaryNotKillable");
      task.blocking = false;
      task.faultBoundary = "forcedTermination";
      return;
    }
    case "claimDeadlockFreedom": {
      requireTask(state, operation.task);
      fail("generalDeadlockDetectorNotPromised");
    }
    case "admitStart": {
      if (state.shutdown !== "ready") fail("admissionClosed");
      state.admittedStarts += 1;
      return;
    }
    case "newGeneration": {
      const service = operation.service ?? "default";
      const current = state.generations[service] ?? 1;
      state.generations[service] = current + 1;
      return;
    }
    case "shutdownCloseAdmission": {
      requirePhase(state.shutdown, "ready", "shutdownAlreadyStarted");
      state.shutdown = "admissionClosed";
      return;
    }
    case "shutdownRequestCancellation": {
      requirePhase(state.shutdown, "admissionClosed", "shutdownCancellationOutOfOrder");
      state.shutdown = "cancellationRequested";
      for (const task of Object.values(state.tasks)) {
        if (task.closure !== "committed") task.cancellationRequested = true;
      }
      return;
    }
    case "shutdownBeginDrain": {
      requirePhase(state.shutdown, "cancellationRequested", "shutdownDrainOutOfOrder");
      state.shutdown = "draining";
      return;
    }
    case "shutdownQuiescent": {
      requirePhase(state.shutdown, "draining", "shutdownQuiescenceOutOfOrder");
      if (Object.values(state.tasks).some((task) => task.closure !== "committed" && !task.faultBoundary)) {
        fail("shutdownRootsNotQuiescent");
      }
      state.shutdown = "quiescent";
      return;
    }
    case "shutdownStop": {
      requirePhase(state.shutdown, "quiescent", "shutdownStopOutOfOrder");
      state.shutdown = "stopped";
      return;
    }
    case "shutdownExpireGrace": {
      requirePhase(state.shutdown, "draining", "shutdownGraceOutOfOrder");
      state.shutdown = "terminating";
      state.trace.push({ event: "graceExpired", roots: Object.keys(state.tasks) });
      return;
    }
    case "shutdownTerminate": {
      requirePhase(state.shutdown, "terminating", "shutdownTerminateOutOfOrder");
      state.shutdown = "terminated";
      for (const task of Object.values(state.tasks)) {
        if (task.closure !== "committed") task.faultBoundary = "forcedTermination";
      }
      state.hostCleanupRegistryReleased = true;
      return;
    }
    case "verifyTask": {
      const task = requireTask(state, operation.task);
      for (const field of ["body", "closure", "observation", "storage", "frame", "outcome", "faultBoundary"]) {
        if (operation[field] !== undefined && task[field] !== operation[field]) fail("unexpectedTaskState");
      }
      if (operation.cancellationRequested !== undefined && task.cancellationRequested !== operation.cancellationRequested) {
        fail("unexpectedCancellationState");
      }
      if (operation.cancellationDisposition !== undefined && task.cancellationDisposition !== operation.cancellationDisposition) {
        fail("unexpectedCancellationDisposition");
      }
      if (operation.children !== undefined && JSON.stringify(task.children) !== JSON.stringify(operation.children)) {
        fail("unexpectedChildRefs");
      }
      return;
    }
    case "verifyWait": {
      const wait = requireWait(state, operation.operationId, operation.generation);
      for (const field of [
        "phase",
        "outcome",
        "providerOutcome",
        "disposition",
        "completionDisposition",
        "cancelDisposition",
        "callback",
      ]) {
        if (operation[field] !== undefined && wait[field] !== operation[field]) fail("unexpectedWaitState");
      }
      if (operation.cancelRequested !== undefined && wait.cancelRequested !== operation.cancelRequested) {
        fail("unexpectedWaitCancellation");
      }
      if (operation.callbackSuppressed !== undefined && wait.callbackSuppressed !== operation.callbackSuppressed) {
        fail("unexpectedWaitCallbackState");
      }
      return;
    }
    case "verifyCleanupNode": {
      const task = requireTask(state, operation.task);
      const node = task.cleanupNode ?? task.cleanupHistory.at(-1);
      if (!node) fail("cleanupNodeNotObserved");
      for (const field of ["label", "kind", "mask", "cancellationReceived"]) {
        if (operation[field] !== undefined && node[field] !== operation[field]) {
          fail("unexpectedCleanupNodeState");
        }
      }
      if (operation.active !== undefined && (task.cleanupNode !== null) !== operation.active) {
        fail("unexpectedCleanupNodeActivity");
      }
      return;
    }
    case "verifyShutdown": {
      if (state.shutdown !== operation.phase) fail("unexpectedShutdownState");
      if (
        operation.hostCleanupRegistryReleased !== undefined &&
        state.hostCleanupRegistryReleased !== operation.hostCleanupRegistryReleased
      ) {
        fail("unexpectedHostCleanupRegistryState");
      }
      if (operation.traceEvent !== undefined) {
        const event = state.trace.find((entry) => entry.event === operation.traceEvent);
        if (!event) fail("missingShutdownTraceEvent");
        if (
          Array.isArray(operation.traceRoots) &&
          operation.traceRoots.some((root) => !event.roots.includes(root))
        ) {
          fail("missingShutdownTraceRoot");
        }
      }
      return;
    }
    default:
      fail("unknownRuntimeLivenessOperation");
  }
}

export function validateRuntimeLivenessOperation(operation) {
  if (!operation || typeof operation !== "object" || typeof operation.op !== "string") return false;
  const string = (field) => typeof operation[field] === "string" && operation[field].length > 0;
  switch (operation.op) {
    case "createTask":
      return string("task") && (operation.parent === undefined || string("parent"));
    case "publishTask":
    case "startBody":
    case "settleBody":
    case "beginClosure":
    case "requestCancellation":
    case "drainChildren":
    case "beginCleanup":
    case "cleanupAllocate":
    case "cleanupError":
    case "cleanupPanic":
    case "cleanupTimeout":
    case "typedDrop":
    case "drainRuntime":
    case "commitOutcome":
    case "moveOutcome":
    case "retireFrame":
    case "retainRuntimeRef":
    case "releaseRuntimeRef":
    case "reclaimFrame":
    case "joinObservation":
    case "retainObservation":
    case "expireRetention":
    case "retireStorage":
    case "reclaimStorage":
    case "enterBlocking":
    case "returnBlocking":
    case "killBlocking":
    case "claimDeadlockFreedom":
      return string("task");
    case "initialize":
      return string("task") && string("label");
    case "createChild":
      return string("task") && string("child");
    case "registerCleanup":
      return string("task") && string("label") && CLEANUP_KINDS.has(operation.kind);
    case "runCleanup":
      return string("task") && string("label");
    case "startCleanupNode":
    case "finishCleanupNode":
      return string("task") && string("label");
    case "finishCleanup":
      return string("task");
    case "registerWait":
    case "submitWait":
    case "startWait":
    case "cancelWait":
    case "drainWait":
      return string("task") && string("operationId") && Number.isInteger(operation.generation) && operation.generation > 0;
    case "completeWait":
      return (
        string("task") &&
        string("operationId") &&
        Number.isInteger(operation.generation) &&
        operation.generation > 0 &&
        COMPLETION_OUTCOMES.has(operation.outcome)
      );
    case "registerResource":
    case "drainResource":
      return string("task") && string("resource") && RESOURCE_KINDS.has(operation.kind);
    case "admitStart":
    case "shutdownCloseAdmission":
    case "shutdownRequestCancellation":
    case "shutdownBeginDrain":
    case "shutdownQuiescent":
    case "shutdownStop":
    case "shutdownExpireGrace":
    case "shutdownTerminate":
      return true;
    case "newGeneration":
      return operation.service === undefined || string("service");
    case "verifyTask":
      return string("task");
    case "verifyWait":
      return string("operationId") && Number.isInteger(operation.generation) && operation.generation > 0;
    case "verifyCleanupNode":
      return string("task");
    case "verifyShutdown":
      return SHUTDOWN_PHASES.has(operation.phase);
    default:
      return false;
  }
}

export function runRuntimeLivenessProgram(operations) {
  const state = {
    tasks: {},
    waits: {},
    resources: {},
    generations: { default: 1 },
    shutdown: "ready",
    admittedStarts: 0,
    hostCleanupRegistryReleased: false,
    trace: [],
  };
  const trace = [];
  for (const [index, operation] of operations.entries()) {
    const before = clone(state);
    try {
      applyOperation(state, operation);
      trace.push({ index, operation: clone(operation), before, after: clone(state) });
    } catch (error) {
      if (!(error instanceof RuntimeLivenessError)) throw error;
      trace.push({ index, operation: clone(operation), before, rejected: error.code });
      return { status: "rejected", code: error.code, operation: index, state, trace };
    }
  }
  return { status: "accepted", state, trace };
}
