import { describe, expect, test } from "bun:test";

function constructShared(input) {
  const trace = [];
  const reject = (reason) => ({ status: "rejected", reason, trace });

  if (input.form === "contextual-type") {
    return reject("implicit-ownership-promotion");
  }
  if (input.source === "binding" && !input.take) {
    return reject("missing-take");
  }
  if (!input.lifetimeIndependent) {
    return reject("lifetime-dependent");
  }

  const recoverable = input.form === "share-using" || input.form === "tryShare";
  if (input.allocator !== "product.default" && !recoverable) {
    return reject("allocator-requires-fallible-operation");
  }

  trace.push({ operation: "consume-source", source: input.source });
  trace.push({ operation: "allocate-control", allocator: input.allocator });

  if (input.failure) {
    trace.push({ operation: "drop-source", count: 1 });
    trace.push({ operation: "release-partial-control", count: 1 });
    return {
      status: recoverable ? "allocation-error" : "panic",
      reason: input.failure,
      published: false,
      trace,
    };
  }

  trace.push({ operation: "publish-shared-owner", strong: 1, weak: 0 });
  return {
    status: "accepted",
    resultType: "shared MenuSection",
    allocationVisible: input.form !== "contextual-type",
    failurePolicy: recoverable ? "throws AllocationError" : "normal OOM",
    published: true,
    trace,
  };
}

describe("R1 shared-construction host oracle", () => {
  test("selected forms separate normal and recoverable allocation", () => {
    const normal = constructShared({
      form: "share-default",
      source: "temporary",
      take: false,
      allocator: "product.default",
      lifetimeIndependent: true,
    });
    const recoverable = constructShared({
      form: "share-using",
      source: "binding",
      take: true,
      allocator: "request.arena",
      lifetimeIndependent: true,
    });
    expect(normal).toMatchObject({
      status: "accepted",
      resultType: "shared MenuSection",
      allocationVisible: true,
      failurePolicy: "normal OOM",
    });
    expect(recoverable).toMatchObject({
      status: "accepted",
      resultType: "shared MenuSection",
      allocationVisible: true,
      failurePolicy: "throws AllocationError",
    });
  });

  test("failure cleans source and partial control exactly once", () => {
    for (const form of ["share-default", "share-using"]) {
      const result = constructShared({
        form,
        source: "binding",
        take: true,
        allocator: form === "share-default" ? "product.default" : "request.arena",
        lifetimeIndependent: true,
        failure: form === "share-default" ? "outOfMemory" : "budgetExceeded",
      });
      expect(result.status).toBe(form === "share-default" ? "panic" : "allocation-error");
      expect(result.published).toBe(false);
      expect(result.trace.filter((event) => event.operation === "drop-source")).toEqual([
        { operation: "drop-source", count: 1 },
      ]);
      expect(result.trace.filter((event) => event.operation === "release-partial-control")).toEqual([
        { operation: "release-partial-control", count: 1 },
      ]);
    }
  });

  test("a separate recoverable verb preserves the same ownership outcome", () => {
    const selected = constructShared({
      form: "share-using",
      source: "binding",
      take: true,
      allocator: "request.arena",
      lifetimeIndependent: true,
    });
    const alternative = constructShared({
      form: "tryShare",
      source: "binding",
      take: true,
      allocator: "request.arena",
      lifetimeIndependent: true,
    });
    expect(alternative).toEqual(selected);
  });

  test("existing owners require take and borrowed payloads remain rejected", () => {
    const missingTake = constructShared({
      form: "share-default",
      source: "binding",
      take: false,
      allocator: "product.default",
      lifetimeIndependent: true,
    });
    const dependent = constructShared({
      form: "share-using",
      source: "binding",
      take: true,
      allocator: "request.arena",
      lifetimeIndependent: false,
    });
    expect(missingTake.reason).toBe("missing-take");
    expect(dependent.reason).toBe("lifetime-dependent");
  });

  test("expected type cannot hide allocation or ownership promotion", () => {
    const result = constructShared({
      form: "contextual-type",
      source: "temporary",
      take: false,
      allocator: "product.default",
      lifetimeIndependent: true,
    });
    expect(result).toEqual({
      status: "rejected",
      reason: "implicit-ownership-promotion",
      trace: [],
    });
  });
});
