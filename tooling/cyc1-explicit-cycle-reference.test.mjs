import fs from "node:fs";
import path from "node:path";
import { describe, expect, test } from "bun:test";
import { fileURLToPath } from "node:url";
import { evaluateCyc1Case, validateCyc1 } from "./cyc1-explicit-cycle-machine.mjs";
import { digestFile, validateCyc1StudyManifest } from "./cyc1-explicit-cycle-manifest.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "cyc1-explicit-cycle-cases.json");
const studyDirectory = path.join(toolingDirectory, "studies", "cyc1-explicit-cycle-lifecycle");
const studyPath = path.join(studyDirectory, "study.json");

function readCorpus() {
  return JSON.parse(fs.readFileSync(corpusPath, "utf8"));
}

function readStudy() {
  return JSON.parse(fs.readFileSync(studyPath, "utf8"));
}

function byId(corpus, id) {
  return corpus.cases.find((testCase) => testCase.id === id);
}

describe("CYC1 explicit cycle lifecycle host oracle", () => {
  test("the corpus and study manifest validate without implementation claims", () => {
    const corpus = readCorpus();
    expect(validateCyc1(corpus, { root: repositoryRoot }).errors).toEqual([]);
    expect(validateCyc1StudyManifest(readStudy(), { studyDirectory, repositoryRoot })).toEqual([]);
    expect(corpus.cases.length).toBeGreaterThanOrEqual(40);
  });

  test("static SCC, explicit break, weak edge, dynamic residual, and live roots stay distinct", () => {
    const corpus = readCorpus();
    expect(evaluateCyc1Case(byId(corpus, "CYC1-NEG-strong-callback-scc")).code).toBe("W-OWNERSHIP-0014");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-POS-explicit-close-break")).status).toBe("clean");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-POS-observer-weak-capture")).status).toBe("clean");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-DYN-runtime-strong-cycle")).code).toBe("W-MEMORY-0001");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-POS-live-root-after-drain")).status).toBe("live-root");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-NEG-unrelated-root-does-not-hide-residual")).code).toBe("W-MEMORY-0001");
  });

  test("drain, callback, FFI, resources, cancellation, and fault boundaries are ordered", () => {
    const corpus = readCorpus();
    expect(evaluateCyc1Case(byId(corpus, "CYC1-NEG-census-before-drain")).code).toBe("W-MEMORY-AUDIT-BEFORE-DRAIN");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-POS-ffi-order")).status).toBe("clean");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-NEG-ffi-inflight")).code).toBe("drain-with-live-callback");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-POS-resource-async-finish")).status).toBe("clean");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-FAULT-panic-fault-boundary")).code).toBe("panic-fault-boundary");
  });

  test("hidden foreign edges remain unknown and adapter roots remain observable", () => {
    const corpus = readCorpus();
    expect(evaluateCyc1Case(byId(corpus, "CYC1-UNK-hidden-foreign-root")).status).toBe("unknown");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-UNK-hidden-foreign-edge")).status).toBe("unknown");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-POS-adapter-foreign-root")).status).toBe("live-root");
  });

  test("weak acquisition, no resurrection, self-weak phases, lock, ABA, and domains are explicit", () => {
    const corpus = readCorpus();
    const weak = evaluateCyc1Case(byId(corpus, "CYC1-POS-weak-read-linearization"));
    expect(weak.weakReads.map((read) => read.value)).toEqual(["some", "none"]);
    expect(weak.controlBlockFreed).toBe(true);
    expect(evaluateCyc1Case(byId(corpus, "CYC1-NEG-resurrection")).code).toBe("weak-resurrection-forbidden");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-POS-self-weak-two-phase")).status).toBe("clean");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-RESEARCH-self-weak-constructor")).code).toBe("self-weak-before-publication");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-POS-concurrent-unlink-under-lock")).status).toBe("clean");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-NEG-concurrent-unlink-without-lock")).code).toBe("mutation-without-lock");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-NEG-address-reuse-before-weak-zero")).code).toBe("address-reuse-before-weak-zero");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-POS-cross-domain-control-block")).status).toBe("clean");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-NEG-cross-domain-missing-facts")).code).toBe("cross-domain-facts-missing");
  });

  test("conditional-liveness alternatives stay separate from the explicit graph route", () => {
    const corpus = readCorpus();
    expect(evaluateCyc1Case(byId(corpus, "CYC1-POS-generation-id-cache")).status).toBe("composable-alternative");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-POS-owner-scoped-cache-lease")).status).toBe("composable-alternative");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-POS-detached-cache-value")).status).toBe("composable-alternative");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-RESEARCH-naive-weak-key")).status).toBe("future-reopen-candidate");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-RESEARCH-naive-weak-key")).code).toBe("ordinary-weak-insufficient");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-RESEARCH-ephemeron-value-key-cycle")).status).toBe("future-reopen-candidate");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-RESEARCH-ephemeron-value-key-cycle")).code).toBe("ephemeron-value-key-cycle");
    expect(evaluateCyc1Case(byId(corpus, "CYC1-REJECT-transparent-collector")).status).toBe("intentionally-rejected");
  });

  test("mutations cannot forge outcomes, refs, lifecycle order, or collector effects", () => {
    const corpus = readCorpus();
    const forged = structuredClone(corpus);
    forged.cases[0].expect.status = "residual-cycle";
    expect(validateCyc1(forged, { root: repositoryRoot }).errors.some((error) => error.includes("expect.status"))).toBe(true);

    const forgedDisposition = structuredClone(corpus);
    forgedDisposition.cases[0].expect.disposition = "accepted";
    expect(validateCyc1(forgedDisposition, { root: repositoryRoot }).errors.some((error) => error.includes("expect.disposition"))).toBe(true);

    const hiddenKnown = structuredClone(corpus);
    hiddenKnown.cases.find((testCase) => testCase.id === "CYC1-UNK-hidden-foreign-edge").graph.edges[0].authority = "foreign";
    hiddenKnown.cases.find((testCase) => testCase.id === "CYC1-UNK-hidden-foreign-edge").graph.edges[0].known = true;
    hiddenKnown.cases.find((testCase) => testCase.id === "CYC1-UNK-hidden-foreign-edge").expect.status = "clean";
    expect(validateCyc1(hiddenKnown, { root: repositoryRoot }).errors.some((error) => error.includes("foreign edge") || error.includes("expect.status"))).toBe(true);

    const sccMutation = structuredClone(corpus);
    sccMutation.cases.find((testCase) => testCase.id === "CYC1-NEG-strong-callback-scc").graph.edges[0].active = false;
    expect(validateCyc1(sccMutation, { root: repositoryRoot }).errors.some((error) => error.includes("CYC1-NEG-strong-callback-scc.expect"))).toBe(true);

    const callerFlag = structuredClone(corpus);
    callerFlag.cases[0].accepted = true;
    expect(validateCyc1(callerFlag, { root: repositoryRoot }).errors.some((error) => error.includes("caller outcome"))).toBe(true);

    const targetProviderFlag = structuredClone(corpus);
    targetProviderFlag.cases[0].target = "native";
    targetProviderFlag.cases[0].provider = "ready";
    expect(validateCyc1(targetProviderFlag, { root: repositoryRoot }).errors.filter((error) => error.includes("caller outcome")).length).toBeGreaterThanOrEqual(1);

    const collectorEffect = structuredClone(corpus);
    collectorEffect.cases[0].events.push({ op: "collect" });
    expect(validateCyc1(collectorEffect, { root: repositoryRoot }).errors.some((error) => error.includes("collector side effects"))).toBe(true);

    const invalidOrder = structuredClone(corpus);
    invalidOrder.cases.find((testCase) => testCase.id === "CYC1-POS-ffi-order").events.splice(3, 0, { op: "destroy", resource: "bell-lease" });
    expect(evaluateCyc1Case(byId(invalidOrder, "CYC1-POS-ffi-order")).code).toBe("destroy-before-ffi-drain");

    const censusRemoved = structuredClone(corpus);
    const residualWithoutCensus = censusRemoved.cases.find((testCase) => testCase.id === "CYC1-NEG-residual-after-drain");
    residualWithoutCensus.events = residualWithoutCensus.events.filter((event) => event.op !== "census");
    const notAudited = evaluateCyc1Case(residualWithoutCensus);
    expect(notAudited.status).toBe("not-audited");
    expect(notAudited.code).toBe("W-MEMORY-CENSUS-NOT-REQUESTED");
    expect(notAudited.code).not.toBe("W-MEMORY-0001");

    const repeatedCensus = structuredClone(corpus);
    const repeated = repeatedCensus.cases.find((testCase) => testCase.id === "CYC1-POS-menu-weak-parent");
    repeated.events.push({ op: "census" });
    expect(evaluateCyc1Case(repeated).code).toBe("census-repeated");

    const closeDrainEdge = structuredClone(corpus);
    const closeDrain = closeDrainEdge.cases.find((testCase) => testCase.id === "CYC1-POS-lifecycle-drain-break");
    const drainIndex = closeDrain.events.findIndex((event) => event.op === "drain");
    closeDrain.events[drainIndex] = { op: "close", owner: "service", edges: ["service-callback"] };
    expect(evaluateCyc1Case(closeDrain).code).toBe("close-not-authorized");

    const closeWrongOwner = structuredClone(corpus);
    const wrongOwner = closeWrongOwner.cases.find((testCase) => testCase.id === "CYC1-POS-explicit-close-break");
    wrongOwner.events.find((event) => event.op === "close").owner = "callback";
    expect(evaluateCyc1Case(wrongOwner).code).toBe("close-owner-mismatch");

    const closeMissingOwner = structuredClone(corpus);
    const missingOwner = closeMissingOwner.cases.find((testCase) => testCase.id === "CYC1-POS-explicit-close-break");
    delete missingOwner.events.find((event) => event.op === "close").owner;
    expect(evaluateCyc1Case(missingOwner).code).toBe("close-owner-invalid");

    const explicitEdgeMissingOwner = structuredClone(corpus);
    const missingEdgeOwner = explicitEdgeMissingOwner.cases.find((testCase) => testCase.id === "CYC1-POS-explicit-close-break");
    delete missingEdgeOwner.graph.edges.find((edge) => edge.release === "explicitClose").owner;
    expect(evaluateCyc1Case(missingEdgeOwner).code).toBe("edge-owner-missing");

    const explicitEdgeForgedOwner = structuredClone(corpus);
    const forgedEdgeOwner = explicitEdgeForgedOwner.cases.find((testCase) => testCase.id === "CYC1-POS-explicit-close-break");
    forgedEdgeOwner.graph.edges.find((edge) => edge.release === "explicitClose").owner = "foreign-owner";
    expect(evaluateCyc1Case(forgedEdgeOwner).code).toBe("edge-owner-unknown");

    const closedOwnerRegistry = structuredClone(corpus);
    const registryOwner = closedOwnerRegistry.cases.find((testCase) => testCase.id === "CYC1-POS-explicit-close-break");
    registryOwner.graph.ownerRegistry = ["observer-owner"];
    registryOwner.graph.ownerRegistryClosed = true;
    registryOwner.graph.edges.find((edge) => edge.release === "explicitClose").owner = "observer-owner";
    registryOwner.events.find((event) => event.op === "close").owner = "observer-owner";
    expect(evaluateCyc1Case(registryOwner).status).toBe("clean");

    const drainKeepsExplicit = structuredClone(corpus);
    const explicit = drainKeepsExplicit.cases.find((testCase) => testCase.id === "CYC1-POS-explicit-close-break");
    explicit.events = explicit.events.filter((event) => event.op !== "close");
    explicit.events.splice(explicit.events.findIndex((event) => event.op === "drain"), 0, { op: "rootDrop", id: "hub-root" });
    expect(evaluateCyc1Case(explicit).code).toBe("W-MEMORY-0001");

    const liveRootDrop = structuredClone(corpus);
    const liveRoot = liveRootDrop.cases.find((testCase) => testCase.id === "CYC1-POS-weak-read-linearization");
    liveRoot.events = liveRoot.events.filter((event) => event.op !== "rootDrop");
    expect(evaluateCyc1Case(liveRoot).code).toBe("typed-drop-live-root");

    const targetedDrain = structuredClone(corpus);
    const resource = targetedDrain.cases.find((testCase) => testCase.id === "CYC1-POS-resource-async-finish");
    resource.graph.nodes.push({ id: "other" });
    resource.graph.edges.push({ id: "other-cycle", from: "other", to: "other", mode: "strong", release: "lifecycleDrain", owner: "other" });
    expect(evaluateCyc1Case(resource).code).toBe("W-MEMORY-0001");

    const forgedDrainOwner = structuredClone(corpus);
    const forgedDrain = forgedDrainOwner.cases.find((testCase) => testCase.id === "CYC1-POS-resource-async-finish");
    forgedDrain.graph.edges[0].owner = "not-a-resource";
    expect(evaluateCyc1Case(forgedDrain).code).toBe("edge-owner-unknown");

    const missingDrainOwner = structuredClone(corpus);
    const missingDrain = missingDrainOwner.cases.find((testCase) => testCase.id === "CYC1-POS-resource-async-finish");
    delete missingDrain.graph.edges[0].owner;
    expect(evaluateCyc1Case(missingDrain).code).toBe("edge-owner-missing");

    const preDrainOverBudget = structuredClone(corpus);
    const earlyQuota = preDrainOverBudget.cases.find((testCase) => testCase.id === "CYC1-INFO-census-quota");
    earlyQuota.events = [{ op: "admit" }, { op: "census" }];
    expect(evaluateCyc1Case(earlyQuota).code).toBe("W-MEMORY-AUDIT-BEFORE-DRAIN");

    const finishOrder = structuredClone(corpus);
    const unfinished = finishOrder.cases.find((testCase) => testCase.id === "CYC1-POS-resource-async-finish");
    const finishPosition = unfinished.events.findIndex((event) => event.op === "finish");
    unfinished.events.splice(finishPosition, 0, { op: "destroy", resource: "file-resource" });
    expect(evaluateCyc1Case(unfinished).code).toBe("destroy-before-finish");

    const forgedRootAuthority = structuredClone(corpus);
    forgedRootAuthority.cases.find((testCase) => testCase.id === "CYC1-POS-adapter-foreign-root").graph.roots[0].authority = "forged";
    expect(validateCyc1(forgedRootAuthority, { root: repositoryRoot }).errors.some((error) => error.includes("root authority is invalid"))).toBe(true);

    const forgedRootEventAuthority = structuredClone(corpus);
    const rootEventCase = forgedRootEventAuthority.cases.find((testCase) => testCase.id === "CYC1-POS-menu-weak-parent");
    rootEventCase.events.unshift({ op: "rootAdd", id: "forged-root", node: "root", authority: "forged" });
    expect(validateCyc1(forgedRootEventAuthority, { root: repositoryRoot }).errors.some((error) => error.includes("root event authority is invalid"))).toBe(true);

    const forgedServiceCycle = structuredClone(corpus);
    const serviceCase = forgedServiceCycle.cases.find((testCase) => testCase.id === "CYC1-POS-external-service-deadline");
    serviceCase.service.callCycle = "caller-choice";
    expect(validateCyc1(forgedServiceCycle, { root: repositoryRoot }).errors.some((error) => error.includes("service.callCycle"))).toBe(true);
    expect(evaluateCyc1Case(serviceCase).code).toBe("service-call-cycle-invalid");

    const foreignKnownBypass = structuredClone(corpus);
    const foreignEdgeCase = foreignKnownBypass.cases.find((testCase) => testCase.id === "CYC1-UNK-hidden-foreign-edge");
    foreignEdgeCase.graph.edges[0].authority = "foreign";
    foreignEdgeCase.graph.edges[0].known = undefined;
    expect(evaluateCyc1Case(foreignEdgeCase).status).toBe("unknown");
    expect(validateCyc1(foreignKnownBypass, { root: repositoryRoot }).errors).toEqual([]);

    const weakBlocks = evaluateCyc1Case(byId(corpus, "CYC1-POS-weak-blocks-per-target"));
    expect(weakBlocks.controlBlockTrace[1].freed).toBe(false);
    expect(weakBlocks.controlBlocksFreed).toEqual({ left: true, right: true });
    expect(weakBlocks.controlBlockFreeCount).toEqual({ left: 1, right: 1 });

    const duplicateSource = structuredClone(corpus);
    duplicateSource.cases[0].sourceRefs.push({ ...duplicateSource.cases[0].sourceRefs[0] });
    expect(validateCyc1(duplicateSource, { root: repositoryRoot }).errors.some((error) => error.includes("duplicates source reference"))).toBe(true);

    const missingSource = structuredClone(corpus);
    missingSource.cases[0].sourceRefs[0].path = "reference/last-light/missing-cycle.w";
    expect(validateCyc1(missingSource, { root: repositoryRoot }).errors.some((error) => error.includes("source reference is missing"))).toBe(true);

    const staleManifest = readStudy();
    staleManifest.sourceRefs[0].digest = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
    expect(validateCyc1StudyManifest(staleManifest, { studyDirectory, repositoryRoot }).some((error) => error.includes("digest is stale"))).toBe(true);
  });

  test("the snapshot digest is available to the checker without executing W", () => {
    expect(digestFile(corpusPath)).toMatch(/^sha256:[0-9a-f]{64}$/);
  });
});
