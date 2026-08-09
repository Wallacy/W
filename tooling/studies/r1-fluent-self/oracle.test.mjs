import { describe, expect, test } from "bun:test";

function validateMethod({ receiverMode, declaredReturn, exit, fluentUse = false }) {
  const returnType = declaredReturn ?? "Unit";

  if (receiverMode === "take" && returnType === "self") {
    return { accepted: false, reason: "take-self-reborrow" };
  }
  if (returnType === "self" && receiverMode === "mut" && ["fallthrough", "returnSelf"].includes(exit)) {
    return { accepted: true, result: "reborrow", exit };
  }
  if (returnType === "Unit") {
    return fluentUse
      ? { accepted: false, reason: "Unit-not-fluent" }
      : { accepted: true, result: "Unit" };
  }
  if (declaredReturn === "Self") {
    return { accepted: true, result: "owned-self", storage: "new-owner" };
  }
  return { accepted: false, reason: "unsupported-self-contract" };
}

function record(ledger, kind, details = {}) {
  ledger.events.push({ kind, ...details });
}

function resetReborrow(receiver, exit, ledger) {
  record(ledger, "borrow-receiver", { origin: receiver.id, storage: receiver.storage });
  receiver.phase = 0;
  record(ledger, "mutate-receiver", { storage: receiver.storage });
  if (exit === "returnSelf") record(ledger, "return-self", { origin: receiver.id });
  record(ledger, "return-reborrow", { origin: receiver.id, storage: receiver.storage });
  return receiver;
}

function createLedger() {
  return { events: [] };
}

function makeOwnedSelf(receiver, ledger) {
  const owned = { id: `${receiver.id}-owned`, storage: "new-owner", phase: receiver.phase };
  record(ledger, "allocate-owned-self", { source: receiver.storage, storage: owned.storage });
  return owned;
}

describe("R1 fluent-self host oracle", () => {
  test("mut self fallthrough and returnSelf preserve borrow and storage identity", () => {
    for (const exit of ["fallthrough", "returnSelf"]) {
      const receiver = { id: "osc-1", storage: "storage-1", phase: 0.75 };
      const ledger = createLedger();
      const contract = validateMethod({ receiverMode: "mut", declaredReturn: "self", exit });
      expect(contract.accepted).toBe(true);
      expect(contract.result).toBe("reborrow");
      const returned = resetReborrow(receiver, exit, ledger);
      expect(returned.id).toBe(receiver.id);
      expect(returned.storage).toBe(receiver.storage);
      expect(returned.phase).toBe(0);
      expect(ledger.events.find((event) => event.kind === "borrow-receiver")).toEqual({
        kind: "borrow-receiver",
        origin: "osc-1",
        storage: "storage-1",
      });
      expect(ledger.events.find((event) => event.kind === "return-reborrow")).toEqual({
        kind: "return-reborrow",
        origin: "osc-1",
        storage: "storage-1",
      });
      expect(ledger.events.filter((event) => ["allocate-owned-self", "copy", "move"].includes(event.kind))).toHaveLength(0);
    }
  });

  test("omitted type produces Unit and rejects fluent use", () => {
    const statementContract = validateMethod({ receiverMode: "mut", declaredReturn: undefined, fluentUse: false });
    const fluentContract = validateMethod({ receiverMode: "mut", declaredReturn: undefined, fluentUse: true });
    const explicitUnit = validateMethod({ receiverMode: "mut", declaredReturn: "Unit", fluentUse: false });
    expect(statementContract.result).toBe("Unit");
    expect(explicitUnit.result).toBe("Unit");
    expect(fluentContract.reason).toBe("Unit-not-fluent");
  });

  test("take fn cannot use a self reborrow and Self remains owned", () => {
    const takeSelf = validateMethod({ receiverMode: "take", declaredReturn: "self", exit: "fallthrough" });
    const ownedSelf = validateMethod({ receiverMode: "mut", declaredReturn: "Self", exit: "returnSelf" });
    const receiver = { id: "osc-1", storage: "storage-1", phase: 0.25 };
    const ownedLedger = createLedger();
    const returnedOwned = makeOwnedSelf(receiver, ownedLedger);
    expect(takeSelf.reason).toBe("take-self-reborrow");
    expect(ownedSelf.result).toBe("owned-self");
    expect(ownedSelf.storage).toBe("new-owner");
    expect(returnedOwned.id).not.toBe(receiver.id);
    expect(returnedOwned.storage).not.toBe(receiver.storage);
    expect(ownedLedger.events).toContainEqual({
      kind: "allocate-owned-self",
      source: "storage-1",
      storage: "new-owner",
    });
  });
});
