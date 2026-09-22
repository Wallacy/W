import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const file = path.join(root, "tooling", "safety-evidence.json");
const value = JSON.parse(fs.readFileSync(file, "utf8"));
const expectedAreas = [
  "memory-lifetime",
  "numeric",
  "errors-panic-oom-cleanup",
  "concurrency-reclamation",
  "ffi-unsafe-abi",
  "resource-exhaustion-io",
  "optimizer-codegen",
  "build-supply-chain",
];
const expectedGates = [
  "source", "verifiedHir", "lowering", "nativeProduct", "runtimeProvider",
  "targetExecution", "negative", "sanitizerFuzz", "faultInjection",
  "scheduleExploration", "resourceExhaustion", "abiLayout",
];
const states = new Set(["not-applicable", "missing", "bounded", "current"]);

function fail(message) {
  throw new Error(`safety evidence: ${message}`);
}

if (value.$schema !== "w-safety-evidence-1" || value.version !== 1) fail("invalid schema/version");
if (JSON.stringify(value.policy?.gateOrder) !== JSON.stringify(expectedGates)) fail("gate order drift");
if (JSON.stringify(value.policy?.states) !== JSON.stringify([...states])) fail("state vocabulary drift");
if (!Array.isArray(value.areas) || JSON.stringify(value.areas.map((area) => area.id)) !== JSON.stringify(expectedAreas)) {
  fail("area inventory/order drift");
}
for (const area of value.areas) {
  if (!new Set(["blocked", "bounded", "general"]).has(area.promotion)) fail(`${area.id} has invalid promotion`);
  if (!Array.isArray(area.risks) || area.risks.length === 0 || new Set(area.risks).size !== area.risks.length) fail(`${area.id} risks invalid`);
  if (!area.gates || JSON.stringify(Object.keys(area.gates)) !== JSON.stringify(expectedGates)) fail(`${area.id} gates invalid`);
  for (const [gate, state] of Object.entries(area.gates)) if (!states.has(state)) fail(`${area.id}.${gate} has invalid state`);
  if (!Array.isArray(area.nextEvidence) || area.nextEvidence.length === 0) fail(`${area.id} lacks next evidence`);
  if (area.promotion === "general") {
    const incomplete = Object.entries(area.gates).filter(([, state]) => state !== "current" && state !== "not-applicable");
    if (incomplete.length !== 0) fail(`${area.id} cannot be general with incomplete gates`);
  }
}

const summary = Object.fromEntries(["blocked", "bounded", "general"].map((state) => [
  state,
  value.areas.filter((area) => area.promotion === state).length,
]));
process.stdout.write(`Safety evidence: ${value.areas.length} areas; ${JSON.stringify(summary)}\n`);
