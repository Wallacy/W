import assert from "node:assert/strict";
import test from "node:test";
import {
  chooseAbiArtifact,
  comparePhysicalCalls,
  linkWExact,
  validateAbiNote,
  validateBoundaryValue,
  validateCFacade,
  validateCRecord,
  validateForeignRecovery,
  validateHeaderPair,
} from "./layout-abi-machine.mjs";

function wAbi(overrides = {}) {
  return {
    abiRevision: "w-abi-1",
    producer: "w-abi-producer-1",
    target: "x86_64-unknown-linux-gnu",
    dataLayout: "e-p:64:64-i64:64-n8:16:32:64-S128",
    callingConventionRevision: "w-call-1",
    representationPolicyRevision: "portable-1",
    runtimeAbiRevisions: { drop: "1", witness: "1" },
    panicModel: "abort-process",
    cleanupModel: "w-cleanup-1",
    hardeningAbiFacts: [],
    ...overrides,
  };
}

function note({
  semanticInterfaceKey,
  abi = wAbi(),
  recipeDigest,
  representations = [],
  symbols = [],
  imports = [],
  requiredFeatures = [
    "exact-link-v1",
    "representation-map-v1",
    "semantic-import-key-v1",
  ],
} = {}) {
  return {
    schema: "w-abi-logical-note-l0-v1",
    requiredFeatures,
    wAbi: abi,
    semanticInterfaceKey,
    recipeDigest,
    representations,
    symbols,
    imports,
  };
}

const statusRepresentation = {
  id: "restaurant.horizon.HorizonStatus",
  fingerprint: "rep:horizon-status-v1",
  fields: [],
};

function exactPair() {
  const provider = note({
    semanticInterfaceKey: "if:horizon-provider-v1",
    recipeDigest: "recipe:provider",
    representations: [statusRepresentation],
    symbols: [
      {
        name: "restaurant.horizon::classify",
        boundary: "wExact",
        semanticSignature: "semantic:classify-v1",
        physicalSignature: "physical:classify-sret-v1",
        representations: [statusRepresentation.id],
      },
    ],
  });
  const consumer = note({
    semanticInterfaceKey: "if:observatory-consumer-v7",
    recipeDigest: "recipe:consumer",
    imports: [
      {
        id: "horizon.classify",
        symbol: "restaurant.horizon::classify",
        providerInterfaceKey: provider.semanticInterfaceKey,
        semanticSignature: "semantic:classify-v1",
        physicalSignature: "physical:classify-sret-v1",
        representations: {
          [statusRepresentation.id]: statusRepresentation.fingerprint,
        },
      },
    ],
  });
  return { consumer, provider };
}

test("W exact compares the imported interface, not the consumer's own interface", () => {
  const { consumer, provider } = exactPair();
  assert.notEqual(consumer.semanticInterfaceKey, provider.semanticInterfaceKey);
  assert.equal(linkWExact(consumer, provider, "horizon.classify"), true);

  const wrongProvider = structuredClone(provider);
  wrongProvider.semanticInterfaceKey = "if:horizon-provider-v2";
  assert.throws(() => linkWExact(consumer, wrongProvider, "horizon.classify"), {
    code: "providerInterfaceMismatch",
  });
});

test("only representations reachable through the imported symbol constrain the link", () => {
  const { consumer, provider } = exactPair();
  const privateDifference = structuredClone(provider);
  privateDifference.representations.push({
    id: "restaurant.horizon.PrivateCache",
    fingerprint: "rep:private-v9",
    fields: [],
  });
  assert.equal(linkWExact(consumer, privateDifference, "horizon.classify"), true);

  const sharedDifference = structuredClone(provider);
  sharedDifference.representations[0].fingerprint = "rep:horizon-status-v2";
  assert.throws(() => linkWExact(consumer, sharedDifference, "horizon.classify"), {
    code: "representationMismatch",
  });
});

test("recipe and runtime requirements stay outside the exact ABI descriptor", () => {
  const { consumer, provider } = exactPair();
  consumer.runtimeRequirements = ["allocator.system@1"];
  provider.runtimeRequirements = ["allocator.mimalloc@2"];
  assert.notEqual(consumer.recipeDigest, provider.recipeDigest);
  assert.equal(linkWExact(consumer, provider, "horizon.classify"), true);

  provider.wAbi.hardeningAbiFacts = ["shadow-call-stack-v1"];
  assert.throws(() => linkWExact(consumer, provider, "horizon.classify"), {
    code: "abiKeyMismatch",
  });
});

test("physical call compatibility includes ABI attributes, not only byte size", () => {
  const direct = {
    callingConvention: "w-v1",
    result: { mode: "direct", class: "integer", bits: 32, extension: "zero" },
    parameters: [{ mode: "direct", class: "float", bits: 32 }],
    hidden: [],
  };
  assert.equal(comparePhysicalCalls(direct, structuredClone(direct)), true);

  const signed = structuredClone(direct);
  signed.result.extension = "sign";
  assert.throws(() => comparePhysicalCalls(direct, signed), {
    code: "physicalCallMismatch",
  });

  const integerParameter = structuredClone(direct);
  integerParameter.parameters[0].class = "integer";
  assert.throws(() => comparePhysicalCalls(direct, integerParameter), {
    code: "physicalCallMismatch",
  });
});

test("artifact recovery follows exact, rebuild, declared boundary, then failure", () => {
  assert.equal(
    chooseAbiArtifact({
      exactArtifact: true,
      sourceAvailable: true,
      declaredBoundary: true,
      boundaryArtifact: true,
    }),
    "exactArtifact",
  );
  assert.equal(
    chooseAbiArtifact({
      exactArtifact: false,
      sourceAvailable: true,
      declaredBoundary: true,
      boundaryArtifact: true,
    }),
    "rebuildSource",
  );
  assert.equal(
    chooseAbiArtifact({
      exactArtifact: false,
      sourceAvailable: false,
      declaredBoundary: true,
      boundaryArtifact: true,
    }),
    "canonicalBoundary",
  );
  assert.throws(
    () =>
      chooseAbiArtifact({
        exactArtifact: false,
        sourceAvailable: false,
        declaredBoundary: false,
        boundaryArtifact: true,
      }),
    { code: "implicitBoundaryFallback" },
  );
});

test("C façades make borrow, owner, callback, panic, and runtime contracts explicit", () => {
  const facade = {
    unsafe: true,
    abi: "c",
    generic: false,
    capture: false,
    async: false,
    throws: false,
    panic: "forbid",
    panicFree: true,
    runtime: "none",
    hiddenRuntimeContext: false,
    parameters: [
      {
        kind: "pointer",
        pointee: "c.uchar",
        ownership: "borrowed",
        retention: "call",
        sequence: true,
        extentParameter: "size",
      },
      { kind: "scalar", type: "c.size" },
    ],
    result: { kind: "scalar", type: "c.uint" },
  };
  assert.equal(validateCFacade(facade), true);

  const escaping = structuredClone(facade);
  escaping.parameters[0].retention = "persistent";
  assert.throws(() => validateCFacade(escaping), {
    code: "borrowedCarrierMayEscape",
  });

  const owned = structuredClone(facade);
  owned.parameters = [];
  owned.result = {
    kind: "pointer",
    pointee: "ll_snapshot",
    ownership: "owned",
    callerFree: false,
    destroySymbol: "ll_snapshot_destroy_v1",
  };
  assert.equal(validateCFacade(owned), true);
  owned.result.destroySymbol = "";
  assert.throws(() => validateCFacade(owned), { code: "ownedCarrierNeedsDestroy" });

  owned.result.dropCallback = true;
  owned.result.dropContext = true;
  assert.equal(validateCFacade(owned), true);
  owned.result.dropContext = false;
  assert.throws(() => validateCFacade(owned), { code: "ownedCarrierNeedsDestroy" });

  const borrowedResult = structuredClone(facade);
  borrowedResult.parameters = [];
  borrowedResult.result = {
    kind: "pointer",
    pointee: "c.uchar",
    ownership: "borrowed",
    retention: "call",
  };
  assert.throws(() => validateCFacade(borrowedResult), {
    code: "borrowedCarrierMayEscape",
  });

  const hiddenContext = structuredClone(facade);
  hiddenContext.parameters = [
    { kind: "pointer", pointee: "ll_context", role: "runtimeContext" },
  ];
  assert.throws(() => validateCFacade(hiddenContext), {
    code: "cFacadeRuntimeContextInvalid",
  });
});

test("foreign layout can describe unaligned storage but cannot create a W borrow", () => {
  const packedHeader = {
    kind: "struct",
    origin: "headerImport",
    target: "x86_64-unknown-linux-gnu",
    layoutDigest: "layout:packed-sample-v1",
    size: 5,
    alignment: 1,
    fields: [
      { name: "tag", offset: 0, size: 1, alignment: 1, borrowable: true },
      { name: "value", offset: 1, size: 4, alignment: 4, borrowable: false },
    ],
  };
  assert.equal(validateCRecord(packedHeader), true);

  const borrowed = structuredClone(packedHeader);
  borrowed.fields[1].borrowable = true;
  assert.throws(() => validateCRecord(borrowed), {
    code: "unalignedBorrowForbidden",
  });

  const commonW = structuredClone(packedHeader);
  commonW.origin = "wCommon";
  assert.throws(() => validateCRecord(commonW), {
    code: "cRecordNeedsTargetLayout",
  });

  const generatedUnaligned = structuredClone(packedHeader);
  generatedUnaligned.origin = "generatedFacade";
  assert.throws(() => validateCRecord(generatedUnaligned), {
    code: "generatedCRecordMustBeNatural",
  });

  const invalidAlignment = structuredClone(packedHeader);
  invalidAlignment.fields[1].alignment = 0;
  assert.throws(() => validateCRecord(invalidAlignment), {
    code: "malformedCRecord",
  });
});

test("C integers and pointers recover safe W values only after their own proof", () => {
  assert.equal(
    validateForeignRecovery({
      source: "cInteger",
      target: "closedEnum",
      validated: true,
    }),
    true,
  );
  assert.throws(
    () =>
      validateForeignRecovery({
        source: "cInteger",
        target: "closedEnum",
        validated: false,
      }),
    { code: "foreignValueNeedsValidation" },
  );
  const safeBorrow = {
    source: "cPointer",
    target: "safeBorrow",
    ownerProof: true,
    lifetimeProof: true,
    boundsProof: true,
    alignmentProof: true,
    noEscape: true,
  };
  assert.equal(validateForeignRecovery(safeBorrow), true);
  safeBorrow.alignmentProof = false;
  assert.throws(() => validateForeignRecovery(safeBorrow), {
    code: "foreignBorrowNeedsProof",
  });
  assert.equal(
    validateBoundaryValue({
      boundary: "wire",
      schemaDigest: "wire:horizon-v1",
      containsPointer: false,
    }),
    true,
  );
  assert.throws(
    () =>
      validateBoundaryValue({
        boundary: "wire",
        schemaDigest: "wire:horizon-v1",
        containsPointer: true,
      }),
    { code: "schemaBoundaryRequired" },
  );
});

test("the ABI note reader rejects unknown requirements and bounded inputs", () => {
  const { consumer } = exactPair();
  assert.equal(validateAbiNote(consumer), true);

  const unknown = structuredClone(consumer);
  unknown.requiredFeatures.push("future-layout-magic-v9");
  assert.throws(() => validateAbiNote(unknown), {
    code: "unknownRequiredAbiFeature",
  });
  const duplicate = structuredClone(consumer);
  duplicate.requiredFeatures.push("exact-link-v1");
  assert.throws(() => validateAbiNote(duplicate), {
    code: "duplicateRequiredAbiFeature",
  });
  const malformedImport = structuredClone(consumer);
  delete malformedImport.imports[0].physicalSignature;
  assert.throws(() => validateAbiNote(malformedImport), {
    code: "malformedAbiNote",
  });
  assert.throws(() => validateAbiNote(consumer, { noteBytes: 32 }), {
    code: "abiReaderLimitExceeded",
  });
});

test("a C header can pair only with its indexed target slice", () => {
  const pair = {
    headerTarget: "x86_64-unknown-linux-gnu",
    libraryTarget: "x86_64-unknown-linux-gnu",
    headerDigest: "header:horizon-v1",
    indexedHeaderDigest: "header:horizon-v1",
    libraryDigest: "library:horizon-v1",
    indexedLibraryDigest: "library:horizon-v1",
    headerHasTimestamp: false,
    headerHasLocalPath: false,
  };
  assert.equal(validateHeaderPair(pair), true);
  pair.headerTarget = "";
  pair.libraryTarget = "";
  assert.throws(() => validateHeaderPair(pair), { code: "headerSliceMismatch" });
});
