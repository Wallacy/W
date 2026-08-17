const DECISIONS = new Set([
  "W-1360",
  "W-1363",
  "W-1370",
  "W-1373",
  "W-1375",
  "W-1380",
  "W-1398",
  "W-1399",
]);
const IMPLEMENTATION_GAP_MAP = Object.freeze({
  SYN2: Object.freeze(["W-1362", "W-1366", "W-1367", "W-1368", "W-1369", "W-1371", "W-1372", "W-1374", "W-1376", "W-1377", "W-1378", "W-1379"]),
  DYN2: Object.freeze(["W-1358", "W-1392", "W-1393", "W-1394", "W-1395", "W-1396", "W-1397"]),
});
const IMPLEMENTATION_GAPS = new Set(Object.values(IMPLEMENTATION_GAP_MAP).flat());
const PERSISTENT_FIELDS = [
  "generationId", "artifactDigest", "recipeDigest", "semanticInterfaceKey",
  "schemaDigest", "targetReceipt", "resolveReceipt", "migrationReceipt",
];

const EXPECTED_MUTATION_KINDS = Object.freeze({
  "SYN2-reject-typed-recipe": "c2-duplicate-frontend",
  "SYN2-reject-current-module-injection": "current-module-injection",
  "DYN2-reject-live-state-migration": "live-state-migration",
  "DYN2-D-reject-eval-frame": "dynamic-eval",
  "DYN2-reject-native-unload-callback": "live-dlclose",
});
const EXPECTED_CASE_AXES = Object.freeze({
  "SYN2-C-module-set": "SYN2",
  "SYN2-C-action-result-interface": "SYN2",
  "SYN2-C-result-preserved-on-parse-fault": "SYN2",
  "SYN2-C-pre-result-cancel": "SYN2",
  "SYN2-C-target-receipts": "SYN2",
  "SYN2-C-implementation-boundary": "SYN2",
  "SYN2-events-derived": "SYN2",
  "SYN2-manifest-boundary": "SYN2",
  "SYN2-reject-typed-recipe": "SYN2",
  "SYN2-reject-current-module-injection": "SYN2",
  "DYN2-A-repl-snapshot": "DYN2",
  "DYN2-B-typed-service-local-split": "DYN2",
  "DYN2-C-generation-reference": "DYN2",
  "DYN2-reject-live-state-migration": "DYN2",
  "DYN2-D-reject-eval-frame": "DYN2",
  "DYN2-switch-cleanup-fault": "DYN2",
  "DYN2-reject-native-unload-callback": "DYN2",
});

function rejectCase(testCase, code) {
  return {
    id: testCase.id,
    axis: testCase.axis,
    route: "rejected",
    status: "rejected",
    code,
    implementationEvidenceGap: false,
  };
}

function evaluateSyn2(testCase) {
  const facts = testCase.facts ?? {};
  if (facts.artifact === "typed-declaration-recipe" || facts.duplicatesFrontend === true) {
    if (facts.artifact !== "typed-declaration-recipe" || facts.duplicatesFrontend !== true || facts.separateSourceUnit !== false) {
      return rejectCase(testCase, "typed-recipe-boundary-facts");
    }
    return rejectCase(testCase, "typed-recipe-duplicates-frontend");
  } else if (facts.artifact === "current-module-injection" || facts.hirSplice === true || facts.ambientAuthority === true) {
    if (facts.artifact !== "current-module-injection" || facts.hirSplice !== true || facts.ambientAuthority !== true || facts.separateSourceUnit !== false) {
      return rejectCase(testCase, "current-module-injection-facts");
    }
    return rejectCase(testCase, "current-module-injection-rejected");
  } else if (testCase.id === "SYN2-C-module-set") {
    if (facts.artifact !== "generated-module-set" || facts.sourceUnits !== "content-addressed-w-files" ||
        facts.reopen !== true || facts.interfaceBeforeFreeze !== true || facts.ambientAuthority !== false ||
        facts.identity !== "target-neutral-semantic" || JSON.stringify(facts.semanticPhases) !== JSON.stringify(["parse", "name", "type", "ownership", "effect", "ConstIR"])) {
      return rejectCase(testCase, "module-set-contract-incomplete");
    }
  } else if (testCase.id === "SYN2-C-action-result-interface") {
    if (facts.actionResultPublished !== true || facts.casPublished !== true || facts.interfacePublished !== true ||
        facts.compilerCachePublished !== false ||
        facts.actionResultIdentity !== "content-addressed-action-result" || facts.casIdentity !== "content-addressed-cas" ||
        facts.interfaceIdentity !== "semantic-interface" || facts.recipeIdentity !== "action-recipe") return rejectCase(testCase, "publication-identities-collapsed");
    const identities = [facts.actionResultIdentity, facts.casIdentity, facts.interfaceIdentity, facts.recipeIdentity];
    if (new Set(identities).size !== identities.length) return rejectCase(testCase, "publication-identities-collapsed");
  } else if (testCase.id === "SYN2-C-result-preserved-on-parse-fault") {
    if (facts.actionResultPublished !== true || facts.parseOrReceiptFailure !== true || facts.resultRetained !== true ||
        facts.interfacePublished !== false || facts.compilerCachePublished !== false || facts.cleanupCount !== 0) {
      return rejectCase(testCase, "parse-fault-publication-boundary");
    }
  } else if (testCase.id === "SYN2-C-pre-result-cancel") {
    if (facts.actionResultPublished !== false || facts.preResultFailure !== "cancel" || facts.staging !== true ||
        JSON.stringify(facts.cleanupOrder) !== JSON.stringify(["cleanup", "drain", "discard"]) || facts.cleanupCount !== 1 ||
        facts.oomAdmissionReject !== true || facts.faultBoundary !== "host-bookkeeping-only") {
      return rejectCase(testCase, "pre-result-cancel-cleanup");
    }
  } else if (testCase.id === "SYN2-C-target-receipts") {
    if (facts.projections !== 2 || facts.targetNeutralIdentity !== true || facts.targetWAbiReceipt !== true ||
        facts.registryDigest !== true || facts.physicalArtifactMayDiffer !== true || facts.targetImplicit !== false) {
      return rejectCase(testCase, "target-receipt-boundary");
    }
  } else if (testCase.id === "SYN2-C-implementation-boundary") {
    if (facts.hostOracle !== true || facts.compilerSemanticEvidence !== false || facts.targetCompilerProviderEvidence !== false ||
        facts.runEvidence !== false || facts.humanModelEvidence !== false || facts.claim !== "design-boundary-only") {
      return rejectCase(testCase, "implementation-boundary-forged");
    }
    return {
      id: testCase.id,
      axis: testCase.axis,
      route: "composable",
      status: "current-contract",
      code: "implementation-evidence-gap",
      implementationEvidenceGap: true,
      missingEvidence: ["compiler-semantic", "target-compiler-provider", "run", "human-model"],
    };
  } else if (testCase.id === "SYN2-events-derived") {
    if (JSON.stringify(facts.eventTrace) !== JSON.stringify(["tool-start", "tool-stage", "tool-write", "tool-finish"]) ||
        facts.statusDerived !== true || facts.routeDerived !== true || facts.expectedSelectsOutcome !== false || facts.callerFailureSelector !== false) {
      return rejectCase(testCase, "caller-owned-event-outcome");
    }
  } else if (testCase.id === "SYN2-manifest-boundary") {
    if (!["exactRoles", "exactArtifacts", "sourceRefs", "officialRefs", "httpsAllowlist", "targetRegistrySeparate"].every((key) => facts[key] === true) ||
        facts.forgedProviderReady !== false) return rejectCase(testCase, "manifest-evidence-boundary");
  }
  return { id: testCase.id, axis: testCase.axis, route: "composable", status: "current-contract", code: "promoted-design-contract", implementationEvidenceGap: false };
}

function evaluateDyn2(testCase) {
  const facts = testCase.facts ?? {};
  if (testCase.id === "DYN2-A-repl-snapshot") {
    if (facts.scenario !== "repl-snapshot" || facts.immutableSnapshot !== true || facts.exportBounded !== true || facts.liveStateWrite !== false || facts.generationIdentity !== true) {
      return rejectCase(testCase, "repl-snapshot-boundary");
    }
  } else if (testCase.id === "DYN2-B-typed-service-local-split") {
    if (facts.scenario !== "typed-service-generation" || facts.localSplitPair !== true || facts.logicalOutcomeEqual !== true ||
        facts.physicalTraceMayDiffer !== true || facts.capabilitiesExplicit !== true || facts.effectsReceipt !== true || facts.activeFrameMutation !== false) {
      return rejectCase(testCase, "typed-service-boundary");
    }
  } else if (testCase.id === "DYN2-C-generation-reference") {
    if (facts.scenario !== "generation-reference" || facts.readOnly !== true || facts.migrationReceipt !== true ||
        JSON.stringify(facts.persistentFields) !== JSON.stringify(PERSISTENT_FIELDS) || facts.targetMode !== "exact" ||
        facts.targetReceiptIncludesWAbi !== true || facts.targetReceiptIncludesRuntimeClosure !== true ||
        !Array.isArray(facts.extraFields) || facts.extraFields.length !== 0 || facts.duplicateFields !== false ||
        JSON.stringify(facts.forbiddenFields) !== JSON.stringify(["heap", "task", "loan", "frame", "capability", "ServiceRef", "callback", "providerHandle"]) || facts.migrationLiveState !== false) {
      return rejectCase(testCase, "generation-reference-boundary");
    }
  } else if (testCase.id === "DYN2-switch-cleanup-fault") {
    const cleanup = ["cancel", "drain", "unregister", "inFlightDrain", "destroy", "release"];
    if (facts.switchAtomic !== true || facts.postSwitchDrainFailure !== true || facts.derivedStatus !== "degraded" || facts.rollback !== false ||
        JSON.stringify(facts.cleanupOrder) !== JSON.stringify(cleanup) || facts.pinned !== false || facts.ffiCallback !== false ||
        facts.mappedProcess !== false || facts.mappedWasmComponent !== false || facts.nativeExactWAbi !== true || facts.mappingRetained !== true ||
        facts.staleCompletionRejected !== true) {
      return rejectCase(testCase, "post-switch-cleanup-boundary");
    }
  } else if (facts.liveState === true || facts.heap === true || facts.task === true || facts.loan === true || (facts.scenario === "generation-reference" && facts.readOnly === false)) {
    if (facts.scenario !== "generation-reference" || facts.readOnly !== false || facts.liveState !== true) {
      return rejectCase(testCase, "live-state-migration-facts");
    }
    return rejectCase(testCase, "live-state-migration-rejected");
  } else if (facts.scenario === "dynamic-eval" || facts.evalExec === true || facts.activeFrameMutation === true || facts.ambientLookup === true) {
    if (facts.scenario !== "dynamic-eval" || facts.evalExec !== true || facts.activeFrameMutation !== true || facts.ambientLookup !== true || facts.productionMode !== true) {
      return rejectCase(testCase, "dynamic-eval-facts");
    }
    return rejectCase(testCase, "eval-exec-active-frame-rejected");
  } else if (facts.scenario === "native-dynamic-library" || facts.callbackInFlight === true || facts.unmap === true) {
    if (facts.scenario !== "native-dynamic-library" || facts.callbackInFlight !== true || facts.unmap !== true || facts.mappingRetained !== false) {
      return rejectCase(testCase, "live-dlclose-facts");
    }
    return rejectCase(testCase, "live-callback-unload-rejected");
  }
  return { id: testCase.id, axis: testCase.axis, route: "composable", status: "current-contract", code: "promoted-design-contract", implementationEvidenceGap: false };
}

export function evaluateSyn2Dyn2Case(testCase) {
  if (testCase?.axis === "SYN2") return evaluateSyn2(testCase);
  if (testCase?.axis === "DYN2") return evaluateDyn2(testCase);
  return rejectCase(testCase ?? { id: "unknown", axis: "unknown" }, "unknown-axis");
}

export function validateSyn2Dyn2(corpus) {
  const errors = [];
  if (corpus?.$schema !== "w-syn2-dyn2-closure-cases-1") errors.push("schema mismatch");
  if (corpus?.status !== "host-derived-design-oracle") errors.push("status must be host-derived-design-oracle");
  if (corpus?.id !== "SYN2-DYN2") errors.push("id must be SYN2-DYN2");
  for (const axis of ["SYN2", "DYN2"]) {
    if (JSON.stringify(corpus?.implementationGapMap?.[axis]) !== JSON.stringify(IMPLEMENTATION_GAP_MAP[axis])) {
      errors.push(`implementationGapMap.${axis} must equal the exact implementation-gap map`);
    }
  }
  if (!Array.isArray(corpus?.cases) || corpus.cases.length !== 17) errors.push("closure corpus must contain exactly 17 cases");
  const actualIds = (corpus?.cases ?? []).map((testCase) => testCase?.id);
  const expectedIds = Object.keys(EXPECTED_CASE_AXES);
  if (actualIds.length !== expectedIds.length || JSON.stringify([...actualIds].sort()) !== JSON.stringify([...expectedIds].sort())) {
    errors.push("closure corpus must use the exact 17 case IDs");
  }
  const ids = new Set();
  for (const [index, testCase] of (corpus?.cases ?? []).entries()) {
    const location = `cases[${index}]`;
    if (!testCase || typeof testCase !== "object") { errors.push(`${location} must be an object`); continue; }
    if (ids.has(testCase.id)) errors.push(`${location}.id duplicates ${testCase.id}`);
    ids.add(testCase.id);
    if (!["SYN2", "DYN2"].includes(testCase.axis)) errors.push(`${location}.axis is not SYN2/DYN2`);
    if (EXPECTED_CASE_AXES[testCase.id] !== testCase.axis) errors.push(`${location}.axis does not match the exact case ID set`);
    if (!Array.isArray(testCase.decisionIds) || testCase.decisionIds.length === 0 || testCase.decisionIds.some((id) => !DECISIONS.has(id))) errors.push(`${location}.decisionIds are not closed`);
    if (testCase.gapRefs !== undefined && (!Array.isArray(testCase.gapRefs) || testCase.gapRefs.some((id) => !IMPLEMENTATION_GAP_MAP[testCase.axis].includes(id)))) errors.push(`${location}.gapRefs must use implementation-gap IDs from its axis map`);
    if (!testCase.facts || typeof testCase.facts !== "object") errors.push(`${location}.facts are required`);
    if (testCase.mutation !== null && (!testCase.mutation || typeof testCase.mutation !== "object" || typeof testCase.mutation.kind !== "string")) errors.push(`${location}.mutation must be null or typed`);
    if (!Array.isArray(testCase.reuse) || testCase.reuse.length === 0) errors.push(`${location}.reuse must link existing studies`);
    for (const forbidden of ["expected", "result", "status", "observedStatus", "route", "published", "drained", "rollback", "authority"]) {
      if (Object.hasOwn(testCase, forbidden)) errors.push(`${location}.${forbidden} is caller-owned`);
    }
    const result = evaluateSyn2Dyn2Case(testCase);
    const expectedMutationKind = EXPECTED_MUTATION_KINDS[testCase.id];
    if (testCase.mutation === null && expectedMutationKind !== undefined) errors.push(`${location} negative case must carry its expected mutation label`);
    if (testCase.mutation !== null) {
      if (expectedMutationKind === undefined || testCase.mutation.kind !== expectedMutationKind) errors.push(`${location}.mutation.kind is not the expected assertion label`);
      if (result.status !== "rejected") errors.push(`${location} facts did not derive rejection`);
    } else if (result.status !== "current-contract") errors.push(`${location} valid case did not derive current-contract`);
  }
  return { errors, caseCount: corpus?.cases?.length ?? 0 };
}

export function summarizeSyn2Dyn2(corpus) {
  const results = (corpus?.cases ?? []).map(evaluateSyn2Dyn2Case);
  return {
    caseCount: results.length,
    currentContractCount: results.filter((result) => result.status === "current-contract").length,
    rejectedCount: results.filter((result) => result.status === "rejected").length,
    implementationBoundaryCount: results.filter((result) => result.implementationEvidenceGap).length,
    axes: Object.fromEntries(["SYN2", "DYN2"].map((axis) => [axis, results.filter((result) => result.axis === axis).length])),
  };
}
