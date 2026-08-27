import fs from "node:fs";
import path from "node:path";
import {
  BENCHMARK_DISPOSITIONS,
  DESCRIPTOR_IDENTITIES,
  LANGUAGE_PROFILES,
  LIFECYCLE_PHASES,
  REQUIRED_CASES,
  SCHEMA_VERSION,
  SOURCE_FIXTURE,
  loadBmdDocuments,
  reduceCorpus,
  validateCorpus,
  validateManifest,
  validateProgram,
} from "./benchmark-driven-development-machine.mjs";

const root = path.resolve(import.meta.dir, "..");
const documents = loadBmdDocuments();
const errors = [];

function fail(message) {
  errors.push(`BMD0: ${message}`);
}

function hasAll(values, expected) {
  return expected.every((value) => values.includes(value));
}

function checkErrors(label, values) {
  for (const value of values) fail(`${label}: ${value}`);
}

checkErrors("program", validateProgram(documents.program, documents.corpus));
checkErrors("manifest", validateManifest(documents.manifest));
checkErrors("corpus", validateCorpus(documents.corpus));

const resultDefinition = documents.schema?.$defs?.result;
if (!resultDefinition) fail("WBench/1 must define a result kind even when no result fixture exists.");
else {
  const resultRequired = resultDefinition.required ?? [];
  const resultFields = [
    "$schema",
    "schema",
    "kind",
    "id",
    "status",
    "workload",
    "oracle",
    "samples",
    "environment",
    "provenance",
    "metrics",
    "summary",
    "semanticDeviations",
    "disclosures",
  ];
  if (!hasAll(resultRequired, resultFields)) fail("result schema must require the complete result record.");
  if (resultDefinition.properties?.kind?.const !== "result") fail("result schema kind must be result.");
  const oracleRequired = resultDefinition.properties?.oracle?.required ?? [];
  if (!hasAll(oracleRequired, ["validationDigest", "complete", "beforeSamples"])) fail("result oracle must bind validation digest before samples.");
  const sampleRequired = resultDefinition.properties?.samples?.required ?? [];
  if (!hasAll(sampleRequired, ["raw", "warmup", "stopRule", "order"])) fail("result samples must include raw, warmup, stop rule and order.");
  const environmentRequired = resultDefinition.properties?.environment?.required ?? [];
  if (!hasAll(environmentRequired, ["hardware", "kernel", "toolchain", "flags", "target", "provider"])) {
    fail("result environment must contain reproducibility facts without provenance fields.");
  }
  const provenanceRequired = resultDefinition.properties?.provenance?.required ?? [];
  if (!hasAll(provenanceRequired, ["sourceDigest", "artifactDigest", "inputDigest", "recipeDigest", "runnerDigest", "toolchainDigest"])) fail("result provenance must bind source, artifact, input, recipe, runner and toolchain digests separately from environment.");
  const workloadRequired = resultDefinition.properties?.workload?.required ?? [];
  if (!hasAll(workloadRequired, ["manifestDigest", "track", "lane", "phase", "baseline", "variant"])) fail("result workload must identify manifest, track, lane, phase, baseline and variant.");
  const workloadProfile = resultDefinition.properties?.workload?.properties?.profile;
  if (JSON.stringify(workloadProfile?.enum ?? []) !== JSON.stringify(["learner", "idiomatic", "frontier", null])) fail("result workload profile must allow language profiles or null for compiler lifecycle.");
  const workloadRules = resultDefinition.properties?.workload?.allOf ?? [];
  const languageRule = workloadRules.find((entry) => entry.if?.properties?.track?.const === "language");
  const lifecycleRule = workloadRules.find((entry) => entry.if?.properties?.track?.const === "compiler-lifecycle");
  if (!languageRule?.then?.required?.includes("profile")) fail("language result workloads must identify their profile.");
  if (lifecycleRule?.then?.properties?.profile?.const !== null) fail("compiler-lifecycle result workloads must omit or set profile to null.");
  if (resultDefinition.properties?.summary?.properties?.derivedFromRawSamples?.const !== true) fail("result summary must declare derivation from raw samples.");
  if (!documents.schema.oneOf?.some((entry) => entry.$ref === "#/$defs/result")) fail("WBench/1 root must expose kind result.");
}

if (documents.schema?.$id !== SCHEMA_VERSION) fail("schema id must be wbench/1.");
if (documents.manifest.identity?.source?.path !== SOURCE_FIXTURE.path || documents.manifest.identity?.source?.digest !== SOURCE_FIXTURE.digest) {
  fail("seed manifest source must be the current checker_bootstrap fixture.");
}
for (const [axis, identity] of Object.entries(DESCRIPTOR_IDENTITIES)) {
  const actual = documents.manifest.identity?.[axis];
  if (JSON.stringify(actual) !== JSON.stringify(identity)) fail(`seed manifest ${axis} descriptor identity is not source-backed.`);
}

const cases = documents.corpus.cases ?? [];
const reductions = reduceCorpus(documents.corpus);
const accepted = cases.filter((item) => item.kind === "accepted");
const rejected = cases.filter((item) => item.kind === "rejected");
if (cases.length !== 21 || accepted.length !== 5 || rejected.length !== 16) fail(`expected 21 corpus cases (5 accepted, 16 rejected), found ${cases.length} (${accepted.length}, ${rejected.length}).`);
if (!hasAll(cases.map((item) => item.id), REQUIRED_CASES)) fail("corpus is missing a required adversarial or disposition case.");
for (const reduction of reductions.filter((item) => item.id.startsWith("BMD0-W-1487-"))) {
  const source = cases.find((item) => item.id === reduction.id);
  if (source?.kind === "accepted" && reduction.classification !== "accepted") fail(`${reduction.id} must be accepted by the host reducer.`);
  if (source?.kind === "rejected" && reduction.classification !== "rejected") fail(`${reduction.id} must be rejected by the host reducer.`);
}

const acceptedByDisposition = new Map(accepted.map((item) => [item.input?.benchmarkDisposition, item]));
for (const disposition of BENCHMARK_DISPOSITIONS) if (!acceptedByDisposition.has(disposition)) fail(`accepted corpus must cover ${disposition}.`);
if (acceptedByDisposition.get("required")?.track !== "language") fail("required disposition belongs to language workloads.");
if (acceptedByDisposition.get("compiler-lifecycle")?.track !== "compiler-lifecycle") fail("compiler-lifecycle disposition belongs to its own track.");
if (acceptedByDisposition.get("deferred")?.track !== "language") fail("deferred disposition must preserve language profiles.");
if (acceptedByDisposition.get("not-applicable")?.track !== "documentation") fail("not-applicable disposition must be documentation/digest-only.");

const workflow = fs.readFileSync(path.join(root, ".codex", "W-WORKFLOW.md"), "utf8");
for (const phrase of ["benchmarkDisposition", "required", "compiler-lifecycle", "deferred", "not-applicable", "digest-only", "correctness/oracle"]) {
  if (!workflow.toLowerCase().includes(phrase.toLowerCase())) fail(`workflow must state ${phrase}.`);
}
const design = fs.readFileSync(path.join(root, "DESIGN.md"), "utf8");
const rationale = fs.readFileSync(path.join(root, "RATIONALE.md"), "utf8");
const cheatsheet = fs.readFileSync(path.join(root, "CHEATSHEET.md"), "utf8");
const readme = fs.readFileSync(path.join(root, "benchmarks", "README.md"), "utf8");
if (!design.includes("W-1487") || !design.includes("WBench/1") || !design.includes("learner→idiomatic")) fail("DESIGN §18.8 must document current BMD0 policy.");
if (!rationale.includes("W-1487") || !rationale.includes("oracle-backed-current")) fail("RATIONALE §1.37 and ledger must document W-1487.");
if (!cheatsheet.includes("W-1487") || !cheatsheet.includes("WBench/1")) fail("CHEATSHEET performance section must document W-1487.");
if (!readme.includes("checker_bootstrap.w") || !readme.includes("seed-check-graph.json") || !readme.includes("seed-check-input.json")) fail("benchmarks README must identify the source-backed seed and descriptors.");

const resultFixtures = fs.readdirSync(path.join(root, "benchmarks"), { withFileTypes: true })
  .filter((entry) => entry.isFile() && /(?:result|timing)/iu.test(entry.name) && entry.name !== "wbench-1.schema.json");
if (resultFixtures.length > 0) fail(`BMD0 must not create result/timing fixtures: ${resultFixtures.map((entry) => entry.name).join(", ")}.`);
if (fs.existsSync(path.join(root, "benchmarks", "results"))) fail("BMD0 must not create a results directory.");

if (errors.length > 0) {
  for (const error of errors) console.error(error);
  process.exitCode = 1;
} else {
  console.log(`BMD0 benchmark protocol: ${LANGUAGE_PROFILES.length} language profiles, 2 lanes, ${LIFECYCLE_PHASES.length} lifecycle phases, ${documents.program.tasks.length} tasks, ${cases.length} cases (5 accepted, 16 rejected); no runtime results.`);
}
