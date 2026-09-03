import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { ledgerIdSet as designDecisionIds } from "./design-ledger.mjs";
import {
  LOWERINGS,
  compareGen1Lowerings,
  extractSourceSlice,
  measureBundleVariants,
  measureSourceSlice,
  measureSourceText,
  runGen1Program,
  validateVariantDisposition,
  validateGen1Operation,
} from "./gen1-incremental-suspension-machine.mjs";

const toolingDirectory = path.dirname(fileURLToPath(import.meta.url));
const wDirectory = path.resolve(toolingDirectory, "..");
const corpusPath = path.join(toolingDirectory, "gen1-incremental-suspension-cases.json");
const snapshotPath = path.join(toolingDirectory, "gen1-incremental-suspension-results.snapshot.jsonl");
const bundlePath = path.join(toolingDirectory, "studies", "gen1-incremental-suspension", "bundle.json");
const corpus = JSON.parse(fs.readFileSync(corpusPath, "utf8"));
const bundle = JSON.parse(fs.readFileSync(bundlePath, "utf8"));
const errors = [];
const results = [];
const caseIds = new Set();

function digest(file) { return `sha256:${crypto.createHash("sha256").update(fs.readFileSync(file)).digest("hex")}`; }
function nonEmpty(value) { return typeof value === "string" && value.trim() !== ""; }
function localFile(relative, location) {
  if (!nonEmpty(relative)) { errors.push(`${location} must be a repository path.`); return undefined; }
  const resolved = path.resolve(wDirectory, relative); const relativeToRoot = path.relative(wDirectory, resolved);
  if (!relativeToRoot || relativeToRoot.startsWith(`..${path.sep}`) || path.isAbsolute(relativeToRoot) || !fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) { errors.push(`${location} references a missing or out-of-tree file.`); return undefined; }
  return resolved;
}
function checkReference(reference, location) {
  const file = localFile(reference?.path, `${location}.path`); if (!file) return;
  if (!nonEmpty(reference.symbol) || !fs.readFileSync(file, "utf8").includes(reference.symbol)) errors.push(`${location}.symbol is absent.`);
  if (!nonEmpty(reference.claim)) errors.push(`${location}.claim must be non-empty.`);
  if (digest(file) !== reference.digest) errors.push(`${location}.digest is stale; expected ${digest(file)}.`);
}
function checkOfficialSources() {
  const hosts = new Set(["www.open-std.org", "doc.rust-lang.org", "docs.python.org", "peps.python.org", "pubs.opengroup.org", "www.llvm.org"]); const ids = new Set();
  if (!Array.isArray(corpus.officialSources) || corpus.officialSources.length < 8) errors.push("officialSources must contain primary C/POSIX/LLVM/Rust/Python sources.");
  for (const [index, source] of (corpus.officialSources ?? []).entries()) {
    let parsed; try { parsed = new URL(source.url); } catch { parsed = null; }
    if (!nonEmpty(source.id) || ids.has(source.id)) errors.push(`officialSources[${index}].id must be unique.`); ids.add(source.id);
    if (!parsed || parsed.protocol !== "https:" || !hosts.has(parsed.hostname)) errors.push(`officialSources[${index}] must use an official primary host.`);
    if (!nonEmpty(source.claim)) errors.push(`officialSources[${index}].claim must be non-empty.`);
  }
}
function checkBundle() {
  if (bundle.$schema !== "w-substitution-study-bundle-1" || bundle.id !== "R1-gen1-incremental-suspension") errors.push("bundle identity is stale.");
  if (bundle.researchState !== "historical" || bundle.supersededBy !== "W-1437" || JSON.stringify(bundle.implementationEvidenceGaps ?? []) !== JSON.stringify(["W-1438", "W-1440"])) errors.push("GEN1 bundle must identify historical state, W-1437 successor, and W-1438/W-1440 implementation evidence gaps.");
  const ids = new Set();
  for (const [index, variant] of (bundle.variants ?? []).entries()) {
    if (!nonEmpty(variant.id) || ids.has(variant.id)) errors.push(`bundle.variants[${index}].id must be unique.`); ids.add(variant.id);
    const file = localFile(path.join("tooling", "studies", "gen1-incremental-suspension", variant.path ?? ""), `bundle.variants[${index}]`); if (!file) continue;
    const source = fs.readFileSync(file, "utf8");
    if (digest(file) !== variant.digest) errors.push(`bundle.variants[${index}].digest is stale; expected ${digest(file)}.`);
    if (!source.includes(bundle.entry)) errors.push(`bundle.variants[${index}] lacks ${bundle.entry}.`);
    if (variant.language === "w-reserved" && variant.parseEvidence?.status !== "reserved-not-parsed") errors.push("reserved witness must stay unparsed.");
    if (variant.language !== "w-reserved" && variant.parseEvidence?.status !== "tree-sitter-parse") errors.push(`${variant.id} lacks parse evidence.`);
    if (variant.id === "compiler-stream-block" && (!["some Stream<Item, Failure>", "capture", "capacity", "prefetch", "Result<Item, Failure>", "cancellation", "cleanup", "view does not escape"].every((term) => source.includes(term)) || /\bprotocol\s+Resumable\b|\bpublic\s+frame\s+identity\s*[:=]|\byield\s+frame\.resume\b/u.test(source.replaceAll(/\s+/gu, " ")))) errors.push("compiler-stream-block witness must keep its source contract and no public frame identity.");
    if (variant.id === "public-resumable-frame" && (!["protocol Resumable", "public frame identity", "resume token", "scheduler", "runtime metadata", "ABI"].every((term) => source.includes(term)))) errors.push("public-resumable-frame witness must expose the rejected public mechanism facts.");
  }
  if (ids.size !== 5 || !ids.has("stream-structured") || !ids.has("nominal-state-machine") || !ids.has("dual-bounded-channels") || !ids.has("compiler-stream-block") || !ids.has("public-resumable-frame")) errors.push("GEN1 must contain A/B/C, a historical rejected compiler-owned witness, and a public-frame rejected witness.");
  for (const variant of bundle.variants ?? []) if (!validateVariantDisposition(variant)) errors.push(`bundle variant ${variant.id} has an invalid role/disposition manifest.`);
  const compilerWitness = bundle.variants?.find((variant) => variant.id === "compiler-stream-block");
  const publicWitness = bundle.variants?.find((variant) => variant.id === "public-resumable-frame");
  if (compilerWitness?.hiddenStatePolicy !== "compiler-owned") errors.push("compiler-stream-block must mark suspension storage compiler-owned.");
  if (publicWitness?.hiddenStatePolicy === "compiler-owned") errors.push("public-resumable-frame cannot use compiler-owned exposure policy.");
  const helperIds = new Set();
  for (const [index, helper] of (bundle.helpers ?? []).entries()) {
    if (!nonEmpty(helper.id) || helperIds.has(helper.id)) errors.push(`bundle.helpers[${index}].id must be unique.`); helperIds.add(helper.id);
    const file = localFile(path.join("tooling", "studies", "gen1-incremental-suspension", helper.path ?? ""), `bundle.helpers[${index}]`); if (!file) continue;
    if (digest(file) !== helper.digest) errors.push(`bundle.helpers[${index}].digest is stale; expected ${digest(file)}.`);
    if (!nonEmpty(helper.symbol) || !fs.readFileSync(file, "utf8").includes(helper.symbol)) errors.push(`bundle.helpers[${index}].symbol is absent.`);
    if (!nonEmpty(helper.claim)) errors.push(`bundle.helpers[${index}].claim must be non-empty.`);
    const helperText = fs.readFileSync(file, "utf8"); if (!helperText.includes("Channel<") || !helperText.includes("capacity") || !helperText.includes("take")) errors.push("bounded-dialogue-builder must construct owned bounded channels.");
  }
  const oracle = localFile(path.join("tooling", "studies", "gen1-incremental-suspension", bundle.oracle?.path ?? ""), "bundle.oracle"); if (oracle && digest(oracle) !== bundle.oracle.digest) errors.push(`bundle.oracle.digest is stale; expected ${digest(oracle)}.`);
  for (const [index, source] of (bundle.sourceRefs ?? []).entries()) {
    const file = localFile(path.join("reference", "last-light", path.basename(source.path ?? "")), `bundle.sourceRefs[${index}]`);
    if (file && digest(file) !== source.digest) errors.push(`bundle.sourceRefs[${index}].digest is stale; expected ${digest(file)}.`);
    if (file && (!nonEmpty(source.symbol) || !fs.readFileSync(file, "utf8").includes(source.symbol))) errors.push(`bundle.sourceRefs[${index}].symbol is absent.`);
  }
}
function deriveConclusions(metricsByVariant, scenarioSliceMetrics) {
  const operationalByScenario = new Map();
  for (const testCase of corpus.cases ?? []) operationalByScenario.set(testCase.scenario, (operationalByScenario.get(testCase.scenario) ?? 0) + (Array.isArray(testCase.operations) && testCase.operations.length > 0 ? 1 : 0));
  const missingOperational = (corpus.requiredScenarioIds ?? []).map((id) => corpus.cases.find((testCase) => testCase.id === id)?.scenario).filter((scenario) => !scenario || !operationalByScenario.has(scenario));
  const invalidSourceSlices = [...scenarioSliceMetrics.values()].filter((slice) => slice.implementations.some((implementation) => !implementation.applicable)).map((slice) => slice.scenario);
  const sourceBackedScenarios = [...scenarioSliceMetrics.keys()].sort();
  const operationalScenarios = [...operationalByScenario.keys()].sort();
  const capabilityGap = { status: missingOperational.length || invalidSourceSlices.length ? "historical-candidate" : "none", uncoveredScenarios: [...new Set([...missingOperational, ...invalidSourceSlices])], sourceBackedScenarios, operationalScenarios, basis: "each source-backed scenario requires a validated unique symbol slice and every required scenario requires an operational trace; A/B/C applicability is recorded per source slice" };
  const ergonomicScenarios = [];
  for (const slice of scenarioSliceMetrics.values()) {
    const sliceMetrics = slice.implementations.filter((implementation) => implementation.applicable).map((implementation) => implementation.metrics);
    if (sliceMetrics.length < 2) continue;
    const structural = ["publicDeclarationCount", "explicitOwnerHandoffs", "explicitEffectPoints", "explicitCleanupPoints", "hiddenStateCount", "sourceOperations", "capacityFacts"];
    const differs = structural.some((key) => new Set(sliceMetrics.map((metric) => JSON.stringify(metric[key]))).size > 1);
    if (differs) ergonomicScenarios.push(slice.scenario);
  }
  const publicFrame = metricsByVariant.get("public-resumable-frame");
  const compilerBlock = metricsByVariant.get("compiler-stream-block");
  const builder = metricsByVariant.get("bounded-dialogue-builder");
  return {
    capabilityGap,
    ergonomicGap: { status: ergonomicScenarios.length ? "historical-candidate" : "none", scenarios: ergonomicScenarios, observedStructuralDifference: ergonomicScenarios, humanDecisionPending: true, basis: "same-scenario declaration slices only; structural difference is a historical question, not a current design opening" },
    ergonomicQuestion: { status: "historical-question", scenarios: ergonomicScenarios, observedStructuralDifference: ergonomicScenarios, humanDecisionPending: true },
    dispositions: {
      publicResumableFrameOrScheduler: { status: publicFrame?.hiddenStateCount > 0 ? "intentionally-rejected" : "candidate", witness: "public-resumable-frame", exposure: "public", mechanism: "protocol/frame identity, resume token, scheduler, runtime metadata, or ABI" },
      boundedLibraryProducerBuilder: { status: builder?.capacityFacts?.length >= 2 ? "current-candidate" : "placeholder", scenarios: ["dialogue-resume-value"], doesNotResolve: ["traversal-local-retention", "delegation-equivalent"], helper: "bounded-dialogue-builder" },
      compilerOwnedStreamBlock: { status: compilerBlock?.hiddenStateCount === 0 ? "historical-rejected" : "historical-candidate", witness: "compiler-stream-block", exposure: "compiler-owned", prerequisites: ["explicit capture modes", "some Stream<Item,Failure> source contract", "capacity and prefetch", "item-only Result emission", "compiler/lowering proof", "human/model evidence"], supersededBy: "W-1437", implementationEvidenceGaps: ["W-1438", "W-1440"] },
    },
  };
}

if (corpus.$schema !== "w-gen1-incremental-suspension-1" || corpus.status !== "design-oracle-input-gen1" || corpus.id !== "GEN1") errors.push("corpus identity is stale.");
if (corpus.historicalStatus !== "superseded" || corpus.supersededBy !== "W-1437" || JSON.stringify(corpus.implementationEvidenceGaps ?? []) !== JSON.stringify(["W-1438", "W-1440"])) errors.push("GEN1 corpus must identify W-1437 as successor and W-1438/W-1440 as implementation evidence gaps.");
if (JSON.stringify(corpus.lowerings ?? []) !== JSON.stringify(LOWERINGS)) errors.push("corpus lowerings must preserve both target lowerings.");
if (Object.prototype.hasOwnProperty.call(corpus, "conclusion")) errors.push("corpus must not contain a caller-provided conclusion.");
checkOfficialSources(); checkBundle();
const variantMetrics = measureBundleVariants(bundle); const metricsByVariant = new Map(variantMetrics.map((metric) => [metric.variant, metric]));
const helperMetrics = (bundle.helpers ?? []).map((helper) => measureSourceText(fs.readFileSync(localFile(path.join("tooling", "studies", "gen1-incremental-suspension", helper.path), `bundle.helpers.${helper.id}`), "utf8"), { id: helper.id, language: "w", parseEvidence: { status: "tree-sitter-parse" }, path: helper.path }));
for (const metric of helperMetrics) metricsByVariant.set(metric.variant, metric);
const sourceEntries = new Map();
for (const variant of bundle.variants ?? []) {
  const file = localFile(path.join("tooling", "studies", "gen1-incremental-suspension", variant.path), `bundle.variants.${variant.id}`);
  if (file) sourceEntries.set(variant.id, { file, language: variant.language, parseEvidence: variant.parseEvidence });
}
for (const helper of bundle.helpers ?? []) {
  const file = localFile(path.join("tooling", "studies", "gen1-incremental-suspension", helper.path), `bundle.helpers.${helper.id}`);
  if (file) sourceEntries.set(helper.id, { file, language: "w", parseEvidence: { status: "tree-sitter-parse" } });
}
const scenarioSliceMetrics = new Map();
for (const slice of corpus.scenarioSlices ?? []) {
  if (!nonEmpty(slice.scenario) || !Array.isArray(slice.implementations) || slice.implementations.length === 0) errors.push("corpus scenarioSlices must name source implementations.");
}
const corpusSliceByScenario = new Map((corpus.scenarioSlices ?? []).map((slice) => [slice.scenario, slice]));
for (const slice of bundle.scenarioSlices ?? []) {
  if (!nonEmpty(slice.scenario) || !Array.isArray(slice.implementations) || slice.implementations.length === 0) { errors.push("bundle scenarioSlices must name at least one source implementation."); continue; }
  const corpusSlice = corpusSliceByScenario.get(slice.scenario); if (!corpusSlice) errors.push(`bundle scenario slice ${slice.scenario} is missing from corpus.`);
  const implementations = []; const seen = new Set();
  for (const [index, implementation] of slice.implementations.entries()) {
    const location = `bundle.scenarioSlices.${slice.scenario}.implementations[${index}]`; const key = `${implementation?.variant}#${implementation?.symbol}`;
    if (seen.has(key)) errors.push(`${location} duplicates an implementation.`); seen.add(key);
    const entry = sourceEntries.get(implementation?.variant);
    const corpusImplementation = corpusSlice?.implementations?.find((candidate) => candidate.variant === implementation?.variant && candidate.symbol === implementation?.symbol);
    if (!entry || !nonEmpty(implementation?.symbol) || !nonEmpty(implementation?.applicability) || !corpusImplementation) { errors.push(`${location} needs a real variant, symbol, applicability, and corpus reference.`); continue; }
    const source = fs.readFileSync(entry.file, "utf8"); const extracted = extractSourceSlice(source, implementation.symbol);
    if (extracted.count !== 1 || !extracted.slice) errors.push(`${location} symbol must occur exactly once with a bounded declaration body.`);
    const metrics = measureSourceSlice(source, implementation.symbol, { id: implementation.variant, language: entry.language, parseEvidence: entry.parseEvidence, path: `${implementation.variant}:${implementation.symbol}` });
    if (implementation.digest && metrics.sourceDigest !== implementation.digest) errors.push(`${location}.digest is stale; expected ${metrics.sourceDigest}.`);
    if (!implementation.digest) errors.push(`${location}.digest must record the sliced source digest.`);
    implementations.push({ variant: implementation.variant, symbol: implementation.symbol, applicability: implementation.applicability, applicable: metrics.applicable, metrics });
  }
  scenarioSliceMetrics.set(slice.scenario, { scenario: slice.scenario, implementations });
}
for (const [index, testCase] of (corpus.cases ?? []).entries()) {
  const location = `cases[${index}]`; if (!/^GEN1-[a-z0-9]+(?:-[a-z0-9]+)*$/u.test(testCase.id ?? "") || caseIds.has(testCase.id)) errors.push(`${location}.id is invalid or duplicated.`); caseIds.add(testCase.id);
  if (!nonEmpty(testCase.scenario) || !Array.isArray(testCase.references) || testCase.references.length === 0) errors.push(`${location} needs scenario and Last Light references.`); else testCase.references.forEach((reference, refIndex) => checkReference(reference, `${location}.references[${refIndex}]`));
  if (!Array.isArray(testCase.decisions) || testCase.decisions.some((decision) => !designDecisionIds.has(decision))) errors.push(`${location}.decisions must use existing ledger IDs.`);
  if (!Array.isArray(testCase.operations) || testCase.operations.length === 0 || testCase.operations.some((operation) => !validateGen1Operation(operation))) errors.push(`${location}.operations are malformed.`);
  const comparison = compareGen1Lowerings(testCase.operations ?? []); const expected = testCase.expected ?? {};
  if (!comparison.equivalence.equivalent) errors.push(`${location} lowerings diverge.`);
  if (expected.loweringsEquivalent !== undefined && expected.loweringsEquivalent !== comparison.equivalence.equivalent) errors.push(`${location}.expected.loweringsEquivalent is stale.`);
  for (const actual of [comparison.switched, comparison.returned]) {
    if (actual.status !== expected.status) errors.push(`${location}.expected.status does not match engine.`);
    if (expected.status === "rejected" && (actual.reason !== expected.reason || actual.operation !== expected.operation)) errors.push(`${location}.expected rejection does not match engine.`);
    if (expected.status === "accepted" && expected.result !== undefined && actual.state.typedResult !== expected.result) errors.push(`${location}.expected.result does not match engine.`);
  }
  if (expected.status === "accepted" && !comparison.switched.state.singleOwnerProof) errors.push(`${location} accepted trace lacks token ownership proof.`);
  results.push({ caseId: testCase.id, scenario: testCase.scenario, status: comparison.switched.status, ...(comparison.switched.reason ? { reason: comparison.switched.reason, operation: comparison.switched.operation } : {}), typedResult: comparison.switched.state.typedResult, state: comparison.switched.state, lowerings: comparison.equivalence, logicalTrace: comparison.switched.logicalTrace, physicalTrace: { switchedResumeFrame: comparison.switched.physicalTrace, returnedContinuationStateLoop: comparison.returned.physicalTrace } });
}
for (const required of corpus.requiredScenarioIds ?? []) if (!caseIds.has(required)) errors.push(`missing required GEN1 case ${required}.`);
const derivedConclusion = deriveConclusions(metricsByVariant, scenarioSliceMetrics);
const snapshotHeader = { schema: "w-gen1-incremental-suspension-results-1", status: "design-oracle-output-gen1", corpus: "tooling/gen1-incremental-suspension-cases.json", corpusDigest: digest(corpusPath), caseCount: results.length, metricBasis: "structural token positions over extracted declarations after comments and strings are stripped; LOC is secondary", metrics: [...variantMetrics, ...helperMetrics], scenarioMetrics: [...scenarioSliceMetrics.values()].map((slice) => ({ scenario: slice.scenario, implementations: slice.implementations.map((implementation) => ({ variant: implementation.variant, symbol: implementation.symbol, applicability: implementation.applicability, digest: implementation.metrics.sourceDigest, metrics: implementation.metrics })) })), derivedConclusion };
const expectedSnapshot = `${[snapshotHeader, ...results].map((result) => JSON.stringify(result)).join("\n")}\n`;
if (errors.length > 0) { process.stderr.write(`${errors.join("\n")}\n`); process.exit(1); }
if (process.argv.includes("--write")) { fs.writeFileSync(snapshotPath, expectedSnapshot); process.stdout.write(`GEN1 incremental suspension: ${results.length} cases; snapshot updated.\n`); process.exit(0); }
if (!fs.existsSync(snapshotPath) || fs.readFileSync(snapshotPath, "utf8") !== expectedSnapshot) { process.stderr.write("gen1-incremental-suspension-results.snapshot.jsonl is stale; run with --write.\n"); process.exit(1); }
process.stdout.write(`GEN1 incremental suspension: ${results.length} cases, ${results.filter((result) => result.status === "accepted").length} accepted, ${results.filter((result) => result.status === "rejected").length} rejected.\n`);
