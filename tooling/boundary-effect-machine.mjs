const CALL_POLICIES = new Set(["atMostOnce", "repeatable", "idempotent", "transactional"]);
const CALL_BODY_OUTCOMES = new Set(["success", "applicationError", "canceled"]);
const CALL_TERMINALS = new Set(["committed", "commitFailed", "unknownOutcome", "canceled"]);
const TRANSACTION_TERMINALS = new Set(["committed", "aborted", "unknownCommit"]);
const PIPELINE_OUTCOMES = new Set([
  "success",
  "applicationError",
  "boundaryError",
  "unknownOutcome",
  "canceled",
]);

export class BoundaryEffectError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

function clone(value) {
  return structuredClone(value);
}

function requireCall(state, name) {
  const call = state.calls[name];
  if (!call) throw new BoundaryEffectError("unknownCall");
  return call;
}

function requireTransaction(state, name) {
  const transaction = state.transactions[name];
  if (!transaction) throw new BoundaryEffectError("unknownTransaction");
  return transaction;
}

function requirePipeline(state, name) {
  const pipeline = state.pipelines[name];
  if (!pipeline) throw new BoundaryEffectError("unknownPipeline");
  return pipeline;
}

function requireState(value, expected, code) {
  if (value.state !== expected) throw new BoundaryEffectError(code);
}

function discardCallOutput(call) {
  if (call.output === "staged") call.output = "discarded";
}

function cleanupCallEnvelope(call) {
  if (call.envelopeCleanupCount > 0) {
    throw new BoundaryEffectError("callEnvelopeCleanedTwice");
  }
  call.envelopeCleanupCount += 1;
  call.inputOwner = "dropped";
}

function finishCallTerminal(call, stateName, terminal) {
  call.state = stateName;
  call.terminal = terminal;
  if (terminal !== "committed") discardCallOutput(call);
}

function blockPipelineDependents(pipeline, failedNode) {
  const blocked = new Set([failedNode]);
  let changed = true;
  while (changed) {
    changed = false;
    for (const nodeName of pipeline.order) {
      const node = pipeline.nodes[nodeName];
      if (
        node.state === "pending" &&
        node.dependencies.some((dependency) => blocked.has(dependency))
      ) {
        node.state = "blocked";
        blocked.add(nodeName);
        changed = true;
      }
    }
  }
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "registerInstance": {
      if (state.instances[operation.instance]) {
        throw new BoundaryEffectError("instanceAlreadyRegistered");
      }
      state.instances[operation.instance] = { activeCall: null };
      return;
    }
    case "prepareCall": {
      if (state.calls[operation.call]) throw new BoundaryEffectError("callAlreadyPrepared");
      if (!state.instances[operation.instance]) throw new BoundaryEffectError("unknownInstance");
      state.calls[operation.call] = {
        state: "prepared",
        instance: operation.instance,
        effectId: operation.effectId,
        policy: operation.policy,
        inputOwner: "callerStaging",
        envelopeCleanupCount: 0,
        bodyStarted: false,
        bodyOutcome: null,
        output: "none",
        terminal: null,
        cancellationRequested: false,
        lateCancellation: false,
      };
      (state.effectAttempts[operation.effectId] ??= []).push(operation.call);
      return;
    }
    case "commitEnvelope": {
      const call = requireCall(state, operation.call);
      requireState(call, "prepared", "invalidEnvelopeCommit");
      call.state = "envelopeCommitted";
      call.inputOwner = "envelope";
      return;
    }
    case "admitCall": {
      const call = requireCall(state, operation.call);
      requireState(call, "envelopeCommitted", "invalidCallAdmission");
      call.state = "admitted";
      call.inputOwner = "callee";
      return;
    }
    case "beginTurn": {
      const call = requireCall(state, operation.call);
      requireState(call, "admitted", "invalidTurnStart");
      const instance = state.instances[call.instance];
      if (instance.activeCall) throw new BoundaryEffectError("closedTurnBusy");
      instance.activeCall = operation.call;
      call.state = "turnRunning";
      call.bodyStarted = true;
      return;
    }
    case "settleCall": {
      const call = requireCall(state, operation.call);
      requireState(call, "turnRunning", "invalidCallSettlement");
      if (operation.outcome === "canceled" && !call.cancellationRequested) {
        throw new BoundaryEffectError("callCanceledWithoutSignal");
      }
      call.state = "bodySettled";
      call.bodyOutcome = operation.outcome;
      if (operation.outcome === "success") call.output = "staged";
      return;
    }
    case "beginCallCommit": {
      const call = requireCall(state, operation.call);
      requireState(call, "bodySettled", "callCommitBeforeBodySettlement");
      call.state = "committing";
      return;
    }
    case "confirmCallCommit": {
      const call = requireCall(state, operation.call);
      requireState(call, "committing", "callCommitNotPending");
      finishCallTerminal(call, "committed", "committed");
      return;
    }
    case "failCallCommit": {
      const call = requireCall(state, operation.call);
      requireState(call, "committing", "callCommitNotPending");
      finishCallTerminal(call, "commitFailed", "commitFailed");
      return;
    }
    case "loseCallConfirmation": {
      const call = requireCall(state, operation.call);
      requireState(call, "committing", "callCommitNotPending");
      finishCallTerminal(call, "unknownOutcome", "unknownOutcome");
      return;
    }
    case "loseBoundaryEvidence": {
      const call = requireCall(state, operation.call);
      if (
        ![
          "envelopeCommitted",
          "admitted",
          "turnRunning",
          "bodySettled",
          "committing",
        ].includes(call.state)
      ) {
        throw new BoundaryEffectError("boundaryEvidenceLossNotAmbiguous");
      }
      finishCallTerminal(call, "unknownOutcome", "unknownOutcome");
      return;
    }
    case "cancelCall": {
      const call = requireCall(state, operation.call);
      if (["prepared", "envelopeCommitted"].includes(call.state)) {
        call.cancellationRequested = true;
        finishCallTerminal(call, "canceled", "canceled");
        cleanupCallEnvelope(call);
        return;
      }
      if (["admitted", "turnRunning", "bodySettled", "committing"].includes(call.state)) {
        call.cancellationRequested = true;
        return;
      }
      if (CALL_TERMINALS.has(call.terminal) || ["drained", "delivered"].includes(call.state)) {
        call.lateCancellation = true;
        return;
      }
      throw new BoundaryEffectError("invalidCallCancellation");
    }
    case "drainCall": {
      const call = requireCall(state, operation.call);
      if (!CALL_TERMINALS.has(call.terminal)) {
        throw new BoundaryEffectError("callDrainBeforeTerminal");
      }
      if (call.state === "drained" || call.state === "delivered") {
        throw new BoundaryEffectError("callAlreadyDrained");
      }
      const instance = state.instances[call.instance];
      if (instance.activeCall === operation.call) instance.activeCall = null;
      if (call.envelopeCleanupCount === 0) cleanupCallEnvelope(call);
      call.state = "drained";
      return;
    }
    case "deliverCall": {
      const call = requireCall(state, operation.call);
      requireState(call, "drained", "callDeliveryBeforeDrain");
      if (call.terminal === "committed" && call.bodyOutcome === "success") {
        if (call.output !== "staged") throw new BoundaryEffectError("missingStagedCallOutput");
        call.output = "delivered";
      } else {
        discardCallOutput(call);
      }
      call.state = "delivered";
      return;
    }
    case "prepareCallRetry": {
      const source = requireCall(state, operation.from);
      requireState(source, "delivered", "retryBeforeCallOutcome");
      if (source.terminal !== "unknownOutcome") {
        throw new BoundaryEffectError("retryRequiresUnknownOutcome");
      }
      if (source.policy === "atMostOnce") {
        throw new BoundaryEffectError("retryUnknownForbidden");
      }
      if (source.policy === "transactional") {
        throw new BoundaryEffectError("retryRequiresReconciliation");
      }
      if (state.calls[operation.call]) throw new BoundaryEffectError("callAlreadyPrepared");
      state.calls[operation.call] = {
        ...clone(source),
        state: "prepared",
        inputOwner: "callerStaging",
        envelopeCleanupCount: 0,
        bodyStarted: false,
        bodyOutcome: null,
        output: "none",
        terminal: null,
        cancellationRequested: false,
        lateCancellation: false,
      };
      state.effectAttempts[source.effectId].push(operation.call);
      return;
    }
    case "verifyCall": {
      const call = requireCall(state, operation.call);
      for (const field of [
        "state",
        "effectId",
        "inputOwner",
        "bodyOutcome",
        "output",
        "terminal",
        "envelopeCleanupCount",
        "bodyStarted",
        "cancellationRequested",
        "lateCancellation",
      ]) {
        if (operation[field] !== undefined && call[field] !== operation[field]) {
          throw new BoundaryEffectError("unexpectedCallState");
        }
      }
      return;
    }
    case "createTransaction": {
      if (state.transactions[operation.transaction]) {
        throw new BoundaryEffectError("transactionAlreadyCreated");
      }
      state.transactions[operation.transaction] = {
        state: "staging",
        effectId: operation.effectId,
        provider: operation.provider,
        bodyRuns: 0,
        output: "none",
        terminal: null,
        cancellationRequested: false,
      };
      return;
    }
    case "beginTransaction": {
      const transaction = requireTransaction(state, operation.transaction);
      requireState(transaction, "staging", "invalidTransactionBegin");
      transaction.state = "begun";
      return;
    }
    case "failTransactionBegin": {
      const transaction = requireTransaction(state, operation.transaction);
      requireState(transaction, "staging", "transactionBeginNotPending");
      transaction.state = "aborted";
      transaction.terminal = "aborted";
      transaction.output = "discarded";
      return;
    }
    case "enterTransactionBody": {
      const transaction = requireTransaction(state, operation.transaction);
      requireState(transaction, "begun", "invalidTransactionBodyEntry");
      transaction.state = "body";
      transaction.bodyRuns += 1;
      return;
    }
    case "requestTransactionCommit": {
      const transaction = requireTransaction(state, operation.transaction);
      requireState(transaction, "body", "transactionCommitOutsideBody");
      transaction.state = "commitRequested";
      transaction.output = "staged";
      return;
    }
    case "failTransactionBody": {
      const transaction = requireTransaction(state, operation.transaction);
      if (!["begun", "body"].includes(transaction.state)) {
        throw new BoundaryEffectError("transactionAbortAfterCommitRequest");
      }
      transaction.state = "abortRequested";
      transaction.output = "discarded";
      return;
    }
    case "cancelTransaction": {
      const transaction = requireTransaction(state, operation.transaction);
      transaction.cancellationRequested = true;
      if (transaction.state === "staging") {
        transaction.state = "aborted";
        transaction.terminal = "aborted";
        transaction.output = "discarded";
      } else if (["begun", "body"].includes(transaction.state)) {
        transaction.state = "abortRequested";
        transaction.output = "discarded";
      } else if (transaction.state !== "commitRequested") {
        throw new BoundaryEffectError("invalidTransactionCancellation");
      }
      return;
    }
    case "confirmTransactionAbort": {
      const transaction = requireTransaction(state, operation.transaction);
      requireState(transaction, "abortRequested", "transactionAbortNotPending");
      transaction.state = "aborted";
      transaction.terminal = "aborted";
      return;
    }
    case "confirmTransactionCommit": {
      const transaction = requireTransaction(state, operation.transaction);
      requireState(transaction, "commitRequested", "transactionCommitNotPending");
      transaction.state = "committed";
      transaction.terminal = "committed";
      return;
    }
    case "loseTransactionConfirmation": {
      const transaction = requireTransaction(state, operation.transaction);
      requireState(transaction, "commitRequested", "transactionCommitNotPending");
      transaction.state = "unknownCommit";
      transaction.terminal = "unknownCommit";
      transaction.output = "discarded";
      return;
    }
    case "finishTransaction": {
      const transaction = requireTransaction(state, operation.transaction);
      if (!TRANSACTION_TERMINALS.has(transaction.terminal)) {
        throw new BoundaryEffectError("transactionFinishBeforeTerminal");
      }
      if (transaction.terminal === "committed") {
        if (transaction.output !== "staged") {
          throw new BoundaryEffectError("missingStagedTransactionOutput");
        }
        transaction.output = "delivered";
      } else {
        transaction.output = "discarded";
      }
      transaction.state = "finished";
      return;
    }
    case "rerunTransactionBody": {
      const transaction = requireTransaction(state, operation.transaction);
      if (transaction.terminal === "unknownCommit") {
        throw new BoundaryEffectError("automaticTransactionRetryForbidden");
      }
      throw new BoundaryEffectError("transactionBodyRunsOnce");
    }
    case "verifyTransaction": {
      const transaction = requireTransaction(state, operation.transaction);
      for (const field of [
        "state",
        "effectId",
        "provider",
        "bodyRuns",
        "output",
        "terminal",
        "cancellationRequested",
      ]) {
        if (operation[field] !== undefined && transaction[field] !== operation[field]) {
          throw new BoundaryEffectError("unexpectedTransactionState");
        }
      }
      return;
    }
    case "createPipeline": {
      if (state.pipelines[operation.pipeline]) {
        throw new BoundaryEffectError("pipelineAlreadyCreated");
      }
      const order = operation.nodes.map((node) => node.node);
      if (new Set(order).size !== order.length) {
        throw new BoundaryEffectError("duplicatePipelineNode");
      }
      const effectIds = operation.nodes.map((node) => node.effectId);
      if (new Set(effectIds).size !== effectIds.length) {
        throw new BoundaryEffectError("duplicatePipelineEffect");
      }
      if (operation.selected.some((node) => !order.includes(node))) {
        throw new BoundaryEffectError("unknownPipelineSelection");
      }
      for (const [index, node] of operation.nodes.entries()) {
        if (node.dependencies.some((dependency) => !order.includes(dependency))) {
          throw new BoundaryEffectError("unknownPipelineDependency");
        }
        if (node.dependencies.some((dependency) => order.indexOf(dependency) >= index)) {
          throw new BoundaryEffectError("pipelineForwardReference");
        }
      }
      state.pipelines[operation.pipeline] = {
        state: "active",
        order,
        selected: operation.selected,
        aggregate: null,
        uncertainEffects: [],
        nodes: Object.fromEntries(
          operation.nodes.map((node) => [
            node.node,
            {
              effectId: node.effectId,
              dependencies: node.dependencies,
              state: "pending",
              outcome: null,
              cancellationRequested: false,
              capability: "absent",
            },
          ]),
        ),
      };
      return;
    }
    case "startPipelineNode": {
      const pipeline = requirePipeline(state, operation.pipeline);
      const node = pipeline.nodes[operation.node];
      if (!node) throw new BoundaryEffectError("unknownPipelineNode");
      if (node.state === "blocked") throw new BoundaryEffectError("dependentNodeBlocked");
      requireState(node, "pending", "invalidPipelineNodeStart");
      if (
        node.dependencies.some((dependency) => {
          const upstream = pipeline.nodes[dependency];
          return !upstream || upstream.outcome !== "success";
        })
      ) {
        throw new BoundaryEffectError("pipelineDependencyNotReady");
      }
      node.state = "running";
      return;
    }
    case "settlePipelineNode": {
      const pipeline = requirePipeline(state, operation.pipeline);
      const node = pipeline.nodes[operation.node];
      if (!node) throw new BoundaryEffectError("unknownPipelineNode");
      requireState(node, "running", "invalidPipelineNodeSettlement");
      if (operation.outcome === "canceled" && !node.cancellationRequested) {
        throw new BoundaryEffectError("pipelineNodeCanceledWithoutSignal");
      }
      node.state = "settled";
      node.outcome = operation.outcome;
      if (operation.capabilityCreated) node.capability = "pending";
      if (operation.outcome !== "success") blockPipelineDependents(pipeline, operation.node);
      return;
    }
    case "requestPipelineFailFast": {
      const pipeline = requirePipeline(state, operation.pipeline);
      const trigger = pipeline.nodes[operation.trigger];
      if (!trigger || trigger.state !== "settled" || trigger.outcome === "success") {
        throw new BoundaryEffectError("pipelineFailFastWithoutFailure");
      }
      for (const nodeName of pipeline.order) {
        if (nodeName === operation.trigger) continue;
        const node = pipeline.nodes[nodeName];
        if (node.state === "running") node.cancellationRequested = true;
        if (node.state === "pending") {
          node.state = "settled";
          node.outcome = "canceled";
          node.cancellationRequested = true;
        }
      }
      return;
    }
    case "drainPipelineNode": {
      const pipeline = requirePipeline(state, operation.pipeline);
      const node = pipeline.nodes[operation.node];
      if (!node) throw new BoundaryEffectError("unknownPipelineNode");
      requireState(node, "settled", "pipelineNodeDrainBeforeSettlement");
      node.state = "drained";
      return;
    }
    case "resolvePipeline": {
      const pipeline = requirePipeline(state, operation.pipeline);
      if (
        pipeline.order.some(
          (nodeName) => !["drained", "blocked"].includes(pipeline.nodes[nodeName].state),
        )
      ) {
        throw new BoundaryEffectError("pipelineNotDrained");
      }
      pipeline.uncertainEffects = pipeline.order
        .filter((nodeName) => pipeline.nodes[nodeName].outcome === "unknownOutcome")
        .map((nodeName) => pipeline.nodes[nodeName].effectId);
      if (pipeline.uncertainEffects.length > 0) {
        pipeline.aggregate = "pipelineUnknown";
      } else {
        const firstError = pipeline.order
          .map((nodeName) => pipeline.nodes[nodeName].outcome)
          .find((outcome) =>
            ["applicationError", "boundaryError"].includes(outcome),
          );
        const canceled = pipeline.order.some(
          (nodeName) => pipeline.nodes[nodeName].outcome === "canceled",
        );
        pipeline.aggregate = firstError ?? (canceled ? "canceled" : "success");
      }
      for (const nodeName of pipeline.order) {
        const node = pipeline.nodes[nodeName];
        if (node.capability !== "pending") continue;
        node.capability =
          pipeline.aggregate === "success" && pipeline.selected.includes(nodeName)
            ? "transferred"
            : "released";
      }
      pipeline.state = "resolved";
      return;
    }
    case "verifyPipeline": {
      const pipeline = requirePipeline(state, operation.pipeline);
      if (operation.state !== undefined && pipeline.state !== operation.state) {
        throw new BoundaryEffectError("unexpectedPipelineState");
      }
      if (operation.aggregate !== undefined && pipeline.aggregate !== operation.aggregate) {
        throw new BoundaryEffectError("unexpectedPipelineAggregate");
      }
      if (
        operation.uncertainEffects !== undefined &&
        JSON.stringify(pipeline.uncertainEffects) !== JSON.stringify(operation.uncertainEffects)
      ) {
        throw new BoundaryEffectError("unexpectedPipelineUncertainty");
      }
      if (operation.node) {
        const node = pipeline.nodes[operation.node];
        if (!node) throw new BoundaryEffectError("unknownPipelineNode");
        for (const field of ["state", "outcome", "cancellationRequested", "capability"]) {
          if (operation[field] !== undefined && node[field] !== operation[field]) {
            throw new BoundaryEffectError("unexpectedPipelineNodeState");
          }
        }
      }
      return;
    }
    default:
      throw new BoundaryEffectError("unknownBoundaryEffectOperation");
  }
}

export function validateBoundaryEffectOperation(operation) {
  if (!operation || typeof operation !== "object" || typeof operation.op !== "string") {
    return false;
  }
  const string = (field) => typeof operation[field] === "string" && operation[field].length > 0;
  switch (operation.op) {
    case "registerInstance":
      return string("instance");
    case "prepareCall":
      return (
        string("call") &&
        string("instance") &&
        string("effectId") &&
        CALL_POLICIES.has(operation.policy)
      );
    case "commitEnvelope":
    case "admitCall":
    case "beginTurn":
    case "beginCallCommit":
    case "confirmCallCommit":
    case "failCallCommit":
    case "loseCallConfirmation":
    case "loseBoundaryEvidence":
    case "cancelCall":
    case "drainCall":
    case "deliverCall":
      return string("call");
    case "settleCall":
      return string("call") && CALL_BODY_OUTCOMES.has(operation.outcome);
    case "prepareCallRetry":
      return string("from") && string("call");
    case "verifyCall":
      return string("call");
    case "createTransaction":
      return string("transaction") && string("effectId") && string("provider");
    case "beginTransaction":
    case "failTransactionBegin":
    case "enterTransactionBody":
    case "requestTransactionCommit":
    case "failTransactionBody":
    case "cancelTransaction":
    case "confirmTransactionAbort":
    case "confirmTransactionCommit":
    case "loseTransactionConfirmation":
    case "finishTransaction":
    case "rerunTransactionBody":
    case "verifyTransaction":
      return string("transaction");
    case "createPipeline":
      return (
        string("pipeline") &&
        Array.isArray(operation.nodes) &&
        operation.nodes.length > 0 &&
        operation.nodes.every(
          (node) =>
            typeof node?.node === "string" &&
            node.node.length > 0 &&
            typeof node.effectId === "string" &&
            node.effectId.length > 0 &&
            Array.isArray(node.dependencies) &&
            node.dependencies.every((dependency) => typeof dependency === "string"),
        ) &&
        Array.isArray(operation.selected) &&
        operation.selected.every((node) => typeof node === "string")
      );
    case "startPipelineNode":
    case "drainPipelineNode":
      return string("pipeline") && string("node");
    case "settlePipelineNode":
      return (
        string("pipeline") &&
        string("node") &&
        PIPELINE_OUTCOMES.has(operation.outcome) &&
        (operation.capabilityCreated === undefined ||
          typeof operation.capabilityCreated === "boolean")
      );
    case "requestPipelineFailFast":
      return string("pipeline") && string("trigger");
    case "resolvePipeline":
      return string("pipeline");
    case "verifyPipeline":
      return string("pipeline");
    default:
      return false;
  }
}

export function runBoundaryEffectProgram(operations) {
  const state = {
    instances: {},
    calls: {},
    effectAttempts: {},
    transactions: {},
    pipelines: {},
  };
  const trace = [];
  for (const [index, operation] of operations.entries()) {
    try {
      applyOperation(state, operation);
      trace.push({ index, op: operation.op, accepted: true });
    } catch (error) {
      if (!(error instanceof BoundaryEffectError)) throw error;
      trace.push({ index, op: operation.op, rejected: error.code });
      return { status: "rejected", code: error.code, operation: index, state, trace };
    }
  }
  return { status: "accepted", state, trace };
}
