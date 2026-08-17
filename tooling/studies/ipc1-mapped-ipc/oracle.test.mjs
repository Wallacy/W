import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { fileURLToPath } from "node:url";
import { describe, expect, test } from "bun:test";
import { evaluateIpc1Case, validateIpc1 } from "../../ipc1-mapped-ipc-machine.mjs";
import { validateIpc1StudyManifest } from "../../ipc1-mapped-ipc-manifest.mjs";

const studyDirectory = path.dirname(fileURLToPath(import.meta.url));
const toolingDirectory = path.resolve(studyDirectory, "../..");
const root = path.resolve(toolingDirectory, "..");
const corpus = JSON.parse(fs.readFileSync(path.join(toolingDirectory, "ipc1-mapped-ipc-cases.json"), "utf8"));
const manifest = JSON.parse(fs.readFileSync(path.join(studyDirectory, "study.json"), "utf8"));
const byId = new Map(corpus.cases.map((testCase) => [testCase.id, testCase]));
const oracleOptions = { providers: corpus.providers, layouts: corpus.layouts, schemas: corpus.schemas };

function fileDigest(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function probeFixture({ mutateReceipt, mutateOutput } = {}) {
  const fixtureDirectory = fs.mkdtempSync(path.join(studyDirectory, "probe-fixture-"));
  const probeDirectory = path.join(studyDirectory, "probes");
  const sourceFile = path.join(fixtureDirectory, "posix-two-process.c");
  const outputFile = path.join(fixtureDirectory, "posix-two-process.output.json");
  const receiptFile = path.join(fixtureDirectory, "posix-two-process.receipt.json");
  fs.copyFileSync(path.join(probeDirectory, "posix-two-process.c"), sourceFile);
  fs.copyFileSync(path.join(probeDirectory, "posix-two-process.output.json"), outputFile);
  const receipt = JSON.parse(fs.readFileSync(path.join(probeDirectory, "posix-two-process.receipt.json"), "utf8"));
  const output = JSON.parse(fs.readFileSync(outputFile, "utf8"));
  mutateOutput?.(output);
  fs.writeFileSync(outputFile, `${JSON.stringify(output)}\n`);
  receipt.source.digest = fileDigest(sourceFile);
  receipt.output.digest = fileDigest(outputFile);
  mutateReceipt?.(receipt);
  fs.writeFileSync(receiptFile, `${JSON.stringify(receipt, null, 2)}\n`);
  const probe = structuredClone(corpus.probeRefs[0]);
  probe.path = path.relative(root, receiptFile).replaceAll(path.sep, "/");
  probe.digest = fileDigest(receiptFile);
  const manifestProbe = structuredClone(manifest.probeRefs[0]);
  manifestProbe.path = path.relative(studyDirectory, receiptFile).replaceAll(path.sep, "/");
  manifestProbe.digest = fileDigest(receiptFile);
  return { fixtureDirectory, probe, manifestProbe };
}

function removeProbeFixture(fixtureDirectory) {
  fs.rmSync(fixtureDirectory, { recursive: true, force: true });
}

describe("IPC1 mapped memory and IPC host oracle", () => {
  test("validates the corpus and keeps two target projections", () => {
    const checked = validateIpc1(corpus, { root });
    expect(checked.errors).toEqual([]);
    expect(checked.results.length).toBe(corpus.cases.length);
    for (const item of checked.results) expect(item.targetProjections.map((projection) => projection.target).sort()).toEqual(["posix", "windows"]);
  });

  test("snapshot events enforce header/layout, release visibility, and receipt order", () => {
    expect(evaluateIpc1Case(byId.get("IPC1-live-horizon-address-independent"), oracleOptions).code).toBe("immutable-generation");
    expect(evaluateIpc1Case(byId.get("IPC1-live-horizon-address-independent"), oracleOptions).logical.unknownDurability).toBe(false);
    expect(evaluateIpc1Case(byId.get("IPC1-publish-before-durability-unknown"), oracleOptions).status).toBe("unknown");
    expect(evaluateIpc1Case(byId.get("IPC1-publish-before-durability-unknown"), oracleOptions).logical.visibility).toBe("published");
    expect(evaluateIpc1Case(byId.get("IPC1-publish-after-durability-receipt"), oracleOptions).code).toBe("published-after-receipt");
    for (const [id, code] of [["IPC1-reject-corrupt-offset", "layout-invalid"], ["IPC1-reject-corrupt-extent", "layout-bounds"], ["IPC1-reject-corrupt-overlap", "layout-overlap"], ["IPC1-reject-corrupt-overflow", "layout-bounds"], ["IPC1-publish-before-validate-reject", "view-before-validate"], ["IPC1-read-before-validate-reject", "read-before-validate"], ["IPC1-receipt-before-request-reject", "receipt-before-request"], ["IPC1-duplicate-publish-reject", "duplicate-selector-publish"], ["IPC1-generation-nonmonotonic-reject", "generation-not-monotonic"]]) expect(evaluateIpc1Case(byId.get(id), oracleOptions).code).toBe(code);
  });

  test("wire carriers derive cap0 rendezvous, capN backpressure, ownership, and faults", () => {
    expect(evaluateIpc1Case(byId.get("IPC1-channel-cap0-rendezvous"), oracleOptions).code).toBe("rendezvous-channel");
    expect(evaluateIpc1Case(byId.get("IPC1-channel-capn-bounded"), oracleOptions).code).toBe("bounded-mapped-channel");
    expect(evaluateIpc1Case(byId.get("IPC1-channel-full-backpressure"), oracleOptions).code).toBe("backpressure");
    expect(evaluateIpc1Case(byId.get("IPC1-channel-cancel-before-commit"), oracleOptions).logical.transferred).toBe(false);
    expect(evaluateIpc1Case(byId.get("IPC1-channel-cancel-after-commit"), oracleOptions).logical.transferred).toBe(true);
    expect(evaluateIpc1Case(byId.get("IPC1-channel-double-materialize-reject"), oracleOptions).code).toBe("double-materialize");
    const materializationOom = evaluateIpc1Case(byId.get("IPC1-channel-materialize-oom-preserves-slot"), oracleOptions);
    expect(materializationOom.code).toBe("materialization-oom");
    expect(materializationOom.logical.partialOwnerMutation).toBe(false);
    expect(materializationOom.logical.slotState).toBe("full");
    expect(materializationOom.logical.reservationState).toBe("reading");
    expect(materializationOom.logical.retryable).toBe(true);
    const panic = evaluateIpc1Case(byId.get("IPC1-channel-panic-fault-no-repair"), oracleOptions);
    expect(panic.status).toBe("faulted");
    expect(panic.code).toBe("generation-fault");
    expect(panic.logical.hiddenRepair).toBe(false);
    for (const id of ["IPC1-channel-crash-writing-fault", "IPC1-channel-crash-reading-fault"]) {
      const outcome = evaluateIpc1Case(byId.get(id), oracleOptions);
      expect(outcome.status).toBe("faulted");
      expect(outcome.code).toBe("generation-fault");
      expect(outcome.logical.hiddenRepair).toBe(false);
    }
    expect(evaluateIpc1Case(byId.get("IPC1-channel-committed-full-survives-producer-crash"), oracleOptions).code).toBe("committed-full-survives-producer-crash");
    const committed = evaluateIpc1Case(byId.get("IPC1-channel-committed-full-survives-producer-crash"), oracleOptions);
    expect(committed.logical.committedFullSurvived).toBe(true);
    expect(committed.logical.owner).toBe("receiver-new-owner");
    expect(evaluateIpc1Case(byId.get("IPC1-channel-unrelated-process-crash-no-fault"), oracleOptions).code).toBe("unrelated-process-crash-no-fault");
    expect(evaluateIpc1Case(byId.get("IPC1-channel-unrelated-process-crash-no-fault"), oracleOptions).logical.unrelatedProcessCrash).toBe(true);
    expect(evaluateIpc1Case(byId.get("IPC1-channel-empty-trace-reject"), oracleOptions).code).toBe("incomplete-channel-trace");
    expect(evaluateIpc1Case(byId.get("IPC1-channel-capacity-slots-overflow"), oracleOptions).code).toBe("channel-capacity-bounds");
    expect(evaluateIpc1Case(byId.get("IPC1-channel-crash-reading-reopen"), oracleOptions).code).toBe("generation-reopened");
  });

  test("provider authority, volatility, lifecycle, and fallback remain explicit", () => {
    expect(evaluateIpc1Case(byId.get("IPC1-provider-authoritative"), oracleOptions).code).toBe("provider-authoritative");
    expect(evaluateIpc1Case(byId.get("IPC1-provider-durable-requirement-reject"), oracleOptions).code).toBe("durability-unavailable");
    expect(evaluateIpc1Case(byId.get("IPC1-fallback-to-snapshot"), oracleOptions).logical.route).toBe("snapshot-wire-service");
    expect(evaluateIpc1Case(byId.get("IPC1-robust-owner-death-fault"), oracleOptions).code).toBe("generation-fault");
    expect(evaluateIpc1Case(byId.get("IPC1-robust-cooperative-reject"), oracleOptions).code).toBe("fallback-context-incompatible");
    expect(evaluateIpc1Case(byId.get("IPC1-immediate-name-withdrawal-unsupported"), oracleOptions).code).toBe("name-withdrawal-unsupported");
    expect(evaluateIpc1Case(byId.get("IPC1-shm-unlink-live-lease"), oracleOptions).code).toBe("name-discovery-target-specific");
    expect(evaluateIpc1Case(byId.get("IPC1-ffi-close-order"), oracleOptions).code).toBe("ffi-lease-close");
    expect(evaluateIpc1Case(byId.get("IPC1-unmap-active-callback-reject"), oracleOptions).code).toBe("unmap-with-active-lease");
    expect(evaluateIpc1Case(byId.get("IPC1-unmap-active-loan-reject"), oracleOptions).code).toBe("unmap-with-active-lease");
    expect(evaluateIpc1Case(byId.get("IPC1-lifecycle-empty-trace-reject"), oracleOptions).code).toBe("incomplete-lifecycle-trace");
    for (const [id, code] of [["IPC1-atomic-width-unsupported", "atomic-width-unsupported"], ["IPC1-atomic-order-unsupported", "atomic-order-unsupported"], ["IPC1-atomic-alignment-unsupported", "atomic-alignment-unsupported"], ["IPC1-atomic-progress-unsupported", "atomic-progress-unsupported"]]) expect(evaluateIpc1Case(byId.get(id), oracleOptions).code).toBe(code);
  });

  test("mutations cannot forge source, provider, expected, or legacy facts", () => {
    const stale = structuredClone(corpus);
    stale.sourceRefs[0].digest = "sha256:stale";
    expect(validateIpc1(stale, { root }).errors.some((error) => error.includes("sourceRefs[0].digest is stale"))).toBe(true);
    const missing = structuredClone(corpus);
    missing.sourceRefs[0].path = "reference/last-light/missing.w";
    expect(validateIpc1(missing, { root }).errors.some((error) => error.includes("missing or out-of-tree"))).toBe(true);
    const duplicate = structuredClone(corpus);
    duplicate.sourceRefs.push(structuredClone(duplicate.sourceRefs[0]));
    expect(validateIpc1(duplicate, { root }).errors.some((error) => error.includes("duplicates source reference"))).toBe(true);
    const forged = structuredClone(corpus);
    forged.cases[1].input.providerFacts = { durable: true };
    expect(validateIpc1(forged, { root }).errors.some((error) => error.includes("legacy oracle field"))).toBe(true);
    const expectedMutation = structuredClone(byId.get("IPC1-live-horizon-address-independent"));
    expectedMutation.expected.code = "forged-result";
    expect(evaluateIpc1Case(expectedMutation, oracleOptions).code).toBe("immutable-generation");
    const divergentProviders = structuredClone(corpus.providers);
    divergentProviders["windows-file-durable"].allowedSchemas[1].digest = "sha256:3333333333333333333333333333333333333333333333333333333333333333";
    expect(evaluateIpc1Case(byId.get("IPC1-live-horizon-address-independent"), { providers: divergentProviders, layouts: corpus.layouts, schemas: corpus.schemas }).code).toBe("target-divergence");
    const duplicateProviderId = structuredClone(corpus);
    duplicateProviderId.providers["windows-file-durable"].objectIdentity = duplicateProviderId.providers["posix-file-durable"].objectIdentity;
    expect(validateIpc1(duplicateProviderId, { root }).errors.some((error) => error.includes("objectIdentity is duplicated"))).toBe(true);
    const providerAlias = structuredClone(corpus);
    providerAlias.providers["posix-file-durable"].providerId = "caller-alias";
    expect(validateIpc1(providerAlias, { root }).errors.some((error) => error.includes("providerId is forbidden"))).toBe(true);
    const duplicateLayoutId = structuredClone(corpus);
    duplicateLayoutId.providers["posix-shm-volatile"].allowedLayouts.push(structuredClone(duplicateLayoutId.providers["posix-shm-volatile"].allowedLayouts[0]));
    expect(validateIpc1(duplicateLayoutId, { root }).errors.some((error) => error.includes("allowedLayouts contains duplicate layout ID"))).toBe(true);
    const duplicateSchemaId = structuredClone(corpus);
    duplicateSchemaId.providers["posix-shm-volatile"].allowedSchemas.push(structuredClone(duplicateSchemaId.providers["posix-shm-volatile"].allowedSchemas[0]));
    expect(validateIpc1(duplicateSchemaId, { root }).errors.some((error) => error.includes("allowedSchemas contains duplicate schema ID"))).toBe(true);
    const sameProcessWake = structuredClone(corpus);
    sameProcessWake.providers["windows-pagefile-volatile"].atomic.waitWake = "WaitOnAddress";
    expect(validateIpc1(sameProcessWake, { root }).errors.some((error) => error.includes("same-process WaitOnAddress"))).toBe(true);
    const invalidWakeProvider = structuredClone(corpus);
    invalidWakeProvider.providers["windows-pagefile-volatile"].wakeProvider.kind = "same-process-address";
    expect(validateIpc1(invalidWakeProvider, { root }).errors.some((error) => error.includes("wakeProvider.kind is invalid or same-process"))).toBe(true);
    const swappedWakeKind = structuredClone(corpus);
    swappedWakeKind.providers["windows-robust-blocking"].wakeProvider.kind = "named-semaphore";
    expect(validateIpc1(swappedWakeKind, { root }).errors.some((error) => error.includes("named-semaphore lifecycle must exactly equal"))).toBe(true);
    const extraNamedLifecycle = structuredClone(corpus);
    extraNamedLifecycle.providers["windows-robust-blocking"].wakeProvider.handleLifecycle.push("ReleaseSemaphore");
    expect(validateIpc1(extraNamedLifecycle, { root }).errors.some((error) => error.includes("named-event lifecycle must exactly equal"))).toBe(true);
    const extraPosixLifecycle = structuredClone(corpus);
    extraPosixLifecycle.providers["posix-robust-blocking"].wakeProvider.handleLifecycle.push("pthread_cond_wait");
    expect(validateIpc1(extraPosixLifecycle, { root }).errors.some((error) => error.includes("posix robust mutex lifecycle must exactly equal"))).toBe(true);
    const eventOwnerDeath = structuredClone(corpus);
    eventOwnerDeath.providers["windows-robust-blocking"].wakeProvider.ownerDeath = "typed-fault";
    expect(validateIpc1(eventOwnerDeath, { root }).errors.some((error) => error.includes("named kernel objects must not claim owner death"))).toBe(true);
    const semaphoreOwnerDeath = structuredClone(corpus);
    semaphoreOwnerDeath.providers["windows-robust-blocking"].wakeProvider.kind = "named-semaphore";
    semaphoreOwnerDeath.providers["windows-robust-blocking"].wakeProvider.ownerDeath = "typed-fault";
    semaphoreOwnerDeath.providers["windows-robust-blocking"].wakeProvider.handleLifecycle = ["CreateSemaphore", "OpenSemaphore", "Wait", "ReleaseSemaphore", "CloseHandle"];
    expect(validateIpc1(semaphoreOwnerDeath, { root }).errors.some((error) => error.includes("named kernel objects must not claim owner death"))).toBe(true);
    const atom2Receipt = structuredClone(corpus);
    atom2Receipt.providers["posix-shm-volatile"].atomic.addressFree = false;
    expect(validateIpc1(atom2Receipt, { root }).errors.some((error) => error.includes("addressFree must be a provider receipt"))).toBe(true);
    const lockFreeReceipt = structuredClone(corpus);
    lockFreeReceipt.providers["windows-pagefile-volatile"].atomic.lockFreeReceipt = "inferred-from-Atomic";
    expect(validateIpc1(lockFreeReceipt, { root }).errors.some((error) => error.includes("exact target fact receipt"))).toBe(true);
  });

  test("layout/schema/provider digests and ordered recovery are authoritative", () => {
    const drift = structuredClone(corpus);
    drift.layouts["immutable-menu"].segments[1].length -= 1;
    expect(validateIpc1(drift, { root }).errors.some((error) => error.includes("layoutDigest does not match canonical descriptor"))).toBe(true);
    const providerDrift = structuredClone(corpus);
    providerDrift.providers["posix-shm-volatile"].allowedLayouts[0].digest = "sha256:3333333333333333333333333333333333333333333333333333333333333333";
    expect(validateIpc1(providerDrift, { root }).errors.some((error) => error.includes("allowedLayouts contains an unknown or stale"))).toBe(true);
    const reuse = structuredClone(byId.get("IPC1-menu-immutable-publish"));
    reuse.operations = [
      { op: "map", process: "writer", generation: 8, objectId: "menu.v1-g8" },
      { op: "validate", process: "writer", generation: 8, objectId: "menu.v1-g8" },
      { op: "stage", generation: 9, objectId: "menu.v1-g8" },
    ];
    expect(evaluateIpc1Case(reuse, oracleOptions).code).toBe("generation-reuse-with-live-lease");
    const order = structuredClone(byId.get("IPC1-menu-immutable-publish"));
    order.operations = order.operations.filter((operation) => operation.op !== "flush-data");
    expect(evaluateIpc1Case(order, oracleOptions).code).toBe("publish-before-generation-flush");
    const scope = structuredClone(byId.get("IPC1-menu-immutable-publish"));
    scope.operations.find((operation) => operation.op === "request-durability").scope = "data";
    expect(evaluateIpc1Case(scope, oracleOptions).code).toBe("durability-scope-forbidden");
    const forgedSelector = structuredClone(byId.get("IPC1-menu-immutable-publish"));
    forgedSelector.operations.find((operation) => operation.op === "publish-selector").objectId = "menu.v1-forged";
    expect(evaluateIpc1Case(forgedSelector, oracleOptions).code).toBe("selector-object-mismatch");
    const forgedObservation = structuredClone(byId.get("IPC1-stale-generation-remap"));
    forgedObservation.operations.find((operation) => operation.op === "observe-generation").objectId = "telemetry.v1-forged";
    expect(evaluateIpc1Case(forgedObservation, oracleOptions).code).toBe("generation-observation-mismatch");
    const wrongMapGeneration = structuredClone(byId.get("IPC1-channel-capn-bounded"));
    wrongMapGeneration.operations.find((operation) => operation.op === "map").generation = 99;
    expect(evaluateIpc1Case(wrongMapGeneration, oracleOptions).code).toBe("channel-generation-mismatch");
    const recovery = structuredClone(byId.get("IPC1-channel-crash-reading-reopen"));
    recovery.operations = recovery.operations.filter((operation) => operation.op !== "drop-view");
    expect(evaluateIpc1Case(recovery, oracleOptions).code).toBe("unmap-active-view");
    const recoveryOrder = structuredClone(byId.get("IPC1-channel-crash-reading-reopen"));
    const stopIndex = recoveryOrder.operations.findIndex((operation) => operation.op === "stop-access");
    const dropIndex = recoveryOrder.operations.findIndex((operation) => operation.op === "drop-view");
    [recoveryOrder.operations[stopIndex], recoveryOrder.operations[dropIndex]] = [recoveryOrder.operations[dropIndex], recoveryOrder.operations[stopIndex]];
    expect(evaluateIpc1Case(recoveryOrder, oracleOptions).code).toBe("drop-before-drain");
    const ffiOrder = structuredClone(byId.get("IPC1-ffi-close-order"));
    const unregisterIndex = ffiOrder.operations.findIndex((operation) => operation.op === "unregister-callback");
    const drainIndex = ffiOrder.operations.findIndex((operation) => operation.op === "drain");
    [ffiOrder.operations[unregisterIndex], ffiOrder.operations[drainIndex]] = [ffiOrder.operations[drainIndex], ffiOrder.operations[unregisterIndex]];
    expect(evaluateIpc1Case(ffiOrder, oracleOptions).code).toBe("drain-before-unregister");
    const singularProvider = structuredClone(corpus);
    singularProvider.providers["posix-file-durable"].layoutDigest = "sha256:3333333333333333333333333333333333333333333333333333333333333333";
    expect(validateIpc1(singularProvider, { root }).errors.some((error) => error.includes("singular provider digests"))).toBe(true);
    const legacy = structuredClone(corpus);
    legacy.cases[1].operations[0].point = "before-selector";
    legacy.cases[1].operations[0].race = true;
    legacy.cases.find((testCase) => testCase.axis === "channel").input.capacity = 4;
    legacy.cases.find((testCase) => testCase.axis === "channel").operations[0].slot = "s0";
    expect(validateIpc1(legacy, { root }).errors.filter((error) => error.includes("legacy oracle field")).length).toBeGreaterThanOrEqual(2);
  });

  test("study manifest guards digests, refs, reserved variants, and readiness claims", () => {
    expect(validateIpc1StudyManifest(manifest, { studyDirectory })).toEqual([]);
    const stale = structuredClone(manifest);
    stale.sourceRefs[0].digest = "sha256:stale";
    expect(validateIpc1StudyManifest(stale, { studyDirectory }).some((error) => error.includes("sourceRefs[0].digest is stale"))).toBe(true);
    const missing = structuredClone(manifest);
    missing.sourceRefs[0].path = "../../../reference/last-light/missing.w";
    expect(validateIpc1StudyManifest(missing, { studyDirectory }).some((error) => error.includes("sourceRefs[0] is missing"))).toBe(true);
    const duplicate = structuredClone(manifest);
    duplicate.sourceRefs.push(structuredClone(duplicate.sourceRefs[0]));
    expect(validateIpc1StudyManifest(duplicate, { studyDirectory }).some((error) => error.includes("sourceRefs") && error.includes("duplicated"))).toBe(true);
    const reservedExtension = structuredClone(manifest);
    reservedExtension.variants[2].path = "current-snapshot.w";
    expect(validateIpc1StudyManifest(reservedExtension, { studyDirectory }).some((error) => error.includes("reserved variant must not use .w"))).toBe(true);
    const forgedFacts = structuredClone(manifest);
    forgedFacts.evidence.current.push("provider-ready");
    expect(validateIpc1StudyManifest(forgedFacts, { studyDirectory }).some((error) => error.includes("provider readiness"))).toBe(true);
    const providerReadyClaim = structuredClone(manifest);
    providerReadyClaim.providerRefs[0].claim = "Authoritative provider-ready receipt.";
    expect(validateIpc1StudyManifest(providerReadyClaim, { studyDirectory }).some((error) => error.includes("design fixture, not a provider receipt"))).toBe(true);
    const windowsWithoutReceipt = structuredClone(manifest);
    windowsWithoutReceipt.evidence.current.push("two-process-windows-probe");
    windowsWithoutReceipt.evidence.missing = windowsWithoutReceipt.evidence.missing.filter((item) => item !== "two-process-windows-probe");
    expect(validateIpc1StudyManifest(windowsWithoutReceipt, { studyDirectory }).some((error) => error.includes("two-process-windows-probe must derive from an observed Windows receipt"))).toBe(true);
    const posixNotCurrent = structuredClone(manifest);
    posixNotCurrent.evidence.current = posixNotCurrent.evidence.current.filter((item) => item !== "two-process-posix-probe");
    posixNotCurrent.evidence.missing.push("two-process-posix-probe");
    expect(validateIpc1StudyManifest(posixNotCurrent, { studyDirectory }).some((error) => error.includes("two-process-posix-probe must derive from an observed POSIX receipt"))).toBe(true);
    const providerProfile = structuredClone(manifest);
    providerProfile.providerRefs[0].digest = "sha256:3333333333333333333333333333333333333333333333333333333333333333";
    expect(validateIpc1StudyManifest(providerProfile, { studyDirectory }).some((error) => error.includes("providerRefs[0].digest is stale"))).toBe(true);
  });

  test("probe receipts bind source/output bytes, identity, observed facts, and negative claims", () => {
    const mutations = [
      {
        name: "stale source digest",
        mutateReceipt: (receipt) => { receipt.source.digest = "sha256:3333333333333333333333333333333333333333333333333333333333333333"; },
        expected: "source.digest is stale",
      },
      {
        name: "forged observed",
        mutateReceipt: (receipt) => { receipt.observed.twoProcess = false; },
        mutateOutput: (output) => { output.observed.twoProcess = false; },
        expected: "observed facts must equal the canonical observed result",
      },
      {
        name: "transcript identity mismatch",
        mutateOutput: (output) => { output.id = "IPC2-forged"; },
        expected: "output transcript id does not match probe identity",
      },
      {
        name: "provider receipt claim",
        mutateReceipt: (receipt) => { receipt.providerReceipt = true; },
        expected: "providerReceipt must be false",
      },
      {
        name: "missing notProven",
        mutateReceipt: (receipt) => { receipt.notProven = receipt.notProven.filter((entry) => entry !== "durability"); },
        expected: "notProven must equal the canonical ordered set",
      },
      {
        name: "extra notProven",
        mutateReceipt: (receipt) => { receipt.notProven.push("unverified-extra"); },
        expected: "notProven must equal the canonical ordered set",
      },
      {
        name: "duplicate notProven",
        mutateReceipt: (receipt) => { receipt.notProven.push("durability"); },
        expected: "notProven must equal the canonical ordered set",
      },
      {
        name: "top-level extra",
        mutateReceipt: (receipt) => { receipt.unexpected = true; },
        expected: "receipt top-level keys are invalid",
      },
      {
        name: "host extra",
        mutateReceipt: (receipt) => { receipt.host.unexpected = true; },
        expected: "host keys are invalid",
      },
      {
        name: "host empty",
        mutateReceipt: (receipt) => { receipt.host.os = ""; },
        expected: "host.os must be a non-empty string",
      },
      {
        name: "readiness claim",
        mutateReceipt: (receipt) => { receipt.claimBoundary = "provider readiness is complete"; },
        expected: "claimBoundary must use the canonical negative boundary",
      },
      {
        name: "out of tree source path",
        mutateReceipt: (receipt) => { receipt.source.path = "../../../../outside.c"; },
        expected: "source.path is missing or out-of-tree",
      },
    ];
    for (const mutation of mutations) {
      const fixture = probeFixture(mutation);
      try {
        const corpusMutation = structuredClone(corpus);
        corpusMutation.probeRefs = [fixture.probe];
        expect(validateIpc1(corpusMutation, { root }).errors.some((error) => error.includes(mutation.expected)), mutation.name).toBe(true);
        const manifestMutation = structuredClone(manifest);
        manifestMutation.probeRefs = [fixture.manifestProbe];
        expect(validateIpc1StudyManifest(manifestMutation, { studyDirectory }).some((error) => error.includes(mutation.expected)), `${mutation.name} manifest`).toBe(true);
      } finally {
        removeProbeFixture(fixture.fixtureDirectory);
      }
    }
  });
});
