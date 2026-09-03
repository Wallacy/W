import crypto from "node:crypto";

/*
 * SEC0 is a host oracle for the security model. It consumes closed evidence
 * records and derives the outcome. It does not implement W, a sandbox, or a
 * provider. A caller cannot supply the outcome, route, verification, or
 * security status as input.
 */

export class Sec0Error extends Error {
  constructor(code, details = {}) {
    super(code);
    this.code = code;
    this.details = details;
  }
}

const DIGEST = /^sha256:[0-9a-f]{64}$/u;

export const PROFILES = Object.freeze([
  "trusted-native-cpu",
  "sandboxed-native-process",
  "wasm-component",
  "multi-tenant-isolate",
  "embedded-freestanding",
  "fpga-asic-hardware",
]);

const PROFILE_SET = new Set(PROFILES);
const CALLER_ECHO_KEYS = new Set([
  "expected",
  "status",
  "route",
  "result",
  "available",
  "verified",
  "verification",
  "secure",
  "securityStatus",
  "satisfied",
  "ok",
  "outcome",
]);

const SCHEMA_KEYS = Object.freeze({
  safeInvariants: new Set([
    "kind", "memoryProof", "typeProof", "effectProof", "capabilityProof",
    "resourceProof", "checkRequired", "checkProven", "checkElided",
    "ambientAuthority", "dynamicEvaluation", "uncheckedOperation",
    "unsafeExplicit", "unsafeContractComplete", "arbitraryCode", "inputBounded",
  ]),
  apiMediation: new Set([
    "kind", "api", "capabilityOrigin", "explicitCapability", "effectDeclared",
    "apiMediated", "attenuation", "ambientLookup", "stringLookup", "unknownApi",
  ]),
  inputResource: new Set([
    "kind", "inputBounded", "traversalBudget", "resourceQuota", "secretLease",
    "secretNotSerialized", "secretNotLogged", "auditTrail", "sourceLock",
    "sourceProvenance", "artifactAttestation", "reproducibleBuild",
  ]),
  profileIsolation: new Set([
    "kind", "profile", "productMinimum", "deploymentControls", "protections",
    "threatModel", "threatExclusions", "residualRisk", "target", "artifactDigest", "physicalChange", "semanticInterfaceKey",
    "previousSemanticInterfaceKey", "wAbiKey", "runtimeClosureKey",
    "hardeningReceipt", "externalMediation", "hardwareEnforcement",
  ]),
  sideChannel: new Set([
    "kind", "threats", "clockPolicy", "schedulerPolicy", "concurrencyPolicy",
    "mitigations", "residualRisk", "universalClaim", "timerSource",
  ]),
  ffiUnsafe: new Set([
    "kind", "unsafeExplicit", "abiContract", "provenance", "cleanup",
    "bounds", "foreignInputBounded", "effectDeclared", "reviewedBoundary",
    "undefinedBehavior", "rawPointer", "noescape", "allocatorContract",
  ]),
  multiTenant: new Set([
    "kind", "tenantBinding", "isolationBoundary", "resourceMediation",
    "networkMediation", "secretPartition", "auditTrail", "crossTenantCapability",
    "debuggerBypass", "tenantCountBounded",
  ]),
  supplyChain: new Set([
    "kind", "sourceDigest", "lockDigest", "artifactDigest", "signer",
    "attestation", "reproducible", "patchBase", "patchDelta", "rollbackPolicy",
  ]),
  patchAttestation: new Set(["kind", "events", "target", "profile"]),
  identity: new Set([
    "kind", "publicContractChanged", "semanticInterfaceKey", "previousSemanticInterfaceKey",
    "physicalChange", "wAbiKey", "runtimeClosureKey", "hardeningReceipt",
  ]),
  featureComposition: new Set([
    "kind", "runtimeFeature", "availability", "packageFeature", "securityProfile",
    "capabilityGrant", "effectGrant", "dependencyLoad", "moduleLoad", "abiChange",
    "interfaceChange", "unsafeEnable", "protectionDisable",
  ]),
});

const COMMON_MINIMUMS = Object.freeze(["memory-safety", "effect-capability-checks", "input-bounds", "supply-chain"]);
const PROFILE_MINIMUMS = Object.freeze({
  "trusted-native-cpu": [],
  "sandboxed-native-process": ["process-boundary", "mediated-io"],
  "wasm-component": ["component-boundary", "mediated-io"],
  "multi-tenant-isolate": ["tenant-boundary", "mediated-io", "resource-quota"],
  "embedded-freestanding": ["freestanding-budgets"],
  "fpga-asic-hardware": ["hardware-boundary", "information-flow"],
});
const TARGET_BY_PROFILE = Object.freeze({
  "trusted-native-cpu": "native-cpu",
  "sandboxed-native-process": "sandboxed-native-process",
  "wasm-component": "wasm-component",
  "multi-tenant-isolate": "multi-tenant-isolate",
  "embedded-freestanding": "embedded-freestanding",
  "fpga-asic-hardware": "fpga-asic-hardware",
});
const TARGET_SET = new Set(Object.values(TARGET_BY_PROFILE));

const THREATS = new Set([
  "untrusted-input", "faulty-dependency", "tenant-confusion", "cross-tenant",
  "timing", "cache", "scheduler", "resource-use", "microarchitectural-observation",
  "network-peer", "supply-chain", "fault-injection", "debugger", "ffi",
]);
const RESIDUAL_RISKS = new Set([
  "microarchitectural-timing", "hardware-cache-observation", "resource-contention",
  "host-scheduler", "physical-probe", "power-analysis", "fault-injection", "provider-unknown",
]);
const BASIS_BY_CLASS = Object.freeze({
  memory: new Set(["runtime-enforcement", "static-proof", "hardware-enforcement"]),
  boundary: new Set(["runtime-enforcement", "static-proof", "hardware-enforcement", "external-mediation"]),
  resource: new Set(["runtime-enforcement", "static-proof", "hardware-enforcement", "external-mediation"]),
  isolation: new Set(["runtime-enforcement", "hardware-enforcement", "external-mediation", "threat-model-not-applicable"]),
  tenant: new Set(["runtime-enforcement", "hardware-enforcement", "external-mediation", "threat-model-not-applicable"]),
  "side-channel": new Set(["runtime-enforcement", "hardware-enforcement", "external-mediation", "threat-model-not-applicable"]),
});
const RECEIPT_ISSUER_BY_BASIS = Object.freeze({
  "runtime-enforcement": "runtime-provider",
  "static-proof": "compiler",
  "hardware-enforcement": "target-provider",
  "external-mediation": "deployment-provider",
  "threat-model-not-applicable": "policy-review",
});
const RECEIPT_STAGE_BY_BASIS = Object.freeze({
  "runtime-enforcement": "runtime",
  "static-proof": "compile",
  "hardware-enforcement": "target",
  "external-mediation": "deployment",
  "threat-model-not-applicable": "policy",
});
const THREAT_EXCLUSIONS = new Set(["isolation", "tenant", "side-channel"]);
const RUNTIME_ENFORCEMENT = new Set(["present", "omitted"]);

function object(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function fail(code, details = {}) {
  throw new Sec0Error(code, details);
}

function clone(value) {
  return structuredClone(value);
}

export function canonical(value) {
  if (Array.isArray(value)) return `[${value.map(canonical).join(",")}]`;
  if (object(value)) return `{${Object.keys(value).sort().map((key) => `${JSON.stringify(key)}:${canonical(value[key])}`).join(",")}}`;
  return JSON.stringify(value);
}

export function digestRecord(schema, value) {
  return `sha256:${crypto.createHash("sha256").update(`${schema}\n${canonical(value)}`).digest("hex")}`;
}

function rejectCallerEcho(value, location = "operation") {
  if (!object(value) && !Array.isArray(value)) return;
  if (Array.isArray(value)) {
    value.forEach((child, index) => rejectCallerEcho(child, `${location}[${index}]`));
    return;
  }
  for (const [key, child] of Object.entries(value)) {
    if (CALLER_ECHO_KEYS.has(key)) fail("callerEchoRejected", { location: `${location}.${key}` });
    rejectCallerEcho(child, `${location}.${key}`);
  }
}

function closed(value, allowed, code = "schemaClosed", location = "operation") {
  if (!object(value)) fail(code, { location });
  for (const key of Object.keys(value)) if (!allowed.has(key)) fail(code, { location: `${location}.${key}` });
}

function nonEmpty(value, code, field) {
  if (typeof value !== "string" || value.trim() === "") fail(code, { field });
  return value;
}

function bool(value, code, field) {
  if (typeof value !== "boolean") fail(code, { field });
  return value;
}

function list(value, code, field, { nonempty = true } = {}) {
  if (!Array.isArray(value) || (nonempty && value.length === 0)) fail(code, { field });
  return value;
}

function digest(value, code, field) {
  if (typeof value !== "string" || !DIGEST.test(value)) fail(code, { field });
  return value;
}

function allTrue(operation, fields, code) {
  for (const field of fields) if (operation[field] !== true) fail(code, { field });
}

function result(code, extra = {}, route = "current") {
  return { status: "accepted", route, code, ...extra };
}

function reduceSafeInvariants(operation) {
  closed(operation, SCHEMA_KEYS.safeInvariants, "safeInvariantSchemaClosed");
  allTrue(operation, ["memoryProof", "typeProof", "effectProof", "capabilityProof", "resourceProof", "inputBounded"], "safeProofMissing");
  bool(operation.ambientAuthority, "safeFactInvalid", "ambientAuthority");
  bool(operation.dynamicEvaluation, "safeFactInvalid", "dynamicEvaluation");
  bool(operation.uncheckedOperation, "safeFactInvalid", "uncheckedOperation");
  bool(operation.arbitraryCode, "safeFactInvalid", "arbitraryCode");
  bool(operation.checkRequired, "safeFactInvalid", "checkRequired");
  bool(operation.checkProven, "safeFactInvalid", "checkProven");
  bool(operation.checkElided, "safeFactInvalid", "checkElided");
  bool(operation.unsafeExplicit, "safeFactInvalid", "unsafeExplicit");
  bool(operation.unsafeContractComplete, "safeFactInvalid", "unsafeContractComplete");
  if (operation.ambientAuthority || operation.dynamicEvaluation || operation.arbitraryCode) fail("safeAuthorityRejected");
  if (operation.checkRequired && operation.checkElided && !operation.checkProven) fail("checkElisionUnproven");
  if (operation.uncheckedOperation && !operation.unsafeExplicit) fail("uncheckedOperationRejected");
  if (operation.unsafeExplicit && !operation.unsafeContractComplete) fail("unsafeContractIncomplete");
  return result(operation.unsafeExplicit ? "explicitUnsafeIsland" : "safeProofsPreserved", { safe: !operation.unsafeExplicit });
}

function reduceApiMediation(operation) {
  closed(operation, SCHEMA_KEYS.apiMediation, "apiMediationSchemaClosed");
  nonEmpty(operation.api, "apiMediationFactInvalid", "api");
  nonEmpty(operation.capabilityOrigin, "apiMediationFactInvalid", "capabilityOrigin");
  bool(operation.explicitCapability, "apiMediationFactInvalid", "explicitCapability");
  bool(operation.effectDeclared, "apiMediationFactInvalid", "effectDeclared");
  bool(operation.apiMediated, "apiMediationFactInvalid", "apiMediated");
  bool(operation.ambientLookup, "apiMediationFactInvalid", "ambientLookup");
  bool(operation.stringLookup, "apiMediationFactInvalid", "stringLookup");
  bool(operation.unknownApi, "apiMediationFactInvalid", "unknownApi");
  nonEmpty(operation.attenuation, "apiMediationFactInvalid", "attenuation");
  if (operation.ambientLookup || operation.stringLookup || operation.unknownApi) fail("ambientAuthorityRejected");
  if (!operation.explicitCapability || !operation.effectDeclared || !operation.apiMediated) fail("apiMediationMissing");
  if (!new Set(["deployment-binding", "explicit-payload", "derived-attenuated"]).has(operation.capabilityOrigin)) fail("capabilityOriginRejected");
  if (!new Set(["same", "narrower"]).has(operation.attenuation)) fail("capabilityAmplificationRejected");
  return result("apiMediated", { authority: "attenuated" });
}

function reduceInputResource(operation) {
  closed(operation, SCHEMA_KEYS.inputResource, "inputResourceSchemaClosed");
  allTrue(operation, ["inputBounded", "traversalBudget", "resourceQuota", "secretLease", "secretNotSerialized", "secretNotLogged", "auditTrail", "sourceLock", "sourceProvenance", "artifactAttestation", "reproducibleBuild"], "securityEvidenceMissing");
  return result("boundedInputResourceAndSupplyChain", { audit: "explicit" });
}

function requireControls(operation) {
  list(operation.productMinimum, "profileFactInvalid", "productMinimum");
  list(operation.deploymentControls, "profileFactInvalid", "deploymentControls");
  const product = new Set(operation.productMinimum);
  const deployment = new Set(operation.deploymentControls);
  for (const control of [...COMMON_MINIMUMS, ...PROFILE_MINIMUMS[operation.profile]]) {
    if (!product.has(control)) fail("profileMinimumMissing", { profile: operation.profile, control, scope: "product" });
    if (!deployment.has(control)) fail("profileMinimumMissing", { profile: operation.profile, control, scope: "deployment" });
  }
  for (const control of operation.productMinimum) if (!deployment.has(control)) fail("deploymentWeakensProductMinimum", { control });
}

function validateProtectionReceipt(protection, operation, index) {
  if (typeof protection.basis !== "string" || !BASIS_BY_CLASS[protection.class]?.has(protection.basis)) {
    fail("runtimeProtectionBasisInvalid", { name: protection.name, class: protection.class, basis: protection.basis });
  }
  const receipt = protection.receipt;
  if (!object(receipt)) fail("protectionReceiptMissing", { name: protection.name, index });
  closed(receipt, new Set(["schema", "issuer", "stage", "profile", "target", "artifactDigest", "proofDigest"]), "protectionReceiptSchemaClosed", `protections[${index}].receipt`);
  if (receipt.schema !== "w.security-proof/1") fail("protectionReceiptInvalid", { name: protection.name, field: "schema" });
  if (receipt.issuer !== RECEIPT_ISSUER_BY_BASIS[protection.basis]) fail("protectionReceiptInvalid", { name: protection.name, field: "issuer" });
  if (receipt.stage !== RECEIPT_STAGE_BY_BASIS[protection.basis]) fail("protectionReceiptInvalid", { name: protection.name, field: "stage" });
  if (receipt.profile !== operation.profile) fail("protectionReceiptScopeInvalid", { name: protection.name, scope: "profile" });
  if (receipt.target !== operation.target) fail("protectionReceiptScopeInvalid", { name: protection.name, scope: "target" });
  digest(receipt.artifactDigest, "protectionReceiptInvalid", "artifactDigest");
  if (receipt.artifactDigest !== operation.artifactDigest) fail("protectionReceiptScopeInvalid", { name: protection.name, scope: "artifact" });
  digest(receipt.proofDigest, "protectionReceiptInvalid", "proofDigest");
}

function reduceProfileIsolation(operation) {
  closed(operation, SCHEMA_KEYS.profileIsolation, "profileSchemaClosed");
  if (!PROFILE_SET.has(operation.profile)) fail("profileUnknown", { profile: operation.profile });
  if (operation.target !== TARGET_BY_PROFILE[operation.profile]) {
    if (typeof operation.target !== "string" || !TARGET_SET.has(operation.target)) fail("profileTargetUnknown", { profile: operation.profile, target: operation.target });
    fail("profileTargetMismatch", { profile: operation.profile, target: operation.target });
  }
  digest(operation.artifactDigest, "profileFactInvalid", "artifactDigest");
  requireControls(operation);
  list(operation.threatModel, "profileFactInvalid", "threatModel");
  list(operation.threatExclusions, "profileFactInvalid", "threatExclusions", { nonempty: false });
  list(operation.residualRisk, "profileFactInvalid", "residualRisk");
  for (const [index, threat] of operation.threatModel.entries()) if (!THREATS.has(threat)) fail("profileThreatUnknown", { index, threat });
  for (const [index, exclusion] of operation.threatExclusions.entries()) if (!THREAT_EXCLUSIONS.has(exclusion)) fail("threatExclusionUnknown", { index, exclusion });
  for (const [index, risk] of operation.residualRisk.entries()) if (!RESIDUAL_RISKS.has(risk)) fail("profileResidualRiskUnknown", { index, risk });
  if (operation.residualRisk.includes("hardware-cache-observation") && !operation.threatModel.includes("cache") && !operation.threatModel.includes("microarchitectural-observation")) fail("profileResidualThreatMismatch");
  list(operation.protections, "profileFactInvalid", "protections");
  const seen = new Set();
  for (const [index, protection] of operation.protections.entries()) {
    closed(protection, new Set(["name", "class", "runtimeEnforcement", "basis", "receipt"]), "protectionSchemaClosed", `protections[${index}]`);
    nonEmpty(protection.name, "protectionFactInvalid", `protections[${index}].name`);
    nonEmpty(protection.class, "protectionFactInvalid", `protections[${index}].class`);
    if (!BASIS_BY_CLASS[protection.class]) fail("protectionClassUnknown", { class: protection.class });
    if (!RUNTIME_ENFORCEMENT.has(protection.runtimeEnforcement)) fail("protectionFactInvalid", { field: `protections[${index}].runtimeEnforcement` });
    if (protection.runtimeEnforcement === "present" && protection.basis !== "runtime-enforcement") fail("runtimeProtectionBasisInvalid", { name: protection.name, basis: protection.basis, runtimeEnforcement: protection.runtimeEnforcement });
    if (protection.runtimeEnforcement === "omitted" && protection.basis === "runtime-enforcement") fail("runtimeProtectionBasisInvalid", { name: protection.name, basis: protection.basis, runtimeEnforcement: protection.runtimeEnforcement });
    if (seen.has(protection.name)) fail("protectionDuplicate", { name: protection.name });
    seen.add(protection.name);
    validateProtectionReceipt(protection, operation, index);
    if (protection.basis === "threat-model-not-applicable" && !operation.threatExclusions.includes(protection.class)) fail("threatModelExceptionInvalid", { name: protection.name, class: protection.class });
  }
  bool(operation.physicalChange, "profileFactInvalid", "physicalChange");
  if (operation.physicalChange) {
    digest(operation.semanticInterfaceKey, "profileIdentityInvalid", "semanticInterfaceKey");
    digest(operation.previousSemanticInterfaceKey, "profileIdentityInvalid", "previousSemanticInterfaceKey");
    digest(operation.wAbiKey, "profileIdentityInvalid", "wAbiKey");
    digest(operation.runtimeClosureKey, "profileIdentityInvalid", "runtimeClosureKey");
    digest(operation.hardeningReceipt, "profileIdentityInvalid", "hardeningReceipt");
    if (operation.semanticInterfaceKey !== operation.previousSemanticInterfaceKey) fail("physicalChangeMutatesSemanticInterface");
  }
  return result("profileAdmitted", { profile: operation.profile, residualRisk: operation.residualRisk.length }, "current-design-evidence-gap");
}

function reduceSideChannel(operation) {
  closed(operation, SCHEMA_KEYS.sideChannel, "sideChannelSchemaClosed");
  list(operation.threats, "sideChannelFactInvalid", "threats");
  list(operation.mitigations, "sideChannelFactInvalid", "mitigations");
  list(operation.residualRisk, "sideChannelFactInvalid", "residualRisk");
  for (const [index, threat] of operation.threats.entries()) if (!THREATS.has(threat)) fail("sideChannelThreatUnknown", { index, threat });
  for (const [index, risk] of operation.residualRisk.entries()) if (!RESIDUAL_RISKS.has(risk)) fail("sideChannelResidualRiskUnknown", { index, risk });
  nonEmpty(operation.clockPolicy, "sideChannelFactInvalid", "clockPolicy");
  nonEmpty(operation.schedulerPolicy, "sideChannelFactInvalid", "schedulerPolicy");
  nonEmpty(operation.concurrencyPolicy, "sideChannelFactInvalid", "concurrencyPolicy");
  nonEmpty(operation.timerSource, "sideChannelFactInvalid", "timerSource");
  bool(operation.universalClaim, "sideChannelFactInvalid", "universalClaim");
  if (operation.universalClaim) fail("universalSideChannelClaimRejected");
  return result("sideChannelBudgeted", { residualRisk: operation.residualRisk.length }, "current-design-evidence-gap");
}

function reduceFfiUnsafe(operation) {
  closed(operation, SCHEMA_KEYS.ffiUnsafe, "ffiSchemaClosed");
  bool(operation.unsafeExplicit, "ffiFactInvalid", "unsafeExplicit");
  bool(operation.undefinedBehavior, "ffiFactInvalid", "undefinedBehavior");
  bool(operation.rawPointer, "ffiFactInvalid", "rawPointer");
  bool(operation.noescape, "ffiFactInvalid", "noescape");
  allTrue(operation, ["abiContract", "provenance", "cleanup", "bounds", "foreignInputBounded", "effectDeclared", "reviewedBoundary", "allocatorContract"], "ffiEvidenceMissing");
  if (operation.undefinedBehavior) fail("undefinedBehaviorRejected");
  if (operation.rawPointer && !operation.unsafeExplicit) fail("unsafeBoundaryRequired");
  if (!operation.unsafeExplicit && !operation.noescape) fail("foreignRetentionRequiresUnsafe");
  return result(operation.unsafeExplicit ? "explicitUnsafeFfi" : "mediatedFfi", { unsafe: operation.unsafeExplicit });
}

function reduceMultiTenant(operation) {
  closed(operation, SCHEMA_KEYS.multiTenant, "multiTenantSchemaClosed");
  allTrue(operation, ["tenantBinding", "isolationBoundary", "resourceMediation", "networkMediation", "secretPartition", "auditTrail", "tenantCountBounded"], "tenantEvidenceMissing");
  bool(operation.crossTenantCapability, "tenantFactInvalid", "crossTenantCapability");
  bool(operation.debuggerBypass, "tenantFactInvalid", "debuggerBypass");
  if (operation.crossTenantCapability || operation.debuggerBypass) fail("tenantBoundaryRejected");
  return result("tenantIsolated", { tenants: "bounded" });
}

function reduceSupplyChain(operation) {
  closed(operation, SCHEMA_KEYS.supplyChain, "supplyChainSchemaClosed");
  for (const field of ["sourceDigest", "lockDigest", "artifactDigest", "attestation", "patchBase", "patchDelta"]) digest(operation[field], "supplyChainFactInvalid", field);
  if (typeof operation.signer !== "string" || !/^sigstore:[A-Za-z0-9._:@/-]+$/u.test(operation.signer)) fail("supplyChainFactInvalid", { field: "signer" });
  if (!new Set(["retain-last-known-good", "forward-only", "dual-control"]).has(operation.rollbackPolicy)) fail("supplyChainFactInvalid", { field: "rollbackPolicy" });
  allTrue(operation, ["reproducible"], "supplyChainEvidenceMissing");
  return result("supplyChainAdmitted", { provenance: "attested" });
}

function reducePatchAttestation(operation) {
  closed(operation, SCHEMA_KEYS.patchAttestation, "patchSchemaClosed");
  if (!PROFILE_SET.has(operation.profile)) fail("profileUnknown", { profile: operation.profile });
  nonEmpty(operation.target, "patchFactInvalid", "target");
  list(operation.events, "patchEventsInvalid", "events");
  const expected = ["source-digest", "lock-digest", "build-recipe", "artifact-digest", "signature", "attestation", "deployment-admit"];
  const actual = operation.events.map((event, index) => {
    closed(event, new Set(["kind", "value"]), "patchEventSchemaClosed", `events[${index}]`);
    nonEmpty(event.kind, "patchEventInvalid", `events[${index}].kind`);
    nonEmpty(event.value, "patchEventInvalid", `events[${index}].value`);
    const digestKinds = new Set(["source-digest", "lock-digest", "build-recipe", "artifact-digest", "attestation", "deployment-admit"]);
    if (digestKinds.has(event.kind) && !DIGEST.test(event.value)) fail("patchEventFactInvalid", { index, kind: event.kind });
    if (event.kind === "signature" && !/^sigstore:[A-Za-z0-9._:@/-]+$/u.test(event.value)) fail("patchEventFactInvalid", { index, kind: event.kind });
    return event.kind;
  });
  if (actual.length !== expected.length || actual.some((kind, index) => kind !== expected[index])) fail("patchAttestationOrderInvalid", { expected, actual });
  return result("patchAttested", { receipt: digestRecord("w.security-attestation/1", { target: operation.target, profile: operation.profile, events: operation.events }) }, "current-design-evidence-gap");
}

function reduceIdentity(operation) {
  closed(operation, SCHEMA_KEYS.identity, "identitySchemaClosed");
  bool(operation.publicContractChanged, "identityFactInvalid", "publicContractChanged");
  bool(operation.physicalChange, "identityFactInvalid", "physicalChange");
  digest(operation.semanticInterfaceKey, "identityFactInvalid", "semanticInterfaceKey");
  digest(operation.previousSemanticInterfaceKey, "identityFactInvalid", "previousSemanticInterfaceKey");
  if (operation.publicContractChanged && operation.semanticInterfaceKey === operation.previousSemanticInterfaceKey) fail("publicChangeKeepsSemanticInterface");
  if (!operation.publicContractChanged && operation.semanticInterfaceKey !== operation.previousSemanticInterfaceKey) fail("privateChangeMutatesSemanticInterface");
  if (operation.physicalChange) {
    digest(operation.wAbiKey, "identityFactInvalid", "wAbiKey");
    digest(operation.runtimeClosureKey, "identityFactInvalid", "runtimeClosureKey");
    digest(operation.hardeningReceipt, "identityFactInvalid", "hardeningReceipt");
  }
  return result("identityConsistent", { semanticChanged: operation.publicContractChanged, physicalChanged: operation.physicalChange });
}

function reduceFeatureComposition(operation) {
  closed(operation, SCHEMA_KEYS.featureComposition, "featureCompositionSchemaClosed");
  for (const field of ["runtimeFeature", "availability", "packageFeature", "securityProfile", "capabilityGrant", "effectGrant", "dependencyLoad", "moduleLoad", "abiChange", "interfaceChange", "unsafeEnable", "protectionDisable"]) bool(operation[field], "featureFactInvalid", field);
  if (operation.securityProfile || operation.capabilityGrant || operation.effectGrant || operation.dependencyLoad || operation.moduleLoad || operation.abiChange || operation.interfaceChange || operation.unsafeEnable || operation.protectionDisable) fail("featureSecurityAuthorityRejected");
  return result("featureSecurityNeutral", { authority: "unchanged" });
}

const REDUCERS = Object.freeze({
  safeInvariants: reduceSafeInvariants,
  apiMediation: reduceApiMediation,
  inputResource: reduceInputResource,
  profileIsolation: reduceProfileIsolation,
  sideChannel: reduceSideChannel,
  ffiUnsafe: reduceFfiUnsafe,
  multiTenant: reduceMultiTenant,
  supplyChain: reduceSupplyChain,
  patchAttestation: reducePatchAttestation,
  identity: reduceIdentity,
  featureComposition: reduceFeatureComposition,
});

export function deriveSec0Case(testCase) {
  if (!testCase || typeof testCase.id !== "string") throw new Error("SEC0 case requires id");
  try {
    if (!object(testCase.operation)) fail("operationMissing");
    rejectCallerEcho(testCase.operation);
    const reducer = REDUCERS[testCase.operation.kind];
    if (!reducer) fail("operationKindUnknown", { kind: testCase.operation.kind });
    return { caseId: testCase.id, axis: testCase.operation.kind, ...reducer(clone(testCase.operation)) };
  } catch (error) {
    if (!(error instanceof Sec0Error)) throw error;
    return { caseId: testCase.id, axis: testCase.operation?.kind ?? "unknown", status: "rejected", route: "rejected", code: error.code, details: error.details };
  }
}

function merge(base, patch) {
  const output = clone(base ?? {});
  for (const [key, value] of Object.entries(patch ?? {})) {
    if (object(value) && object(output[key])) output[key] = merge(output[key], value);
    else output[key] = clone(value);
  }
  return output;
}

export function deriveSec0(corpus) {
  return (corpus.cases ?? []).map((testCase) => deriveSec0Case({
    ...testCase,
    operation: merge(corpus.fixtures?.[testCase.fixture], testCase.patch),
  }));
}

export function validateSec0(corpus) {
  const errors = [];
  if (corpus?.$schema !== "w-sec0-security-model-cases-1") errors.push("SEC0 corpus schema is invalid.");
  if (corpus?.status !== "design-oracle-input") errors.push("SEC0 corpus status must be design-oracle-input.");
  if (!Array.isArray(corpus?.cases) || corpus.cases.length < 50) errors.push("SEC0 requires at least 50 cases.");
  const ids = new Set();
  const kinds = new Set();
  for (const [index, testCase] of (corpus.cases ?? []).entries()) {
    if (!/^SEC0-[a-z0-9]+(?:-[a-z0-9]+)*$/u.test(testCase.id ?? "")) errors.push(`cases[${index}] id is invalid.`);
    if (ids.has(testCase.id)) errors.push(`duplicate case ${testCase.id}.`);
    ids.add(testCase.id);
    if (!testCase.fixture || !corpus.fixtures?.[testCase.fixture]) errors.push(`${testCase.id} fixture is missing.`);
    if (!testCase.expected || !["accepted", "rejected"].includes(testCase.expected.status) || typeof testCase.expected.code !== "string") errors.push(`${testCase.id} expected result is invalid.`);
    const result = deriveSec0Case({ ...testCase, operation: merge(corpus.fixtures?.[testCase.fixture], testCase.patch) });
    kinds.add(result.axis);
  }
  for (const kind of Object.keys(REDUCERS)) if (!kinds.has(kind)) errors.push(`SEC0 operation kind ${kind} is missing.`);
  return { errors, results: deriveSec0(corpus) };
}
