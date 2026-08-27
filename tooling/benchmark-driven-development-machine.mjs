import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

export const ROOT = path.resolve(import.meta.dir, "..");
export const SCHEMA_VERSION = "wbench/1";
export const LANGUAGE_PROFILES = Object.freeze(["learner", "idiomatic", "frontier"]);
export const PROFILE_DISCLOSURES = Object.freeze([
  "unsafe",
  "ffi",
  "targetSpecialization",
  "manualLayout",
  "algorithm",
  "legibility",
]);
export const LANES = Object.freeze(["equivalent", "open"]);
export const BENCHMARK_DISPOSITIONS = Object.freeze([
  "required",
  "compiler-lifecycle",
  "deferred",
  "not-applicable",
]);
export const LIFECYCLE_PHASES = Object.freeze([
  "clean",
  "no-op",
  "edit",
  "frontend",
  "hir",
  "lowering",
  "codegen",
  "link",
  "startup",
  "execution",
]);
export const REQUIRED_CASES = Object.freeze([
  "BMD0-W-1487-current",
  "BMD0-W-1487-compiler-lifecycle",
  "BMD0-W-1487-open-lane",
  "BMD0-W-1487-deferred",
  "BMD0-W-1487-not-applicable",
  "BMD0-W-1487-profile-missing",
  "BMD0-W-1487-profile-duplicate",
  "BMD0-W-1487-idiomatic-not-primary",
  "BMD0-W-1487-oracle-partial",
  "BMD0-W-1487-equivalent-semantic-difference",
  "BMD0-W-1487-learner-artificially-slow",
  "BMD0-W-1487-frontier-missing-disclosure",
  "BMD0-W-1487-baseline-provenance-missing",
  "BMD0-W-1487-compile-execution-mixed",
  "BMD0-W-1487-best-only",
  "BMD0-W-1487-output-constant-bypass",
  "BMD0-W-1487-claim-without-backend",
  "BMD0-W-1487-specialization-universal",
  "BMD0-W-1487-invalid-benchmark-disposition",
  "BMD0-W-1487-deferred-missing-blocker",
  "BMD0-W-1487-not-applicable-missing-reason",
]);
export const SOURCE_FIXTURE = Object.freeze({
  path: "reference/last-light/checker_bootstrap.w",
  symbol: "export fn canAcceptOrder(",
  digest: "sha256:06a1d29ebac7e2b80e0c37d0a7cfced8a5d1ce35fed3d945146a46f7fdca43bc",
});
export const SOURCE_ID = "last-light-checker-bootstrap-source";
export const EDIT_RECIPE = Object.freeze({
  kind: "whitespace-only",
  sourcePath: SOURCE_FIXTURE.path,
  targetSymbol: SOURCE_FIXTURE.symbol,
  occurrence: 1,
  match: "  return open\n",
  replacement: "    return open\n",
  semanticPreserving: true,
  applyTo: "temporary-copy",
  operation: "replace-once",
  expectedMatches: 1,
  temporaryCopy: true,
});
export const DESCRIPTOR_IDENTITIES = Object.freeze({
  graph: Object.freeze({
    id: "last-light-checker-bootstrap-graph",
    path: "benchmarks/seed-check-graph.json",
    digest: "sha256:ff78805c96c17e617106c27af1007ac3fc0e0327e2d87c1bfa36e3d5f23160f5",
  }),
  input: Object.freeze({
    id: "last-light-checker-bootstrap-input",
    path: "benchmarks/seed-check-input.json",
    digest: "sha256:c202dd03968c1a9a2999e81eb6bfbb633e8d53cb33e12e62adeb65e3fa4a2020",
  }),
});

const DIGEST_PATTERN = /^sha256:[0-9a-f]{64}$/u;
const PROFILE_SET = new Set(LANGUAGE_PROFILES);
const LANE_SET = new Set(LANES);
const DISPOSITION_SET = new Set(BENCHMARK_DISPOSITIONS);
const PHASE_SET = new Set(LIFECYCLE_PHASES);

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function push(errors, message) {
  errors.push(message);
}

function requiredString(value, name, errors) {
  if (typeof value !== "string" || value.trim() === "") {
    push(errors, `${name} must be a non-empty string.`);
    return false;
  }
  return true;
}

function requiredBoolean(value, name, errors) {
  if (value !== true) {
    push(errors, `${name} must be true.`);
    return false;
  }
  return true;
}

function requiredDigest(value, name, errors) {
  if (!DIGEST_PATTERN.test(value ?? "")) {
    push(errors, `${name} must be a lowercase sha256 digest.`);
    return false;
  }
  return true;
}

function sameArray(actual, expected) {
  return Array.isArray(actual) && actual.length === expected.length &&
    actual.every((value, index) => value === expected[index]);
}

function repositoryPath(relativePath) {
  if (typeof relativePath !== "string" || relativePath.trim() === "") return undefined;
  const absolutePath = path.resolve(ROOT, relativePath);
  const relative = path.relative(ROOT, absolutePath);
  if (relative === "" || relative === ".." || relative.startsWith(".." + path.sep) || path.isAbsolute(relative)) return undefined;
  return absolutePath;
}

function fileDigest(relativePath) {
  const absolutePath = repositoryPath(relativePath);
  if (!absolutePath) return undefined;
  if (!fs.existsSync(absolutePath) || !fs.statSync(absolutePath).isFile()) return undefined;
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(absolutePath)).digest("hex")}`;
}

function checkSource(source, name, errors, requirePath = true) {
  if (!isObject(source)) {
    push(errors, `${name} must be an object.`);
    return;
  }
  if (requirePath) requiredString(source.path, `${name}.path`, errors);
  const hasSymbol = requiredString(source.symbol, `${name}.symbol`, errors);
  const hasDigest = requiredDigest(source.digest, `${name}.digest`, errors);
  if (source.path !== SOURCE_FIXTURE.path) push(errors, `${name}.path must use the Last Light benchmark fixture.`);
  if (hasSymbol && source.symbol !== SOURCE_FIXTURE.symbol) push(errors, `${name}.symbol must identify canAcceptOrder.`);
  if (hasDigest && source.digest !== SOURCE_FIXTURE.digest) push(errors, `${name}.digest is not the current fixture digest.`);
  const actual = fileDigest(source.path);
  if (!actual) push(errors, `${name}.path does not identify an existing source file.`);
  else {
    if (source.digest !== actual) push(errors, `${name}.digest is stale.`);
    if (hasSymbol && !fs.readFileSync(repositoryPath(source.path), "utf8").includes(source.symbol)) push(errors, `${name}.symbol is not present in the source bytes.`);
  }
}

function checkCorpusSource(source, name, errors) {
  if (!isObject(source)) {
    push(errors, `${name} must be an object.`);
    return;
  }
  requiredString(source.path, `${name}.path`, errors);
  requiredString(source.symbol, `${name}.symbol`, errors);
  const hasDigest = requiredDigest(source.digest, `${name}.digest`, errors);
  const actual = fileDigest(source.path);
  if (!actual) push(errors, `${name}.path does not identify an existing source file.`);
  else {
    if (hasDigest && source.digest !== actual) push(errors, `${name}.digest is stale.`);
    if (typeof source.symbol === "string" && !fs.readFileSync(repositoryPath(source.path), "utf8").includes(source.symbol)) push(errors, `${name}.symbol is not present in the source bytes.`);
  }
}

function checkIdentity(identity, name, errors, requirePath = false) {
  if (!isObject(identity)) {
    push(errors, `${name} must be an object.`);
    return false;
  }
  const validId = requiredString(identity.id, `${name}.id`, errors);
  const validDigest = requiredDigest(identity.digest, `${name}.digest`, errors);
  const validPath = requirePath ? requiredString(identity.path, `${name}.path`, errors) : true;
  return validId && validDigest && validPath;
}

function checkOracle(oracle, name, errors, expectedKind = "host-oracle") {
  if (!isObject(oracle)) {
    push(errors, `${name} must be an object.`);
    return;
  }
  if (expectedKind === "host-structural" && oracle.kind !== expectedKind) push(errors, `${name}.kind must be ${expectedKind}.`);
  if (expectedKind === "host-oracle" && oracle.kind !== undefined && oracle.kind !== expectedKind) push(errors, `${name}.kind must be ${expectedKind} when present.`);
  if (expectedKind === "host-structural") {
    requiredBoolean(oracle.requiredBeforeSamples, `${name}.requiredBeforeSamples`, errors);
    if (oracle.runtime !== "unavailable") push(errors, `${name}.runtime must be unavailable.`);
    return;
  }
  requiredString(oracle.correctnessRecord, `${name}.correctnessRecord`, errors);
  requiredBoolean(oracle.complete, `${name}.complete`, errors);
  requiredBoolean(oracle.beforeSamples, `${name}.beforeSamples`, errors);
}

function checkBaseline(baseline, name, errors) {
  if (!isObject(baseline)) {
    push(errors, `${name} must be an object.`);
    return;
  }
  if (!Array.isArray(baseline.independent)) {
    push(errors, `${name}.independent must be an array.`);
  } else {
    const unique = new Set(baseline.independent);
    if (unique.size !== baseline.independent.length) push(errors, `${name}.independent must not contain duplicates.`);
    for (const baselineId of baseline.independent) {
      if (!["c-clang", "rust"].includes(baselineId)) push(errors, `${name}.independent contains an unknown baseline.`);
    }
    if (baseline.independent.length < 2 && !requiredString(baseline.exceptionReason, `${name}.exceptionReason`, errors)) {
      push(errors, `${name} needs two independent baselines or an exception reason.`);
    }
    if (baseline.independent.length >= 2 && baseline.exceptionReason !== null) {
      push(errors, `${name}.exceptionReason must be null when two baselines are present.`);
    }
  }
  if (baseline.provenanceComplete !== true) push(errors, `${name}.provenanceComplete must be true.`);
}

function checkSamples(samples, name, errors, backendAvailable) {
  if (!isObject(samples)) {
    push(errors, `${name} must be an object.`);
    return;
  }
  if (samples.order !== "randomized-interleaved") push(errors, `${name}.order must be randomized-interleaved.`);
  if (!backendAvailable && samples.mode !== "not-started") {
    push(errors, `${name}.mode must be not-started while the backend is unavailable.`);
  }
}

function checkBackend(backendAvailable, claims, name, errors) {
  if (backendAvailable !== false) push(errors, `${name}.backendAvailable must be false for this protocol.`);
  if (!Array.isArray(claims)) {
    push(errors, `${name}.claims must be an array.`);
  } else if (backendAvailable === false && claims.length > 0) {
    push(errors, `${name}.claims must be empty without a backend.`);
  }
}

function checkDisposition(value, name, errors) {
  if (!DISPOSITION_SET.has(value)) push(errors, `${name} must be required, compiler-lifecycle, deferred or not-applicable.`);
}

function checkLanguageProfiles(profiles, name, errors) {
  if (!Array.isArray(profiles) || profiles.length !== LANGUAGE_PROFILES.length) {
    push(errors, `${name} must contain exactly learner, idiomatic and frontier.`);
    return new Map();
  }
  const byId = new Map();
  for (const [index, profile] of profiles.entries()) {
    const location = `${name}[${index}]`;
    if (!isObject(profile)) {
      push(errors, `${location} must be an object.`);
      continue;
    }
    if (!PROFILE_SET.has(profile.id)) push(errors, `${location}.id is not a required profile.`);
    if (byId.has(profile.id)) push(errors, `${location}.id duplicates ${profile.id}.`);
    byId.set(profile.id, profile);
    requiredBoolean(profile.correct, `${location}.correct`, errors);
    requiredBoolean(profile.plausible, `${location}.plausible`, errors);
  }
  for (const profileId of LANGUAGE_PROFILES) {
    if (!byId.has(profileId)) push(errors, `${name} is missing ${profileId}.`);
  }
  const learner = byId.get("learner");
  if (learner) {
    for (const field of ["sleep", "uselessWork", "worseFlags", "bypass"]) {
      if (learner[field] !== false) push(errors, `${name}.learner.${field} must be false.`);
    }
  }
  const frontier = byId.get("frontier");
  if (frontier) {
    if (!isObject(frontier.disclosures)) {
      push(errors, `${name}.frontier.disclosures must declare every frontier axis.`);
    } else {
      for (const field of PROFILE_DISCLOSURES) requiredString(frontier.disclosures[field], `${name}.frontier.disclosures.${field}`, errors);
      for (const field of Object.keys(frontier.disclosures)) {
        if (!PROFILE_DISCLOSURES.includes(field)) push(errors, `${name}.frontier.disclosures has an unknown axis ${field}.`);
      }
      if (frontier.disclosures.targetSpecialization === "universal") {
        push(errors, `${name}.frontier.disclosures.targetSpecialization must name a bounded target and fallback.`);
      }
    }
  }
  return byId;
}

function checkEquivalence(input, name, errors) {
  const fields = ["sameAlgorithm", "sameRepresentation", "sameValidation", "sameNumericContract", "sameInput"];
  for (const field of fields) {
    if (input[field] !== true) push(errors, `${name}.${field} must be true in the equivalent lane.`);
  }
}

function checkOpenLane(input, name, errors) {
  for (const field of ["sameValidation", "sameNumericContract", "sameInput"]) {
    if (input[field] !== true) push(errors, `${name}.${field} must be true in the open lane.`);
  }
  if (input.sameAlgorithm !== false && input.sameRepresentation !== false) {
    push(errors, `${name} must record an algorithm or representation difference in the open lane.`);
  }
}

function checkLanguageScenario(input, name, errors) {
  const disposition = input?.benchmarkDisposition;
  checkDisposition(disposition, `${name}.benchmarkDisposition`, errors);
  if (!(["required", "deferred"].includes(disposition))) {
    push(errors, `${name}.language workload must use required or deferred disposition.`);
  }
  checkLanguageProfiles(input?.profiles, `${name}.profiles`, errors);
  if (input?.primaryProfile !== "idiomatic") push(errors, `${name}.primaryProfile must be idiomatic.`);
  if (input?.regressionProfile !== "idiomatic") push(errors, `${name}.regressionProfile must be idiomatic.`);
  if (input?.lane === "equivalent") checkEquivalence(input, name, errors);
  else if (input?.lane === "open") checkOpenLane(input, name, errors);
  else push(errors, `${name}.lane must be equivalent or open.`);
  checkOracle(input?.oracle, `${name}.oracle`, errors);
  checkBackend(input?.backendAvailable, input?.claims, name, errors);
  checkBaseline(input?.baseline, `${name}.baseline`, errors);
  checkSamples(input?.samples, `${name}.samples`, errors, input?.backendAvailable);
  if (disposition === "deferred") {
    requiredString(input.blocker, `${name}.blocker`, errors);
    requiredString(input.taskId, `${name}.taskId`, errors);
    requiredString(input.stopCondition, `${name}.stopCondition`, errors);
  }
}

function checkProgramProfiles(profiles, name, errors) {
  if (!Array.isArray(profiles) || profiles.length !== LANGUAGE_PROFILES.length) {
    push(errors, `${name} must contain exactly learner, idiomatic and frontier.`);
    return new Map();
  }
  const byId = new Map();
  for (const [index, profile] of profiles.entries()) {
    const location = `${name}[${index}]`;
    if (!isObject(profile)) {
      push(errors, `${location} must be an object.`);
      continue;
    }
    if (!PROFILE_SET.has(profile.id)) push(errors, `${location}.id is not a required profile.`);
    if (byId.has(profile.id)) push(errors, `${location}.id duplicates ${profile.id}.`);
    byId.set(profile.id, profile);
    if (profile.track !== "language") push(errors, `${location}.track must be language.`);
    requiredString(profile.description, `${location}.description`, errors);
    if (typeof profile.primary !== "boolean") push(errors, `${location}.primary must be boolean.`);
    if (typeof profile.regression !== "boolean") push(errors, `${location}.regression must be boolean.`);
  }
  for (const profileId of LANGUAGE_PROFILES) if (!byId.has(profileId)) push(errors, `${name} is missing ${profileId}.`);
  const frontier = byId.get("frontier");
  if (!isObject(frontier?.disclosures)) {
    push(errors, `${name}.frontier.disclosures must declare every frontier axis.`);
  } else {
    for (const field of PROFILE_DISCLOSURES) requiredString(frontier.disclosures[field], `${name}.frontier.disclosures.${field}`, errors);
    for (const field of Object.keys(frontier.disclosures)) if (!PROFILE_DISCLOSURES.includes(field)) push(errors, `${name}.frontier.disclosures has an unknown axis ${field}.`);
    if (frontier.disclosures.targetSpecialization === "universal") push(errors, `${name}.frontier.disclosures.targetSpecialization must name a bounded target and fallback.`);
  }
  return byId;
}

function checkCompilerLifecycle(input, name, errors) {
  if (input?.benchmarkDisposition !== "compiler-lifecycle") {
    push(errors, `${name}.benchmarkDisposition must be compiler-lifecycle.`);
  }
  if (Object.prototype.hasOwnProperty.call(input ?? {}, "profiles")) {
    push(errors, `${name}.profiles must not define language source profiles.`);
  }
  if (input?.languageProfiles?.applicability !== "not-applicable" ||
      !requiredString(input?.languageProfiles?.reason, `${name}.languageProfiles.reason`, errors)) {
    push(errors, `${name}.languageProfiles must mark language profiles not-applicable with a reason.`);
  }
  checkIdentity(input?.source, `${name}.source`, errors);
  checkIdentity(input?.graph, `${name}.graph`, errors);
  checkIdentity(input?.inputIdentity, `${name}.inputIdentity`, errors);
  if (input?.source?.id !== SOURCE_ID || input?.source?.digest !== SOURCE_FIXTURE.digest) {
    push(errors, `${name}.source must use the current checker source identity.`);
  }
  if (input?.graph?.id !== DESCRIPTOR_IDENTITIES.graph.id || input?.graph?.digest !== DESCRIPTOR_IDENTITIES.graph.digest) {
    push(errors, `${name}.graph must use the source-backed graph descriptor.`);
  }
  if (input?.inputIdentity?.id !== DESCRIPTOR_IDENTITIES.input.id || input?.inputIdentity?.digest !== DESCRIPTOR_IDENTITIES.input.digest) {
    push(errors, `${name}.inputIdentity must use the source-backed invocation descriptor.`);
  }
  if (!sameArray(input?.phases, LIFECYCLE_PHASES)) push(errors, `${name}.phases must separate all ten lifecycle phases in order.`);
  if (!Array.isArray(input?.phaseIdentities) || input.phaseIdentities.length !== LIFECYCLE_PHASES.length) {
    push(errors, `${name}.phaseIdentities must contain one identity record per lifecycle phase.`);
  } else {
    for (const [index, phase] of input.phaseIdentities.entries()) {
      const location = `${name}.phaseIdentities[${index}]`;
      if (!isObject(phase)) {
        push(errors, `${location} must be an object.`);
        continue;
      }
      if (phase.phase !== LIFECYCLE_PHASES[index]) push(errors, `${location}.phase must preserve lifecycle order.`);
      if (phase.source !== input.source?.id) push(errors, `${location}.source must equal the source identity.`);
      if (phase.graph !== input.graph?.id) push(errors, `${location}.graph must equal the graph identity.`);
      if (phase.input !== input.inputIdentity?.id) push(errors, `${location}.input must equal the input identity.`);
    }
  }
  if (input?.phaseMix !== undefined) push(errors, `${name}.phaseMix must not mix compiler and execution phases.`);
  checkOracle(input?.oracle, `${name}.oracle`, errors, "host-oracle");
  checkBackend(input?.backendAvailable, input?.claims, name, errors);
  checkBaseline(input?.baseline, `${name}.baseline`, errors);
  if (input?.baseline?.primary !== "historical-w") push(errors, `${name}.baseline.primary must be historical-w.`);
  if (input?.baseline?.role !== "contextual-not-ranking") push(errors, `${name}.baseline.role must be contextual-not-ranking.`);
  if (input?.baseline?.recipe !== "equivalent") push(errors, `${name}.baseline.recipe must be equivalent.`);
  checkSamples(input?.samples, `${name}.samples`, errors, input?.backendAvailable);
}

function checkManifestDescriptor(descriptor, name, expectedKind, errors) {
  if (!isObject(descriptor)) {
    push(errors, `${name} must be an object.`);
    return;
  }
  if (descriptor.$schema !== "wbench/1-identity") push(errors, `${name}.$schema must be wbench/1-identity.`);
  if (descriptor.schema !== "wbench/1-identity") push(errors, `${name}.schema must be wbench/1-identity.`);
  if (descriptor.kind !== expectedKind) push(errors, `${name}.kind must be ${expectedKind}.`);
  requiredString(descriptor.id, `${name}.id`, errors);
  checkSource(descriptor.source, `${name}.source`, errors);
  if (expectedKind === "source-graph") {
    if (!Array.isArray(descriptor.nodes) || descriptor.nodes.length !== 1) push(errors, `${name}.nodes must contain one source node.`);
    if (!Array.isArray(descriptor.edges) || descriptor.edges.length !== 0) push(errors, `${name}.edges must be empty for a single-source graph.`);
    const node = descriptor.nodes?.[0];
    if (node?.path !== descriptor.source?.path || node?.digest !== descriptor.source?.digest) push(errors, `${name}.nodes[0] must preserve the source path and digest.`);
  } else {
    if (descriptor.invocation?.entry !== "canAcceptOrder") push(errors, `${name}.invocation.entry must be canAcceptOrder.`);
    const edit = descriptor.invocation?.edit;
    const recipe = descriptor.invocation?.recipe;
    if (edit?.kind !== EDIT_RECIPE.kind) push(errors, `${name}.invocation.edit.kind must be whitespace-only.`);
    if (edit?.sourcePath !== descriptor.source?.path) push(errors, `${name}.invocation.edit.sourcePath must preserve the source path.`);
    if (edit?.targetSymbol !== SOURCE_FIXTURE.symbol) push(errors, `${name}.invocation.edit.targetSymbol must identify canAcceptOrder.`);
    if (edit?.occurrence !== EDIT_RECIPE.occurrence) push(errors, `${name}.invocation.edit.occurrence must be one.`);
    if (edit?.match !== EDIT_RECIPE.match || edit?.replacement !== EDIT_RECIPE.replacement) push(errors, `${name}.invocation.edit must use the fixed whitespace-preserving replacement.`);
    if (edit?.semanticPreserving !== true) push(errors, `${name}.invocation.edit.semanticPreserving must be true.`);
    if (edit?.applyTo !== EDIT_RECIPE.applyTo) push(errors, `${name}.invocation.edit.applyTo must be temporary-copy.`);
    if (recipe?.operation !== EDIT_RECIPE.operation || recipe?.expectedMatches !== EDIT_RECIPE.expectedMatches || recipe?.temporaryCopy !== true) {
      push(errors, `${name}.invocation.recipe must define one fixed replace-once operation on a temporary copy.`);
    }
    if (recipe?.match !== EDIT_RECIPE.match || recipe?.replacement !== EDIT_RECIPE.replacement || recipe?.semanticPreserving !== true) {
      push(errors, `${name}.invocation.recipe must preserve semantics with the fixed replacement.`);
    }
    const sourcePath = repositoryPath(descriptor.source?.path);
    if (!sourcePath || !fs.existsSync(sourcePath)) return;
    const sourceText = fs.readFileSync(sourcePath, "utf8");
    const matches = sourceText.split(EDIT_RECIPE.match).length - 1;
    if (matches !== EDIT_RECIPE.occurrence) push(errors, `${name}.invocation.recipe match must occur exactly once in the source bytes.`);
    else {
      const editedText = sourceText.replace(EDIT_RECIPE.match, EDIT_RECIPE.replacement);
      if (editedText === sourceText) push(errors, `${name}.invocation.recipe must change the temporary copy.`);
      if (editedText.replace(/\s+/gu, " ") !== sourceText.replace(/\s+/gu, " ")) push(errors, `${name}.invocation.recipe must be semantic-preserving whitespace only.`);
      if (!editedText.includes(SOURCE_FIXTURE.symbol)) push(errors, `${name}.invocation.recipe must preserve the source symbol.`);
    }
  }
}

function checkDescriptorFile(identity, name, expectedKind, errors) {
  if (!checkIdentity(identity, name, errors, true)) return;
  const absolutePath = repositoryPath(identity.path);
  if (!absolutePath) {
    push(errors, `${name}.path must stay inside the repository.`);
    return;
  }
  if (!fs.existsSync(absolutePath) || !fs.statSync(absolutePath).isFile()) {
    push(errors, `${name}.path does not exist.`);
    return;
  }
  const actualDigest = fileDigest(identity.path);
  if (actualDigest !== identity.digest) push(errors, `${name}.digest is stale.`);
  try {
    const descriptor = JSON.parse(fs.readFileSync(absolutePath, "utf8"));
    if (descriptor.id !== identity.id) push(errors, `${name}.path descriptor id must match its identity.`);
    checkManifestDescriptor(descriptor, name, expectedKind, errors);
  } catch (error) {
    push(errors, `${name}.path is not valid JSON: ${error.message}`);
  }
}

function containsForbiddenInputKey(value, location, errors) {
  if (Array.isArray(value)) {
    value.forEach((item, index) => containsForbiddenInputKey(item, `${location}[${index}]`, errors));
    return;
  }
  if (!isObject(value)) return;
  for (const [key, item] of Object.entries(value)) {
    if (["expected", "expectedResult", "result"].includes(key)) {
      push(errors, `${location}.${key} must not echo an expected result.`);
    }
    containsForbiddenInputKey(item, `${location}.${key}`, errors);
  }
}

export function validateScenario(input) {
  const errors = [];
  if (!isObject(input)) {
    push(errors, "scenario must be an object.");
    return errors;
  }
  if (input.track === "language") checkLanguageScenario(input, "scenario", errors);
  else if (input.track === "compiler-lifecycle") checkCompilerLifecycle(input, "scenario", errors);
  else if (input.track === "documentation") {
    if (input.benchmarkDisposition !== "not-applicable") push(errors, "documentation benchmarkDisposition must be not-applicable.");
    requiredString(input.reason, "scenario.reason", errors);
    if (input.digestOnly !== true) push(errors, "documentation digestOnly must be true.");
    if (Object.prototype.hasOwnProperty.call(input, "profiles")) push(errors, "documentation must not define language source profiles.");
  }
  else push(errors, "scenario.track must be language, compiler-lifecycle or documentation.");
  if (!LANE_SET.has(input.lane)) push(errors, "scenario.lane must be equivalent or open.");
  containsForbiddenInputKey(input, "scenario", errors);
  return errors;
}

function checkTaskGraph(tasks, corpusIds, errors) {
  const expected = [
    "protocol",
    "seed-compiler-lifecycle",
    "core-language-units",
    "computer-language-benchmarks-game",
    "restaurant-composition",
  ];
  if (!Array.isArray(tasks) || !sameArray(tasks.map((task) => task?.id), expected)) {
    push(errors, "program.tasks must use the five BMD0 tasks in order.");
    return;
  }
  const ids = new Set(expected);
  const seen = new Set();
  for (const [index, task] of tasks.entries()) {
    const location = `program.tasks[${index}]`;
    if (!isObject(task)) {
      push(errors, `${location} must be an object.`);
      continue;
    }
    if (seen.has(task.id)) push(errors, `${location}.id duplicates ${task.id}.`);
    seen.add(task.id);
    if (!Array.isArray(task.dependencies)) push(errors, `${location}.dependencies must be an array.`);
    else for (const dependency of task.dependencies) {
      if (!ids.has(dependency)) push(errors, `${location}.dependencies contains an unknown task.`);
      if (dependency === task.id) push(errors, `${location}.dependencies must not self-reference.`);
      if (expected.indexOf(dependency) >= index) push(errors, `${location}.dependencies must point to an earlier task.`);
    }
    if (!Array.isArray(task.outputs) || task.outputs.length === 0) push(errors, `${location}.outputs must not be empty.`);
    if (!Array.isArray(task.adversarialCases) || task.adversarialCases.length === 0) push(errors, `${location}.adversarialCases must not be empty.`);
    else for (const caseId of task.adversarialCases) if (!corpusIds.has(caseId)) push(errors, `${location}.adversarialCases references an unknown case ${caseId}.`);
    requiredString(task.stopCondition, `${location}.stopCondition`, errors);
    if (task.id === "protocol" && (task.status !== "completed" || task.implementation !== "complete")) push(errors, "protocol task must be completed.");
    if (task.id === "seed-compiler-lifecycle" && (task.status !== "ready" || task.implementation !== "pending")) push(errors, "seed compiler lifecycle must be ready with pending implementation.");
    if (["core-language-units", "computer-language-benchmarks-game", "restaurant-composition"].includes(task.id) &&
        (task.status !== "blocked" || task.implementation !== "blocked")) push(errors, `${task.id} must be blocked.`);
    if (task.id === "core-language-units" || task.id === "computer-language-benchmarks-game") {
      if (!Array.isArray(task.blockedBy) || !task.blockedBy.includes("codegen")) push(errors, `${task.id} must be blocked by codegen.`);
    }
    if (task.id === "restaurant-composition") {
      if (!Array.isArray(task.blockedBy) || !task.blockedBy.includes("runtime/provider")) push(errors, "restaurant-composition must be blocked by runtime/provider.");
    }
  }
}

export function validateProgram(program, corpus = undefined) {
  const errors = [];
  if (!isObject(program)) return ["program must be an object."];
  if (program.schema !== SCHEMA_VERSION) push(errors, "program.schema must be wbench/1.");
  if (program.kind !== "program") push(errors, "program.kind must be program.");
  if (program.id !== "bmd0") push(errors, "program.id must be bmd0.");
  if (program.status !== "protocol-only") push(errors, "program.status must be protocol-only.");
  if (program.backend?.benchmarkRunnerAvailable !== false || program.backend?.resultsAllowed !== false) push(errors, "program benchmark runner must be unavailable and must not allow results.");
  if (!requiredString(program.backend?.note, "program.backend.note", errors)) {}
  const profiles = checkProgramProfiles(program.profiles, "program.profiles", errors);
  if (profiles.get("idiomatic")?.primary !== true || profiles.get("idiomatic")?.regression !== true) push(errors, "idiomatic must be primary and regression.");
  if (profiles.get("learner")?.primary !== false || profiles.get("learner")?.regression !== false ||
      profiles.get("frontier")?.primary !== false || profiles.get("frontier")?.regression !== false) {
    push(errors, "learner and frontier must not be primary or regression profiles.");
  }
  if (Object.prototype.hasOwnProperty.call(program, "results") || Object.prototype.hasOwnProperty.call(program, "timings")) {
    push(errors, "program must not contain runtime results or timings.");
  }
  if (!Array.isArray(program.lanes) || !sameArray(program.lanes.map((lane) => lane?.id), [...LANES])) push(errors, "program.lanes must define equivalent and open in order.");
  else {
    const equivalent = program.lanes[0];
    const open = program.lanes[1];
    for (const field of ["same algorithm", "same representation", "same validation", "same numeric contract", "same input"]) {
      if (!equivalent.requirements?.includes(field)) push(errors, `equivalent lane must require ${field}.`);
    }
    if (!open.requirements?.includes("record algorithm and representation changes")) push(errors, "open lane must record algorithm and representation changes.");
  }
  if (!sameArray(program.baselinePolicy?.defaultIndependent, ["c-clang", "rust"])) push(errors, "program baseline default must be C/Clang and Rust.");
  if (program.baselinePolicy?.gameRole !== "exploratory-never-authority") push(errors, "Benchmarks Game must remain exploratory and never authority.");
  const corpusIds = new Set(corpus?.cases?.map((item) => item?.id) ?? []);
  checkTaskGraph(program.tasks, corpusIds, errors);
  return errors;
}

export function validateManifest(manifest) {
  const errors = [];
  if (!isObject(manifest)) return ["manifest must be an object."];
  if (manifest.schema !== SCHEMA_VERSION) push(errors, "manifest.schema must be wbench/1.");
  if (manifest.kind !== "workload-manifest") push(errors, "manifest.kind must be workload-manifest.");
  if (manifest.id !== "bmd0-seed-check-lifecycle") push(errors, "manifest.id must be bmd0-seed-check-lifecycle.");
  if (manifest.status !== "ready") push(errors, "manifest.status must be ready.");
  if (manifest.track !== "compiler-lifecycle") push(errors, "manifest.track must be compiler-lifecycle.");
  if (!LANE_SET.has(manifest.lane)) push(errors, "manifest.lane must be equivalent or open.");
  if (manifest.benchmarkDisposition !== "compiler-lifecycle") push(errors, "manifest.benchmarkDisposition must be compiler-lifecycle.");
  if (manifest.backend?.benchmarkRunnerAvailable !== false || manifest.backend?.frontendAvailable !== true ||
      manifest.backend?.nativeBackendAvailable !== false || manifest.backend?.runtimeAvailable !== false ||
      manifest.backend?.execution !== "pending" || manifest.backend?.resultsAllowed !== false) {
    push(errors, "manifest backend must expose frontend only, keep native/runtime unavailable and remain result-free.");
  }
  checkSource(manifest.identity?.source, "manifest.identity.source", errors);
  const graph = manifest.identity?.graph;
  const input = manifest.identity?.input;
  checkDescriptorFile(graph, "manifest.identity.graph", "source-graph", errors);
  checkDescriptorFile(input, "manifest.identity.input", "invocation-edit-input", errors);
  if (manifest.command?.tool !== "w" || manifest.command?.operation !== "check") push(errors, "manifest.command must invoke w check.");
  if (!Array.isArray(manifest.command?.arguments) || manifest.command.arguments.length === 0) push(errors, "manifest.command.arguments must not be empty.");
  else if (manifest.command.arguments[0] !== SOURCE_FIXTURE.path) push(errors, "manifest.command.arguments must check the source-backed fixture.");
  if (manifest.languageProfiles?.applicability !== "not-applicable" || !requiredString(manifest.languageProfiles?.reason, "manifest.languageProfiles.reason", errors)) {
    push(errors, "manifest.languageProfiles must mark compiler lifecycle as not-applicable with a reason.");
  }
  if (manifest.baselinePolicy?.primary !== "historical-w" || manifest.baselinePolicy?.role !== "contextual-not-ranking") {
    push(errors, "manifest.baselinePolicy must use historical W as primary and C/Clang plus Rust as contextual-not-ranking.");
  }
  if (manifest.baselinePolicy?.recipe !== "equivalent") push(errors, "manifest.baselinePolicy.recipe must be equivalent.");
  if (!sameArray(manifest.baselinePolicy?.independent, ["c-clang", "rust"])) {
    push(errors, "manifest.baselinePolicy.independent must list C/Clang and Rust.");
  }
  if (manifest.baselinePolicy?.exceptionReason !== null && !requiredString(manifest.baselinePolicy?.exceptionReason, "manifest.baselinePolicy.exceptionReason", errors)) {}
  if (!Array.isArray(manifest.lifecycle) || !sameArray(manifest.lifecycle.map((phase) => phase?.id), [...LIFECYCLE_PHASES])) {
    push(errors, "manifest.lifecycle must contain all ten phases in order.");
  } else {
    for (const [index, phase] of manifest.lifecycle.entries()) {
      const location = `manifest.lifecycle[${index}]`;
      if (!isObject(phase)) {
        push(errors, `${location} must be an object.`);
        continue;
      }
      if (!PHASE_SET.has(phase.id)) push(errors, `${location}.id is not a lifecycle phase.`);
      if (!["ready", "pending", "blocked"].includes(phase.status)) push(errors, `${location}.status is invalid.`);
      const statusByPhase = {
        clean: "ready",
        "no-op": "ready",
        edit: "ready",
        frontend: "ready",
        hir: "blocked",
        lowering: "blocked",
        codegen: "blocked",
        link: "blocked",
        startup: "blocked",
        execution: "blocked",
      };
      if (statusByPhase[phase.id] && phase.status !== statusByPhase[phase.id]) push(errors, `${phase.id} must be ${statusByPhase[phase.id]} in the current backend boundary.`);
      if (phase.id === "execution" && (!Array.isArray(phase.blockedBy) || !phase.blockedBy.includes("runtime") || !phase.blockedBy.includes("provider"))) push(errors, "execution must be blocked by runtime and provider.");
      const blockerByPhase = {
        hir: "hir",
        lowering: "lowering",
        codegen: "codegen",
        link: "codegen",
        startup: "runtime",
      };
      if (blockerByPhase[phase.id] && (!Array.isArray(phase.blockedBy) || !phase.blockedBy.includes(blockerByPhase[phase.id]))) push(errors, `${phase.id} must be blocked by ${blockerByPhase[phase.id]}.`);
      const expectedAxes = {
        source: { id: SOURCE_ID, digest: manifest.identity?.source?.digest },
        graph,
        input,
      };
      for (const axis of ["source", "graph", "input"]) {
        const expected = expectedAxes[axis];
        checkIdentity(phase[axis], `${location}.${axis}`, errors);
        if (phase[axis]?.id !== expected?.id || phase[axis]?.digest !== expected?.digest) push(errors, `${location}.${axis} must preserve the manifest identity.`);
      }
    }
  }
  checkOracle(manifest.oracle, "manifest.oracle", errors, "host-structural");
  const policy = manifest.measurementPolicy;
  if (!isObject(policy)) push(errors, "manifest.measurementPolicy must be an object.");
  else {
    requiredBoolean(policy.correctnessFirst, "manifest.measurementPolicy.correctnessFirst", errors);
    if (policy.rawSamples !== "required-after-oracle") push(errors, "manifest.measurementPolicy.rawSamples must require the oracle first.");
    if (policy.warmup !== "record-before-samples") push(errors, "manifest.measurementPolicy.warmup must be recorded before samples.");
    if (policy.order !== "randomized-interleaved") push(errors, "manifest.measurementPolicy.order must be randomized-interleaved.");
    requiredString(policy.stopRule, "manifest.measurementPolicy.stopRule", errors);
    if (!Array.isArray(policy.environmentFields) || !["hardware", "kernel", "toolchain", "flags", "target", "provider", "source-digest", "artifact-digest", "input-digest"].every((field) => policy.environmentFields.includes(field))) {
      push(errors, "manifest.measurementPolicy.environmentFields must include the full provenance set.");
    }
    if (!Array.isArray(policy.metrics) || !["latency", "throughput", "memory", "allocations", "artifact-size"].every((metric) => policy.metrics.includes(metric))) {
      push(errors, "manifest.measurementPolicy.metrics must cover applicable latency, throughput, memory, allocations and artifact size.");
    }
    if (policy.semanticDeviations !== "record-all") push(errors, "manifest.measurementPolicy.semanticDeviations must record all deviations.");
    if (policy.safetyDisclosures !== "record-all") push(errors, "manifest.measurementPolicy.safetyDisclosures must record all disclosures.");
  }
  if (!Array.isArray(manifest.outputs) || manifest.outputs.length === 0) push(errors, "manifest.outputs must not be empty.");
  if (Object.prototype.hasOwnProperty.call(manifest, "results") || Object.prototype.hasOwnProperty.call(manifest, "timings")) push(errors, "manifest must not contain runtime results or timings.");
  return errors;
}

export function validateCorpus(corpus) {
  const errors = [];
  if (!isObject(corpus)) return ["corpus must be an object."];
  if (corpus.$schema !== "w-benchmark-driven-development-cases-1") push(errors, "corpus schema is invalid.");
  if (corpus.status !== "design-oracle-input") push(errors, "corpus status must be design-oracle-input.");
  if (corpus.id !== "BMD0") push(errors, "corpus id must be BMD0.");
  if (!sameArray(corpus.decisions, ["W-1487"])) push(errors, "corpus decisions must cite W-1487.");
  if (!Array.isArray(corpus.cases)) {
    push(errors, "corpus.cases must be an array.");
    return errors;
  }
  const ids = new Set();
  for (const [index, item] of corpus.cases.entries()) {
    const location = `corpus.cases[${index}]`;
    if (!isObject(item)) {
      push(errors, `${location} must be an object.`);
      continue;
    }
    if (ids.has(item.id)) push(errors, `${location}.id duplicates ${item.id}.`);
    ids.add(item.id);
    requiredString(item.id, `${location}.id`, errors);
    if (!["accepted", "rejected"].includes(item.kind)) push(errors, `${location}.kind must be accepted or rejected.`);
    if (!LANE_SET.has(item.lane)) push(errors, `${location}.lane is invalid.`);
    if (!sameArray(item.decisions, ["W-1487"])) push(errors, `${location}.decisions must cite W-1487.`);
    if (item.kind === "rejected") requiredString(item.violation, `${location}.violation`, errors);
    if (item.source !== undefined) checkCorpusSource(item.source, `${location}.source`, errors);
    const scenarioErrors = validateScenario({ ...item.input, track: item.track, lane: item.lane });
    if (item.kind === "accepted" && scenarioErrors.length > 0) push(errors, `${location} accepted scenario is invalid: ${scenarioErrors.join(" ")}`);
    if (item.kind === "rejected" && scenarioErrors.length === 0) push(errors, `${location} rejected scenario has no observable violation.`);
    containsForbiddenInputKey(item.input, `${location}.input`, errors);
  }
  for (const caseId of REQUIRED_CASES) if (!ids.has(caseId)) push(errors, `corpus is missing required case ${caseId}.`);
  return errors;
}

export function reduceCase(item) {
  const errors = validateScenario({ ...item.input, track: item.track, lane: item.lane });
  return {
    id: item.id,
    classification: errors.length === 0 ? "accepted" : "rejected",
    errors,
  };
}

export function reduceCorpus(corpus) {
  return (corpus.cases ?? []).map(reduceCase);
}

export function loadBmdDocuments() {
  const read = (relativePath) => JSON.parse(fs.readFileSync(path.resolve(ROOT, relativePath), "utf8"));
  return {
    schema: read("benchmarks/wbench-1.schema.json"),
    program: read("benchmarks/program.json"),
    manifest: read("benchmarks/seed-check-lifecycle.manifest.json"),
    corpus: read("tooling/benchmark-driven-development-cases.json"),
  };
}
