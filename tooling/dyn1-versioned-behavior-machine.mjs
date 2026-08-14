import crypto from "node:crypto";
import fs from "node:fs";

/*
 * DYN1 is a host oracle.  It consumes receipts and events.  It does not
 * implement W, and it does not trust caller outcome fields.
 */

export const FORBIDDEN_INPUT_KEYS = new Set([
  "status",
  "route",
  "compatible",
  "published",
  "drained",
  "rollback",
  "authority",
  "providerOutcome",
]);

export const REQUIRED_CASES = [
  "DYN1-A-repl-snapshot",
  "DYN1-A-repl-invalidation",
  "DYN1-A-repl-invalid-preserves-old",
  "DYN1-A-repl-post-switch-degraded",
  "DYN1-A-repl-preflight-resource-failure",
  "DYN1-A-repl-stale-completion",
  "DYN1-A-repl-stale-message",
  "DYN1-A-repl-export-import",
  "DYN1-A-export-stale-receipt",
  "DYN1-A-export-missing-receipt",
  "DYN1-A-export-duplicate-receipt",
  "DYN1-A-export-forged-receipt",
  "DYN1-A-export-stale-source-map",
  "DYN1-A-import-live-state-rejected",
  "DYN1-A-crash-before-publication",
  "DYN1-A-crash-after-publication",
  "DYN1-A-provider-rollback-receipt",
  "DYN1-A-schema-exact-mismatch",
  "DYN1-A-schema-add-optional",
  "DYN1-A-quota-failure",
  "DYN1-A-committed-inspector",
  "DYN1-B-local-plugin-generation",
  "DYN1-B-paired-local-split",
  "DYN1-B-interface-exact-drift",
  "DYN1-B-interface-compatible-drift",
  "DYN1-B-wabi-mismatch",
  "DYN1-B-runtime-closure-new-generation",
  "DYN1-B-capability-exact",
  "DYN1-B-hidden-capability",
  "DYN1-B-effect-undeclared",
  "DYN1-B-revoked-old-capability",
  "DYN1-B-old-completion",
  "DYN1-B-old-message",
  "DYN1-B-service-drain-order",
  "DYN1-B-unload-live-callback",
  "DYN1-B-unload-after-drain",
  "DYN1-B-native-exact-mapping-retained",
  "DYN1-B-process-isolation",
  "DYN1-B-wasm-isolation",
  "DYN1-B-component-isolation",
  "DYN1-B-target-pair-equivalence",
  "DYN1-B-target-specific-abi",
  "DYN1-B-physical-trace-differs",
  "DYN1-B-concurrent-selection",
  "DYN1-B-stale-service-reference",
  "DYN1-B-plugin-crash-unknown",
  "DYN1-B-cancel-before-ready",
  "DYN1-B-post-switch-drain-degraded",
  "DYN1-B-provider-rollback",
  "DYN1-C-persistent-generation-reference",
  "DYN1-C-persistent-reference-write-rejected",
  "DYN1-A-crash-before-publication-unknown-effect",
  "DYN1-A-rollback-forged-receipt",
  "DYN1-A-rollback-after-switch-rejected",
  "DYN1-A-phase-duplicate-switch-rejected",
  "DYN1-A-import-parse-missing",
  "DYN1-B-late-callback-rejected",
  "DYN1-B-selection-duplicate-rejected",
  "DYN1-D-forged-mechanism-invalid",
  "DYN1-D-eval-rejected",
  "DYN1-D-exec-rejected",
  "DYN1-D-monkey-patch-rejected",
  "DYN1-D-active-frame-write-rejected",
  "DYN1-D-debugger-write-rejected",
  "DYN1-D-ambient-lookup-rejected",
  "DYN1-D-native-in-process-sandbox-rejected",
  "DYN1-D-dlclose-live-callback-rejected",
  "DYN1-D-arbitrary-eval-rejected",
  "DYN1-D-string-authority-rejected",
  "DYN1-D-current-module-injection-rejected",
];

export const CLEANUP_STEPS = [
  "cancelChildren",
  "drainWaits",
  "drainLoans",
  "drainStreams",
  "drainCallbacks",
  "drainResources",
  "unregister",
  "inFlightDrain",
  "destroy",
  "unpin",
  "release",
  "unmap",
];

/* Native exact-WAbi libraries retain their mapping until the runtime island
 * ends.  They still require every admission/in-flight/destruction step. */
export const NATIVE_CLEANUP_STEPS = CLEANUP_STEPS.slice(0, -1);

export const TRACE_LIBRARY = {
  commit: [
    "prepare",
    "validate",
    "preflight",
    "ready",
    "switch",
    "closeAdmission",
    "cancel:children",
    "drain:waits",
    "drain:loans",
    "drain:streams",
    "drain:callbacks",
    "drain:resources",
    "unregister",
    "inFlightDrain",
    "destroy",
    "unpin",
    "release",
    "unmap",
  ],
  prepare: ["prepare", "validate", "preflight", "ready"],
  commitTail: ["switch", "closeAdmission", "cancel:children", "drain:waits", "drain:loans", "drain:streams", "drain:callbacks", "drain:resources", "unregister", "inFlightDrain", "destroy", "unpin", "release", "unmap"],
};

export const MECHANISMS = Object.freeze({
  eval: { family: "dynamic-evaluation", invariant: "no-arbitrary-code", disposition: "intentionally-rejected" },
  exec: { family: "dynamic-evaluation", invariant: "no-arbitrary-code", disposition: "intentionally-rejected" },
  monkeyPatch: { family: "active-code-mutation", invariant: "no-active-frame-mutation", disposition: "intentionally-rejected" },
  activeFrameWrite: { family: "active-frame-write", invariant: "no-active-frame-mutation", disposition: "intentionally-rejected" },
  debuggerWrite: { family: "active-frame-write", invariant: "no-debugger-write", disposition: "intentionally-rejected" },
  ambientLookup: { family: "ambient-authority", invariant: "no-ambient-authority", disposition: "intentionally-rejected" },
  nativeInProcessSandbox: { family: "native-isolation", invariant: "native-is-not-sandbox", disposition: "intentionally-rejected" },
  dlcloseLiveCallback: { family: "ffi-lifetime", invariant: "callback-must-drain", disposition: "intentionally-rejected" },
  arbitraryRuntimeEval: { family: "dynamic-evaluation", invariant: "no-arbitrary-code", disposition: "intentionally-rejected" },
  stringAuthority: { family: "authority-by-name", invariant: "no-string-authority", disposition: "intentionally-rejected" },
  currentModuleInjection: { family: "active-code-mutation", invariant: "no-module-injection", disposition: "intentionally-rejected" },
});

const REQUIRED_FACTS = {
  recipe: ["key", "version", "toolArtifactDigest"],
  artifact: ["key", "indexDigest", "lockDigest", "sourceDigest", "provenanceDigest", "logicalArtifact", "physicalArtifactDigest"],
  source: ["sourceDigest", "packageLockDigest", "logicalSource"],
  interface: ["semanticInterfaceKey", "serviceIRKey", "wAbiKey", "runtimeClosureKey", "documentationKey", "sourceMapKey", "schemaDigest", "old", "candidate", "wAbiReceipt", "serviceIRReceipt", "runtimeClosureReceipt"],
  schema: ["policy", "old", "candidate"],
  generations: ["old", "next"],
  target: ["targetId", "family", "registryDigest", "abiDigest", "physicalArtifactDigest"],
  capabilityFacts: ["declared", "grants", "revoked", "generation"],
  effectFacts: ["declared", "observed"],
  exportFacts: ["provenanceDigest", "redactions", "bytes", "maximumBytes"],
  provider: ["identity", "transaction", "frontierDigest"],
};

const IDENTITY_DIGEST_PATHS = [
  ["recipe", "key"],
  ["artifact", "key"],
  ["artifact", "indexDigest"],
  ["artifact", "lockDigest"],
  ["artifact", "provenanceDigest"],
  ["source", "sourceDigest"],
  ["source", "packageLockDigest"],
  ["interface", "semanticInterfaceKey"],
  ["interface", "serviceIRKey"],
  ["interface", "wAbiKey"],
  ["interface", "runtimeClosureKey"],
  ["interface", "documentationKey"],
  ["interface", "sourceMapKey"],
  ["interface", "schemaDigest"],
  ["target", "registryDigest"],
  ["target", "abiDigest"],
  ["target", "physicalArtifactDigest"],
];

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function deepMerge(base, override) {
  if (!isObject(base)) return structuredClone(override);
  const result = structuredClone(base);
  if (!isObject(override)) return result;
  for (const [key, value] of Object.entries(override)) {
    result[key] = isObject(value) && isObject(result[key]) ? deepMerge(result[key], value) : structuredClone(value);
  }
  return result;
}

/* Recursive canonical encoding.  Object keys are sorted at every depth. */
export function canonical(value) {
  if (Array.isArray(value)) return `[${value.map((item) => canonical(item)).join(",")}]`;
  if (isObject(value)) {
    return `{${Object.keys(value).sort().map((key) => `${JSON.stringify(key)}:${canonical(value[key])}`).join(",")}}`;
  }
  return JSON.stringify(value);
}

export function canonicalDigest(value) {
  return `sha256:${crypto.createHash("sha256").update(canonical(value)).digest("hex")}`;
}

/* A compatible ServiceIR receipt carries a digest of the field relation that
 * the old and candidate schema records prove.  The relation is derived from
 * the records, not accepted as a caller-supplied map. */
export function deriveCompatibilityMapDigest(oldSchema, candidateSchema) {
  if (!oldSchema || !candidateSchema || canonical(oldSchema) === canonical(candidateSchema)) return null;
  const oldFields = new Map((oldSchema.fields ?? []).map((field) => [field.name, field]));
  const candidateFields = new Map((candidateSchema.fields ?? []).map((field) => [field.name, field]));
  const names = [...new Set([...oldFields.keys(), ...candidateFields.keys()])].sort();
  const relation = names.map((name) => {
    const oldField = oldFields.get(name) ?? null;
    const candidateField = candidateFields.get(name) ?? null;
    const relationKind = !oldField ? "added" : !candidateField ? "removed" : canonical(oldField) === canonical(candidateField) ? "preserved" : "changed";
    return { name, relation: relationKind, old: oldField, candidate: candidateField };
  });
  return canonicalDigest({ oldDigest: oldSchema.digest, candidateDigest: candidateSchema.digest, relation });
}

export function validDigest(value) {
  return typeof value === "string" && /^sha256:[0-9a-f]{64}$/u.test(value);
}

function valueAt(root, path) {
  return path.split(".").reduce((value, key) => value?.[key], root);
}

function requireFields(root, section, fields, errors, location = section) {
  if (!isObject(root?.[section])) {
    errors.push(`${location} must be a complete object.`);
    return;
  }
  for (const field of fields) if (!(field in root[section])) errors.push(`${location}.${field} is required.`);
}

function validString(value) {
  return typeof value === "string" && value.trim() !== "";
}

function checkUnknownKeys(value, allowed, location, errors) {
  if (!isObject(value)) return;
  for (const key of Object.keys(value)) if (!allowed.has(key)) errors.push(`${location}.${key} is not in the closed facts schema.`);
}

function validReceipt(receipt, fields = []) {
  return isObject(receipt) && fields.every((field) => validString(receipt[field]));
}

function validateRecord(record, location, errors) {
  if (!isObject(record) || !validDigest(record.digest) || !Array.isArray(record.fields)) {
    errors.push(`${location} must contain a digest and fields.`);
    return;
  }
  checkUnknownKeys(record, new Set(["digest", "fields"]), location, errors);
  const names = new Set();
  for (const [index, field] of record.fields.entries()) {
    checkUnknownKeys(field, new Set(["name", "type", "optional"]), `${location}.fields[${index}]`, errors);
    if (!isObject(field) || !validString(field.name) || !validString(field.type) || typeof field.optional !== "boolean") {
      errors.push(`${location}.fields[${index}] must contain name, type, and optional.`);
    }
    if (names.has(field?.name)) errors.push(`${location}.fields duplicates ${field?.name}.`);
    names.add(field?.name);
  }
}

function validateInterfaceRecord(record, location, errors) {
  const fields = ["semanticInterfaceKey", "serviceIRKey", "wAbiKey", "runtimeClosureKey", "schemaDigest"];
  if (!isObject(record) || fields.some((field) => !validDigest(record[field]))) errors.push(`${location} must bind semantic, ServiceIR, WAbi, runtime, and schema digests.`);
}

function validateCapabilityEntries(entries, location, errors) {
  if (!Array.isArray(entries) || entries.length === 0) {
    errors.push(`${location} must contain structured capability entries.`);
    return;
  }
  const ids = new Set();
  for (const [index, entry] of entries.entries()) {
    checkUnknownKeys(entry, new Set(["id", "interfaceKey", "generation", "artifactKey", "scope"]), `${location}[${index}]`, errors);
    if (!isObject(entry) || !validString(entry.id) || !validDigest(entry.interfaceKey) || !validString(entry.generation) || !validDigest(entry.artifactKey) || !Array.isArray(entry.scope)) {
      errors.push(`${location}[${index}] must bind id, interface, generation, artifact, and scope.`);
    }
    if (!Array.isArray(entry?.scope) || entry.scope.length === 0 || entry.scope.some((item) => !validString(item)) || new Set(entry.scope).size !== entry.scope.length) errors.push(`${location}[${index}].scope must contain unique non-empty rights.`);
    if (ids.has(entry?.id)) errors.push(`${location} contains duplicate ${entry?.id}.`);
    ids.add(entry?.id);
  }
}

function validateEffectEntries(entries, location, errors) {
  if (!Array.isArray(entries) || entries.length === 0) {
    errors.push(`${location} must contain structured effect entries.`);
    return;
  }
  const ids = new Set();
  const rights = new Set();
  for (const [index, entry] of entries.entries()) {
    checkUnknownKeys(entry, new Set(["id", "right", "interfaceKey", "generation"]), `${location}[${index}]`, errors);
    if (!isObject(entry) || !validString(entry.id) || !validString(entry.right) || !validDigest(entry.interfaceKey) || !validString(entry.generation)) errors.push(`${location}[${index}] must bind id, right, interface, and generation.`);
    if (ids.has(entry?.id)) errors.push(`${location} contains duplicate ${entry?.id}.`);
    if (rights.has(entry?.right)) errors.push(`${location} contains duplicate right ${entry?.right}.`);
    ids.add(entry?.id);
    rights.add(entry?.right);
  }
}

function validateFactSchema(facts, errors, location = "facts") {
  if (!isObject(facts)) {
    errors.push(`${location} must be a complete object.`);
    return errors;
  }
  for (const [section, fields] of Object.entries(REQUIRED_FACTS)) requireFields(facts, section, fields, errors, `${location}.${section}`);
  checkUnknownKeys(facts, new Set([...Object.keys(REQUIRED_FACTS), "targets", "isolation", "cleanupPlan", "nativeCleanupPlan", "limits", "researchGap"]), location, errors);
  checkUnknownKeys(facts.recipe, new Set(REQUIRED_FACTS.recipe), `${location}.recipe`, errors);
  checkUnknownKeys(facts.artifact, new Set(REQUIRED_FACTS.artifact), `${location}.artifact`, errors);
  checkUnknownKeys(facts.source, new Set(REQUIRED_FACTS.source), `${location}.source`, errors);
  checkUnknownKeys(facts.interface, new Set(REQUIRED_FACTS.interface), `${location}.interface`, errors);
  for (const record of ["old", "candidate"]) checkUnknownKeys(facts.interface?.[record], new Set(["semanticInterfaceKey", "serviceIRKey", "wAbiKey", "runtimeClosureKey", "schemaDigest"]), `${location}.interface.${record}`, errors);
  checkUnknownKeys(facts.interface?.wAbiReceipt, new Set(["targetId", "wAbiKey", "abiDigest", "artifactDigest", "decision"]), `${location}.interface.wAbiReceipt`, errors);
  checkUnknownKeys(facts.interface?.serviceIRReceipt, new Set(["oldServiceIRKey", "candidateServiceIRKey", "oldSchemaDigest", "candidateSchemaDigest", "compatibilityMapDigest", "decision"]), `${location}.interface.serviceIRReceipt`, errors);
  checkUnknownKeys(facts.interface?.runtimeClosureReceipt, new Set(["oldKey", "candidateKey", "providerIdentity", "generation", "decision"]), `${location}.interface.runtimeClosureReceipt`, errors);
  checkUnknownKeys(facts.schema, new Set(REQUIRED_FACTS.schema), `${location}.schema`, errors);
  checkUnknownKeys(facts.generations, new Set(REQUIRED_FACTS.generations), `${location}.generations`, errors);
  checkUnknownKeys(facts.target, new Set(REQUIRED_FACTS.target), `${location}.target`, errors);
  for (const [id, target] of Object.entries(facts.targets ?? {})) {
    checkUnknownKeys(target, new Set([...REQUIRED_FACTS.target, "wAbiReceipt"]), `${location}.targets.${id}`, errors);
    checkUnknownKeys(target?.wAbiReceipt, new Set(["targetId", "wAbiKey", "abiDigest", "artifactDigest", "decision"]), `${location}.targets.${id}.wAbiReceipt`, errors);
  }
  checkUnknownKeys(facts.capabilityFacts, new Set(REQUIRED_FACTS.capabilityFacts), `${location}.capabilityFacts`, errors);
  checkUnknownKeys(facts.effectFacts, new Set(REQUIRED_FACTS.effectFacts), `${location}.effectFacts`, errors);
  checkUnknownKeys(facts.exportFacts, new Set(REQUIRED_FACTS.exportFacts), `${location}.exportFacts`, errors);
  checkUnknownKeys(facts.provider, new Set(REQUIRED_FACTS.provider), `${location}.provider`, errors);
  checkUnknownKeys(facts.limits, new Set(["maximumEvents", "maximumExportBytes"]), `${location}.limits`, errors);
  if (isObject(facts.researchGap)) {
    checkUnknownKeys(facts.researchGap, new Set(["operation", "providerIdentity", "interfaceKey", "schemaDigest", "rights", "stableDomainKey", "savedGenerationPolicy", "currentContract", "languageSurface", "resolveReceipt"]), `${location}.researchGap`, errors);
    checkUnknownKeys(facts.researchGap.resolveReceipt, new Set(["providerIdentity", "interfaceKey", "schemaDigest", "decision", "digest"]), `${location}.researchGap.resolveReceipt`, errors);
  }
  if (!validString(facts.recipe?.version) || !validDigest(facts.recipe?.toolArtifactDigest)) errors.push(`${location}.recipe must contain a version and tool artifact digest.`);
  for (const section of ["recipe", "artifact", "source", "interface", "target", "provider"]) {
    if (!isObject(facts[section])) continue;
  }
  validateRecord(facts.schema?.old, `${location}.schema.old`, errors);
  validateRecord(facts.schema?.candidate, `${location}.schema.candidate`, errors);
  validateInterfaceRecord(facts.interface?.old, `${location}.interface.old`, errors);
  validateInterfaceRecord(facts.interface?.candidate, `${location}.interface.candidate`, errors);
  if (!isObject(facts.generations) || !validString(facts.generations.old) || !validString(facts.generations.next) || facts.generations.old === facts.generations.next) errors.push(`${location}.generations must contain distinct old and next generations.`);
  if (!facts.schema || !["exact", "compatible"].includes(facts.schema.policy)) errors.push(`${location}.schema.policy must be exact or compatible.`);
  if (!validDigest(facts.interface?.old?.semanticInterfaceKey) || !validDigest(facts.interface?.candidate?.semanticInterfaceKey)) errors.push(`${location}.interface semantic records require digests.`);
  if (!validReceipt(facts.interface?.wAbiReceipt, ["targetId", "wAbiKey", "abiDigest", "artifactDigest", "decision"]) || !validDigest(facts.interface.wAbiReceipt.wAbiKey) || !validDigest(facts.interface.wAbiReceipt.abiDigest) || !validDigest(facts.interface.wAbiReceipt.artifactDigest) || facts.interface.wAbiReceipt.decision !== "validated") errors.push(`${location}.interface.wAbiReceipt is invalid.`);
  const serviceIRReceipt = facts.interface?.serviceIRReceipt;
  if (!validReceipt(serviceIRReceipt, ["oldServiceIRKey", "candidateServiceIRKey", "oldSchemaDigest", "candidateSchemaDigest", "decision"]) || !("compatibilityMapDigest" in (serviceIRReceipt ?? {})) || !validDigest(serviceIRReceipt.oldServiceIRKey) || !validDigest(serviceIRReceipt.candidateServiceIRKey) || !validDigest(serviceIRReceipt.oldSchemaDigest) || !validDigest(serviceIRReceipt.candidateSchemaDigest) || (serviceIRReceipt.compatibilityMapDigest !== null && !validDigest(serviceIRReceipt.compatibilityMapDigest)) || !["exact", "compatible"].includes(serviceIRReceipt.decision)) errors.push(`${location}.interface.serviceIRReceipt is invalid.`);
  if (!validReceipt(facts.interface?.runtimeClosureReceipt, ["oldKey", "candidateKey", "providerIdentity", "generation", "decision"]) || facts.interface.runtimeClosureReceipt.decision !== "resolved") errors.push(`${location}.interface.runtimeClosureReceipt is invalid.`);
  if (!Array.isArray(facts.cleanupPlan) || JSON.stringify(facts.cleanupPlan) !== JSON.stringify(CLEANUP_STEPS)) errors.push(`${location}.cleanupPlan must be the complete ordered cleanup plan.`);
  if (!Array.isArray(facts.nativeCleanupPlan) || JSON.stringify(facts.nativeCleanupPlan) !== JSON.stringify(NATIVE_CLEANUP_STEPS)) errors.push(`${location}.nativeCleanupPlan must retain native mappings after the complete ordered drain.`);
  if (!isObject(facts.limits) || !Number.isInteger(facts.limits.maximumEvents) || facts.limits.maximumEvents < 1 || !Number.isInteger(facts.limits.maximumExportBytes) || facts.limits.maximumExportBytes < 1) errors.push(`${location}.limits must contain positive integer event and export bounds.`);
  if (!Array.isArray(facts.capabilityFacts?.revoked) || !validString(facts.capabilityFacts?.generation)) errors.push(`${location}.capabilityFacts revocation and generation fields are required.`);
  validateCapabilityEntries(facts.capabilityFacts?.declared, `${location}.capabilityFacts.declared`, errors);
  validateCapabilityEntries(facts.capabilityFacts?.grants, `${location}.capabilityFacts.grants`, errors);
  validateEffectEntries(facts.effectFacts?.declared, `${location}.effectFacts.declared`, errors);
  if (!Array.isArray(facts.effectFacts?.observed) || facts.effectFacts.observed.length !== 0) errors.push(`${location}.effectFacts.observed must be exactly [] because providerReceipt events are the only operational observations.`);
  if (!validDigest(facts.exportFacts?.provenanceDigest) || !Array.isArray(facts.exportFacts?.redactions) || !Number.isInteger(facts.exportFacts?.bytes) || !Number.isInteger(facts.exportFacts?.maximumBytes)) errors.push(`${location}.exportFacts must contain provenance, redactions, and bounds.`);
  if (!validString(facts.isolation)) errors.push(`${location}.isolation is required.`);
  if (isObject(facts.researchGap) && (!Array.isArray(facts.researchGap.rights) || facts.researchGap.rights.length === 0 || facts.researchGap.rights.some((right) => !validString(right)) || new Set(facts.researchGap.rights).size !== facts.researchGap.rights.length)) errors.push(`${location}.researchGap.rights must contain unique rights.`);
  if (facts.artifact?.physicalPath || facts.artifact?.path || facts.artifact?.name) errors.push(`${location}.artifact cannot use host path or name lookup.`);
  return errors;
}

export function validateFactsSchema(facts, { location = "facts" } = {}) {
  return validateFactSchema(facts, [], location);
}

function validateOverrideShape(override, errors, location) {
  if (override === undefined) return;
  if (!isObject(override)) {
    errors.push(`${location} must be an object.`);
    return;
  }
  for (const [section, fields] of Object.entries(REQUIRED_FACTS)) {
    if (section in override && isObject(override[section])) {
      for (const field of fields) if (!(field in override[section])) errors.push(`${location}.${section}.${field} is required for a complete mutation.`);
    }
  }
  const requireNested = (value, fields, nestedLocation) => {
    if (!isObject(value)) return;
    for (const field of fields) if (!(field in value)) errors.push(`${nestedLocation}.${field} is required for a complete mutation.`);
  };
  if (isObject(override.schema)) {
    requireNested(override.schema.old, ["digest", "fields"], `${location}.schema.old`);
    requireNested(override.schema.candidate, ["digest", "fields"], `${location}.schema.candidate`);
    for (const record of ["old", "candidate"]) for (const [index, field] of (override.schema[record]?.fields ?? []).entries()) requireNested(field, ["name", "type", "optional"], `${location}.schema.${record}.fields[${index}]`);
  }
  if (isObject(override.interface)) {
    for (const record of ["old", "candidate"]) requireNested(override.interface[record], ["semanticInterfaceKey", "serviceIRKey", "wAbiKey", "runtimeClosureKey", "schemaDigest"], `${location}.interface.${record}`);
    requireNested(override.interface.wAbiReceipt, ["targetId", "wAbiKey", "abiDigest", "artifactDigest", "decision"], `${location}.interface.wAbiReceipt`);
    requireNested(override.interface.serviceIRReceipt, ["oldServiceIRKey", "candidateServiceIRKey", "oldSchemaDigest", "candidateSchemaDigest", "compatibilityMapDigest", "decision"], `${location}.interface.serviceIRReceipt`);
    requireNested(override.interface.runtimeClosureReceipt, ["oldKey", "candidateKey", "providerIdentity", "generation", "decision"], `${location}.interface.runtimeClosureReceipt`);
  }
  for (const section of ["capabilityFacts", "effectFacts", "exportFacts"]) {
    if (!isObject(override[section])) continue;
    for (const field of REQUIRED_FACTS[section]) if (!(field in override[section])) errors.push(`${location}.${section}.${field} is required for a complete mutation.`);
  }
  for (const section of ["capabilityFacts", "effectFacts"]) {
    const arrays = override[section];
    for (const field of ["declared", "grants"]) if (Array.isArray(arrays?.[field])) for (const [index, entry] of arrays[field].entries()) {
      const fields = section === "capabilityFacts" ? ["id", "interfaceKey", "generation", "artifactKey", "scope"] : ["id", "right", "interfaceKey", "generation"];
      requireNested(entry, fields, `${location}.${section}.${field}[${index}]`);
    }
  }
}

export function normaliseFacts(corpus, testCase) {
  if (!isObject(corpus?.defaults)) return undefined;
  const facts = deepMerge(corpus.defaults, testCase?.facts ?? {});
  if (testCase?.facts?.target && !testCase?.facts?.interface?.wAbiReceipt && isObject(facts.targets)) {
    const targetReceipt = Object.values(facts.targets).find((target) => target?.targetId === facts.target?.targetId)?.wAbiReceipt;
    if (targetReceipt) {
      facts.interface.wAbiReceipt = structuredClone(targetReceipt);
      facts.interface.wAbiKey = targetReceipt.wAbiKey;
      facts.interface.old.wAbiKey = targetReceipt.wAbiKey;
      facts.interface.candidate.wAbiKey = targetReceipt.wAbiKey;
    }
  }
  return facts;
}

function eventObject(raw) {
  if (isObject(raw)) return structuredClone(raw);
  if (typeof raw !== "string") return { op: "invalid" };
  const [head, ...tail] = raw.split(":");
  if (head === "drain" && tail[0]) return { op: "drain", kind: tail[0] };
  if (head === "cancel" && tail[0]) return { op: "cancel", kind: tail[0] };
  if (["oldCompletion", "oldMessage", "oldCapability", "newCompletion"].includes(head)) return { op: head, generation: tail[0] };
  if (head === "effect" && tail.length >= 2) return { op: "effect", id: tail[0], right: tail[1] };
  if (head === "inspectCommitted") return { op: head, mode: "readOnly", right: "metadata.read" };
  return { op: head };
}

function expandEvents(events) {
  if (!Array.isArray(events)) return events;
  return events.flatMap((event) => typeof event === "string" && event.startsWith("@") ? (TRACE_LIBRARY[event.slice(1)] ?? [event]) : [event]);
}

function visitInput(value, location, errors) {
  if (Array.isArray(value)) {
    value.forEach((item, index) => visitInput(item, `${location}[${index}]`, errors));
    return;
  }
  if (!isObject(value)) return;
  for (const [key, child] of Object.entries(value)) {
    if (FORBIDDEN_INPUT_KEYS.has(key)) errors.push(`${location}.${key} is a forbidden caller outcome claim.`);
    visitInput(child, `${location}.${key}`, errors);
  }
}

function pushError(state, code, reason, phase = state.phase) {
  if (!state.error) state.error = { code, reason, phase };
  state.failed = true;
}

function identityFactsValid(facts, state) {
  for (const [section, field] of IDENTITY_DIGEST_PATHS) if (!validDigest(facts?.[section]?.[field])) {
    pushError(state, "identity-facts-invalid", `${section}.${field} must be a lowercase SHA-256 receipt.`);
    return false;
  }
  const interfaceKeys = ["semanticInterfaceKey", "serviceIRKey", "wAbiKey", "runtimeClosureKey", "documentationKey", "sourceMapKey", "schemaDigest"].map((key) => facts.interface[key]);
  if (new Set(interfaceKeys).size !== interfaceKeys.length) pushError(state, "identity-collision", "SemanticInterfaceKey, ServiceIRKey, WAbiKey, RuntimeClosureKey, documentation, map, and schema keys must remain distinct.");
  const structural = [facts.recipe.key, facts.artifact.key, facts.interface.semanticInterfaceKey, facts.interface.wAbiKey, facts.interface.runtimeClosureKey];
  if (new Set(structural).size !== structural.length) pushError(state, "identity-collision", "recipe, artifact, semantic interface, WAbi, and runtime closure identities must remain distinct.");
  return !state.error;
}

function initialState(facts, mode) {
  const oldGeneration = facts.generations.old;
  const nextGeneration = facts.generations.next;
  return {
    mode,
    phase: "idle",
    oldGeneration,
    nextGeneration,
    activeGeneration: oldGeneration,
    publication: "none",
    oldAdmission: "open",
    switched: false,
    failed: false,
    terminal: false,
    boundaryViolation: false,
    providerRollback: false,
    rollbackReceipt: null,
    crash: null,
    error: null,
    postSwitchDrainFailure: null,
    cleanup: [],
    staleRejections: [],
    acceptedCompletions: 0,
    selection: null,
    selectionReceipt: null,
    readyReceipts: [],
    effects: [],
    exportDigest: null,
    imported: false,
    inspected: false,
    migration: false,
    research: false,
    capabilityState: "declared",
    ownerGraph: [`old:${oldGeneration}`],
    interfaceResult: "exact",
    validatedReceipts: [],
    callbackRegistered: false,
    callbackInFlight: 0,
    unregistered: false,
    ffiRelease: null,
    physicalTrace: mode === "split" ? ["wire-prepare", "service-switch", "wire-drain"] : ["in-process-stage", "in-process-drain"],
  };
}

function requiredCleanup(facts) {
  return facts.isolation === "native" ? facts.nativeCleanupPlan : facts.cleanupPlan;
}

function cleanupStepFor(event) {
  if (event.op === "cancel") return `cancel${String(event.kind ?? "")[0]?.toUpperCase() ?? ""}${String(event.kind ?? "").slice(1)}`;
  if (event.op === "drain") return `drain${String(event.kind ?? "")[0]?.toUpperCase() ?? ""}${String(event.kind ?? "").slice(1)}`;
  if (["unregister", "inFlightDrain", "destroy", "unpin", "release", "unmap"].includes(event.op)) return event.op;
  return undefined;
}

function applyLocalCleanup(state, facts, event) {
  const step = cleanupStepFor(event);
  if (!step) return false;
  if (!state.switched || state.oldAdmission !== "closed") {
    pushError(state, "cleanup-before-admission-close", "cleanup requires publication and closed old admission");
    return true;
  }
  if (event.generation && event.generation !== state.oldGeneration) {
    pushError(state, "cleanup-generation-mismatch", "cleanup event must name the old generation");
    return true;
  }
  const expected = requiredCleanup(facts)[state.cleanup.length];
  if (expected !== step) {
    pushError(state, "cleanup-order", `expected ${expected ?? "complete"}, observed ${step}`);
    return true;
  }
  state.cleanup.push(step);
  if (step === "unregister") {
    state.callbackRegistered = false;
    state.unregistered = true;
  }
  if (step === "inFlightDrain" && state.callbackInFlight !== 0) pushError(state, "inFlight-not-drained", "in-flight callbacks must be zero before destroy");
  if (state.cleanup.length === requiredCleanup(facts).length) state.ownerGraph = [`new:${state.nextGeneration}`];
  return true;
}

/* Keep split-service cleanup operationally independent.  The step vocabulary
 * and pure order helper are shared, but this reducer owns its own mutations so
 * a local cleanup defect cannot be hidden by a common handler. */
function applySplitCleanup(state, facts, event) {
  const step = cleanupStepFor(event);
  if (!step) return false;
  if (!state.switched || state.oldAdmission !== "closed") {
    pushError(state, "cleanup-before-admission-close", "split cleanup requires publication and closed old admission");
    return true;
  }
  if (event.generation && event.generation !== state.oldGeneration) {
    pushError(state, "cleanup-generation-mismatch", "split cleanup event must name the old generation");
    return true;
  }
  const expected = requiredCleanup(facts)[state.cleanup.length];
  if (expected !== step) {
    pushError(state, "cleanup-order", `split expected ${expected ?? "complete"}, observed ${step}`);
    return true;
  }
  state.cleanup.push(step);
  if (step === "unregister") {
    state.callbackRegistered = false;
    state.unregistered = true;
  }
  if (step === "inFlightDrain" && state.callbackInFlight !== 0) pushError(state, "inFlight-not-drained", "split in-flight callbacks must be zero before destroy");
  if (state.cleanup.length === requiredCleanup(facts).length) state.ownerGraph = [`new:${state.nextGeneration}`];
  return true;
}

function validateSchema(facts, state) {
  const schema = facts.schema;
  if (schema.policy === "exact") {
    if (canonical(schema.old) !== canonical(schema.candidate)) pushError(state, "schema-exact-mismatch", "exact policy requires equal old and candidate schema records");
    else state.interfaceResult = "exact";
    return;
  }
  const oldFields = new Map(schema.old.fields.map((field) => [field.name, field]));
  for (const oldField of schema.old.fields) {
    const candidate = schema.candidate.fields.find((field) => field.name === oldField.name);
    if (!candidate || candidate.type !== oldField.type || candidate.optional !== oldField.optional) {
      pushError(state, "schema-incompatible", "compatible policy does not permit removal or incompatible change of an existing field");
      return;
    }
  }
  for (const candidate of schema.candidate.fields) if (!oldFields.has(candidate.name) && !candidate.optional) {
    pushError(state, "schema-incompatible", "new fields must be optional under compatible policy");
    return;
  }
  state.interfaceResult = canonical(schema.old) === canonical(schema.candidate) ? "exact" : "compatible";
}

function validateCapabilityFacts(facts, state) {
  const caps = facts.capabilityFacts;
  const declared = new Map(caps.declared.map((entry) => [entry.id, entry]));
  const revoked = new Set(caps.revoked);
  for (const grant of caps.grants) {
    const source = declared.get(grant.id);
    if (!source || source.interfaceKey !== grant.interfaceKey || source.generation !== grant.generation || source.artifactKey !== grant.artifactKey || grant.scope.some((item) => !source.scope.includes(item))) {
      pushError(state, "capability-grant-mismatch", "grants must be an attenuation subset of declared rights bound to interface, generation, and artifact");
      return;
    }
  }
  for (const effect of facts.effectFacts.declared) {
    const grant = caps.grants.find((item) => item.id === effect.right && item.interfaceKey === effect.interfaceKey && item.generation === effect.generation);
    if (!grant) continue;
    if (revoked.has(`${effect.generation}:${effect.right}`)) pushError(state, "capability-revoked", `capability ${effect.right} is revoked for ${effect.generation}`);
  }
}

function validateInterface(facts, state, mode) {
  const oldRecord = facts.interface.old;
  const candidateRecord = facts.interface.candidate;
  const topLevelCandidate = ["semanticInterfaceKey", "serviceIRKey", "wAbiKey", "runtimeClosureKey", "schemaDigest"];
  if (topLevelCandidate.some((key) => facts.interface[key] !== candidateRecord[key])) {
    pushError(state, "interface-authority-duplicate", "top-level interface authority must exactly equal the candidate interface record");
    return;
  }
  if (oldRecord.schemaDigest !== facts.schema.old.digest || candidateRecord.schemaDigest !== facts.schema.candidate.digest) {
    pushError(state, "schema-receipt-mismatch", "interface records must bind the old and candidate schema receipts");
    return;
  }
  const schemaChanged = canonical(facts.schema.old) !== canonical(facts.schema.candidate);
  if (facts.schema.policy === "exact" && (oldRecord.semanticInterfaceKey !== candidateRecord.semanticInterfaceKey || oldRecord.serviceIRKey !== candidateRecord.serviceIRKey)) {
    pushError(state, "semantic-interface-mismatch", "exact policy requires equal semantic interface and ServiceIR identities");
    return;
  }
  if (facts.schema.policy === "compatible" && schemaChanged && (oldRecord.semanticInterfaceKey === candidateRecord.semanticInterfaceKey || oldRecord.serviceIRKey === candidateRecord.serviceIRKey)) {
    pushError(state, "compatible-identity-stale", "a changed compatible schema requires new candidate SemanticInterfaceKey and ServiceIRKey identities");
    return;
  }
  const runtime = facts.interface.runtimeClosureReceipt;
  if (runtime.oldKey !== oldRecord.runtimeClosureKey || runtime.candidateKey !== candidateRecord.runtimeClosureKey || runtime.providerIdentity !== facts.provider.identity || runtime.generation !== facts.generations.next || runtime.decision !== "resolved") {
    pushError(state, "runtime-closure-receipt", "a new runtime closure requires a validated provider resolution receipt");
    return;
  }
  const abi = facts.interface.wAbiReceipt;
  if (!validDigest(abi.artifactDigest) || abi.artifactDigest !== facts.target.physicalArtifactDigest) pushError(state, "wabi-receipt-mismatch", "WAbi receipt must bind the exact target physical artifact");
  else if (abi.wAbiKey !== candidateRecord.wAbiKey || abi.wAbiKey !== oldRecord.wAbiKey) pushError(state, "wabi-mismatch", "WAbi receipt must bind both old and candidate interface records");
  if (mode === "local" && (abi.targetId !== facts.target.targetId || abi.abiDigest !== facts.target.abiDigest || abi.wAbiKey !== candidateRecord.wAbiKey)) pushError(state, "wabi-mismatch", "local W requires an exact host WAbi receipt");
  const service = facts.interface.serviceIRReceipt;
  const expectedDecision = schemaChanged ? "compatible" : "exact";
  const expectedCompatibilityMapDigest = facts.schema.policy === "compatible" && schemaChanged ? deriveCompatibilityMapDigest(facts.schema.old, facts.schema.candidate) : null;
  if (service.oldServiceIRKey !== oldRecord.serviceIRKey || service.candidateServiceIRKey !== candidateRecord.serviceIRKey || service.oldSchemaDigest !== facts.schema.old.digest || service.candidateSchemaDigest !== facts.schema.candidate.digest || service.decision !== expectedDecision || service.compatibilityMapDigest !== expectedCompatibilityMapDigest) pushError(state, "serviceir-receipt-mismatch", "ServiceIR receipt must bind old and candidate identities, schema digests, exact compatibility decision, and the derived compatibility map digest");
}

function validateTarget(facts, state) {
  if (facts.isolation === "native") {
    const abi = facts.interface.wAbiReceipt;
    if (abi.targetId !== facts.target.targetId || abi.abiDigest !== facts.target.abiDigest || abi.artifactDigest !== facts.target.physicalArtifactDigest || abi.decision !== "validated") {
      pushError(state, "native-wabi-mismatch", "native W requires an exact target WAbi receipt before admission");
      return;
    }
  } else if (!["process", "wasm", "component"].includes(facts.isolation)) {
    pushError(state, "native-not-sandbox", "a native dynamic library is not a sandbox");
    return;
  }
  if (!validDigest(facts.target.registryDigest) || !validDigest(facts.target.abiDigest) || !validDigest(facts.target.physicalArtifactDigest)) pushError(state, "target-facts-missing", "target projection requires registry, WAbi, and physical artifact receipts");
}

function deriveReceiptSet(facts) {
  return [
    { id: "source", digest: facts.source.sourceDigest },
    { id: "package-lock", digest: facts.source.packageLockDigest },
    { id: "recipe", digest: facts.recipe.key },
    { id: "artifact", digest: facts.artifact.key },
    { id: "interface", digest: facts.interface.semanticInterfaceKey },
    { id: "source-map", digest: facts.interface.sourceMapKey },
  ];
}

function validateExportFacts(facts, event, state) {
  if (state.publication !== "committed") {
    pushError(state, "export-before-commit", "export requires the committed publication");
    return;
  }
  const expected = state.validatedReceipts;
  const receipts = event.receipts ?? expected;
  if (!Array.isArray(receipts) || receipts.length !== expected.length) {
    pushError(state, "export-receipt-missing", "export requires the exact validated receipt set");
    return;
  }
  const ids = new Set();
  for (const receipt of receipts) {
    if (!isObject(receipt) || !validString(receipt.id) || ids.has(receipt.id)) {
      pushError(state, "export-receipt-duplicate", "export receipts must have unique IDs");
      return;
    }
    ids.add(receipt.id);
    const candidate = expected.find((item) => item.id === receipt.id);
    if (!candidate) {
      pushError(state, "export-receipt-forged", "export receipt is not in the validated committed set");
      return;
    }
    if (candidate.digest !== receipt.digest) {
      pushError(state, "export-receipt-stale", "export receipt digest is stale");
      return;
    }
  }
  if (event.sourceMapDigest && event.sourceMapDigest !== facts.interface.sourceMapKey) {
    pushError(state, "export-source-map-stale", "source-map digest does not match the committed interface");
    return;
  }
  if (event.packageLockDigest && event.packageLockDigest !== facts.source.packageLockDigest) {
    pushError(state, "export-lock-stale", "package-lock digest does not match the committed source");
    return;
  }
  const redactions = event.redactions ?? facts.exportFacts.redactions;
  const requiredRedactions = ["heap", "tasks", "loans", "capabilities", "serviceRefs", "providerHandles"];
  if (!Array.isArray(redactions) || requiredRedactions.some((item) => !redactions.includes(item))) {
    pushError(state, "export-redaction-incomplete", "export must redact heap, tasks, loans, capabilities, ServiceRefs, and provider handles");
    return;
  }
  const bytes = event.bytes ?? facts.exportFacts.bytes;
  const bound = event.maximumBytes ?? facts.exportFacts.maximumBytes ?? facts.limits.maximumExportBytes;
  if (!Number.isInteger(bytes) || bytes > bound) {
    pushError(state, "export-quota", "export exceeds its declared bound");
    return;
  }
  state.exportDigest = canonicalDigest({
    source: facts.source.sourceDigest,
    packageLock: facts.source.packageLockDigest,
    recipe: facts.recipe,
    artifact: facts.artifact,
    interface: facts.interface,
    receipts,
    provenance: facts.exportFacts.provenanceDigest,
    redactions,
    bounds: { bytes, maximumBytes: bound },
  });
  if (event.digest && event.digest !== state.exportDigest) pushError(state, "export-digest-mismatch", "export digest does not match canonical manifest");
}

function validateImport(event, state, facts) {
  if (!state.exportDigest) {
    pushError(state, "import-without-export", "import requires a committed export");
    return;
  }
  const liveFields = ["heap", "tasks", "loans", "capabilities", "serviceRefs", "providerHandles"];
  if (liveFields.some((field) => event.restore?.includes(field))) {
    pushError(state, "import-live-state", "import never restores live heap, tasks, loans, capabilities, ServiceRefs, or provider handles");
    return;
  }
  if (event.exportDigest && event.exportDigest !== state.exportDigest) {
    pushError(state, "import-digest-mismatch", "import digest does not match the export");
    return;
  }
  const required = ["reopen", "parse", "check", "resolveReceipts"];
  const steps = event.steps ?? required;
  if (!Array.isArray(steps) || JSON.stringify(steps) !== JSON.stringify(required)) {
    const missing = required.find((step) => !steps?.includes(step));
    pushError(state, missing === "parse" ? "import-parse-missing" : missing === "check" ? "import-check-missing" : missing === "resolveReceipts" ? "import-receipt-missing" : missing === "reopen" ? "import-reopen-missing" : "import-step-order");
    return;
  }
  if (event.sourceMapDigest && event.sourceMapDigest !== facts.interface.sourceMapKey) {
    pushError(state, "import-source-map-stale", "import source map digest is stale");
    return;
  }
  if (event.packageLockDigest && event.packageLockDigest !== facts.source.packageLockDigest) {
    pushError(state, "import-lock-stale", "import package-lock digest is stale");
    return;
  }
  const receipts = event.receipts ?? state.validatedReceipts;
  if (canonical(receipts) !== canonical(state.validatedReceipts)) {
    pushError(state, "import-receipt-missing", "import must resolve the exact committed receipt set");
    return;
  }
  state.imported = true;
}

function validateInspector(event, state, facts) {
  if (state.publication !== "committed") {
    pushError(state, "inspect-before-commit", "inspector reads committed snapshots only");
    return;
  }
  if (event.mode !== "readOnly" || event.write === true) {
    pushError(state, "inspector-write", "the committed inspector is read-only");
    return;
  }
  if (event.generation && event.generation !== state.activeGeneration) {
    pushError(state, "inspector-generation", "inspector requires the exact active generation");
    return;
  }
  const right = event.right ?? "metadata.read";
  const grant = facts.capabilityFacts.grants.find((item) => item.id === right && item.generation === state.activeGeneration && item.interfaceKey === facts.interface.semanticInterfaceKey);
  if (!grant || right !== "metadata.read") pushError(state, "inspector-right", "inspector requires the metadata.read right for the active generation");
  else state.inspected = true;
}

function validatePersistentResolve(event, state, facts) {
  const gap = facts.researchGap;
  if (!isObject(gap) || !["persistentResolve", "migration"].includes(gap.operation) || gap.currentContract !== "missing") {
    pushError(state, "research-descriptor-missing", "Research requires an explicit persistentResolve or migration descriptor with a missing contract");
    return;
  }
  if (event.mode === "write" || event.heap || event.liveHandles || event.liveHeap) {
    pushError(state, "migration-live-state", "persistent references cannot carry heap, tasks, or live handles");
    return;
  }
  const required = ["providerIdentity", "interfaceKey", "schemaDigest", "stableDomainKey", "savedGenerationPolicy"];
  if (required.some((field) => !validString(gap[field])) || !Array.isArray(gap.rights) || !validReceipt(gap.resolveReceipt, ["providerIdentity", "interfaceKey", "schemaDigest", "decision", "digest"]) || gap.resolveReceipt.decision !== "resolved") {
    pushError(state, "persistent-reference-receipt", "persistent reference requires provider, interface, schema, rights, stable key, generation policy, and resolve receipt");
    return;
  }
  if (gap.providerIdentity !== gap.resolveReceipt.providerIdentity || gap.interfaceKey !== gap.resolveReceipt.interfaceKey || gap.schemaDigest !== gap.resolveReceipt.schemaDigest || !validDigest(gap.resolveReceipt.digest)) {
    pushError(state, "persistent-reference-receipt", "persistent resolve receipt is stale or forged");
    return;
  }
  state.migration = true;
  state.research = true;
}

function validRollbackReceipt(receipt, facts, state) {
  return validReceipt(receipt, ["provider", "generation", "transaction", "frontierDigest", "artifactDigest", "decision"]) &&
    receipt.provider === facts.provider.identity &&
    receipt.generation === state.oldGeneration &&
    receipt.transaction === facts.provider.transaction &&
    receipt.frontierDigest === facts.provider.frontierDigest &&
    receipt.artifactDigest === facts.artifact.physicalArtifactDigest &&
    receipt.decision === "rolledBack";
}

function validCrashReceipt(receipt, facts, state, position) {
  if (!validReceipt(receipt, ["provider", "generation", "transaction", "frontierDigest", "artifactDigest", "decision"])) return false;
  if (receipt.provider !== facts.provider.identity || receipt.transaction !== facts.provider.transaction || receipt.frontierDigest !== facts.provider.frontierDigest || receipt.artifactDigest !== facts.artifact.physicalArtifactDigest) return false;
  if (position === "before-publication") return receipt.generation === state.oldGeneration && ["fault-boundary", "unknown"].includes(receipt.decision);
  return receipt.generation === state.nextGeneration && ["committed", "degraded"].includes(receipt.decision);
}

function validEffectReceipt(receipt, facts, state) {
  if (!validReceipt(receipt, ["provider", "generation", "transaction", "frontierDigest", "artifactDigest", "decision"])) return false;
  if (Object.keys(receipt).some((key) => !["provider", "generation", "transaction", "frontierDigest", "artifactDigest", "decision"].includes(key))) return false;
  return receipt.provider === facts.provider.identity &&
    receipt.generation === state.activeGeneration &&
    receipt.transaction === facts.provider.transaction &&
    receipt.frontierDigest === facts.provider.frontierDigest &&
    receipt.artifactDigest === facts.artifact.physicalArtifactDigest &&
    ["committed", "rolledBack", "unknown", "observed"].includes(receipt.decision);
}

function applyLocalCrash(state, facts, event) {
  const position = event.position ?? (state.switched ? "after-publication" : "before-publication");
  const receipt = event.crashReceipt;
  if (!validCrashReceipt(receipt, facts, state, position)) {
    pushError(state, "crash-receipt-invalid", "crash requires an exact provider receipt bound to generation, transaction, frontier, and staged artifact");
    state.terminal = true;
    return;
  }
  state.crash = { position, decision: receipt.decision, receipt };
  if (receipt.decision === "degraded") state.postSwitchDrainFailure = event.reason ?? "provider-drain-failure";
  state.terminal = true;
}

function applySplitCrash(state, facts, event) {
  const position = event.position ?? (state.switched ? "after-publication" : "before-publication");
  const receipt = event.crashReceipt;
  if (!validCrashReceipt(receipt, facts, state, position)) {
    pushError(state, "crash-receipt-invalid", "split crash requires an exact provider receipt bound to generation, transaction, frontier, and staged artifact");
    state.terminal = true;
    return;
  }
  state.crash = { position, decision: receipt.decision, receipt };
  if (receipt.decision === "degraded") state.postSwitchDrainFailure = event.reason ?? "provider-drain-failure";
  state.terminal = true;
}

function rejectAfterBoundary(state, reason = "event-after-boundary") {
  state.boundaryViolation = true;
  pushError(state, reason, "a terminal rollback, crash, or pre-switch failure cannot accept a later event");
}

function selectionReceiptValid(receipt, state, candidates) {
  if (!validReceipt(receipt, ["generation", "frontierDigest", "decision", "receiptDigest"]) || receipt.generation !== state.nextGeneration || receipt.decision !== "selected") return false;
  const ready = candidates.map((candidate) => state.readyReceipts.find((item) => item.generation === candidate));
  if (ready.some((item) => !item)) return false;
  const frontierDigest = canonicalDigest(ready);
  const receiptDigest = canonicalDigest({ generation: receipt.generation, frontierDigest, decision: receipt.decision });
  return receipt.frontierDigest === frontierDigest && receipt.receiptDigest === receiptDigest;
}

function applyLocalEvent(state, facts, event) {
  if (state.terminal) {
    rejectAfterBoundary(state, "terminal-event-after-boundary");
    return;
  }
  if (state.failed) {
    if (!["prepare", "validate", "preflight", "ready"].includes(event.op)) rejectAfterBoundary(state, "event-after-error");
    return;
  }
  switch (event.op) {
    case "prepare":
      if (state.phase !== "idle") pushError(state, "phase-order", "prepare is one-shot and must be first");
      else if (identityFactsValid(facts, state)) state.phase = "prepared";
      break;
    case "validate":
      if (state.phase !== "prepared") pushError(state, "phase-order", "validate requires prepare");
      else {
        validateSchema(facts, state);
        validateCapabilityFacts(facts, state);
        if (!state.error) validateInterface(facts, state, state.mode);
        if (!state.error) state.phase = "validated";
      }
      break;
    case "preflight":
      if (state.phase !== "validated") pushError(state, "phase-order", "preflight requires validate");
      else {
        validateTarget(facts, state);
        if (!state.error) {
          state.phase = "preflight";
        }
      }
      break;
    case "ready":
      if (state.phase !== "preflight") pushError(state, "phase-order", "ready requires preflight");
      else {
        state.readyReceipts = [state.oldGeneration, state.nextGeneration].map((generation) => ({ generation, digest: canonicalDigest({ generation, interface: facts.interface.semanticInterfaceKey }) }));
        state.phase = "ready";
      }
      break;
    case "switch":
      if (state.phase !== "ready") pushError(state, "phase-order", "switch requires ready");
      else {
        state.switched = true;
        state.publication = "committed";
        state.validatedReceipts = deriveReceiptSet(facts);
        state.phase = "switched";
        state.activeGeneration = state.nextGeneration;
        state.ownerGraph = [`old:${state.oldGeneration}`, `new:${state.nextGeneration}`];
      }
      break;
    case "closeAdmission":
      if (!state.switched || state.oldAdmission !== "open") pushError(state, "admission-order", "closeAdmission is one-shot and follows atomic switch");
      else {
        state.oldAdmission = "closed";
        state.phase = "admission-closed";
      }
      break;
    case "cancel":
    case "drain":
    case "unregister":
    case "inFlightDrain":
    case "destroy":
    case "unpin":
    case "release":
    case "unmap":
      applyLocalCleanup(state, facts, event);
      break;
    case "oldCompletion":
    case "oldMessage":
    case "oldCapability":
      if (state.switched && event.generation === state.oldGeneration) state.staleRejections.push(event.op);
      else pushError(state, "stale-generation-claim", "old-generation completion, message, or capability was not rejected");
      break;
    case "newCompletion":
      if (state.switched && event.generation === state.nextGeneration) state.acceptedCompletions += 1;
      else pushError(state, "new-generation-mismatch", "new completion must name the published generation");
      break;
    case "select": {
      const candidates = event.candidates;
      const receipt = event.selectionReceipt ?? event.receipt;
      const duplicate = Array.isArray(candidates) && new Set(candidates).size !== candidates.length;
      const ready = Array.isArray(candidates) && candidates.every((candidate) => state.readyReceipts.some((item) => item.generation === candidate));
      if (state.selection || !state.switched || !Array.isArray(candidates) || candidates.length < 2 || duplicate || !ready || event.generation !== state.nextGeneration || !selectionReceiptValid(receipt, state, candidates)) pushError(state, "concurrent-selection", "selection requires one exact winner receipt over two or more unique ready candidates");
      else {
        state.selection = state.nextGeneration;
        state.selectionReceipt = receipt;
      }
      break;
    }
    case "effect": {
      const right = event.right ?? event.capability;
      const declared = facts.effectFacts.declared.find((item) => item.id === (event.id ?? right) || item.right === right);
      const grant = facts.capabilityFacts.grants.find((item) => item.id === right && item.generation === state.activeGeneration && item.interfaceKey === facts.interface.semanticInterfaceKey && item.artifactKey === facts.artifact.key);
      const receipt = event.providerReceipt;
      if (!state.switched || !declared || declared.right !== right || declared.generation !== state.activeGeneration || !grant || !validEffectReceipt(receipt, facts, state)) pushError(state, "effect-capability-mismatch", "effect requires a declared right, active generation grant, and exact provider receipt");
      else state.effects.push(receipt.decision);
      break;
    }
    case "providerRollbackReceipt": {
      const receipt = event.receipt;
      if (state.switched) pushError(state, "rollback-after-publication", "rollback is valid only before publication");
      else if (!validRollbackReceipt(receipt, facts, state)) pushError(state, "rollback-receipt-invalid", "rollback requires an exact provider receipt for the prepublication frontier and staged artifact");
      else {
        state.providerRollback = true;
        state.rollbackReceipt = receipt;
        state.terminal = true;
      }
      break;
    }
    case "preSwitchFailure":
      if (state.switched) pushError(state, "post-switch-rollback", "pre-switch failure cannot occur after publication");
      else {
        pushError(state, event.code ?? "pre-switch-failure", "pre-switch failure preserves the old generation");
        state.terminal = true;
      }
      break;
    case "drainFailure":
      if (!state.switched) pushError(state, event.code ?? "pre-switch-drain-failure", "pre-switch drain failure preserves old generation");
      else state.postSwitchDrainFailure = event.reason ?? "provider-drain-failure";
      break;
    case "crash":
      applyLocalCrash(state, facts, event);
      break;
    case "export":
      validateExportFacts(facts, event, state);
      break;
    case "import":
      validateImport(event, state, facts);
      break;
    case "inspectCommitted":
      validateInspector(event, state, facts);
      break;
    case "persistentResolve":
    case "migration":
      validatePersistentResolve(event, state, facts);
      break;
    case "lookupPath":
    case "lookupName":
      pushError(state, "identity-path-lookup", "artifact and package identity uses digest, index, and lock receipts");
      break;
    case "registerCallback":
      if (state.oldAdmission === "closed" || state.unregistered) pushError(state, "callback-registration-closed", "callback registration is closed after admission close or unregister");
      else state.callbackRegistered = true;
      break;
    case "callbackEnter":
      if (state.oldAdmission === "closed" || state.unregistered) pushError(state, "late-callback", "callbackEnter after close or unregister is rejected");
      else {
        state.callbackRegistered = true;
        state.callbackInFlight += 1;
      }
      break;
    case "callbackExit":
      if (state.callbackInFlight < 1) pushError(state, "callback-underflow", "callbackExit requires an in-flight callback");
      else state.callbackInFlight -= 1;
      break;
    case "unload":
      if (state.callbackInFlight !== 0) pushError(state, "unload-live-callback", "unload requires callback unregister and in-flight drain");
      else if (state.oldAdmission !== "closed" || !state.unregistered) pushError(state, "unload-before-drain", "unload requires close admission and callback unregister");
      else if (state.cleanup.length !== requiredCleanup(facts).length) pushError(state, "unload-before-drain", facts.isolation === "native" ? "native release requires complete drain with mapping retained" : "isolated process, Wasm, or component unload requires full drain");
      else state.ffiRelease = facts.isolation === "native" ? "native-release-mapping-pinned" : "isolated-stop-after-drain";
      break;
    case "capabilityRevoked":
      if (event.generation === state.oldGeneration) state.staleRejections.push("capability-revoked");
      else if (event.generation === state.activeGeneration) state.capabilityState = "revoked-current";
      else pushError(state, "capability-generation", "capability revocation must name old or active generation");
      break;
    case "attempt":
      pushError(state, "attempt-route-mismatch", "rejected mechanism attempts belong to route D");
      break;
    default:
      pushError(state, "unknown-event", `unsupported event ${event.op}`);
  }
}

function finishState(state, facts, testCase) {
  let status = "rejected";
  let route = state.research ? "research" : "composable";
  let code = state.error?.code ?? "none";
  if (state.boundaryViolation) {
    status = "rejected";
    code = state.error?.code ?? "terminal-event-after-boundary";
  } else if (state.providerRollback) {
    status = "rolled-back";
    code = "provider-receipt-rollback";
  } else if (state.crash) {
    if (state.crash.position === "before-publication") {
      status = state.crash.decision === "unknown" ? "unknown-effect" : "fault-boundary";
      code = state.crash.decision === "unknown" ? "unknown-provider-effect" : "pre-publication-crash-preserves-old";
    } else {
      status = state.postSwitchDrainFailure ? "degraded" : "committed";
      code = state.postSwitchDrainFailure ? "post-publication-crash-degraded" : "post-publication-crash";
    }
  } else if (state.error) {
    status = "rejected";
  } else if (state.postSwitchDrainFailure) {
    status = "degraded";
    code = "post-switch-drain-failure";
  } else if (state.research && state.switched) {
    status = "research";
  } else if (state.switched && state.cleanup.length !== requiredCleanup(facts).length) {
    status = "draining";
    code = "cleanup-incomplete";
  } else if (state.switched) {
    status = "committed";
  }
  if (state.staleRejections.length > 0 && state.capabilityState === "declared") state.capabilityState = "stale-generation-rejected";
  return {
    caseId: testCase.id,
    axis: testCase.axis,
    mode: state.mode,
    status,
    route,
    code,
    generation: state.activeGeneration,
    oldGeneration: state.oldGeneration,
    publication: state.publication,
    interfaceResult: state.interfaceResult,
    ownerGraph: state.ownerGraph,
    effectOutcome: state.effects.length === 0 ? "none" : state.effects.some((effect) => effect === "failure") ? "failure" : state.effects.some((effect) => effect === "unknown") ? "unknown" : "success",
    effectDecisions: state.effects,
    cleanupOrder: state.cleanup,
    capabilityState: state.capabilityState,
    staleRejections: state.staleRejections,
    exportDigest: state.exportDigest,
    imported: state.imported,
    inspected: state.inspected,
    migration: state.migration,
    research: state.research,
    selection: state.selection,
    selectionReceipt: state.selectionReceipt,
    rollbackReceipt: state.rollbackReceipt,
    readyReceipts: state.readyReceipts,
    postSwitchDrainFailure: state.postSwitchDrainFailure,
    crash: state.crash,
    ffiRelease: state.ffiRelease,
    callbackInFlight: state.callbackInFlight,
    callbackRegistered: state.callbackRegistered,
    unregistered: state.unregistered,
    physicalTrace: state.physicalTrace,
    declaredGeneration: facts.capabilityFacts.generation,
  };
}

function applyReducerMutation(state, mutation, owner, event) {
  if (!validString(mutation) || !mutation.startsWith(`${owner}-`)) return;
  if (mutation === `${owner}-switch` && state.switched) {
    state.ownerGraph = [`${owner}:mutated-switch`, `new:${state.nextGeneration}`];
  } else if (mutation === `${owner}-cleanup` && state.cleanup.length > 0) {
    if (!state.cleanup.includes(`mutated-${owner}-cleanup`)) state.cleanup.push(`mutated-${owner}-cleanup`);
  } else if (mutation === `${owner}-callback` && state.switched) {
    state.callbackInFlight = Math.max(1, state.callbackInFlight);
  } else if (mutation === `${owner}-selection` && state.selection) {
    state.selectionReceipt = { ...state.selectionReceipt, receiptDigest: canonicalDigest({ forgedBy: owner, generation: state.nextGeneration }) };
  } else if (mutation === `${owner}-crash` && state.crash) {
    state.crash = { ...state.crash, decision: "forged-mutation" };
  }
  /* Keep the mutation attached to an event boundary.  This is host-only
   * instrumentation; corpus cases never carry this field. */
  if (event?.op === "switch" && mutation === `${owner}-switch`) state.ownerGraph = [`${owner}:mutated-switch`, `new:${state.nextGeneration}`];
}

export function reduceLocal(facts, events, testCase = { id: "local", axis: "A" }, mutation) {
  const state = initialState(facts, "local");
  const expanded = expandEvents(events);
  if (!Array.isArray(expanded) || expanded.length > facts.limits.maximumEvents) pushError(state, "event-quota", "local trace exceeds its declared bound");
  for (const raw of expanded ?? []) {
    const event = eventObject(raw);
    applyLocalEvent(state, facts, event);
    applyReducerMutation(state, mutation, "local", event);
  }
  return finishState(state, facts, testCase);
}

export function reduceSplit(facts, events, testCase = { id: "split", axis: "B" }, mutation) {
  const state = initialState(facts, "split");
  const expanded = expandEvents(events);
  if (!Array.isArray(expanded) || expanded.length > facts.limits.maximumEvents) pushError(state, "event-quota", "split trace exceeds its declared bound");
  for (const raw of expanded ?? []) {
    const event = eventObject(raw);
    /* Keep a separate operational handler.  Split selection and callback
     * admission are checked at the service boundary before common receipts. */
    applySplitEvent(state, facts, event);
    applyReducerMutation(state, mutation, "split", event);
  }
  return finishState(state, facts, testCase);
}

function applySplitEvent(state, facts, event) {
  /* Split publication owns ServiceIR/schema validation at the service
   * boundary.  Keep this handler separate so projection mutations can diverge
   * without mutating the local owner graph. */
  if (event.op === "attempt") {
    pushError(state, "attempt-route-mismatch", "rejected mechanism attempts belong to route D");
    return;
  }
  if (event.op === "switch" && state.mode !== "split") pushError(state, "split-owner-mismatch", "split reducer lost its owner mode");
  applySplitStateEvent(state, facts, event);
}

/* Split-service state machine.  It intentionally repeats the operational
 * transitions.  Validators and canonical receipt helpers are shared, but a
 * split event cannot call the local transition function. */
function applySplitStateEvent(state, facts, event) {
  if (state.terminal) {
    rejectAfterBoundary(state, "terminal-event-after-boundary");
    return;
  }
  if (state.failed) {
    if (!["prepare", "validate", "preflight", "ready"].includes(event.op)) rejectAfterBoundary(state, "event-after-error");
    return;
  }
  switch (event.op) {
    case "prepare":
      if (state.phase !== "idle") pushError(state, "phase-order", "split prepare is one-shot and must be first");
      else if (identityFactsValid(facts, state)) state.phase = "prepared";
      break;
    case "validate":
      if (state.phase !== "prepared") pushError(state, "phase-order", "split validate requires prepare");
      else {
        validateSchema(facts, state);
        validateCapabilityFacts(facts, state);
        if (!state.error) validateInterface(facts, state, "split");
        if (!state.error) state.phase = "validated";
      }
      break;
    case "preflight":
      if (state.phase !== "validated") pushError(state, "phase-order", "split preflight requires validate");
      else {
        validateTarget(facts, state);
        if (!state.error) {
          state.phase = "preflight";
        }
      }
      break;
    case "ready":
      if (state.phase !== "preflight") pushError(state, "phase-order", "split ready requires preflight");
      else {
        state.readyReceipts = [state.oldGeneration, state.nextGeneration].map((generation) => ({ generation, digest: canonicalDigest({ generation, interface: facts.interface.semanticInterfaceKey }) }));
        state.phase = "ready";
      }
      break;
    case "switch":
      if (state.phase !== "ready") pushError(state, "phase-order", "split switch requires ready");
      else {
        state.switched = true;
        state.publication = "committed";
        state.validatedReceipts = deriveReceiptSet(facts);
        state.phase = "switched";
        state.activeGeneration = state.nextGeneration;
        state.ownerGraph = [`old:${state.oldGeneration}`, `new:${state.nextGeneration}`];
      }
      break;
    case "closeAdmission":
      if (!state.switched || state.oldAdmission !== "open") pushError(state, "admission-order", "split closeAdmission is one-shot and follows atomic switch");
      else {
        state.oldAdmission = "closed";
        state.phase = "admission-closed";
      }
      break;
    case "cancel":
    case "drain":
    case "unregister":
    case "inFlightDrain":
    case "destroy":
    case "unpin":
    case "release":
    case "unmap":
      applySplitCleanup(state, facts, event);
      break;
    case "oldCompletion":
    case "oldMessage":
    case "oldCapability":
      if (state.switched && event.generation === state.oldGeneration) state.staleRejections.push(event.op);
      else pushError(state, "stale-generation-claim", "split service must reject the exact old generation event");
      break;
    case "newCompletion":
      if (state.switched && event.generation === state.nextGeneration) state.acceptedCompletions += 1;
      else pushError(state, "new-generation-mismatch", "split completion must name the published generation");
      break;
    case "select": {
      const candidates = event.candidates;
      const receipt = event.selectionReceipt ?? event.receipt;
      const duplicate = Array.isArray(candidates) && new Set(candidates).size !== candidates.length;
      const ready = Array.isArray(candidates) && candidates.every((candidate) => state.readyReceipts.some((item) => item.generation === candidate));
      if (state.selection || !state.switched || !Array.isArray(candidates) || candidates.length < 2 || duplicate || !ready || event.generation !== state.nextGeneration || !selectionReceiptValid(receipt, state, candidates)) pushError(state, "concurrent-selection", "split selection requires one exact winner receipt over two or more unique ready candidates");
      else {
        state.selection = state.nextGeneration;
        state.selectionReceipt = receipt;
      }
      break;
    }
    case "effect": {
      const right = event.right ?? event.capability;
      const declared = facts.effectFacts.declared.find((item) => item.id === (event.id ?? right) || item.right === right);
      const grant = facts.capabilityFacts.grants.find((item) => item.id === right && item.generation === state.activeGeneration && item.interfaceKey === facts.interface.semanticInterfaceKey && item.artifactKey === facts.artifact.key);
      const receipt = event.providerReceipt;
      if (!state.switched || !declared || declared.right !== right || declared.generation !== state.activeGeneration || !grant || !validEffectReceipt(receipt, facts, state)) pushError(state, "effect-capability-mismatch", "split effect requires a declared right, active grant, and exact provider receipt");
      else state.effects.push(receipt.decision);
      break;
    }
    case "providerRollbackReceipt": {
      const receipt = event.receipt;
      if (state.switched) pushError(state, "rollback-after-publication", "split rollback is valid only before publication");
      else if (!validRollbackReceipt(receipt, facts, state)) pushError(state, "rollback-receipt-invalid", "split rollback requires an exact provider receipt for the prepublication frontier");
      else {
        state.providerRollback = true;
        state.rollbackReceipt = receipt;
        state.terminal = true;
      }
      break;
    }
    case "preSwitchFailure":
      if (state.switched) pushError(state, "post-switch-rollback", "split pre-switch failure cannot occur after publication");
      else {
        pushError(state, event.code ?? "pre-switch-failure", "split pre-switch failure preserves the old generation");
        state.terminal = true;
      }
      break;
    case "drainFailure":
      if (!state.switched) pushError(state, event.code ?? "pre-switch-drain-failure", "split pre-switch drain failure preserves old generation");
      else state.postSwitchDrainFailure = event.reason ?? "provider-drain-failure";
      break;
    case "crash":
      applySplitCrash(state, facts, event);
      break;
    case "export":
      validateExportFacts(facts, event, state);
      break;
    case "import":
      validateImport(event, state, facts);
      break;
    case "inspectCommitted":
      validateInspector(event, state, facts);
      break;
    case "persistentResolve":
    case "migration":
      validatePersistentResolve(event, state, facts);
      break;
    case "lookupPath":
    case "lookupName":
      pushError(state, "identity-path-lookup", "split identity uses digest, index, and lock receipts");
      break;
    case "registerCallback":
      if (state.oldAdmission === "closed" || state.unregistered) pushError(state, "callback-registration-closed", "split callback registration is closed");
      else state.callbackRegistered = true;
      break;
    case "callbackEnter":
      if (state.oldAdmission === "closed" || state.unregistered) pushError(state, "late-callback", "split callbackEnter after close or unregister is rejected");
      else {
        state.callbackRegistered = true;
        state.callbackInFlight += 1;
      }
      break;
    case "callbackExit":
      if (state.callbackInFlight < 1) pushError(state, "callback-underflow", "split callbackExit requires an in-flight callback");
      else state.callbackInFlight -= 1;
      break;
    case "unload":
      if (state.callbackInFlight !== 0) pushError(state, "unload-live-callback", "split unload requires in-flight callbacks to be zero");
      else if (state.oldAdmission !== "closed" || !state.unregistered || state.cleanup.length !== requiredCleanup(facts).length) pushError(state, "unload-before-drain", facts.isolation === "native" ? "split native release requires unregister and complete drain with mapping retained" : "split unload requires unregister, in-flight drain, and complete isolated cleanup");
      else state.ffiRelease = facts.isolation === "native" ? "native-release-mapping-pinned" : "isolated-stop-after-drain";
      break;
    case "capabilityRevoked":
      if (event.generation === state.oldGeneration) state.staleRejections.push("capability-revoked");
      else if (event.generation === state.activeGeneration) state.capabilityState = "revoked-current";
      else pushError(state, "capability-generation", "split capability revocation must name old or active generation");
      break;
    case "attempt":
      pushError(state, "attempt-route-mismatch", "split route D attempts are not admitted");
      break;
    default:
      pushError(state, "unknown-event", `unsupported split event ${event.op}`);
  }
}

function logicalFields(result) {
  return {
    status: result.status,
    code: result.code,
    generation: result.generation,
    publication: result.publication,
    interfaceResult: result.interfaceResult,
    ownerGraph: result.ownerGraph,
    effectOutcome: result.effectOutcome,
    effectDecisions: result.effectDecisions,
    cleanupOrder: result.cleanupOrder,
    capabilityState: result.capabilityState,
    staleRejections: result.staleRejections,
    exportDigest: result.exportDigest,
    imported: result.imported,
    selection: result.selection,
    selectionReceipt: result.selectionReceipt,
    crash: result.crash,
    postSwitchDrainFailure: result.postSwitchDrainFailure,
    ffiRelease: result.ffiRelease,
    callbackInFlight: result.callbackInFlight,
    callbackRegistered: result.callbackRegistered,
    unregistered: result.unregistered,
    rollbackReceipt: result.rollbackReceipt,
  };
}

function targetProjectionFacts(facts, projection) {
  const target = facts.targets?.[projection];
  if (!target) return facts;
  const receipt = target.wAbiReceipt ?? facts.interface.wAbiReceipt;
  if (!receipt) return deepMerge(facts, { target });
  return deepMerge(facts, {
    target,
    interface: {
      wAbiKey: receipt.wAbiKey,
      wAbiReceipt: receipt,
      old: { wAbiKey: receipt.wAbiKey },
      candidate: { wAbiKey: receipt.wAbiKey },
    },
  });
}

/* Host-only comparison hook.  `mutate` is intentionally outside the corpus
 * schema and lets reference tests break one operational reducer at a time. */
export function compareDyn1Projections(testCase, { corpus, mutate } = {}) {
  const facts = normaliseFacts(corpus, testCase);
  const localFacts = targetProjectionFacts(facts, testCase.targetPair ? "A" : "default");
  const splitFacts = targetProjectionFacts(facts, testCase.targetPair ? "B" : "default");
  const local = reduceLocal(localFacts, testCase.projections.local, { id: testCase.id, axis: testCase.axis }, mutate);
  const split = reduceSplit(splitFacts, testCase.projections.split, { id: testCase.id, axis: testCase.axis }, mutate);
  const left = logicalFields(local);
  const right = logicalFields(split);
  return { local, split, left, right, equal: canonical(left) === canonical(right) };
}

function validateDAttempt(testCase) {
  const events = expandEvents(testCase.events);
  if (!Array.isArray(events) || events.length !== 1) return { ok: false, code: "DYN1-D-attempt-shape" };
  const event = eventObject(events[0]);
  const mechanism = event.mechanism;
  if (!isObject(mechanism) || !validString(mechanism.id)) return { ok: false, code: "unknown-mechanism" };
  const entry = MECHANISMS[mechanism.id];
  if (!entry) return { ok: false, code: "unknown-mechanism" };
  if (mechanism.family !== entry.family) return { ok: false, code: "mechanism-family-mismatch" };
  if (event.op !== "attempt" || !validString(event.invariant)) return { ok: false, code: "attempt-invariant-missing" };
  if (event.invariant !== entry.invariant) return { ok: false, code: "mechanism-invariant-mismatch" };
  if (mechanism.disposition !== entry.disposition) return { ok: false, code: "mechanism-disposition-mismatch" };
  return { ok: true, mechanism: { id: mechanism.id, family: mechanism.family, disposition: mechanism.disposition }, invariant: event.invariant };
}

export function evaluateDyn1Case(testCase, { corpus, mutate } = {}) {
  if (!isObject(corpus?.defaults)) return { caseId: testCase?.id, axis: testCase?.axis, status: "invalid-assay", route: "invalid-assay", code: "missing-corpus-defaults" };
  const overrideErrors = [];
  validateOverrideShape(testCase?.facts, overrideErrors, `case ${testCase?.id}.facts`);
  if (overrideErrors.length > 0) return { caseId: testCase?.id, axis: testCase?.axis, status: "invalid-assay", route: "invalid-assay", code: "facts-mutation-incomplete", errors: overrideErrors };
  const facts = normaliseFacts(corpus, testCase);
  const factErrors = validateFactSchema(facts, [], `case ${testCase.id}.facts`);
  if (factErrors.length > 0) return { caseId: testCase.id, axis: testCase.axis, status: "invalid-assay", route: "invalid-assay", code: "facts-schema-invalid", errors: factErrors };
  if (testCase.axis === "D") {
    const attempt = validateDAttempt(testCase);
    if (!attempt.ok) return { caseId: testCase.id, axis: "D", status: "invalid-assay", route: "invalid-assay", code: attempt.code, cleanupOrder: [], capabilityState: "not-admitted", physicalTrace: [] };
    return { caseId: testCase.id, axis: "D", status: "intentionally-rejected", route: "intentionally-rejected", code: "DYN1-REJECT-dynamic-mutation", mechanism: attempt.mechanism, invariant: attempt.invariant, cleanupOrder: [], capabilityState: "not-admitted", physicalTrace: [] };
  }
  if (testCase.projections) {
    const comparison = compareDyn1Projections(testCase, { corpus, mutate });
    if (!comparison.equal) return { ...comparison.local, caseId: testCase.id, axis: testCase.axis, mode: "paired", status: "rejected", route: "composable", code: "projection-divergence", projection: { local: comparison.left, split: comparison.right }, physicalTrace: { local: comparison.local.physicalTrace, split: comparison.split.physicalTrace } };
    return { ...comparison.local, caseId: testCase.id, axis: testCase.axis, mode: "paired", projection: { local: comparison.left, split: comparison.right }, physicalTrace: { local: comparison.local.physicalTrace, split: comparison.split.physicalTrace } };
  }
  const result = testCase.mode === "split" ? reduceSplit(facts, testCase.events, testCase) : reduceLocal(facts, testCase.events, testCase);
  const hasResearchDescriptor = isObject(facts.researchGap) && ["persistentResolve", "migration"].includes(facts.researchGap.operation) && facts.researchGap.currentContract === "missing";
  if (hasResearchDescriptor) {
    result.route = "research";
    if (result.status === "committed") result.status = "research";
  } else if (result.research && !result.error) result.route = "research";
  return result;
}

function compareExpected(result, expected, id, errors) {
  if (!isObject(expected)) {
    errors.push(`${id}.expect must be an object of assertions.`);
    return;
  }
  for (const [key, expectedValue] of Object.entries(expected)) if (canonical(result[key]) !== canonical(expectedValue)) errors.push(`${id}.expect.${key} does not match the derived result.`);
}

export function validateDyn1(corpus, { root } = {}) {
  const errors = [];
  if (corpus?.$schema !== "w-dyn1-versioned-behavior-cases-1") errors.push("DYN1 corpus schema is invalid.");
  if (corpus?.status !== "design-oracle-input") errors.push("DYN1 corpus status must be design-oracle-input.");
  if (!Array.isArray(corpus?.cases) || corpus.cases.length < 45 || corpus.cases.length > 70) {
    errors.push("DYN1 corpus must contain 45–70 cases.");
    return { errors, results: [] };
  }
  const defaultsErrors = validateFactSchema(corpus.defaults, [], "defaults");
  errors.push(...defaultsErrors);
  const ids = new Set();
  const results = [];
  for (const testCase of corpus.cases) {
    if (!testCase || typeof testCase.id !== "string" || ids.has(testCase.id)) errors.push(`duplicate or invalid case ${testCase?.id ?? "<missing>"}`);
    ids.add(testCase.id);
    if (!["A", "B", "C", "D"].includes(testCase.axis)) errors.push(`${testCase.id}.axis must be A, B, C, or D.`);
    if (testCase.axis !== "D" && !Array.isArray(testCase.events) && !testCase.projections) errors.push(`${testCase.id} must provide events or paired projections.`);
    if (testCase.axis === "D" && (!Array.isArray(testCase.events) || !isObject(eventObject(testCase.events[0])?.mechanism))) errors.push(`${testCase.id} must provide a structured route D attempt.`);
    const input = structuredClone(testCase);
    delete input.expect;
    visitInput(input, `case ${testCase.id}`, errors);
    validateOverrideShape(testCase.facts, errors, `case ${testCase.id}.facts`);
    const result = defaultsErrors.length === 0 ? evaluateDyn1Case(testCase, { corpus }) : { caseId: testCase.id, status: "invalid-assay", route: "invalid-assay", code: "defaults-schema-invalid" };
    results.push(result);
    compareExpected(result, testCase.expect, testCase.id, errors);
  }
  for (const required of REQUIRED_CASES) if (!ids.has(required)) errors.push(`DYN1 required case missing: ${required}`);
  const dCases = results.filter((result) => result.axis === "D");
  const registeredD = results.filter((result) => result.axis === "D" && corpus.cases.find((item) => item.id === result.caseId)?.family === "rejected");
  if (registeredD.length < 10 || registeredD.some((result) => result.status !== "intentionally-rejected")) errors.push("DYN1 route D must reject every registered dynamic mutation mechanism.");
  if (dCases.some((result) => corpus.cases.find((item) => item.id === result.caseId)?.family !== "rejected" && result.status !== "invalid-assay")) errors.push("DYN1 forged route D mechanisms must be invalid assays.");
  const pairCases = results.filter((result) => result.mode === "paired");
  if (pairCases.length < 3) errors.push("DYN1 must compare independent local and split reducers.");
  const researchCases = results.filter((result) => result.route === "research");
  if (researchCases.length === 0 || researchCases.some((result) => result.axis !== "C")) errors.push("DYN1 Research must come from an explicit route C persistent or migration descriptor.");
  if (results.some((result) => result.route === "intentionally-rejected" && result.axis !== "D")) errors.push("DYN1 A/B/C routes cannot inherit route D.");
  return { errors, results };
}

export function digestFile(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}
