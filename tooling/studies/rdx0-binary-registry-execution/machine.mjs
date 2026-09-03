// Pure W-1518 design oracle. It reduces bounded structures and state
// transitions; it does not perform cryptography, HTTP, provider enforcement,
// registry serving or native execution.

export const CURRENT_PATHS = Object.freeze({
  discovery: "/.well-known/w-registry.json",
  root: "/v1/root/<version>.dsse",
  timestamp: "/v1/timestamp.dsse",
  object: "/v1/o/sha256/<hex>",
  channel: "/v1/channels/<encoded-package-id>/<encoded-channel>/<encoded-target-profile>.json",
  search: "/v1/search",
});

export const ROOT_ROLES = new Set(["root", "targets", "snapshot", "timestamp"]);
export const JSON_CONVENIENCE = new Set(["discovery", "search", "channel", "update"]);
export const EVIDENCE_LANES = new Set([
  "unit",
  "compile-fail",
  "property/fuzz",
  "simulation",
  "provider",
  "fault",
  "performance",
]);

const FORBIDDEN_CLAIMS = new Set([
  "compiler-available",
  "registry-available",
  "provider-conformant",
  "sandbox-isolated",
  "crypto-verified",
  "runner-available",
  "source-physically-discarded",
  "drm-inviolable",
]);

function accepted(facts = {}) {
  return { status: "accepted", evidenceBoundary: "design-only", facts };
}

function rejected(code, facts = {}) {
  return { status: "rejected", evidenceBoundary: "design-only", code, facts };
}

function objectLike(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function nonEmptyString(value) {
  return typeof value === "string" && value.length > 0;
}

function arrayEquals(actual, expected) {
  return Array.isArray(actual) && actual.length === expected.length &&
    actual.every((value, index) => value === expected[index]);
}

function exactKeys(value, expected) {
  return objectLike(value) && JSON.stringify(Object.keys(value).sort()) ===
    JSON.stringify([...expected].sort());
}

function commonBoundary(input) {
  if (input?.evidenceBoundary !== undefined && input.evidenceBoundary !== "design-only") {
    return rejected("implementationClaimForbidden");
  }
  if (input?.implementationClaim === true || FORBIDDEN_CLAIMS.has(input?.claim)) {
    return rejected("implementationClaimForbidden");
  }
  return null;
}

function reduceSerialization(input) {
  const w = input.wOwned;
  if (!objectLike(w) || w.payloadEncoding !== "deterministic-CBOR" ||
      w.envelope !== "DSSE" || w.roleSpecific !== true ||
      w.payloadBytesExact !== true) {
    return rejected("wOwnedSerializationContract");
  }
  if (w.objectDigestDomain !== "stored-envelope-bytes" ||
      w.dssePayloadDomain !== "exact-payload-bytes") {
    return rejected("digestDomainConfusion");
  }
  const external = input.externalAttestation;
  if (external !== undefined &&
      (!objectLike(external) || !arrayEquals(external.formats, ["in-toto-Statement-v1", "SLSA-provenance-v1.2"]) ||
       external.payloadEncoding !== "JSON" || external.envelope !== "DSSE" || external.convertToCbor === true)) {
    return rejected("externalAttestationEncoding");
  }
  if (input.jsonAuthority === true || (input.jsonKind && !JSON_CONVENIENCE.has(input.jsonKind))) {
    return rejected("jsonAuthorityForbidden");
  }
  if (input.hash?.algorithm !== "sha256" || input.hash?.tagged !== true) {
    return rejected("sha256TaggedRequired");
  }
  return accepted({
    wOwned: "deterministic-CBOR/DSSE",
    external: external ? "in-toto/SLSA JSON inside DSSE" : null,
    objectDigest: "stored-envelope-bytes",
    payloadDigest: "exact-payload-bytes",
    hash: "sha256-tagged",
  });
}

function reducePaths(input) {
  if (!objectLike(input.paths) || !exactKeys(input.paths, Object.keys(CURRENT_PATHS)) ||
      !Object.keys(CURRENT_PATHS).every((key) => input.paths[key] === CURRENT_PATHS[key])) {
    return rejected("pathSetMismatch");
  }
  if (!arrayEquals(input.objectMethods, ["GET", "HEAD"]) || input.range !== "optional") {
    return rejected("objectTransportContract");
  }
  if (input.discoveryTrusted !== false || input.searchAuthority !== false) {
    return rejected("convenienceAuthorityForbidden");
  }
  return accepted({
    paths: CURRENT_PATHS,
    objectMethods: ["GET", "HEAD"],
    range: "optional",
  });
}

function reduceRoot(input) {
  if (!objectLike(input.genesis) || input.genesis.outOfBand !== true ||
      input.genesis.completePayload !== true || input.genesis.signed !== false) {
    return rejected("genesisTrustInput");
  }
  if (!Number.isSafeInteger(input.previousVersion) || input.previousVersion < 1 ||
      !Number.isSafeInteger(input.nextVersion) || input.nextVersion < 1 ||
      input.nextVersion <= input.previousVersion) {
    return rejected("rootRollback");
  }
  if (input.nextVersion !== input.previousVersion + 1) return rejected("rootVersionGap");
  if (input.oldThresholdValid !== true || input.newThresholdValid !== true) {
    return rejected("rootThresholdMissing");
  }
  if (!arrayEquals(input.roles, ["root", "targets", "snapshot", "timestamp"]) ||
      !input.roles.every((role) => ROOT_ROLES.has(role))) {
    return rejected("rootRoleSet");
  }
  if (input.mixMatch === true) return rejected("rootMixMatch");
  if (input.freeze === true) return rejected("timestampFreeze");
  if (input.clockProven !== true) return rejected("clockUnproven");
  if (input.persistedMonotonic !== true) return rejected("timestampPersistence");
  if (input.expiryValid !== true) return rejected("expiryUnproven");
  return accepted({
    trustedGenesis: "out-of-band-complete-payload",
    rootTransition: `${input.previousVersion}->${input.nextVersion}`,
    thresholds: "old+new",
    roles: [...input.roles],
    freshness: "clock/provider-proven-and-monotonic",
  });
}

function reduceCatalog(input) {
  const index = input.packageIndex;
  if (!objectLike(index) || !nonEmptyString(index.package) ||
      !nonEmptyString(index.version) || !/^sha256:[0-9a-f]{64}$/.test(index.releaseDigest ?? "") ||
      index.objectPath !== CURRENT_PATHS.object) {
    return rejected("packageIndexContract");
  }
  if (input.searchAuthority === true || input.searchInLock === true || input.jsonAuthority === true ||
      input.knownIdentityUsesSearch !== false || input.updateUsesSearch !== false) {
    return rejected("convenienceAuthorityForbidden");
  }
  const update = input.update;
  if (update !== undefined) {
    if (!objectLike(update)) return rejected("convenienceRevalidation");
    const fields = ["schema", "snapshotDigest", "package", "channel", "targetProfile", "version", "releaseDigest", "artifactDigest", "state"];
    if (!objectLike(update) || !fields.every((field) => Object.hasOwn(update, field)) ||
        update.revalidatesAuthority !== true || update.json !== true) {
      return rejected("convenienceRevalidation");
    }
    if (!fields.every((field) => nonEmptyString(update[field])) ||
        !/^sha256:[0-9a-f]{64}$/.test(update.snapshotDigest) ||
        !/^sha256:[0-9a-f]{64}$/.test(update.releaseDigest) ||
        !/^sha256:[0-9a-f]{64}$/.test(update.artifactDigest)) {
      return rejected("convenienceDigestInvalid");
    }
    if (update.noChange204 === true && update.statusCode !== 204) return rejected("update204Mismatch");
  }
  if (input.appendOnly !== true || input.rewritesRelease === true || input.rewritesAttestation === true) {
    return rejected("appendOnlyViolation");
  }
  if (input.clockProven !== true || input.expiryProven !== true) return rejected("freshnessUnproven");
  const states = input.states;
  if (!objectLike(states) || !exactKeys(states, ["deprecation", "yank", "revocation"]) ||
      !Object.values(states).every((value) => value === "append-only")) return rejected("stateAppendOnlyRequired");
  return accepted({
    packageIndex: `${index.package}@${index.version}->${index.releaseDigest}`,
    knownIdentity: "not-search-dependent",
    states: "append-only",
    update: update ? "json-convenience-revalidated" : "none",
    stateKinds: ["deprecation", "yank", "revocation"],
  });
}

function reducePrivateRead(input) {
  const scope = input.scope;
  if (!objectLike(scope) || !nonEmptyString(scope.object) || !nonEmptyString(scope.package) ||
      !nonEmptyString(scope.audience) || !nonEmptyString(scope.expiry)) {
    return rejected("capabilityScopeMissing");
  }
  if (input.authority !== false || input.mirrorAuthority !== false) return rejected("capabilityAuthorityForbidden");
  if (input.authForwardedToUnconfiguredOrigin !== false) return rejected("authForwardingForbidden");
  if (![401, 403, 404].includes(input.privacyStatus)) return rejected("privacyModeMissing");
  return accepted({ scope: "object/package/audience/expiry", authority: false, privacyStatus: input.privacyStatus });
}

function reducePublication(input) {
  const requiredClaims = ["iss", "aud", "sub", "repository", "ref", "sha", "job_workflow_ref"];
  const oidc = input.oidc;
  if (!objectLike(oidc) || !arrayEquals(oidc.claims, requiredClaims) || oidc.jobWorkflowPinned !== true) {
    return rejected("oidcClaimsIncomplete");
  }
  if (oidc.replayed === true) return rejected("oidcReplay");
  const capability = input.publicationCapability;
  if (!objectLike(capability) || capability.oneUse !== true || capability.shortLived !== true || capability.scoped !== true) {
    return rejected("publicationCapabilityContract");
  }
  if (capability.reused === true) return rejected("capabilityReplay");
  if (input.rolesDistinct !== true) return rejected("publicationAuthorityConflation");
  if (!arrayEquals(input.visibilityStates, ["publicSource", "authorizedReproduction", "independentPublicReproduction"])) {
    return rejected("reproductionStateConflation");
  }
  if (input.providerDeletionProof === true) return rejected("providerDiscardNotProof");
  return accepted({ oidc: "validated-pinned-claims", capability: "one-use-short-lived-scoped", authorities: "separate" });
}

function reduceCapsule(input) {
  if (input.format !== "w.capsule/1" || !nonEmptyString(input.target) ||
      !nonEmptyString(input.profile) || !nonEmptyString(input.toolchain) ||
      input.contentAddressed !== true || input.boundedIndex !== true) {
    return rejected("capsuleIdentityContract");
  }
  const requiredKinds = ["native-object", "static-archive", "WInterface", "WMeta", "WAbi", "symbols", "runtime-requirements"];
  if (!Array.isArray(input.chunkKinds) || !requiredKinds.every((kind) => input.chunkKinds.includes(kind))) {
    return rejected("capsuleChunkKinds");
  }
  if (input.privateIR === true && input.exactToolchainKey !== true) return rejected("capsuleToolchainKeyRequired");
  if (input.rawMemoryHash === true) return rejected("relocatedMemoryHashForbidden");
  if (input.reuseLink !== "exact") return rejected("capsuleReuseContract");
  if (input.futureDylibExe !== "future") return rejected("capsuleFutureScope");
  return accepted({ format: "w.capsule/1", identity: `${input.target}/${input.profile}/${input.toolchain}`, reuse: "exact", futureDylibExe: true });
}

function reduceEvidence(input) {
  const descriptorKeys = ["stableId", "owner", "origin", "sourceMap", "kind", "fixtures", "effects", "oracle", "target", "profile", "seed", "limits", "bodyDigest"];
  const evidenceKeys = ["source", "release", "artifact", "plan", "analyzer", "recipe", "toolchain", "target", "profile", "seed", "limits", "outcome"];
  if (!Array.isArray(input.descriptorKeys) || !descriptorKeys.every((key) => input.descriptorKeys.includes(key))) return rejected("descriptorKeyMissing");
  if (!Array.isArray(input.evidenceKeys) || !evidenceKeys.every((key) => input.evidenceKeys.includes(key))) return rejected("evidenceKeyMissing");
  if (!Array.isArray(input.lanes) || input.lanes.length !== EVIDENCE_LANES.size ||
      new Set(input.lanes).size !== EVIDENCE_LANES.size ||
      !input.lanes.every((lane) => EVIDENCE_LANES.has(lane))) return rejected("evidenceLaneCollapse");
  if (input.simulationProvesProvider === true || input.aggregateSafeBadge === true) return rejected("evidenceClaimConflation");
  if (input.sbomSeparate !== true || input.runtimeClosureSeparate !== true || input.reachabilitySeparate !== true) {
    return rejected("evidenceAxisCollapse");
  }
  return accepted({ descriptorKeys, evidenceKeys, lanes: [...input.lanes], simulation: "not-provider-proof", badge: "none" });
}

function reduceSandbox(input) {
  if (input.remoteNative !== true || !["child-process", "compartment"].includes(input.launch) ||
      input.enforcementBeforeLoader !== true || input.providerReceiptRequired !== true ||
      input.capabilitiesExplicit !== true || input.requirementsExplicit !== true || input.budgetsExplicit !== true ||
      input.signatureGrantsCapability !== false) {
    return rejected("sandboxAdmissionContract");
  }
  if (input.fallback !== false || input.unsupportedFallsBack !== false) return rejected("providerFallbackForbidden");
  const forbidden = ["seccompOnly", "denylist", "ldPreload", "ptrace", "capSysAdmin", "sameProcessPlugin", "learnModeReceipt"];
  if (forbidden.some((key) => input[key] === true)) return rejected("sandboxBaselineRejected");
  return accepted({ launch: input.launch, enforcement: "before-loader", missingReceipt: "unsupported-no-fallback" });
}

function reduceRun(input) {
  if (input.exactVersion !== true || input.range === true || input.implicitChannel === true) return rejected("exactVersionRequired");
  const stages = ["trusted-root", "freshness-snapshot", "package-release-artifact-digest", "evidence-policy", "sandbox", "entrypoint"];
  if (!arrayEquals(input.admissionStages, stages)) return rejected("admissionOrderMismatch");
  if (input.providerReceipt !== true || input.networkDefault !== "denied" ||
      !["child-process", "compartment"].includes(input.launch) ||
      input.signatureGrantsCapability !== false || input.osSigningSeparate !== true) {
    return rejected("runAdmissionIncomplete");
  }
  const expectedCommands = new Set([
    "w run registry:last-light/restaurant@0.1.0 --product last-light-native --entry default --",
    "w run registry:last-light/restaurant@0.1.0 --product last-light-native --",
  ]);
  if (!expectedCommands.has(input.command)) {
    return rejected("remoteCliShape");
  }
  return accepted({ command: input.command, version: "exact", network: "denied-by-default", stages });
}

function reduceOffline(input) {
  if (input.pinned !== true || input.artifactVerified !== true || input.action !== "reuse") return rejected("offlinePinRequired");
  if (input.downloads !== false || input.updates !== false || input.defaultPolicy !== false) return rejected("offlineNetworkForbidden");
  if (!["stale", "unknown"].includes(input.freshness)) return rejected("offlineFreshnessRecord");
  return accepted({ action: "verified-artifact-reuse", freshness: input.freshness, network: "none" });
}

function reduceEntitlement(input) {
  const scope = input.scope;
  if (!objectLike(scope) || !nonEmptyString(scope.product) || !nonEmptyString(scope.feature) ||
      !nonEmptyString(scope.audience) || !nonEmptyString(scope.expiry)) return rejected("entitlementScopeMissing");
  if (input.rawTokenInApp === true || input.tokenReturnedToApp === true) return rejected("rawEntitlementTokenForbidden");
  if (input.inviolableDrmClaim === true) return rejected("inviolableDrmClaimForbidden");
  if (!["online", "offline"].includes(input.policy)) return rejected("entitlementPolicyMissing");
  return accepted({ entitlement: "opaque", scope: "product/feature/audience/expiry", policy: input.policy });
}

export function evaluateW1518Contract(input) {
  const boundary = commonBoundary(input);
  if (boundary) return boundary;
  if (!objectLike(input) || !nonEmptyString(input.kind)) return rejected("invalidDesignInput");
  switch (input.kind) {
    case "serialization": return reduceSerialization(input);
    case "paths": return reducePaths(input);
    case "root": return reduceRoot(input);
    case "catalog": return reduceCatalog(input);
    case "private-read": return reducePrivateRead(input);
    case "publication": return reducePublication(input);
    case "capsule": return reduceCapsule(input);
    case "evidence": return reduceEvidence(input);
    case "sandbox": return reduceSandbox(input);
    case "run": return reduceRun(input);
    case "offline": return reduceOffline(input);
    case "entitlement": return reduceEntitlement(input);
    default: return rejected("unknownDesignCase");
  }
}

export function runW1518Case(testCase) {
  if (!Array.isArray(testCase?.decisions) || !testCase.decisions.includes("W-1518")) {
    return rejected("decisionReferenceMissing");
  }
  return evaluateW1518Contract(testCase.input);
}
