import fs from "node:fs";
import path from "node:path";
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
    const providerProfile = structuredClone(manifest);
    providerProfile.providerRefs[0].digest = "sha256:3333333333333333333333333333333333333333333333333333333333333333";
    expect(validateIpc1StudyManifest(providerProfile, { studyDirectory }).some((error) => error.includes("providerRefs[0].digest is stale"))).toBe(true);
  });
});
