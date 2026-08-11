import { expect, test } from "bun:test";
import { runExecutionProgram } from "./execution-concurrency-machine.mjs";

function rootOperations() {
  return [
    { op: "reserveTask", task: "root" },
    { op: "publishTask", task: "root", id: "root-start" },
  ];
}

test("a relaxed RMW continues a release sequence", () => {
  const result = runExecutionProgram([
    ...rootOperations(),
    { op: "atomicStore", task: "root", id: "release", storage: "epoch", order: "release" },
    {
      op: "atomicRmw",
      task: "root",
      id: "relay",
      storage: "epoch",
      order: "relaxed",
      observes: "release",
    },
    {
      op: "atomicLoad",
      task: "root",
      id: "acquire",
      storage: "epoch",
      order: "acquire",
      observes: "relay",
    },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.atomicModificationOrder["epoch@0:all"]).toEqual([
    "release",
    "relay",
  ]);
  expect(result.state.edges).toContainEqual({
    from: "release",
    to: "acquire",
    kind: "atomicReleaseAcquire",
  });
});

test("a relaxed store cuts the preceding release sequence", () => {
  const result = runExecutionProgram([
    ...rootOperations(),
    { op: "atomicStore", task: "root", id: "release", storage: "epoch", order: "release" },
    { op: "atomicStore", task: "root", id: "overwrite", storage: "epoch", order: "relaxed" },
    {
      op: "atomicLoad",
      task: "root",
      id: "acquire",
      storage: "epoch",
      order: "acquire",
      observes: "overwrite",
    },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.edges).not.toContainEqual({
    from: "release",
    to: "acquire",
    kind: "atomicReleaseAcquire",
  });
});

test("a failed compare-exchange is a load and validates its failure order", () => {
  const failed = runExecutionProgram([
    ...rootOperations(),
    { op: "atomicStore", task: "root", id: "initial", storage: "state", order: "release" },
    {
      op: "atomicCompareExchange",
      task: "root",
      id: "mismatch",
      storage: "state",
      successOrder: "acquireRelease",
      failureOrder: "acquire",
      result: "mismatch",
      observes: "initial",
    },
  ]);
  expect(failed.status).toBe("accepted");
  expect(failed.state.atomicModificationOrder["state@0:all"]).toEqual(["initial"]);

  const invalid = runExecutionProgram([
    ...rootOperations(),
    { op: "atomicStore", task: "root", id: "initial", storage: "state", order: "relaxed" },
    {
      op: "atomicCompareExchange",
      task: "root",
      id: "invalid",
      storage: "state",
      successOrder: "release",
      failureOrder: "acquire",
      result: "mismatch",
      observes: "initial",
    },
  ]);
  expect(invalid).toMatchObject({
    status: "rejected",
    code: "invalidAtomicFailureOrder",
    operation: 3,
  });
});

test("the compare-exchange matrix rejects every stronger failure order", () => {
  const allowed = {
    relaxed: ["relaxed"],
    acquire: ["relaxed", "acquire"],
    release: ["relaxed"],
    acquireRelease: ["relaxed", "acquire"],
    sequential: ["relaxed", "acquire", "sequential"],
  };
  const orders = ["relaxed", "acquire", "release", "acquireRelease", "sequential"];

  for (const successOrder of orders) {
    for (const failureOrder of orders) {
      const result = runExecutionProgram([
        ...rootOperations(),
        { op: "atomicStore", task: "root", id: "initial", storage: "state", order: "relaxed" },
        {
          op: "atomicCompareExchange",
          task: "root",
          id: "compare",
          storage: "state",
          successOrder,
          failureOrder,
          result: "mismatch",
          observes: "initial",
        },
      ]);
      expect(result.status).toBe(
        allowed[successOrder].includes(failureOrder) ? "accepted" : "rejected",
      );
    }
  }
});

test("load, store and fence accept only their static order subsets", () => {
  const orders = ["relaxed", "acquire", "release", "acquireRelease", "sequential"];
  const accepted = {
    atomicLoad: ["relaxed", "acquire", "sequential"],
    atomicStore: ["relaxed", "release", "sequential"],
    atomicFence: ["acquire", "release", "acquireRelease", "sequential"],
  };

  for (const [operation, acceptedOrders] of Object.entries(accepted)) {
    for (const order of orders) {
      const result = runExecutionProgram([
        ...rootOperations(),
        {
          op: operation,
          task: "root",
          id: `${operation}-${order}`,
          ...(operation === "atomicFence" ? {} : { storage: "state" }),
          order,
        },
      ]);
      expect(result.status).toBe(acceptedOrders.includes(order) ? "accepted" : "rejected");
    }
  }
});

test("a fence pair needs an atomic reads-from witness", () => {
  const result = runExecutionProgram([
    ...rootOperations(),
    { op: "reserveTask", task: "writer", parent: "root" },
    { op: "publishTask", task: "writer", id: "writer-start", after: "root-start" },
    { op: "reserveTask", task: "reader", parent: "root" },
    { op: "publishTask", task: "reader", id: "reader-start", after: "root-start" },
    { op: "atomicFence", task: "writer", id: "release-fence", order: "release" },
    { op: "atomicStore", task: "writer", id: "publish", storage: "ready", order: "relaxed" },
    {
      op: "atomicLoad",
      task: "reader",
      id: "observe",
      storage: "ready",
      order: "relaxed",
      observes: "publish",
    },
    {
      op: "atomicFence",
      task: "reader",
      id: "acquire-fence",
      order: "acquire",
      observes: "observe",
    },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.edges).toContainEqual({
    from: "release-fence",
    to: "acquire-fence",
    kind: "atomicReleaseAcquire",
  });
});

test("a domain barrier orders earlier and later tickets", () => {
  const result = runExecutionProgram([
    ...rootOperations(),
    { op: "reserveTask", task: "before", parent: "root" },
    { op: "reserveTask", task: "writer", parent: "root" },
    { op: "reserveTask", task: "after", parent: "root" },
    {
      op: "domainAdmit",
      task: "root",
      id: "before-admit",
      job: "before-job",
      jobTask: "before",
      domain: "catalog",
      ticket: 0,
      mode: "ordinary",
    },
    {
      op: "domainAdmit",
      task: "root",
      id: "writer-admit",
      job: "writer-job",
      jobTask: "writer",
      domain: "catalog",
      ticket: 1,
      mode: "barrier",
    },
    {
      op: "domainAdmit",
      task: "root",
      id: "after-admit",
      job: "after-job",
      jobTask: "after",
      domain: "catalog",
      ticket: 2,
      mode: "ordinary",
    },
    { op: "domainStart", task: "before", id: "before-start", job: "before-job" },
    { op: "domainComplete", task: "before", id: "before-done", job: "before-job" },
    { op: "domainStart", task: "writer", id: "writer-start", job: "writer-job" },
    { op: "domainComplete", task: "writer", id: "writer-done", job: "writer-job" },
    { op: "domainStart", task: "after", id: "after-start", job: "after-job" },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.edges).toContainEqual({
    from: "before-done",
    to: "writer-start",
    kind: "domainPriorToBarrier",
  });
  expect(result.state.edges).toContainEqual({
    from: "writer-done",
    to: "after-start",
    kind: "domainBarrierToLater",
  });
});

test("a same-domain child remains inside its parent ticket group", () => {
  const result = runExecutionProgram([
    ...rootOperations(),
    { op: "reserveTask", task: "parent", parent: "root" },
    { op: "reserveTask", task: "child", parent: "parent" },
    { op: "reserveTask", task: "writer", parent: "root" },
    {
      op: "domainAdmit",
      task: "root",
      id: "parent-admit",
      job: "parent-job",
      jobTask: "parent",
      domain: "catalog",
      ticket: 0,
      mode: "ordinary",
    },
    {
      op: "domainAdmit",
      task: "root",
      id: "writer-admit",
      job: "writer-job",
      jobTask: "writer",
      domain: "catalog",
      ticket: 1,
      mode: "barrier",
    },
    { op: "domainStart", task: "parent", id: "parent-start", job: "parent-job" },
    {
      op: "domainAttachChild",
      task: "parent",
      id: "child-attach",
      job: "child-job",
      jobTask: "child",
      domain: "catalog",
      parentJob: "parent-job",
    },
    { op: "domainStart", task: "child", id: "child-start", job: "child-job" },
    { op: "domainComplete", task: "child", id: "child-done", job: "child-job" },
    { op: "domainComplete", task: "parent", id: "parent-done", job: "parent-job" },
    { op: "domainStart", task: "writer", id: "writer-start", job: "writer-job" },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.domains.catalog.jobs["child-job"].ticket).toBe(0);
  expect(result.state.edges).toContainEqual({
    from: "child-attach",
    to: "child-start",
    kind: "parentToChild",
  });
  expect(result.state.edges).toContainEqual({
    from: "child-done",
    to: "writer-start",
    kind: "domainPriorToBarrier",
  });
});

test("a domain barrier neither suspends nor starts before prior work", () => {
  const suspending = runExecutionProgram([
    ...rootOperations(),
    { op: "reserveTask", task: "writer", parent: "root" },
    {
      op: "domainAdmit",
      task: "root",
      id: "writer-admit",
      job: "writer-job",
      jobTask: "writer",
      domain: "catalog",
      ticket: 0,
      mode: "barrier",
      maySuspend: true,
    },
  ]);
  expect(suspending).toMatchObject({
    status: "rejected",
    code: "barrierMaySuspend",
    operation: 3,
  });

  const early = runExecutionProgram([
    ...rootOperations(),
    { op: "reserveTask", task: "reader", parent: "root" },
    { op: "reserveTask", task: "writer", parent: "root" },
    {
      op: "domainAdmit",
      task: "root",
      id: "reader-admit",
      job: "reader-job",
      jobTask: "reader",
      domain: "catalog",
      ticket: 0,
      mode: "ordinary",
    },
    {
      op: "domainAdmit",
      task: "root",
      id: "writer-admit",
      job: "writer-job",
      jobTask: "writer",
      domain: "catalog",
      ticket: 1,
      mode: "barrier",
    },
    { op: "domainStart", task: "reader", id: "reader-start", job: "reader-job" },
    { op: "domainStart", task: "writer", id: "writer-start", job: "writer-job" },
  ]);
  expect(early).toMatchObject({
    status: "rejected",
    code: "domainBarrierBeforePriorCompletion",
    operation: 7,
  });
});

test("only exclusive authority opens an atomic payload", () => {
  const accepted = runExecutionProgram([
    ...rootOperations(),
    { op: "createStorage", task: "root", id: "create", storage: "counter", mode: "atomic" },
    {
      op: "beginAtomicExclusive",
      task: "root",
      id: "begin",
      storage: "counter",
      token: "token",
      authority: "inout",
    },
    {
      op: "atomicPayloadAccess",
      task: "root",
      id: "reset",
      storage: "counter",
      token: "token",
      access: "write",
    },
    { op: "endAtomicExclusive", task: "root", id: "end", storage: "counter", token: "token" },
  ]);
  expect(accepted.status).toBe("accepted");

  const rejected = runExecutionProgram([
    ...rootOperations(),
    { op: "createStorage", task: "root", id: "create", storage: "counter", mode: "atomic" },
    {
      op: "beginAtomicExclusive",
      task: "root",
      id: "begin",
      storage: "counter",
      token: "token",
      authority: "ref",
    },
  ]);
  expect(rejected).toMatchObject({
    status: "rejected",
    code: "atomicExclusiveRequiresInout",
    operation: 3,
  });
});

test("atomic wait resumes through the observed release", () => {
  const result = runExecutionProgram([
    ...rootOperations(),
    { op: "createStorage", task: "root", id: "create", storage: "epoch", mode: "atomic" },
    {
      op: "atomicStore",
      task: "root",
      id: "initial",
      storage: "epoch",
      order: "relaxed",
      value: 0,
    },
    { op: "reserveTask", task: "waiter", parent: "root" },
    { op: "publishTask", task: "waiter", id: "waiter-start", after: "root-start" },
    {
      op: "atomicWaitStart",
      task: "waiter",
      id: "wait-register",
      storage: "epoch",
      order: "acquire",
      expected: 0,
      observes: "initial",
      ticket: 0,
    },
    {
      op: "atomicStore",
      task: "root",
      id: "publish",
      storage: "epoch",
      order: "release",
      value: 1,
    },
    { op: "atomicNotify", task: "root", id: "notify", storage: "epoch", mode: "all" },
    {
      op: "atomicWaitResume",
      task: "waiter",
      id: "wait-resume",
      storage: "epoch",
      order: "acquire",
      observes: "publish",
      ticket: 0,
    },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.atomicWaitQueues["epoch@0:all"].waiters).toEqual([]);
  expect(result.state.edges).toContainEqual({
    from: "publish",
    to: "wait-resume",
    kind: "atomicReleaseAcquire",
  });
});

test("notifyOne selects the oldest eligible atomic waiter", () => {
  const result = runExecutionProgram([
    ...rootOperations(),
    { op: "createStorage", task: "root", id: "create", storage: "epoch", mode: "atomic" },
    {
      op: "atomicStore",
      task: "root",
      id: "initial",
      storage: "epoch",
      order: "relaxed",
      value: 0,
    },
    { op: "reserveTask", task: "first", parent: "root" },
    { op: "publishTask", task: "first", id: "first-start", after: "root-start" },
    { op: "reserveTask", task: "second", parent: "root" },
    { op: "publishTask", task: "second", id: "second-start", after: "root-start" },
    {
      op: "atomicWaitStart",
      task: "first",
      id: "first-register",
      storage: "epoch",
      order: "acquire",
      expected: 0,
      observes: "initial",
      ticket: 0,
    },
    {
      op: "atomicWaitStart",
      task: "second",
      id: "second-register",
      storage: "epoch",
      order: "acquire",
      expected: 0,
      observes: "initial",
      ticket: 1,
    },
    {
      op: "atomicStore",
      task: "root",
      id: "publish",
      storage: "epoch",
      order: "release",
      value: 1,
    },
    { op: "atomicNotify", task: "root", id: "notify", storage: "epoch", mode: "one" },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.events.notify.wokenTickets).toEqual([0]);
  expect(result.state.atomicWaitQueues["epoch@0:all"].waiters).toMatchObject([
    { ticket: 0, state: "notified" },
    { ticket: 1, state: "waiting" },
  ]);
});

test("cancellation drains a waiter before storage destruction", () => {
  const result = runExecutionProgram([
    ...rootOperations(),
    { op: "createStorage", task: "root", id: "create", storage: "epoch", mode: "atomic" },
    {
      op: "atomicStore",
      task: "root",
      id: "initial",
      storage: "epoch",
      order: "relaxed",
      value: 0,
    },
    { op: "reserveTask", task: "waiter", parent: "root" },
    { op: "publishTask", task: "waiter", id: "waiter-start", after: "root-start" },
    {
      op: "atomicWaitStart",
      task: "waiter",
      id: "wait-register",
      storage: "epoch",
      order: "acquire",
      expected: 0,
      observes: "initial",
      ticket: 0,
    },
    { op: "requestCancel", task: "waiter", reason: "scope closing" },
    {
      op: "atomicWaitCancel",
      task: "waiter",
      id: "wait-cancel",
      storage: "epoch",
      ticket: 0,
    },
    { op: "destroyStorage", task: "root", id: "destroy", storage: "epoch" },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.atomicWaitQueues["epoch@0:all"].waiters).toEqual([]);
  expect(result.state.atomicWaitQueues["epoch@0:all"].history.at(-1)).toMatchObject({
    ticket: 0,
    outcome: "canceled",
  });
});

test("an ABA value does not complete atomic wait", () => {
  const result = runExecutionProgram([
    ...rootOperations(),
    { op: "createStorage", task: "root", id: "create", storage: "epoch", mode: "atomic" },
    {
      op: "atomicStore",
      task: "root",
      id: "initial",
      storage: "epoch",
      order: "relaxed",
      value: 0,
    },
    { op: "reserveTask", task: "waiter", parent: "root" },
    { op: "publishTask", task: "waiter", id: "waiter-start", after: "root-start" },
    {
      op: "atomicWaitStart",
      task: "waiter",
      id: "wait-register",
      storage: "epoch",
      order: "acquire",
      expected: 0,
      observes: "initial",
      ticket: 0,
    },
    {
      op: "atomicStore",
      task: "root",
      id: "transient",
      storage: "epoch",
      order: "release",
      value: 1,
    },
    {
      op: "atomicStore",
      task: "root",
      id: "aba",
      storage: "epoch",
      order: "release",
      value: 0,
    },
    { op: "atomicNotify", task: "root", id: "notify", storage: "epoch", mode: "all" },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.events.notify.wokenTickets).toEqual([]);
  expect(result.state.atomicWaitQueues["epoch@0:all"].waiters[0]).toMatchObject({
    ticket: 0,
    state: "waiting",
  });
});

test("the atomic wait fast path does not allocate a parking queue", () => {
  const result = runExecutionProgram([
    ...rootOperations(),
    { op: "createStorage", task: "root", id: "create", storage: "epoch", mode: "atomic" },
    {
      op: "atomicStore",
      task: "root",
      id: "published",
      storage: "epoch",
      order: "release",
      value: 1,
    },
    { op: "reserveTask", task: "waiter", parent: "root" },
    { op: "publishTask", task: "waiter", id: "waiter-start", after: "root-start" },
    {
      op: "atomicWaitStart",
      task: "waiter",
      id: "wait-fast",
      storage: "epoch",
      order: "acquire",
      expected: 0,
      observes: "published",
    },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.atomicWaitQueues).toEqual({});
  expect(result.state.events["wait-fast"]).toMatchObject({
    kind: "atomicWaitImmediate",
    waiting: false,
  });
});

test("a notified atomic waiter reparks when ABA restores its expected value", () => {
  const result = runExecutionProgram([
    ...rootOperations(),
    { op: "createStorage", task: "root", id: "create", storage: "epoch", mode: "atomic" },
    { op: "atomicStore", task: "root", id: "initial", storage: "epoch", order: "relaxed", value: 0 },
    { op: "reserveTask", task: "waiter", parent: "root" },
    { op: "publishTask", task: "waiter", id: "waiter-start", after: "root-start" },
    {
      op: "atomicWaitStart",
      task: "waiter",
      id: "wait-register",
      storage: "epoch",
      order: "acquire",
      expected: 0,
      observes: "initial",
      ticket: 0,
    },
    { op: "atomicStore", task: "root", id: "transient", storage: "epoch", order: "release", value: 1 },
    { op: "atomicNotify", task: "root", id: "notify", storage: "epoch", mode: "all" },
    { op: "atomicStore", task: "root", id: "aba", storage: "epoch", order: "release", value: 0 },
    {
      op: "atomicWaitRecheck",
      task: "waiter",
      id: "repark",
      storage: "epoch",
      order: "acquire",
      observes: "aba",
      ticket: 0,
    },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.events.repark.kind).toBe("atomicWaitReparked");
  expect(result.state.atomicWaitQueues["epoch@0:all"].waiters[0]).toMatchObject({
    ticket: 0,
    state: "waiting",
    notification: null,
  });
});

test("a committed notification wins a later cancellation", () => {
  const result = runExecutionProgram([
    ...rootOperations(),
    { op: "createStorage", task: "root", id: "create", storage: "epoch", mode: "atomic" },
    {
      op: "atomicStore",
      task: "root",
      id: "initial",
      storage: "epoch",
      order: "relaxed",
      value: 0,
    },
    { op: "reserveTask", task: "waiter", parent: "root" },
    { op: "publishTask", task: "waiter", id: "waiter-start", after: "root-start" },
    {
      op: "atomicWaitStart",
      task: "waiter",
      id: "wait-register",
      storage: "epoch",
      order: "acquire",
      expected: 0,
      observes: "initial",
      ticket: 0,
    },
    {
      op: "atomicStore",
      task: "root",
      id: "published",
      storage: "epoch",
      order: "release",
      value: 1,
    },
    { op: "atomicNotify", task: "root", id: "notify", storage: "epoch", mode: "one" },
    { op: "requestCancel", task: "waiter", reason: "deadline" },
    { op: "atomicWaitCancel", task: "waiter", id: "late-cancel", storage: "epoch", ticket: 0 },
    {
      op: "atomicWaitResume",
      task: "waiter",
      id: "wait-resume",
      storage: "epoch",
      order: "acquire",
      observes: "published",
      ticket: 0,
    },
  ]);

  expect(result.status).toBe("accepted");
  expect(result.state.events["late-cancel"].outcome).toBe("notificationWon");
  expect(result.state.tasks.waiter.cancellation.requestedReasons).toContain("deadline");
  expect(result.state.atomicWaitQueues["epoch@0:all"].waiters).toEqual([]);
});
