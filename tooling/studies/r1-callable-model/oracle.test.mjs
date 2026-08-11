import { describe, expect, test } from "bun:test";

function applicationOutcome(input) {
  return {
    firstGate: 1,
    routedGate: input.gate,
    tickets: [input.initial + 1, input.initial + 2],
    manifestCount: input.orderIds.length,
  };
}

function separatedContract(input) {
  return {
    ...applicationOutcome(input),
    manifestAvailable: false,
    staticRepresentation: "thin",
    routedRepresentation: "erased",
    counterAccess: "exclusive-repeatable",
    manifestAccess: "consuming",
    erasureFailure: "allocation-error",
  };
}

function unifiedContract(input) {
  return {
    ...applicationOutcome(input),
    manifestAvailable: true,
    staticRepresentation: "unspecified-callable",
    routedRepresentation: "unspecified-callable",
    counterAccess: "not-expressed",
    manifestAccess: "not-expressed",
    erasureFailure: "not-recoverable",
  };
}

function protocolContract(input) {
  return {
    ...applicationOutcome(input),
    manifestAvailable: false,
    staticRepresentation: "opaque-concrete",
    routedRepresentation: "erased-witness",
    counterAccess: "exclusive-repeatable-requirement",
    manifestAccess: "consuming-requirement",
    erasureFailure: "allocation-error",
  };
}

function functionValueSurface({ declarationLabel, hasDefault }) {
  return {
    declarationLabel,
    declarationHasDefault: hasDefault,
    storedType: "fn(Arrival): Welcome",
    storedLabels: [],
    storedDefaults: [],
    invocation: "greeter(arrival)",
    omittedArgumentAccepted: false,
  };
}

function callableCompatibility(actual, expected) {
  for (const field of ["parameter", "ownership", "result", "error"]) {
    if (actual[field] !== expected[field]) {
      return { status: "rejected", reason: `${field}-invariant` };
    }
  }
  return { status: "accepted" };
}

describe("R1 callable-model host oracle", () => {
  const inputs = [
    { gate: 2, initial: 40, orderIds: [7, 8, 9] },
    { gate: 0, initial: 0, orderIds: [] },
  ];

  test("all variants preserve the restaurant outcome", () => {
    for (const input of inputs) {
      const expected = applicationOutcome(input);
      expect(separatedContract(input)).toMatchObject(expected);
      expect(unifiedContract(input)).toMatchObject(expected);
      expect(protocolContract(input)).toMatchObject(expected);
    }
  });

  test("only the unified spelling loses static consuming access", () => {
    const input = inputs[0];
    expect(separatedContract(input).manifestAccess).toBe("consuming");
    expect(protocolContract(input).manifestAccess).toBe("consuming-requirement");
    expect(unifiedContract(input).manifestAccess).toBe("not-expressed");
    expect(unifiedContract(input).manifestAvailable).toBe(true);
  });

  test("the selected form distinguishes thin, opaque, and erased representation", () => {
    const result = separatedContract(inputs[0]);
    expect(result.staticRepresentation).toBe("thin");
    expect(result.routedRepresentation).toBe("erased");
    expect(result.counterAccess).toBe("exclusive-repeatable");
  });

  test("only explicit erasure forms expose recoverable allocation failure", () => {
    const input = inputs[0];
    expect(separatedContract(input).erasureFailure).toBe("allocation-error");
    expect(protocolContract(input).erasureFailure).toBe("allocation-error");
    expect(unifiedContract(input).erasureFailure).toBe("not-recoverable");
  });

  test("function values keep parameter types but not declaration labels or defaults", () => {
    expect(functionValueSurface({ declarationLabel: "arrival", hasDefault: true })).toEqual({
      declarationLabel: "arrival",
      declarationHasDefault: true,
      storedType: "fn(Arrival): Welcome",
      storedLabels: [],
      storedDefaults: [],
      invocation: "greeter(arrival)",
      omittedArgumentAccepted: false,
    });
  });

  test("callable parameter result ownership and error contracts are invariant", () => {
    const exact = {
      parameter: "Payment",
      ownership: "value",
      result: "Receipt",
      error: "PaymentError",
    };
    expect(callableCompatibility(exact, { ...exact })).toEqual({ status: "accepted" });
    expect(callableCompatibility({ ...exact, parameter: "CardPayment" }, exact)).toEqual({
      status: "rejected",
      reason: "parameter-invariant",
    });
    expect(callableCompatibility({ ...exact, result: "AnyReceipt" }, exact)).toEqual({
      status: "rejected",
      reason: "result-invariant",
    });
    expect(callableCompatibility({ ...exact, error: "Error" }, exact)).toEqual({
      status: "rejected",
      reason: "error-invariant",
    });
  });
});
