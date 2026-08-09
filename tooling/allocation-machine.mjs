// Pure design oracle. This file does not allocate host memory or implement W.

const OPERATIONS = new Set([
  "bind",
  "faultNext",
  "allocate",
  "write",
  "beginLoan",
  "endLoan",
  "pin",
  "beginAddressLease",
  "endAddressLease",
  "resize",
  "moveDomain",
  "rehome",
  "deallocate",
  "reclaim",
  "bulkClose",
  "requireProgress",
]);

const PROGRESS = new Set(["general", "bounded", "lockFree", "waitFree"]);

export class AllocationMachineError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

function fail(code) {
  throw new AllocationMachineError(code);
}

function clone(value) {
  return structuredClone(value);
}

function isPowerOfTwo(value) {
  if (!Number.isSafeInteger(value) || value <= 0) return false;
  const bits = BigInt(value);
  return (bits & (bits - 1n)) === 0n;
}

function roundUsable(size, sizeClass) {
  if (size === 0) return 0;
  return Math.ceil(size / sizeClass) * sizeClass;
}

function makeState(fixtures) {
  const providers = {};
  for (const [name, profile] of Object.entries(fixtures.providers ?? {})) {
    providers[name] = {
      ...clone(profile),
      calls: { allocate: 0, resize: 0, deallocate: 0, bulkClose: 0 },
      liveBlocks: 0,
      closed: false,
      nextToken: 1,
      faults: [],
    };
  }
  return { providers, blocks: {}, retiredTokens: [] };
}

function providerFor(state, name) {
  const provider = state.providers[name];
  if (!provider) fail("unknownAllocatorProvider");
  if (provider.closed) fail("allocatorProviderClosed");
  return provider;
}

function blockFor(state, name, { live = true } = {}) {
  const block = state.blocks[name];
  if (!block) fail("unknownRawBlock");
  if (live && block.state !== "live" && block.state !== "noStorage") {
    fail("rawBlockNotLive");
  }
  return block;
}

function validateLayout(provider, size, alignment) {
  if (!Number.isSafeInteger(size) || size < 0 || !Number.isSafeInteger(alignment)) {
    fail("invalidAllocationLayout");
  }
  if (!isPowerOfTwo(alignment)) fail("invalidAllocationLayout");
  if (alignment > provider.maximumAlignment) fail("unsupportedAllocationAlignment");
  if (size > provider.maximumBytes) fail("allocationSizeExceeded");
}

function domainAllows(rule, domain, provider) {
  return rule === "any" || rule === domain || (rule === "home" && provider.homeDomain === domain);
}

function consumeFault(provider, operation) {
  const index = provider.faults.findIndex((fault) => fault.operation === operation);
  if (index < 0) return null;
  return provider.faults.splice(index, 1)[0].code;
}

function initializedBytes(size, initialization) {
  if (initialization === "zeroed") return new Array(size).fill(0);
  if (initialization === "uninitialized") return new Array(size).fill(null);
  fail("unknownAllocationInitialization");
}

function allocateBlock(state, operation, { countCall = true } = {}) {
  if (state.blocks[operation.block]) fail("duplicateRawBlock");
  const provider = providerFor(state, operation.provider);
  validateLayout(provider, operation.size, operation.alignment);
  if (!domainAllows(provider.allocateDomain, operation.domain, provider)) {
    fail("allocatorAllocationDomainViolation");
  }
  if (operation.size === 0) {
    state.blocks[operation.block] = {
      state: "noStorage",
      provider: operation.provider,
      token: null,
      generation: 0,
      requestedBytes: 0,
      usableBytes: 0,
      alignment: operation.alignment,
      domain: operation.domain,
      bytes: [],
      loans: 0,
      pinned: false,
      addressLeases: 0,
      physicallyReusable: true,
    };
    return state.blocks[operation.block];
  }
  if (countCall) provider.calls.allocate += 1;
  const fault = consumeFault(provider, "allocate");
  if (fault) fail(fault);
  const token = `${operation.provider}:${provider.nextToken++}`;
  state.blocks[operation.block] = {
    state: "live",
    provider: operation.provider,
    token,
    generation: 0,
    requestedBytes: operation.size,
    usableBytes: roundUsable(operation.size, provider.sizeClass),
    alignment: operation.alignment,
    domain: operation.domain,
    bytes: initializedBytes(operation.size, operation.initialization ?? "uninitialized"),
    loans: 0,
    pinned: false,
    addressLeases: 0,
    physicallyReusable: false,
  };
  provider.liveBlocks += 1;
  return state.blocks[operation.block];
}

function retireBlock(state, name, domain, expectedProvider) {
  const block = blockFor(state, name);
  if (expectedProvider !== undefined && block.provider !== expectedProvider) {
    fail("allocationOriginMismatch");
  }
  if (block.state === "noStorage") {
    block.state = "retired";
    return;
  }
  if (block.loans !== 0) fail("deallocateWithActiveLoan");
  if (block.addressLeases !== 0) fail("deallocateWithAddressLease");
  const provider = providerFor(state, block.provider);
  if (!domainAllows(provider.deallocateDomain, domain, provider)) {
    fail("allocatorDeallocationDomainViolation");
  }
  provider.calls.deallocate += 1;
  provider.liveBlocks -= 1;
  block.state = "retired";
  block.physicallyReusable = !provider.deferredReuse;
  state.retiredTokens.push(block.token);
}

function requireRelocatable(block) {
  if (block.loans !== 0) fail("relocationWithActiveLoan");
  if (block.addressLeases !== 0) fail("relocationWithAddressLease");
  if (block.pinned) fail("relocationOfPinnedStorage");
}

function resizeBytes(block, size, tailInitialization) {
  if (size < block.requestedBytes) return block.bytes.slice(0, size);
  const tail = initializedBytes(size - block.requestedBytes, tailInitialization);
  return [...block.bytes, ...tail];
}

function resizeBlock(state, operation) {
  const block = blockFor(state, operation.block);
  if (block.state === "noStorage") {
    const prior = clone(block);
    delete state.blocks[operation.block];
    try {
      const resized = allocateBlock(state, {
        ...operation,
        provider: block.provider,
        domain: block.domain,
        alignment: operation.alignment ?? block.alignment,
        initialization: operation.tailInitialization ?? "uninitialized",
      });
      resized.generation = prior.generation + (operation.size === 0 ? 0 : 1);
      return resized;
    } catch (error) {
      state.blocks[operation.block] = prior;
      throw error;
    }
  }
  requireRelocatable(block);
  const provider = providerFor(state, block.provider);
  const alignment = operation.alignment ?? block.alignment;
  validateLayout(provider, operation.size, alignment);
  if (operation.size === 0) {
    retireBlock(state, operation.block, block.domain, block.provider);
    state.blocks[operation.block] = {
      ...block,
      state: "noStorage",
      token: null,
      generation: block.generation + 1,
      requestedBytes: 0,
      usableBytes: 0,
      bytes: [],
      physicallyReusable: true,
    };
    return;
  }

  const tailInitialization = operation.tailInitialization ?? "uninitialized";
  const nextBytes = resizeBytes(block, operation.size, tailInitialization);

  if (provider.resize === "none") {
    if (!operation.fallback) fail("allocatorResizeUnsupported");
    const allocationFault = consumeFault(provider, "allocate");
    provider.calls.allocate += 1;
    if (allocationFault) fail(allocationFault);
    const priorToken = block.token;
    block.token = `${block.provider}:${provider.nextToken++}`;
    block.generation += 1;
    block.requestedBytes = operation.size;
    block.usableBytes = roundUsable(operation.size, provider.sizeClass);
    block.alignment = alignment;
    block.bytes = nextBytes;
    provider.calls.deallocate += 1;
    state.retiredTokens.push(priorToken);
    return;
  }

  provider.calls.resize += 1;
  const fault = consumeFault(provider, "resize");
  if (fault) fail(fault);

  if (provider.resize === "inPlace") {
    block.requestedBytes = operation.size;
    block.usableBytes = roundUsable(operation.size, provider.sizeClass);
    block.alignment = alignment;
    block.bytes = nextBytes;
    return;
  }

  if (provider.resize === "remap") {
    if (provider.movesOnResize) {
      const priorToken = block.token;
      block.token = `${block.provider}:${provider.nextToken++}`;
      block.generation += 1;
      state.retiredTokens.push(priorToken);
    }
    block.requestedBytes = operation.size;
    block.usableBytes = roundUsable(operation.size, provider.sizeClass);
    block.alignment = alignment;
    block.bytes = nextBytes;
    return;
  }
  fail("invalidAllocatorResizeCapability");
}

function requirementAccepts(profile, requirement) {
  if (requirement.fallible === true && profile.failureMode !== "return") return false;
  if (requirement.mobility && profile.mobility !== requirement.mobility) return false;
  if (requirement.bulkRelease === true && profile.bulkRelease !== true) return false;
  return true;
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "bind": {
      const provider = providerFor(state, operation.provider);
      if (!requirementAccepts(provider, operation.requirement ?? {})) {
        fail("allocatorProfileRequirementUnsatisfied");
      }
      return;
    }
    case "faultNext": {
      const provider = providerFor(state, operation.provider);
      if (!new Set(["allocate", "resize"]).has(operation.operation)) {
        fail("invalidAllocationFault");
      }
      provider.faults.push({ operation: operation.operation, code: operation.code });
      return;
    }
    case "allocate":
      allocateBlock(state, operation);
      return;
    case "write": {
      const block = blockFor(state, operation.block);
      if (block.state === "noStorage" || !Number.isInteger(operation.index) ||
          operation.index < 0 || operation.index >= block.requestedBytes) {
        fail("allocationWriteOutOfBounds");
      }
      if (!Number.isInteger(operation.value) || operation.value < 0 || operation.value > 255) {
        fail("invalidAllocationByte");
      }
      block.bytes[operation.index] = operation.value;
      return;
    }
    case "beginLoan": {
      const block = blockFor(state, operation.block);
      if (block.state === "noStorage") fail("loanRequiresStorage");
      block.loans += 1;
      return;
    }
    case "endLoan": {
      const block = blockFor(state, operation.block);
      if (block.loans === 0) fail("invalidAllocationLoanEnd");
      block.loans -= 1;
      return;
    }
    case "pin": {
      const block = blockFor(state, operation.block);
      if (block.state === "noStorage") fail("pinRequiresStorage");
      if (block.loans !== 0) fail("pinWithActiveLoan");
      block.pinned = true;
      return;
    }
    case "beginAddressLease": {
      const block = blockFor(state, operation.block);
      if (!block.pinned) fail("addressLeaseRequiresPinnedStorage");
      block.addressLeases += 1;
      return;
    }
    case "endAddressLease": {
      const block = blockFor(state, operation.block);
      if (block.addressLeases === 0) fail("invalidAddressLeaseEnd");
      block.addressLeases -= 1;
      return;
    }
    case "resize":
      resizeBlock(state, operation);
      return;
    case "moveDomain": {
      const block = blockFor(state, operation.block);
      const provider = providerFor(state, block.provider);
      if (block.loans !== 0) fail("allocationDomainMoveWithActiveLoan");
      if (block.addressLeases !== 0) fail("allocationDomainMoveWithAddressLease");
      if (provider.mobility !== "crossDomain") fail("allocationOriginNotTransferable");
      block.domain = operation.domain;
      return;
    }
    case "rehome": {
      const source = blockFor(state, operation.source);
      requireRelocatable(source);
      const snapshot = {
        bytes: [...source.bytes],
        size: source.requestedBytes,
        alignment: source.alignment,
        domain: operation.domain,
      };
      providerFor(state, operation.provider);
      try {
        allocateBlock(state, {
          op: "allocate",
          block: operation.destination,
          provider: operation.provider,
          size: snapshot.size,
          alignment: snapshot.alignment,
          domain: operation.domain,
          initialization: "uninitialized",
        });
      } catch (error) {
        retireBlock(state, operation.source, source.domain, source.provider);
        throw error;
      }
      const destination = state.blocks[operation.destination];
      destination.bytes = snapshot.bytes;
      retireBlock(state, operation.source, source.domain, source.provider);
      return;
    }
    case "deallocate":
      retireBlock(state, operation.block, operation.domain, operation.provider);
      return;
    case "reclaim": {
      const block = blockFor(state, operation.block, { live: false });
      if (block.state !== "retired") fail("reclaimRequiresRetiredBlock");
      block.physicallyReusable = true;
      return;
    }
    case "bulkClose": {
      const provider = providerFor(state, operation.provider);
      if (!provider.bulkRelease) fail("allocatorBulkReleaseUnsupported");
      if (operation.dropsDrained !== true) fail("allocatorBulkCloseNotDrained");
      const live = Object.values(state.blocks).filter(
        (block) => block.provider === operation.provider &&
          (block.state === "live" || block.state === "noStorage"),
      );
      if (live.some((block) => block.loans !== 0 || block.addressLeases !== 0)) {
        fail("allocatorBulkCloseNotDrained");
      }
      for (const block of live) {
        block.state = "retired";
        block.physicallyReusable = true;
        if (block.token !== null) state.retiredTokens.push(block.token);
      }
      provider.liveBlocks = 0;
      provider.calls.bulkClose += 1;
      provider.closed = true;
      return;
    }
    case "requireProgress": {
      const provider = providerFor(state, operation.provider);
      const actual = provider.progress?.[operation.operation];
      if (!PROGRESS.has(actual) || !(operation.accepted ?? []).includes(actual)) {
        fail("allocatorProgressRequirementUnsatisfied");
      }
      return;
    }
    default:
      fail("unknownAllocationOperation");
  }
}

function stableState(state) {
  const providers = Object.fromEntries(
    Object.entries(state.providers).map(([name, provider]) => [name, {
      closed: provider.closed,
      liveBlocks: provider.liveBlocks,
      calls: provider.calls,
      pendingFaults: provider.faults.length,
    }]),
  );
  const blocks = Object.fromEntries(
    Object.entries(state.blocks).map(([name, block]) => [name, {
      state: block.state,
      provider: block.provider,
      token: block.token,
      generation: block.generation,
      requestedBytes: block.requestedBytes,
      usableBytes: block.usableBytes,
      alignment: block.alignment,
      domain: block.domain,
      bytes: block.bytes,
      loans: block.loans,
      pinned: block.pinned,
      addressLeases: block.addressLeases,
      physicallyReusable: block.physicallyReusable,
    }]),
  );
  return { providers, blocks, retiredTokens: state.retiredTokens };
}

export function validateAllocationOperation(operation) {
  return operation !== null && typeof operation === "object" &&
    typeof operation.op === "string" && OPERATIONS.has(operation.op);
}

export function runAllocationProgram(operations, fixtures) {
  const state = makeState(fixtures);
  const trace = [];
  for (const [index, operation] of operations.entries()) {
    try {
      applyOperation(state, operation);
      trace.push({ operation: index, op: operation.op, status: "accepted" });
    } catch (error) {
      if (!(error instanceof AllocationMachineError)) throw error;
      trace.push({ operation: index, op: operation.op, status: "rejected", code: error.code });
      return {
        status: "rejected",
        code: error.code,
        operation: index,
        state: stableState(state),
        trace,
      };
    }
  }
  return { status: "accepted", state: stableState(state), trace };
}
