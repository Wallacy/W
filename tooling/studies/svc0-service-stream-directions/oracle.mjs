import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const directory = path.dirname(fileURLToPath(import.meta.url));

export function loadCases() {
  return JSON.parse(fs.readFileSync(path.join(directory, "cases.json"), "utf8"));
}

function edgeError(edge, direction) {
  if (!edge) return null;
  if (edge.direct !== true) return "streamMustBeDirect";
  if (direction === "input" && edge.taken !== true) return "inputStreamMustTransfer";
  if (direction === "output" && edge.opaque !== true) return "outputStreamMustBeOpaque";
  if (edge.ownedItem !== true) return "borrowedStreamItem";
  if (edge.wireItem !== true) return "nonWireStreamItem";
  if (edge.boundaryFailure !== true) return "boundaryFailureMissing";
  return null;
}

export function topologyOf(testCase) {
  if (testCase.input && testCase.output) return "bidirectional";
  if (testCase.input) return "client-streaming";
  if (testCase.output) return "server-streaming";
  return "unary";
}

export function evaluateCase(testCase) {
  let code = null;
  if (testCase.streamFn === true) code = "streamFunctionRejected";
  else if (testCase.implicitChannel === true) code = "implicitChannelRejected";
  else if (testCase.nestedStream === true) code = "nestedStreamRejected";
  else if (testCase.publishedAnyStream === true) code = "publishedAnyStreamRejected";
  else if (testCase.awaitOpen !== true) code = "serviceOpenRequiresAwait";
  else code = edgeError(testCase.input, "input") ?? edgeError(testCase.output, "output");
  if (!code && testCase.drainsOnSettlement !== true) code = "streamEdgesMustDrain";
  return { id: testCase.id, topology: topologyOf(testCase), status: code ? "rejected" : "accepted", code };
}

export function validate(data) {
  const errors = [];
  if (data?.$schema !== "w-svc0-service-stream-directions-cases-1") errors.push("SVC0 schema is invalid");
  if (!Array.isArray(data?.cases)) return { errors: [...errors, "SVC0 cases are missing"], results: [] };
  const ids = new Set();
  for (const [index, testCase] of data.cases.entries()) {
    if (!testCase || typeof testCase !== "object") { errors.push(`cases[${index}] is invalid`); continue; }
    if (typeof testCase.id !== "string" || testCase.id === "") errors.push(`cases[${index}].id is invalid`);
    else if (ids.has(testCase.id)) errors.push(`duplicate case ${testCase.id}`);
    else ids.add(testCase.id);
    for (const key of ["awaitOpen", "implicitChannel", "streamFn", "nestedStream", "publishedAnyStream", "drainsOnSettlement"]) {
      if (typeof testCase[key] !== "boolean") errors.push(`${testCase.id}.${key} is invalid`);
    }
  }
  return { errors, results: data.cases.map(evaluateCase) };
}
