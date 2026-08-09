import assert from "node:assert/strict";
import test from "node:test";
import { runAllocationProgram } from "./allocation-machine.mjs";

function profile(overrides = {}) {
  return {
    maximumBytes: 4096,
    maximumAlignment: 4096,
    failureMode: "return",
    resize: "remap",
    movesOnResize: true,
    sizeClass: 16,
    allocateDomain: "any",
    deallocateDomain: "any",
    mobility: "crossDomain",
    progress: {
      allocate: "general",
      resize: "general",
      deallocate: "general",
    },
    deferredReuse: true,
    bulkRelease: false,
    homeDomain: "process",
    ...overrides,
  };
}

function fixtures() {
  return {
    providers: {
      system: profile(),
      local: profile({
        maximumBytes: 512,
        maximumAlignment: 64,
        resize: "inPlace",
        movesOnResize: false,
        sizeClass: 8,
        allocateDomain: "home",
        deallocateDomain: "home",
        mobility: "local",
        progress: { allocate: "bounded", resize: "bounded", deallocate: "bounded" },
        deferredReuse: false,
        homeDomain: "kitchen",
      }),
      fixed: profile({
        maximumBytes: 128,
        maximumAlignment: 16,
        resize: "none",
        movesOnResize: false,
        sizeClass: 1,
        allocateDomain: "home",
        deallocateDomain: "home",
        mobility: "local",
        progress: { allocate: "waitFree", resize: "waitFree", deallocate: "waitFree" },
        deferredReuse: false,
        bulkRelease: true,
        homeDomain: "audio",
      }),
    },
  };
}

test("zero bytes create no provider call", () => {
  const result = runAllocationProgram([
    { op: "allocate", block: "empty", provider: "system", size: 0, alignment: 8, domain: "process" },
    { op: "deallocate", block: "empty", provider: "system", domain: "process" },
  ], fixtures());

  assert.equal(result.status, "accepted");
  assert.deepEqual(result.state.providers.system.calls, {
    allocate: 0,
    resize: 0,
    deallocate: 0,
    bulkClose: 0,
  });
  assert.equal(result.state.blocks.empty.state, "retired");
  assert.equal(result.state.blocks.empty.token, null);
});

test("failed growth from no storage preserves the empty receipt", () => {
  const result = runAllocationProgram([
    { op: "allocate", block: "frames", provider: "fixed", size: 0, alignment: 8, domain: "audio" },
    { op: "faultNext", provider: "fixed", operation: "allocate", code: "allocationExhausted" },
    { op: "resize", block: "frames", size: 8, tailInitialization: "zeroed" },
  ], fixtures());

  assert.equal(result.code, "allocationExhausted");
  assert.equal(result.state.blocks.frames.state, "noStorage");
  assert.equal(result.state.blocks.frames.token, null);
  assert.equal(result.state.blocks.frames.requestedBytes, 0);
});

test("failed remap preserves the old receipt and bytes", () => {
  const result = runAllocationProgram([
    { op: "allocate", block: "menu", provider: "system", size: 8, alignment: 8, domain: "process" },
    { op: "write", block: "menu", index: 0, value: 91 },
    { op: "faultNext", provider: "system", operation: "resize", code: "allocationExhausted" },
    { op: "resize", block: "menu", size: 32, tailInitialization: "zeroed" },
  ], fixtures());

  assert.equal(result.code, "allocationExhausted");
  assert.equal(result.state.blocks.menu.token, "system:1");
  assert.equal(result.state.blocks.menu.requestedBytes, 8);
  assert.equal(result.state.blocks.menu.bytes[0], 91);
  assert.deepEqual(result.state.retiredTokens, []);
});

test("successful remap changes the receipt and preserves the prefix", () => {
  const result = runAllocationProgram([
    { op: "allocate", block: "menu", provider: "system", size: 4, alignment: 8, domain: "process" },
    { op: "write", block: "menu", index: 0, value: 42 },
    { op: "resize", block: "menu", size: 20, tailInitialization: "zeroed" },
  ], fixtures());

  assert.equal(result.status, "accepted");
  assert.equal(result.state.blocks.menu.token, "system:2");
  assert.equal(result.state.blocks.menu.generation, 1);
  assert.equal(result.state.blocks.menu.bytes[0], 42);
  assert.deepEqual(result.state.blocks.menu.bytes.slice(4), new Array(16).fill(0));
  assert.deepEqual(result.state.retiredTokens, ["system:1"]);
});

test("unsupported raw resize uses caller allocation fallback", () => {
  const result = runAllocationProgram([
    { op: "allocate", block: "samples", provider: "fixed", size: 4, alignment: 4, domain: "audio" },
    { op: "resize", block: "samples", size: 8, fallback: true, tailInitialization: "zeroed" },
  ], fixtures());

  assert.equal(result.status, "accepted");
  assert.deepEqual(result.state.providers.fixed.calls, {
    allocate: 2,
    resize: 0,
    deallocate: 1,
    bulkClose: 0,
  });
  assert.equal(result.state.blocks.samples.generation, 1);
});

test("loans, pinning, and address leases prevent relocation", () => {
  const borrowed = runAllocationProgram([
    { op: "allocate", block: "value", provider: "system", size: 8, alignment: 8, domain: "process" },
    { op: "beginLoan", block: "value" },
    { op: "resize", block: "value", size: 16 },
  ], fixtures());
  assert.equal(borrowed.code, "relocationWithActiveLoan");

  const pinned = runAllocationProgram([
    { op: "allocate", block: "value", provider: "system", size: 8, alignment: 8, domain: "process" },
    { op: "pin", block: "value" },
    { op: "resize", block: "value", size: 16 },
  ], fixtures());
  assert.equal(pinned.code, "relocationOfPinnedStorage");

  const leased = runAllocationProgram([
    { op: "allocate", block: "value", provider: "system", size: 8, alignment: 8, domain: "process" },
    { op: "pin", block: "value" },
    { op: "beginAddressLease", block: "value" },
    { op: "resize", block: "value", size: 16 },
  ], fixtures());
  assert.equal(leased.code, "relocationWithAddressLease");
});

test("failed consuming rehome retires the source", () => {
  const result = runAllocationProgram([
    { op: "allocate", block: "draft", provider: "local", size: 8, alignment: 8, domain: "kitchen" },
    { op: "faultNext", provider: "system", operation: "allocate", code: "allocationExhausted" },
    { op: "rehome", source: "draft", destination: "snapshot", provider: "system", domain: "process" },
  ], fixtures());

  assert.equal(result.code, "allocationExhausted");
  assert.equal(result.state.blocks.draft.state, "retired");
  assert.equal(result.state.blocks.snapshot, undefined);
});

test("retirement precedes physical reuse", () => {
  const retired = runAllocationProgram([
    { op: "allocate", block: "node", provider: "system", size: 8, alignment: 8, domain: "process" },
    { op: "deallocate", block: "node", provider: "system", domain: "process" },
  ], fixtures());
  assert.equal(retired.state.blocks.node.state, "retired");
  assert.equal(retired.state.blocks.node.physicallyReusable, false);

  const reclaimed = runAllocationProgram([
    { op: "allocate", block: "node", provider: "system", size: 8, alignment: 8, domain: "process" },
    { op: "deallocate", block: "node", provider: "system", domain: "process" },
    { op: "reclaim", block: "node" },
  ], fixtures());
  assert.equal(reclaimed.state.blocks.node.physicallyReusable, true);
});

test("progress requirements use explicit accepted sets", () => {
  const result = runAllocationProgram([
    { op: "requireProgress", provider: "local", operation: "allocate", accepted: ["lockFree"] },
  ], fixtures());

  assert.equal(result.code, "allocatorProgressRequirementUnsatisfied");
});

test("bulk close requires typed drops and active loans to drain", () => {
  const missingDropDrain = runAllocationProgram([
    { op: "allocate", block: "frame", provider: "fixed", size: 8, alignment: 8, domain: "audio" },
    { op: "bulkClose", provider: "fixed" },
  ], fixtures());
  assert.equal(missingDropDrain.code, "allocatorBulkCloseNotDrained");

  const activeLoan = runAllocationProgram([
    { op: "allocate", block: "frame", provider: "fixed", size: 8, alignment: 8, domain: "audio" },
    { op: "beginLoan", block: "frame" },
    { op: "bulkClose", provider: "fixed", dropsDrained: true },
  ], fixtures());
  assert.equal(activeLoan.code, "allocatorBulkCloseNotDrained");
});

test("a receipt can only return to its origin provider", () => {
  const result = runAllocationProgram([
    { op: "allocate", block: "dish", provider: "system", size: 8, alignment: 8, domain: "process" },
    { op: "deallocate", block: "dish", provider: "local", domain: "process" },
  ], fixtures());

  assert.equal(result.code, "allocationOriginMismatch");
  assert.equal(result.state.blocks.dish.state, "live");
});

test("rehome can place storage in a local destination domain", () => {
  const result = runAllocationProgram([
    { op: "allocate", block: "source", provider: "system", size: 8, alignment: 8, domain: "process" },
    { op: "rehome", source: "source", destination: "audioCopy", provider: "fixed", domain: "audio" },
  ], fixtures());

  assert.equal(result.status, "accepted");
  assert.equal(result.state.blocks.source.state, "retired");
  assert.equal(result.state.blocks.audioCopy.provider, "fixed");
  assert.equal(result.state.blocks.audioCopy.domain, "audio");
});

test("large safe power-of-two alignments do not use 32-bit arithmetic", () => {
  const largeAlignment = 2 ** 40;
  const largeFixtures = {
    providers: {
      huge: profile({ maximumAlignment: largeAlignment }),
    },
  };
  const result = runAllocationProgram([
    { op: "allocate", block: "page", provider: "huge", size: 8, alignment: largeAlignment, domain: "process" },
  ], largeFixtures);

  assert.equal(result.status, "accepted");
  assert.equal(result.state.blocks.page.alignment, largeAlignment);
});
