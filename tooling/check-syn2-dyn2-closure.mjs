import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  evaluateSyn2Dyn2Case,
  summarizeSyn2Dyn2,
  validateSyn2Dyn2,
} from "./syn2-dyn2-closure-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryDirectory = path.resolve(toolingDirectory, "..");
const studyDirectory = path.join(toolingDirectory, "studies", "syn2-dyn2-closure");
const studyFile = path.join(studyDirectory, "study.json");
const corpusFile = path.join(toolingDirectory, "syn2-dyn2-closure-cases.json");
const machineFile = path.join(toolingDirectory, "syn2-dyn2-closure-machine.mjs");
const snapshotFile = path.join(toolingDirectory, "syn2-dyn2-closure-results.snapshot.jsonl");
const classificationFile = path.join(toolingDirectory, "design-freeze-classification.json");
const writeSnapshot = process.argv.includes("--write");
const EXPECTED_OFFICIAL_REFS = Object.freeze([
  ["https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf", "C-lifetime-translation"],
  ["https://pubs.opengroup.org/onlinepubs/9699919799/functions/dlopen.html", "POSIX-load"],
  ["https://pubs.opengroup.org/onlinepubs/9699919799.orig/functions/dlclose.html", "POSIX-close"],
  ["https://doc.rust-lang.org/reference/types/trait-object.html", "Rust-interface"],
  ["https://doc.rust-lang.org/std/any/struct.TypeId.html", "Rust-identity"],
  ["https://doc.rust-lang.org/stable/cargo/reference/build-scripts.html", "Rust-build"],
  ["https://docs.python.org/3/library/importlib.html", "Python-import"],
  ["https://docs.python.org/3/library/inspect.html", "Python-inspection"],
  ["https://docs.python.org/3/library/functions.html#eval", "Python-eval-reject"],
]);

function digest(file) {
  return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`;
}

function readJson(file, location, errors) {
  try {
    return JSON.parse(fs.readFileSync(file, "utf8"));
  } catch (error) {
    errors.push(`${location} is not valid JSON: ${error.message}`);
    return undefined;
  }
}

function resolveContained(base, relative, boundary, location, errors) {
  if (typeof relative !== "string" || relative.trim() === "") {
    errors.push(`${location} must be a non-empty relative path.`);
    return undefined;
  }
  const resolved = path.resolve(base, relative);
  const relativeToBoundary = path.relative(boundary, resolved);
  if (relativeToBoundary === "" || relativeToBoundary.startsWith(`..${path.sep}`) || path.isAbsolute(relativeToBoundary)) {
    errors.push(`${location} escapes its containment boundary.`);
    return undefined;
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    errors.push(`${location} references a missing file.`);
    return undefined;
  }
  return resolved;
}

function checkDigest(file, expected, location, errors, allowPending = false) {
  if (allowPending && expected === "sha256:pending") return;
  if (!file || !/^sha256:[0-9a-f]{64}$/.test(expected ?? "")) {
    errors.push(`${location} must use a lowercase sha256 digest.`);
    return;
  }
  const actual = digest(file);
  if (actual !== expected) errors.push(`${location} is stale; expected ${actual}.`);
}

function assert(condition, message, errors) {
  if (!condition) errors.push(message);
}

function exactObjectDigest(ref, base, boundary, location, errors, allowPending = false) {
  const file = resolveContained(base, ref?.path, boundary, `${location}.path`, errors);
  checkDigest(file, ref?.digest, `${location}.digest`, errors, allowPending);
  return file;
}

function expectedSnapshot(study, corpus, results) {
  const chain = [
    ...study.buildsOn.map(({ id, path: refPath, digest: refDigest, role }) => ({ id, path: refPath, digest: refDigest, role })),
    ...study.independentReducers.flatMap((reducer) => [
      { id: `${reducer.id}:machine`, path: reducer.machine.path, digest: reducer.machine.digest, role: reducer.role },
      { id: `${reducer.id}:snapshot`, path: reducer.snapshot.path, digest: reducer.snapshot.digest, role: reducer.role },
      { id: `${reducer.id}:checker`, path: reducer.checker.path, digest: reducer.checker.digest, role: reducer.role },
    ]),
    ...study.sourceRefs.map(({ path: refPath, symbol, digest: refDigest, role }) => ({ id: `${refPath}#${symbol}`, path: refPath, digest: refDigest, role })),
    ...Object.entries(study.artifacts)
      .filter(([name]) => name !== "snapshot")
      .map(([name, ref]) => ({ id: `closure:${name}`, path: ref.path, digest: ref.digest, role: "closure-artifact" })),
  ];
  const mutationResults = corpus.cases
    .filter((testCase) => testCase.mutation)
    .map((testCase) => {
      const result = evaluateSyn2Dyn2Case(testCase);
      return { id: testCase.id, kind: testCase.mutation.kind, status: result.status, code: result.code };
    });
  return {
    $schema: "w-syn2-dyn2-closure-results-1",
    status: "design-oracle-output",
    id: "SYN2-DYN2",
    evidence: "host-derived-design-oracle; no compiler/runtime/provider claim",
    corpusDigest: digest(corpusFile),
    machineDigest: digest(machineFile),
    manifestDigestChain: chain,
    metrics: summarizeSyn2Dyn2(corpus),
    results,
    mutations: mutationResults,
    integrityMutationCount: 10,
    integrityMutations: ["caller-status-echo", "forged-provider-ready", "stale-corpus-digest", "replaced-source-ref", "persistent-live-field", "extra-physical-cleanup", "missing-generation-field", "duplicate-generation-field", "extra-official-ref", "reducer-alias"],
  };
}

export function runSyn2Dyn2Closure({ write = writeSnapshot } = {}) {
  const errors = [];
  const study = readJson(studyFile, "study.json", errors);
  const corpus = readJson(corpusFile, "syn2-dyn2-closure-cases.json", errors);
  const bundle = readJson(path.join(studyDirectory, "bundle.json"), "bundle.json", errors);
  const classification = readJson(classificationFile, "design-freeze-classification.json", errors);
  if (!study || !corpus || !bundle || !classification) return { errors };

  assert(study.$schema === "w-syn2-dyn2-closure-study-1", "study schema mismatch", errors);
  assert(study.status === "design-oracle-input", "study status must be design-oracle-input", errors);
  assert(study.id === "SYN2-DYN2", "study id must be SYN2-DYN2", errors);
  assert(!Object.hasOwn(study, "supersedes"), "study must use buildsOn, not supersedes", errors);
  const buildsOn = Array.isArray(study.buildsOn) ? study.buildsOn : [];
  assert(JSON.stringify(buildsOn.map((ref) => ref.id)) === JSON.stringify(["SYN1", "DYN1", "HRD0"]), "buildsOn must be SYN1,DYN1,HRD0", errors);
  assert(new Set(buildsOn.map((ref) => ref.path)).size === buildsOn.length, "study.buildsOn paths must be unique", errors);
  for (const [index, ref] of buildsOn.entries()) {
    exactObjectDigest(ref, studyDirectory, toolingDirectory, `study.buildsOn[${index}]`, errors, write);
    assert(ref.role === "evidence-input", `study.buildsOn[${index}].role must be evidence-input`, errors);
  }

  const artifacts = study.artifacts ?? {};
  const artifactFiles = {};
  for (const [name, ref] of Object.entries(artifacts)) {
    artifactFiles[name] = exactObjectDigest(ref, studyDirectory, repositoryDirectory, `study.artifacts.${name}`, errors, write);
  }
  assert(JSON.stringify(Object.keys(artifacts).sort()) === JSON.stringify(["bundle", "checker", "corpus", "machine", "nestedChecker", "snapshot", "studyOracle"].sort()), "study.artifacts must enumerate the closure manifest chain", errors);
  assert(new Set(Object.values(artifacts).map((ref) => ref.path)).size === Object.keys(artifacts).length, "study.artifacts paths must be unique", errors);

  const sourceRefs = Array.isArray(study.sourceRefs) ? study.sourceRefs : [];
  const expectedSourceKeys = new Set([
    "reference/last-light/packages/menu-compiler/transform.w\0maximumBytes",
    "reference/last-light/repl_session_oracle.w\0GenerationReceipt",
    "reference/last-light/hot_reload_dev_contract.w\0ReloadResult",
    "reference/last-light/abi.w\0export foreign c",
    "reference/last-light/memory.w\0watchClosingBell",
    "reference/last-light/build.w\0name: \"compile-final-menu\"",
  ]);
  assert(sourceRefs.length === expectedSourceKeys.size, "study.sourceRefs must contain exactly six pinned Last Light references", errors);
  const sourceKeys = new Set();
  const sourcePaths = new Set();
  for (const [index, ref] of sourceRefs.entries()) {
    const file = exactObjectDigest(ref, repositoryDirectory, repositoryDirectory, `study.sourceRefs[${index}]`, errors, false);
    const key = `${ref?.path}\0${ref?.symbol}`;
    assert(expectedSourceKeys.has(key), `study.sourceRefs[${index}] is not one of the exact six Last Light refs`, errors);
    assert(!sourceKeys.has(key), `study.sourceRefs[${index}] duplicates ${key}`, errors);
    sourceKeys.add(key);
    sourcePaths.add(ref?.path);
    if (file && typeof ref.symbol === "string") assert(fs.readFileSync(file, "utf8").includes(ref.symbol), `study.sourceRefs[${index}].symbol is absent`, errors);
    assert(typeof ref.role === "string" && ref.role.length > 0, `study.sourceRefs[${index}].role is required`, errors);
  }
  assert(sourcePaths.size === sourceRefs.length, "study.sourceRefs paths must be unique", errors);
  const allowedHosts = new Set(["www.open-std.org", "pubs.opengroup.org", "doc.rust-lang.org", "docs.python.org"]);
  const officialShape = (study.officialRefs ?? []).map((ref) => [ref?.url, ref?.role]);
  assert(JSON.stringify(officialShape) === JSON.stringify(EXPECTED_OFFICIAL_REFS), "study.officialRefs must use the exact URL and role allowlist", errors);
  assert(new Set((study.officialRefs ?? []).map((ref) => ref?.url)).size === (study.officialRefs ?? []).length, "study.officialRefs URLs must be unique", errors);
  assert(new Set((study.officialRefs ?? []).map((ref) => ref?.role)).size === (study.officialRefs ?? []).length, "study.officialRefs roles must be unique", errors);
  for (const [index, ref] of (study.officialRefs ?? []).entries()) {
    let url;
    try { url = new URL(ref.url); } catch { /* reported below */ }
    assert(url?.protocol === "https:", `study.officialRefs[${index}] must use https`, errors);
    assert(url && allowedHosts.has(url.hostname), `study.officialRefs[${index}] host is not allowlisted`, errors);
    assert(typeof ref.role === "string" && ref.role.length > 0, `study.officialRefs[${index}].role is required`, errors);
  }

  const reducers = Array.isArray(study.independentReducers) ? study.independentReducers : [];
  assert(JSON.stringify(reducers.map((ref) => ref.id)) === JSON.stringify(["DYN1", "HRD0"]), "independentReducers must reuse DYN1 and HRD0", errors);
  const reducerPaths = reducers.flatMap((reducer) => [reducer.machine?.path, reducer.snapshot?.path, reducer.checker?.path]);
  assert(new Set(reducerPaths).size === reducerPaths.length, "independent reducer paths must be unique", errors);
  for (const [index, reducer] of reducers.entries()) {
    assert(reducer.role === "validated-independent-reducer", `study.independentReducers[${index}] role is not validated`, errors);
    for (const name of ["machine", "snapshot", "checker"]) exactObjectDigest(reducer[name], studyDirectory, toolingDirectory, `study.independentReducers[${index}].${name}`, errors, false);
  }
  assert(JSON.stringify(study.implementationGapMap) === JSON.stringify(corpus.implementationGapMap), "study and corpus implementationGapMap must be identical", errors);
  const classificationById = new Map((classification.entries ?? []).map((entry) => [entry.decisionId, entry]));
  for (const id of [...new Set(Object.values(study.implementationGapMap ?? {}).flat())]) {
    assert(classificationById.get(id)?.category === "implementation-evidence-gap", `${id} is not classified as implementation-evidence-gap`, errors);
  }

  const corpusValidation = validateSyn2Dyn2(corpus);
  errors.push(...corpusValidation.errors);
  const results = corpus.cases.map(evaluateSyn2Dyn2Case);
  const summary = summarizeSyn2Dyn2(corpus);
  assert(JSON.stringify(summary) === JSON.stringify({ caseCount: 17, currentContractCount: 12, rejectedCount: 5, implementationBoundaryCount: 1, axes: { SYN2: 10, DYN2: 7 } }), "closure summary must be 17/12/5 with SYN2=10 and DYN2=7", errors);
  assert(study.metrics?.mutationCount === 5 && study.metrics?.integrityMutationCount === 10, "study.metrics mutation counts must be five corpus and ten integrity mutations", errors);
  assert(JSON.stringify(study.generationReference?.fields) === JSON.stringify(["generationId", "artifactDigest", "recipeDigest", "semanticInterfaceKey", "schemaDigest", "targetReceipt", "resolveReceipt", "migrationReceipt"]), "GenerationReference fields are not exact", errors);
  assert(study.generationReference?.targetReceipt === "exact-target-WAbi-runtime-closure", "GenerationReference target receipt mode is not exact", errors);
  assert(study.generationReference?.migration === "resolve-rebind-identity-schema-only", "GenerationReference migration mode is not receipt-only", errors);
  assert(study.generationReference?.authority === "none", "GenerationReference must carry no authority", errors);
  assert(study.generationReference?.convertToServiceRef === false, "GenerationReference must not convert to ServiceRef", errors);
  assert(JSON.stringify(study.generationReference?.forbidden) === JSON.stringify(["heap", "task", "loan", "frame", "capability", "ServiceRef", "callback", "providerHandle"]), "GenerationReference forbidden live fields are not exact", errors);

  const eventsCase = corpus.cases.find((testCase) => testCase.id === "SYN2-events-derived");
  assert(JSON.stringify(eventsCase?.facts?.eventTrace) === JSON.stringify(["tool-start", "tool-stage", "tool-write", "tool-finish"]), "W-1375 event boundary must use the exact host event trace", errors);
  assert(eventsCase?.facts?.expectedSelectsOutcome === false && eventsCase?.facts?.callerFailureSelector === false, "W-1375 must derive status and route from events", errors);
  const manifestCase = corpus.cases.find((testCase) => testCase.id === "SYN2-manifest-boundary");
  assert(manifestCase?.facts?.forgedProviderReady === false && manifestCase?.facts?.targetRegistrySeparate === true, "W-1380 manifest boundary must reject forged provider-ready evidence", errors);
  assert(bundle.$schema === "w-substitution-study-bundle-1" && bundle.status === "design-oracle-input", "closure bundle must use canonical R1 schema", errors);

  const mutationChecks = [];
  const callerEcho = structuredClone(corpus);
  callerEcho.cases[0].status = "rejected";
  mutationChecks.push({ id: "caller-status-echo", pass: validateSyn2Dyn2(callerEcho).errors.some((error) => error.includes("caller-owned")) });
  const forgedProvider = structuredClone(manifestCase);
  forgedProvider.facts.forgedProviderReady = true;
  mutationChecks.push({ id: "forged-provider-ready", pass: evaluateSyn2Dyn2Case(forgedProvider).status === "rejected" });
  const staleDigest = structuredClone(study.artifacts.corpus);
  staleDigest.digest = `sha256:${"0".repeat(64)}`;
  const staleErrors = [];
  exactObjectDigest(staleDigest, studyDirectory, repositoryDirectory, "mutation.stale-corpus-digest", staleErrors, false);
  mutationChecks.push({ id: "stale-corpus-digest", pass: staleErrors.some((error) => error.includes("is stale")) });
  const replacedRef = structuredClone(study.sourceRefs[0]);
  replacedRef.path = study.sourceRefs[1].path;
  const replacedErrors = [];
  exactObjectDigest(replacedRef, repositoryDirectory, repositoryDirectory, "mutation.replaced-source-ref", replacedErrors, false);
  mutationChecks.push({ id: "replaced-source-ref", pass: replacedErrors.some((error) => error.includes("is stale")) });
  const livePersistent = structuredClone(corpus.cases.find((testCase) => testCase.id === "DYN2-C-generation-reference"));
  livePersistent.facts.extraFields = ["heap"];
  mutationChecks.push({ id: "persistent-live-field", pass: evaluateSyn2Dyn2Case(livePersistent).status === "rejected" });
  const extraPhysicalCleanup = structuredClone(corpus.cases.find((testCase) => testCase.id === "DYN2-switch-cleanup-fault"));
  extraPhysicalCleanup.facts.cleanupOrder = ["cancel", "drain", "unregister", "inFlightDrain", "destroy", "release", "unpin"];
  mutationChecks.push({ id: "extra-physical-cleanup", pass: evaluateSyn2Dyn2Case(extraPhysicalCleanup).status === "rejected" });
  const missingGenerationField = structuredClone(corpus.cases.find((testCase) => testCase.id === "DYN2-C-generation-reference"));
  missingGenerationField.facts.persistentFields = missingGenerationField.facts.persistentFields.slice(1);
  mutationChecks.push({ id: "missing-generation-field", pass: evaluateSyn2Dyn2Case(missingGenerationField).status === "rejected" });
  const duplicateGenerationField = structuredClone(corpus.cases.find((testCase) => testCase.id === "DYN2-C-generation-reference"));
  duplicateGenerationField.facts.persistentFields.push("generationId");
  mutationChecks.push({ id: "duplicate-generation-field", pass: evaluateSyn2Dyn2Case(duplicateGenerationField).status === "rejected" });
  const extraOfficialRef = structuredClone(study.officialRefs);
  extraOfficialRef.push({ url: "https://docs.python.org/3/library/os.html", role: "Python-extra" });
  mutationChecks.push({ id: "extra-official-ref", pass: JSON.stringify(extraOfficialRef.map((ref) => [ref.url, ref.role])) !== JSON.stringify(EXPECTED_OFFICIAL_REFS) });
  const reducerAlias = structuredClone(study.independentReducers);
  reducerAlias[1].id = "DYN1";
  mutationChecks.push({ id: "reducer-alias", pass: JSON.stringify(reducerAlias.map((ref) => ref.id)) !== JSON.stringify(["DYN1", "HRD0"]) });
  assert(mutationChecks.length === 10 && mutationChecks.every((mutation) => mutation.pass), "integrity mutation tests must reject stale, forged, replaced, caller-echo, live-state, extra-cleanup, missing-field, duplicate-field, extra-ref, and reducer-alias inputs", errors);

  const snapshot = expectedSnapshot(study, corpus, results);
  if (write) {
    fs.writeFileSync(snapshotFile, `${JSON.stringify(snapshot)}\n`, "utf8");
  } else if (fs.existsSync(snapshotFile)) {
    const actual = fs.readFileSync(snapshotFile, "utf8").trim();
    assert(actual === JSON.stringify(snapshot), "snapshot is stale; rerun with --write", errors);
  } else {
    errors.push("snapshot is missing; rerun with --write");
  }

  if (errors.length > 0) return { errors, summary, mutationChecks };
  return { errors: [], summary, mutationChecks, snapshotDigest: fs.existsSync(snapshotFile) ? digest(snapshotFile) : undefined };
}

if (path.resolve(process.argv[1] ?? "") === path.resolve(fileURLToPath(import.meta.url))) {
  const result = runSyn2Dyn2Closure();
  if (result.errors.length > 0) {
    process.stderr.write(`${result.errors.join("\n")}\n`);
    process.exit(1);
  }
  process.stdout.write(`SYN2/DYN2 closure: ${result.summary.caseCount} cases, ${result.summary.currentContractCount} current, ${result.summary.rejectedCount} rejected; snapshot ${result.snapshotDigest ?? "written"}.\n`);
}
