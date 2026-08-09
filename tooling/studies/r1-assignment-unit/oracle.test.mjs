import { describe, expect, test } from "bun:test";

function owner(id) {
  return { id };
}

function createLedger(oldOwner) {
  return {
    events: [],
    resolveCount: 0,
    rhsCount: 0,
    placeReads: 0,
    placeWrites: 0,
    drops: new Map([[oldOwner.id, 0]]),
    ownership: new Map([[oldOwner.id, 1]]),
  };
}

function drop(ledger, value) {
  ledger.drops.set(value.id, (ledger.drops.get(value.id) ?? 0) + 1);
  ledger.ownership.set(value.id, 0);
}

function assign(place, resolvePlace, rhs, ledger) {
  const target = resolvePlace();
  let next;
  try {
    next = rhs();
  } catch (error) {
    ledger.events.push("rhs-failure");
    return { kind: "error", error };
  }
  const old = target.value;
  target.value = next;
  ledger.events.push("replace");
  drop(ledger, old);
  ledger.events.push("drop-old");
  return { kind: "Unit" };
}

function compound(place, resolvePlace, ledger) {
  const target = resolvePlace();
  ledger.resolveCount += 1;
  ledger.placeReads += 1;
  target.value += 1;
  ledger.placeWrites += 1;
  ledger.events.push("compound-read-write");
}

function validateAssignment({ operator, context }) {
  if (operator === "=" && (context === "value" || context === "assignment-rhs")) {
    return { accepted: false, reason: "assignment-is-Unit" };
  }
  return { accepted: true, type: "Unit" };
}

describe("R1 assignment-unit host oracle", () => {
  test("place and RHS callbacks run once before replacement", () => {
    const oldOwner = owner("owner-1");
    const nextOwner = owner("owner-2");
    const place = { value: oldOwner };
    const ledger = createLedger(oldOwner);
    const resolvePlace = () => {
      ledger.resolveCount += 1;
      ledger.events.push("resolve-place");
      return place;
    };
    const result = assign(place, resolvePlace, () => {
      ledger.rhsCount += 1;
      ledger.ownership.set(nextOwner.id, 1);
      ledger.events.push("rhs-success");
      return nextOwner;
    }, ledger);

    expect(result.kind).toBe("Unit");
    expect(ledger.resolveCount).toBe(1);
    expect(ledger.rhsCount).toBe(1);
    expect(place.value.id).toBe("owner-2");
    expect(ledger.drops.get(oldOwner.id)).toBe(1);
    expect(ledger.ownership.get(oldOwner.id)).toBe(0);
    expect(ledger.ownership.get(nextOwner.id)).toBe(1);
  });

  test("RHS failure preserves the old owner and does not drop it", () => {
    const oldOwner = owner("owner-1");
    const place = { value: oldOwner };
    const ledger = createLedger(oldOwner);
    const failure = new Error("rhs failed");
    const result = assign(
      place,
      () => {
        ledger.resolveCount += 1;
        return place;
      },
      () => {
        ledger.rhsCount += 1;
        throw failure;
      },
      ledger,
    );

    expect(result.kind).toBe("error");
    expect(result.error).toBe(failure);
    expect(place.value.id).toBe("owner-1");
    expect(ledger.ownership.get(oldOwner.id)).toBe(1);
    expect(ledger.drops.get(oldOwner.id)).toBe(0);
    expect(ledger.resolveCount).toBe(1);
    expect(ledger.rhsCount).toBe(1);
  });

  test("move-only replacement has one owner and compound uses one place", () => {
    const oldOwner = owner("owner-1");
    const nextOwner = owner("owner-2");
    const place = { value: oldOwner };
    const ledger = createLedger(oldOwner);
    const resolvePlace = () => {
      ledger.resolveCount += 1;
      return place;
    };
    assign(place, resolvePlace, () => {
      ledger.rhsCount += 1;
      ledger.ownership.set(nextOwner.id, 1);
      return nextOwner;
    }, ledger);

    const numericPlace = { value: 4 };
    const compoundLedger = { resolveCount: 0, placeReads: 0, placeWrites: 0, events: [] };
    compound(numericPlace, () => numericPlace, compoundLedger);
    expect(numericPlace.value).toBe(5);
    expect(compoundLedger.resolveCount).toBe(1);
    expect(compoundLedger.placeReads).toBe(1);
    expect(compoundLedger.placeWrites).toBe(1);
    expect(ledger.ownership.get(nextOwner.id)).toBe(1);
  });

  test("value context and assignment chains are rejected by context facts", () => {
    expect(validateAssignment({ operator: "=", context: "value" }).reason).toBe("assignment-is-Unit");
    expect(validateAssignment({ operator: "=", context: "assignment-rhs" }).accepted).toBe(false);
    expect(validateAssignment({ operator: "=", context: "statement" }).type).toBe("Unit");
  });
});
