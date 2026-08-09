import { expect, test } from "bun:test";
import { runRuntimeLivenessProgram } from "./runtime-liveness-machine.mjs";

function taskPrefix() {
  return [
    { op: "createTask", task: "root" },
    { op: "publishTask", task: "root" },
    { op: "startBody", task: "root" },
    { op: "initialize", task: "root", label: "resource" },
    { op: "settleBody", task: "root", outcome: "success" },
    { op: "beginClosure", task: "root" },
    { op: "drainChildren", task: "root" },
    { op: "beginCleanup", task: "root" },
    { op: "finishCleanup", task: "root" },
    { op: "typedDrop", task: "root", label: "resource" },
    { op: "drainRuntime", task: "root" },
    { op: "commitOutcome", task: "root" },
  ];
}

test("closure order is enforced independently of the snapshot", () => {
  const result = runRuntimeLivenessProgram([
    ...taskPrefix(),
    { op: "moveOutcome", task: "root" },
    { op: "retireFrame", task: "root" },
    { op: "reclaimFrame", task: "root" },
    { op: "verifyTask", task: "root", frame: "reclaimed", observation: "committed" },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.tasks.root.frame).toBe("reclaimed");
  expect(result.state.tasks.root.observation).toBe("committed");
});

test("provider completion wins a cancel request and stale generation is suppressed", () => {
  const result = runRuntimeLivenessProgram([
    { op: "createTask", task: "root", service: "orders" },
    { op: "publishTask", task: "root" },
    { op: "startBody", task: "root" },
    { op: "registerWait", task: "root", operationId: "read", generation: 1, service: "orders" },
    { op: "submitWait", task: "root", operationId: "read", generation: 1 },
    { op: "startWait", task: "root", operationId: "read", generation: 1 },
    { op: "cancelWait", task: "root", operationId: "read", generation: 1 },
    { op: "completeWait", task: "root", operationId: "read", generation: 1, outcome: "success" },
    { op: "drainWait", task: "root", operationId: "read", generation: 1 },
    { op: "newGeneration", service: "orders" },
    { op: "registerWait", task: "root", operationId: "stale", generation: 1, service: "orders" },
    { op: "submitWait", task: "root", operationId: "stale", generation: 1 },
    { op: "startWait", task: "root", operationId: "stale", generation: 1 },
    { op: "completeWait", task: "root", operationId: "stale", generation: 1, outcome: "success" },
    { op: "drainWait", task: "root", operationId: "stale", generation: 1 },
    { op: "registerWait", task: "root", operationId: "read", generation: 2, service: "orders" },
    { op: "submitWait", task: "root", operationId: "read", generation: 2 },
    { op: "startWait", task: "root", operationId: "read", generation: 2 },
    { op: "completeWait", task: "root", operationId: "read", generation: 2, outcome: "success" },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.waits["read@1"].disposition).toBe("selectedSuccess");
  expect(result.state.waits["stale@1"].completionDisposition).toBe("staleGeneration");
  expect(result.state.waits["stale@1"].callbackSuppressed).toBe(true);
  expect(result.state.waits["read@2"].disposition).toBe("selectedSuccess");
});

test("frame bytes can reclaim while outcome observation remains committed", () => {
  const result = runRuntimeLivenessProgram([
    ...taskPrefix(),
    { op: "moveOutcome", task: "root" },
    { op: "retireFrame", task: "root" },
    { op: "reclaimFrame", task: "root" },
    { op: "verifyTask", task: "root", frame: "reclaimed", observation: "committed" },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.tasks.root.outcomeMoved).toBe(true);
  expect(result.state.tasks.root.outcomeRefs).toBe(1);
});

test("runtime refs prevent premature frame reclaim", () => {
  const result = runRuntimeLivenessProgram([
    ...taskPrefix(),
    { op: "moveOutcome", task: "root" },
    { op: "retireFrame", task: "root" },
    { op: "retainRuntimeRef", task: "root" },
    { op: "reclaimFrame", task: "root" },
  ]);

  expect(result).toMatchObject({ status: "rejected", code: "frameRuntimeRefsLive", operation: 15 });
});

test("graceful shutdown and forced boundary are distinct", () => {
  const graceful = runRuntimeLivenessProgram([
    { op: "shutdownCloseAdmission" },
    { op: "shutdownRequestCancellation" },
    { op: "shutdownBeginDrain" },
    { op: "shutdownQuiescent" },
    { op: "shutdownStop" },
  ]);
  const forced = runRuntimeLivenessProgram([
    { op: "createTask", task: "root" },
    { op: "publishTask", task: "root" },
    { op: "startBody", task: "root" },
    { op: "shutdownCloseAdmission" },
    { op: "shutdownRequestCancellation" },
    { op: "shutdownBeginDrain" },
    { op: "shutdownExpireGrace" },
    { op: "shutdownTerminate" },
  ]);

  expect(graceful.state.shutdown).toBe("stopped");
  expect(forced.state.shutdown).toBe("terminated");
  expect(forced.state.tasks.root.faultBoundary).toBe("forcedTermination");
});

test("unbounded foreign blocking is rejected outside a killable boundary", () => {
  const result = runRuntimeLivenessProgram([
    { op: "createTask", task: "root" },
    { op: "publishTask", task: "root" },
    { op: "startBody", task: "root" },
    { op: "enterBlocking", task: "root", killable: false, graceFinite: false },
  ]);

  expect(result).toMatchObject({ status: "rejected", code: "unboundedBlockingAdapter", operation: 3 });
});

test("cleanup-created wait must complete before typed drop", () => {
  const result = runRuntimeLivenessProgram([
    { op: "createTask", task: "root" },
    { op: "publishTask", task: "root" },
    { op: "startBody", task: "root" },
    { op: "registerCleanup", task: "root", kind: "deferAsync", label: "close" },
    { op: "settleBody", task: "root", outcome: "success" },
    { op: "beginClosure", task: "root" },
    { op: "drainChildren", task: "root" },
    { op: "beginCleanup", task: "root" },
    { op: "startCleanupNode", task: "root", label: "close" },
    { op: "registerWait", task: "root", operationId: "close", generation: 1, owner: "cleanup" },
    { op: "finishCleanupNode", task: "root", label: "close" },
  ]);

  expect(result).toMatchObject({ status: "rejected", code: "cleanupWaitNotDrained", operation: 10 });
});
