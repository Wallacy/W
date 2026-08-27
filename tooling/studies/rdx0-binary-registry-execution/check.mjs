import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const studyDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(studyDirectory, "..", "..", "..");
const ledgerPath = path.join(studyDirectory, "task-ledger.json");
const ledger = JSON.parse(fs.readFileSync(ledgerPath, "utf8"));
const errors = [];
const expectedIds = ["RDX0", "PCB0", "WEC0", "TEV0", "SEV0", "SBX0", "RSX0", "ENT0"];
const allowedStates = new Set(["Direção", "Pesquisa"]);
const forbiddenStates = new Set([
  "implemented",
  "implementation-complete",
  "oracle-backed-current",
  "provider-conformant",
  "sandbox-isolated"
]);

function requiredString(value, location) {
  if (typeof value !== "string" || value.trim() === "") errors.push(`${location} must be a non-empty string.`);
}

function requiredStringArray(value, location) {
  if (!Array.isArray(value) || value.length === 0) {
    errors.push(`${location} must be a non-empty array.`);
    return;
  }
  for (const [index, item] of value.entries()) requiredString(item, `${location}[${index}]`);
}

function sourceRefArray(value, location) {
  if (!Array.isArray(value) || value.length === 0) {
    errors.push(`${location} must be a non-empty array.`);
    return;
  }
  for (const [index, item] of value.entries()) {
    requiredString(item?.path, `${location}[${index}].path`);
    requiredString(item?.anchor, `${location}[${index}].anchor`);
    requiredString(item?.claim, `${location}[${index}].claim`);
    if (typeof item?.path !== "string") continue;
    const resolved = path.resolve(repositoryRoot, item.path);
    const relative = path.relative(repositoryRoot, resolved);
    if (relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) {
      errors.push(`${location}[${index}].path escapes the repository.`);
    } else if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
      errors.push(`${location}[${index}].path references a missing file.`);
    } else if (typeof item?.anchor === "string" && !fs.readFileSync(resolved, "utf8").includes(item.anchor)) {
      errors.push(`${location}[${index}].anchor is absent from ${item.path}.`);
    }
  }
}

function exactStringArray(value, expected, location) {
  if (JSON.stringify(value) !== JSON.stringify(expected)) {
    errors.push(`${location} must equal ${JSON.stringify(expected)}.`);
  }
}

function promotionRule(value, location) {
  requiredString(value, location);
  if (typeof value === "string" && !/revisão futura/i.test(value)) errors.push(`${location} must permit a future schema/checker revision.`);
  if (typeof value === "string" && !/evidence/i.test(value)) errors.push(`${location} must require evidence for promotion.`);
}

function checkForbiddenState(value, location) {
  if (typeof value === "string" && forbiddenStates.has(value)) errors.push(`${location} uses a forbidden completion state.`);
}

if (ledger.$schema !== "w-rdx0-binary-registry-execution-ledger-1") errors.push("ledger schema is not RDX0 v1.");
if (ledger.id !== "RDX0") errors.push("ledger id must be RDX0.");
if (ledger.status !== "registered-research-bundle") errors.push("ledger status must remain registered-research-bundle.");
if (ledger.languageSurface !== "none") errors.push("RDX0 must not register a language surface.");
requiredStringArray(ledger.contractRefs, "contractRefs");
sourceRefArray(ledger.sourceRefs, "sourceRefs");
requiredStringArray(ledger.nonClaims, "nonClaims");
checkForbiddenState(ledger.status, "ledger.status");
if (ledger.statePolicy?.registrationOnly !== true) errors.push("statePolicy.registrationOnly must be true.");
exactStringArray(ledger.statePolicy?.taskStatesAtRegistration, ["Direção", "Pesquisa"], "statePolicy.taskStatesAtRegistration");
if (ledger.statePolicy?.promotionRequiresEvidence !== true) errors.push("statePolicy.promotionRequiresEvidence must be true.");
promotionRule(ledger.statePolicy?.promotionRule, "statePolicy.promotionRule");
const expectedCandidatePaths = {
  discovery: "/.well-known/w-registry.json",
  packageIndex: "/v1/packages/<encoded-package-id>/index.json",
  releaseRecord: "/v1/releases/<algorithm>/<digest>.json",
  object: "/v1/objects/<algorithm>/<digest>",
  catalogCheckpoint: "/v1/catalog/checkpoint.json",
  catalogPages: "/v1/catalog/pages/<first>-<last>.jsonl",
  evidenceProjection: "/v1/evidence/<algorithm>/<subject-digest>/index.json",
  channel: "/v1/channels/<encoded-package-id>/<encoded-channel>/<encoded-target-profile>.json"
};
if (JSON.stringify(ledger.candidatePaths) !== JSON.stringify(expectedCandidatePaths)) errors.push("candidatePaths do not match the bounded RDX0 path set.");
const expectedSerializationPolicy = {
  jsonEncoding: "UTF-8-strict",
  duplicateKeys: "reject",
  canonicalSigningPayload: "research-output-no-choice"
};
if (JSON.stringify(ledger.serializationPolicy) !== JSON.stringify(expectedSerializationPolicy)) errors.push("serializationPolicy must keep strict UTF-8, duplicate rejection and canonicalization as research-only.");

if (!Array.isArray(ledger.precedentRefs) || ledger.precedentRefs.length === 0) {
  errors.push("precedentRefs must contain the registered external precedents.");
} else {
  const cloudflare = ledger.precedentRefs.find((item) => item?.url === "https://github.com/cloudflare/sandbox");
  if (!cloudflare) errors.push("precedentRefs must include the exact cloudflare/sandbox URL.");
  else requiredString(cloudflare.role, "precedentRefs[cloudflare].role");
}

if (!Array.isArray(ledger.tasks) || ledger.tasks.length !== expectedIds.length) {
  errors.push(`tasks must contain exactly ${expectedIds.length} entries.`);
}

const tasks = Array.isArray(ledger.tasks) ? ledger.tasks : [];
const seen = new Set();
const allOutputIds = new Set();
const allCaseIds = new Set();
for (const [index, task] of tasks.entries()) {
  const location = `tasks[${index}]`;
  requiredString(task?.id, `${location}.id`);
  if (seen.has(task?.id)) errors.push(`${location}.id is duplicated.`);
  seen.add(task?.id);
  if (task?.id !== expectedIds[index]) errors.push(`${location}.id must be ${expectedIds[index]}.`);
  if (!allowedStates.has(task?.state)) errors.push(`${location}.state must be Direção or Pesquisa.`);
  checkForbiddenState(task?.state, `${location}.state`);
  requiredString(task?.title, `${location}.title`);
  requiredStringArray(task?.dependencies, `${location}.dependencies`);
  requiredString(task?.stopCondition, `${location}.stopCondition`);
  requiredStringArray(task?.evidence?.current, `${location}.evidence.current`);
  requiredStringArray(task?.evidence?.missing, `${location}.evidence.missing`);

  if (!Array.isArray(task?.outputs) || task.outputs.length < 3) {
    errors.push(`${location}.outputs must contain at least three observable outputs.`);
  }
  const taskOutputIds = new Set();
  for (const [outputIndex, output] of (task?.outputs ?? []).entries()) {
    const outputLocation = `${location}.outputs[${outputIndex}]`;
    requiredString(output?.id, `${outputLocation}.id`);
    if (typeof output?.id === "string") {
      if (taskOutputIds.has(output.id)) errors.push(`${outputLocation}.id is duplicated within the task.`);
      if (allOutputIds.has(output.id)) errors.push(`${outputLocation}.id is duplicated across tasks.`);
      taskOutputIds.add(output.id);
      allOutputIds.add(output.id);
      if (!output.id.startsWith(`${task.id}-`)) errors.push(`${outputLocation}.id must be prefixed by ${task.id}-.`);
    }
    requiredString(output?.kind, `${outputLocation}.kind`);
    requiredString(output?.observable, `${outputLocation}.observable`);
  }

  if (!Array.isArray(task?.adversarialCases) || task.adversarialCases.length < 5) {
    errors.push(`${location}.adversarialCases must contain at least five cases.`);
  }
  const taskCaseIds = new Set();
  for (const [caseIndex, adversarialCase] of (task?.adversarialCases ?? []).entries()) {
    const caseLocation = `${location}.adversarialCases[${caseIndex}]`;
    requiredString(adversarialCase?.id, `${caseLocation}.id`);
    if (taskCaseIds.has(adversarialCase?.id)) errors.push(`${caseLocation}.id is duplicated within the task.`);
    taskCaseIds.add(adversarialCase?.id);
    if (typeof adversarialCase?.id === "string" && !adversarialCase.id.startsWith(`${task.id}-`)) errors.push(`${caseLocation}.id must be prefixed by ${task.id}-.`);
    if (typeof adversarialCase?.id === "string") {
      if (allCaseIds.has(adversarialCase.id)) errors.push(`${caseLocation}.id is duplicated across tasks.`);
      allCaseIds.add(adversarialCase.id);
    }
    requiredString(adversarialCase?.expected, `${caseLocation}.expected`);
  }
}

if (JSON.stringify(tasks.map((task) => task.id)) !== JSON.stringify(expectedIds)) errors.push("task order is not the finite RDX0 order.");
if (!ledger.completion || JSON.stringify(ledger.completion.taskIds) !== JSON.stringify(expectedIds)) errors.push("completion.taskIds does not match the finite task set.");
if (!Array.isArray(ledger.completion?.requiredPerTask) || ledger.completion.requiredPerTask.length < 5) errors.push("completion.requiredPerTask is incomplete.");
if (ledger.completion?.registrationOnly !== true) errors.push("completion.registrationOnly must be true.");
exactStringArray(ledger.completion?.taskStatesAtRegistration, ["Direção", "Pesquisa"], "completion.taskStatesAtRegistration");
if (ledger.completion?.promotionRequiresEvidence !== true) errors.push("completion.promotionRequiresEvidence must be true.");
promotionRule(ledger.completion?.promotionRule, "completion.promotionRule");
if (Object.hasOwn(ledger.completion ?? {}, "allStatesMustRemain")) errors.push("completion must describe registration states, not freeze them forever.");

const dependencyEdges = new Map(expectedIds.map((id) => [id, []]));
for (const [index, task] of tasks.entries()) {
  const internal = (task?.dependencies ?? []).filter((dependency) => expectedIds.includes(dependency));
  for (const dependency of internal) {
    if (dependency === task.id) errors.push(`tasks[${index}] cannot depend on itself.`);
    if (expectedIds.indexOf(dependency) >= index) errors.push(`tasks[${index}] dependency ${dependency} must precede the task in the finite order.`);
    if (dependencyEdges.has(task.id)) dependencyEdges.get(task.id).push(dependency);
  }
}
const visiting = new Set();
const visited = new Set();
function visitDependency(id, path = []) {
  if (visiting.has(id)) {
    errors.push(`dependency cycle detected: ${[...path, id].join(" -> ")}.`);
    return;
  }
  if (visited.has(id)) return;
  visiting.add(id);
  for (const dependency of dependencyEdges.get(id) ?? []) visitDependency(dependency, [...path, id]);
  visiting.delete(id);
  visited.add(id);
}
for (const id of expectedIds) visitDependency(id);

const witnesses = Array.isArray(ledger.witnesses) ? ledger.witnesses : [];
if (witnesses.length === 0) errors.push("witnesses must contain the required Last Light adversarial fixture.");
const witnessIds = new Set();
for (const [index, witness] of witnesses.entries()) {
  const location = `witnesses[${index}]`;
  requiredString(witness?.id, `${location}.id`);
  if (witnessIds.has(witness?.id)) errors.push(`${location}.id is duplicated.`);
  witnessIds.add(witness?.id);
  if (witness?.id !== "RDX0-last-light-restaurant-full-traversal") errors.push(`${location}.id must identify the Last Light full traversal witness.`);
  requiredString(witness?.kind, `${location}.kind`);
  requiredString(witness?.scenario, `${location}.scenario`);
  exactStringArray(witness?.taskIds, expectedIds, `${location}.taskIds`);
  sourceRefArray(witness?.sourceRefs, `${location}.sourceRefs`);
  if (!Array.isArray(witness?.outputs) || witness.outputs.length < 3) errors.push(`${location}.outputs must contain at least three observable outputs.`);
  for (const [outputIndex, output] of (witness?.outputs ?? []).entries()) {
    const outputLocation = `${location}.outputs[${outputIndex}]`;
    requiredString(output?.id, `${outputLocation}.id`);
    if (typeof output?.id === "string") {
      if (allOutputIds.has(output.id)) errors.push(`${outputLocation}.id is duplicated.`);
      allOutputIds.add(output.id);
    }
    requiredString(output?.kind, `${outputLocation}.kind`);
    requiredString(output?.observable, `${outputLocation}.observable`);
  }
  if (!Array.isArray(witness?.adversarialCases) || witness.adversarialCases.length < 3) errors.push(`${location}.adversarialCases must contain at least three cases.`);
  for (const [caseIndex, adversarialCase] of (witness?.adversarialCases ?? []).entries()) {
    const caseLocation = `${location}.adversarialCases[${caseIndex}]`;
    requiredString(adversarialCase?.id, `${caseLocation}.id`);
    if (typeof adversarialCase?.id === "string") {
      if (allCaseIds.has(adversarialCase.id)) errors.push(`${caseLocation}.id is duplicated.`);
      allCaseIds.add(adversarialCase.id);
    }
    requiredString(adversarialCase?.expected, `${caseLocation}.expected`);
  }
}
exactStringArray(ledger.completion?.witnessIds, [...witnessIds], "completion.witnessIds");

if (errors.length > 0) {
  process.stderr.write(`${errors.join("\n")}\n`);
  process.exit(1);
}

const caseCount = tasks.reduce((total, task) => total + task.adversarialCases.length, 0);
const outputCount = tasks.reduce((total, task) => total + task.outputs.length, 0);
const witnessOutputCount = witnesses.reduce((total, witness) => total + witness.outputs.length, 0);
const witnessCaseCount = witnesses.reduce((total, witness) => total + witness.adversarialCases.length, 0);
process.stdout.write(`RDX0 task ledger: ${tasks.length} tasks, ${outputCount} task outputs, ${caseCount} task adversarial cases, ${witnessOutputCount} witness outputs and ${witnessCaseCount} witness cases; registration-only states validated.\n`);
