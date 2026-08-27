import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

export const ROOT = path.resolve(import.meta.dir, "..");
export const LANGUAGE_CATALOG_VERSION = "bmd3/1";
export const LANGUAGE_CATALOG_ID = "w-language-catalog";
export const BYTE_SCAN_MANIFEST_ID = "bmd3-byte-scan-view";
export const BYTE_SCAN_MAX_BYTES = 64 * 1024 * 1024;
export const BYTE_SCAN_OUTPUT_SHAPE = "{\"bytes\":\"<u64>\",\"matches\":\"<u64>\"}";
export const BYTE_SCAN_PROFILE_DISCLOSURES = Object.freeze([
  "unsafe",
  "ffi",
  "targetSpecialization",
  "manualLayout",
  "algorithm",
  "legibility",
]);

export const LANGUAGE_STRATA = Object.freeze([
  "scalar/control",
  "borrow/memory",
  "collections/text",
  "abstraction/error",
  "concurrency/service",
  "I/O/data",
  "numeric/accelerator",
]);

export const LANGUAGE_UNIT_IDS = Object.freeze([
  "integer-bit-mix",
  "branch-enum-dispatch",
  "call-generic-specialization",
  "byte-scan-view",
  "copy-move-buffer",
  "allocation-lifecycle",
  "array-transform-reduce",
  "hash-table-mixed",
  "unicode-scalar-grapheme",
  "option-result-pipeline",
  "protocol-dispatch",
  "json-adapter",
  "task-tree-join",
  "bounded-channel-pipeline",
  "local-service-roundtrip",
  "mapped-file-scan",
  "buffered-file-copy",
  "database-row-materialization",
  "float-reduction-modes",
  "matrix-small-gemm",
  "tensor-strided-reduction",
]);

const STRATUM_SET = new Set(LANGUAGE_STRATA);
const UNIT_SET = new Set(LANGUAGE_UNIT_IDS);
const DISCLOSURE_SET = new Set(BYTE_SCAN_PROFILE_DISCLOSURES);
const DIGEST_PATTERN = /^sha256:[0-9a-f]{64}$/u;
const U64_PATTERN = /^(?:0|[1-9][0-9]*)$/u;
const REQUIRED_UNIT_W_IDS = Object.freeze({
  "byte-scan-view": ["W-1472"],
  "copy-move-buffer": ["W-1472"],
  "allocation-lifecycle": ["W-406", "W-413", "W-1333"],
  "mapped-file-scan": ["W-1473"],
});

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function hasOwn(value, key) {
  return isObject(value) && Object.prototype.hasOwnProperty.call(value, key);
}

function push(errors, message) {
  errors.push(message);
}

function requiredString(value, name, errors) {
  if (typeof value !== "string" || value.trim() === "") {
    push(errors, name + " must be a non-empty string.");
    return false;
  }
  return true;
}

function checkExactKeys(value, name, keys, errors) {
  if (!isObject(value)) return false;
  const actual = Object.keys(value).sort();
  const expected = [...keys].sort();
  if (actual.length !== expected.length || actual.some((key, index) => key !== expected[index])) {
    push(errors, name + " must use a closed object shape.");
    return false;
  }
  return true;
}

function checkStringArray(value, name, errors, minimum = 1) {
  if (!Array.isArray(value) || value.length < minimum) {
    push(errors, name + " must contain at least " + minimum + " non-empty string(s).");
    return false;
  }
  const seen = new Set();
  for (const [index, item] of value.entries()) {
    if (requiredString(item, name + "[" + index + "]", errors) && seen.has(item)) {
      push(errors, name + " must not contain duplicates.");
    }
    seen.add(item);
  }
  return true;
}

function checkDigest(value, name, errors) {
  if (typeof value !== "string" || !DIGEST_PATTERN.test(value)) {
    push(errors, name + " must be a lowercase sha256 digest.");
    return false;
  }
  return true;
}

function repositoryPath(relativePath) {
  if (typeof relativePath !== "string" || relativePath.trim() === "") return undefined;
  const absolutePath = path.resolve(ROOT, relativePath);
  const relative = path.relative(ROOT, absolutePath);
  if (relative === "" || relative === ".." || relative.startsWith(".." + path.sep) || path.isAbsolute(relative)) {
    return undefined;
  }
  return absolutePath;
}

function fileDigest(relativePath) {
  const absolutePath = repositoryPath(relativePath);
  if (!absolutePath || !fs.existsSync(absolutePath) || !fs.statSync(absolutePath).isFile()) return undefined;
  return "sha256:" + crypto.createHash("sha256").update(fs.readFileSync(absolutePath)).digest("hex");
}

function checkFileDigest(relativePath, expectedDigest, name, errors) {
  const absolutePath = repositoryPath(relativePath);
  if (!absolutePath) {
    push(errors, name + ".path must remain inside the repository.");
    return;
  }
  if (!fs.existsSync(absolutePath) || !fs.statSync(absolutePath).isFile()) {
    push(errors, name + ".path must identify an existing file.");
    return;
  }
  if (expectedDigest !== fileDigest(relativePath)) push(errors, name + ".digest is stale.");
}

function checkU64(value, name, errors) {
  if (typeof value !== "string" || !U64_PATTERN.test(value)) {
    push(errors, name + " must be a canonical decimal u64 string.");
    return undefined;
  }
  try {
    const parsed = BigInt(value);
    if (parsed > ((1n << 64n) - 1n)) throw new RangeError();
    return parsed;
  } catch {
    push(errors, name + " must fit in u64.");
    return undefined;
  }
}

function checkBoolean(value, name, errors) {
  if (typeof value !== "boolean") push(errors, name + " must be boolean.");
}

function checkStatus(value, name, allowed, errors) {
  if (!allowed.includes(value)) push(errors, name + " must be " + allowed.join(" or ") + ".");
}

function checkReadiness(readiness, name, errors, expectedStatus) {
  const keys = ["status", "source", "oracle", "codegen", "runtime", "performance", "blockers"];
  if (!checkExactKeys(readiness, name, keys, errors)) return;
  if (readiness.status !== expectedStatus) push(errors, name + ".status must be " + expectedStatus + ".");
  checkStatus(readiness.source, name + ".source", ["reserved", "source-shaped"], errors);
  checkStatus(readiness.oracle, name + ".oracle", ["reserved", "host-ready"], errors);
  for (const field of ["codegen", "runtime", "performance"]) {
    if (readiness[field] !== "blocked") push(errors, name + "." + field + " must be blocked.");
  }
  checkStringArray(readiness.blockers, name + ".blockers", errors);
  if (readiness.blockers?.includes("performance")) {
    push(errors, name + ".blockers must name the missing language-benchmark-runner, not performance alone.");
  }
}

function checkBaselineRole(value, name, errors) {
  if (!checkExactKeys(value, name, ["primary", "independent", "role", "futurePerformanceRole", "futurePerformanceRecipe", "recipe"], errors)) return;
  if (value.primary !== "historical-w") push(errors, name + ".primary must be historical-w.");
  if (JSON.stringify(value.independent) !== JSON.stringify(["c11", "rust"])) {
    push(errors, name + ".independent must list c11 and rust.");
  }
  if (value.role !== "correctness-reference-no-ranking") push(errors, name + ".role must be correctness-reference-no-ranking.");
  if (value.futurePerformanceRole !== "independent-comparison-after-equivalence") {
    push(errors, name + ".futurePerformanceRole must be independent-comparison-after-equivalence.");
  }
  if (value.futurePerformanceRecipe !== "pin-toolchain-and-equivalent-recipe") {
    push(errors, name + ".futurePerformanceRecipe must pin the toolchain and equivalent recipe.");
  }
  if (value.recipe !== "equivalent") push(errors, name + ".recipe must be equivalent.");
}

function checkRestaurantCrosspoint(value, name, errors) {
  if (!checkExactKeys(value, name, ["status", "reference", "crosspoint"], errors)) return;
  if (value.status !== "separate-track") push(errors, name + ".status must be separate-track.");
  requiredString(value.reference, name + ".reference", errors);
  requiredString(value.crosspoint, name + ".crosspoint", errors);
}

function checkCatalogOracle(value, name, errors) {
  if (!checkExactKeys(value, name, ["kind", "status", "independent", "output"], errors)) return;
  if (value.kind !== "host-independent") push(errors, name + ".kind must be host-independent.");
  if (value.status !== "declared") push(errors, name + ".status must be declared.");
  if (value.independent !== true) push(errors, name + ".independent must be true.");
  if (value.output !== "canonical-json-u64") push(errors, name + ".output must be canonical-json-u64.");
}

export function validateLanguageCatalog(catalog) {
  const errors = [];
  if (!isObject(catalog)) return ["catalog must be an object."];
  checkExactKeys(catalog, "catalog", [
    "$schema", "schema", "kind", "id", "version", "status", "track",
    "benchmarkDisposition", "strata", "units",
  ], errors);
  if (catalog.$schema !== "./wbench-1.schema.json") push(errors, "catalog.$schema must point to wbench-1.schema.json.");
  if (catalog.schema !== "wbench/1") push(errors, "catalog.schema must be wbench/1.");
  if (catalog.kind !== "language-catalog") push(errors, "catalog.kind must be language-catalog.");
  if (catalog.id !== LANGUAGE_CATALOG_ID) push(errors, "catalog.id must be " + LANGUAGE_CATALOG_ID + ".");
  if (catalog.version !== LANGUAGE_CATALOG_VERSION) push(errors, "catalog.version must be " + LANGUAGE_CATALOG_VERSION + ".");
  if (catalog.status !== "ready") push(errors, "catalog.status must be ready.");
  if (catalog.track !== "language") push(errors, "catalog.track must be language.");
  if (catalog.benchmarkDisposition !== "required") push(errors, "catalog.benchmarkDisposition must be required.");

  if (!Array.isArray(catalog.strata) || catalog.strata.length !== LANGUAGE_STRATA.length) {
    push(errors, "catalog.strata must contain exactly seven strata.");
  }
  const strataById = new Map();
  for (const [index, stratum] of (catalog.strata ?? []).entries()) {
    const location = "catalog.strata[" + index + "]";
    if (!isObject(stratum)) {
      push(errors, location + " must be an object.");
      continue;
    }
    checkExactKeys(stratum, location, ["id", "units"], errors);
    if (!STRATUM_SET.has(stratum.id)) push(errors, location + ".id is not a declared stratum.");
    if (strataById.has(stratum.id)) push(errors, location + ".id duplicates " + stratum.id + ".");
    strataById.set(stratum.id, stratum);
    checkStringArray(stratum.units, location + ".units", errors, 3);
    if (stratum.units?.length !== 3) push(errors, location + ".units must contain exactly three IDs.");
  }
  if (JSON.stringify(catalog.strata?.map((item) => item?.id)) !== JSON.stringify(LANGUAGE_STRATA)) {
    push(errors, "catalog.strata must use the seven declared strata in order.");
  }

  if (!Array.isArray(catalog.units) || catalog.units.length !== LANGUAGE_UNIT_IDS.length) {
    push(errors, "catalog.units must contain exactly 21 units.");
  }
  const unitsById = new Map();
  const unitKeys = [
    "id", "stratum", "benchmarkDisposition", "featureOwners", "wIds", "scope", "oracle",
    "inputClasses", "futureMetrics", "baselineRole", "readiness", "restaurantCrosspoint", "stopCondition",
  ];
  for (const [index, unit] of (catalog.units ?? []).entries()) {
    const location = "catalog.units[" + index + "]";
    if (!isObject(unit)) {
      push(errors, location + " must be an object.");
      continue;
    }
    checkExactKeys(unit, location, unitKeys, errors);
    if (!UNIT_SET.has(unit.id)) push(errors, location + ".id is not a declared language unit.");
    if (unitsById.has(unit.id)) push(errors, location + ".id duplicates " + unit.id + ".");
    unitsById.set(unit.id, unit);
    if (!STRATUM_SET.has(unit.stratum)) push(errors, location + ".stratum is not declared.");
    if (unit.benchmarkDisposition !== "required") push(errors, location + ".benchmarkDisposition must be required.");
    checkStringArray(unit.featureOwners, location + ".featureOwners", errors);
    checkStringArray(unit.wIds, location + ".wIds", errors);
    for (const wId of unit.wIds ?? []) {
      if (!/^W-[0-9]{3,}$/u.test(wId)) push(errors, location + ".wIds must use W-ID syntax.");
    }
    for (const wId of REQUIRED_UNIT_W_IDS[unit.id] ?? []) {
      if (!unit.wIds?.includes(wId)) push(errors, location + ".wIds must include " + wId + " for this unit.");
    }
    requiredString(unit.scope, location + ".scope", errors);
    checkCatalogOracle(unit.oracle, location + ".oracle", errors);
    checkStringArray(unit.inputClasses, location + ".inputClasses", errors);
    checkStringArray(unit.futureMetrics, location + ".futureMetrics", errors);
    checkBaselineRole(unit.baselineRole, location + ".baselineRole", errors);
    checkReadiness(unit.readiness, location + ".readiness", errors,
      unit.id === "byte-scan-view" ? "source-oracle-ready" : "reserved");
    checkRestaurantCrosspoint(unit.restaurantCrosspoint, location + ".restaurantCrosspoint", errors);
    requiredString(unit.stopCondition, location + ".stopCondition", errors);
  }
  if (JSON.stringify(catalog.units?.map((item) => item?.id)) !== JSON.stringify(LANGUAGE_UNIT_IDS)) {
    push(errors, "catalog.units must use exactly the 21 declared IDs in order.");
  }
  for (const stratum of catalog.strata ?? []) {
    const expected = (catalog.units ?? []).filter((unit) => unit?.stratum === stratum?.id).map((unit) => unit.id);
    if (JSON.stringify(stratum?.units) !== JSON.stringify(expected)) {
      push(errors, "catalog.strata." + stratum?.id + " must list its unit IDs in catalog order.");
    }
  }
  return errors;
}

function checkSourceShape(profile, name, errors) {
  if (!isObject(profile)) return;
  if (/\b(?:sleep|precomputed|constant output|bypass)\b/iu.test(profile.shape ?? "")) {
    push(errors, name + ".shape must not declare artificial work or a precomputed output.");
  }
  requiredString(profile.path, name + ".path", errors);
  requiredString(profile.symbol, name + ".symbol", errors);
  checkDigest(profile.digest, name + ".digest", errors);
  checkFileDigest(profile.path, profile.digest, name, errors);
  const absolutePath = repositoryPath(profile.path);
  if (!absolutePath || !fs.existsSync(absolutePath)) return;
  const source = fs.readFileSync(absolutePath, "utf8");
  if (!source.includes("view Bytes")) push(errors, name + " must use view Bytes.");
  if (!source.includes("delimiter")) push(errors, name + " must use a runtime delimiter.");
  if (!source.includes("matches")) push(errors, name + " must compute matches.");
  const comparesDelimiter = profile.id === "frontier"
    ? /equalLanes\(delimiterVector\)/u.test(source)
    : /byte\s*==\s*delimiter/u.test(source);
  if (!comparesDelimiter) push(errors, name + " must compare each input byte with the runtime delimiter.");
  if (!/matches\s*\+=/u.test(source)) push(errors, name + " must accumulate matches from the input scan.");
  if (!/bytes\s*:\s*(?:u64\()?source\.count/u.test(source)) push(errors, name + " must derive bytes from the runtime input extent.");
  if (/\b(?:bytes|matches)\s*:\s*[0-9]+\b/u.test(source)) push(errors, name + " must not return a precomputed numeric output.");
  if (/precomputed|constant output|return\s+\{\s*bytes/iu.test(source)) {
    push(errors, name + " must not precompute output.");
  }
  if (profile.id === "learner" && (!source.includes("while") || !/source\s*\[\s*index\s*\]/u.test(source))) {
    push(errors, name + " learner shape must use checked loop/indexing.");
  }
  if (profile.id === "idiomatic" && !source.includes("for byte in source")) {
    push(errors, name + " idiomatic shape must use safe for byte in source.");
  }
  if (profile.id === "frontier" && (!source.includes("loadPartial") || !source.includes("countTrue"))) {
    push(errors, name + " frontier shape must disclose the SIMD partial-load/count path.");
  }
  if (/\b(?:sleep|uselessWork|worseFlags)\b/iu.test(source)) push(errors, name + " must not contain artificial learner work.");
}

function checkProfileDisclosures(disclosures, name, errors) {
  if (!checkExactKeys(disclosures, name, BYTE_SCAN_PROFILE_DISCLOSURES, errors)) return;
  for (const field of BYTE_SCAN_PROFILE_DISCLOSURES) requiredString(disclosures[field], name + "." + field, errors);
  for (const field of Object.keys(disclosures ?? {})) {
    if (!DISCLOSURE_SET.has(field)) push(errors, name + " has an unknown disclosure axis.");
  }
}

function checkInputClasses(inputs, name, errors) {
  const expected = [
    "empty", "boundary", "ascii", "utf8-mixed", "mixed-binary", "dense", "sparse", "no-matches",
  ];
  if (!Array.isArray(inputs) || inputs.length !== expected.length) {
    push(errors, name + " must contain the eight deterministic input classes.");
    return;
  }
  const seen = new Set();
  for (const [index, item] of inputs.entries()) {
    const location = name + "[" + index + "]";
    if (!isObject(item)) {
      push(errors, location + " must be an object.");
      continue;
    }
    checkExactKeys(item, location, ["id", "kind", "sizes", "delimiterValues"], errors);
    if (!expected.includes(item.id)) push(errors, location + ".id is not an expected class.");
    if (seen.has(item.id)) push(errors, location + ".id duplicates " + item.id + ".");
    seen.add(item.id);
    requiredString(item.kind, location + ".kind", errors);
    if (!Array.isArray(item.sizes) || item.sizes.length === 0) push(errors, location + ".sizes must not be empty.");
    for (const size of item.sizes ?? []) {
      if (!Number.isInteger(size) || size < 0 || size > BYTE_SCAN_MAX_BYTES) push(errors, location + ".sizes contains an invalid bound.");
    }
    checkStringArray(item.delimiterValues, location + ".delimiterValues", errors);
    for (const delimiter of item.delimiterValues ?? []) {
      if (!/^(?:0|[1-9][0-9]{0,2})$/u.test(delimiter) || Number(delimiter) > 255) {
        push(errors, location + ".delimiterValues must use decimal bytes from 0 through 255.");
      }
    }
  }
  if (JSON.stringify([...seen]) !== JSON.stringify(expected)) push(errors, name + " classes must be unique and ordered.");
  const allSizes = (inputs ?? []).flatMap((item) => item?.sizes ?? []);
  if (!allSizes.includes(0) || !allSizes.includes(BYTE_SCAN_MAX_BYTES)) push(errors, name + " must include empty and maximum boundary sizes.");
}

function checkBaselineEntry(baseline, name, errors) {
  if (!isObject(baseline)) {
    push(errors, name + " must be an object.");
    return;
  }
  checkExactKeys(baseline, name, [
    "id", "path", "digest", "recipePath", "recipeDigest", "language", "compileSteps", "invoke", "flags", "edition", "role", "futurePerformanceRole", "futurePerformanceRecipe", "output",
  ], errors);
  if (!new Set(["c11", "rust"]).has(baseline.id)) push(errors, name + ".id must be c11 or rust.");
  requiredString(baseline.path, name + ".path", errors);
  checkDigest(baseline.digest, name + ".digest", errors);
  checkFileDigest(baseline.path, baseline.digest, name, errors);
  if (baseline.id === "c11") {
    requiredString(baseline.recipePath, name + ".recipePath", errors);
    checkDigest(baseline.recipeDigest, name + ".recipeDigest", errors);
    checkFileDigest(baseline.recipePath, baseline.recipeDigest, name + ".recipe", errors);
    if (JSON.stringify(baseline.compileSteps) !== JSON.stringify([
      ["cmake", "-S", "<source-dir>", "-B", "<build-dir>", "-DCMAKE_BUILD_TYPE=Release"],
      ["cmake", "--build", "<build-dir>", "--config", "Release"],
    ])) {
      push(errors, name + ".compileSteps must match the versioned CMake configure and build recipe.");
    }
  } else if (baseline.id === "rust" && (baseline.recipePath !== null || baseline.recipeDigest !== null)) {
    push(errors, name + ".recipePath and recipeDigest must be null for the direct rustc recipe.");
  }
  requiredString(baseline.language, name + ".language", errors);
  if (!Array.isArray(baseline.compileSteps) || baseline.compileSteps.length === 0) {
    push(errors, name + ".compileSteps must contain at least one argv step.");
  } else {
    for (const [index, step] of baseline.compileSteps.entries()) {
      checkStringArray(step, name + ".compileSteps[" + index + "]", errors);
      if (Array.isArray(step) && step.some((item) => /[;&|`$]/u.test(item))) {
        push(errors, name + ".compileSteps must use argv tokens without shell syntax.");
      }
    }
  }
  checkStringArray(baseline.invoke, name + ".invoke", errors);
  if (!baseline.invoke?.includes("<path>") || !baseline.invoke?.includes("<delimiter>")) {
    push(errors, name + ".invoke must accept an explicit path and delimiter.");
  }
  checkStringArray(baseline.flags, name + ".flags", errors);
  if (baseline.id === "rust" && baseline.edition !== "2021") push(errors, name + ".edition must be 2021.");
  if (baseline.id === "c11" && baseline.edition !== null) push(errors, name + ".edition must be null for C11.");
  if (baseline.compileSteps?.flat()?.some((item) => /cargo|lockfile/iu.test(item))) push(errors, name + ".compileSteps must not use Cargo or a lockfile.");
  if (baseline.role !== "correctness-reference-no-ranking") push(errors, name + ".role must be correctness-reference-no-ranking.");
  if (baseline.futurePerformanceRole !== "independent-comparison-after-equivalence") {
    push(errors, name + ".futurePerformanceRole must be independent-comparison-after-equivalence.");
  }
  if (baseline.futurePerformanceRecipe !== "pin-toolchain-and-equivalent-recipe") {
    push(errors, name + ".futurePerformanceRecipe must pin the toolchain and equivalent recipe.");
  }
  if (baseline.output !== "canonical-json-u64") push(errors, name + ".output must be canonical-json-u64.");
}

export function validateByteScanManifest(manifest, catalog = undefined) {
  const errors = [];
  if (!isObject(manifest)) return ["byte-scan manifest must be an object."];
  checkExactKeys(manifest, "manifest", [
    "$schema", "schema", "kind", "id", "version", "status", "track", "benchmarkDisposition", "unitId", "stratum",
    "lane", "frontierLane", "backend", "scope", "profiles", "oracle", "inputs", "baselines", "futureMetrics",
    "readiness", "restaurantCrosspoint", "provenance", "stopCondition",
  ], errors);
  if (manifest.$schema !== "./wbench-1.schema.json") push(errors, "manifest.$schema must point to wbench-1.schema.json.");
  if (manifest.schema !== "wbench/1") push(errors, "manifest.schema must be wbench/1.");
  if (manifest.kind !== "language-workload-manifest") push(errors, "manifest.kind must be language-workload-manifest.");
  if (manifest.id !== BYTE_SCAN_MANIFEST_ID) push(errors, "manifest.id must be " + BYTE_SCAN_MANIFEST_ID + ".");
  if (manifest.version !== LANGUAGE_CATALOG_VERSION) push(errors, "manifest.version must be " + LANGUAGE_CATALOG_VERSION + ".");
  if (manifest.status !== "source-oracle-ready") push(errors, "manifest.status must be source-oracle-ready.");
  if (manifest.track !== "language") push(errors, "manifest.track must be language.");
  if (manifest.benchmarkDisposition !== "required") push(errors, "manifest.benchmarkDisposition must be required.");
  if (manifest.unitId !== "byte-scan-view") push(errors, "manifest.unitId must be byte-scan-view.");
  if (manifest.stratum !== "borrow/memory") push(errors, "manifest.stratum must be borrow/memory.");
  if (manifest.lane !== "equivalent" || manifest.frontierLane !== "open") push(errors, "manifest lanes must be equivalent with frontier open.");

  if (catalog) {
    const catalogErrors = validateLanguageCatalog(catalog);
    if (catalogErrors.length > 0) push(errors, "manifest catalog is invalid: " + catalogErrors.join(" "));
    const unit = catalog.units?.find((item) => item?.id === manifest.unitId);
    if (!unit) push(errors, "manifest unitId must identify a catalog unit.");
    else if (unit.stratum !== manifest.stratum) push(errors, "manifest.stratum must match catalog unit.");
  }

  if (!checkExactKeys(manifest.backend, "manifest.backend", [
    "correctnessRunnerAvailable", "benchmarkRunnerAvailable", "nativeBackendAvailable", "runtimeAvailable", "languageResultsAllowed", "timingResultsAllowed", "note",
    ], errors)) {
    // The closed shape error is sufficient. Field checks below still provide a useful first error.
  }
  if (manifest.backend?.correctnessRunnerAvailable !== true || manifest.backend?.benchmarkRunnerAvailable !== false ||
      manifest.backend?.nativeBackendAvailable !== false ||
      manifest.backend?.runtimeAvailable !== false || manifest.backend?.languageResultsAllowed !== false ||
      manifest.backend?.timingResultsAllowed !== false) {
    push(errors, "manifest.backend must block W language, runtime and timing results.");
  }
  requiredString(manifest.backend?.note, "manifest.backend.note", errors);
  if (!checkExactKeys(manifest.scope, "manifest.scope", ["kernel", "representation", "output", "setup"], errors)) {}
  if (manifest.scope?.representation !== "view Bytes") push(errors, "manifest.scope.representation must be view Bytes.");
  if (manifest.scope?.output !== BYTE_SCAN_OUTPUT_SHAPE) push(errors, "manifest.scope.output must use the canonical byte-scan JSON shape.");
  if (manifest.scope?.setup !== "input file creation is outside future timing") push(errors, "manifest.scope.setup must exclude input setup from future timing.");

  const expectedProfiles = ["learner", "idiomatic", "frontier"];
  if (!Array.isArray(manifest.profiles) || manifest.profiles.length !== expectedProfiles.length) {
    push(errors, "manifest.profiles must contain learner, idiomatic and frontier.");
  }
  const profilesById = new Map();
  for (const [index, profile] of (manifest.profiles ?? []).entries()) {
    const location = "manifest.profiles[" + index + "]";
    if (!isObject(profile)) {
      push(errors, location + " must be an object.");
      continue;
    }
    checkExactKeys(profile, location, [
      "id", "path", "symbol", "digest", "shape", "lane", "sameAlgorithm", "sameRepresentation",
      "samePhysicalStrategy", "sameValidation", "sameNumericContract", "sameInput", "disclosures",
    ], errors);
    if (!expectedProfiles.includes(profile.id)) push(errors, location + ".id is not a required profile.");
    if (profilesById.has(profile.id)) push(errors, location + ".id duplicates " + profile.id + ".");
    profilesById.set(profile.id, profile);
    requiredString(profile.shape, location + ".shape", errors);
    checkSourceShape(profile, location, errors);
    checkProfileDisclosures(profile.disclosures, location + ".disclosures", errors);
    const isFrontier = profile.id === "frontier";
    if (profile.lane !== (isFrontier ? "open" : "equivalent")) push(errors, location + ".lane is inconsistent with the profile.");
    for (const field of ["sameAlgorithm", "sameRepresentation", "samePhysicalStrategy", "sameValidation", "sameNumericContract", "sameInput"]) {
      checkBoolean(profile[field], location + "." + field, errors);
    }
    if (isFrontier) {
      if (profile.sameAlgorithm !== true || profile.sameRepresentation !== true || profile.samePhysicalStrategy !== false || profile.sameValidation !== true ||
          profile.sameNumericContract !== true || profile.sameInput !== true) {
        push(errors, location + " must preserve the logical algorithm and representation while disclosing an open physical strategy.");
      }
      if (profile.disclosures?.targetSpecialization !== "none") push(errors, location + " target specialization must be none for portable SIMD with scalar fallback.");
    } else if (["sameAlgorithm", "sameRepresentation", "samePhysicalStrategy", "sameValidation", "sameNumericContract", "sameInput"].some((field) => profile[field] !== true)) {
      push(errors, location + " learner and idiomatic profiles must be equivalent.");
    }
  }
  if (JSON.stringify([...profilesById.keys()]) !== JSON.stringify(expectedProfiles)) push(errors, "manifest.profiles must use each profile exactly once in order.");

  if (!checkExactKeys(manifest.oracle, "manifest.oracle", [
    "kind", "path", "symbol", "correctnessRecord", "complete", "beforeSamples", "independent", "runtime", "setup", "output",
  ], errors)) {}
  if (manifest.oracle?.kind !== "host-oracle") push(errors, "manifest.oracle.kind must be host-oracle.");
  requiredString(manifest.oracle?.path, "manifest.oracle.path", errors);
  requiredString(manifest.oracle?.symbol, "manifest.oracle.symbol", errors);
  checkFileDigest(manifest.oracle?.path, manifest.provenance?.oracleDigest, "manifest.oracle", errors);
  if (manifest.oracle?.complete !== true || manifest.oracle?.beforeSamples !== true || manifest.oracle?.independent !== true) {
    push(errors, "manifest.oracle must be independent, complete and before samples.");
  }
  if (manifest.oracle?.runtime !== "unavailable") push(errors, "manifest.oracle.runtime must be unavailable.");
  if (manifest.oracle?.setup !== "input setup outside future timing") push(errors, "manifest.oracle.setup must exclude setup from future timing.");
  if (manifest.oracle?.output !== BYTE_SCAN_OUTPUT_SHAPE) push(errors, "manifest.oracle.output must use canonical JSON.");

  if (!checkExactKeys(manifest.inputs, "manifest.inputs", [
    "pathArgument", "delimiter", "delimiterValues", "maximumBytes", "classes", "setup",
  ], errors)) {}
  if (manifest.inputs?.pathArgument !== true) push(errors, "manifest.inputs.pathArgument must be true.");
  if (manifest.inputs?.delimiter !== "runtime-parameter") push(errors, "manifest.inputs.delimiter must be runtime-parameter.");
  checkStringArray(manifest.inputs?.delimiterValues, "manifest.inputs.delimiterValues", errors);
  if (manifest.inputs?.maximumBytes !== BYTE_SCAN_MAX_BYTES) push(errors, "manifest.inputs.maximumBytes must be 64 MiB.");
  checkInputClasses(manifest.inputs?.classes, "manifest.inputs.classes", errors);
  if (manifest.inputs?.setup !== "deterministic temporary files outside future timing") push(errors, "manifest.inputs.setup must be outside future timing.");

  if (!Array.isArray(manifest.baselines) || manifest.baselines.length !== 2) push(errors, "manifest.baselines must contain c11 and rust.");
  const baselineIds = new Set();
  for (const [index, baseline] of (manifest.baselines ?? []).entries()) {
    const location = "manifest.baselines[" + index + "]";
    checkBaselineEntry(baseline, location, errors);
    if (baselineIds.has(baseline?.id)) push(errors, location + ".id duplicates.");
    baselineIds.add(baseline?.id);
  }
  if (JSON.stringify([...baselineIds]) !== JSON.stringify(["c11", "rust"])) push(errors, "manifest.baselines must use c11 and rust in order.");
  checkStringArray(manifest.futureMetrics, "manifest.futureMetrics", errors);
  checkReadiness(manifest.readiness, "manifest.readiness", errors, "source-oracle-ready");
  checkRestaurantCrosspoint(manifest.restaurantCrosspoint, "manifest.restaurantCrosspoint", errors);

  if (!checkExactKeys(manifest.provenance, "manifest.provenance", ["catalogPath", "sourceRoot", "oraclePath", "oracleDigest"], errors)) {}
  if (manifest.provenance?.catalogPath !== "benchmarks/language-catalog.json") push(errors, "manifest.provenance.catalogPath must identify the catalog.");
  if (manifest.provenance?.sourceRoot !== "benchmarks/byte-scan-view") push(errors, "manifest.provenance.sourceRoot must identify the source root.");
  if (manifest.provenance?.oraclePath !== manifest.oracle?.path) push(errors, "manifest.provenance.oraclePath must match the oracle path.");
  checkDigest(manifest.provenance?.oracleDigest, "manifest.provenance.oracleDigest", errors);
  requiredString(manifest.stopCondition, "manifest.stopCondition", errors);
  if (/\bW\s+(?:timing|performance|results?)\s+(?:are|is)\s+(?:allowed|available|recorded|reported)\b/iu.test(manifest.stopCondition)) {
    push(errors, "manifest.stopCondition must not claim W timing or performance.");
  }
  for (const key of ["results", "timings", "samples", "claim"]) if (hasOwn(manifest, key)) push(errors, "manifest must not contain " + key + ".");
  return errors;
}

export function canonicalU64(value) {
  const parsed = typeof value === "bigint" ? value : BigInt(value);
  if (parsed < 0n || parsed > ((1n << 64n) - 1n)) throw new RangeError("u64 out of range");
  return parsed.toString(10);
}

export function calculateByteScan(input, delimiter) {
  if (!(input instanceof Uint8Array) && !Buffer.isBuffer(input)) throw new TypeError("byte-scan input must be bytes");
  if (input.length > BYTE_SCAN_MAX_BYTES) throw new RangeError("byte-scan input exceeds 64 MiB");
  if (!Number.isInteger(delimiter) || delimiter < 0 || delimiter > 255) throw new RangeError("delimiter must be a byte");
  let matches = 0n;
  for (const byte of input) if (byte === delimiter) matches += 1n;
  return { bytes: BigInt(input.length), matches };
}

export function canonicalByteScanOutput(bytes, matches) {
  return JSON.stringify({
    bytes: canonicalU64(bytes),
    matches: canonicalU64(matches),
  });
}

export function expectedByteScanOutput(input, delimiter) {
  const result = calculateByteScan(input, delimiter);
  return canonicalByteScanOutput(result.bytes, result.matches);
}

function bytesOf(length, generator) {
  const result = Buffer.alloc(length);
  for (let index = 0; index < length; index += 1) result[index] = generator(index) & 0xff;
  return result;
}

export function deterministicByteScanCases(includeMaximum = false) {
  const cases = [
    { id: "empty", kind: "empty", delimiter: 10, bytes: Buffer.alloc(0) },
    { id: "one-byte", kind: "boundary", delimiter: 10, bytes: Buffer.from([10]) },
    { id: "boundary-15", kind: "boundary", delimiter: 10, bytes: bytesOf(15, (index) => index === 14 ? 10 : index) },
    { id: "boundary-16", kind: "boundary", delimiter: 10, bytes: bytesOf(16, (index) => index === 15 ? 10 : index) },
    { id: "boundary-17", kind: "boundary", delimiter: 10, bytes: bytesOf(17, (index) => index === 16 ? 10 : index) },
    { id: "boundary-64", kind: "boundary", delimiter: 10, bytes: bytesOf(64, (index) => index === 63 ? 10 : index) },
    { id: "boundary-65", kind: "boundary", delimiter: 10, bytes: bytesOf(65, (index) => index === 64 ? 10 : index) },
    { id: "ascii", kind: "ascii", delimiter: 124, bytes: Buffer.from("W|byte|scan|view|", "ascii") },
    { id: "ascii-space", kind: "ascii", delimiter: 32, bytes: Buffer.from("W byte scan view", "ascii") },
    { id: "utf8-mixed", kind: "utf8-mixed", delimiter: 10, bytes: Buffer.from("café\n世界\nW", "utf8") },
    { id: "mixed-binary", kind: "mixed-binary", delimiter: 255, bytes: Buffer.from([0, 255, 1, 128, 255, 10, 0, 255]) },
    { id: "dense", kind: "dense", delimiter: 0, bytes: bytesOf(257, () => 0) },
    { id: "sparse", kind: "sparse", delimiter: 255, bytes: bytesOf(4097, (index) => index % 1024 === 0 ? 255 : 7) },
    { id: "no-matches", kind: "no-matches", delimiter: 255, bytes: bytesOf(1025, (index) => index % 255) },
  ];
  if (includeMaximum) cases.push({
    id: "maximum", kind: "boundary", delimiter: 10,
    bytes: bytesOf(BYTE_SCAN_MAX_BYTES, (index) => index % 251 === 0 ? 10 : 17),
  });
  return cases;
}

export function loadByteScanDocuments() {
  const read = (relativePath) => JSON.parse(fs.readFileSync(path.join(ROOT, relativePath), "utf8"));
  return {
    catalog: read("benchmarks/language-catalog.json"),
    manifest: read("benchmarks/byte-scan-view.manifest.json"),
  };
}
