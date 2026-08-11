import { describe, expect, test } from "bun:test";
import { compactDLPackState, runDLPackProgram } from "./dlpack-machine.mjs";

const cpu = { provider: "cpu-a", kind: "cpu", id: 0 };
const tensor = (overrides = {}) => ({
  carrierKind: "versioned",
  version: { major: 1, minor: 3 },
  dtype: { name: "f32", lanes: 1 },
  ndim: 1,
  shape: [4],
  strides: [1],
  byteOffset: 0,
  dataPresent: true,
  device: cpu,
  flags: [],
  ...overrides,
});

const provider = (device = cpu, profile = {}) => ({
  op: "providerResolve",
  device,
  profile: { trusted: true, nativeEndian: true, storage: {}, layout: {}, ...profile },
  allocationExtent: 20,
  baseAlignment: 4,
});

describe("PYN4 DLPack host oracle", () => {
  test("consumes a versioned capsule and releases exactly once", () => {
    const result = runDLPackProgram([
      provider(),
      { op: "create", tensor: tensor() },
      { op: "capsule", name: "dltensor_versioned" },
      { op: "consume", name: "dltensor_versioned" },
      { op: "lease" },
      { op: "open" },
      { op: "close" },
    ]);
    expect(result.status).toBe("accepted");
    expect(compactDLPackState(result.state).releaseCalls).toBe(1);
  });

  test("rejects hidden copy and queue mismatch", () => {
    const hidden = runDLPackProgram([
      provider(),
      { op: "create", tensor: tensor() }, { op: "capsule" }, { op: "consume" }, { op: "lease" }, { op: "open" },
      { op: "hiddenCopy" },
    ]);
    expect(hidden.error.code).toBe("W-DLPACK-0028");

    const mismatch = runDLPackProgram([
      provider({ provider: "cuda-a", kind: "cuda", id: 0 }),
      { op: "create", tensor: tensor({ device: { provider: "cuda-a", kind: "cuda", id: 0 } }) },
      { op: "capsule" }, { op: "consume" }, { op: "lease" },
      { op: "open", queue: { device: { provider: "cuda-b", kind: "cuda", id: 0 } } },
    ]);
    expect(mismatch.error.code).toBe("W-DLPACK-0021");
  });

  test("Python lease drains before finalization", () => {
    const invalid = runDLPackProgram([
      { op: "python", action: "attach" }, { op: "python", action: "gil" },
      { op: "python", action: "lease" }, { op: "python", action: "finalize" },
    ]);
    expect(invalid.error.code).toBe("W-DLPACK-0030");
  });

  test("C Exchange N0 keeps metadata scoped and the producer through work drain", () => {
    const accepted = runDLPackProgram([{
      op: "cExchange",
      bridge: "python",
      apiStatic: true,
      capsuleName: "dlpack_exchange_api",
      gilHeld: true,
      ownership: "borrowed",
      escapes: false,
      suspends: false,
      streamResolved: true,
      controlReturned: true,
      ownerHeldUntilWorkDrained: true,
    }]);
    expect(accepted.status).toBe("accepted");
    expect(accepted.result.cExchangeCalls).toEqual([
      { scope: "callback", ownership: "borrowed", stream: "producer-current" },
    ]);

    const rejected = runDLPackProgram([{
      op: "cExchange",
      bridge: "python",
      apiStatic: true,
      capsuleName: "dlpack_exchange_api",
      gilHeld: true,
      ownership: "borrowed",
      streamResolved: true,
      controlReturned: true,
      ownerHeldUntilWorkDrained: false,
    }]);
    expect(rejected.status).toBe("rejected");
    expect(rejected.error.code).toBe("W-DLPACK-0032");
  });
});
