const OWNER_STATES = new Set(["owned", "moved", "dropped"]);
const ADDRESS_STATES = new Set(["unstable", "stable", "published"]);
const BOUNDARIES = new Set([
  "internal",
  "wExact",
  "foreignC",
  "wire",
  "persisted",
  "capability",
]);
const REPRESENTATIONS = new Set([
  "explicitTag",
  "provenNiche",
  "lowBit",
  "highBit",
  "nativeCarrier",
]);

export class HirMemoryError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

function clone(value) {
  return structuredClone(value);
}

function activeBorrowsForPayload(state, payload) {
  return Object.values(state.borrows).filter((borrow) => borrow.payload === payload);
}

function requireBinding(state, name) {
  const binding = state.bindings[name];
  if (!binding || binding.state !== "owned") {
    throw new HirMemoryError("operationRequiresOwner");
  }
  return binding;
}

function requireNoBorrow(state, payload, code) {
  if (activeBorrowsForPayload(state, payload).length > 0) {
    throw new HirMemoryError(code);
  }
}

function acceptsRepresentation(boundary, representation) {
  if (representation === "explicitTag") return true;
  if (representation === "provenNiche") {
    return !["foreignC", "wire", "persisted"].includes(boundary);
  }
  if (representation === "lowBit") return boundary === "internal";
  if (representation === "nativeCarrier") {
    return ["foreignC", "capability"].includes(boundary);
  }
  return false;
}

function applyOperation(state, operation) {
  switch (operation.op) {
    case "initialize": {
      if (state.bindings[operation.binding]) {
        throw new HirMemoryError("bindingAlreadyInitialized");
      }
      const payload = `p${state.nextPayload}`;
      state.nextPayload += 1;
      state.payloads[payload] = {
        address: operation.address ?? "unstable",
        allocatorKnown: operation.allocatorKnown ?? false,
      };
      state.bindings[operation.binding] = {
        state: "owned",
        payload,
        pinnedHandle: false,
      };
      return;
    }
    case "use": {
      requireBinding(state, operation.binding);
      return;
    }
    case "move": {
      const source = requireBinding(state, operation.from);
      requireNoBorrow(state, source.payload, "moveWithActiveBorrow");
      if (state.bindings[operation.to]) {
        throw new HirMemoryError("moveTargetAlreadyInitialized");
      }
      source.state = "moved";
      state.bindings[operation.to] = {
        state: "owned",
        payload: source.payload,
        pinnedHandle: source.pinnedHandle,
      };
      return;
    }
    case "drop": {
      const binding = requireBinding(state, operation.binding);
      requireNoBorrow(state, binding.payload, "dropWithActiveBorrow");
      binding.state = "dropped";
      return;
    }
    case "beginBorrow": {
      const binding = requireBinding(state, operation.binding);
      if (state.borrows[operation.token]) {
        throw new HirMemoryError("borrowTokenAlreadyActive");
      }
      const active = activeBorrowsForPayload(state, binding.payload);
      if (
        (operation.mode === "shared" && active.some((borrow) => borrow.mode === "exclusive")) ||
        (operation.mode === "exclusive" && active.length > 0)
      ) {
        throw new HirMemoryError("borrowConflict");
      }
      state.borrows[operation.token] = {
        mode: operation.mode,
        binding: operation.binding,
        payload: binding.payload,
      };
      return;
    }
    case "endBorrow": {
      if (!state.borrows[operation.token]) {
        throw new HirMemoryError("invalidBorrowEnd");
      }
      delete state.borrows[operation.token];
      return;
    }
    case "pin": {
      const source = requireBinding(state, operation.from);
      requireNoBorrow(state, source.payload, "pinWithActiveBorrow");
      if (state.bindings[operation.to]) {
        throw new HirMemoryError("pinTargetAlreadyInitialized");
      }
      source.state = "moved";
      state.payloads[source.payload].address = "stable";
      state.bindings[operation.to] = {
        state: "owned",
        payload: source.payload,
        pinnedHandle: true,
      };
      return;
    }
    case "publishAddress": {
      const binding = requireBinding(state, operation.binding);
      if (!binding.pinnedHandle || state.payloads[binding.payload].address === "unstable") {
        throw new HirMemoryError("publishRequiresPinnedStorage");
      }
      state.payloads[binding.payload].address = "published";
      return;
    }
    case "suspend": {
      for (const borrow of Object.values(state.borrows)) {
        const payload = state.payloads[borrow.payload];
        if (payload.address === "unstable") {
          throw new HirMemoryError("suspendWithUnstableBorrow");
        }
      }
      return;
    }
    case "verifyBoundary": {
      if (!acceptsRepresentation(operation.boundary, operation.representation)) {
        throw new HirMemoryError("nonCanonicalBoundaryRepresentation");
      }
      if (operation.binding) {
        const binding = requireBinding(state, operation.binding);
        if (!state.payloads[binding.payload].allocatorKnown) {
          throw new HirMemoryError("missingAllocatorOrigin");
        }
      }
      return;
    }
    case "verifyAbi": {
      const fields = ["target", "callingConvention", "representationPolicy", "runtimeAbi"];
      if (fields.some((field) => operation.consumer[field] !== operation.provider[field])) {
        throw new HirMemoryError("abiMismatch");
      }
      return;
    }
    case "joinOwnerStates": {
      if (
        operation.states.length === 0 ||
        operation.states.some((ownerState) => ownerState !== operation.states[0])
      ) {
        throw new HirMemoryError("ownerStateMismatchAtJoin");
      }
      return;
    }
    default:
      throw new HirMemoryError("unknownMemoryOperation");
  }
}

export function validateMemoryOperation(operation) {
  if (!operation || typeof operation !== "object" || typeof operation.op !== "string") {
    return false;
  }
  const hasString = (field) =>
    typeof operation[field] === "string" && operation[field].length > 0;

  switch (operation.op) {
    case "initialize":
      return (
        hasString("binding") &&
        (!operation.address || ADDRESS_STATES.has(operation.address)) &&
        (operation.allocatorKnown === undefined ||
          typeof operation.allocatorKnown === "boolean")
      );
    case "use":
    case "drop":
    case "publishAddress":
      return hasString("binding");
    case "move":
    case "pin":
      return hasString("from") && hasString("to") && operation.from !== operation.to;
    case "beginBorrow":
      return (
        hasString("binding") &&
        hasString("token") &&
        ["shared", "exclusive"].includes(operation.mode)
      );
    case "endBorrow":
      return hasString("token");
    case "suspend":
      return true;
    case "verifyBoundary":
      return (
        BOUNDARIES.has(operation.boundary) &&
        REPRESENTATIONS.has(operation.representation) &&
        (operation.binding === undefined || hasString("binding"))
      );
    case "verifyAbi": {
      const fields = ["target", "callingConvention", "representationPolicy", "runtimeAbi"];
      return [operation.consumer, operation.provider].every(
        (key) =>
          key &&
          typeof key === "object" &&
          fields.every((field) => typeof key[field] === "string" && key[field].length > 0),
      );
    }
    case "joinOwnerStates":
      return (
        Array.isArray(operation.states) &&
        operation.states.every((ownerState) => OWNER_STATES.has(ownerState))
      );
    default:
      return false;
  }
}

export function runMemoryProgram(operations) {
  const state = {
    bindings: {},
    payloads: {},
    borrows: {},
    nextPayload: 0,
  };
  const trace = [];

  for (const [index, operation] of operations.entries()) {
    const before = clone(state);
    try {
      applyOperation(state, operation);
      trace.push({ index, operation: clone(operation), before, after: clone(state) });
    } catch (error) {
      if (!(error instanceof HirMemoryError)) throw error;
      trace.push({
        index,
        operation: clone(operation),
        before,
        rejected: error.code,
      });
      return { status: "rejected", code: error.code, operation: index, state, trace };
    }
  }

  return { status: "accepted", state, trace };
}
